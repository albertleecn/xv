#include "../build/xv-x64.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Reads instructions from the given flat file, printing each opcode */
int main(int const argc, char const *const *const argv) {
  int const fd = open(argv[1], O_RDONLY);
  if (fd == -1) return 1;

  struct stat s;
  if (fstat(fd, &s)) return 2;

  xv_x64_const_ibuffer buf;
  buf.current = buf.start
              = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

  buf.logical_start = 0;
  buf.capacity = s.st_size;
  if (buf.start == (void *const) -1) return 3;

  printf("initialized buffer; printing bytes...\n");
  for (int i = 0;
       i < buf.capacity;
       ++i & 0x0f || printf("\n"))
    printf("%02x ", buf.start[i]);

  printf("\n");

  xv_x64_insn insn;
  int status;
  while (!(status = xv_x64_read_insn(&buf, &insn)))
    printf("rip: %04lx, opcode: %02lx, escape: %ld\n",
           (uint64_t) insn.rip,
           (uint64_t) insn.opcode,
           (uint64_t) insn.escape);

  printf("after loop\n");
  printf("rip: %04lx, opcode: %02lx, escape: %ld\n",
         (uint64_t) insn.rip,
         (uint64_t) insn.opcode,
         (uint64_t) insn.escape);

  printf("last status code: %d\n", status);

  munmap((void*) buf.start, buf.capacity);
  close(fd);
  return 0;
}
