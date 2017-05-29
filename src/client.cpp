#include <tcp_socket.h>
#include <au_stream_socket.h>
#include <common.h>
#include <protocol.h>
#include <opcode.h>

#include <stdint.h>
#include <string.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <exception>

static const char *host = "localhost";
static uint16_t port = 40001;
static struct stream_client_socket *sock;

static uint16_t client_port = 123;

/* we only need this class to wrap all data needed
 * to send/recv in background job and pass it as
 * (void *arg) in pthread_create
 *
 * think of it as a JavaBean
 */
class op_data_wrapper {
public:
    op_data_wrapper(stream_client_socket *sock, void *buff, int len)
    {
        this->sock = sock;
        this->buff = buff;
        this->len = len;
    }

    stream_client_socket* get_sock()
    {
        return this->sock;
    }

    void* get_buff()
    {
        return buff;
    }

    int get_len()
    {
        return len;
    }

    // when background job finished, we don't need sock and buff anymore
    // so just free them
    ~op_data_wrapper() {
        free(buff);
        delete sock;
    }

private:
    stream_client_socket *sock;
    void *buff;
    int len;
};

/*
 * just send buffer on server, wait for response and print it
 */
static
void send_and_recv(struct stream_socket *sock, void *buff, int len)
{
    sock->send(buff, len);
    int32_t result = get_response(sock);
    pr_success("%d\n", result);
}

static
void* backgroud_op(void *data)
{
    try {
        op_data_wrapper *op_data = (op_data_wrapper *)data;
        op_data->get_sock()->connect();
        send_and_recv(op_data->get_sock(), op_data->get_buff(), op_data->get_len());
        delete op_data;
    } catch (...) {
        handle_eptr(std::current_exception());
    }
    return NULL;
}

/*
 * handle user's input.
 *
 * parse it into a buffer as specified by our protocol,
 * if operation is blocking, then send and recv result it a
 * current thread, do it in background otherwise
 */
static
void handle(const std::string &user_input)
{
    int len;
    void *buff;

    try {
        buff = user_to_net(user_input, &len);
    } catch (...) {
        handle_eptr(std::current_exception());
        return;
    }

    if (is_blocking(buff)) {
        send_and_recv(sock, buff, len);
        free(buff);
    } else {
        // we create it on stack cuz we don't need it afterwards, we wont join new thread
        pthread_t thread;
        /* 
         * we need new connection for non-blocking operations.
         * without it our next call to `recv` in main thread
         * will wait for our `recv` call made to get result of
         * `async` operation (fact, fib)
         *
         * Even if I add something like message id in protocol
         * and will recv results of non-blocking operations
         * while performing blocking ones, I won't be able to
         * recv result while user is typing its request
         *
         * So as i see, this is the only way (though ugly) to do it.
         * We could theoretically just poll our main connection for a result
         * in background, but we can't specify NO_WAIT flag in our recv.
         */
        stream_client_socket *new_connection;
        if (is_tcp())
            new_connection = new tcp_client_socket(host, port);
        else
            new_connection = new au_stream_client_socket(host, client_port++, port);
        op_data_wrapper *odw = new op_data_wrapper(new_connection, buff, len);
        if (pthread_create(&thread, NULL, backgroud_op, odw) < 0)
            pr_warn("%s\n", "Could not start thread for background operation");
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = atoi(argv[2]);

    try {
        if (is_tcp()) {
            pr_info("%s\n", "using TCP sockets");
            sock = new tcp_client_socket(host, port);
        } else {
            pr_info("%s\n", "using AU sockets");
            sock = new au_stream_client_socket(host, client_port++, port);
        }
        sock->connect();

        std::string user_input;
        while (!std::cin.eof()) {
            getline(std::cin, user_input);
            if (user_input == "")
                continue;
            if (user_input == "exit") {
                delete sock;
                break;
            }
            handle(user_input);
        }
    } catch (...) {
        handle_eptr(std::current_exception());
        return 0;
    }
}
