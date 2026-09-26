#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include "protocol.h"

// Global Variables
char topics_root_dir[1024];
double failure_probability = 0.0;

// Utility: Send Error
void send_error(int client_sock, const char *msg) {
    uint32_t len = strlen(msg);
    msg_header_t header;
    header.length = htonl(len);
    header.type = TYPE_ERROR;
    send_all(client_sock, &header, sizeof(header));
    send_all(client_sock, msg, len);
}

// Thread routine to handle a client connection
void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg); 

    // Seed the RNG for this thread
    unsigned int seed = time(NULL) ^ pthread_self();

    msg_header_t header;
    if (recv_all(client_sock, &header, sizeof(header)) <= 0) {
        close(client_sock);
        return NULL;
    }

    header.length = ntohl(header.length);
    if (header.type != TYPE_GET_REQUEST) {
        send_error(client_sock, "Expected GET_REQUEST");
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

    if (strstr(topic, "/") != NULL || strstr(topic, "..") != NULL) {
        send_error(client_sock, "Invalid topic name");
        free(topic);
        close(client_sock);
        return NULL;
    }

    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", topics_root_dir, topic);
    free(topic);

    DIR *dir = opendir(topic_dir);
    if (!dir) {
        header.length = htonl(1);
        header.type = TYPE_TOPIC_RESPONSE;
        send_all(client_sock, &header, sizeof(header));
        uint8_t status = STATUS_NOT_FOUND;
        send_all(client_sock, &status, 1);
        close(client_sock);
        return NULL;
    }

    uint32_t total_files = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) total_files++;
    }
    
    header.length = htonl(1 + 4);
    header.type = TYPE_TOPIC_RESPONSE;
    if(send_all(client_sock, &header, sizeof(header)) <= 0) goto cleanup;
    uint8_t status = STATUS_FOUND;
    if(send_all(client_sock, &status, 1) <= 0) goto cleanup;
    uint32_t net_total = htonl(total_files);
    if(send_all(client_sock, &net_total, 4) <= 0) goto cleanup;

    rewinddir(dir);
    uint32_t file_index = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, entry->d_name);
            
            struct stat st;
            if (stat(filepath, &st) == -1) continue;

            uint16_t fname_len = strlen(entry->d_name);
            header.length = htonl(4 + 8 + 2 + fname_len);
            header.type = TYPE_FILE_META;
            if(send_all(client_sock, &header, sizeof(header)) <= 0) goto cleanup;
            
            uint32_t net_idx = htonl(file_index);
            uint64_t net_size = htonll((uint64_t)st.st_size);
            uint16_t net_fname_len = htons(fname_len);
            
            if(send_all(client_sock, &net_idx, 4) <= 0) goto cleanup;
            if(send_all(client_sock, &net_size, 8) <= 0) goto cleanup;
            if(send_all(client_sock, &net_fname_len, 2) <= 0) goto cleanup;
            if(send_all(client_sock, entry->d_name, fname_len) <= 0) goto cleanup;

            FILE *f = fopen(filepath, "rb");
            if (f) {
                char buffer[CHUNK_SIZE];
                size_t bytes_read;
                uint64_t offset = 0;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    
                    // --- FAULT INJECTION ---
                    if (failure_probability > 0.0) {
                        double r = (double)rand_r(&seed) / RAND_MAX;
                        if (r < failure_probability) {
                            printf("[FAULT INJECTED] Thread %lu terminating connection.\n", (unsigned long)pthread_self());
                            fclose(f);
                            goto cleanup;
                        }
                    }
                    // -----------------------

                    header.length = htonl(4 + 8 + bytes_read);
                    header.type = TYPE_FILE_CHUNK;
                    if(send_all(client_sock, &header, sizeof(header)) <= 0) {
                        fclose(f);
                        goto cleanup;
                    }
                    
                    uint64_t net_offset = htonll(offset);
                    if(send_all(client_sock, &net_idx, 4) <= 0) { fclose(f); goto cleanup; }
                    if(send_all(client_sock, &net_offset, 8) <= 0) { fclose(f); goto cleanup; }
                    if(send_all(client_sock, buffer, bytes_read) <= 0) { fclose(f); goto cleanup; }
                    
                    offset += bytes_read;
                }
                fclose(f);
            }

            header.length = htonl(4);
            header.type = TYPE_FILE_EOF;
            if(send_all(client_sock, &header, sizeof(header)) <= 0) goto cleanup;
            if(send_all(client_sock, &net_idx, 4) <= 0) goto cleanup;

            file_index++;
        }
    }

    header.length = htonl(0);
    header.type = TYPE_TOPIC_EOF;
    send_all(client_sock, &header, sizeof(header));

cleanup:
    closedir(dir);
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
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

    printf("Server listening on port %d\n", port);
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
