/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */
#include "macb_common.h"

#ifndef _MACB_UIO_H_
#define _MACB_UIO_H_

#define UIO_HDR_STR		"uio_%s"
#define UIO_HDR_SZ		sizeof(UIO_HDR_STR)

#define UIO_MAX_NAME_SIZE	64
#define UIO_MAX_NUM		255

#define UIO_INVALID_SIZE	0
#define UIO_INVALID_ADDR	(~0)
#define UIO_INVALID_FD		-1

#define UIO_MMAP_NOT_DONE	0
#define UIO_MMAP_OK		1
#define UIO_MMAP_FAILED		2

struct uio_map {
	uint64_t addr;
	uint64_t size;
	char name[UIO_MAX_NAME_SIZE];
	void *internal_addr;
};

struct uio_info {
	struct uio_map map;
	unsigned long event_count;
	char name[UIO_MAX_NAME_SIZE];
	char version[UIO_MAX_NAME_SIZE];
};

struct macb_iomem {
	char *name;
	int udev_id;
	int fd;
	struct uio_info *info;
};

int macb_uio_exist(const char *name);
int macb_uio_init(const char *name, struct macb_iomem **iomem);
void macb_uio_deinit(struct macb_iomem *iomem);
int macb_uio_map(struct macb_iomem *iomem, phys_addr_t *pa, void **va, phys_addr_t paddr);
int macb_uio_unmap(struct macb_iomem *iomem);

#endif /* _MACB_UIO_H_ */
