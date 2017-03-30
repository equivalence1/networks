#include <tcp_socket.h>
#include <common.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <mutex>

tcp_connection_socket::tcp_connection_socket()
{
    this->sockfd = 0;
}

tcp_connection_socket::tcp_connection_socket(int sockfd): tcp_connection_socket()
{
    this->sockfd = sockfd;
}

void tcp_connection_socket::send(const void *buff, size_t size)
{
    std::lock_guard<std::mutex> lock{this->m};
    if (this->sockfd <= 0)
        throw "Socket is not initialized";
    if (::send(this->sockfd, buff, size, 0) < 0) {
        print_errno();
        throw "Could not send";
    }
}

void tcp_connection_socket::recv(void *buff, size_t size)
{
    std::lock_guard<std::mutex> lock{this->m};
    if (this->sockfd <= 0)
        throw "Socket is not initialized";
    if (::recv(this->sockfd, buff, size, 0) < 0) {
        print_errno();
        throw "Could not recv";
    }
}

tcp_client_socket::tcp_client_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->sockfd < 0) {
        print_errno();
        throw "Could not create tcp socket";
    }

    int one = 1;
    if (setsockopt(this->sockfd, SOL_TCP, TCP_NODELAY, &one, sizeof(int)) < 0) {
        print_errno();
        pr_warn("%s\n", "Could not set TCP_NODELAY option");
    }

    if (setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0) {
        print_errno();
        pr_warn("%s\n", "Could not set SO_REUSEADDR option");
    }
}

void tcp_client_socket::connect()
{
    // so that no thread can send before connection finished
    std::lock_guard<std::mutex> lock{this->m};

    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    char port_s[10];
    snprintf(port_s, sizeof port_s, "%d", this->port);

    struct addrinfo *res, *rp;

    // resolve given hostname to use it in bind
    if (getaddrinfo(this->hostname, port_s, &hints, &res) < 0) {
        print_errno();
        throw "Could not resolve hostname";
    }

    // getaddrinfo can return multiple addresses, so test them all
    // and just take first that succeed in `connect`
    //
    // PS. in our case when we ask for a specific ip/port, it should
    // be unique but this `for` is more general way
    bool connected = false;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (::connect(this->sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            connected = true;
            break;
        }
    }

    freeaddrinfo(res);

    if (!connected)
        throw "Could not connect client socket";

    pr_success("%s\n", "Socket successfully connected");
}

tcp_server_socket::tcp_server_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
    this->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (this->sockfd < 0) {
        print_errno();
        throw "Could not create tcp socket";
    }

    int one = 1;
    if (setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0) {
        print_errno();
        pr_warn("%s\n", "Could not set SO_REUSEADDR option");
    }

    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // we need this flag for server socket to perform bind/listen/accept
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    char port_s[10];
    snprintf(port_s, sizeof port_s, "%d", this->port);

    struct addrinfo *res, *rp;

    // resolve given hostname to use it in bind
    if (getaddrinfo(this->hostname, port_s, &hints, &res) < 0) {
        print_errno();
        throw "Could not resolve hostname";
    }

    // getaddrinfo can return multiple addresses, so test them all
    // and just take first that succeed in `bind`
    //
    // PS. in our case when we ask for a specific ip/port, it should
    // be unique but this `for` is more general way
    bool binded = false;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (bind(this->sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            binded = true;
            break;
        }
    }

    freeaddrinfo(res);

    if (!binded)
        throw "Could not bind socket";

    if (listen(this->sockfd, tcp_server_socket::backlog) < 0) {
        print_errno();
        throw "Socket listen failed";
    }

    pr_success("%s\n", "Server socket is listening for connections");
}

stream_socket* tcp_server_socket::accept_one_client()
{
    struct sockaddr peer_addr;
    socklen_t addrlen = sizeof peer_addr;

    int new_sockfd = accept(this->sockfd, &peer_addr, &addrlen);
    if (new_sockfd < 0) {
        print_errno();
        throw "Could not accept new connection";
    }

    int one = 1;
    if (setsockopt(new_sockfd, SOL_TCP, TCP_NODELAY, &one, sizeof(int)) < 0) {
        print_errno();
        pr_warn("%s\n", "Could not set TCP_NODELAY option on accepted socket");
    }

    char host[NI_MAXHOST], service[NI_MAXSERV];
    snprintf(host, sizeof host, "unresolved");
    snprintf(service, sizeof service, "unresolved");

    if (getnameinfo(&peer_addr, addrlen,
            host, NI_MAXHOST,
            service, NI_MAXSERV, 0) < 0) {
        print_errno();
        pr_warn("%s\n", "Could not resolve host and service of accepted connection");
    }

    pr_info("Accepted new connection from %s:%s\n", host, service);

    return new tcp_connection_socket(new_sockfd);
}
