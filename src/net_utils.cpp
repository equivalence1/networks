#include <net_utils.h>
#include <common.h>

// send buffer

send_buffer::send_buffer(size_t size)
{
    buff = malloc(size);
    if (buff == NULL)
        throw "No mem";
    l = r = 0;
    this->size = size;
}

void send_buffer::init_seq_num(size_t new_seq)
{
    l = new_seq;
    r = new_seq;
}

size_t send_buffer::get_seq_num()
{
    return l;
}

send_buffer::~send_buffer()
{
    free(buff);
}

// recv_buffer

recv_buffer::recv_buffer(size_t size)
{
    buff = malloc(size);
    if (buff == NULL)
        throw "No mem";
    l = r = 0;
    this->size = size;
}

void recv_buffer::init_seq_num(size_t new_seq)
{
    r = new_seq;
    l = new_seq;
}

size_t recv_buffer::get_seq_num()
{
    return r;
}

recv_buffer::~recv_buffer()
{
    free(buff);
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

