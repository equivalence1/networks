#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#ifdef DEBUG
#define pr_success(fmt, ...)            \
        fprintf(stderr, ANSI_COLOR_GREEN "[%d]: " fmt ANSI_COLOR_RESET, getpid(), ##__VA_ARGS__)

#define pr_info(fmt, ...)               \
        fprintf(stderr, ANSI_COLOR_BLUE "[%d]: " fmt ANSI_COLOR_RESET, getpid(), ##__VA_ARGS__)

#define pr_err(fmt, ...)                \
        fprintf(stderr, ANSI_COLOR_RED "[%d]: " fmt ANSI_COLOR_RESET, getpid(), ##__VA_ARGS__)

#define pr_warn(fmt, ...)               \
        fprintf(stderr, ANSI_COLOR_YELLOW "[%d]: " fmt ANSI_COLOR_RESET, getpid(), ##__VA_ARGS__)
#else // DEBUG
#define pr_success(fmt, ...)
#define pr_info(fmt, ...)
#define pr_err(fmt, ...)
#define pr_warn(fmt, ...)
#endif // DEBUG
#define pr_msg(fmt, ...)                \
        fprintf(stdout, fmt, ##__VA_ARGS__)

void print_errno();

#endif // __COMMON_H__
