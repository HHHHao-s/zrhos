// Format of an ELF executable file
#include "types.h"
#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint_t magic;  // must equal ELF_MAGIC
  uchar_t elf[12];
  ushort_t type;
  ushort_t machine;
  uint_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint_t flags;
  ushort_t ehsize;
  ushort_t phentsize;
  ushort_t phnum;
  ushort_t shentsize;
  ushort_t shnum;
  ushort_t shstrndx;
};

// Program section header
struct proghdr {
  uint32_t type;
  uint32_t flags;
  uint64_t off;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
