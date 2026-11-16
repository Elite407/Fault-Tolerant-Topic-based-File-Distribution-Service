#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
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

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { perror("socket"); exit(1); }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton"); exit(1);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    uint64_t total_bytes_received = 0;
    uint64_t useful_bytes_received = 0; // Only file payload data

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect"); exit(1);
    }

    int nodelay_flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(nodelay_flag));

    // transaction_id: use 1 for the initial GET request
    uint32_t txid = 1;

    // Send GET_REQUEST (§2 model: GET <topic>)
    msg_header_t header = make_header(TYPE_GET_REQUEST, txid, strlen(topic));
    send_all(sock, &header, sizeof(header));
    send_all(sock, topic, strlen(topic));

    FILE *current_file = NULL;

    while (1) {
        if (recv_all(sock, &header, sizeof(header)) <= 0) {
            fprintf(stderr, "Connection closed by server unexpectedly.\n");
            break;
        }

        uint32_t payload_len = ntohl(header.length);
        total_bytes_received += sizeof(header) + payload_len;

        char *payload = NULL;
        if (payload_len > 0) {
            payload = malloc(payload_len);
            if (recv_all(sock, payload, payload_len) <= 0) {
                free(payload);
                break;
            }
        }

        if (header.type == TYPE_ERROR) {
            fprintf(stderr, "Server Error: %.*s\n", payload_len, payload);
            free(payload);
            break;
        }
        else if (header.type == TYPE_TOPIC_RESPONSE) {
            uint8_t status = payload[0];
            if (status == STATUS_NOT_FOUND) {
                printf("Topic '%s' not found on server.\n", topic);
                free(payload);
                break;
            }
            uint32_t total_files;
            memcpy(&total_files, payload + 1, 4);
            total_files = ntohl(total_files);
            printf("Topic found. Preparing to receive %u files.\n", total_files);
            free(payload);

            // §2 model: Client sends MSG_START after receiving OK / TOPIC_RESPONSE
            msg_header_t start = make_header(TYPE_MSG_START, txid, 0);
            send_all(sock, &start, sizeof(start));
            continue;
        }
        else if (header.type == TYPE_FILE_META) {
            uint32_t idx; memcpy(&idx, payload, 4); idx = ntohl(idx);
            uint64_t size; memcpy(&size, payload + 4, 8); size = ntohll(size);
            uint16_t nlen; memcpy(&nlen, payload + 12, 2); nlen = ntohs(nlen);

            char fname[1024];
            memcpy(fname, payload + 14, nlen);
            fname[nlen] = '\0';

            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, fname);

            if (current_file) fclose(current_file);
            current_file = fopen(filepath, "wb");
            if (!current_file) perror("fopen");
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
            // §2 model: CLOSE received – transfer complete
            printf("All files received successfully.\n");
            free(payload);
            break;
        }

        if (payload) free(payload);
    }

    if (current_file) fclose(current_file);
    close(sock);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double time_taken = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    double transfer_rate = (total_bytes_received / 1024.0 / 1024.0) / time_taken;

    printf("\n--- Performance Metrics ---\n");
    printf("Total Bytes Received  : %lu\n", total_bytes_received);
    printf("Useful Bytes (Data)   : %lu\n", useful_bytes_received);
    printf("Time Taken            : %.4f seconds\n", time_taken);
    printf("Transfer Rate (Total) : %.2f MB/s\n", transfer_rate);

    return 0;
}
