#include "inc/net.h"
#include <stdio.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

struct sockaddr_in* get_addr4(const char* addr, const char* port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    struct addrinfo* info;
    int ret;
    if ((ret = getaddrinfo(addr, port, &hints, &info)) != 0) {
        fprintf(stderr, "getaddr4: getaddrinfo failed, %d\n", ret);
        return NULL;
    }

    struct sockaddr_in* res = calloc(1, sizeof(*res));
    if (res == NULL) {
        fprintf(stderr, "getaddr4: calloc failed\n");
        freeaddrinfo(info);
        return NULL;
    }
    memcpy(res, info->ai_addr, sizeof(*info->ai_addr));

    freeaddrinfo(info);
    return res;
}

int conn_tcpalive(int sockfd) {
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &((int) {1}), sizeof(int)) == -1) {
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &((struct timeval) {.tv_sec = 3}), sizeof(struct timeval)) == -1) {
        return -1;
    }

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &((int) {3}), sizeof(int)) == -1) {
        return -1;
    }

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &((int) {1}), sizeof(int)) == -1) {
        return -1;
    }

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &((int) {2}), sizeof(int)) == -1) {
        return -1;
    }

    return 1;
}

uint8_t* conn_read(int sockfd, size_t* size) {
    size_t sz;
    size_t n = recv(sockfd, &sz, sizeof(sz), 0);
    if (n == 0) {
        *size = 0;
        return NULL;
    }

    if (n != sizeof(sz)) {
        fprintf(stderr, "conn_read: failed to read size of msg %zu %d\n", n, errno);
        return NULL;
    }

    uint8_t* buf = calloc(sz, sizeof(*buf));
    if (buf == NULL) {
        fprintf(stderr, "conn_read: calloc failed\n");
        return NULL;
    }

    n = recv(sockfd, buf, sz, MSG_WAITALL);
    if ((size_t) n != sz) {
        fprintf(stderr, "conn_read: failed to read msg\n");
        return NULL;
    }

    *size = sz;
    return buf;
}

int conn_write(int sockfd, const uint8_t* value, size_t sz) {
    size_t n = send(sockfd, &sz, sizeof(sz), 0);
    if (n != sizeof(sz)) {
        fprintf(stderr, "conn_write: failed to write size of msg\n");
        return -1;
    }

    n = send(sockfd, value, sz, 0);
    if (n != sz) {
        fprintf(stderr, "conn_write: Failed to write msg\n");
        return -1;
    }
    return 0;
}
