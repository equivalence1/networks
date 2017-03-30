#include <protocol.h>

#include <stdint.h>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <iterator>
#include <arpa/inet.h>

template<typename Out>
static
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

static
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

bool is_blocking(int opcode)
{
    if (opcode == FIB_OP || opcode == FACT_OP)
        return false;
    return true;
}

uint8_t get_opcode(const std::string &s)
{
    if (s == "+" || s == "plus")
        return PLUS_OP;
    if (s == "-" || s == "minus")
        return MINUS_OP;
    if (s == "*" || s == "mult")
        return MULT_OP;
    if (s == "/" || s == "div")
        return DIV_OP;
    if (s == "fib")
        return FIB_OP;
    if (s == "fact")
        return FACT_OP;
    return UNKNOWN_OP;
}

#define add_to_buff(buff, offset, val) \
    do { \
        memcpy((char *)buff + offset, &(val), sizeof(val)); \
        offset += sizeof(val); \
    } while (0);

/*
 * Converts user input into format specified in protocol.h
 * @param len will be set to the length of returned buffer
 *
 * allocated buffer should be deleted afterwards
 */
void* user_to_net(const std::string &user_query, int *len)
{
    std::vector<std::string> splited_query = split(user_query, ' ');
	if (splited_query.size() == 0)
        throw "Empty query";

	uint8_t opcode = get_opcode(splited_query[0]);
    if (opcode == UNKNOWN_OP)
        throw "Unknown option";

    uint16_t msg_len = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t) * (splited_query.size() - 1);

    std::vector<uint32_t> operands;
    for (size_t i = 1; i < splited_query.size(); i++) {
        operands.push_back(htonl(stoul(splited_query[i])));
    }

    *len = msg_len;
    void *buff = malloc(msg_len);
    if (buff == NULL)
        throw "No memory";

    msg_len = htons(msg_len);

    // ok everything is prepared, now fill the buffer

    int offset = 0;
    add_to_buff(buff, offset, msg_len);
    add_to_buff(buff, offset, opcode);
    for(auto const& operand: operands)
        add_to_buff(buff, offset, operand);

    return buff;
}

int32_t get_response(tcp_connection_socket *sock)
{
    uint32_t result;
    sock->recv(&result, sizeof result);
    return (int32_t)ntohl(result);
}
