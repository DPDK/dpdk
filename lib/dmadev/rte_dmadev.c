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

	if (dev->state == RTE_DMA_DEV_READY)
		return rte_dma_close(dev->dev_id);

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

int
rte_dma_info_get(int16_t dev_id, struct rte_dma_info *dev_info)
{
	const struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	int ret;

	if (!rte_dma_is_valid(dev_id) || dev_info == NULL)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_info_get, -ENOTSUP);
	memset(dev_info, 0, sizeof(struct rte_dma_info));
	ret = (*dev->dev_ops->dev_info_get)(dev, dev_info,
					    sizeof(struct rte_dma_info));
	if (ret != 0)
		return ret;

	dev_info->dev_name = dev->dev_name;
	dev_info->numa_node = dev->device->numa_node;
	dev_info->nb_vchans = dev->dev_conf.nb_vchans;

	return 0;
}

int
rte_dma_configure(int16_t dev_id, const struct rte_dma_conf *dev_conf)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	struct rte_dma_info dev_info;
	int ret;

	if (!rte_dma_is_valid(dev_id) || dev_conf == NULL)
		return -EINVAL;

	if (dev->dev_started != 0) {
		RTE_DMA_LOG(ERR,
			"Device %d must be stopped to allow configuration",
			dev_id);
		return -EBUSY;
	}

	ret = rte_dma_info_get(dev_id, &dev_info);
	if (ret != 0) {
		RTE_DMA_LOG(ERR, "Device %d get device info fail", dev_id);
		return -EINVAL;
	}
	if (dev_conf->nb_vchans == 0) {
		RTE_DMA_LOG(ERR,
			"Device %d configure zero vchans", dev_id);
		return -EINVAL;
	}
	if (dev_conf->nb_vchans > dev_info.max_vchans) {
		RTE_DMA_LOG(ERR,
			"Device %d configure too many vchans", dev_id);
		return -EINVAL;
	}
	if (dev_conf->enable_silent &&
	    !(dev_info.dev_capa & RTE_DMA_CAPA_SILENT)) {
		RTE_DMA_LOG(ERR, "Device %d don't support silent", dev_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);
	ret = (*dev->dev_ops->dev_configure)(dev, dev_conf,
					     sizeof(struct rte_dma_conf));
	if (ret == 0)
		memcpy(&dev->dev_conf, dev_conf, sizeof(struct rte_dma_conf));

	return ret;
}

int
rte_dma_start(int16_t dev_id)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	int ret;

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	if (dev->dev_conf.nb_vchans == 0) {
		RTE_DMA_LOG(ERR, "Device %d must be configured first", dev_id);
		return -EINVAL;
	}

	if (dev->dev_started != 0) {
		RTE_DMA_LOG(WARNING, "Device %d already started", dev_id);
		return 0;
	}

	if (dev->dev_ops->dev_start == NULL)
		goto mark_started;

	ret = (*dev->dev_ops->dev_start)(dev);
	if (ret != 0)
		return ret;

mark_started:
	dev->dev_started = 1;
	return 0;
}

int
rte_dma_stop(int16_t dev_id)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	int ret;

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	if (dev->dev_started == 0) {
		RTE_DMA_LOG(WARNING, "Device %d already stopped", dev_id);
		return 0;
	}

	if (dev->dev_ops->dev_stop == NULL)
		goto mark_stopped;

	ret = (*dev->dev_ops->dev_stop)(dev);
	if (ret != 0)
		return ret;

mark_stopped:
	dev->dev_started = 0;
	return 0;
}

int
rte_dma_close(int16_t dev_id)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	int ret;

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	/* Device must be stopped before it can be closed */
	if (dev->dev_started == 1) {
		RTE_DMA_LOG(ERR,
			"Device %d must be stopped before closing", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);
	ret = (*dev->dev_ops->dev_close)(dev);
	if (ret == 0)
		dma_release(dev);

	return ret;
}

int
rte_dma_vchan_setup(int16_t dev_id, uint16_t vchan,
		    const struct rte_dma_vchan_conf *conf)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	struct rte_dma_info dev_info;
	bool src_is_dev, dst_is_dev;
	int ret;

	if (!rte_dma_is_valid(dev_id) || conf == NULL)
		return -EINVAL;

	if (dev->dev_started != 0) {
		RTE_DMA_LOG(ERR,
			"Device %d must be stopped to allow configuration",
			dev_id);
		return -EBUSY;
	}

	ret = rte_dma_info_get(dev_id, &dev_info);
	if (ret != 0) {
		RTE_DMA_LOG(ERR, "Device %d get device info fail", dev_id);
		return -EINVAL;
	}
	if (dev->dev_conf.nb_vchans == 0) {
		RTE_DMA_LOG(ERR, "Device %d must be configured first", dev_id);
		return -EINVAL;
	}
	if (vchan >= dev_info.nb_vchans) {
		RTE_DMA_LOG(ERR, "Device %d vchan out range!", dev_id);
		return -EINVAL;
	}
	if (conf->direction != RTE_DMA_DIR_MEM_TO_MEM &&
	    conf->direction != RTE_DMA_DIR_MEM_TO_DEV &&
	    conf->direction != RTE_DMA_DIR_DEV_TO_MEM &&
	    conf->direction != RTE_DMA_DIR_DEV_TO_DEV) {
		RTE_DMA_LOG(ERR, "Device %d direction invalid!", dev_id);
		return -EINVAL;
	}
	if (conf->direction == RTE_DMA_DIR_MEM_TO_MEM &&
	    !(dev_info.dev_capa & RTE_DMA_CAPA_MEM_TO_MEM)) {
		RTE_DMA_LOG(ERR,
			"Device %d don't support mem2mem transfer", dev_id);
		return -EINVAL;
	}
	if (conf->direction == RTE_DMA_DIR_MEM_TO_DEV &&
	    !(dev_info.dev_capa & RTE_DMA_CAPA_MEM_TO_DEV)) {
		RTE_DMA_LOG(ERR,
			"Device %d don't support mem2dev transfer", dev_id);
		return -EINVAL;
	}
	if (conf->direction == RTE_DMA_DIR_DEV_TO_MEM &&
	    !(dev_info.dev_capa & RTE_DMA_CAPA_DEV_TO_MEM)) {
		RTE_DMA_LOG(ERR,
			"Device %d don't support dev2mem transfer", dev_id);
		return -EINVAL;
	}
	if (conf->direction == RTE_DMA_DIR_DEV_TO_DEV &&
	    !(dev_info.dev_capa & RTE_DMA_CAPA_DEV_TO_DEV)) {
		RTE_DMA_LOG(ERR,
			"Device %d don't support dev2dev transfer", dev_id);
		return -EINVAL;
	}
	if (conf->nb_desc < dev_info.min_desc ||
	    conf->nb_desc > dev_info.max_desc) {
		RTE_DMA_LOG(ERR,
			"Device %d number of descriptors invalid", dev_id);
		return -EINVAL;
	}
	src_is_dev = conf->direction == RTE_DMA_DIR_DEV_TO_MEM ||
		     conf->direction == RTE_DMA_DIR_DEV_TO_DEV;
	if ((conf->src_port.port_type == RTE_DMA_PORT_NONE && src_is_dev) ||
	    (conf->src_port.port_type != RTE_DMA_PORT_NONE && !src_is_dev)) {
		RTE_DMA_LOG(ERR, "Device %d source port type invalid", dev_id);
		return -EINVAL;
	}
	dst_is_dev = conf->direction == RTE_DMA_DIR_MEM_TO_DEV ||
		     conf->direction == RTE_DMA_DIR_DEV_TO_DEV;
	if ((conf->dst_port.port_type == RTE_DMA_PORT_NONE && dst_is_dev) ||
	    (conf->dst_port.port_type != RTE_DMA_PORT_NONE && !dst_is_dev)) {
		RTE_DMA_LOG(ERR,
			"Device %d destination port type invalid", dev_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vchan_setup, -ENOTSUP);
	return (*dev->dev_ops->vchan_setup)(dev, vchan, conf,
					sizeof(struct rte_dma_vchan_conf));
}

int
rte_dma_stats_get(int16_t dev_id, uint16_t vchan, struct rte_dma_stats *stats)
{
	const struct rte_dma_dev *dev = &rte_dma_devices[dev_id];

	if (!rte_dma_is_valid(dev_id) || stats == NULL)
		return -EINVAL;

	if (vchan >= dev->dev_conf.nb_vchans &&
	    vchan != RTE_DMA_ALL_VCHAN) {
		RTE_DMA_LOG(ERR,
			"Device %d vchan %u out of range", dev_id, vchan);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	memset(stats, 0, sizeof(struct rte_dma_stats));
	return (*dev->dev_ops->stats_get)(dev, vchan, stats,
					  sizeof(struct rte_dma_stats));
}

int
rte_dma_stats_reset(int16_t dev_id, uint16_t vchan)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	if (vchan >= dev->dev_conf.nb_vchans &&
	    vchan != RTE_DMA_ALL_VCHAN) {
		RTE_DMA_LOG(ERR,
			"Device %d vchan %u out of range", dev_id, vchan);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_reset, -ENOTSUP);
	return (*dev->dev_ops->stats_reset)(dev, vchan);
}

static const char *
dma_capability_name(uint64_t capability)
{
	static const struct {
		uint64_t capability;
		const char *name;
	} capa_names[] = {
		{ RTE_DMA_CAPA_MEM_TO_MEM,  "mem2mem" },
		{ RTE_DMA_CAPA_MEM_TO_DEV,  "mem2dev" },
		{ RTE_DMA_CAPA_DEV_TO_MEM,  "dev2mem" },
		{ RTE_DMA_CAPA_DEV_TO_DEV,  "dev2dev" },
		{ RTE_DMA_CAPA_SVA,         "sva"     },
		{ RTE_DMA_CAPA_SILENT,      "silent"  },
		{ RTE_DMA_CAPA_OPS_COPY,    "copy"    },
		{ RTE_DMA_CAPA_OPS_COPY_SG, "copy_sg" },
		{ RTE_DMA_CAPA_OPS_FILL,    "fill"    },
	};

	const char *name = "unknown";
	uint32_t i;

	for (i = 0; i < RTE_DIM(capa_names); i++) {
		if (capability == capa_names[i].capability) {
			name = capa_names[i].name;
			break;
		}
	}

	return name;
}

static void
dma_dump_capability(FILE *f, uint64_t dev_capa)
{
	uint64_t capa;

	(void)fprintf(f, "  dev_capa: 0x%" PRIx64 " -", dev_capa);
	while (dev_capa > 0) {
		capa = 1ull << __builtin_ctzll(dev_capa);
		(void)fprintf(f, " %s", dma_capability_name(capa));
		dev_capa &= ~capa;
	}
	(void)fprintf(f, "\n");
}

int
rte_dma_dump(int16_t dev_id, FILE *f)
{
	const struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	struct rte_dma_info dev_info;
	int ret;

	if (!rte_dma_is_valid(dev_id) || f == NULL)
		return -EINVAL;

	ret = rte_dma_info_get(dev_id, &dev_info);
	if (ret != 0) {
		RTE_DMA_LOG(ERR, "Device %d get device info fail", dev_id);
		return -EINVAL;
	}

	(void)fprintf(f, "DMA Dev %d, '%s' [%s]\n",
		dev->dev_id,
		dev->dev_name,
		dev->dev_started ? "started" : "stopped");
	dma_dump_capability(f, dev_info.dev_capa);
	(void)fprintf(f, "  max_vchans_supported: %u\n", dev_info.max_vchans);
	(void)fprintf(f, "  nb_vchans_configured: %u\n", dev_info.nb_vchans);
	(void)fprintf(f, "  silent_mode: %s\n",
		dev->dev_conf.enable_silent ? "on" : "off");

	if (dev->dev_ops->dev_dump != NULL)
		return (*dev->dev_ops->dev_dump)(dev, f);

	return 0;
}
