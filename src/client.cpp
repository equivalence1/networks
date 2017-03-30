#include <tcp_socket.h>

int main(int argc, char *argv[])
{
    struct tcp_client_socket *sock = new tcp_client_socket("localhost", 40001);
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
