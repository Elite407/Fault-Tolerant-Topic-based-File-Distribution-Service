#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
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

// Global root dir (kept for reference / logging)
char topics_root_dir[1024];

// Scan topics_root_dir at startup, validate each subdirectory, populate topic_table
void init_topic_table(const char *root_dir) {
    DIR *d = opendir(root_dir);
    if (!d) {
        fprintf(stderr, "[SERVER] Cannot open topics_root_dir '%s': ", root_dir);
        perror("");
        exit(1);
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && topic_count < MAX_TOPICS) {
        if (entry->d_name[0] == '.') continue; // skip . and ..
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

// Return directory path for the given topic, or NULL if not found
const char *find_topic_directory(const char *topic) {
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topic_table[i].topic, topic) == 0)
            return topic_table[i].directory;
    }
    return NULL;
}

// -----------------------------------------------------------------------
// Utility helpers
// -----------------------------------------------------------------------
// Build a header with the given type, transaction_id, and payload length
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
// Thread routine to handle one client connection
// -----------------------------------------------------------------------
void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg); // Free the dynamically allocated socket pointer

    // --- Read GET_REQUEST ---
    msg_header_t header;
    if (recv_all(client_sock, &header, sizeof(header)) <= 0) {
        close(client_sock);
        return NULL;
    }

    uint32_t txid = ntohl(header.transaction_id);
    header.length = ntohl(header.length);

    if (header.type != TYPE_GET_REQUEST) {
        send_error(client_sock, "Expected GET_REQUEST", txid);
        close(client_sock);
        return NULL;
    }
    if (header.length > 2048 || header.length == 0) {
        send_error(client_sock, "Topic length invalid", txid);
        close(client_sock);
        return NULL;
    }

    char *topic = malloc(header.length + 1);
    if (recv_all(client_sock, topic, header.length) <= 0) {
        free(topic);
        close(client_sock);
        return NULL;
    }
    topic[header.length] = '\0';

    // Prevent basic path traversal
    if (strstr(topic, "/") != NULL || strstr(topic, "..") != NULL) {
        send_error(client_sock, "Invalid topic name", txid);
        free(topic);
        close(client_sock);
        return NULL;
    }

    // Look up topic in the pre-built table (§1 of model design)
    const char *topic_dir = find_topic_directory(topic);
    if (!topic_dir) {
        // Topic not found – send TOPIC_RESPONSE(NOT_FOUND) then close
        msg_header_t resp = make_header(TYPE_TOPIC_RESPONSE, txid, 1);
        send_all(client_sock, &resp, sizeof(resp));
        uint8_t status = STATUS_NOT_FOUND;
        send_all(client_sock, &status, 1);
        free(topic);
        close(client_sock);
        return NULL;
    }
    free(topic);

    struct dirent **namelist;
    int num_entries = scandir(topic_dir, &namelist, NULL, alphasort);
    if (num_entries < 0) {
        // Directory became unreadable after startup – treat as not found
        msg_header_t resp = make_header(TYPE_TOPIC_RESPONSE, txid, 1);
        send_all(client_sock, &resp, sizeof(resp));
        uint8_t status = STATUS_NOT_FOUND;
        send_all(client_sock, &status, 1);
        close(client_sock);
        return NULL;
    }

    // Count regular files
    uint32_t total_files = 0;
    for (int i = 0; i < num_entries; i++) {
        if (namelist[i]->d_type == DT_REG) total_files++;
    }

    // Send TOPIC_RESPONSE(FOUND, total_files)  – §2 model: OK <directory, file_list>
    msg_header_t resp = make_header(TYPE_TOPIC_RESPONSE, txid, 1 + 4);
    send_all(client_sock, &resp, sizeof(resp));
    uint8_t status = STATUS_FOUND;
    send_all(client_sock, &status, 1);
    uint32_t net_total = htonl(total_files);
    send_all(client_sock, &net_total, 4);

    // --- Wait for MSG_START from client (§2 model: START handshake) ---
    msg_header_t start_hdr;
    if (recv_all(client_sock, &start_hdr, sizeof(start_hdr)) <= 0) {
        goto cleanup;
    }
    if (start_hdr.type != TYPE_MSG_START) {
        send_error(client_sock, "Expected MSG_START", txid);
        goto cleanup;
    }

    // --- Send files ---
    uint32_t file_index = 0;
    for (int i = 0; i < num_entries; i++) {
        if (namelist[i]->d_type == DT_REG) {
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, namelist[i]->d_name);

            struct stat st;
            if (stat(filepath, &st) == -1) continue;

            // Send FILE_META
            uint16_t fname_len = strlen(namelist[i]->d_name);
            msg_header_t fmeta = make_header(TYPE_FILE_META, txid, 4 + 8 + 2 + fname_len);
            send_all(client_sock, &fmeta, sizeof(fmeta));

            uint32_t net_idx      = htonl(file_index);
            uint64_t net_size     = htonll((uint64_t)st.st_size);
            uint16_t net_fname_len = htons(fname_len);

            send_all(client_sock, &net_idx, 4);
            send_all(client_sock, &net_size, 8);
            send_all(client_sock, &net_fname_len, 2);
            send_all(client_sock, namelist[i]->d_name, fname_len);

            // Send FILE_CHUNKs
            FILE *f = fopen(filepath, "rb");
            if (f) {
                char buffer[CHUNK_SIZE];
                size_t bytes_read;
                uint64_t offset = 0;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    msg_header_t chunk = make_header(TYPE_FILE_CHUNK, txid, 4 + 8 + bytes_read);
                    send_all(client_sock, &chunk, sizeof(chunk));
                    uint64_t net_offset = htonll(offset);
                    send_all(client_sock, &net_idx, 4);
                    send_all(client_sock, &net_offset, 8);
                    send_all(client_sock, buffer, bytes_read);
                    offset += bytes_read;
                }
                fclose(f);
            }

            // Send FILE_EOF
            msg_header_t feof_hdr = make_header(TYPE_FILE_EOF, txid, 4);
            send_all(client_sock, &feof_hdr, sizeof(feof_hdr));
            send_all(client_sock, &net_idx, 4);

            file_index++;
        }
    }

    // Send TOPIC_EOF (CLOSE)
    {
        msg_header_t close_hdr = make_header(TYPE_TOPIC_EOF, txid, 0);
        send_all(client_sock, &close_hdr, sizeof(close_hdr));
    }

cleanup:
    for (int i = 0; i < num_entries; i++) free(namelist[i]);
    free(namelist);
    close(client_sock);
    return NULL;
}

// -----------------------------------------------------------------------
// main
// -----------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <topics_root_dir> [single|multi]\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    strncpy(topics_root_dir, argv[2], sizeof(topics_root_dir) - 1);
    topics_root_dir[sizeof(topics_root_dir) - 1] = '\0';

    int is_multi = 1; // Default to multi-threaded
    if (argc >= 4 && strcmp(argv[3], "single") == 0) {
        is_multi = 0;
    }

    // §1 of model design: populate topic table at startup
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

    printf("Server listening on port %d (%s-threaded)\n", port, is_multi ? "multi" : "single");
    printf("Topics root: %s\n", topics_root_dir);

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

        if (is_multi) {
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, client_sock) != 0) {
                perror("pthread_create");
                close(*client_sock);
                free(client_sock);
            } else {
                pthread_detach(thread);
            }
        } else {
            handle_client(client_sock);
        }
    }

    close(server_sock);
    return 0;
}
