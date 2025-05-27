#include "inc/client.h"
#include "inc/net.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int connect_to_server(const char* addr, const char* port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "connect_to_server: unable to create socket\n");
        return -1;
    }

    struct sockaddr_in* conn_addr = get_addr4(addr, port);
    if (conn_addr == NULL) {
        fprintf(stderr, "connect_to_server: get_addr4 failed\n");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*) conn_addr, sizeof(*conn_addr)) == -1) {
        free(conn_addr);

        if (errno == ECONNREFUSED) {
            close(sockfd);
            fprintf(stderr, "connect_to_server: connection refused\n");
            return -1;
        }

        fprintf(stderr, "connect_to_master: unable to connect to master");
        return -1;
    }

    if (conn_tcpalive(sockfd) == -1) {
        fprintf(stderr, "connect_to_server: can't enable TCP keepalive\n");
        return -1;
    }

    free(conn_addr);

    return sockfd;
}
