#include <tcp_socket.h>

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

    struct tcp_client_socket *sock = new tcp_client_socket(host, port);
    sock->connect();
    sock->send("bla-bla-bla", 10);
    sock->send("1", 1);
    sock->send("2", 1);
    sock->send("3", 1);
    sock->send("4", 1);
    sock->send("5", 1);
    sock->send("6", 1);
    sock->send("7", 1);
    sock->send("8", 1);
    sock->send("9", 1);
    sock->send("0", 1);
}
