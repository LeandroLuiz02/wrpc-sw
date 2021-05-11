#ifndef __WRC_GLOBAL_H
#define __WRC_GLOBAL_H
#include <lib/ipv4.h>

#define WRC_G_MAGIC 0xADA5301E
#define WRC_G_VERSION 1

#define WRC_G_LINK_VERSION 1

struct wrc_global_link {
	uint32_t version;
	int link_up;
	int vlan;
	int ip_status;
	uint8_t ip_addr[INET_ALEN];
};

struct wrc_global {
	uint32_t magic;
	uint32_t version;
	char wrc_hw_name[HW_NAME_LENGTH];
	struct wrc_global_link *link_status;
	int task_list_max;
	struct wrc_task *task_list;
	volatile struct softpll_state *softpll;
	struct spll_fifo_log *pll_fifo;
	void * config;
};

extern struct wrc_global_link wrc_global_link;
extern struct wrc_global wrc_global;

#endif
