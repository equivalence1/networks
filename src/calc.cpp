#include <calc.h>
#include <protocol.h>
#include <opcode.h>

#include <vector>
#include <stdint.h>
#include <algorithm>

static
int32_t plus(std::vector<int32_t> operands)
{
    int32_t res = 0;
    for(auto const& operand: operands)
        res += operand;
    return res;
}

static
int32_t minus(std::vector<int32_t> operands)
{
    int32_t res = operands[0];
    for (size_t i = 1; i < operands.size(); i++)
        res -= operands[i];
    return res;
}

static
int32_t mult(std::vector<int32_t> operands)
{
    int32_t res = 1;
    for (auto const& operand: operands)
        res *= operand;
    return res;
}

static
int32_t div(std::vector<int32_t> operands)
{
    int32_t res = operands[0];
    for (size_t i = 1; i < operands.size(); i++)
        res /= operands[i];
    return res;
}

static
int32_t fact(std::vector<int32_t> operands)
{
    int32_t x = operands[0];
    int32_t res = 1;
    while (x > 0) {
        res *= x;
        x -= 1;
    }
    return res;
}

static
int32_t fib(std::vector<int32_t> operands)
{
    int32_t a = 1;
    int32_t b = 0;
    int32_t x = operands[0];

    while (x != 0) {
        std::swap(a, b);
        b = a + b;
        x -= 1;
    }

    return b;
}

int32_t perform_op(uint8_t opcode, std::vector<int32_t> &operands)
{
    if (operands.size() == 0)
        return 0;

    switch (opcode) {
        case PLUS_OP:
            return plus(operands);
        case MINUS_OP:
            return minus(operands);
        case MULT_OP:
            return mult(operands);
        case DIV_OP:
            return div(operands);
        case FIB_OP:
            return fib(operands);
        case FACT_OP:
            return fact(operands);
    }
    return 0; // can never happen
}
