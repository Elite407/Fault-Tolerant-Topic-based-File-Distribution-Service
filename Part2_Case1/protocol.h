#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// Message Types  (aligned with instructor model design)
#define TYPE_GET_REQUEST    0x01  // Client -> Server: topic request
#define TYPE_TOPIC_RESPONSE 0x02  // Server -> Client: OK / NOT FOUND + file list
#define TYPE_FILE_META      0x03  // Server -> Client: per-file header (idx, size, name)
#define TYPE_FILE_CHUNK     0x04  // Server -> Client: file data chunk
#define TYPE_FILE_EOF       0x05  // Server -> Client: end of one file
#define TYPE_ERROR          0x06  // Server -> Client: error with message
#define TYPE_TOPIC_EOF      0x07  // Server -> Client: all files sent (CLOSE)
#define TYPE_MSG_START      0x08  // Client -> Server: ready to receive files

// Status Codes
#define STATUS_NOT_FOUND 0
#define STATUS_FOUND     1

// Chunk Size for reading/sending files
#define CHUNK_SIZE 65536

// 9-byte header: type (1) + transaction_id (4) + length (4)
// transaction_id is used to match request/response pairs
#pragma pack(push, 1)
typedef struct {
    uint8_t  type;           // Message Type
    uint32_t transaction_id; // Request/response correlation ID (network byte order)
    uint32_t length;         // Length of payload (Network Byte Order)
} msg_header_t;
#pragma pack(pop)

// Utility to send exactly 'len' bytes
ssize_t send_all(int sockfd, const void *buf, size_t len);

// Utility to receive exactly 'len' bytes
ssize_t recv_all(int sockfd, void *buf, size_t len);

// 64-bit byte swapping utilities (for offsets and sizes)
uint64_t htonll(uint64_t value);
uint64_t ntohll(uint64_t value);

#endif
