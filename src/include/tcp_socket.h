#ifndef __TCP_SOCKET_H__
#define __TCP_SOCKET_H__

#include "stream_socket.h"

struct tcp_connection_socket: public stream_socket
{
public:
    tcp_connection_socket();
    tcp_connection_socket(int sockfd);
    void send(const void *buff, size_t size);
    void recv(void *buff, size_t size);
protected:
    int sockfd;
};

struct tcp_client_socket: public tcp_connection_socket
{
public:
    tcp_client_socket(const char *hostname, port_t port);
    void connect();
private:
    const char *hostname;
    const port_t port;
};

struct tcp_server_socket: public stream_server_socket
{
public:
    tcp_server_socket(const char *hostname, port_t port);
    stream_socket* accept_one_client();
private:
    int sockfd;
    const char *hostname;
    const port_t port;
    const static int backlog = 5;
};

#endif // __TCP_SOCKET_H__
