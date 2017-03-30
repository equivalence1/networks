#include <tcp_socket.h>
#include <stream_socket.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char *host = "localhost";
static uint16_t port = 40001;

int main(int argc, char *argv[])
{
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = atoi(argv[2]);

    struct tcp_server_socket *server_socket = new tcp_server_socket(host, port);
    stream_socket *cli_socket = server_socket->accept_one_client();
    char buff[100];
    cli_socket->recv(buff, sizeof buff);
    printf("received %s\n", buff);
}
