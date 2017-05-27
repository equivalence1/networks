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

struct send_buffer
{
public:
    send_buffer(size_t size);
    void init_seq_num(size_t new_seq);
    size_t get_seq_num();
    ~send_buffer();
private:
    size_t size;
    size_t l, r;
    void *buff;
};

struct recv_buffer
{
public:
    recv_buffer(size_t size);
    size_t write(void *src, size_t size);
    size_t copy(void *dst, size_t size);
    void init_seq_num(size_t new_seq);
    size_t get_seq_num();
    size_t filled_size();
    bool is_full();
    ~recv_buffer();
private:
    size_t size;
    size_t l, r;
    void *buff;
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
