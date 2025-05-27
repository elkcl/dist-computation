#include "inc/net.h"
#include "inc/server.h"
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


int server_init(const char *addr, const char *port) {
    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd == -1) {
        fprintf(stderr, "server_init: can't create socket\n");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &((int) {1}), sizeof(int)) == -1) {
        fprintf(stderr, "server_init: can't set SO_REUSEADDR\n");
        return -1;
    }

    struct sockaddr_in* listen_addr = get_addr4(addr, port);

    if (bind(sockfd, (struct sockaddr*) listen_addr, sizeof(*listen_addr)) == -1) {
        free(listen_addr);
        fprintf(stderr, "server_init: socket cannot be bound to the mortal realm\n");
        return -1;
    }

    free(listen_addr);

    if (listen(sockfd, CONN_QUEUE_SIZE) == -1) {
        fprintf(stderr, "server_init: can't listen(), socket possibly deaf\n");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &((struct linger) {.l_onoff = 1, .l_linger = 1}), sizeof(struct linger)) == -1) {
        fprintf(stderr, "server_init: can't set SO_LINGER\n");
        return -1;
    }

    return sockfd;
}

int server_try_accept(int sockfd) {
    int connfd = accept(sockfd, NULL, NULL);
    if (connfd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2;
        }

        fprintf(stderr, "server_try_accept: can't accept() connection on a socket\n");
        return -1;
    }

    if (setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &((int) {1}), sizeof(int)) == -1) {
        fprintf(stderr, "server_try_accept: can't set TCP_NODELAY\n");
        return -1;
    }

    if (setsockopt(connfd, IPPROTO_TCP, TCP_CORK, &((int) {0}), sizeof(int)) == -1) {
        fprintf(stderr, "server_try_accept: can't unset TCP_CORK\n");
        return -1;
    }

    if (conn_tcpalive(connfd) == -1) {
        fprintf(stderr, "server_try_accept: can't configure TCPALIVE\n");
        return -1;
    }

    printf("server_try_accept: new connection\n");
    return connfd;
}
