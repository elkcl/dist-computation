#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

constexpr uint32_t CONN_QUEUE_SIZE = 10;

int server_init(const char *addr, const char *port);
int server_try_accept(int sockfd);

#endif  // SERVER_H
