#ifndef __LIBSDBFS_USER_H__
#define __LIBSDBFS_USER_H__

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h> /* htonl */

#define SDB_KERNEL	0
#define SDB_USER	1
#define SDB_FREESTAND	0

#define sdb_print(format, ...) fprintf(stderr, format, __VA_ARGS__)

/* This is needed to convert endianness. Hoping it is not defined elsewhere */
static inline uint64_t htonll(uint64_t ll)
{
	uint64_t res;

	if (htonl(1) == 1)
		return ll;
	res = htonl(ll >> 32);
	res |= (uint64_t)(htonl((uint32_t)ll)) << 32;
	return res;
}

static inline uint64_t ntohll(uint64_t ll)
{
	return htonll(ll);
}

#endif /* __LIBSDBFS_USER_H__ */
