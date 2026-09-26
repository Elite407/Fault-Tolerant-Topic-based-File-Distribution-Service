#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// Message Types
#define TYPE_GET_REQUEST    0x01
#define TYPE_TOPIC_RESPONSE 0x02
#define TYPE_FILE_META      0x03
#define TYPE_FILE_CHUNK     0x04
#define TYPE_FILE_EOF       0x05
#define TYPE_ERROR          0x06
#define TYPE_TOPIC_EOF      0x07

// Status Codes
#define STATUS_NOT_FOUND 0
#define STATUS_FOUND     1

// Chunk Size for reading/sending files
#define CHUNK_SIZE 8192

#pragma pack(push, 1)
typedef struct {
    uint32_t length; // Length of payload (Network Byte Order)
    uint8_t type;    // Message Type
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
