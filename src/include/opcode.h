#ifndef __OP_PARSE_H__
#define __OP_PARSE_H__

#include <stdint.h>
#include <string>

#define   PLUS_OP      0
#define   MINUS_OP     1
#define   MULT_OP      2
#define   DIV_OP       3
#define   FIB_OP       4
#define   FACT_OP      5
#define   UNKNOWN_OP   255

bool is_blocking(void *buff);
uint8_t get_opcode(const std::string &s);

#endif // __OP_PARSE_H__
