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


/*
 * get host string and port number of address.
 * @param ipstr should be a char array with a length >= NI_MAXHOST
 */
static
int get_addr_host_port(const struct sockaddr_in *addr, char *ipstr, int *port)
{
    *port = ntohs(addr->sin_port);
    if (inet_ntop(AF_INET, &addr->sin_addr, ipstr, NI_MAXHOST) == NULL)
        return -1;
    else
        return 0;
}

/*
 * used to notify user if connect/bind failed on returned after
 * getaddrinfo address
 */
static
void warn_op_fail(const char *op, struct addrinfo *rp)
{
    char ipstr[NI_MAXHOST];
    int port;

    print_errno();
    if (get_addr_host_port((struct sockaddr_in *)rp->ai_addr, ipstr, &port) >= 0)
        pr_warn("Could not %s on %s:%d\n", op, ipstr, port);
}

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

    size_t total_sent = 0;
    int sent_right_now = 0;

    while (total_sent != size) {
        if ((sent_right_now = ::send(this->sockfd, (char *)buff + total_sent, size, 0)) < 0) {
            print_errno();
            throw "Could not send";
        }
        total_sent += sent_right_now;
    }
}

// i think this function should return int.
// How else should I notify that connection was closed?
void tcp_connection_socket::recv(void *buff, size_t size)
{
    std::lock_guard<std::mutex> lock{this->m};

    if (this->sockfd <= 0)
        throw "Socket is not initialized";

    int received = 0;

    if ((received = ::recv(this->sockfd, buff, size, 0)) < 0) {
        print_errno();
        throw "Could not recv";
    }

    // connection was closed, inform about it
    if (received == 0) {
        socklen_t len;
        struct sockaddr_storage addr;
        len = sizeof addr;

        if (getpeername(this->sockfd, (struct sockaddr*)&addr, &len) >= 0) {
            int port;
            char ipstr[NI_MAXHOST];

            if (get_addr_host_port((struct sockaddr_in *)&addr, ipstr, &port) >= 0)
                pr_info("Connection with %s:%d is closed\n", ipstr, port);
        }

        close(this->sockfd);
        throw "Connection is closed";
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
        } else
            warn_op_fail("connect", rp);
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
    hints.ai_flags = 0; // we're specifing hostname (node) in getaddrinfo, so we don't need AF_PASSIVE
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
        } else
            warn_op_fail("bind", rp);
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

tcp_connection_socket::~tcp_connection_socket()
{
    if (this->sockfd > 0)
        close(this->sockfd);
}

tcp_server_socket::~tcp_server_socket()
{
    if (this->sockfd > 0)
        close(this->sockfd);
}
