#include <au_stream_socket.h>
#include <common.h>
#include <net_utils.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <netinet/tcp.h>

#define IPPROTO_AU 200

bool good_packet(ip_packet *packet, uint16_t port)
{
    size_t len = packet->header.tot_len - sizeof(packet->header);
    uint16_t csum = ntohs(packet->au_data.header.csum);
    packet->au_data.header.csum = 0;
    if (get_csum((void *)&packet->au_data, len) != csum)
        return false;
    if (packet->au_data.header.dest_port != htons(port))
        return false;
    return true;
}

au_stream_socket::au_stream_socket()
{
    this->sockfd = 0;

    this->send_buff = new send_buffer(4000);
    this->recv_buff = new recv_buffer(4000);
}

au_stream_socket::au_stream_socket(int sockfd, struct sockaddr remote_addr): au_stream_socket()
{
    this->sockfd = sockfd;
    this->remote_addr = remote_addr;
    this->addrlen = sizeof remote_addr;
}

void au_stream_socket::send(const void *buff, size_t size)
{
}

void au_stream_socket::recv(void *buff, size_t size)
{
}

void au_stream_socket::init_remote_addr(struct sockaddr remote_addr)
{
    this->remote_addr = remote_addr;
    this->addrlen = sizeof remote_addr;
}

void au_stream_socket::packet_send(au_packet *packet, size_t size)
{
    packet->header.csum = htons(get_csum(packet, size));
    for (int try_n = 0; try_n < 10; try_n++) {
        // since it's not packet sockets, sendto either sends full packet or fails (see man 7 raw)
        if (::sendto(this->sockfd, (void *)packet, size, 0, &this->remote_addr, this->addrlen) == (int)size)
            return;
    }

    throw "packet_send failed";
}

au_stream_socket::~au_stream_socket()
{
}

// ==========

au_stream_client_socket::au_stream_client_socket(const char *hostname, port_t server_port): hostname(hostname), server_port(server_port)
{
    this->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_AU);
    if (this->sockfd < 0) {
        print_errno();
        throw "Could not create raw socket";
    }
}

void au_stream_client_socket::connect()
{
    if (this->connected)
        return;

    this->client_port = 123; // TODO

    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_AU;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    struct addrinfo *res;

    // resolve given hostname to use it as remote_addr
    if (getaddrinfo(this->hostname, NULL, &hints, &res) < 0) {
        print_errno();
        throw "Could not resolve hostname";
    }

    // TODO for now we just take first option
    this->init_remote_addr(*res->ai_addr);

    freeaddrinfo(res);

    this->send_syn();
    this->wait_syn_ack();
    this->send_ack();

    pr_success("%s\n", "Connection established!");
    while (true);
    // Yey, we are connected!
    this->connected = true;
}

void au_stream_client_socket::send_syn()
{
    au_packet syn_packet = {0};
    syn_packet.header.source_port = htons(this->client_port);
    syn_packet.header.dest_port = htons(this->server_port);
    syn_packet.header.seq_num = htonl(this->send_buff->get_seq_num());
    syn_packet.header.flags = AU_PACKET_SYN;

    try {
        this->packet_send(&syn_packet, sizeof(struct au_packet_header));
    } catch (...) {
        handle_eptr(std::current_exception());
        throw "Could not send SYN packet";
    }

    pr_info("%s\n", "client sent SYN packet");
}

void au_stream_client_socket::wait_syn_ack()
{
    ip_packet recved_packet;

    while (true) {
        int res = ::recvfrom(this->sockfd, (void *)&recved_packet, sizeof(recved_packet), 0, &this->remote_addr, &addrlen);
        if (res == EAGAIN)
            continue;
        if (res < 0) {
            print_errno();
            throw "Could not recv to SYN-ACK packet";
        }
        if (good_packet(&recved_packet, this->client_port)
                && (recved_packet.au_data.header.flags & AU_PACKET_SYN)
                && (recved_packet.au_data.header.flags & AU_PACKET_ACK))
            break;
    }

    pr_info("%s\n", "client received SYN-ACK packet");

    if (ntohl(recved_packet.au_data.header.ack_num) != this->send_buff->get_seq_num() + 1)
        throw "SYN-ACK packet has bad ACK number";

    this->recv_buff->init_seq_num(ntohl(recved_packet.au_data.header.seq_num) + 1);
    this->send_buff->init_seq_num(ntohl(recved_packet.au_data.header.ack_num));
}

void au_stream_client_socket::send_ack()
{
    au_packet ack_packet = {0};
    ack_packet.header.source_port = htons(this->client_port);
    ack_packet.header.dest_port = htons(this->server_port);
    ack_packet.header.ack_num = htonl(this->recv_buff->get_seq_num());
    ack_packet.header.flags = AU_PACKET_ACK;

    try {
        this->packet_send(&ack_packet, sizeof(struct au_packet_header));
    } catch (...) {
        handle_eptr(std::current_exception());
        throw "Could not send ACK packet";
    }

    pr_info("%s\n", "client sent ACK packet");
}

// ===========

au_stream_server_socket::au_stream_server_socket(const char *hostname, port_t port): hostname(hostname), port(port)
{
    this->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_AU);
    if (this->sockfd < 0) {
        print_errno();
        throw "Could not create raw socket";
    }

    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_PASSIVE; // in case if node (i.e. hostname) is null
    hints.ai_protocol = IPPROTO_AU;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    struct addrinfo *res, *rp;

    pr_info("hostname: %s\n", hostname);
    // resolve given hostname to use it in bind
    if (getaddrinfo(this->hostname, NULL, &hints, &res) < 0) {
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
            this->init_self_addr(*rp->ai_addr);
            break;
        } else
            warn_op_fail("bind", rp);
    }

    freeaddrinfo(res);

    if (!binded)
        throw "Could not bind socket";

    // TODO listen?

    pr_success("%s\n", "Server socket is listening for connections");
}

stream_socket* au_stream_server_socket::accept_one_client()
{
    ip_packet recved_packet;
    socklen_t addrlen = sizeof(this->self_addr);

    // 1. receive syn packet

    while (true) {
        int res = ::recvfrom(this->sockfd, (void *)&recved_packet, sizeof(recved_packet), 0, &this->self_addr, &addrlen); // TODO NULL?
        if (res == EAGAIN)
            continue;
        if (res < 0) {
            print_errno();
            throw "Could not recv SYN";
        }
        if (good_packet(&recved_packet, this->port)
                && (recved_packet.au_data.header.flags & AU_PACKET_SYN))
            break;
    }

    au_stream_socket *new_connection = this->create_new_connection(&recved_packet);

    // 2. send syn-ack packet in response to syn

    au_packet syn_ack_packet = {0};
    syn_ack_packet.header.source_port = htons(this->port);
    syn_ack_packet.header.dest_port = recved_packet.au_data.header.source_port;
    syn_ack_packet.header.seq_num = htonl(new_connection->send_buff->get_seq_num());
    syn_ack_packet.header.ack_num = htonl(new_connection->recv_buff->get_seq_num());
    syn_ack_packet.header.flags = AU_PACKET_SYN | AU_PACKET_ACK;

    try {
        new_connection->packet_send(&syn_ack_packet, sizeof(au_packet_header));
    } catch (...) {
        handle_eptr(std::current_exception());
        delete new_connection;
        throw "Could not send SYN-ACk packet";
    }

    pr_info("%s\n", "SYN-ACK packet sent");

    // 3. wait for ack packet

    while (true) {
        int res = ::recvfrom(this->sockfd, (void *)&recved_packet, sizeof(recved_packet), 0, &this->self_addr, &addrlen); // TODO NULL?
        if (res == EAGAIN)
            continue;
        if (res < 0) {
            print_errno();
            throw "Could not recv ACK";
        }
        if (good_packet(&recved_packet, this->port)
                && (recved_packet.au_data.header.flags & AU_PACKET_ACK))
            break;
    }

    if (recved_packet.au_data.header.ack_num != htonl(new_connection->send_buff->get_seq_num() + 1))
        throw "wrong ACK num";

    new_connection->send_buff->init_seq_num(ntohl(recved_packet.au_data.header.ack_num));

    pr_success("%s\n", "New connection accepted");
    // handshake is done!

    return new_connection;
}

au_stream_socket* au_stream_server_socket::create_new_connection(ip_packet *recved_packet)
{
    // create socket for a new connection
    int new_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_AU);
    if (new_sockfd < 0) {
        print_errno();
        throw "Could not create raw socket";
    }

    struct sockaddr_in peer_addr = {0};
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = recved_packet->au_data.header.source_port;
    peer_addr.sin_addr.s_addr = recved_packet->header.saddr;

    pr_info("Server received SYN packet from %s:%d\n", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));

    au_stream_socket *new_connection = new au_stream_socket(new_sockfd, *((struct sockaddr *)&peer_addr));
    new_connection->recv_buff->init_seq_num(ntohl(recved_packet->au_data.header.seq_num) + 1);

    return new_connection;
}

void au_stream_server_socket::init_self_addr(struct sockaddr addr)
{
    this->self_addr = addr;
}

au_stream_server_socket::~au_stream_server_socket()
{
    if (this->sockfd > 0)
        close(this->sockfd);
}
