#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <netinet/tcp.h>
#include "protocol.h"

// Utility to create directory recursively
void mkdir_p(const char *dir) {
    char tmp[2048];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/') tmp[len - 1] = 0;

    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    }
    mkdir(tmp, S_IRWXU);
}

// -----------------------------------------------------------------------
// clean_topic_dir: remove all files in dir then rmdir it
// Required by instructor model design §4 (Case 1): on disconnect the client
// MUST remove the topic directory before reconnecting from scratch.
// -----------------------------------------------------------------------
void clean_topic_dir(const char *topic_dir) {
    DIR *d = opendir(topic_dir);
    if (!d) return;
    struct dirent *p;
    while ((p = readdir(d)) != NULL) {
        if (strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0) continue;
        char filepath[4096];
        snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, p->d_name);
        unlink(filepath);
    }
    closedir(d);
    rmdir(topic_dir);
    printf("[CASE 1] Cleaned up partial download directory: %s\n", topic_dir);
}

// Build a header with type, transaction_id, and payload length
static inline msg_header_t make_header(uint8_t type, uint32_t txid, uint32_t payload_len) {
    msg_header_t h;
    h.type           = type;
    h.transaction_id = htonl(txid);
    h.length         = htonl(payload_len);
    return h;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <topic> <output_dir>\n", argv[0]);
        exit(1);
    }

    const char *server_ip  = argv[1];
    int port               = atoi(argv[2]);
    const char *topic      = argv[3];
    const char *output_dir = argv[4];

    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", output_dir, topic);
    mkdir_p(topic_dir);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    uint64_t total_bytes_received  = 0;
    uint64_t useful_bytes_received = 0;
    uint32_t reconnects            = 0;

    int success = 0;
    int topic_not_found = 0;

    while (!success && !topic_not_found) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) { perror("socket"); exit(1); }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port   = htons(port);
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("inet_pton"); exit(1);
        }

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            perror("connect");
            sleep(1);
            close(sock);
            continue;
        }

        int nodelay_flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(nodelay_flag));

        if (reconnects > 0) {
            printf("[CASE 1] Reconnecting and restarting transfer from beginning.\n");
        }

        uint32_t txid = 1; // transaction_id for this session
        msg_header_t header = make_header(TYPE_GET_REQUEST, txid, strlen(topic));
        send_all(sock, &header, sizeof(header));
        send_all(sock, topic, strlen(topic));

        FILE *current_file = NULL;
        int error_occurred = 0;

        // Reset useful bytes because Case 1 restarts from scratch
        useful_bytes_received = 0;

        while (1) {
            if (recv_all(sock, &header, sizeof(header)) <= 0) {
                printf("Connection closed by server unexpectedly.\n");
                error_occurred = 1;
                break;
            }

            uint32_t payload_len = ntohl(header.length);
            total_bytes_received += sizeof(header) + payload_len;

            char *payload = NULL;
            if (payload_len > 0) {
                payload = malloc(payload_len);
                if (recv_all(sock, payload, payload_len) <= 0) {
                    free(payload);
                    error_occurred = 1;
                    break;
                }
            }

            if (header.type == TYPE_ERROR) {
                fprintf(stderr, "Server Error: %.*s\n", payload_len, payload);
                free(payload);
                error_occurred = 1;
                break;
            }
            else if (header.type == TYPE_TOPIC_RESPONSE) {
                uint8_t status = payload[0];
                if (status == STATUS_NOT_FOUND) {
                    fprintf(stderr, "[ERROR] Topic '%s' not found on server. Exiting.\n", topic);
                    free(payload);
                    topic_not_found = 1;
                    error_occurred = 1;
                    break;
                }
                free(payload);

                // §2 model: Send MSG_START to signal readiness
                msg_header_t start = make_header(TYPE_MSG_START, txid, 0);
                send_all(sock, &start, sizeof(start));
                continue;
            }
            else if (header.type == TYPE_FILE_META) {
                uint16_t nlen; memcpy(&nlen, payload + 12, 2); nlen = ntohs(nlen);
                char fname[1024];
                memcpy(fname, payload + 14, nlen);
                fname[nlen] = '\0';

                char filepath[4096];
                snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, fname);

                if (current_file) fclose(current_file);
                // We restart from scratch, so we overwrite the file
                current_file = fopen(filepath, "wb");
            }
            else if (header.type == TYPE_FILE_CHUNK) {
                uint32_t data_len = payload_len - 12;
                if (current_file) {
                    fwrite(payload + 12, 1, data_len, current_file);
                }
                useful_bytes_received += data_len;
            }
            else if (header.type == TYPE_FILE_EOF) {
                if (current_file) {
                    fclose(current_file);
                    current_file = NULL;
                }
            }
            else if (header.type == TYPE_TOPIC_EOF) {
                printf("All files received successfully.\n");
                success = 1;
                free(payload);
                break;
            }

            if (payload) free(payload);
        }

        if (current_file) fclose(current_file);
        close(sock);

        if (!success && topic_not_found) {
            break;
        } else if (!success && error_occurred) {
            reconnects++;
            // §4 model (Case 1 CRITICAL): wipe partial directory before retry
            clean_topic_dir(topic_dir);
            mkdir_p(topic_dir);
            usleep(100000); // 100ms delay before reconnect
        } else if (!success) {
            // Unrecoverable error (e.g. topic not found)
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double time_taken = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    double transfer_rate = (useful_bytes_received / 1024.0 / 1024.0) / time_taken;

    printf("\n--- Performance Metrics (Case 1) ---\n");
    printf("Total Bytes Received  : %lu\n", total_bytes_received);
    printf("Useful Bytes (Data)   : %lu\n", useful_bytes_received);
    printf("Redundant Bytes       : %lu\n", total_bytes_received - useful_bytes_received);
    printf("Reconnects            : %u\n", reconnects);
    printf("Time Taken            : %.4f seconds\n", time_taken);
    printf("Throughput (Useful)   : %.2f MB/s\n", transfer_rate);

    return 0;
}
