#include <au_stream_socket.h>
#include <common.h>
#include <net_utils.h>

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string.h>

#define IPPROTO_AU 200

static size_t get_data_len(ip_packet *packet)
{
    return ntohs(packet->header.tot_len) - sizeof(iphdr) - sizeof(au_packet_header);
}

au_stream_socket::au_stream_socket()
{
    this->sockfd = 0;

    this->send_buff = new send_buffer(4000);
    this->recv_buff = new recv_buffer(4000);

    state = AU_SOCKET_STATE_UNINIT;

    sender = new std::thread(&au_stream_socket::sender_fun, this);
    receiver = new std::thread(&au_stream_socket::receiver_fun, this);
}

au_stream_socket::au_stream_socket(int sockfd, struct sockaddr remote_addr): au_stream_socket()
{
    this->sockfd = sockfd;
    this->remote_addr = remote_addr;
    this->addrlen = sizeof remote_addr;
}

void au_stream_socket::send(const void *buff, size_t size)
{
    if (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        throw "Socket is not connected";
printf("enter send\n");
    size_t total_sent = 0;
    while (total_sent < size) {
        int sent = this->send_buff->write((char *)buff + total_sent, size - total_sent);
        total_sent += sent;
        if (sent == 0) {
            // no space in buffer. Sleep for 0.01 sec and give sender chance to send something
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
printf("leave send\n");
}

void au_stream_socket::recv(void *buff, size_t size)
{
    if (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        throw "Socket is not connected";
printf("enter recv\n");
    size_t total_received = 0;
    while (total_received < size) {
        int received = this->recv_buff->copy((char *)buff + total_received, size - total_received);
        total_received += received;
        if (received == 0) {
            // buffer is empty. Sleep a little bit so maybe something will be received
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 0.01 sec
        }
    }
printf("leave recv\n");
}

void au_stream_socket::packet_send(au_packet *packet, size_t size)
{
    std::lock_guard<std::mutex> lockg{this->lock};

    packet->header.csum = htons(get_csum(packet, size));
    for (int try_n = 0; try_n < 10; try_n++) {
        if (::sendto(this->sockfd, (void *)packet, size, 0, &this->remote_addr, this->addrlen) == (int)size)
            return;
    }

    throw "packet_send failed";
}

void au_stream_socket::packet_recv(ip_packet *packet)
{
    while (true) {
        // since it's not packet sockets, recvfrom either recves full ip packet or fails (see man 7 raw)
        int res = ::recvfrom(this->sockfd, (void *)packet, sizeof(*packet), 0, &this->remote_addr, &this->addrlen);
        if ((res == EAGAIN) || (res == EWOULDBLOCK) || (res == EINPROGRESS))
            continue;
        if (res < 0) {
            print_errno();
            throw "Could not recv packet";
        }
        if (this->good_packet(packet))
            break;
    }
}

bool au_stream_socket::good_packet(ip_packet *packet)
{
    size_t len = ntohs(packet->header.tot_len) - sizeof(packet->header);
    uint16_t csum = ntohs(packet->au_data.header.csum);
    packet->au_data.header.csum = 0;
    if (get_csum((void *)&packet->au_data, len) != csum) {
printf("csum failed\n");
        return false;
    }
    if (packet->au_data.header.dest_port != htons(this->port)) {
printf("dest port failed\n");
        return false;
    }
    if (packet->au_data.header.source_port != htons(this->remote_port)) {
printf("src port failed\n");
        return false;
    }
    if (packet->header.saddr != (*(struct sockaddr_in *)&this->remote_addr).sin_addr.s_addr) {
printf("ip failed\n");
        return false;
    }
    return true;
}

void au_stream_socket::sender_fun()
{
    while (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // sleep for 0.5 sec

    while (state.load() == AU_SOCKET_STATE_ESTABLISHED) {
        try {
            // check if we got all acks
            if (this->need_resend()) {
printf("need resend\n");
                this->send_buff->need_resend();
            }

            // ok, send up to 5 packets w/o waiting for ACKs
            for (int i = 0; i < 5; i++) {
                if (this->send_buff->has_to_send())
                    this->send_one_packet();
            }
        } catch (...) {
            handle_eptr(std::current_exception());
            return;
        }
    }
}

bool au_stream_socket::need_resend()
{
    std::lock_guard<std::mutex> lockg{this->lock};

    double since_last_ack = (std::clock() - this->last_ack) / (double)CLOCKS_PER_SEC;
    return ((since_last_ack > 5 && this->send_buff->has_unapproved()) || same_ack >= 3);
}

void au_stream_socket::send_one_packet()
{
    au_packet packet = {0};
    packet.header.source_port = htons(this->port);
    packet.header.dest_port = htons(this->remote_port);

    size_t copied = this->send_buff->copy(packet.data, sizeof(packet.data));
printf("copied to send %d\n", copied);
    if (copied == 0)
        return;

    packet.header.seq_num = htonl(this->send_buff->get_seq());

printf("one packet send\n");
    this->packet_send(&packet, sizeof(au_packet_header) + copied);
}

void au_stream_socket::receiver_fun()
{
    while (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // sleep for 0.5 sec

    ip_packet packet;

    while (state.load() == AU_SOCKET_STATE_ESTABLISHED) {
        try {
            this->packet_recv(&packet);

            // check if it's ACK packet and handle it
            if (packet.au_data.header.flags & AU_PACKET_ACK) {
printf("it's ACK\n");
                this->recved_ack(&packet.au_data);
                continue;
            }

            // FIN packet indicates that we are closing connection
            if (packet.au_data.header.flags & AU_PACKET_FIN) {
                // TODO
                return;
            }
printf("it's data\n");
            // ok, it's just a regular data
            this->recved_data(&packet);
        } catch (...) {
            handle_eptr(std::current_exception());
            return;
        }
    }
}

void au_stream_socket::recved_ack(au_packet *packet)
{
    std::lock_guard<std::mutex> lockg{this->lock};

printf("current ack: %d, received ack %d\n", this->send_buff->get_ack(), ntohl(packet->header.ack_num));

    this->last_ack = std::clock();

    if (this->send_buff->get_ack() < ntohl(packet->header.ack_num)) {
        this->send_buff->move_ack(ntohl(packet->header.ack_num));
        this->same_ack = 1;
printf("it is right ACK!\n");
    } else {
printf("it is bad ACK :(\n");
        // we received same ack
        this->same_ack += 1;
    }
}

void au_stream_socket::recved_data(ip_packet *packet)
{
printf("data size %d\n", get_data_len(packet));
    if (ntohl(packet->au_data.header.seq_num) < this->recv_buff->get_seq()) {
printf("this data was already received\n");
        // seems like sender tries to retransmit us some packet
        // noop
    } else {
printf("it's new data!\n");
        // ok, it's something new
        this->recv_buff->write(packet->au_data.data, get_data_len(packet));
    }
    this->send_ack();
}

void au_stream_socket::send_ack()
{
    uint32_t ack_num = this->recv_buff->get_seq();

printf("sending ack to %d\n", ack_num);

    au_packet packet = {0};
    packet.header.source_port = htons(this->port);
    packet.header.dest_port = htons(this->remote_port);
    packet.header.ack_num = htonl(ack_num);
    packet.header.flags = AU_PACKET_ACK;

    this->packet_send(&packet, sizeof(au_packet_header));
}

au_stream_socket::~au_stream_socket()
{
    delete this->send_buff;
    delete this->recv_buff;

    if (this->sockfd > 0)
        close(this->sockfd);
}

// ==========

au_stream_client_socket::au_stream_client_socket(const char *hostname, port_t server_port): hostname(hostname)
{
    this->remote_port = server_port;

    this->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_AU);
    if (this->sockfd < 0) {
        print_errno();
        throw "Could not create raw socket";
    }
}

static int glob_port = 100; // TODO

void au_stream_client_socket::connect()
{
    if (state.load() == AU_SOCKET_STATE_ESTABLISHED)
        return;

    this->port = (glob_port++); // TODO

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
    this->remote_addr = *res->ai_addr;
    this->addrlen = sizeof remote_addr;

    freeaddrinfo(res);

    this->send_syn();
    this->wait_syn_ack();
    this->send_ack();

    pr_success("%s\n", "Connection established!");
    
    state = AU_SOCKET_STATE_ESTABLISHED;
}

void au_stream_client_socket::send_syn()
{
    au_packet syn_packet = {0};
    syn_packet.header.source_port = htons(this->port);
    syn_packet.header.dest_port = htons(this->remote_port);
    syn_packet.header.seq_num = htonl(this->send_buff->get_ack());
    syn_packet.header.flags = AU_PACKET_SYN;

    try {
printf("sending syn\n");
        this->packet_send(&syn_packet, sizeof(struct au_packet_header));
printf("sent syn\n");
    } catch (...) {
        handle_eptr(std::current_exception());
printf("syn failed\n");
        throw "Could not send SYN packet";
    }

    pr_info("%s\n", "client sent SYN packet");
}

void au_stream_client_socket::wait_syn_ack()
{
    ip_packet recved_packet;

    while (true) {
        this->packet_recv(&recved_packet);
        if ((recved_packet.au_data.header.flags & AU_PACKET_SYN)
                && (recved_packet.au_data.header.flags & AU_PACKET_ACK))
            break;
    }

    pr_info("%s\n", "client received SYN-ACK packet");

    if (ntohl(recved_packet.au_data.header.ack_num) != this->send_buff->get_ack() + 1)
        throw "SYN-ACK packet has bad ACK number";

    this->recv_buff->init_seq(ntohl(recved_packet.au_data.header.seq_num) + 1);
    this->send_buff->move_ack(ntohl(recved_packet.au_data.header.ack_num));
}

void au_stream_client_socket::send_ack()
{
    au_packet ack_packet = {0};
    ack_packet.header.source_port = htons(this->port);
    ack_packet.header.dest_port = htons(this->remote_port);
    ack_packet.header.ack_num = htonl(this->recv_buff->get_seq());
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
            break;
        } else
            warn_op_fail("bind", rp);
    }

    freeaddrinfo(res);

    if (!binded)
        throw "Could not bind socket";

    pr_success("%s\n", "Server socket is working");
}

stream_socket* au_stream_server_socket::accept_one_client()
{
    ip_packet recved_packet;

    // 1. receive syn packet

    while (true) {
        int res = ::recvfrom(this->sockfd, (void *)&recved_packet, sizeof(recved_packet), 0, NULL, 0);
        if (res < 0) {
            print_errno();
            throw "Could not recv SYN";
        }
        if (this->good_packet(&recved_packet)
                && (recved_packet.au_data.header.flags & AU_PACKET_SYN))
            break;
    }

    au_stream_socket *new_connection = this->create_new_connection(&recved_packet);

    // 2. send syn-ack packet in response to syn

    au_packet syn_ack_packet = {0};
    syn_ack_packet.header.source_port = htons(this->port);
    syn_ack_packet.header.dest_port = recved_packet.au_data.header.source_port;
    syn_ack_packet.header.seq_num = htonl(new_connection->send_buff->get_ack());
    syn_ack_packet.header.ack_num = htonl(new_connection->recv_buff->get_seq());
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
        new_connection->packet_recv(&recved_packet);
        if (recved_packet.au_data.header.flags & AU_PACKET_ACK)
            break;
    }

    if (recved_packet.au_data.header.ack_num != htonl(new_connection->send_buff->get_ack() + 1))
        throw "wrong ACK num";

    new_connection->send_buff->init_ack(ntohl(recved_packet.au_data.header.ack_num));
    new_connection->state = AU_SOCKET_STATE_ESTABLISHED;

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
    new_connection->port = this->port;
    new_connection->remote_port = ntohs(peer_addr.sin_port);
    new_connection->recv_buff->init_seq(ntohl(recved_packet->au_data.header.seq_num) + 1);

    return new_connection;
}

bool au_stream_server_socket::good_packet(ip_packet *packet)
{
    size_t len = ntohs(packet->header.tot_len) - sizeof(packet->header);
    uint16_t csum = ntohs(packet->au_data.header.csum);
    packet->au_data.header.csum = 0;
    if (get_csum((void *)&packet->au_data, len) != csum) {
printf("server failed csum\n");
        return false;
    }
    if (packet->au_data.header.dest_port != htons(this->port)) {
printf("server failed port (server's port: %d, dest port %d)\n", this->port, ntohs(packet->au_data.header.dest_port));
        return false;
    }
    return true;
}

au_stream_server_socket::~au_stream_server_socket()
{
    if (this->sockfd > 0)
        close(this->sockfd);
}
