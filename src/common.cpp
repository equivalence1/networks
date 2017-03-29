#include <common.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

void print_errno()
{
    pr_err("%s\n", strerror(errno));
}
