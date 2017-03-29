#include <tcp_socket.h>
#include <common.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>

tcp_connection_socket::tcp_connection_socket(int sockfd)
{
    this->sockfd = sockfd;
}

tcp_connection_socket::tcp_connection_socket()
{
    this->sockfd = 0;
}

void tcp_connection_socket::send(const void *buff, size_t size)
{
    printf("%s\n", __func__);
    if (sockfd <= 0)
        throw "Socket is not initialized";
}

void tcp_connection_socket::recv(void *buff, size_t size)
{
    printf("%s\n", __func__);
    if (sockfd <= 0)
        throw "Socket is not initialized";
}

tcp_client_socket::tcp_client_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        print_errno();
        throw "Could not create tcp socket";
    }
}

void tcp_client_socket::connect()
{
}

tcp_server_socket::tcp_server_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        print_errno();
        throw "Could not create tcp socket";
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
    snprintf(port_s, sizeof port_s, "%d", port);

    struct addrinfo *res, *rp;

    // resolve given hostname to use it in bind
    if (getaddrinfo(hostname, port_s, &hints, &res) < 0) {
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
        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            binded = true;
            break;
        } else
            pr_warn("%s", "Could not bind server socket on ...");
    }

    freeaddrinfo(res);

    if (!binded)
        throw "Could not bind socket";

    if (listen(sockfd, backlog) < 0) {
        print_errno();
        throw "Socket listen failed";
    }

    pr_success("%s\n", "Server socket is listening for connections");
}

stream_socket* tcp_server_socket::accept_one_client()
{
    struct sockaddr peer_addr;
    socklen_t addrlen = sizeof peer_addr;

    int new_sockfd = accept(sockfd, &peer_addr, &addrlen);
    if (new_sockfd < 0) {
        print_errno();
        throw "Could not accept new connection";
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
