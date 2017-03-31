#include <common.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <exception>
#include <iostream>

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
