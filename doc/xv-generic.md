XV platform-agnostic functions | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

This is where we do stuff like option parsing, module setup, and utility
function definitions. XV data structures are defined in xv-self.h. We
forward-declare any system-specific implementation functions needed here.

    #ifndef XV_GENERIC_H
    #define XV_GENERIC_H

    #include <stddef.h>
    #include <sys/types.h>

# Utility functions

Useful utilities that exist one layer above the platform-independent
abstractions defined by syscall layers.

    #define xv_stdin  0
    #define xv_stdout 1
    #define xv_stderr 2

    ssize_t xv_puts_(int fd, const void *buf, ssize_t len);
    #define xv_puts(fd, s) xv_puts_(fd, s, sizeof(s))
    #define xv_log(s)      xv_puts(xv_stderr, s)

    void xv_exit(int status);

    #ifdef NO_ASSERTIONS
      #define fail_if(cond, msg)
    #else
      #define fail_if(cond, msg)              \
        do {                                  \
          const char header[] = "xv error: "; \
          const char message[] = (msg);       \
          const char newline[] = "\n";        \
          if (!(cond)) {                      \
            xv_log(header);                   \
            xv_log(message);                  \
            xv_log(newline);                  \
            xv_exit(1);                       \
          }                                   \
        } while (0);
    #endif

    #endif