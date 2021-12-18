
/* Though freestanding, some minimal headers are expected to exist */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define SDB_KERNEL	0
#define SDB_USER	0
#define SDB_FREESTAND	1


#if defined SDBFS_BIG_ENDIAN || defined CONFIG_ARCH_LM32
#  define ntohs(x) (x)
#  define htons(x) (x)
#  define ntohl(x) (x)
#  define htonl(x) (x)
#  define ntohll(x) (x)
#  define htonll(x) (x)
#elif defined SDBFS_LITTLE_ENDIAN || defined CONFIG_ARCH_RISCV
#  ifndef CONFIG_ARCH_RISCV
#    define CONFIG_ARCH_RISCV
#  endif
#  include <endianness.h>
#else
#  error "Unknown endianness for freestanding library"
#endif
