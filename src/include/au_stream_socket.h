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
#include <condition_variable>
#include <chrono>

#define AU_PACKET_SYN         (1 << 0)
#define AU_PACKET_ACK         (1 << 1)
#define AU_PACKET_FIN         (1 << 2)
// this flag tells that this packet is acking FIN packet.
// it's just easier to impliment connection closing with it.
#define AU_PACKET_FIN_ACK     (1 << 3)
#define AU_PACKET_DUDE_ARE_U_EVEN_ALIVE (1 << 4)
#define AU_PACKET_YEAH_DUDE_IM_FINE (1 << 5)

#define AU_SOCKET_STATE_UNINIT       0
#define AU_SOCKET_STATE_ESTABLISHED  1
// see diagram here: http://www.tcpipguide.com/free/t_TCPConnectionTermination-2.htm
#define AU_SOCKET_STATE_FIN          2
#define AU_SOCKET_STATE_FIN_WAIT_1   3
#define AU_SOCKET_STATE_FIN_WAIT_2   4
#define AU_SOCKET_STATE_TIME_WAIT    5
#define AU_SOCKET_STATE_CLOSE_WAIT   6
#define AU_SOCKET_STATE_LAST_ACK     7
#define AU_SOCKET_STATE_CLOSED       8

/*
 * enhancements:
 * 1. Cumulative ACK
 * 2. Fast retransmit (if I understood it correctly, then it's just retransmitting when we have 3 equal ACKs)
 */

typedef port_t au_stream_port; // this is only for compatibility with provided test

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

    bool packet_send(au_packet *packet, size_t size); // directly sends packet
    bool packet_recv(ip_packet *packet); // directly recvs packet

    void send_special_packet(uint8_t flags);

    void init_remote_addr(struct sockaddr remote_addr);
protected:
    bool good_packet(ip_packet *packet);

    void set_nonblock();

    void sender_fun();
    void receiver_fun();
private:
    bool need_resend();
    void send_from_buff();
    void recved_ack(au_packet *packet);
    void recved_data(ip_packet *packet);
    void send_ack();
    void check_alive();
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
    std::condition_variable send_sync;
    std::condition_variable recv_sync;


    std::atomic<int> state;


    send_buffer *send_buff;
    recv_buffer *recv_buff;


    std::chrono::time_point<std::chrono::system_clock> last_activity;
    std::chrono::time_point<std::chrono::system_clock> last_ack;
    int same_ack;


    volatile int sockfd;
    struct sockaddr remote_addr;
    socklen_t addrlen;
    port_t remote_port;
    port_t port;


    /*
     * This timeouts I just chose on random
     */
    static const size_t BUFFER_SIZE = 4000;
    static constexpr double RECV_TIMEOUT_SEC = 5;
    static constexpr double SINGLE_RECV_TIMEOUT_SEC = 0.5;
    static constexpr double SEND_TIMEOUT_SEC = 5;
    static constexpr double SINGLE_SEND_TIMEOUT_SEC = 0.5;
    static constexpr double FAST_RETRANSMIT_TIMEOUT_SEC = 1; // if it's high test works too long. 
    static constexpr double KEEP_ALIVE_TIMEOUT_SEC = 30;
    static constexpr double ABORT_CONNECTION_TIMEOUT = 3 * KEEP_ALIVE_TIMEOUT_SEC;
};

struct au_stream_client_socket: public au_stream_socket, public stream_client_socket
{
public:
    au_stream_client_socket(const char *hostname, port_t client_port, port_t server_port);
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
