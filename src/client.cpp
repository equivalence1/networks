#include <tcp_socket.h>
#include <common.h>
#include <protocol.h>

#include <stdint.h>
#include <string.h>
#include <string>
#include <iostream>

static const char *host = "localhost";
static uint16_t port = 40001;
static struct tcp_client_socket *sock;

void handle(const std::string &user_input)
{
    int len;
    void *buff;

    try {
        buff = user_to_net(user_input, &len);
    } catch (const char *s) {
        pr_warn("%s\n", s);
        return;
    }

    if (is_blocking((uint8_t)(*((char *)buff)))) {
        sock->send(buff, len);
        int32_t result = get_response(sock);
        pr_success("%d\n", result);
    }

    free(buff);
}

int main(int argc, char *argv[])
{
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = atoi(argv[2]);

    try {
        sock = new tcp_client_socket(host, port);
        sock->connect();

        std::string user_input;
        while (!std::cin.eof()) {
            getline(std::cin, user_input);
            if (user_input == "")
                continue;
            handle(user_input);
        }
    } catch (const char *s) {
        // In client code we can just print error and exit if some failure occurred
        pr_err("%s\n", s);
    }
}
