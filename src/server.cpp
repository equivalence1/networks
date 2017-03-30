#include <tcp_socket.h>
#include <stream_socket.h>

#include <stdio.h>

int main(int argc, char *argv[])
{
    struct tcp_server_socket *server_socket = new tcp_server_socket("localhost", 40001);
    stream_socket *cli_socket = server_socket->accept_one_client();
    char buff[100];
    cli_socket->recv(buff, sizeof buff);
    printf("received %s\n", buff);
}
