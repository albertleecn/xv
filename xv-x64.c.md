XV instruction stream rewriter: x86-64
Copyright (C) 2013, Spencer Tipping
Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt

# Introduction

Implementations of most of the functions in xv-x64.h; see also xv-x64.s for the
assembly-language syscall intercept.

```c
#include "xv-x64.h"
```

```c
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
```

# Operand encoding table

Enough of an approximation of each instruction's operand encoding that we can
manipulate it correctly. Note, however, that when transcribing instructions, we
also need to know some stuff like whether the source instruction had a VEX
prefix. (This is taken care of in xv_x64_insn.)

```c
#define R2(...)  __VA_ARGS__, __VA_ARGS__
#define R4(...)  R2(R2(__VA_ARGS__))
#define R8(...)  R4(R2(__VA_ARGS__))
#define R16(...) R8(R2(__VA_ARGS__))
```

```c
xv_x64_insn_encoding const xv_x64_insn_encodings[1024] = {
  /* one-byte opcode, no prefix */
  /* 0x00 - 0x3f */ R8(R4(XV_MODRM_MEM | XV_IMM_NONE),  /* ALU */
                       XV_MODRM_NONE | XV_IMM_I8,       /* ALU imm8 */
                       XV_MODRM_NONE | XV_IMM_ISZW,     /* ALU imm */
                       R2(XV_INVALID)),
```

```c
  /* 0x40 - 0x4f */ R16(XV_INVALID),                    /* REX prefix */
  /* 0x50 - 0x5f */ R16(XV_MODRM_NONE | XV_IMM_NONE),
```

```c
  /* 0x60 - 0x6f */ R8(XV_INVALID),                     /* invalid, prefixes */
                    XV_MODRM_NONE | XV_IMM_ISZW,        /* push imm */
                    XV_MODRM_MEM  | XV_IMM_ISZW,        /* three-arg imul */
                    XV_MODRM_NONE | XV_IMM_I8,          /* push imm8 */
                    XV_MODRM_MEM  | XV_IMM_I8,          /* three-arg imul */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* ins, outs */
```

```c
  /* 0x70 - 0x7f */ R16(XV_MODRM_NONE | XV_IMM_D8),     /* jcc */
```

```c
  /* 0x80 - 0x8f */ XV_MODRM_MEM | XV_IMM_I8,           /* group1 insns */
                    XV_MODRM_MEM | XV_IMM_ISZW,         /* group1 insns */
                    XV_INVALID,
                    XV_MODRM_MEM | XV_IMM_I8,           /* group1 insns */
                    R4(XV_MODRM_MEM | XV_IMM_NONE),     /* test, xchg */
                    R8(XV_MODRM_MEM | XV_IMM_NONE),     /* mov, pop, lea */
```

```c
  /* 0x90 - 0x9f */ R8(XV_MODRM_NONE | XV_IMM_NONE),    /* xchg with %rax */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* %rax conversions */
                    XV_INVALID,                         /* callf */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* wait */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* flag insns */
```

```c
  /* 0xa0 - 0xaf */ R2(XV_MODRM_NONE | XV_IMM_I8,       /* mov %al */
                       XV_MODRM_NONE | XV_IMM_ISZQ),    /* mov %[re_]ax */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* movs */
                    XV_MODRM_NONE | XV_IMM_I8,          /* test %al */
                    XV_MODRM_NONE | XV_IMM_ISZW,        /* test %[re_]ax */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* stos[b] */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* lods[b], scas[b] */
```

```c
  /* 0xb0 - 0xbf */ R8(XV_MODRM_NONE | XV_IMM_I8),      /* movb %rxx, ib */
                    R8(XV_MODRM_NONE | XV_IMM_ISZW),    /* mov[wlq] ... */
```

```c
  /* 0xc0 - 0xcf */ R2(XV_MODRM_MEM | XV_IMM_I8),       /* group2 insns */
                    XV_MODRM_NONE | XV_IMM_ISZW,        /* ret imm */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* ret */
                    R2(XV_INVALID),                     /* les, lds */
                    XV_MODRM_MEM | XV_IMM_I8,           /* movb */
                    XV_MODRM_MEM | XV_IMM_ISZW,         /* mov[wlq] */
                    XV_MODRM_NONE | XV_IMM_I2,          /* enter */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* leave */
                    XV_MODRM_NONE | XV_IMM_I16,         /* retf imm16 */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* retf */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* int 3 */
                    XV_MODRM_NONE | XV_IMM_I8,          /* int imm8 */
                    XV_INVALID,                         /* into */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* iret */
```

```c
  /* 0xd0 - 0xdf */ R4(XV_MODRM_MEM | XV_IMM_NONE),     /* group2 insns */
                    R2(XV_INVALID),                     /* aam, aad */
                    XV_INVALID,
                    XV_MODRM_NONE | XV_IMM_NONE,        /* xlat[b_] */
                    R8(XV_INVALID),                     /* coprocessor */
```

```c
  /* 0xe0 - 0xef */ R4(XV_MODRM_NONE | XV_IMM_D8),      /* loop, jcxz */
                    R4(XV_MODRM_NONE | XV_IMM_I8),      /* in/out %al, ... */
                    R2(XV_MODRM_NONE | XV_IMM_DSZW),    /* call/jmp dispW */
                    XV_INVALID,                         /* far jmp */
                    XV_MODRM_NONE | XV_IMM_D8,          /* short jmp disp8 */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* in/out %a, %d */
```

```c
  /* 0xf0 - 0xff */ R4(XV_INVALID),                     /* prefixes */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* hlt, cmc */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* group3 insns */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* flags */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* flags */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* group4/5 insns */
```

```c
  /* two-byte opcode, 0x0f prefix */
  /* 0x00 - 0x0f */ XV_MODRM_MEM | XV_IMM_NONE,         /* group6 insns */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* group7 insns */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* lar, lsl */
                    XV_INVALID,
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* syscall, clts */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* sysret */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* invd, wbinvd */
                    R2(XV_INVALID),
                    XV_INVALID,
                    XV_MODRM_MEM | XV_IMM_NONE,         /* prefetchw */
                    R2(XV_INVALID),
```

```c
  /* 0x10 - 0x1f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* AVX, prefetch */
  /* 0x20 - 0x2f */ R4(XV_MODRM_MEM | XV_IMM_NONE),     /* AVX */
                    R4(XV_INVALID),
                    R8(XV_MODRM_MEM | XV_IMM_NONE),     /* SSE, AVX */
```

```c
  /* 0x30 - 0x3f */ R4(XV_MODRM_NONE | XV_IMM_NONE),    /* MSR */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* sysenter, sysexit */
                    XV_INVALID,
                    XV_MODRM_NONE | XV_IMM_NONE,        /* getsec */
                    R8(XV_INVALID),                     /* prefix, invalid */
```

```c
  /* 0x40 - 0x4f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* cmov */
  /* 0x50 - 0x5f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
  /* 0x60 - 0x6f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
```

```c
  /* 0x70 - 0x7f */ R4(XV_MODRM_MEM | XV_IMM_I8),       /* SSE, AVX imm8 */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* SSE, AVX */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* SSE, AVX */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* emms */
                    R8(XV_MODRM_MEM | XV_IMM_NONE),     /* VMX, SSE, AVX */
```

```c
  /* 0x80 - 0x8f */ R16(XV_MODRM_NONE | XV_IMM_DSZW),   /* long jcc */
  /* 0x90 - 0x9f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* setcc */
```

```c
  /* 0xa0 - 0xaf */ R2(XV_MODRM_NONE | XV_IMM_NONE),    /* push fs, pop fs */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* cpuid */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* bt */
                    XV_MODRM_MEM | XV_IMM_I8,           /* shld imm8 */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* shld %cl */
                    R2(XV_INVALID),
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* push gs, pop gs */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* rsm */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* bts */
                    XV_MODRM_MEM | XV_IMM_I8,           /* shrd imm8 */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* shrd %cl */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* group15 insns */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* imul */
```

```c
  /* 0xb0 - 0xbf */ R8(XV_MODRM_MEM | XV_IMM_NONE),     /* misc */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* popcnt */
                    XV_INVALID,                         /* group10 invalid */
                    XV_MODRM_MEM | XV_IMM_I8,           /* group8 insns */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* btc */
                    R4(XV_MODRM_MEM | XV_IMM_NONE),     /* bsf, bsr, movsx */
```

```c
  /* 0xc0 - 0xcf */ R2(XV_MODRM_MEM | XV_IMM_NONE),     /* xadd */
                    XV_MODRM_MEM | XV_IMM_I8,           /* vcmpxx */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* movnti */
                    R2(XV_MODRM_MEM | XV_IMM_I8),       /* SSE, AVX */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* group9 insns */
                    R8(XV_MODRM_NONE | XV_IMM_NONE),    /* bswap */
```

```c
  /* 0xd0 - 0xdf */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
  /* 0xe0 - 0xef */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
  /* 0xf0 - 0xff */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
```

```c
  /* three-byte opcode, 0x0f 0x38 prefix */
  R16(R16(XV_MODRM_MEM | XV_IMM_NONE)),                 /* SSE, AVX */
```

```c
  /* three-byte opcode, 0x0f 0x3a prefix */
  R16(R16(XV_MODRM_MEM | XV_IMM_I8))                    /* SSE, AVX */
};
```

```c
#undef R2
#undef R4
#undef R8
#undef R16
```

# Write buffer reallocation

This is kind of interesting. xv uses a lame allocation strategy: you allocate
some initial memory, hopefully enough, then it tries to write into that memory.
If it fails, you have to throw it all away, allocate more memory, and try
again. The reason is that instructions change as you move them, and in some
cases even their sizes change. So there is no way you can reuse the old
compilation output. Fortunately, rewriting should be fast enough that this
isn't a problem.

Note that the first time you "reallocate" a buffer, it should have a zero
start-pointer. Otherwise this call will do nothing and return an error code
because it will think it couldn't free memory.

```c
int xv_x64_reallocate_ibuffer(xv_x64_ibuffer *const buf,
                              ssize_t         const size) {
  ssize_t const page_size = getpagesize();
  ssize_t const rounded   = size + page_size - 1 & ~(page_size - 1);
```

```c
  if (buf->capacity == rounded) return 0;
  if (buf->start && munmap(buf->start, buf->capacity)) return errno;
```

```c
  if (size > 0) {
    void *const region = mmap(NULL,
                              rounded,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1,
                              0);
```

```c
    if (region == (void *const) -1) return errno;
```

```c
    buf->start    = region;
    buf->capacity = rounded;
  } else {
    buf->start    = NULL;
    buf->capacity = 0;
  }
```

```c
  return 0;
}
```

# Instruction parser

See xv-x64.h for relevant definitions. In particular, we make heavy use of the
encoding table, which tells us just about everything we need to know about the
instruction.

```c
/* Instruction-prefix predicates for byte values */
#define XV_G1P(x) ((x) == 0xf0 || (x) == 0xf2 || (x) == 0xf3)
```

```c
#define XV_G2P(x) ((x) == 0x2e || (x) == 0x36 || \
                   (x) == 0x3e || (x) == 0x26 || \
                   (x) == 0x64 || (x) == 0x65)
```

```c
#define XV_G3P(x) ((x) == 0x66)
#define XV_G4P(x) ((x) == 0x67)
```

```c
#define XV_REXP(x)  (((x) & 0xf0) == 0x40)
#define XV_VEX3P(x) ((x) == 0xc4)
#define XV_VEX2P(x) ((x) == 0xc5)
```

```c
#define XV_OPESC1P(x) ((x) == 0x0f)
#define XV_OPESC2P(x) ((x) == 0x38 || (x) == 0x3a)
```

```c
static inline int xv_x64_immediate_bytes(xv_x64_insn const *const insn) {
  xv_x64_insn_encoding const enc = xv_x64_insn_encodings[xv_x64_insn_key(insn)];
  switch (enc & XV_IMM_MASK) {
    case XV_IMM_NONE: return 0;
    case XV_IMM_D8:
    case XV_IMM_I8:   return 1;
    case XV_IMM_I16:  return 2;
    case XV_IMM_D32:
    case XV_IMM_I32:  return 4;
    case XV_IMM_I64:  return 8;
    case XV_IMM_DSZW:
    case XV_IMM_ISZW: return insn->p66 ? 2 : 4;
    case XV_IMM_I2:   return 3;
    case XV_IMM_ISZQ: return insn->p66 ? 2 : insn->rex_w ? 8 : 4;
    default:          return 0;
  }
}
```

```c
int xv_x64_read_insn(xv_x64_const_ibuffer *const buf,
                     xv_x64_insn          *const insn) {
  int offset = buf->current - buf->start;
```

```c
  memset(insn, 0, sizeof(xv_x64_insn));
```

```c
  /* Look for group 1, 2, 3, and 4 prefixes, ignoring each one after we see the
   * first. We're done looking if we hit the end of the input stream, or if we
   * see anything that isn't a prefix. */
  if (offset >= buf->capacity) return XV_READ_ENDP;
  for (unsigned g1p, g2p, g3p, g4p, current = buf->start[offset];
```

```c
       offset < buf->capacity
         && ((g1p = XV_G1P(current)) || (g2p = XV_G2P(current)) ||
             (g3p = XV_G3P(current)) || (g4p = XV_G4P(current)));
```

```c
       ++offset,
       insn->p1 = !insn->p1 && g1p ? current == 0xf0 ? XV_INSN_LOCK
                                   : current == 0xf2 ? XV_INSN_REPNZ
                                                     : XV_INSN_REPZ
                                   : insn->p1,
```

```c
       insn->p2 = !insn->p2 && g2p ? current == 0x2e ? XV_INSN_CS
                                   : current == 0x36 ? XV_INSN_SS
                                   : current == 0x3e ? XV_INSN_DS
                                   : current == 0x26 ? XV_INSN_ES
                                   : current == 0x64 ? XV_INSN_FS
                                                     : XV_INSN_GS
                                   : insn->p2,
       insn->p66 |= g3p,
       insn->p67 |= g4p);
```

```c
  /* Now look for REX and VEX prefixes. If there are multiple (technically
   * disallowed), we will get wonky results. */
  if (offset >= buf->capacity) return XV_READ_ENDO1;
  for (unsigned rexp  = 0, vex2p   = 0,
                vex3p = 0, current = buf->start[offset];
```

```c
       offset < buf->capacity
         && ((rexp  =  XV_REXP(current = buf->start[offset])) ||
             (vex2p = XV_VEX2P(current)) ||
             (vex3p = XV_VEX3P(current)));
```

```c
       ++offset,
       insn->vex   |= vex2p || vex3p,
       insn->rex_w |= rexp && !!(current & 0x08),
       insn->r1    |= rexp &&   (current & 0x04) << 1,  /* REX.R */
       insn->index |= rexp &&   (current & 0x02) << 2,  /* REX.X */
       insn->base  |= rexp &&   (current & 0x01) << 3,  /* REX.B */
       rexp = vex2p = vex3p = 0)
```

```c
    if (vex2p || vex3p) {
      if (vex2p) {
        if (++offset >= buf->capacity) return XV_READ_ENDV2;
        current = buf->start[offset];
        insn->escape = XV_INSN_ESC1;                    /* implied */
        insn->r1     = !(current & 0x80) << 3;          /* VEX.R */
      } else if (vex3p) {
        if (++offset >= buf->capacity) return XV_READ_ENDV2;
        current = buf->start[offset];
        insn->r1     = !(current & 0x80) << 3;          /* VEX.R */
        insn->index  = !(current & 0x40) << 3;          /* VEX.X */
        insn->base   = !(current & 0x20) << 3;          /* VEX.B */
        insn->escape = current & 0x1f;                  /* VEX.m-mmmm */
```

```c
        if (++offset >= buf->capacity) return XV_READ_ENDV3;
        current = buf->start[offset];
        insn->rex_w = !!(current & 0x80);               /* VEX.W */
      }
```

```c
      insn->r3     =   (current & 0x78) >> 3 ^ 0x0f;    /* VEX.vvvv */
      insn->vex_l  = !!(current & 0x04);
      insn->p66   |= current == 0x01;                   /* VEX.pp */
      insn->p2     = (current & 0x03) == 2 ? XV_INSN_REPZ
                   : (current & 0x03) == 3 ? XV_INSN_REPNZ
                   : insn->p2;
    }
```

```c
  /* By this point we've read all prefixes except for opcode escapes. Now look
   * for those. */
  if (offset >= buf->capacity) return XV_READ_ENDO2;
  if (XV_OPESC1P(buf->start[offset])) {
    /* We have a 0x0f prefix; now see whether we have either 0x38 or 0x3a */
    if (++offset >= buf->capacity) return XV_READ_ENDO3;
```

```c
    unsigned current = buf->start[offset];
    if (XV_OPESC2P(current)) {
      insn->escape = current == 0x38 ? XV_INSN_ESC238 : XV_INSN_ESC23A;
      if (++offset >= buf->capacity) return XV_READ_ENDO4;
    } else
      insn->escape = 1;
  }
```

```c
  insn->opcode = buf->start[offset];
  if (xv_x64_insn_encodings[xv_x64_insn_key(insn)] == XV_INVALID)
    return XV_READ_INV;
```

```c
  /* Now we have the opcode and all associated prefixes, so we can parse out
   * the remaining bytes using the operand encoding table. */
  xv_x64_insn_encoding const enc =
    xv_x64_insn_encodings[xv_x64_insn_key(insn)];
```

```c
  /* If we use ModR/M, parse that byte and check for SIB. */
  if (enc & XV_MODRM_MASK) {
    if (++offset >= buf->capacity) return XV_READ_ENDM;
    unsigned current = buf->start[offset];
    insn->mod  = (current & 0xc0) >> 6;
    insn->r1  |= (current & 0x38) >> 3;
```

```c
    /* Detect special encoding for absolute-mode address */
    if (insn->mod == 0 && (current & 0x07) == XV_RBP)
      insn->base = XV_NO_REG;
```

```c
    int const displacement_bytes =
        insn->mod == 0 ? (current & 0x07) == 5 ? 4 : 0
      : insn->mod == 1 ? 1
      : insn->mod == 2 ? 4 : 0;
```

```c
    if (insn->mod != 3 && (current & 0x07) == XV_RSP) {
      if (++offset >= buf->capacity) return XV_READ_ENDS;
      current = buf->start[offset];
      insn->scale  = (current & 0xc0) >> 6;
      insn->index |= (current & 0x38) >> 3;
```

```c
      /* TODO: detect special cases */
    }
```

```c
    insn->base |= current & 0x07;
```

```c
    /* Now read little-endian displacement. */
    if ((offset += displacement_bytes) >= buf->capacity) return XV_READ_ENDD;
    for (int i = 0; i < displacement_bytes; ++i)
      insn->displacement = insn->displacement << 8 | buf->start[offset - i];
  }
```

```c
  /* And read the immediate data, little-endian. */
  int const immediate_size = xv_x64_immediate_bytes(insn);
  if ((offset += immediate_size) >= buf->capacity) return XV_READ_ENDI;
  for (int i = 0; i < immediate_size; ++i)
    insn->immediate = insn->immediate << 8 | buf->start[offset - i];
```

```c
  /* Now update the buffer and record the instruction's logical %rip value.
   * This will allow us to track the absolute address for cases like
   * %rip-relative displacement. */
  buf->current = buf->start + ++offset;
  insn->rip    = buf->current - buf->start + buf->logical_start;
```

```c
  return XV_READ_CONT;
}
```

# Instruction generator

Write a structured instruction back out as x86 machine code. To do this, we
first size out the instruction and then, assuming there's space, copy the
memory. The buffer will be unchanged if any errors occur.

Note that we rely entirely on the .vex field to tell us whether we should
VEX-encode the instruction. This means that instructions are closed under VEX
encoding, so xv will at the very least not VEX-encode stuff on a processor that
would have run the program otherwise.

```c
static xv_x64_const_i xv_g1v[4] = { 0x00, 0xf0, 0xf2, 0xf3 };
static xv_x64_const_i xv_g2v[7] = { 0x00, 0x2e, 0x36, 0x3e, 0x26, 0x64, 0x65 };
```

```c
/* Return nonzero if the quantity stored in x overflows the given number of
 * bits; also works for unsigned data */
static inline int xv_overflowp(int64_t  const x,
                               unsigned const bits) {
  int64_t const high = x >> bits;
  return high == 0 || high == -1;
}
```

```c
int xv_x64_write_insn(xv_x64_ibuffer    *const buf,
                      xv_x64_insn const *const insn) {
  xv_x64_i                   stage[15] = { 0 };
  unsigned                   index     = 0;
  xv_x64_insn_encoding const enc       =
    xv_x64_insn_encodings[xv_x64_insn_key(insn)];
```

```c
  /* Validity check: is the opcode known to be invalid? */
  if (enc == XV_INVALID) return XV_WR_INV;
```

```c
  /* Validity check: are we trying to encode an immediate larger than what the
   * instruction supports? */
  if (xv_overflowp(insn->immediate, 8 * xv_x64_immediate_bytes(insn)))
    return XV_WR_EOP;
```

```c
  /* Reconstruct group 1, 2, 3, and 4 prefixes */
  if (insn->p1)  stage[index++] = xv_g1v[insn->p1];
  if (insn->p2)  stage[index++] = xv_g2v[insn->p2];
  if (insn->p66) stage[index++] = 0x66;
  if (insn->p67) stage[index++] = 0x67;
```

```c
  /* Encode any REX or VEX prefix */
  if (insn->vex) {
    /* Preserve VEX */
    xv_x64_i const vvvv_l_pp = (~insn->r3 & 0x0f) << 3
                             | insn->vex_l        << 2
                             | insn->escape;
```

```c
    if (insn->escape != XV_INSN_ESC1
        || insn->rex_w
        || 0x08 & (insn->index | insn->base | insn->r1))
      /* Need a three-byte prefix */
      stage[index++] = 0xc4,
      stage[index++] = (1 & ~insn->r1    >> 3) << 7
                     | (1 & ~insn->index >> 3) << 6
                     | (1 & ~insn->base  >> 3) << 5
                     | insn->escape,
      stage[index++] = insn->rex_w << 7 | vvvv_l_pp;
    else
      /* Two-byte prefix */
      stage[index++] = 0xc5,
      stage[index++] = (1 & ~insn->r1 >> 3) << 7 | vvvv_l_pp;
  } else {
    if (insn->rex_w || 0x08 & (insn->r1 | insn->index | insn->base))
      /* REX is required */
      stage[index++] = 0x40
                     | insn->rex_w            << 3
                     | (1 & insn->r1    >> 3) << 2
                     | (1 & insn->index >> 3) << 1
                     | (1 & insn->base  >> 3) << 0;
```

```c
    /* Opcode escaping: VEX captures this information inside the prefix, but
     * REX and no-prefix forms require us to write escape bytes */
    switch (insn->escape) {
      case XV_INSN_ESC0:   break;
      case XV_INSN_ESC1:   stage[index++] = 0x0f; break;
      case XV_INSN_ESC238: stage[index++] = 0x0f; stage[index++] = 0x38; break;
      case XV_INSN_ESC23A: stage[index++] = 0x0f; stage[index++] = 0x3a; break;
    }
  }
```

```c
  stage[index++] = insn->opcode;
```

```c
  /* Now encode operands. This is a matter of a few things. First, we need to
   * figure out the x86 way to express what is logically encoded in the operand
   * data. For example, we may have a %rip-relative address, which is encoded
   * as a special case of ModR/M and SIB. */
  if (enc & XV_MODRM_MASK) {
    xv_x64_i const reg = insn->r1 << 3 & 0x38;
```

```c
    /* Figure out what kind of ModR/M and SIB setup to use. Here are the
     * possibilities:
     *
     * 1. Absolute addressing: use SIB with index = %rsp.
     * 2. Instruction is %rip-relative: mod = 0, r/m = %rbp.
     * 3. %rsp or %r12 is base: use SIB with scale = 0.
     * 4. %rbp or %r13 is base: force displacement encoding.
     *
     * Otherwise, we can encode the instruction as specified. */
```

```c
    unsigned displacement_bytes = 0;
    switch (insn->addr) {
      case XV_INSN_RIPREL:
        stage[index++] = XV_RBP | reg;
        displacement_bytes = 4;
        break;
```

```c
      case XV_INSN_ABS:
        displacement_bytes = xv_overflowp(insn->displacement, 1) ? 4 : 1;
        stage[index++] = (displacement_bytes == 4 ? 0x80 : 0x40)
                       | reg
                       | XV_RSP;
        stage[index++] = insn->scale << 6
                       | insn->index << 3 & 0x38
                       | insn->base       & 0x07;
        break;
```

```c
      default:
        /* No known-wonky cases yet, but some that still require escaping.
         * Check for those: */
        if (insn->base & 0x07 == XV_RSP) {
          /* TODO */
        }
    }
```

```c
    if (xv_overflowp(insn->displacement, displacement_bytes * 8))
      return XV_WR_EOP;
```

```c
    --displacement_bytes;
    for (int i = 0; i <= displacement_bytes; ++i)
      stage[index++] =
        insn->displacement >> (displacement_bytes - i << 3) & 0xff;
  }
```

```c
  return XV_WR_ERR;
}
```

# Instruction rewriter

This is where the fun stuff happens. This function steps a rewriter forward by
one instruction, returning a status code to indicate what should happen next.

```c
inline int xv_x64_riprelp(xv_x64_insn const *const insn) {
  return insn->mod == 0 && insn->base == XV_RSP;
}
```

```c
inline int xv_x64_immrelp(xv_x64_insn const *const insn) {
  xv_x64_insn_encoding const enc = xv_x64_insn_encodings[xv_x64_insn_key(insn)];
  unsigned             const imm = enc & XV_IMM_MASK;
  return imm == XV_IMM_D8
      || imm == XV_IMM_D32
      || imm == XV_IMM_DSZW;
}
```

```c
inline int xv_x64_syscallp(xv_x64_insn const *const insn) {
  return insn->escape == XV_INSN_ESC1   /* syscall, sysenter */
           && (insn->opcode == 0x05 || insn->opcode == 0x34)
      || insn->escape == XV_INSN_ESC0   /* int 80; maybe invalid in 64-bit? */
           && insn->opcode    == 0xcd
           && insn->immediate == 0x80;
}
```

```c
int xv_x64_step_rw(xv_x64_rewriter *const shared_rw) {
  /* TODO */
  return XV_RW_R;
}

```