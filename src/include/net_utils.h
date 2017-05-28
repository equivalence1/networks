#ifndef __NET_UTILS_H__
#define __NET_UTILS_H__

#include <common.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <mutex>

/*
 * I have two different buffers for send/recv because
 * it was to much pain in the ass when I tried to use one
 * even though they have a lot of common functionality
 */

/*
 * ack -- last acknowledged position (via send_buffer::move_ack)
 * seq -- last sent position (via send_buffer::copy)
 * end -- last written to buffer position (via send_buffer::write)
 *
 * thus seq is always in [ack, end]
 */
struct send_buffer
{
public:
    send_buffer(size_t size);
    size_t write(const void *src, size_t size);
    size_t copy(void *dst, size_t size);
    size_t get_ack();
    void  move_ack(size_t new_ack);
    void init_ack(size_t ack);
    size_t get_seq();
    void need_resend();
    bool has_to_send();
    bool has_unapproved();
    ~send_buffer();
private:
    size_t size;
    size_t ack, seq, end;
    void *buff;
    std::mutex lock;
};

struct recv_buffer
{
public:
    recv_buffer(size_t size);
    size_t write(const void *src, size_t size);
    size_t copy(void *dst, size_t size);
    size_t get_seq();
    void init_seq(size_t seq);
    ~recv_buffer();
private:
    size_t size;
    size_t begin, seq;
    void *buff;
    std::mutex lock;
};

int get_addr_host_port(const struct sockaddr_in *addr, char *ipstr, int *port);
void warn_op_fail(const char *op, struct addrinfo *rp);
uint16_t get_csum(void *packet, size_t len);

int inline bad_sockfd(int sockfd)
{
    return (sockfd == ENETDOWN) || (sockfd == EPROTO) || (sockfd == ENOPROTOOPT)
            || (sockfd == EHOSTDOWN) || (sockfd == ENONET) || (sockfd == EHOSTUNREACH)
            || (sockfd == EOPNOTSUPP) || (sockfd == ENETUNREACH) || (sockfd == EAGAIN);
}


#endif // __NET_UTILS_H__
