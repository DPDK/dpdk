/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */
#include <dirent.h>

#include "macb_uio.h"

#define MACB_UIO_DRV_DIR "/sys/bus/platform/drivers/macb_uio"
#define UIO_DEV_DIR "/sys/class/uio"

static int udev_id_from_filename(char *name)
{
	enum scan_states { ss_u, ss_i, ss_o, ss_num, ss_err };
	enum scan_states state = ss_u;
	int i = 0, num = -1;
	char ch = name[0];
	while (ch && (state != ss_err)) {
		switch (ch) {
		case 'u':
			if (state == ss_u)
				state = ss_i;
			else
				state = ss_err;
			break;
		case 'i':
			if (state == ss_i)
				state = ss_o;
			else
				state = ss_err;
			break;
		case 'o':
			if (state == ss_o)
				state = ss_num;
			else
				state = ss_err;
			break;
		default:
			if ((ch >= '0') && (ch <= '9') && state == ss_num) {
				if (num < 0)
					num = (ch - '0');
				else
					num = (num * 10) + (ch - '0');
			} else {
				state = ss_err;
			}
		}
		i++;
		ch = name[i];
	}
	if (state == ss_err)
		num = -1;
	return num;
}

static int line_buf_from_filename(char *filename, char *linebuf)
{
	char *s;
	int i;
	FILE *file = fopen(filename, "r");

	if (!file)
		return -1;

	memset(linebuf, 0, UIO_MAX_NAME_SIZE);
	s = fgets(linebuf, UIO_MAX_NAME_SIZE, file);
	if (!s) {
		fclose(file);
		return -2;
	}
	for (i = 0; (*s) && (i < UIO_MAX_NAME_SIZE); i++) {
		if (*s == '\n')
			*s = '\0';
		s++;
	}
	fclose(file);
	return 0;
}

static int uio_get_map_size(const int udev_id, uint64_t *map_size)
{
	int ret;
	char filename[64];

	*map_size = UIO_INVALID_SIZE;
	snprintf(filename, sizeof(filename), "%s/uio%d/maps/map0/size",
			 UIO_DEV_DIR, udev_id);

	FILE *file = fopen(filename, "r");
	if (!file)
		return -1;

	ret = fscanf(file, "0x%" PRIx64, map_size);
	fclose(file);
	if (ret < 0)
		return -2;

	return 0;
}

static int uio_get_map_addr(const int udev_id, uint64_t *map_addr)
{
	int ret;
	char filename[64];

	*map_addr = UIO_INVALID_ADDR;
	snprintf(filename, sizeof(filename), "%s/uio%d/maps/map0/addr",
			 UIO_DEV_DIR, udev_id);

	FILE *file = fopen(filename, "r");
	if (!file)
		return -1;

	ret = fscanf(file, "0x%" PRIx64, map_addr);
	fclose(file);
	if (ret < 0)
		return -2;

	return 0;
}

static int uio_get_map_name(const int udev_id, char *map_name)
{
	char filename[64];

	snprintf(filename, sizeof(filename), "%s/uio%d/maps/map0/name",
			 UIO_DEV_DIR, udev_id);

	return line_buf_from_filename(filename, map_name);
}

static int uio_get_info_name(const int udev_id, char *info_name)
{
	char filename[64];

	snprintf(filename, sizeof(filename), "%s/uio%d/name",
			 UIO_DEV_DIR, udev_id);

	return line_buf_from_filename(filename, info_name);
}

static int uio_get_info_version(const int udev_id, char *info_ver)
{
	char filename[64];

	snprintf(filename, sizeof(filename), "%s/uio%d/version",
			 UIO_DEV_DIR, udev_id);

	return line_buf_from_filename(filename, info_ver);
}

static int uio_get_info_event_count(const int udev_id, unsigned long *event_count)
{
	int ret;
	char filename[64];

	*event_count = 0;
	snprintf(filename, sizeof(filename), "%s/uio%d/event",
			 UIO_DEV_DIR, udev_id);

	FILE *file = fopen(filename, "r");
	if (!file)
		return -1;

	ret = fscanf(file, "%d", (int *)event_count);
	fclose(file);
	if (ret < 0)
		return -2;

	return 0;
}

static int uio_get_udev_id(const char *name, int *udev_id)
{
	struct dirent **namelist;
	int n, len;
	char filename[64];
	char buf[256];

	n = scandir(UIO_DEV_DIR, &namelist, 0, alphasort);
	if (n <= 0) {
		MACB_LOG(ERR,
				 "scandir for %s "
				 "failed, errno = %d (%s)",
				 UIO_DEV_DIR, errno, strerror(errno));
		return 0;
	}

	while (n--) {
		snprintf(filename, sizeof(filename), "%s/%s", UIO_DEV_DIR,
				 namelist[n]->d_name);
		len = readlink(filename, buf, sizeof(buf) - 1);
		if (len != -1)
			buf[len] = '\0';
		if (strstr(buf, name)) {
			*udev_id = udev_id_from_filename(namelist[n]->d_name);
			break;
		}
	}

	return 0;
}

static int uio_get_all_info(struct macb_iomem *iomem)
{
	struct uio_info *info = iomem->info;
	struct uio_map *map = &info->map;
	char *name = iomem->name;

	if (!info)
		return -EINVAL;

	uio_get_udev_id(name, &iomem->udev_id);

	uio_get_info_name(iomem->udev_id, info->name);
	uio_get_info_version(iomem->udev_id, info->version);
	uio_get_info_event_count(iomem->udev_id, &info->event_count);
	uio_get_map_name(iomem->udev_id, map->name);
	uio_get_map_addr(iomem->udev_id, &map->addr);
	uio_get_map_size(iomem->udev_id, &map->size);

	return 0;
}

int macb_uio_exist(const char *name)
{
	struct dirent **namelist;
	int n, ret = 0;

	n = scandir(MACB_UIO_DRV_DIR, &namelist,
				0, alphasort);
	if (n <= 0) {
		MACB_LOG(ERR,
				 "scandir for %s "
				 "failed, errno = %d (%s)",
				 MACB_UIO_DRV_DIR, errno, strerror(errno));
		return 0;
	}

	while (n--) {
		if (!strncmp(namelist[n]->d_name, name, strlen(name)))
			ret = 1;
	}

	return ret;
}

int macb_uio_init(const char *name, struct macb_iomem **iomem)
{
	struct macb_iomem *new;
	int ret;

	new = malloc(sizeof(struct macb_iomem));
	if (!new) {
		MACB_LOG(ERR, "No memory for IOMEM obj.");
		return -ENOMEM;
	}
	memset(new, 0, sizeof(struct macb_iomem));

	new->name = strdup(name);
	if (!new->name) {
		MACB_LOG(ERR, "No memory for IOMEM-name obj.");
		ret = -ENOMEM;
		goto out_free;
	}

	new->info = malloc(sizeof(struct uio_info));
	if (!new->info) {
		ret = -ENOSPC;
		goto out_free_name;
	}

	uio_get_all_info(new);

	*iomem = new;

	return 0;

out_free_name:
	free(new->name);
out_free:
	free(new);

	return ret;
}

void macb_uio_deinit(struct macb_iomem *iomem)
{
	free(iomem->info);
	free(iomem->name);
	free(iomem);
}

static void *uio_single_mmap(struct uio_info *info, int fd, phys_addr_t paddr)
{
	unsigned long pagesize;
	size_t offset;

	if (!fd)
		return NULL;

	if (info->map.size == UIO_INVALID_SIZE)
		return NULL;

	pagesize = getpagesize();
	offset = paddr - (paddr & ~((unsigned long)pagesize - 1));
	info->map.internal_addr =
		mmap(NULL, info->map.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (info->map.internal_addr != MAP_FAILED) {
		info->map.internal_addr = (void *)((unsigned long)info->map.internal_addr + offset);
		return info->map.internal_addr;
	}

	return NULL;
}

static void uio_single_munmap(struct uio_info *info)
{
	munmap(info->map.internal_addr, info->map.size);
}

int macb_uio_map(struct macb_iomem *iomem, phys_addr_t *pa, void **va, phys_addr_t paddr)
{
	if (iomem->fd <= 0) {
		char dev_name[16];
		snprintf(dev_name, sizeof(dev_name), "/dev/uio%d",
				 iomem->udev_id);
		iomem->fd = open(dev_name, O_RDWR);
	}

	if (iomem->fd > 0) {
		*va = uio_single_mmap(iomem->info, iomem->fd, paddr);
		if (!*va)
			return -EINVAL;

		if (pa)
			*pa = paddr;
	} else {
		return -EINVAL;
	}

	return 0;
}

int macb_uio_unmap(struct macb_iomem *iomem)
{
	uio_single_munmap(iomem->info);
	if (iomem->fd > 0)
		close(iomem->fd);
	return 0;
}
