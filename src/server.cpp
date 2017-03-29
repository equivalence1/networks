#include <tcp_socket.h>

int main(int argc, char *argv[])
{
    struct tcp_server_socket *sock = new tcp_server_socket("localhost", 40001);
    sock->accept_one_client();
}
