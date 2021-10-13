/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 * Copyright(c) 2021 Intel Corporation
 */

#include <inttypes.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>

#include "rte_dmadev.h"
#include "rte_dmadev_pmd.h"

static int16_t dma_devices_max;

struct rte_dma_dev *rte_dma_devices;

RTE_LOG_REGISTER_DEFAULT(rte_dma_logtype, INFO);
#define RTE_DMA_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, rte_dma_logtype, RTE_FMT("dma: " \
		RTE_FMT_HEAD(__VA_ARGS__,) "\n", RTE_FMT_TAIL(__VA_ARGS__,)))

int
rte_dma_dev_max(size_t dev_max)
{
	/* This function may be called before rte_eal_init(), so no rte library
	 * function can be called in this function.
	 */
	if (dev_max == 0 || dev_max > INT16_MAX)
		return -EINVAL;

	if (dma_devices_max > 0)
		return -EINVAL;

	dma_devices_max = dev_max;

	return 0;
}

static int
dma_check_name(const char *name)
{
	size_t name_len;

	if (name == NULL) {
		RTE_DMA_LOG(ERR, "Name can't be NULL");
		return -EINVAL;
	}

	name_len = strnlen(name, RTE_DEV_NAME_MAX_LEN);
	if (name_len == 0) {
		RTE_DMA_LOG(ERR, "Zero length DMA device name");
		return -EINVAL;
	}
	if (name_len >= RTE_DEV_NAME_MAX_LEN) {
		RTE_DMA_LOG(ERR, "DMA device name is too long");
		return -EINVAL;
	}

	return 0;
}

static int16_t
dma_find_free_id(void)
{
	int16_t i;

	if (rte_dma_devices == NULL)
		return -1;

	for (i = 0; i < dma_devices_max; i++) {
		if (rte_dma_devices[i].state == RTE_DMA_DEV_UNUSED)
			return i;
	}

	return -1;
}

static struct rte_dma_dev*
dma_find_by_name(const char *name)
{
	int16_t i;

	if (rte_dma_devices == NULL)
		return NULL;

	for (i = 0; i < dma_devices_max; i++) {
		if ((rte_dma_devices[i].state != RTE_DMA_DEV_UNUSED) &&
		    (!strcmp(name, rte_dma_devices[i].dev_name)))
			return &rte_dma_devices[i];
	}

	return NULL;
}

static int
dma_dev_data_prepare(void)
{
	size_t size;

	if (rte_dma_devices != NULL)
		return 0;

	size = dma_devices_max * sizeof(struct rte_dma_dev);
	rte_dma_devices = malloc(size);
	if (rte_dma_devices == NULL)
		return -ENOMEM;
	memset(rte_dma_devices, 0, size);

	return 0;
}

static int
dma_data_prepare(void)
{
	if (dma_devices_max == 0)
		dma_devices_max = RTE_DMADEV_DEFAULT_MAX;
	return dma_dev_data_prepare();
}

static struct rte_dma_dev *
dma_allocate(const char *name, int numa_node, size_t private_data_size)
{
	struct rte_dma_dev *dev;
	void *dev_private;
	int16_t dev_id;
	int ret;

	ret = dma_data_prepare();
	if (ret < 0) {
		RTE_DMA_LOG(ERR, "Cannot initialize dmadevs data");
		return NULL;
	}

	dev = dma_find_by_name(name);
	if (dev != NULL) {
		RTE_DMA_LOG(ERR, "DMA device already allocated");
		return NULL;
	}

	dev_private = rte_zmalloc_socket(name, private_data_size,
					 RTE_CACHE_LINE_SIZE, numa_node);
	if (dev_private == NULL) {
		RTE_DMA_LOG(ERR, "Cannot allocate private data");
		return NULL;
	}

	dev_id = dma_find_free_id();
	if (dev_id < 0) {
		RTE_DMA_LOG(ERR, "Reached maximum number of DMA devices");
		rte_free(dev_private);
		return NULL;
	}

	dev = &rte_dma_devices[dev_id];
	rte_strscpy(dev->dev_name, name, sizeof(dev->dev_name));
	dev->dev_id = dev_id;
	dev->numa_node = numa_node;
	dev->dev_private = dev_private;

	return dev;
}

static void
dma_release(struct rte_dma_dev *dev)
{
	rte_free(dev->dev_private);
	memset(dev, 0, sizeof(struct rte_dma_dev));
}

struct rte_dma_dev *
rte_dma_pmd_allocate(const char *name, int numa_node, size_t private_data_size)
{
	struct rte_dma_dev *dev;

	if (dma_check_name(name) != 0 || private_data_size == 0)
		return NULL;

	dev = dma_allocate(name, numa_node, private_data_size);
	if (dev == NULL)
		return NULL;

	dev->state = RTE_DMA_DEV_REGISTERED;

	return dev;
}

int
rte_dma_pmd_release(const char *name)
{
	struct rte_dma_dev *dev;

	if (dma_check_name(name) != 0)
		return -EINVAL;

	dev = dma_find_by_name(name);
	if (dev == NULL)
		return -EINVAL;

	dma_release(dev);
	return 0;
}

int
rte_dma_get_dev_id_by_name(const char *name)
{
	struct rte_dma_dev *dev;

	if (dma_check_name(name) != 0)
		return -EINVAL;

	dev = dma_find_by_name(name);
	if (dev == NULL)
		return -EINVAL;

	return dev->dev_id;
}

bool
rte_dma_is_valid(int16_t dev_id)
{
	return (dev_id >= 0) && (dev_id < dma_devices_max) &&
		rte_dma_devices != NULL &&
		rte_dma_devices[dev_id].state != RTE_DMA_DEV_UNUSED;
}

uint16_t
rte_dma_count_avail(void)
{
	uint16_t count = 0;
	uint16_t i;

	if (rte_dma_devices == NULL)
		return count;

	for (i = 0; i < dma_devices_max; i++) {
		if (rte_dma_devices[i].state != RTE_DMA_DEV_UNUSED)
			count++;
	}

	return count;
}
