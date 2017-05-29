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
#include <cassert>
#include <ctime>
#include <errno.h>
#include <chrono>

#define IPPROTO_AU 200

#define FORM_PACKET(packet, src_port, dst_port, flgs) \
    do { \
        packet = {0}; \
        packet.header.source_port = htons(src_port); \
        packet.header.dest_port = htons(dst_port); \
        packet.header.flags = (flgs); \
    } while (0);

static double inline get_time_sec_since(std::chrono::time_point<std::chrono::system_clock> start)
{
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = now - start;
    return elapsed_seconds.count();
}

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
    this->set_nonblock();
}

void au_stream_socket::set_nonblock()
{
	/*
	 * We need to set recv timeout because without it we wont 
	 * leave recvfrom function even after FIN packets, so connection
     * will never really close.
 	 *
	 * Also we set send timeout. We need this because if other end
     * is unreachable we want to try sending for some timeout.
     */

    struct timeval recv_timeout = {0};
    recv_timeout.tv_usec = (int)(au_stream_socket::SINGLE_RECV_TIMEOUT_SEC * 1e6);

    if (setsockopt(this->sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout))) {
		print_errno();
		pr_warn("%s\n", "Could not set SO_RCVTIMEO socket option");
    }

    struct timeval send_timeout = {0};
    send_timeout.tv_usec = (int)(au_stream_socket::SINGLE_RECV_TIMEOUT_SEC * 1e6);

    if (setsockopt(this->sockfd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout))) {
		print_errno();
		pr_warn("%s\n", "Could not set SO_SNDTIMEO socket option");
    }
}

void au_stream_socket::send(const void *buff, size_t size)
{
    if (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        throw "Socket is not connected";

    size_t total_sent = 0;
    while (total_sent < size) {
        int sent = this->send_buff->write((char *)buff + total_sent, size - total_sent);
        total_sent += sent;
        if (sent == 0) {
            // no space in buffer. Sleep for 0.01 sec and give sender chance to send something
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    size_t end = this->send_buff->get_end();

    // wait for ack to come
    while (end > this->send_buff->get_ack())
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void au_stream_socket::recv(void *buff, size_t size)
{
    if (state.load() == AU_SOCKET_STATE_UNINIT)
        throw "Socket is not connected";

    size_t total_received = 0;
    while (total_received < size) {
        int received = this->recv_buff->copy((char *)buff + total_received, size - total_received);
        total_received += received;
        if (received == 0) {
            if (state.load() == AU_SOCKET_STATE_CLOSED)
                throw "Connection is closed";
            // buffer is empty. Sleep a little bit so maybe something will be received
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 0.01 sec
        }
    }
}

bool au_stream_socket::packet_send(au_packet *packet, size_t size)
{
    std::lock_guard<std::mutex> lockg{this->lock};

    packet->header.csum = htons(get_csum(packet, size));

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    while (get_time_sec_since(start) < au_stream_socket::SEND_TIMEOUT_SEC) {
        int res = ::sendto(this->sockfd, (void *)packet, size, MSG_NOSIGNAL, &this->remote_addr, this->addrlen);
        if (res == (int)size)
            return true;
        if (res == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS))
            continue;
        if (res < 0 ) {
            print_errno();
            throw "Could not send packet";
        }
    }

    return false;
}

void au_stream_socket::send_special_packet(uint8_t flags)
{
    au_packet packet;
    FORM_PACKET(packet, this->port, this->remote_port, flags);
    if (!this->packet_send(&packet, sizeof(au_packet_header)))
       pr_warn("%d: Failed to send special packet with flag %d\n", this->port, flags);
}

bool au_stream_socket::packet_recv(ip_packet *packet)
{
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    while (get_time_sec_since(start) < au_stream_socket::RECV_TIMEOUT_SEC) {
        // since it's not packet sockets, recvfrom either recves full ip packet or fails (see man 7 raw)
        int res = ::recvfrom(this->sockfd, (void *)packet, sizeof(*packet), 0, &this->remote_addr, &this->addrlen);
        if (res == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS))
            continue;
        if (res < 0 ) {
            print_errno();
            throw "Could not recv packet";
        }
        if (this->good_packet(packet))
            return true;
    }

    return false;
}

bool au_stream_socket::good_packet(ip_packet *packet)
{
    size_t len = ntohs(packet->header.tot_len) - sizeof(packet->header);
    uint16_t csum = ntohs(packet->au_data.header.csum);
    packet->au_data.header.csum = 0;
    if (get_csum((void *)&packet->au_data, len) != csum)
        return false;
    if (packet->au_data.header.dest_port != htons(this->port))
        return false;
    if (packet->au_data.header.source_port != htons(this->remote_port))
        return false;
    if (packet->header.saddr != (*(struct sockaddr_in *)&this->remote_addr).sin_addr.s_addr)
        return false;
    return true;
}

void au_stream_socket::sender_fun()
{
    while (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // sleep for 0.5 sec

    // setup last_ack to current time since we have't sent anything yet
    this->lock.lock();
    this->last_ack = std::chrono::system_clock::now();
    this->same_ack = 1;
    this->lock.unlock();

    while (state.load() == AU_SOCKET_STATE_ESTABLISHED) {
        try {
            // check if we got all acks
            if (this->need_resend()) {
                this->send_buff->need_resend();
                this->lock.lock();
                this->last_ack = std::chrono::system_clock::now();
                this->same_ack = 0;
                this->lock.unlock();
            }

            if (this->send_buff->has_to_send()) // TODO check number of packets sent
                this->send_from_buff();
        } catch (...) {
            handle_eptr(std::current_exception());
            return;
        }
    }

// We are initiator of closing
    if (state.load() == AU_SOCKET_STATE_FIN) {
        // client is closing connection -- send FIN to server
        state = AU_SOCKET_STATE_FIN_WAIT_1;
        this->send_special_packet(AU_PACKET_FIN);

        // wait for FIN from server
        while (state.load() != AU_SOCKET_STATE_TIME_WAIT)
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // sleep for 0.5 sec

        // we received FIN from server. Send him our ACK
        this->send_special_packet(AU_PACKET_FIN_ACK);

        // wait for some time
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // now we can really close the connection
        state = AU_SOCKET_STATE_CLOSED;
        return;
    }

// We are not

    if (state.load() == AU_SOCKET_STATE_CLOSE_WAIT) {
        // we received FIN packet -- send FIN-ACK in return
        this->send_special_packet(AU_PACKET_FIN_ACK);

        // now send our FIN packet
        state = AU_SOCKET_STATE_LAST_ACK;
        this->send_special_packet(AU_PACKET_FIN);

        while (state.load() != AU_SOCKET_STATE_CLOSED)
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // sleep for 0.5 sec

        return;
    }

// Wtf?
    assert(false);
}

bool au_stream_socket::need_resend()
{
    std::lock_guard<std::mutex> lockg{this->lock};

    double since_last_ack = get_time_sec_since(this->last_ack);
    return ((since_last_ack > 5 && this->send_buff->has_unapproved()) || same_ack >= 3);
}

// sends one packet from send_buff
void au_stream_socket::send_from_buff()
{
    au_packet packet = {0};
    packet.header.source_port = htons(this->port);
    packet.header.dest_port = htons(this->remote_port);

    // reset timeout all sent data is already approved
    if (!this->send_buff->has_unapproved()) {
        this->lock.lock();
        this->last_ack = std::chrono::system_clock::now();
        this->lock.unlock();
    }

    size_t copied = this->send_buff->copy(packet.data, sizeof(packet.data));
    if (copied == 0)
        return;

    packet.header.seq_num = htonl(this->send_buff->get_seq());
    if (!this->packet_send(&packet, sizeof(au_packet_header) + copied))
        pr_warn("%d: Could not send data from send_buff\n", this->port);
}

void au_stream_socket::receiver_fun()
{
    while (state.load() != AU_SOCKET_STATE_ESTABLISHED)
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // sleep for 0.5 sec

    ip_packet packet;
    this->last_activity = std::chrono::system_clock::now();

    while (state.load() != AU_SOCKET_STATE_CLOSED) {
        try {
            if (!this->packet_recv(&packet)) {
                this->check_alive();
                continue;
            }

            this->last_activity = std::chrono::system_clock::now();

            if (packet.au_data.header.flags & AU_PACKET_DUDE_ARE_U_EVEN_ALIVE) {
                this->send_special_packet(AU_PACKET_YEAH_DUDE_IM_FINE);
                continue;
            }

            if (packet.au_data.header.flags & AU_PACKET_YEAH_DUDE_IM_FINE)
                continue;

            // check if it's ACK packet and handle it
            if (packet.au_data.header.flags & AU_PACKET_ACK) {
                this->recved_ack(&packet.au_data);
                continue;
            }

            if (packet.au_data.header.flags & AU_PACKET_FIN) {
                if (state == AU_SOCKET_STATE_ESTABLISHED) {
                    state = AU_SOCKET_STATE_CLOSE_WAIT;
                    continue;
                }
                if (state == AU_SOCKET_STATE_FIN_WAIT_2) {
                    state = AU_SOCKET_STATE_TIME_WAIT;
                    continue;
                }
                throw "Bad FIN";
            }

            // we received acknowledgement for our fin
            if (packet.au_data.header.flags & AU_PACKET_FIN_ACK) {
                if (state.load() == AU_SOCKET_STATE_LAST_ACK)
                    state = AU_SOCKET_STATE_CLOSED;
                else if (state.load() == AU_SOCKET_STATE_FIN_WAIT_1)
                    state = AU_SOCKET_STATE_FIN_WAIT_2;
                else
                    throw "ACK-FIN received suddenly";
                continue;
            }
            
            // ok, it's just a regular data
            this->recved_data(&packet);
        } catch (...) {
            handle_eptr(std::current_exception());
            return;
        }
    }
}

void au_stream_socket::check_alive()
{
    if (get_time_sec_since(this->last_activity) > ABORT_CONNECTION_TIMEOUT) {
        pr_err("%s\n", "Other side does not respond. Aboring connection.");
        state = AU_SOCKET_STATE_CLOSED;
        throw "Connection aborted due to timeout";
    }

    if (get_time_sec_since(this->last_activity) > KEEP_ALIVE_TIMEOUT_SEC) {
        this->send_special_packet(AU_PACKET_DUDE_ARE_U_EVEN_ALIVE);
    }
}

void au_stream_socket::recved_ack(au_packet *packet)
{
    std::lock_guard<std::mutex> lockg{this->lock};

    this->last_ack = std::chrono::system_clock::now();

    if (this->send_buff->get_ack() < ntohl(packet->header.ack_num)) {
        this->send_buff->move_ack(ntohl(packet->header.ack_num));
        this->same_ack = 1;
    } else {
        // we received same ack
        this->same_ack += 1;
    }
}

void au_stream_socket::recved_data(ip_packet *packet)
{
    if (ntohl(packet->au_data.header.seq_num) <= this->recv_buff->get_seq()) {
        // seems like sender tries to retransmit us some packet
        // noop
    } else {
        /*
         * This packet may contain some old data along with new one.
         * Imagine the situation when sender send some data but did not recv
         * ACK packet for it during timeout, though receiver sends it. 
         * Now, right after send does send_buff->need_resend()
         * some new data arrives in this buffer. So send_buff->end increased which means
         * we have some new data for receiver. However, when we start to send all this data
         * our packet will contain old data as well as new one. If we just write it to
         * recv_buff of receiver, old data would be written twice.
         *
         * To prevent it, we calculate how much data we should actually write to the buffer.
         */
        size_t size = ntohl(packet->au_data.header.seq_num) - this->recv_buff->get_seq();
        int offset = (int)get_data_len(packet) - (int)size;
        this->recv_buff->write(packet->au_data.data + offset, size /*get_data_len(packet)*/);
    }
    this->send_ack();
}

void au_stream_socket::send_ack()
{
    uint32_t ack_num = this->recv_buff->get_seq();

    au_packet packet;
    FORM_PACKET(packet, this->port, this->remote_port, AU_PACKET_ACK);
    packet.header.ack_num = htonl(ack_num);

    if (!this->packet_send(&packet, sizeof(au_packet_header)))
        pr_warn("%d: Failed to send ACK", this->port);
}

au_stream_socket::~au_stream_socket()
{
    if (this->state.load() == AU_SOCKET_STATE_ESTABLISHED)
        this->state = AU_SOCKET_STATE_FIN;
    this->sender->join();
    this->receiver->join();

    delete this->send_buff;
    delete this->recv_buff;

    if (this->sockfd > 0)
        close(this->sockfd);
}

// ==========

// accordint to given test, au_stream_client socket is provided with client port by caller
au_stream_client_socket::au_stream_client_socket(const char *hostname, port_t client_port, port_t server_port): hostname(hostname)
{
    this->port = client_port;
    this->remote_port = server_port;

    this->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_AU);
    if (this->sockfd < 0) {
        print_errno();
        throw "Could not create raw socket";
    }

    this->set_nonblock();
}

void au_stream_client_socket::connect()
{
    if (state.load() == AU_SOCKET_STATE_ESTABLISHED)
        return;

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

    // try to send syn for some time, server maybe just starting
    this->send_syn();
    this->wait_syn_ack();
    this->send_ack();

    pr_success("%s\n", "Connection established!");
    
    state = AU_SOCKET_STATE_ESTABLISHED;
}

void au_stream_client_socket::send_syn()
{
    au_packet syn_packet;
    FORM_PACKET(syn_packet, this->port, this->remote_port, AU_PACKET_SYN)
    syn_packet.header.seq_num = htonl(this->send_buff->get_ack());

    try {
        if (!this->packet_send(&syn_packet, sizeof(struct au_packet_header)))
            throw "..."; // Ye, Im lazy
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
        this->packet_recv(&recved_packet);
        if ((recved_packet.au_data.header.flags & AU_PACKET_SYN)
                && (recved_packet.au_data.header.flags & AU_PACKET_ACK))
            break;
    }

    pr_info("%s\n", "client received SYN-ACK packet");

    if (ntohl(recved_packet.au_data.header.ack_num) != this->send_buff->get_ack() + 1)
        throw "SYN-ACK packet has bad ACK number";

    this->recv_buff->init_seq(ntohl(recved_packet.au_data.header.seq_num) + 1);
    this->send_buff->init_ack(ntohl(recved_packet.au_data.header.ack_num));
}

void au_stream_client_socket::send_ack()
{
    au_packet ack_packet;
    FORM_PACKET(ack_packet, this->port, this->remote_port, AU_PACKET_ACK);
    ack_packet.header.ack_num = htonl(this->recv_buff->get_seq());

    try {
        if (!this->packet_send(&ack_packet, sizeof(struct au_packet_header)))
            throw "..."; // Like very lazy
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

    au_packet syn_ack_packet;
    FORM_PACKET(syn_ack_packet, this->port, ntohs(recved_packet.au_data.header.source_port), AU_PACKET_SYN | AU_PACKET_ACK)
    syn_ack_packet.header.seq_num = htonl(new_connection->send_buff->get_ack());
    syn_ack_packet.header.ack_num = htonl(new_connection->recv_buff->get_seq());

    try {
        if (!new_connection->packet_send(&syn_ack_packet, sizeof(au_packet_header)))
            throw "...";
    } catch (...) {
        handle_eptr(std::current_exception());
        delete new_connection;
        throw "Could not send SYN-ACK packet";
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
        return false;
    }
    if (packet->au_data.header.dest_port != htons(this->port)) {
        return false;
    }
    return true;
}

au_stream_server_socket::~au_stream_server_socket()
{
    if (this->sockfd > 0)
        close(this->sockfd);
}
