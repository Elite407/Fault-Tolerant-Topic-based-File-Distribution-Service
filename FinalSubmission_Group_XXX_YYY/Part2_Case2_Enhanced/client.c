#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
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

typedef struct {
    char server_ip[256];
    int port;
    char topic[256];
    char topic_dir[2048];
    uint32_t worker_id;
    uint32_t total_workers;
    uint64_t *total_bytes_received;
    uint64_t *useful_bytes_received;
    uint32_t *reconnects;
    pthread_mutex_t *metrics_mutex;
} worker_args_t;

void *worker_thread(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    
    uint64_t local_total = 0;
    uint64_t local_useful = 0;
    uint32_t local_reconnects = 0;

    int success = 0;
    uint64_t session_id = 0;
    uint32_t current_file_idx = 0xFFFFFFFF;
    uint64_t current_offset = 0;
    char current_filename[1024] = {0};

    while (!success) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) { perror("socket"); exit(1); }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(args->port);
        if (inet_pton(AF_INET, args->server_ip, &server_addr.sin_addr) <= 0) exit(1);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            sleep(1);
            close(sock);
            continue;
        }

        msg_header_t header;
        
        if (session_id == 0) {
            uint32_t topic_len = strlen(args->topic);
            header.length = htonl(8 + topic_len);
            header.type = TYPE_GET_REQUEST_WORKER;
            send_all(sock, &header, sizeof(header));
            
            uint32_t net_wid = htonl(args->worker_id);
            uint32_t net_tot = htonl(args->total_workers);
            send_all(sock, &net_wid, 4);
            send_all(sock, &net_tot, 4);
            send_all(sock, args->topic, topic_len);
        } else {
            local_reconnects++;
            uint32_t topic_len = strlen(args->topic);
            header.length = htonl(20 + topic_len);
            header.type = TYPE_RECONNECT_REQ;
            send_all(sock, &header, sizeof(header));
            
            uint64_t net_sid = htonll(session_id);
            uint32_t net_idx = htonl(current_file_idx);
            uint64_t net_off = htonll(current_offset);
            
            send_all(sock, &net_sid, 8);
            send_all(sock, &net_idx, 4);
            send_all(sock, &net_off, 8);
            send_all(sock, args->topic, topic_len);
        }

        FILE *current_file = NULL;
        if (current_filename[0] != '\0') {
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s", args->topic_dir, current_filename);
            current_file = fopen(filepath, "r+b");
            if (!current_file) current_file = fopen(filepath, "wb");
            if (current_file) fseek(current_file, current_offset, SEEK_SET);
        }
        
        int error_occurred = 0;

        while (1) {
            if (recv_all(sock, &header, sizeof(header)) <= 0) {
                error_occurred = 1;
                break;
            }
            
            uint32_t payload_len = ntohl(header.length);
            local_total += sizeof(header) + payload_len;
            
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
                free(payload);
                error_occurred = 1;
                break;
            } 
            else if (header.type == TYPE_TOPIC_RESPONSE) {
                uint8_t status = payload[0];
                if (status == STATUS_NOT_FOUND) {
                    free(payload);
                    error_occurred = 1;
                    break;
                }
            }
            else if (header.type == TYPE_SESSION_CREATED) {
                memcpy(&session_id, payload, 8);
                session_id = ntohll(session_id);
            }
            else if (header.type == TYPE_RESUME_RESPONSE) {
                uint8_t status = payload[0];
                if (status == RESUME_REJECTED) {
                    session_id = 0;
                    current_file_idx = 0xFFFFFFFF;
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
                    snprintf(filepath, sizeof(filepath), "%s/%s", args->topic_dir, fname);
                    
                    if (current_file) fclose(current_file);
                    current_file = fopen(filepath, "ab");
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
                        local_useful += data_len;
                    }
                }
            }
            else if (header.type == TYPE_FILE_EOF) {
                if (current_file) {
                    fclose(current_file);
                    current_file = NULL;
                }
                current_offset = 0;
                current_filename[0] = '\0';
            }
            else if (header.type == TYPE_TOPIC_EOF) {
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

    pthread_mutex_lock(args->metrics_mutex);
    *(args->total_bytes_received) += local_total;
    *(args->useful_bytes_received) += local_useful;
    *(args->reconnects) += local_reconnects;
    pthread_mutex_unlock(args->metrics_mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <topic> <output_dir> [num_connections]\n", argv[0]);
        exit(1);
    }

    uint32_t num_connections = 4; // Default parallel connections
    if (argc >= 6) {
        num_connections = atoi(argv[5]);
        if (num_connections < 1) num_connections = 1;
        if (num_connections > 64) num_connections = 64;
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
    pthread_mutex_t metrics_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t *threads = malloc(num_connections * sizeof(pthread_t));
    worker_args_t *args = malloc(num_connections * sizeof(worker_args_t));

    for (uint32_t i = 0; i < num_connections; i++) {
        strncpy(args[i].server_ip, server_ip, 255);
        args[i].port = port;
        strncpy(args[i].topic, topic, 255);
        strncpy(args[i].topic_dir, topic_dir, 2047);
        args[i].worker_id = i;
        args[i].total_workers = num_connections;
        args[i].total_bytes_received = &total_bytes_received;
        args[i].useful_bytes_received = &useful_bytes_received;
        args[i].reconnects = &reconnects;
        args[i].metrics_mutex = &metrics_mutex;
        
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (uint32_t i = 0; i < num_connections; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    free(args);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double time_taken = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    double transfer_rate = (useful_bytes_received / 1024.0 / 1024.0) / time_taken;

    printf("\n--- Performance Metrics (Case 2 Enhanced) ---\n");
    printf("Connections           : %u\n", num_connections);
    printf("Total Bytes Received  : %lu\n", total_bytes_received);
    printf("Useful Bytes (Data)   : %lu\n", useful_bytes_received);
    printf("Redundant Bytes       : %lu\n", total_bytes_received - useful_bytes_received);
    printf("Reconnects            : %u\n", reconnects);
    printf("Time Taken            : %.4f seconds\n", time_taken);
    printf("Throughput (Useful)   : %.2f MB/s\n", transfer_rate);

    return 0;
}
