#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include "protocol.h"

#define ACK_INTERVAL 65536

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

// ---- Work-Stealing Pool (Items #8 & #9) ----
typedef struct {
    uint64_t session_id;
    uint32_t file_idx;
    uint64_t offset;
    uint32_t worker_id;
    uint32_t total_workers;
} work_unit_t;

typedef struct {
    work_unit_t *items;
    int          count;
    int          cap;
    pthread_mutex_t mu;
    pthread_cond_t  cv;
} work_pool_t;

static work_pool_t g_pool;

void pool_init(int capacity) {
    g_pool.items = calloc(capacity, sizeof(work_unit_t));
    g_pool.cap   = capacity;
    g_pool.count = 0;
    pthread_mutex_init(&g_pool.mu, NULL);
    pthread_cond_init(&g_pool.cv, NULL);
}

void pool_put(uint64_t sid, uint32_t file_idx, uint64_t offset, uint32_t wid, uint32_t tot) {
    pthread_mutex_lock(&g_pool.mu);
    if (g_pool.count < g_pool.cap) {
        g_pool.items[g_pool.count].session_id     = sid;
        g_pool.items[g_pool.count].file_idx       = file_idx;
        g_pool.items[g_pool.count].offset         = offset;
        g_pool.items[g_pool.count].worker_id      = wid;
        g_pool.items[g_pool.count].total_workers  = tot;
        g_pool.count++;
    }
    pthread_cond_signal(&g_pool.cv);
    pthread_mutex_unlock(&g_pool.mu);
}

int pool_take(work_unit_t *out) {
    pthread_mutex_lock(&g_pool.mu);
    if (g_pool.count > 0) {
        *out = g_pool.items[--g_pool.count];
        pthread_mutex_unlock(&g_pool.mu);
        return 1;
    }
    pthread_mutex_unlock(&g_pool.mu);
    return 0;
}

void pool_destroy(void) {
    free(g_pool.items);
    pthread_mutex_destroy(&g_pool.mu);
    pthread_cond_destroy(&g_pool.cv);
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

// Executes a worker transfer session (initial worker or stolen session).
// Returns 1 if session finished (TYPE_TOPIC_EOF), 0 otherwise.
int run_worker_transfer(worker_args_t *args, uint64_t initial_sid,
                        uint32_t initial_file_idx, uint64_t initial_offset,
                        uint64_t *local_total, uint64_t *local_useful,
                        uint32_t *local_reconnects) {
    uint64_t session_id = initial_sid;
    uint32_t current_file_idx = initial_file_idx;
    uint64_t current_offset = initial_offset;
    char current_filename[1024] = {0};
    uint64_t bytes_since_ack = 0;
    int success = 0;
    int topic_not_found = 0;

    int attempts = 0;
    while (!success && !topic_not_found && attempts < 10) {
        attempts++;
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) { perror("socket"); return 0; }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(args->port);
        if (inet_pton(AF_INET, args->server_ip, &server_addr.sin_addr) <= 0) {
            close(sock);
            return 0;
        }

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            close(sock);
            usleep(100000);
            continue;
        }

        int nodelay_flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(nodelay_flag));

        msg_header_t header;
        uint32_t txid = 1;

        if (session_id == 0) {
            uint32_t topic_len = strlen(args->topic);
            header = make_header(TYPE_GET_REQUEST_WORKER, txid, 8 + topic_len);
            send_all(sock, &header, sizeof(header));

            uint32_t net_wid = htonl(args->worker_id);
            uint32_t net_tot = htonl(args->total_workers);
            send_all(sock, &net_wid, 4);
            send_all(sock, &net_tot, 4);
            send_all(sock, args->topic, topic_len);
        } else {
            (*local_reconnects)++;
            uint32_t topic_len = strlen(args->topic);
            header = make_header(TYPE_RECONNECT_REQ, txid, 20 + topic_len);
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
            if (current_file && current_offset > 0) fseek(current_file, current_offset, SEEK_SET);
        }

        int error_occurred = 0;

        while (1) {
            if (recv_all(sock, &header, sizeof(header)) <= 0) {
                error_occurred = 1;
                break;
            }

            uint32_t payload_len = ntohl(header.length);
            *local_total += sizeof(header) + payload_len;

            char *payload = NULL;
            if (payload_len > 0) {
                payload = malloc(payload_len);
                if (!payload || recv_all(sock, payload, payload_len) <= 0) {
                    if (payload) free(payload);
                    error_occurred = 1;
                    break;
                }
            }

            if (header.type == TYPE_ERROR) {
                if (payload) free(payload);
                error_occurred = 1;
                break;
            }
            else if (header.type == TYPE_TOPIC_RESPONSE) {
                uint8_t status = payload[0];
                if (status == STATUS_NOT_FOUND) {
                    fprintf(stderr, "[ERROR] Topic '%s' not found on server. Exiting.\n", args->topic);
                    free(payload);
                    topic_not_found = 1;
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
            }
            else if (header.type == TYPE_RESUME_RESPONSE) {
                uint8_t status = payload[0];
                if (status == RESUME_REJECTED) {
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

                if (idx != current_file_idx) {
                    current_file_idx = idx;
                    current_offset = 0;
                    strncpy(current_filename, fname, sizeof(current_filename)-1);

                    char filepath[4096];
                    snprintf(filepath, sizeof(filepath), "%s/%s", args->topic_dir, fname);

                    if (current_file) fclose(current_file);
                    current_file = fopen(filepath, "wb");
                } else if (current_file == NULL) {
                    strncpy(current_filename, fname, sizeof(current_filename)-1);
                    char filepath[4096];
                    snprintf(filepath, sizeof(filepath), "%s/%s", args->topic_dir, fname);
                    current_file = fopen(filepath, "r+b");
                    if (!current_file) current_file = fopen(filepath, "wb");
                    if (current_file && current_offset > 0) {
                        fseek(current_file, current_offset, SEEK_SET);
                    }
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
                        *local_useful += data_len;

                        // Send client ACK
                        bytes_since_ack += data_len;
                        if (bytes_since_ack >= ACK_INTERVAL) {
                            bytes_since_ack = 0;
                            fflush(current_file);
                            msg_header_t ack_hdr = make_header(TYPE_CLIENT_ACK, txid, 12);
                            uint32_t net_ack_idx = htonl(current_file_idx);
                            uint64_t net_ack_off = htonll(current_offset);
                            char ack_buf[sizeof(msg_header_t) + 12];
                            memcpy(ack_buf, &ack_hdr, sizeof(ack_hdr));
                            memcpy(ack_buf + sizeof(ack_hdr), &net_ack_idx, 4);
                            memcpy(ack_buf + sizeof(ack_hdr) + 4, &net_ack_off, 8);
                            send_all(sock, ack_buf, sizeof(ack_buf));
                        }
                    }
                }
            }
            else if (header.type == TYPE_FILE_EOF) {
                if (current_file) {
                    fflush(current_file);
                    msg_header_t ack_hdr = make_header(TYPE_CLIENT_ACK, txid, 12);
                    uint32_t net_ack_idx = htonl(current_file_idx);
                    uint64_t net_ack_off = htonll(current_offset);
                    char ack_buf[sizeof(msg_header_t) + 12];
                    memcpy(ack_buf, &ack_hdr, sizeof(ack_hdr));
                    memcpy(ack_buf + sizeof(ack_hdr), &net_ack_idx, 4);
                    memcpy(ack_buf + sizeof(ack_hdr) + 4, &net_ack_off, 8);
                    send_all(sock, ack_buf, sizeof(ack_buf));

                    fclose(current_file);
                    current_file = NULL;
                }
                current_offset = 0;
                current_filename[0] = '\0';
                bytes_since_ack = 0;
            }
            else if (header.type == TYPE_TOPIC_EOF) {
                success = 1;
                if (payload) free(payload);
                break;
            }

            if (payload) free(payload);
        }

        if (current_file) fclose(current_file);
        close(sock);

        if (!success && topic_not_found) {
            break;
        } else if (!success && error_occurred) {
            usleep(100000);
        }
    }

    if (!success && session_id != 0) {
        printf("[WORKER %u] Handing off session %lu (file=%u, offset=%lu) to work-stealing pool\n",
               args->worker_id, session_id, current_file_idx, current_offset);
        pool_put(session_id, current_file_idx, current_offset, args->worker_id, args->total_workers);
    }

    return success;
}

void *worker_thread(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    uint64_t local_total = 0;
    uint64_t local_useful = 0;
    uint32_t local_reconnects = 0;

    // 1. Run primary partition transfer
    run_worker_transfer(args, 0, 0, 0, &local_total, &local_useful, &local_reconnects);

    // 2. Scavenger phase: steal and complete orphaned work units from the pool
    work_unit_t wu;
    while (pool_take(&wu)) {
        printf("[WORKER %u] Scavenging orphaned work from pool: session=%lu, file=%u, offset=%lu\n",
               args->worker_id, wu.session_id, wu.file_idx, wu.offset);
        run_worker_transfer(args, wu.session_id, wu.file_idx, wu.offset,
                            &local_total, &local_useful, &local_reconnects);
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

    pool_init(256);

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
    pool_destroy();

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
