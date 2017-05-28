#include <net_utils.h>
#include <common.h>

#include <cmath>

// send buffer

send_buffer::send_buffer(size_t size)
{
    buff = malloc(size);
    if (buff == NULL)
        throw "No mem";
    ack = seq = end = 0;
    this->size = size;
}

size_t send_buffer::write(const void *src, size_t size)
{
    std::lock_guard<std::mutex> lock{this->lock};

    int to_write = std::min(size, this->size - (end - ack));
    for (int i = 0; i < to_write; i++) {
        ((char *)buff)[(end + i) % this->size] = ((char *)src)[i];
    }
    end = end + to_write;

    return to_write;
}

size_t send_buffer::copy(void *dst, size_t size)
{
    std::lock_guard<std::mutex> lock{this->lock};

    int to_copy = std::min(size, (end - seq) % this->size);
    for (int i = 0; i < to_copy; i++) {
        ((char *)dst)[i] = ((char *)buff)[(seq + i) % this->size];
    }
    seq = seq + to_copy;

    return to_copy;
}

size_t send_buffer::get_ack()
{
    std::lock_guard<std::mutex> lock{this->lock};
    return this->ack;
}

size_t send_buffer::get_seq()
{
    std::lock_guard<std::mutex> lock{this->lock};
    return this->seq;
}

void send_buffer::move_ack(size_t new_ack)
{
    std::lock_guard<std::mutex> lock{this->lock};
    this->ack = new_ack;
}

void send_buffer::init_ack(size_t ack)
{
    std::lock_guard<std::mutex> lock{this->lock};
    this->ack = this->seq = this->end = ack;
}

void send_buffer::need_resend()
{
    std::lock_guard<std::mutex> lock{this->lock};
    this->seq = this->ack;
}

bool send_buffer::has_to_send()
{
    std::lock_guard<std::mutex> lock{this->lock};
    return this->end > this->seq;
}

bool send_buffer::has_unapproved()
{
    std::lock_guard<std::mutex> lock{this->lock};
    return this->seq > this->ack;
}

send_buffer::~send_buffer()
{
    free(this->buff);
}

// recv_buffer

recv_buffer::recv_buffer(size_t size)
{
    buff = malloc(size);
    if (buff == NULL)
        throw "No mem";
    begin = seq = 0;
    this->size = size;
}

size_t recv_buffer::write(const void *src, size_t size)
{
    std::lock_guard<std::mutex> lock{this->lock};

    int to_write = std::min(size, this->size - (seq - begin));
    for (int i = 0; i < to_write; i++) {
        ((char *)buff)[(seq + i) % this->size] = ((char *)src)[i];
    }
    seq = seq + to_write;

    return to_write;
}

size_t recv_buffer::copy(void *dst, size_t size)
{
    std::lock_guard<std::mutex> lock{this->lock};

    int to_copy = std::min(size, (seq - begin) % this->size);
    for (int i = 0; i < to_copy; i++) {
        ((char *)dst)[i] = ((char *)buff)[(begin + i) % this->size];
    }
    begin = begin + to_copy; // TODO should not change l here

    return to_copy;
}

void recv_buffer::init_seq(size_t seq)
{
    std::lock_guard<std::mutex> lock{this->lock};
    this->begin = this->seq = seq;
}

size_t recv_buffer::get_seq()
{
    std::lock_guard<std::mutex> lock{this->lock};
    return this->seq;
}

recv_buffer::~recv_buffer()
{
    free(this->buff);
}


/*
 * get host string and port number of address.
 * @param ipstr should be a char array with a length >= NI_MAXHOST
 */
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
void warn_op_fail(const char *op, struct addrinfo *rp)
{
    char ipstr[NI_MAXHOST];
    int port;

    print_errno();
    if (get_addr_host_port((struct sockaddr_in *)rp->ai_addr, ipstr, &port) >= 0)
        pr_warn("Could not %s on %s:%d\n", op, ipstr, port);
}
// Yes, I took it from ping
uint16_t get_csum(void *packet, size_t len)
{
    size_t bytes_left = len;
    auto half_words = static_cast<const uint16_t *>(packet);
    uint32_t sum = 0;
    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (bytes_left > 1)  {
        sum += *half_words++;
        bytes_left -= 2;
    }
    /* mop up an odd byte, if necessary */
    if (bytes_left == 1) {
        auto last_byte = *reinterpret_cast<const uint8_t*>(half_words);
        sum += last_byte;
    }
    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
    sum += (sum >> 16);			/* add carry */
    return (uint16_t) ~sum;				/* truncate to 16 bits */;
}

