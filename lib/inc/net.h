#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <sys/socket.h>

struct sockaddr_in* get_addr4(const char* addr, const char* port);
int conn_tcpalive(int sockfd);
uint8_t* conn_read(int sockfd, size_t* size);
int conn_write(int sockfd, const uint8_t* data, size_t size);

#endif  // NET_H
