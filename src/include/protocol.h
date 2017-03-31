#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <tcp_socket.h>

#include <stdint.h>
#include <string>

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

#define   PLUS_OP      0
#define   MINUS_OP     1
#define   MULT_OP      2
#define   DIV_OP       3
#define   FIB_OP       4
#define   FACT_OP      5
#define   UNKNOWN_OP   255

bool is_blocking(void *buff);
uint8_t get_opcode(const std::string &s);
void* user_to_net(const std::string &user_query, int *len);
int32_t get_response(tcp_connection_socket *sock);

#endif // __PROTOCOL_H__
