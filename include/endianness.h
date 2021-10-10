/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __ENDIANNESS_H__
#define __ENDIANNESS_H__

#ifdef CONFIG_HOST_PROCESS
#include <arpa/inet.h>

#elif defined CONFIG_ARCH_RISCV
static inline uint32_t htonl(uint32_t hostlong)
{
    return __builtin_bswap32(hostlong);
}

static inline uint16_t htons(uint16_t hostshort){
    return __builtin_bswap16(hostshort);
}

static inline uint32_t ntohl(uint32_t netlong){
    return __builtin_bswap32(netlong);
}

static inline uint16_t ntohs(uint16_t netshort){
    return __builtin_bswap16(netshort);
}
#elif defined CONFIG_ARCH_LM32
#define ntohs(x) (x)
#define ntohl(x) (x)
#define htons(x) (x)
#define htonl(x) (x)

#else
#error (Wrong Arch!)

#endif

#endif /* ENDIANNESS_H__ */
