#include <tcp_socket.h>
#include <stream_socket.h>
#include <calc.h>
#include <protocol.h>
#include <common.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector>
#include <iostream>
#include <exception>

static const char *host = "localhost";
static uint16_t port = 40001;

// this struct is just for RAII
class buff_holder {
public:
    buff_holder(void *buff) {
        this->buff = buff;
    }

    void* get_buff() {
        return this->buff;
    }

    ~buff_holder() {
        free(this->buff);
    }
private:
    void *buff;
};

// this struct is just for RAII
class sock_holder {
public:
    sock_holder(stream_socket *sock) {
        this->sock = sock;
    }

    stream_socket* get_sock() {
        return this->sock;
    }

    ~sock_holder() {
        delete this->sock;
    }
private:
    stream_socket *sock;
};
/*
 * main handler of a new connection
 */
static
void* connection_handler(void *data)
{
    sock_holder sock_h((stream_socket *)data);

    while (true) {
        try {
            // get msg_len first
            uint16_t msg_len;
            sock_h.get_sock()->recv(&msg_len, sizeof msg_len);
            msg_len = ntohs(msg_len);
            msg_len -= sizeof msg_len;

            printf("msg_len = %d\n", msg_len);

            // now get the rest of the message
            void *buff = malloc(msg_len);
            if (buff == NULL)
                throw "No mem";
            buff_holder holder(buff); // RAII -- this done just to free buff in case of error

            sock_h.get_sock()->recv((char *)buff, msg_len);
            std::pair<uint8_t, std::vector<int32_t> > requies_pair = net_to_operands(buff, msg_len);

            int32_t res = perform_op(requies_pair.first, requies_pair.second);
            res = htonl(res);

            sock_h.get_sock()->send(&res, sizeof res);
        } catch (...) {
            handle_eptr(std::current_exception());
            return NULL;
        }
    }
}

static
void handle_new_connection(stream_socket *cli_socket)
{
    pthread_t th; // we create it on stack cuz we wont need it afterwards
    if (pthread_create(&th, NULL, connection_handler, cli_socket) < 0)
        pr_warn("%s\n", "Could not create thread to handle new connection");
}

int main(int argc, char *argv[])
{
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = atoi(argv[2]);

    try {
        struct tcp_server_socket *server_socket = new tcp_server_socket(host, port);

        while (true) {
            stream_socket *cli_socket = server_socket->accept_one_client();
            handle_new_connection(cli_socket);
        }
    } catch (const char *s) {
        // this can only happen if accept_one_client or server socket creation failed
        pr_err("Error occurred: %s\n", s);
    }
}
