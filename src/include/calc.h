#ifndef __CALC_H__
#define __CALC_H__

#include <stdint.h>
#include <vector>

int32_t perform_op(uint8_t opcode, std::vector<int32_t> &operands);

#endif // __CALC_H__
