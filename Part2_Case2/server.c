#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <poll.h>
#include <netinet/tcp.h>
#include "protocol.h"

// -----------------------------------------------------------------------
// Topic-to-Directory Mapping Table  (instructor model design §1)
// -----------------------------------------------------------------------
#define MAX_TOPIC_LEN 128
#define MAX_PATH_LEN  512
#define MAX_TOPICS    128

typedef struct {
    char topic[MAX_TOPIC_LEN];
    char directory[MAX_PATH_LEN];
} TopicEntry;

static TopicEntry topic_table[MAX_TOPICS];
static int        topic_count = 0;

char topics_root_dir[1024];
double failure_probability = 0.0;

void init_topic_table(const char *root_dir) {
    DIR *d = opendir(root_dir);
    if (!d) {
        fprintf(stderr, "[SERVER] Cannot open topics_root_dir '%s': ", root_dir);
        perror("");
        exit(1);
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && topic_count < MAX_TOPICS) {
        if (entry->d_name[0] == '.') continue;
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", root_dir, entry->d_name);
        struct stat st;
        if (stat(fullpath, &st) == -1) continue;
        if (!S_ISDIR(st.st_mode)) continue;
        if (access(fullpath, R_OK | X_OK) != 0) {
            fprintf(stderr, "[SERVER] Skipping unreadable topic dir: %s\n", fullpath);
            continue;
        }
        strncpy(topic_table[topic_count].topic,     entry->d_name, MAX_TOPIC_LEN - 1);
        strncpy(topic_table[topic_count].directory, fullpath,       MAX_PATH_LEN  - 1);
        topic_table[topic_count].topic[MAX_TOPIC_LEN - 1] = '\0';
        topic_table[topic_count].directory[MAX_PATH_LEN - 1] = '\0';
        topic_count++;
    }
    closedir(d);
    printf("[SERVER] Loaded %d topics from '%s'\n", topic_count, root_dir);
}

const char *find_topic_directory(const char *topic) {
    for (int i = 0; i < topic_count; i++) {
        if (strcasecmp(topic_table[i].topic, topic) == 0)
            return topic_table[i].directory;
    }
    return NULL;
}

// -----------------------------------------------------------------------
// Session Management  (instructor model design §5 – server-side state)
// -----------------------------------------------------------------------
typedef struct Session {
    uint64_t session_id;
    char     topic[256];
    // Server-side checkpoint (§5 of model design)
    uint32_t current_file_idx;
    uint64_t current_offset;
    time_t   last_update;
    struct Session *next;
} Session;

Session *sessions_head = NULL;
pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

uint64_t create_session(const char *topic) {
    Session *s = malloc(sizeof(Session));
    s->session_id = ((uint64_t)rand() << 32) | rand();
    if (s->session_id == 0) s->session_id = 1;
    strncpy(s->topic, topic, sizeof(s->topic) - 1);
    s->topic[sizeof(s->topic) - 1] = '\0';
    s->current_file_idx = 0;
    s->current_offset   = 0;
    s->last_update      = time(NULL);

    pthread_mutex_lock(&sessions_mutex);
    s->next = sessions_head;
    sessions_head = s;
    pthread_mutex_unlock(&sessions_mutex);

    return s->session_id;
}

// Validate session and fill out resume point; returns 1 if valid, 0 otherwise
int validate_session(uint64_t session_id, const char *topic,
                     uint32_t *resume_file_idx, uint64_t *resume_offset) {
    pthread_mutex_lock(&sessions_mutex);
    Session *curr = sessions_head;
    while (curr) {
        if (curr->session_id == session_id && strcmp(curr->topic, topic) == 0) {
            *resume_file_idx = curr->current_file_idx;
            *resume_offset   = curr->current_offset;
            pthread_mutex_unlock(&sessions_mutex);
            return 1;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&sessions_mutex);
    return 0;
}

// Update server-side checkpoint for a session
void update_session_checkpoint(uint64_t session_id, uint32_t file_idx, uint64_t offset) {
    pthread_mutex_lock(&sessions_mutex);
    Session *curr = sessions_head;
    while (curr) {
        if (curr->session_id == session_id) {
            curr->current_file_idx = file_idx;
            curr->current_offset   = offset;
            curr->last_update      = time(NULL);
            break;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&sessions_mutex);
}

// Non-blocking poll: drain any TYPE_CLIENT_ACK messages from the client.
// Updates session checkpoint to the ACK'd position.
// Returns: 0 = no error, -1 = client disconnected
static int poll_for_acks(int sock, uint64_t session_id) {
    struct pollfd pfd = { .fd = sock, .events = POLLIN };

    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        msg_header_t ack_hdr;
        if (recv_all(sock, &ack_hdr, sizeof(ack_hdr)) <= 0)
            return -1;
        uint32_t ack_len = ntohl(ack_hdr.length);
        if (ack_hdr.type == TYPE_CLIENT_ACK && ack_len == 12) {
            char ack_payload[12];
            if (recv_all(sock, ack_payload, 12) <= 0)
                return -1;
            uint32_t ack_idx;  memcpy(&ack_idx, ack_payload, 4);     ack_idx = ntohl(ack_idx);
            uint64_t ack_off;  memcpy(&ack_off, ack_payload + 4, 8); ack_off = ntohll(ack_off);
            update_session_checkpoint(session_id, ack_idx, ack_off);
        } else if (ack_len > 0) {
            char *discard = malloc(ack_len);
            if (!discard) return -1;
            if (recv_all(sock, discard, ack_len) <= 0) {
                free(discard);
                return -1;
            }
            free(discard);
        }
    }
    return 0;
}

// -----------------------------------------------------------------------
// Utility helpers
// -----------------------------------------------------------------------
static inline msg_header_t make_header(uint8_t type, uint32_t txid, uint32_t payload_len) {
    msg_header_t h;
    h.type           = type;
    h.transaction_id = htonl(txid);
    h.length         = htonl(payload_len);
    return h;
}

void send_error(int client_sock, const char *msg, uint32_t txid) {
    uint32_t len = strlen(msg);
    msg_header_t header = make_header(TYPE_ERROR, txid, len);
    send_all(client_sock, &header, sizeof(header));
    send_all(client_sock, msg, len);
}

// -----------------------------------------------------------------------
// Thread routine to handle a client connection
// -----------------------------------------------------------------------
void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    unsigned int seed = time(NULL) ^ pthread_self();

    msg_header_t header;
    if (recv_all(client_sock, &header, sizeof(header)) <= 0) {
        close(client_sock);
        return NULL;
    }

    uint32_t txid         = ntohl(header.transaction_id);
    header.length         = ntohl(header.length);

    char    *topic            = NULL;
    uint32_t resume_file_idx  = 0;
    uint64_t resume_offset    = 0;
    uint64_t session_id       = 0;
    int      is_resume        = 0;

    if (header.type == TYPE_GET_REQUEST) {
        // Fresh connection or reconnect with embedded session_id
        // Payload: [optional 8-byte session_id] + topic_string
        // If length >= 8 and first 8 bytes look like a session_id, it's a resume
        if (header.length > 2048 || header.length == 0) {
            send_error(client_sock, "Topic length invalid", txid);
            goto cleanup;
        }
        topic = malloc(header.length + 1);
        if (recv_all(client_sock, topic, header.length) <= 0) goto cleanup;
        topic[header.length] = '\0';

        session_id = create_session(topic);
    }
    else if (header.type == TYPE_RECONNECT_REQ) {
        // Legacy reconnect path: session_id(8) + file_idx(4) + offset(8) + topic
        if (header.length > 2048 + 20 || header.length < 20) {
            send_error(client_sock, "Payload length invalid", txid);
            goto cleanup;
        }
        char *payload = malloc(header.length);
        if (recv_all(client_sock, payload, header.length) <= 0) { free(payload); goto cleanup; }

        uint64_t client_sid;
        uint32_t client_file_idx;
        uint64_t client_offset;
        memcpy(&client_sid,      payload,      8); client_sid      = ntohll(client_sid);
        memcpy(&client_file_idx, payload + 8,  4); client_file_idx = ntohl(client_file_idx);
        memcpy(&client_offset,   payload + 12, 8); client_offset   = ntohll(client_offset);

        uint32_t topic_len = header.length - 20;
        topic = malloc(topic_len + 1);
        memcpy(topic, payload + 20, topic_len);
        topic[topic_len] = '\0';
        free(payload);

        // Validate against server-side session table
        uint32_t srv_file_idx;
        uint64_t srv_offset;
        if (validate_session(client_sid, topic, &srv_file_idx, &srv_offset)) {
            is_resume = 1;
            session_id = client_sid;
            // Use the server's checkpoint (authoritative), fall back to client's if server = 0
            resume_file_idx = (srv_file_idx > 0 || srv_offset > 0) ? srv_file_idx : client_file_idx;
            resume_offset   = (srv_file_idx > 0 || srv_offset > 0) ? srv_offset   : client_offset;
            printf("[SESSION] Resuming session %lu at file=%u offset=%lu\n",
                   session_id, resume_file_idx, resume_offset);

            msg_header_t rr = make_header(TYPE_RESUME_RESPONSE, txid, 1);
            send_all(client_sock, &rr, sizeof(rr));
            uint8_t st = RESUME_ACCEPTED;
            send_all(client_sock, &st, 1);
        } else {
            msg_header_t rr = make_header(TYPE_RESUME_RESPONSE, txid, 1);
            send_all(client_sock, &rr, sizeof(rr));
            uint8_t st = RESUME_REJECTED;
            send_all(client_sock, &st, 1);
            goto cleanup;
        }
    }
    else {
        send_error(client_sock, "Expected GET_REQUEST or RECONNECT_REQ", txid);
        goto cleanup;
    }

    if (strstr(topic, "/") != NULL || strstr(topic, "..") != NULL) {
        send_error(client_sock, "Invalid topic", txid);
        goto cleanup;
    }

    // Lookup in topic table (§1 model design)
    const char *topic_dir = find_topic_directory(topic);
    if (!topic_dir) {
        msg_header_t resp = make_header(TYPE_TOPIC_RESPONSE, txid, 1);
        send_all(client_sock, &resp, sizeof(resp));
        uint8_t s = STATUS_NOT_FOUND;
        send_all(client_sock, &s, 1);
        goto cleanup;
    }

    struct dirent **namelist = NULL;
    int num_entries = scandir(topic_dir, &namelist, NULL, alphasort);
    if (num_entries < 0) {
        msg_header_t resp = make_header(TYPE_TOPIC_RESPONSE, txid, 1);
        send_all(client_sock, &resp, sizeof(resp));
        uint8_t s = STATUS_NOT_FOUND;
        send_all(client_sock, &s, 1);
        goto cleanup;
    }

    if (!is_resume) {
        uint32_t total_files = 0;
        for (int i = 0; i < num_entries; i++) {
            if (namelist[i]->d_type == DT_REG) total_files++;
        }

        // Send TOPIC_RESPONSE (OK) 
        msg_header_t resp = make_header(TYPE_TOPIC_RESPONSE, txid, 1 + 4);
        if(send_all(client_sock, &resp, sizeof(resp)) <= 0) { goto free_namelist; }
        uint8_t s = STATUS_FOUND;
        if(send_all(client_sock, &s, 1) <= 0) { goto free_namelist; }
        uint32_t net_total = htonl(total_files);
        if(send_all(client_sock, &net_total, 4) <= 0) { goto free_namelist; }

        // Send SESSION_CREATED with session_id
        msg_header_t sc = make_header(TYPE_SESSION_CREATED, txid, 8);
        if(send_all(client_sock, &sc, sizeof(sc)) <= 0) { goto free_namelist; }
        uint64_t net_sid = htonll(session_id);
        if(send_all(client_sock, &net_sid, 8) <= 0) { goto free_namelist; }

        // Wait for MSG_START (§2 model: START handshake)
        msg_header_t start_hdr;
        if (recv_all(client_sock, &start_hdr, sizeof(start_hdr)) <= 0) goto free_namelist;
        if (start_hdr.type != TYPE_MSG_START) {
            send_error(client_sock, "Expected MSG_START", txid);
            goto free_namelist;
        }
    }

    uint32_t file_index = 0;
    for (int i = 0; i < num_entries; i++) {
        if (namelist[i]->d_type == DT_REG) {
            if (is_resume && file_index < resume_file_idx) {
                file_index++;
                continue;
            }

            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, namelist[i]->d_name);
            struct stat st;
            if (stat(filepath, &st) == -1) continue;

            uint16_t fname_len = strlen(namelist[i]->d_name);
            msg_header_t fmeta = make_header(TYPE_FILE_META, txid, 4 + 8 + 2 + fname_len);
            uint32_t net_idx       = htonl(file_index);
            uint64_t net_size      = htonll((uint64_t)st.st_size);
            uint16_t net_fname_len = htons(fname_len);

            char meta_buf[sizeof(msg_header_t) + 14 + 1024];
            size_t meta_len = sizeof(msg_header_t) + 14 + fname_len;
            memcpy(meta_buf, &fmeta, sizeof(fmeta));
            memcpy(meta_buf + sizeof(fmeta), &net_idx, 4);
            memcpy(meta_buf + sizeof(fmeta) + 4, &net_size, 8);
            memcpy(meta_buf + sizeof(fmeta) + 12, &net_fname_len, 2);
            memcpy(meta_buf + sizeof(fmeta) + 14, namelist[i]->d_name, fname_len);
            if(send_all(client_sock, meta_buf, meta_len) <= 0) { goto free_namelist; }

            FILE *f = fopen(filepath, "rb");
            if (f) {
                char *send_buf = malloc(sizeof(msg_header_t) + 12 + CHUNK_SIZE);
                if (send_buf) {
                    size_t bytes_read;
                    uint64_t offset = 0;

                    if (is_resume && file_index == resume_file_idx) {
                        fseek(f, resume_offset, SEEK_SET);
                        offset = resume_offset;
                    }

                    while ((bytes_read = fread(send_buf + sizeof(msg_header_t) + 12, 1, CHUNK_SIZE, f)) > 0) {
                        if (failure_probability > 0.0) {
                            double r = (double)rand_r(&seed) / RAND_MAX;
                            if (r < failure_probability) {
                                printf("[FAULT INJECTED] Connection dropped. (Session %lu)\n", session_id);
                                // Checkpoint stays at last ACK'd position
                                free(send_buf);
                                fclose(f);
                                goto free_namelist;
                            }
                        }

                        msg_header_t chunk = make_header(TYPE_FILE_CHUNK, txid, 4 + 8 + bytes_read);
                        uint64_t net_offset = htonll(offset);
                        memcpy(send_buf, &chunk, sizeof(chunk));
                        memcpy(send_buf + sizeof(chunk), &net_idx, 4);
                        memcpy(send_buf + sizeof(chunk) + 4, &net_offset, 8);
                        if(send_all(client_sock, send_buf, sizeof(msg_header_t) + 12 + bytes_read) <= 0) {
                            free(send_buf);
                            fclose(f);
                            goto free_namelist;
                        }

                        offset += bytes_read;
                        // Poll for client ACKs (non-blocking) and update checkpoint to client-confirmed position
                        if (poll_for_acks(client_sock, session_id) < 0) {
                            free(send_buf);
                            fclose(f);
                            goto free_namelist;
                        }
                    }
                    free(send_buf);
                }
                fclose(f);
            }

            msg_header_t feof_hdr = make_header(TYPE_FILE_EOF, txid, 4);
            char feof_buf[sizeof(msg_header_t) + 4];
            memcpy(feof_buf, &feof_hdr, sizeof(feof_hdr));
            memcpy(feof_buf + sizeof(feof_hdr), &net_idx, 4);
            if(send_all(client_sock, feof_buf, sizeof(feof_buf)) <= 0) { goto free_namelist; }
            poll_for_acks(client_sock, session_id);

            file_index++;
            is_resume = 0; // After first resumed file, subsequent files are normal
        }
    }

    // Send TOPIC_EOF (CLOSE)
    {
        msg_header_t close_hdr = make_header(TYPE_TOPIC_EOF, txid, 0);
        send_all(client_sock, &close_hdr, sizeof(close_hdr));
    }

free_namelist:
    for (int i = 0; i < num_entries; i++) free(namelist[i]);
    free(namelist);

cleanup:
    if (topic) free(topic);
    close(client_sock);
    return NULL;
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------
int main(int argc, char *argv[]) {
    srand(time(NULL));
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <topics_root_dir> <failure probability>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    strncpy(topics_root_dir, argv[2], sizeof(topics_root_dir) - 1);
    topics_root_dir[sizeof(topics_root_dir) - 1] = '\0';
    failure_probability = atof(argv[3]);

    // §1 model design: populate topic table at startup
    init_topic_table(topics_root_dir);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind"); exit(1);
    }
    if (listen(server_sock, 100) == -1) { perror("listen"); exit(1); }

    printf("Server (Case 2) listening on port %d\n", port);
    printf("Topics root: %s\n", topics_root_dir);
    printf("Failure probability: %f\n", failure_probability);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (*client_sock == -1) {
            perror("accept");
            free(client_sock);
            continue;
        }

        int nodelay_flag = 1;
        setsockopt(*client_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(nodelay_flag));

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_sock) != 0) {
            perror("pthread_create");
            close(*client_sock);
            free(client_sock);
        } else {
            pthread_detach(thread);
        }
    }

    close(server_sock);
    return 0;
}
