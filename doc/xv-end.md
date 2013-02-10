XV image end | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

This module is included at the end of each build of XV. It contains exactly two
functions, each with the same definition, and these functions serve no purpose
other than to locate the end of the image after tinyELF transformation.

Here's how this works. We don't initially know how big the ELF image is. We
could count it up externally, but that's lame; it's more fun to get the C
compiler to tell us the size. We can always get the beginning of a function,
but we don't know how long it is. So we define the same function twice in a row
so we know both its size and location; at that point, the end of the image is
`2 * xv_end_sentinel_2 - xv_end_sentinel_1`.

    #ifndef XV_END_H
    #define XV_END_H

    void xv_end_sentinel_1(void) {}
    void xv_end_sentinel_2(void) {}

    #endif