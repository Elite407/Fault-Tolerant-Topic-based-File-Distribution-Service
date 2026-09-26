#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "protocol.h"

// Global Variables
char topics_root_dir[1024];
double failure_probability = 0.0;

// Session Management
typedef struct Session {
    uint64_t session_id;
    char topic[256];
    uint32_t worker_id;
    uint32_t total_workers;
    struct Session *next;
} Session;

Session *sessions_head = NULL;
pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

uint64_t create_session(const char *topic, uint32_t worker_id, uint32_t total_workers) {
    Session *s = malloc(sizeof(Session));
    s->session_id = ((uint64_t)rand() << 32) | rand();
    if (s->session_id == 0) s->session_id = 1;
    strncpy(s->topic, topic, sizeof(s->topic) - 1);
    s->worker_id = worker_id;
    s->total_workers = total_workers;
    
    pthread_mutex_lock(&sessions_mutex);
    s->next = sessions_head;
    sessions_head = s;
    pthread_mutex_unlock(&sessions_mutex);
    
    return s->session_id;
}

int validate_session(uint64_t session_id, const char *topic, uint32_t *worker_id, uint32_t *total_workers) {
    pthread_mutex_lock(&sessions_mutex);
    Session *curr = sessions_head;
    while (curr) {
        if (curr->session_id == session_id && strcmp(curr->topic, topic) == 0) {
            *worker_id = curr->worker_id;
            *total_workers = curr->total_workers;
            pthread_mutex_unlock(&sessions_mutex);
            return 1;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&sessions_mutex);
    return 0;
}

void send_error(int client_sock, const char *msg) {
    uint32_t len = strlen(msg);
    msg_header_t header;
    header.length = htonl(len);
    header.type = TYPE_ERROR;
    send_all(client_sock, &header, sizeof(header));
    send_all(client_sock, msg, len);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg); 
    unsigned int seed = time(NULL) ^ pthread_self();

    msg_header_t header;
    if (recv_all(client_sock, &header, sizeof(header)) <= 0) {
        close(client_sock);
        return NULL;
    }

    header.length = ntohl(header.length);
    
    char *topic = NULL;
    uint32_t resume_file_idx = 0;
    uint64_t resume_offset = 0;
    uint64_t session_id = 0;
    int is_resume = 0;
    uint32_t worker_id = 0;
    uint32_t total_workers = 1;

    if (header.type == TYPE_GET_REQUEST) {
        topic = malloc(header.length + 1);
        if (recv_all(client_sock, topic, header.length) <= 0) goto cleanup;
        topic[header.length] = '\0';
        
        session_id = create_session(topic, 0, 1);
    } 
    else if (header.type == TYPE_GET_REQUEST_WORKER) {
        char *payload = malloc(header.length);
        if (recv_all(client_sock, payload, header.length) <= 0) { free(payload); goto cleanup; }
        
        memcpy(&worker_id, payload, 4); worker_id = ntohl(worker_id);
        memcpy(&total_workers, payload + 4, 4); total_workers = ntohl(total_workers);
        
        uint32_t topic_len = header.length - 8;
        topic = malloc(topic_len + 1);
        memcpy(topic, payload + 8, topic_len);
        topic[topic_len] = '\0';
        free(payload);
        
        session_id = create_session(topic, worker_id, total_workers);
    }
    else if (header.type == TYPE_RECONNECT_REQ) {
        char *payload = malloc(header.length);
        if (recv_all(client_sock, payload, header.length) <= 0) { free(payload); goto cleanup; }
        
        memcpy(&session_id, payload, 8); session_id = ntohll(session_id);
        memcpy(&resume_file_idx, payload + 8, 4); resume_file_idx = ntohl(resume_file_idx);
        memcpy(&resume_offset, payload + 12, 8); resume_offset = ntohll(resume_offset);
        
        uint32_t topic_len = header.length - 20;
        topic = malloc(topic_len + 1);
        memcpy(topic, payload + 20, topic_len);
        topic[topic_len] = '\0';
        free(payload);
        
        if (validate_session(session_id, topic, &worker_id, &total_workers)) {
            is_resume = 1;
            header.length = htonl(1);
            header.type = TYPE_RESUME_RESPONSE;
            send_all(client_sock, &header, sizeof(header));
            uint8_t status = RESUME_ACCEPTED;
            send_all(client_sock, &status, 1);
        } else {
            header.length = htonl(1);
            header.type = TYPE_RESUME_RESPONSE;
            send_all(client_sock, &header, sizeof(header));
            uint8_t status = RESUME_REJECTED;
            send_all(client_sock, &status, 1);
            goto cleanup;
        }
    } 
    else {
        send_error(client_sock, "Expected GET_REQUEST, GET_REQUEST_WORKER, or RECONNECT_REQ");
        goto cleanup;
    }

    if (strstr(topic, "/") != NULL || strstr(topic, "..") != NULL) {
        send_error(client_sock, "Invalid topic");
        goto cleanup;
    }

    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", topics_root_dir, topic);
    
    DIR *dir = opendir(topic_dir);
    if (!dir) {
        header.length = htonl(1);
        header.type = TYPE_TOPIC_RESPONSE;
        send_all(client_sock, &header, sizeof(header));
        uint8_t status = STATUS_NOT_FOUND;
        send_all(client_sock, &status, 1);
        goto cleanup;
    }

    if (!is_resume) {
        uint32_t total_files = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                if ((total_files % total_workers) == worker_id) {
                    // Count only files assigned to this worker (optional, but good for reporting)
                }
                total_files++;
            }
        }
        
        header.length = htonl(1 + 4);
        header.type = TYPE_TOPIC_RESPONSE;
        if(send_all(client_sock, &header, sizeof(header)) <= 0) { closedir(dir); goto cleanup; }
        uint8_t status = STATUS_FOUND;
        if(send_all(client_sock, &status, 1) <= 0) { closedir(dir); goto cleanup; }
        uint32_t net_total = htonl(total_files);
        if(send_all(client_sock, &net_total, 4) <= 0) { closedir(dir); goto cleanup; }

        header.length = htonl(8);
        header.type = TYPE_SESSION_CREATED;
        if(send_all(client_sock, &header, sizeof(header)) <= 0) { closedir(dir); goto cleanup; }
        uint64_t net_sid = htonll(session_id);
        if(send_all(client_sock, &net_sid, 8) <= 0) { closedir(dir); goto cleanup; }
    }

    rewinddir(dir);
    uint32_t file_index = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            if ((file_index % total_workers) != worker_id) {
                file_index++;
                continue;
            }

            if (is_resume && file_index < resume_file_idx) {
                file_index++;
                continue;
            }

            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, entry->d_name);
            struct stat st;
            if (stat(filepath, &st) == -1) continue;

            uint16_t fname_len = strlen(entry->d_name);
            
            header.length = htonl(4 + 8 + 2 + fname_len);
            header.type = TYPE_FILE_META;
            if(send_all(client_sock, &header, sizeof(header)) <= 0) { closedir(dir); goto cleanup; }
            
            uint32_t net_idx = htonl(file_index);
            uint64_t net_size = htonll((uint64_t)st.st_size);
            uint16_t net_fname_len = htons(fname_len);
            
            if(send_all(client_sock, &net_idx, 4) <= 0) { closedir(dir); goto cleanup; }
            if(send_all(client_sock, &net_size, 8) <= 0) { closedir(dir); goto cleanup; }
            if(send_all(client_sock, &net_fname_len, 2) <= 0) { closedir(dir); goto cleanup; }
            if(send_all(client_sock, entry->d_name, fname_len) <= 0) { closedir(dir); goto cleanup; }

            FILE *f = fopen(filepath, "rb");
            if (f) {
                char buffer[CHUNK_SIZE];
                size_t bytes_read;
                uint64_t offset = 0;
                
                if (is_resume && file_index == resume_file_idx) {
                    fseek(f, resume_offset, SEEK_SET);
                    offset = resume_offset;
                }
                
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    if (failure_probability > 0.0) {
                        double r = (double)rand_r(&seed) / RAND_MAX;
                        if (r < failure_probability) {
                            // printf("[FAULT INJECTED] Connection dropped. (Session %lu, Worker %u)\n", session_id, worker_id);
                            fclose(f);
                            closedir(dir);
                            goto cleanup;
                        }
                    }

                    header.length = htonl(4 + 8 + bytes_read);
                    header.type = TYPE_FILE_CHUNK;
                    if(send_all(client_sock, &header, sizeof(header)) <= 0) { fclose(f); closedir(dir); goto cleanup; }
                    
                    uint64_t net_offset = htonll(offset);
                    if(send_all(client_sock, &net_idx, 4) <= 0) { fclose(f); closedir(dir); goto cleanup; }
                    if(send_all(client_sock, &net_offset, 8) <= 0) { fclose(f); closedir(dir); goto cleanup; }
                    if(send_all(client_sock, buffer, bytes_read) <= 0) { fclose(f); closedir(dir); goto cleanup; }
                    
                    offset += bytes_read;
                }
                fclose(f);
            }

            header.length = htonl(4);
            header.type = TYPE_FILE_EOF;
            if(send_all(client_sock, &header, sizeof(header)) <= 0) { closedir(dir); goto cleanup; }
            if(send_all(client_sock, &net_idx, 4) <= 0) { closedir(dir); goto cleanup; }

            file_index++;
            is_resume = 0; // After first file resumes, rest are normal
        }
    }

    header.length = htonl(0);
    header.type = TYPE_TOPIC_EOF;
    send_all(client_sock, &header, sizeof(header));
    closedir(dir);

cleanup:
    if (topic) free(topic);
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <topics_root_dir> <failure probability>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    strncpy(topics_root_dir, argv[2], sizeof(topics_root_dir) - 1);
    failure_probability = atof(argv[3]);
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(server_sock, 100) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server (Case 2 Enhanced) listening on port %d\n", port);
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
