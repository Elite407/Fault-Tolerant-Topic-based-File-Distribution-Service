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
    free(arg); // Free the dynamically allocated socket pointer

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

    // Prevent basic path traversal
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

    // Count files
    uint32_t total_files = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            total_files++;
        }
    }
    
    // Send TOPIC_RESPONSE
    header.length = htonl(1 + 4);
    header.type = TYPE_TOPIC_RESPONSE;
    send_all(client_sock, &header, sizeof(header));
    uint8_t status = STATUS_FOUND;
    send_all(client_sock, &status, 1);
    uint32_t net_total = htonl(total_files);
    send_all(client_sock, &net_total, 4);

    // Rewind and send files
    rewinddir(dir);
    uint32_t file_index = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, entry->d_name);
            
            struct stat st;
            if (stat(filepath, &st) == -1) continue;

            // Send FILE_META
            uint16_t fname_len = strlen(entry->d_name);
            header.length = htonl(4 + 8 + 2 + fname_len);
            header.type = TYPE_FILE_META;
            send_all(client_sock, &header, sizeof(header));
            
            uint32_t net_idx = htonl(file_index);
            uint64_t net_size = htonll((uint64_t)st.st_size);
            uint16_t net_fname_len = htons(fname_len);
            
            send_all(client_sock, &net_idx, 4);
            send_all(client_sock, &net_size, 8);
            send_all(client_sock, &net_fname_len, 2);
            send_all(client_sock, entry->d_name, fname_len);

            // Send FILE_CHUNKs
            FILE *f = fopen(filepath, "rb");
            if (f) {
                char buffer[CHUNK_SIZE];
                size_t bytes_read;
                uint64_t offset = 0;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    header.length = htonl(4 + 8 + bytes_read);
                    header.type = TYPE_FILE_CHUNK;
                    send_all(client_sock, &header, sizeof(header));
                    
                    uint64_t net_offset = htonll(offset);
                    send_all(client_sock, &net_idx, 4);
                    send_all(client_sock, &net_offset, 8);
                    send_all(client_sock, buffer, bytes_read);
                    
                    offset += bytes_read;
                }
                fclose(f);
            }

            // Send FILE_EOF
            header.length = htonl(4);
            header.type = TYPE_FILE_EOF;
            send_all(client_sock, &header, sizeof(header));
            send_all(client_sock, &net_idx, 4);

            file_index++;
        }
    }
    closedir(dir);

    // Send TOPIC_EOF
    header.length = htonl(0);
    header.type = TYPE_TOPIC_EOF;
    send_all(client_sock, &header, sizeof(header));

    close(client_sock);
    return NULL;
}

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
