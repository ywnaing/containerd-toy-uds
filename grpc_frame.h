#ifndef GRPC_FRAME_H
#define GRPC_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>

#define GRPC_HEADER_SIZE 5

// Helper: Ensure exactly 'count' bytes are read from socket stream
static inline ssize_t read_exact(int fd, void *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, (char *)buf + total, count - total);
        if (n <= 0) {
            return n; // 0 = EOF / peer closed, -1 = error
        }
        total += n;
    }
    return (ssize_t)total;
}

// Helper: Ensure exactly 'count' bytes are written to socket stream
static inline ssize_t write_exact(int fd, const void *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = write(fd, (const char *)buf + total, count - total);
        if (n <= 0) {
            return n; // -1 = error
        }
        total += n;
    }
    return (ssize_t)total;
}

// Send a gRPC length-prefixed frame:
// [ 1 byte: compressed flag (0) ] + [ 4 bytes: big-endian payload length ] + [ payload ]
static inline bool grpc_send_frame(int fd, const void *payload, uint32_t payload_len) {
    uint8_t header[GRPC_HEADER_SIZE];
    header[0] = 0; // 0 = uncompressed
    uint32_t net_len = htonl(payload_len); // Convert to big-endian (network byte order)
    memcpy(&header[1], &net_len, 4);

    // 1. Send 5-byte header
    if (write_exact(fd, header, GRPC_HEADER_SIZE) != GRPC_HEADER_SIZE) {
        return false;
    }

    // 2. Send payload
    if (payload_len > 0) {
        if (write_exact(fd, payload, payload_len) != (ssize_t)payload_len) {
            return false;
        }
    }

    return true;
}

// Read and decode a gRPC length-prefixed frame:
static inline bool grpc_recv_frame(int fd, uint8_t *out_payload, size_t max_buf_size, uint32_t *out_len) {
    uint8_t header[GRPC_HEADER_SIZE];

    // 1. Read exactly 5-byte gRPC header
    ssize_t n = read_exact(fd, header, GRPC_HEADER_SIZE);
    if (n <= 0) {
        return false; // Connection closed or read error
    }

    uint8_t compressed = header[0];
    (void)compressed;

    uint32_t net_len;
    memcpy(&net_len, &header[1], 4);
    uint32_t payload_len = ntohl(net_len); // Convert from big-endian to host order

    if (payload_len > max_buf_size) {
        fprintf(stderr, "[gRPC Frame] Error: Payload size %u exceeds buffer %zu\n",
                payload_len, max_buf_size);
        return false;
    }

    // 2. Read exactly payload_len bytes
    if (payload_len > 0) {
        if (read_exact(fd, out_payload, payload_len) != (ssize_t)payload_len) {
            return false;
        }
    }

    *out_len = payload_len;
    return true;
}

#endif // GRPC_FRAME_H
