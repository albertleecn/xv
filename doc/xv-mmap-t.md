XV memory mapping table structure | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

Map tables keep track of which pages are mapped into the memory of a process.
We need to be able to query them in the following ways:

    1. Which mmap arguments produced the mapping containing address X?
    2. How much space is there around address X?

We regularly update this structure, so insertions and deletions must each be
reasonably fast (though not nearly as fast as querying). Another interesting
aspect is that this structure can't store absolute pointers; instead, it must
store relative offsets from its base. The reason is that the XV image can move
in memory arbitrarily at any moment, with the limitations that such movement is
atomic, and that no such movement will occur from within any function here.

    #ifndef XV_MMAP_T_H
    #define XV_MMAP_T_H

    #if 0
    /* Forward definitions (actual defs are platform-specific) */
    struct xv_mmap_args_t;
    inline void*  xv_mapping_start(struct xv_mmap_args_t *mapping);
    inline size_t xv_mapping_size (struct xv_mmap_args_t *mapping);
    #endif

Data structure.

    typedef struct {
    } xv_mmap_t;

    #endif