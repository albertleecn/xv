XV x86-64 Linux syscall interface | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

XV ends up making a lot of syscalls, generally on behalf of the running
process. We abstract this away into a syscall function and a series of wrapper
macros for it.

    #ifndef XV_X64_LINUX_SYSCALL_H
    #define XV_X64_LINUX_SYSCALL_H

    #define XV_SYSCALLS_DEFINED

    #include <asm/unistd.h>
    #include <sys/types.h>
    #include <stdint.h>
    #include <ucontext.h>

    #include "xv-self.h"

    int64_t xv_syscall6(int64_t n, int64_t a, int64_t b, int64_t c,
                                   int64_t d, int64_t e, int64_t f) {
      asm ("movq %3, %%r10;"
           "movq %4, %%r8;"
           "movq %5, %%r9;"
           "syscall;"
         : "=a"(n)
         : "a"(n), "D"(a), "S"(b), "d"(c), "r"(d), "r"(e), "r"(f)
         : "%r10", "%r8", "%r9", "%rcx");
      return n;
    }

    #define xv_syscall5(n,a,b,c,d,e) xv_syscall6(n,a,b,c,d,e,0)
    #define xv_syscall4(n,a,b,c,d)   xv_syscall5(n,a,b,c,d,0)
    #define xv_syscall3(n,a,b,c)     xv_syscall4(n,a,b,c,0)
    #define xv_syscall2(n,a,b)       xv_syscall3(n,a,b,0)
    #define xv_syscall1(n,a)         xv_syscall2(n,a,0)
    #define xv_syscall0(n)           xv_syscall1(n,0)

# Syscall forwarding

The virtualization module creates illegal instructions that we then detect as
SIGILL signals. The context of the illegal instruction can be used to make the
syscall that was originally intended.

This function must always be called against an unlocked XV self.

    void *xv_forward_syscall(xv_self_t *self, ucontext_t *context) {
      fail_if(xv_flag(self->flags, xv_flag_writelock),
              "xv_forward_syscall called against writelocked self");

    }

# Utility functions

These are tied to the syscall interface and are likely to change between
operating systems.

    void xv_exit(int status) {
      xv_syscall1(__NR_exit, status);
    }

    ssize_t xv_puts_(int fd, const void *buf, ssize_t len) {
      return xv_syscall3(__NR_write, fd, buf, len);
    }

    #endif