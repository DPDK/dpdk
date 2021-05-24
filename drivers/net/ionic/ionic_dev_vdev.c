/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2019-2021 Pensando Systems, Inc. All rights reserved.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <rte_errno.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_dev.h>
#include <rte_string_fns.h>

#include "ionic.h"
#include "ionic_logs.h"
#include "ionic_ethdev.h"

#define IONIC_MNET_UNK      "mnet_unknown"

#define IONIC_MAX_NAME_LEN  20
#define IONIC_MAX_DEVICES   5
#define IONIC_MAX_U16_IDX   0xFFFF
#define IONIC_UIO_MAX_TRIES 32

#define IONIC_VDEV_DEV_BAR          0
#define IONIC_VDEV_INTR_CTL_BAR     1
#define IONIC_VDEV_INTR_CFG_BAR     2
#define IONIC_VDEV_DB_BAR           3
#define IONIC_VDEV_BARS_MAX         4

#define IONIC_VDEV_DEV_INFO_REGS_OFFSET      0x0000
#define IONIC_VDEV_DEV_CMD_REGS_OFFSET       0x0800

/*
 * Note: the mnet driver can assign mnet idx any number
 * in the range [0-MAX_MNET_DEVICES)
 */
#define IONIC_MAX_MNET_SCAN 32

struct ionic_map_tbl {
	char dev_name[IONIC_MAX_NAME_LEN];
	uint16_t dev_idx;
	uint16_t mnet_uio_idx;
	char mnet_dev[IONIC_MAX_NAME_LEN];
};

struct ionic_map_tbl ionic_mnet_map[IONIC_MAX_DEVICES] = {
	{ "net_ionic0", 0, IONIC_MAX_U16_IDX, IONIC_MNET_UNK },
	{ "net_ionic1", 1, IONIC_MAX_U16_IDX, IONIC_MNET_UNK },
	{ "net_ionic2", 2, IONIC_MAX_U16_IDX, IONIC_MNET_UNK },
	{ "net_ionic3", 3, IONIC_MAX_U16_IDX, IONIC_MNET_UNK },
	{ "net_ionic4", 4, IONIC_MAX_U16_IDX, IONIC_MNET_UNK },
};
struct uio_name {
	uint16_t idx;
	char name[IONIC_MAX_NAME_LEN];
};

static void
uio_fill_name_cache(struct uio_name *name_cache)
{
	char file[64];
	FILE *fp;
	char *ret;
	int name_idx = 0;
	int i;

	for (i = 0; i < IONIC_UIO_MAX_TRIES &&
			name_idx < IONIC_MAX_DEVICES; i++) {
		sprintf(file, "/sys/class/uio/uio%d/name", i);

		fp = fopen(file, "r");
		if (!fp)
			continue;

		ret = fgets(name_cache[name_idx].name, IONIC_MAX_NAME_LEN, fp);
		if (ret == NULL)
			continue;

		name_cache[name_idx].idx = i;

		fclose(fp);

		if (!strncmp(name_cache[name_idx].name, "cpu_mnic",
				strlen("cpu_mnic")))
			name_idx++;
	}
}

static int
uio_get_uionum_for_devname(struct uio_name *name_cache, char *devname)
{
	int i;

	for (i = 0; i < IONIC_MAX_DEVICES; i++)
		if (!strncmp(name_cache[i].name, devname, strlen(devname)))
			return name_cache[i].idx;

	return -1;
}

static void
uio_scan_mnet_devices(void)
{
	struct ionic_map_tbl *map;
	char mnet_devname[IONIC_MAX_NAME_LEN];
	struct uio_name name_cache[IONIC_MAX_DEVICES];
	bool done;
	int mnet_idx = 0;
	int uio_num;
	int i;

	uio_fill_name_cache(name_cache);

	for (i = 0; i < IONIC_MAX_DEVICES; i++) {
		done = false;

		while (!done) {
			if (mnet_idx > IONIC_MAX_MNET_SCAN)
				break;

			snprintf(mnet_devname, IONIC_MAX_NAME_LEN,
				"cpu_mnic%d", mnet_idx);

			uio_num = uio_get_uionum_for_devname(name_cache,
							mnet_devname);

			if (uio_num >= 0) {
				map = &ionic_mnet_map[i];
				IONIC_PRINT(INFO, "mnet map "
					"[(uio num %d), (name %s), (vdev %s)]",
					uio_num, mnet_devname, map->dev_name);
				map->mnet_uio_idx = (uint16_t)uio_num;
				strlcpy(map->mnet_dev, mnet_devname,
					IONIC_MAX_NAME_LEN);
				done = true;
			}

			mnet_idx++;
		}
	}
}

static int
uio_get_multi_dev_uionum(const char *name)
{
	struct ionic_map_tbl *map;
	int i;

	for (i = 0; i < IONIC_MAX_DEVICES; i++) {
		map = &ionic_mnet_map[i];
		if (!strncmp(map->dev_name, name, IONIC_MAX_NAME_LEN)) {
			if (map->mnet_uio_idx == IONIC_MAX_U16_IDX)
				return -1;
			else
				return map->mnet_uio_idx;
		}
	}

	return -1;
}

static unsigned long
uio_get_res_size(int uio_num, int res_idx)
{
	unsigned long size;
	char file[64];
	FILE *fp;

	sprintf(file, "/sys/class/uio/uio%d/maps/map%d/size",
		uio_num, res_idx);

	fp = fopen(file, "r");
	if (!fp)
		return 0;

	if (fscanf(fp, "0x%lx", &size) != 1) {
		IONIC_PRINT(ERR, "fscanf() failed");
		size = 0;
	}

	IONIC_PRINT(INFO, "size for resource %d: %lu", res_idx, size);

	fclose(fp);

	return size;
}

static unsigned long
uio_get_res_phy_addr_offs(int uio_num, int res_idx)
{
	unsigned long offset;
	char file[64];
	FILE *fp;

	sprintf(file, "/sys/class/uio/uio%d/maps/map%d/offset",
		uio_num, res_idx);

	fp = fopen(file, "r");
	if (!fp)
		return 0;

	if (fscanf(fp, "0x%lx", &offset) != 1) {
		IONIC_PRINT(ERR, "fscanf() failed");
		offset = 0;
	}

	fclose(fp);

	IONIC_PRINT(INFO, "offset for res map (%d) : %lx", res_idx, offset);

	return offset;
}

static unsigned long
uio_get_res_phy_addr(int uio_num, int res_idx)
{
	unsigned long addr;
	char file[64];
	FILE *fp;

	sprintf(file, "/sys/class/uio/uio%d/maps/map%d/addr",
		uio_num, res_idx);

	fp = fopen(file, "r");
	if (!fp)
		return 0;

	if (fscanf(fp, "0x%lx", &addr) != 1) {
		IONIC_PRINT(ERR, "fscanf() failed");
		addr = 0;
	}

	fclose(fp);

	IONIC_PRINT(INFO, "phys addr for res map (%d) : %lx", res_idx, addr);

	return addr;
}

static void *
uio_get_map_res_addr(int uio_num, int size, int res_idx)
{
	char name[64];
	int fd;
	void *addr;

	if (!size)
		return NULL;

	sprintf(name, "/dev/uio%d", uio_num);

	fd = open(name, O_RDWR);
	if (fd < 0)
		return NULL;

	if (size < getpagesize())
		size = getpagesize();

	addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
				fd, res_idx * getpagesize());

	close(fd);

	IONIC_PRINT(INFO, "addr for res map (%d)- size %d : %p",
		res_idx, size, addr);

	return addr;
}

static void
uio_get_resource(const char *name, int res_idx, struct ionic_dev_bar *bar)
{
	int num;
	int offs;

	num = uio_get_multi_dev_uionum(name);
	if (num < 0)
		return;

	bar->len = uio_get_res_size(num, res_idx);
	offs = uio_get_res_phy_addr_offs(num, res_idx);
	bar->bus_addr = uio_get_res_phy_addr(num, res_idx);
	bar->bus_addr += offs;
	bar->vaddr = uio_get_map_res_addr(num, bar->len, res_idx);
	bar->vaddr = ((char *)bar->vaddr) + offs;
}

static void
uio_release_resource(const char *name, int res_idx, struct ionic_dev_bar *bar)
{
	int num, offs;

	num = uio_get_multi_dev_uionum(name);
	if (num < 0)
		return;
	if (!bar->vaddr)
		return;

	offs = uio_get_res_phy_addr_offs(num, res_idx);
	munmap(((char *)bar->vaddr) - offs, bar->len);
	IONIC_PRINT(INFO, "addr for res map (%d)- size %lu : 0x%lx",
		res_idx, bar->len, (long)bar->vaddr - offs);
}

static int
ionic_vdev_setup(struct ionic_adapter *adapter)
{
	struct ionic_bars *bars = &adapter->bars;
	struct ionic_dev *idev = &adapter->idev;
	u_char *bar0_base;
	uint32_t sig;

	IONIC_PRINT_CALL();

	/* BAR0: dev_cmd and interrupts */
	if (bars->num_bars < 1) {
		IONIC_PRINT(ERR, "No bars found, aborting");
		return -EFAULT;
	}

	bar0_base = bars->bar[IONIC_VDEV_DEV_BAR].vaddr;
	idev->dev_info = (union ionic_dev_info_regs *)
		&bar0_base[IONIC_VDEV_DEV_INFO_REGS_OFFSET];
	idev->dev_cmd = (union ionic_dev_cmd_regs *)
		&bar0_base[IONIC_VDEV_DEV_CMD_REGS_OFFSET];
	idev->intr_ctrl = (void *)bars->bar[IONIC_VDEV_INTR_CTL_BAR].vaddr;
	idev->db_pages = (void *)bars->bar[IONIC_VDEV_DB_BAR].vaddr;

	sig = ioread32(&idev->dev_info->signature);
	if (sig != IONIC_DEV_INFO_SIGNATURE) {
		IONIC_PRINT(ERR, "Incompatible firmware signature %x", sig);
		return -EFAULT;
	}

	adapter->name = rte_vdev_device_name(adapter->bus_dev);

	return 0;
}

static void
ionic_vdev_poll(struct ionic_adapter *adapter)
{
	ionic_dev_interrupt_handler(adapter);
}

static void
ionic_vdev_unmap_bars(struct ionic_adapter *adapter)
{
	struct ionic_bars *bars = &adapter->bars;
	struct rte_vdev_device *vdev = adapter->bus_dev;
	uint32_t i;

	for (i = 0; i < IONIC_VDEV_BARS_MAX; i++)
		uio_release_resource(rte_vdev_device_name(vdev),
			i, &bars->bar[i]);
}

static const struct ionic_dev_intf ionic_vdev_intf = {
	.setup = ionic_vdev_setup,
	.poll = ionic_vdev_poll,
	.unmap_bars = ionic_vdev_unmap_bars,
};

static int
eth_ionic_vdev_probe(struct rte_vdev_device *vdev)
{
	struct ionic_bars bars = {};
	unsigned int i;

	IONIC_PRINT(NOTICE, "Initializing device %s",
		rte_eal_process_type() == RTE_PROC_SECONDARY ?
			"[SECONDARY]" : "");

	for (i = 0; i < IONIC_VDEV_BARS_MAX; i++) {
		uio_get_resource(rte_vdev_device_name(vdev),
			i, &bars.bar[i]);
		IONIC_PRINT(NOTICE,
			"bar[%u] = { .va = %p, .pa = 0x%jx, .len = %lu }",
			i, bars.bar[i].vaddr, bars.bar[i].bus_addr,
			bars.bar[i].len);
	}

	bars.num_bars = IONIC_VDEV_BARS_MAX;

	return eth_ionic_dev_probe((void *)vdev,
			&vdev->device,
			&bars,
			&ionic_vdev_intf,
			IONIC_DEV_ID_ETH_VF,
			IONIC_PENSANDO_VENDOR_ID);
}

static int
eth_ionic_vdev_remove(struct rte_vdev_device *vdev)
{
	return eth_ionic_dev_remove(&vdev->device);
}

static struct rte_vdev_driver rte_vdev_ionic_pmd = {
	.probe = eth_ionic_vdev_probe,
	.remove = eth_ionic_vdev_remove,
};

RTE_PMD_REGISTER_VDEV(net_ionic, rte_vdev_ionic_pmd);
RTE_PMD_REGISTER_ALIAS(net_ionic, eth_ionic);

static void
vdev_ionic_scan_cb(__rte_unused void *arg)
{
	uio_scan_mnet_devices();
}

RTE_INIT(vdev_ionic_custom_add)
{
	rte_vdev_add_custom_scan(vdev_ionic_scan_cb, NULL);
}
