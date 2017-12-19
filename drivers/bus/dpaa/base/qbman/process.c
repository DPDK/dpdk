/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2011-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "process.h"

#include <fsl_usd.h>

/* As higher-level drivers will be built on top of this (dma_mem, qbman, ...),
 * it's preferable that the process driver itself not provide any exported API.
 * As such, combined with the fact that none of these operations are
 * performance critical, it is justified to use lazy initialisation, so that's
 * what the lock is for.
 */
static int fd = -1;
static pthread_mutex_t fd_init_lock = PTHREAD_MUTEX_INITIALIZER;

static int check_fd(void)
{
	int ret;

	if (fd >= 0)
		return 0;
	ret = pthread_mutex_lock(&fd_init_lock);
	assert(!ret);
	/* check again with the lock held */
	if (fd < 0)
		fd = open(PROCESS_PATH, O_RDWR);
	ret = pthread_mutex_unlock(&fd_init_lock);
	assert(!ret);
	return (fd >= 0) ? 0 : -ENODEV;
}

#define DPAA_IOCTL_MAGIC 'u'
struct dpaa_ioctl_id_alloc {
	uint32_t base; /* Return value, the start of the allocated range */
	enum dpaa_id_type id_type; /* what kind of resource(s) to allocate */
	uint32_t num; /* how many IDs to allocate (and return value) */
	uint32_t align; /* must be a power of 2, 0 is treated like 1 */
	int partial; /* whether to allow less than 'num' */
};

struct dpaa_ioctl_id_release {
	/* Input; */
	enum dpaa_id_type id_type;
	uint32_t base;
	uint32_t num;
};

struct dpaa_ioctl_id_reserve {
	enum dpaa_id_type id_type;
	uint32_t base;
	uint32_t num;
};

#define DPAA_IOCTL_ID_ALLOC \
	_IOWR(DPAA_IOCTL_MAGIC, 0x01, struct dpaa_ioctl_id_alloc)
#define DPAA_IOCTL_ID_RELEASE \
	_IOW(DPAA_IOCTL_MAGIC, 0x02, struct dpaa_ioctl_id_release)
#define DPAA_IOCTL_ID_RESERVE \
	_IOW(DPAA_IOCTL_MAGIC, 0x0A, struct dpaa_ioctl_id_reserve)

int process_alloc(enum dpaa_id_type id_type, uint32_t *base, uint32_t num,
		  uint32_t align, int partial)
{
	struct dpaa_ioctl_id_alloc id = {
		.id_type = id_type,
		.num = num,
		.align = align,
		.partial = partial
	};
	int ret = check_fd();

	if (ret)
		return ret;
	ret = ioctl(fd, DPAA_IOCTL_ID_ALLOC, &id);
	if (ret)
		return ret;
	for (ret = 0; ret < (int)id.num; ret++)
		base[ret] = id.base + ret;
	return id.num;
}

void process_release(enum dpaa_id_type id_type, uint32_t base, uint32_t num)
{
	struct dpaa_ioctl_id_release id = {
		.id_type = id_type,
		.base = base,
		.num = num
	};
	int ret = check_fd();

	if (ret) {
		fprintf(stderr, "Process FD failure\n");
		return;
	}
	ret = ioctl(fd, DPAA_IOCTL_ID_RELEASE, &id);
	if (ret)
		fprintf(stderr, "Process FD ioctl failure type %d base 0x%x num %d\n",
			id_type, base, num);
}

int process_reserve(enum dpaa_id_type id_type, uint32_t base, uint32_t num)
{
	struct dpaa_ioctl_id_reserve id = {
		.id_type = id_type,
		.base = base,
		.num = num
	};
	int ret = check_fd();

	if (ret)
		return ret;
	return ioctl(fd, DPAA_IOCTL_ID_RESERVE, &id);
}

/***************************************/
/* Mapping and using QMan/BMan portals */
/***************************************/

#define DPAA_IOCTL_PORTAL_MAP \
	_IOWR(DPAA_IOCTL_MAGIC, 0x07, struct dpaa_ioctl_portal_map)
#define DPAA_IOCTL_PORTAL_UNMAP \
	_IOW(DPAA_IOCTL_MAGIC, 0x08, struct dpaa_portal_map)

int process_portal_map(struct dpaa_ioctl_portal_map *params)
{
	int ret = check_fd();

	if (ret)
		return ret;

	ret = ioctl(fd, DPAA_IOCTL_PORTAL_MAP, params);
	if (ret) {
		perror("ioctl(DPAA_IOCTL_PORTAL_MAP)");
		return ret;
	}
	return 0;
}

int process_portal_unmap(struct dpaa_portal_map *map)
{
	int ret = check_fd();

	if (ret)
		return ret;

	ret = ioctl(fd, DPAA_IOCTL_PORTAL_UNMAP, map);
	if (ret) {
		perror("ioctl(DPAA_IOCTL_PORTAL_UNMAP)");
		return ret;
	}
	return 0;
}

#define DPAA_IOCTL_PORTAL_IRQ_MAP \
	_IOW(DPAA_IOCTL_MAGIC, 0x09, struct dpaa_ioctl_irq_map)

int process_portal_irq_map(int ifd, struct dpaa_ioctl_irq_map *map)
{
	map->fd = fd;
	return ioctl(ifd, DPAA_IOCTL_PORTAL_IRQ_MAP, map);
}

int process_portal_irq_unmap(int ifd)
{
	return close(ifd);
}

struct dpaa_ioctl_raw_portal {
	/* inputs */
	enum dpaa_portal_type type; /* Type of portal to allocate */

	uint8_t enable_stash; /* set to non zero to turn on stashing */
	/* Stashing attributes for the portal */
	uint32_t cpu;
	uint32_t cache;
	uint32_t window;
	/* Specifies the stash request queue this portal should use */
	uint8_t sdest;

	/* Specifes a specific portal index to map or QBMAN_ANY_PORTAL_IDX
	 * for don't care.  The portal index will be populated by the
	 * driver when the ioctl() successfully completes.
	 */
	uint32_t index;

	/* outputs */
	uint64_t cinh;
	uint64_t cena;
};

#define DPAA_IOCTL_ALLOC_RAW_PORTAL \
	_IOWR(DPAA_IOCTL_MAGIC, 0x0C, struct dpaa_ioctl_raw_portal)

#define DPAA_IOCTL_FREE_RAW_PORTAL \
	_IOR(DPAA_IOCTL_MAGIC, 0x0D, struct dpaa_ioctl_raw_portal)

static int process_portal_allocate(struct dpaa_ioctl_raw_portal *portal)
{
	int ret = check_fd();

	if (ret)
		return ret;

	ret = ioctl(fd, DPAA_IOCTL_ALLOC_RAW_PORTAL, portal);
	if (ret) {
		perror("ioctl(DPAA_IOCTL_ALLOC_RAW_PORTAL)");
		return ret;
	}
	return 0;
}

static int process_portal_free(struct dpaa_ioctl_raw_portal *portal)
{
	int ret = check_fd();

	if (ret)
		return ret;

	ret = ioctl(fd, DPAA_IOCTL_FREE_RAW_PORTAL, portal);
	if (ret) {
		perror("ioctl(DPAA_IOCTL_FREE_RAW_PORTAL)");
		return ret;
	}
	return 0;
}

int qman_allocate_raw_portal(struct dpaa_raw_portal *portal)
{
	struct dpaa_ioctl_raw_portal input;
	int ret;

	input.type = dpaa_portal_qman;
	input.index = portal->index;
	input.enable_stash = portal->enable_stash;
	input.cpu = portal->cpu;
	input.cache = portal->cache;
	input.window = portal->window;
	input.sdest = portal->sdest;

	ret =  process_portal_allocate(&input);
	if (ret)
		return ret;
	portal->index = input.index;
	portal->cinh = input.cinh;
	portal->cena  = input.cena;
	return 0;
}

int qman_free_raw_portal(struct dpaa_raw_portal *portal)
{
	struct dpaa_ioctl_raw_portal input;

	input.type = dpaa_portal_qman;
	input.index = portal->index;
	input.cinh = portal->cinh;
	input.cena = portal->cena;

	return process_portal_free(&input);
}

int bman_allocate_raw_portal(struct dpaa_raw_portal *portal)
{
	struct dpaa_ioctl_raw_portal input;
	int ret;

	input.type = dpaa_portal_bman;
	input.index = portal->index;
	input.enable_stash = 0;

	ret =  process_portal_allocate(&input);
	if (ret)
		return ret;
	portal->index = input.index;
	portal->cinh = input.cinh;
	portal->cena  = input.cena;
	return 0;
}

int bman_free_raw_portal(struct dpaa_raw_portal *portal)
{
	struct dpaa_ioctl_raw_portal input;

	input.type = dpaa_portal_bman;
	input.index = portal->index;
	input.cinh = portal->cinh;
	input.cena = portal->cena;

	return process_portal_free(&input);
}
