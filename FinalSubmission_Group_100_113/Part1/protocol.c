#include "protocol.h"
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

ssize_t send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = (const char *)buf;
    while (total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return 0; // Connection closed
        total += n;
    }
    return total;
}

ssize_t recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    char *p = (char *)buf;
    while (total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return 0; // Connection closed
        total += n;
    }
    return total;
}

uint64_t htonll(uint64_t value) {
    int num = 42;
    if (*(char *)&num == 42) { // Little endian
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}

uint64_t ntohll(uint64_t value) {
    return htonll(value); // It's symmetric
}
