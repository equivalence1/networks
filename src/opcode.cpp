#include <opcode.h>

#include <vector>
#include <string>

/*
 * given buffer as specified in protocol
 * return true if operation in this buffer is blocking
 * false otherwise
 */
bool is_blocking(void *buff)
{
    uint8_t opcode = (uint8_t)(*((char *)buff + sizeof(uint16_t)));
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
