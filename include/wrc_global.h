#ifndef __WRC_GLOBAL_H
#define __WRC_GLOBAL_H

#define WRC_G_MAGIC 0xADA5301E
#define WRC_G_VERSION 1

struct wrc_global {
	uint32_t magic;
	uint32_t version;
};


#endif
