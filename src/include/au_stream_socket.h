#ifndef __AU_STREAM_SOCKET_H__
#define __AU_STREAM_SOCKET_H__

#include "stream_socket.h"
#include "net_utils.h"

#include <stdint.h>
#include <netinet/ip.h>

#define AU_PACKET_SYN (1 << 0)
#define AU_PACKET_ACK (1 << 1)
#define AU_PACKET_FIN (1 << 2)

struct au_packet_header {
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t csum;
    uint8_t flags;
} __attribute__((packed));

struct au_packet
{
    au_packet_header header;
    char data[IP_MAXPACKET - sizeof(au_packet_header) - sizeof(iphdr)];
} __attribute__((packed));

// When recving, ip header is always included
struct ip_packet
{
    iphdr header;
    struct au_packet au_data;
} __attribute__((packed));

struct au_stream_socket: virtual stream_socket
{
public:
    au_stream_socket();
    au_stream_socket(int sockfd, struct sockaddr remote_addr);
    void send(const void *buff, size_t size);
    void recv(void *buff, size_t size);
    virtual ~au_stream_socket();

    void packet_send(au_packet *packet, size_t size); // directly sends packet
    void packet_recv(ip_packet *packet); // directly recvs packet

    void init_remote_addr(struct sockaddr remote_addr);
protected:
    bool good_packet(ip_packet *packet);
public:
    send_buffer *send_buff;
    recv_buffer *recv_buff;

    struct sockaddr remote_addr;
    socklen_t addrlen;
    int sockfd;

    port_t remote_port;
    port_t port;
};

struct au_stream_client_socket: public au_stream_socket, public stream_client_socket
{
public:
    au_stream_client_socket(const char *hostname, port_t port);
    virtual void connect();
private:
    void send_syn();
    void wait_syn_ack();
    void send_ack();

    const char *hostname;
    bool connected;
};

struct au_stream_server_socket: public stream_server_socket
{
public:
    au_stream_server_socket(const char *hostname, port_t port);
    stream_socket* accept_one_client();
    ~au_stream_server_socket();
private:
    bool good_packet(ip_packet *packet);
private:
    au_stream_socket* create_new_connection(ip_packet *recved_packet);
    int sockfd;
    const char *hostname;
    const port_t port;
    const static int backlog = 5;
};

#endif // __TCP_SOCKET_H__
