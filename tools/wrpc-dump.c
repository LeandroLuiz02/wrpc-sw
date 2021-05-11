#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h> /* ntohl */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <softpll_ng.h>
#include <revision.h>
#include <arch/lm32/crt0.h>
#include <dev/netif.h>

#include <dump-info.h>
#include "time_lib.h"

/* We have a problem: ppsi is built for wrpc, so it has ntoh[sl] wrong */
#undef ntohl
#undef ntohs
#undef ntohll
#define ntohs(x) __do_not_use
#define ntohl(x) __do_not_use
#define ntohll(x) __do_not_use

/* create fancy macro to shorten the switch statements, assign val as a string to p */
#define ENUM_TO_P_IN_CASE(val, p) \
				case val: \
				    p = #val;\
				    break;


uint32_t endian_flag; /* from dump_info[0], lazily */

int print_labels = 1;

void dump_mem_ppsi_wrpc(void *mapaddr, unsigned long ppg_off);
void dump_one_field_ppsi_wrpc(int type, int size, void *p, int i);
int dump_one_field_type_ppsi_wrpc(int type, int size, void *p);

void print_str(char *s)
{
    if (print_labels == 0)
	return;
    printf(" (%s)", s);
}
/*
 * This picks items from memory, converting as needed. No ntohl any more.
 * Next, we'll detect the byte order from the code itself.
 */
long long wrpc_get_64(void *p)
{
	uint64_t *p64 = p;
	uint64_t result;

	if (endian_flag == DUMP_ENDIAN_FLAG) {
		return *p64;
	}
	result = __bswap_32((uint32_t)*p64);
	result <<= 32;
	result |= __bswap_32((uint32_t)(*p64 >> 32));
	return result;
}

/* printf complains for i/l mismatch, so get i32 and l32 separately */
long wrpc_get_l32(void *p)
{
	uint32_t *p32 = p;

	if (endian_flag == DUMP_ENDIAN_FLAG)
		return *p32;
	return __bswap_32(*p32);
}

int wrpc_get_i32(void *p)
{
	return wrpc_get_l32(p);
}

int wrpc_get_16(void *p)
{
	uint16_t *p16 = p;

	if (endian_flag == DUMP_ENDIAN_FLAG)
		return *p16;
	return __bswap_16(*p16);
}

uint8_t wrpc_get_8(void *p)
{
	uint8_t *p8 = p;

	return *p8;
}

void dump_one_field(void *addr, struct dump_info *info, char *info_prefix)
{
	void *p = addr + wrpc_get_i32(&info->offset);
	char format[16];
	char pname[128];
	int i, type, size;
	char *char_p;

	/* now, info may be in wrong-endian. so fix it */
	type = wrpc_get_i32(&info->type);
	size = wrpc_get_i32(&info->size);

	if (type == dump_type_dummy) {
		/* dummy type used to store address of e.g. complex structure.
		 * It makes no point to print such address.*/
		return;
	}

	if (info_prefix!=NULL )
		sprintf(pname, "%s.%s:", info_prefix, info->name);
	else
		sprintf(pname, "%s:", info->name);

	printf("%-60s ", pname); /* name includes trailing ':' */

//	printf("%3d|%2d|", wrpc_get_i32(&info->offset), size);

	/* For some (mostly enum-like types) the size may vary.
	 * Check the size and assign a proper value to
	 * variable i */
	switch(type) {
	case dump_type_yes_no:
		if (size == 1)
			i = *(uint8_t *)p;
		else if (size == 2)
			i = wrpc_get_16(p);
		else
			i = wrpc_get_l32(p);
		break;
	default:
		/* check if this is ppsi type */
		i = dump_one_field_type_ppsi_wrpc(type, size, p);
	}

	switch(type) {
	case dump_type_char:
		sprintf(format,"\"%%.%is\"\n", size);
		printf(format, (char *)p);
		break;
	case dump_type_bina:
		for (i = 0; i < size; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == size - 1 ? '\n' : ':');
		break;

	case dump_type_long_long:
		printf("%lld\n", wrpc_get_64(p));
		break;

	case dump_type_uint32_t:
		printf("0x%08lx\n", wrpc_get_l32(p));
		break;

	case dump_type_int:
		printf("%i\n", wrpc_get_i32(p));
		break;

	case dump_type_unsigned_long:
		printf("%li\n", wrpc_get_l32(p));
		break;

	case dump_type_unsigned_char:
	case dump_type_uint8_t:
		printf("%i\n", *(unsigned char *)p);
		break;

	case dump_type_uint16_t:
	case dump_type_unsigned_short:
		printf("%i\n", wrpc_get_16(p));
		break;

	case dump_type_double:
		printf("%lf\n", *(double *)p);
		break;

	case dump_type_float:
		printf("%f\n", *(float *)p);
		break;

	case dump_type_pointer:
		if (size == 4)
			printf("%08lx\n", wrpc_get_l32(p));
		else
			printf("%016llx\n", wrpc_get_64(p));
		break;

	case dump_type_yes_no:
		printf("%d", i);
		if (i == 0)
			print_str("no");
		else if (i == 1)
			print_str("yes");
		else
			print_str("unknown");
		printf("\n");
		break;

	case dump_type_spll_mode:
		/* check the size of type, e.g. Boolean is not 8 bits! */
		i = wrpc_get_l32(p);

		switch(i) {
		ENUM_TO_P_IN_CASE(SPLL_MODE_GRAND_MASTER, char_p);
		ENUM_TO_P_IN_CASE(SPLL_MODE_FREE_RUNNING_MASTER, char_p);
		ENUM_TO_P_IN_CASE(SPLL_MODE_SLAVE, char_p);
		ENUM_TO_P_IN_CASE(SPLL_MODE_DISABLED, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_ip_address:
		for (i = 0; i < 4; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == 3 ? '\n' : ':');
		break;

	case dump_type_link_up_status:
		i = wrpc_get_l32(p);

		switch(i) {
		ENUM_TO_P_IN_CASE(NETIF_LINK_DOWN, char_p);
		ENUM_TO_P_IN_CASE(NETIF_LINK_WENT_UP, char_p);
		ENUM_TO_P_IN_CASE(NETIF_LINK_WENT_DOWN, char_p);
		ENUM_TO_P_IN_CASE(NETIF_LINK_UP, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	default:
		dump_one_field_ppsi_wrpc(type, size, p, i);
		break;
	}
}

struct dump_info * find_s_name(char *s_name)
{
	struct dump_info *p;

	/* scan WRPC's structures */
	p = dump_wrpc_info_target;
	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, s_name)) {
			/* structure name found */
			return p;
		}

	/* scan PPSI's structures */
	p = dump_ppsi_info_target;
	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, s_name)) {
			/* structure name found */
			return p;
		}

	/* not found */
	return NULL;
}

void dump_many_fields(void *addr, char *name, char *prefix)
{
	struct dump_info *p;

	p = find_s_name(name);

	if (!p) {
		fprintf(stderr, "structure \"%s\" not described\n", name);
		return;
	}

	endian_flag = p->endian_flag;
	for (p++; p->endian_flag == 0; p++)
		dump_one_field(addr, p, prefix);
}


unsigned long wrpc_get_pointer(void *base, char *s_name, char *f_name)
{
	struct dump_info *p;
	int offset;

	p = find_s_name(s_name);

	if (!p) {
		fprintf(stderr, "structure \"%s\" not described\n", s_name);
		return 0;
	}
	endian_flag = p->endian_flag;
	/* Look for the field: we find the offset,  */
	for (p++; p->endian_flag == 0; p++) {
		if (!strcmp(p->name, f_name)) {
			offset = wrpc_get_i32(&p->offset);
			return wrpc_get_l32(base + offset);
		}
	}
	fprintf(stderr, "can't find \"%s\" in \"%s\"\n", f_name, s_name);
	return 0;
}

/* get an offset of a field in a structure */
unsigned long wrpc_get_offset(char *s_name, char *f_name)
{
	struct dump_info *p;
	int offset;

	p = find_s_name(s_name);

	if (!p) {
		fprintf(stderr, "structure \"%s\" not described\n", s_name);
		return 0;
	}
	endian_flag = p->endian_flag;
	/* Look for the field: we find the offset,  */
	for (p++; p->endian_flag == 0; p++) {
		if (!strcmp(p->name, f_name)) {
			offset = wrpc_get_i32(&p->offset);
			return offset;
		}
	}
	fprintf(stderr, "can't find \"%s\" in \"%s\"\n", f_name, s_name);
	return 0;
}

unsigned long wrpc_get_struct_size(char *s_name)
{
	struct dump_info *p;

	p = find_s_name(s_name);

	if (!p) {
		fprintf(stderr, "structure \"%s\" not described\n", s_name);
		return 0;
	}

	return wrpc_get_i32(&p->size);
}


void print_version(void)
{
	fprintf(stderr, "Built in wrpc-sw repo ver:%s, by %s on %s %s\n",
		__GIT_VER__, __GIT_USR__, __TIME__, __DATE__);
	fprintf(stderr, "Supported WRPC structures version %d\n",
		WRPC_SHMEM_VERSION);
	fprintf(stderr, "Supported PPSI structures version %d\n",
		WRS_PPSI_SHMEM_VERSION);
}

void dump_mem_wrpc_task_list(void *mapaddr, unsigned long wrc_global_off)
{
	int task_i;
	int max_task;
	char *prefix;
	char pname[128];
	long unsigned name_off, iterations_off, sec_off, nsec_off,
			max_run_ticks_off, used_off;
	void *task_addr;
	long unsigned task_struct_size;
	unsigned long task_list_off;

	task_list_off = wrpc_get_pointer(mapaddr + wrc_global_off, "wrc_global",
				   "task_list");
	if (!task_list_off) {
		return;
	}

	prefix = "wrc_global.task_list";
	printf("%s at 0x%lx\n", prefix, task_list_off);

	max_task = wrpc_get_i32(mapaddr + wrc_global_off +
			wrpc_get_offset("wrc_global", "task_list_max"));

	/* limit task_i in case max_task is not correct */
	if (max_task < 0)
		max_task = 0;
	if (max_task > 64)
		max_task = 64;

	used_off = wrpc_get_offset("wrc_task", "used");
	name_off = wrpc_get_offset("wrc_task", "name");
	iterations_off = wrpc_get_offset("wrc_task", "nrun");
	sec_off = wrpc_get_offset("wrc_task", "seconds");
	nsec_off = wrpc_get_offset("wrc_task", "nanos");
	max_run_ticks_off = wrpc_get_offset("wrc_task", "max_run_ticks");
	task_struct_size = wrpc_get_struct_size("wrc_task");

	for (task_i = 0; task_i < max_task && task_i < 32; task_i++) {
		task_addr = mapaddr + task_list_off + task_i*task_struct_size;
		if (!wrpc_get_l32(task_addr + used_off)) {
			/* skip not used tasks */
			continue;
		}
		sprintf(pname, "%s[%02d]:", prefix, task_i);
		printf("%-60s ", pname);
		printf("name: %16s ", (char *)(task_addr + name_off));
		printf("iterations: %10ld ", wrpc_get_l32(task_addr + iterations_off));
		printf("secs: %10ld.%06ld ", wrpc_get_l32(task_addr + sec_off),
					    (wrpc_get_l32(task_addr + nsec_off))/1000);
		printf("max_ms: %8ld\n", wrpc_get_l32(task_addr + max_run_ticks_off));
	}
}

void dump_mem_wrpc_global(void *mapaddr, unsigned long wrc_global_off)
{
	unsigned long tmp_off, spll_off;
	char *prefix;

	printf("wrc_global at 0x%lx\n", wrc_global_off);
	dump_many_fields(mapaddr + wrc_global_off, "wrc_global", "wrc_global");
	
	tmp_off = wrpc_get_pointer(mapaddr + wrc_global_off, "wrc_global",
				   "link_status");
	if (tmp_off) {
		prefix = "wrc_global.link_status";
		printf("%s at 0x%lx\n", prefix, tmp_off);
		dump_many_fields(mapaddr + tmp_off, "wrc_global_link",
				 prefix);
	}

	/* dump task list */
	dump_mem_wrpc_task_list(mapaddr, wrc_global_off);

	spll_off = wrpc_get_pointer(mapaddr + wrc_global_off, "wrc_global",
				   "softpll");
	if (spll_off) {
		prefix = "wrc_global.spll";
		printf("%s at 0x%lx\n", prefix, spll_off);
		dump_many_fields(mapaddr + spll_off, "struct_softpll", prefix);
	}

	/* dump config */
	tmp_off = wrpc_get_pointer(mapaddr + wrc_global_off, "wrc_global",
				   "config");
	if (tmp_off) {
		printf("wrc_global.config at 0x%lx:\n", tmp_off);
		printf("%s", (char*)(mapaddr + tmp_off));
	}

}

/* all of these are 0 by default */
unsigned long fifo_off, ppg_off, stats_off, wrc_global_off;

/* Use:  wrs_dump_memory <file> <hex-offset> <name> */
int main(int argc, char **argv)
{
	int fd;
	void *mapaddr;
	unsigned long offset;
	struct stat st;
	char *dumpname = "";
	char c;
	uint8_t version_wrpc, version_ppsi;

	if (argc != 4 && argc != 2) {
		fprintf(stderr, "%s: use \"%s <file> <offset> <name>\n",
			argv[0], argv[0]);
		fprintf(stderr,
			"\"name\" is one of pll, fifo, ppg, ppi, servo_state"
			" or ds for data-sets. \"ds\" gets a ppg offset\n");
		fprintf(stderr, "But with a new binary, just pass <file>\n\n");
		print_version();
		exit(1);
	}

	fd = open(argv[1], O_RDONLY | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "%s: %s: %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "%s: stat(%s): %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	if (!S_ISREG(st.st_mode)) { /* FIXME: support memory */
		fprintf(stderr, "%s: %s not a regular file\n",
			argv[0], argv[1]);
		exit(1);
	}

	if (st.st_size > 256 * 1024) /* support /sys/..../resource0 */
		st.st_size = 256 * 1024;

	if (argc == 4 && sscanf(argv[2], "%lx%c", &offset, &c) != 1) {
		fprintf(stderr, "%s: \"%s\" not a hex offset\n", argv[0],
			argv[2]);
		exit(1);
	}
	mapaddr = mmap(0, st.st_size, PROT_READ | PROT_WRITE,
		       MAP_FILE | MAP_PRIVATE, fd, 0);
	if (mapaddr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap(%s): %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	printf("map at %p size 0x%zx\n", mapaddr, st.st_size);
	/* In case we have a "new" binary file, use such information */
	if (!strncmp(mapaddr + WRPC_MARK, "CPRW", 4))
		setenv("WRPC_SPEC", "yes", 1);

	/* If the dump file needs "spec" byte order, fix it all */
	if (getenv("WRPC_SPEC")) {
		uint32_t *p = mapaddr;
		int i;

		for (i = 0; i < st.st_size / 4; i++, p++)
			*p = __bswap_32(*p);
	}

	if (argc == 4)
		dumpname = argv[3];

	/* If we have a new binary file, pick the pointers
	 * Magic numbers are taken from crt0.S or disassembly of wrc.bin */
	if (!strncmp(mapaddr + WRPC_MARK, "WRPC----", 8)) {
		fifo_off = wrpc_get_l32(mapaddr + FIFO_LOG_PADDR);
		ppg_off = wrpc_get_l32(mapaddr + PPG_STATIC_PADDR);
		stats_off = wrpc_get_l32(mapaddr + STATS_PADDR);
		wrc_global_off = wrpc_get_l32(mapaddr + WRC_STATIC_PADDR);
	}

	/* Check the version of wrpc and ppsi structures */
	version_wrpc = wrpc_get_8(mapaddr + VERSION_WRPC_ADDR);
	version_ppsi = wrpc_get_8(mapaddr + VERSION_PPSI_ADDR);
	if (version_wrpc != WRPC_SHMEM_VERSION) {
		printf("Unsupported version of WRPC structures! Expected %d, "
		       "but read %d\n", WRPC_SHMEM_VERSION, version_wrpc);
		exit(1);
	}
	if (version_ppsi != WRS_PPSI_SHMEM_VERSION) {
		printf("Unsupported version of PPSI structures! Expected %d, "
		       "but read %d\n", WRS_PPSI_SHMEM_VERSION, version_ppsi);
		exit(1);
	}

	if (!strcmp(dumpname, "fifo"))
		fifo_off = offset;
	if (fifo_off) {
		int i;

		printf("fifo log at 0x%lx\n", fifo_off);
		for (i = 0; i < FIFO_LOG_LEN; i++)
			dump_many_fields(mapaddr + fifo_off
					 + i * sizeof(struct spll_fifo_log),
					 "pll_fifo", "fifo");
	}

	if (!strcmp(dumpname, "ppg"))
		ppg_off = offset;
	if (ppg_off) {
		dump_mem_ppsi_wrpc(mapaddr, ppg_off);
	}

	if (!strcmp(dumpname, "stats"))
		stats_off = offset;
	if (stats_off) {
		printf("stats at 0x%lx\n", stats_off);
		dump_many_fields(mapaddr + stats_off, "stats", "stats");
	}

	if (!strcmp(dumpname, "wrc_global"))
		wrc_global_off = offset;
	if (wrc_global_off) {
		dump_mem_wrpc_global(mapaddr, wrc_global_off);
	}

	exit(0);
}
