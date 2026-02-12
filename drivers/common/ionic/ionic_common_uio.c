/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2024 Advanced Micro Devices, Inc.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_string_fns.h>

#include "ionic_common.h"

#define IONIC_MDEV_UNK      "mdev_unknown"
#define IONIC_MNIC          "cpu_mnic"
#define IONIC_MCRYPT        "cpu_mcrypt"

#define IONIC_MAX_NAME_LEN  20
#define IONIC_MAX_MNETS     5
#define IONIC_MAX_MCPTS     1
#define IONIC_MAX_DEVICES   (IONIC_MAX_MNETS + IONIC_MAX_MCPTS)
#define IONIC_MAX_U16_IDX   0xFFFF
#define IONIC_UIO_MAX_TRIES 32

/*
 * Note: the driver can assign any idx number
 * in the range [0-IONIC_MAX_MDEV_SCAN)
 */
#define IONIC_MAX_MDEV_SCAN 32

struct ionic_map_tbl {
	char dev_name[IONIC_MAX_NAME_LEN];
	uint16_t dev_idx;
	uint16_t uio_idx;
	char mdev_name[IONIC_MAX_NAME_LEN];
};

struct ionic_map_tbl ionic_mdev_map[IONIC_MAX_DEVICES] = {
	{ "net_ionic0", 0, IONIC_MAX_U16_IDX, IONIC_MDEV_UNK },
	{ "net_ionic1", 1, IONIC_MAX_U16_IDX, IONIC_MDEV_UNK },
	{ "net_ionic2", 2, IONIC_MAX_U16_IDX, IONIC_MDEV_UNK },
	{ "net_ionic3", 3, IONIC_MAX_U16_IDX, IONIC_MDEV_UNK },
	{ "net_ionic4", 4, IONIC_MAX_U16_IDX, IONIC_MDEV_UNK },
	{ "crypto_ionic0", 5, IONIC_MAX_U16_IDX, IONIC_MDEV_UNK },
};

struct uio_name {
	uint16_t idx;
	char name[IONIC_MAX_NAME_LEN];
};

static void
uio_fill_name_cache(struct uio_name *name_cache, const char *pfx)
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
		if (fp == NULL)
			continue;

		ret = fgets(name_cache[name_idx].name, IONIC_MAX_NAME_LEN, fp);
		if (ret == NULL) {
			fclose(fp);
			continue;
		}

		name_cache[name_idx].idx = i;

		fclose(fp);

		if (strncmp(name_cache[name_idx].name, pfx, strlen(pfx)) == 0)
			name_idx++;
	}
}

static int
uio_get_idx_for_devname(struct uio_name *name_cache, char *devname)
{
	int i;

	for (i = 0; i < IONIC_MAX_DEVICES; i++)
		if (strncmp(name_cache[i].name, devname, strlen(devname)) == 0)
			return name_cache[i].idx;

	return -1;
}

RTE_EXPORT_INTERNAL_SYMBOL(ionic_uio_scan_mnet_devices)
void
ionic_uio_scan_mnet_devices(void)
{
	struct ionic_map_tbl *map;
	char devname[IONIC_MAX_NAME_LEN];
	struct uio_name name_cache[IONIC_MAX_DEVICES];
	bool done;
	int mdev_idx = 0;
	int uio_idx;
	int i;
	static bool scan_done;

	if (scan_done)
		return;

	scan_done = true;

	uio_fill_name_cache(name_cache, IONIC_MNIC);

	for (i = 0; i < IONIC_MAX_MNETS; i++) {
		done = false;

		while (!done) {
			if (mdev_idx > IONIC_MAX_MDEV_SCAN)
				break;

			/* Look for a matching mnic */
			snprintf(devname, IONIC_MAX_NAME_LEN,
				IONIC_MNIC "%d", mdev_idx);
			uio_idx = uio_get_idx_for_devname(name_cache, devname);
			if (uio_idx >= 0) {
				map = &ionic_mdev_map[i];
				map->uio_idx = (uint16_t)uio_idx;
				strlcpy(map->mdev_name, devname,
					IONIC_MAX_NAME_LEN);
				done = true;
			}

			mdev_idx++;
		}
	}
}

RTE_EXPORT_INTERNAL_SYMBOL(ionic_uio_scan_mcrypt_devices)
void
ionic_uio_scan_mcrypt_devices(void)
{
	struct ionic_map_tbl *map;
	char devname[IONIC_MAX_NAME_LEN];
	struct uio_name name_cache[IONIC_MAX_DEVICES];
	bool done;
	int mdev_idx = 0;
	int uio_idx;
	int i;
	static bool scan_done;

	if (scan_done)
		return;

	scan_done = true;

	uio_fill_name_cache(name_cache, IONIC_MCRYPT);

	for (i = IONIC_MAX_MNETS; i < IONIC_MAX_DEVICES; i++) {
		done = false;

		while (!done) {
			if (mdev_idx > IONIC_MAX_MDEV_SCAN)
				break;

			/* Look for a matching mcrypt */
			snprintf(devname, IONIC_MAX_NAME_LEN,
				IONIC_MCRYPT "%d", mdev_idx);
			uio_idx = uio_get_idx_for_devname(name_cache, devname);
			if (uio_idx >= 0) {
				map = &ionic_mdev_map[i];
				map->uio_idx = (uint16_t)uio_idx;
				strlcpy(map->mdev_name, devname,
					IONIC_MAX_NAME_LEN);
				done = true;
			}

			mdev_idx++;
		}
	}
}

static int
uio_get_multi_dev_uionum(const char *name)
{
	struct ionic_map_tbl *map;
	int i;

	for (i = 0; i < IONIC_MAX_DEVICES; i++) {
		map = &ionic_mdev_map[i];
		if (strncmp(map->dev_name, name, IONIC_MAX_NAME_LEN) == 0) {
			if (map->uio_idx == IONIC_MAX_U16_IDX)
				return -1;
			else
				return map->uio_idx;
		}
	}

	return -1;
}

static unsigned long
uio_get_res_size(int uio_idx, int res_idx)
{
	unsigned long size;
	char file[64];
	FILE *fp;

	sprintf(file, "/sys/class/uio/uio%d/maps/map%d/size",
		uio_idx, res_idx);

	fp = fopen(file, "r");
	if (fp == NULL)
		return 0;

	if (fscanf(fp, "0x%lx", &size) != 1)
		size = 0;

	fclose(fp);

	return size;
}

static unsigned long
uio_get_res_phy_addr_offs(int uio_idx, int res_idx)
{
	unsigned long offset;
	char file[64];
	FILE *fp;

	sprintf(file, "/sys/class/uio/uio%d/maps/map%d/offset",
		uio_idx, res_idx);

	fp = fopen(file, "r");
	if (fp == NULL)
		return 0;

	if (fscanf(fp, "0x%lx", &offset) != 1)
		offset = 0;

	fclose(fp);

	return offset;
}

static unsigned long
uio_get_res_phy_addr(int uio_idx, int res_idx)
{
	unsigned long addr;
	char file[64];
	FILE *fp;

	sprintf(file, "/sys/class/uio/uio%d/maps/map%d/addr",
		uio_idx, res_idx);

	fp = fopen(file, "r");
	if (fp == NULL)
		return 0;

	if (fscanf(fp, "0x%lx", &addr) != 1)
		addr = 0;

	fclose(fp);

	return addr;
}

static void *
uio_get_map_res_addr(int uio_idx, int size, int res_idx)
{
	char name[64];
	int fd;
	void *addr;

	if (size == 0)
		return NULL;

	sprintf(name, "/dev/uio%d", uio_idx);

	fd = open(name, O_RDWR);
	if (fd < 0)
		return NULL;

	if (size < getpagesize())
		size = getpagesize();

	addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
				fd, res_idx * getpagesize());

	close(fd);

	return addr;
}

RTE_EXPORT_INTERNAL_SYMBOL(ionic_uio_get_rsrc)
void
ionic_uio_get_rsrc(const char *name, int idx, struct ionic_dev_bar *bar)
{
	int num;
	int offs;

	num = uio_get_multi_dev_uionum(name);
	if (num < 0)
		return;

	bar->len = uio_get_res_size(num, idx);
	offs = uio_get_res_phy_addr_offs(num, idx);
	bar->bus_addr = uio_get_res_phy_addr(num, idx);
	bar->bus_addr += offs;
	bar->vaddr = uio_get_map_res_addr(num, bar->len, idx);
	bar->vaddr = ((char *)bar->vaddr) + offs;
}

RTE_EXPORT_INTERNAL_SYMBOL(ionic_uio_rel_rsrc)
void
ionic_uio_rel_rsrc(const char *name, int idx, struct ionic_dev_bar *bar)
{
	int num, offs;

	num = uio_get_multi_dev_uionum(name);
	if (num < 0)
		return;
	if (bar->vaddr == NULL)
		return;

	offs = uio_get_res_phy_addr_offs(num, idx);
	munmap(((char *)bar->vaddr) - offs, bar->len);
}
