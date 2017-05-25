#ifndef __AU_STREAM_SOCKET_H__
#define __AU_STREAM_SOCKET_H__

#include "stream_socket.h"

struct au_stream_socket: virtual stream_socket
{
public:
    au_stream_socket();
    au_stream_socket(int sockfd);
    void send(const void *buff, size_t size);
    void recv(void *buff, size_t size);
    virtual ~au_stream_socket();
protected:
    int sockfd;
};

struct au_stream_client_socket: public au_stream_socket, public stream_client_socket
{
public:
    au_stream_client_socket(const char *hostname, port_t port);
    virtual void connect();
private:
    const char *hostname;
    const port_t port;
    bool connected;
};

struct au_stream_server_socket: public stream_server_socket
{
public:
    au_stream_server_socket(const char *hostname, port_t port);
    stream_socket* accept_one_client();
    ~au_stream_server_socket();
private:
    int sockfd;
    const char *hostname;
    const port_t port;
    const static int backlog = 5;
};

#endif // __TCP_SOCKET_H__
