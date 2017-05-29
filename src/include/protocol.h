#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stream_socket.h>

#include <stdint.h>
#include <string>
#include <vector>

/*
 *  our protocol:
 *
 *  client -> server
 *  +-----------------------------------------------------------------------+
 *  | msg_len (uint16_t) | opcode (uint8_t) | vector of operands (uint32_t) |
 *  +-----------------------------------------------------------------------+
 *
 *  server -> client
 *  +-------------------+
 *  | result (uint32_t) |
 *  +-------------------+
 *
 *  Even though operands and result are unsigned in network operations,
 *  on host machines they are signed int32_t integers;
 *
 */

void* user_to_net(const std::string &user_query, int *len);
std::pair<uint8_t, std::vector<int32_t> > net_to_operands(void *buff, size_t len);
int32_t get_response(stream_socket *sock);

#endif // __PROTOCOL_H__
