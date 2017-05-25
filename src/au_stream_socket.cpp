#include <au_stream_socket.h>

au_stream_socket::au_stream_socket()
{
}

au_stream_socket::au_stream_socket(int sockfd)
{
}

void au_stream_socket::send(const void *buff, size_t size)
{
}

void au_stream_socket::recv(void *buff, size_t size)
{
}

au_stream_socket::~au_stream_socket()
{
}

// ==========

au_stream_client_socket::au_stream_client_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
}

void au_stream_client_socket::connect()
{
}

// ===========

au_stream_server_socket::au_stream_server_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
}

stream_socket* au_stream_server_socket::accept_one_client()
{
    return NULL;
}

au_stream_server_socket::~au_stream_server_socket()
{
}
