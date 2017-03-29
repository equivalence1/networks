#include <tcp_socket.h>

int main(int argc, char *argv[])
{
    struct tcp_client_socket *sock = new tcp_client_socket("localhost", 40001);
    sock->connect();
}
