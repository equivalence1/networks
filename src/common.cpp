#include <common.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>
#include <stdlib.h>

void print_errno()
{
    pr_err("%s\n", strerror(errno));
}

void handle_eptr(std::exception_ptr eptr)
{
    try {
        if (eptr) {
            std::rethrow_exception(eptr);
        }
    } catch(const std::exception& e) {
        pr_warn("Caught exception: %s\n", e.what());
    } catch (const char *s) {
        pr_warn("Caught excpetion: %s\n", s);
    } catch (...) {
        pr_warn("%s\n", "Caught unknown exception");
    }
}

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

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    std::vector<std::string> result;
    for (auto const& elem : elems)
        if (elem != "")
            result.push_back(elem);
    return result;
}

int is_tcp()
{
    char *socket_type_s = getenv("STREAM_SOCK_TYPE");
    return (socket_type_s == NULL || !strcmp(socket_type_s, "tcp"));
}
