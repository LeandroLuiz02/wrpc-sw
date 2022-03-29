/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012, 2013 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <errno.h>
#include <wrc.h>
#include <storage.h>

#include "types.h"
#include "dev/endpoint.h"
#include "dev/syscon.h"
#include <sdb.h>

#include <libsdbfs.h>

#ifndef BOARD_USE_CUSTOM_SDBFS
static const uint32_t sdbfs_default_bin[] =
{
	#include "generated/sdbfs-default.h"
};
#else
	extern const uint32_t sdbfs_default_bin[];
#endif

struct storage_device wrc_storage_dev;
struct sdbfs wrc_sdbfs;

/* glue callback code to pass storage_rwops to sdbfs */
static int sdbfs_read_callback(struct sdbfs *fs, int offset, void *buf, int count)
{
	struct storage_device *dev = (struct storage_device*) fs->drvdata;
	return dev->rwops->read( dev, offset, buf, count );
}

static int sdbfs_write_callback(struct sdbfs *fs, int offset, void *buf, int count)
{
	struct storage_device *dev = (struct storage_device*) fs->drvdata;
	return dev->rwops->write( dev, offset, buf, count );
}

static int sdbfs_erase_callback(struct sdbfs *fs, int offset, int count)
{
	struct storage_device *dev = (struct storage_device*) fs->drvdata;
	return dev->rwops->erase( dev, offset, count );
}


int storage_mount( struct storage_device *dev )
{
	uint32_t magic = 0;
	uint32_t addr;
	int i;

	/* Check if there is SDBFS in the memory */

	storage_dbg("mounting '%s' [%d bytes]\n", dev->name, dev->size );

	for (i = 0; dev->entry_points[i] >= 0; i++)
	{
		addr = dev->entry_points[i];
		if (addr >= dev->size)
			continue;

		storage_dbg("try entry point 0x%08x\n", addr);
		dev->rwops->read(dev, addr, (void *)&magic, sizeof(magic) );
		if (ntohl(magic) == SDB_MAGIC)
			goto found;
	}
	storage_dbg("SDBFS not found.\n");

	return -ENODEV;

found:
	/* found? mount it! */
	storage_dbg("found SDBFS at 0x%x in device '%s'\n", addr, dev->name );
	wrc_sdbfs.drvdata = dev;
	wrc_sdbfs.blocksize = dev->block_size;
	wrc_sdbfs.entrypoint = addr;
	wrc_sdbfs.read = sdbfs_read_callback;
	wrc_sdbfs.write = sdbfs_write_callback;
	wrc_sdbfs.erase = sdbfs_erase_callback;
	return 0;

}


static inline unsigned long SDB_ALIGN(unsigned long x, int blocksize)
{
	return (x + (blocksize - 1)) & ~(blocksize - 1);
}

int storage_sdbfs_erase( struct storage_device *dev, uint32_t addr, int force_base )
{
	int total_size = SDBFS_REC * wrc_sdbfs.blocksize;
	int count = 0;
	uint32_t base_addr;

	if (force_base)
		base_addr = addr;
	else
		base_addr = dev->cfg_entry;

	wrc_sdbfs.drvdata = dev;
	wrc_sdbfs.blocksize = dev->block_size;

	while( count < total_size )
	{
		sdbfs_erase_callback( &wrc_sdbfs, base_addr + count, wrc_sdbfs.blocksize );
		count +=  wrc_sdbfs.blocksize;
	}

	return 0;
}

int storage_sdbfs_format( struct storage_device *dev, uint32_t addr, int force_base )
{
	struct sdb_device *sdbfs =
		 (struct sdb_device *) sdbfs_default_bin;
	struct sdb_interconnect *sdbfs_dir = (struct sdb_interconnect *)
		sdbfs_default_bin;
	struct sdb_device sdbfs_buf[SDBFS_REC];

	int i;
	char buf[19] = {0};
	int cur_adr, size;
	uint32_t base_addr;

	if (force_base)
		base_addr = addr;
	else
		base_addr = dev->cfg_entry;

	wrc_sdbfs.drvdata = dev;
	wrc_sdbfs.blocksize = dev->block_size;

	/* first file starts after the SDBFS description */
	cur_adr = base_addr + SDB_ALIGN(SDBFS_REC*sizeof(struct sdb_device),
			wrc_sdbfs.blocksize );

	/* scan through files */
	for (i = 1; i < SDBFS_REC; ++i) {
		/* relocate each file depending on base address and block size*/
		size = ntohll(sdbfs[i].sdb_component.addr_last) -
			ntohll(sdbfs[i].sdb_component.addr_first);
		sdbfs[i].sdb_component.addr_first = htonll((uint64_t) cur_adr);
		sdbfs[i].sdb_component.addr_last  =
					    htonll((uint64_t)(cur_adr + size));
		cur_adr = SDB_ALIGN(cur_adr + (size + 1), wrc_sdbfs.blocksize);
	}
	/* update the directory */
	sdbfs_dir->sdb_component.addr_first = htonll(base_addr);
	sdbfs_dir->sdb_component.addr_last  =
		sdbfs[SDBFS_REC-1].sdb_component.addr_last;

	for (i = 0; i < SDBFS_REC; ++i)	{
		strncpy(buf, (char *)sdbfs[i].sdb_component.product.name, 18);
		pp_printf("filename: %s; first: %x; last: %x\n", buf,
			  (int)ntohll(sdbfs[i].sdb_component.addr_first),
			  (int)ntohll(sdbfs[i].sdb_component.addr_last));
	}


	pp_printf("Formatting SDBFS in %s (base 0x%08x, size 0x%08x)...\n",
		  dev->name, (unsigned int) base_addr,
		  (unsigned int) (SDBFS_REC * wrc_sdbfs.blocksize) );

	storage_sdbfs_erase(dev, addr, force_base);

	size = sizeof(struct sdb_device);

	for (i = 0; i < SDBFS_REC; ++i) {
		sdbfs_write_callback(&wrc_sdbfs, base_addr + i*size, &sdbfs[i],
				size);
	}

	pp_printf("Verification...\n");
	sdbfs_read_callback( &wrc_sdbfs, base_addr, sdbfs_buf, SDBFS_REC *
			     sizeof(struct sdb_device));
	if(memcmp(sdbfs, sdbfs_buf, SDBFS_REC * sizeof(struct sdb_device)))
		pp_printf("Error.\n");
	else
		pp_printf("OK.\n");

	return storage_mount( dev );
}


/*
 * A trivial dumper, just to show what's up in there
 */
void storage_sdbfs_list(void)
{
	struct sdbfs *fs = &wrc_sdbfs;
	struct sdb_device *d;
	int new = 1;

	while ((d = sdbfs_scan(fs, new)) != NULL) {
		d->sdb_component.product.record_type = '\0';
		pp_printf("file 0x%08x @ 0x%08x, name %19s\n",
			  (int)(d->sdb_component.product.device_id),
			  (int)(ntohll(d->sdb_component.addr_first)),
			  (char *)(d->sdb_component.product.name));
		new = 0;
	}
}
