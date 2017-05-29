#include <protocol.h>
#include <opcode.h>
#include <common.h>

#include <stdint.h>
#include <string>
#include <string.h>
#include <vector>
#include <iterator>
#include <arpa/inet.h>

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
        operands.push_back(htonl((uint32_t)stoul(splited_query[i])));
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

/*
 * convert received buffer into a pair --
 * (opcode, operands vector)
 */
std::pair<uint8_t, std::vector<int32_t> >
net_to_operands(void *buff, size_t len)
{
    uint8_t opcode = (uint8_t)*((char *)buff);
    size_t offset = 1;
    std::vector<int32_t> operands;

    while (offset < len) {
        uint32_t operand;
        memcpy(&operand, (char *)buff + offset, sizeof operand);
        operands.push_back(ntohl(operand));
        offset += sizeof(uint32_t);
    }

    return std::make_pair(opcode, operands);
}

int32_t get_response(stream_socket *sock)
{
    uint32_t result;
    sock->recv(&result, sizeof result);
    return (int32_t)ntohl(result);
}
