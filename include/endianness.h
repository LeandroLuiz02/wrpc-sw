/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __ENDIANNESS_H__
#define __ENDIANNESS_H__

#ifdef CONFIG_HOST_PROCESS
#include <arpa/inet.h>

#else
# if defined CONFIG_ARCH_RISCV
#  define __ENDIANNESS_SWAP 1
# elif defined CONFIG_ARCH_LM32
#  define __ENDIANNESS_SWAP 0
# else
#  error (Wrong Arch!)
# endif

/* Declare those functions as inline (and not as macro) so that they have
   an address (but only once).  */

#define ntohll htonll
#define ntohl  htonl
#define ntohs  htons

static inline uint64_t htonll(uint64_t hostllong)
{
#if __ENDIANNESS_SWAP
	return __builtin_bswap64(hostllong);
#else
	return hostllong;
#endif
}

static inline uint32_t htonl(uint32_t hostlong)
{
#if __ENDIANNESS_SWAP
	return __builtin_bswap32(hostlong);
#else
	return hostlong;
#endif
}

static inline uint16_t htons(uint16_t hostshort)
{
#if __ENDIANNESS_SWAP
	return __builtin_bswap16(hostshort);
#else
	return hostshort;
#endif
}

#define htonl_mem(mem, size) ntohl_mem(mem, size)

/* Change endianess on a memory region */
static inline void ntohl_mem(uint8_t *mem, int size_bytes)
{
#if __ENDIANNESS_SWAP
	int i;

	for (i = 0; i < size_bytes ; i += sizeof(uint32_t)) {
		*(uint32_t*)(mem + i) = ntohl(*(uint32_t*)(mem + i));
	}
#endif
}

#endif

#endif /* ENDIANNESS_H__ */
