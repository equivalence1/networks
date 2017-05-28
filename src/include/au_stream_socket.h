#ifndef __AU_STREAM_SOCKET_H__
#define __AU_STREAM_SOCKET_H__

#include "stream_socket.h"
#include "net_utils.h"

#include <stdint.h>
#include <netinet/ip.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <ctime>

#define AU_PACKET_SYN (1 << 0)
#define AU_PACKET_ACK (1 << 1)
#define AU_PACKET_FIN (1 << 2)

#define AU_SOCKET_STATE_UNINIT       0
#define AU_SOCKET_STATE_ESTABLISHED  1
#define AU_SOCKET_STATE_CLOSED       2

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

    void sender_fun();
    void receiver_fun();
private:
    bool need_resend();
    void send_one_packet();
    void recved_ack(au_packet *packet);
    void recved_data(ip_packet *packet);
    void send_ack();

public:
/*
 * Извиняюсь за коммент на русском, но я бы убился это писать по-английски,
 * а с моим корявым знанием его, вы бы ещё и не поняли ничего.
 *
 * В общем, я долго думал, как избежать использования доп. потоков для каждого
 * сокета, но так ничего не придумал. Проблема в том, что когда мы делаем
 * send, кто-то нам должен послать ACK, даже если чувак на другой стороне не
 * делает recv. Отсюда берется receiver. Аналогично, пакеты могут теряться и т.д.
 * и если мы не хотим за 2 RTT отсылать 1 пакет (т.е. всегда в send'e на каждый 
 * пакет дожидаться ACK'a), то нам нужен кто-то, кто на заднем плане будет перепосылать
 * пакеты, которые не получили ACK'a. Отсюда берется sender. mutex берется для их
 * синхронизации друг с другом и main потоком. state тоже понадобился для их конроля.
 */
    std::thread *receiver;
    std::thread *sender;
    std::mutex lock;
    
    
    std::atomic<int> state;


    send_buffer *send_buff;
    recv_buffer *recv_buff;


    std::clock_t last_ack;
    int same_ack;


    int sockfd;
    struct sockaddr remote_addr;
    socklen_t addrlen;
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
