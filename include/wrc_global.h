#ifndef __WRC_GLOBAL_H
#define __WRC_GLOBAL_H

#define WRC_G_MAGIC 0xADA5301E
#define WRC_G_VERSION 1

#define WRC_G_LINK_VERSION 1

struct wrc_global_link {
	uint32_t version;
	int link_up;
	int vlan;
};

struct wrc_global {
	uint32_t magic;
	uint32_t version;
	char wrc_hw_name[HW_NAME_LENGTH];
	struct wrc_global_link *link_status;
};


#endif
