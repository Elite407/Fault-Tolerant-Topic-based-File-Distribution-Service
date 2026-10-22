#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include "protocol.h"

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

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *topic = argv[3];
    const char *output_dir = argv[4];

    char topic_dir[2048];
    snprintf(topic_dir, sizeof(topic_dir), "%s/%s", output_dir, topic);
    mkdir_p(topic_dir);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    uint64_t total_bytes_received = 0;
    uint64_t useful_bytes_received = 0;
    uint32_t reconnects = 0;

    int success = 0;
    
    // Checkpoint State
    uint64_t session_id = 0;
    uint32_t current_file_idx = 0; // Fixed from 0xFFFFFFFF
    uint64_t current_offset = 0;
    char current_filename[1024] = {0};

    while (!success) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            perror("socket");
            exit(1);
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("inet_pton");
            exit(1);
        }

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            perror("connect");
            sleep(1);
            close(sock);
            continue;
        }

        msg_header_t header;
        uint32_t txid = 1; // transaction_id for this session
        
        if (session_id == 0) {
            header = make_header(TYPE_GET_REQUEST, txid, strlen(topic));
            send_all(sock, &header, sizeof(header));
            send_all(sock, topic, strlen(topic));
        } else {
            printf("[CASE 2] Reconnecting. Session: %lu, File: %u, Offset: %lu\n", session_id, current_file_idx, current_offset);
            reconnects++;
            
            uint32_t topic_len = strlen(topic);
            header = make_header(TYPE_RECONNECT_REQ, txid, 20 + topic_len);
            send_all(sock, &header, sizeof(header));
            
            uint64_t net_sid = htonll(session_id);
            uint32_t net_idx = htonl(current_file_idx);
            uint64_t net_off = htonll(current_offset);
            
            send_all(sock, &net_sid, 8);
            send_all(sock, &net_idx, 4);
            send_all(sock, &net_off, 8);
            send_all(sock, topic, topic_len);
        }

        FILE *current_file = NULL;
        if (current_filename[0] != '\0') {
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, current_filename);
            current_file = fopen(filepath, "r+b"); // Open for read/update
            if (!current_file) {
                current_file = fopen(filepath, "wb"); // Fallback
            }
            if (current_file) {
                fseek(current_file, current_offset, SEEK_SET);
            }
        }
        
        int error_occurred = 0;

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
                    printf("Topic not found.\n");
                    free(payload);
                    error_occurred = 1;
                    break;
                }
                free(payload);
                // §2 model: send MSG_START after receiving OK / TOPIC_RESPONSE
                msg_header_t start = make_header(TYPE_MSG_START, txid, 0);
                send_all(sock, &start, sizeof(start));
                continue;
            }
            else if (header.type == TYPE_SESSION_CREATED) {
                memcpy(&session_id, payload, 8);
                session_id = ntohll(session_id);
                printf("Session created: %lu\n", session_id);
            }
            else if (header.type == TYPE_RESUME_RESPONSE) {
                uint8_t status = payload[0];
                if (status == RESUME_REJECTED) {
                    printf("Server rejected resume. Restarting from scratch.\n");
                    session_id = 0;
                    current_file_idx = 0;
                    current_offset = 0;
                    current_filename[0] = '\0';
                    free(payload);
                    error_occurred = 1;
                    break;
                }
            }
            else if (header.type == TYPE_FILE_META) {
                uint32_t idx; memcpy(&idx, payload, 4); idx = ntohl(idx);
                uint16_t nlen; memcpy(&nlen, payload + 12, 2); nlen = ntohs(nlen);
                char fname[1024];
                memcpy(fname, payload + 14, nlen);
                fname[nlen] = '\0';
                
                if (idx != current_file_idx || current_file == NULL) {
                    current_file_idx = idx;
                    current_offset = 0;
                    strncpy(current_filename, fname, sizeof(current_filename)-1);
                    
                    char filepath[4096];
                    snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, fname);
                    
                    if (current_file) fclose(current_file);
                    current_file = fopen(filepath, "ab"); // Append binary safely
                }
            }
            else if (header.type == TYPE_FILE_CHUNK) {
                uint32_t idx; memcpy(&idx, payload, 4); idx = ntohl(idx);
                uint64_t offset; memcpy(&offset, payload + 4, 8); offset = ntohll(offset);
                uint32_t data_len = payload_len - 12;
                
                if (current_file) {
                    fseek(current_file, offset, SEEK_SET);
                    size_t w = fwrite(payload + 12, 1, data_len, current_file);
                    if (w == data_len) {
                        current_offset = offset + data_len;
                        useful_bytes_received += data_len;
                    }
                }
            }
            else if (header.type == TYPE_FILE_EOF) {
                if (current_file) {
                    fclose(current_file);
                    current_file = NULL;
                }
                current_file_idx++;
                current_offset = 0;
                current_filename[0] = '\0';
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
        
        if (!success && error_occurred) {
            usleep(100000); 
        } else if (!success) {
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double time_taken = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    double transfer_rate = (useful_bytes_received / 1024.0 / 1024.0) / time_taken;

    printf("\n--- Performance Metrics (Case 2) ---\n");
    printf("Total Bytes Received  : %lu\n", total_bytes_received);
    printf("Useful Bytes (Data)   : %lu\n", useful_bytes_received);
    printf("Redundant Bytes       : %lu\n", total_bytes_received - useful_bytes_received);
    printf("Reconnects            : %u\n", reconnects);
    printf("Time Taken            : %.4f seconds\n", time_taken);
    printf("Throughput (Useful)   : %.2f MB/s\n", transfer_rate);

    return 0;
}
