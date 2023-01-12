/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 * Copyright(c) 2021 Intel Corporation
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>
#include <rte_telemetry.h>

#include "rte_dmadev.h"
#include "rte_dmadev_pmd.h"

static int16_t dma_devices_max;

struct rte_dma_fp_object *rte_dma_fp_objs;
static struct rte_dma_dev *rte_dma_devices;
static struct {
	/* Hold the dev_max information of the primary process. This field is
	 * set by the primary process and is read by the secondary process.
	 */
	int16_t dev_max;
	struct rte_dma_dev_data data[0];
} *dma_devices_shared_data;

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

int16_t
rte_dma_next_dev(int16_t start_dev_id)
{
	int16_t dev_id = start_dev_id;
	while (dev_id < dma_devices_max && rte_dma_devices[dev_id].state == RTE_DMA_DEV_UNUSED)
		dev_id++;

	if (dev_id < dma_devices_max)
		return dev_id;

	return -1;
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

	if (rte_dma_devices == NULL || dma_devices_shared_data == NULL)
		return -1;

	for (i = 0; i < dma_devices_max; i++) {
		if (dma_devices_shared_data->data[i].dev_name[0] == '\0')
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
		    (!strcmp(name, rte_dma_devices[i].data->dev_name)))
			return &rte_dma_devices[i];
	}

	return NULL;
}

static void dma_fp_object_dummy(struct rte_dma_fp_object *obj);

static int
dma_fp_data_prepare(void)
{
	size_t size;
	void *ptr;
	int i;

	if (rte_dma_fp_objs != NULL)
		return 0;

	/* Fast-path object must align cacheline, but the return value of malloc
	 * may not be aligned to the cache line. Therefore, extra memory is
	 * applied for realignment.
	 * note: We do not call posix_memalign/aligned_alloc because it is
	 * version dependent on libc.
	 */
	size = dma_devices_max * sizeof(struct rte_dma_fp_object) +
		RTE_CACHE_LINE_SIZE;
	ptr = malloc(size);
	if (ptr == NULL)
		return -ENOMEM;
	memset(ptr, 0, size);

	rte_dma_fp_objs = RTE_PTR_ALIGN(ptr, RTE_CACHE_LINE_SIZE);
	for (i = 0; i < dma_devices_max; i++)
		dma_fp_object_dummy(&rte_dma_fp_objs[i]);

	return 0;
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
dma_shared_data_prepare(void)
{
	const char *mz_name = "rte_dma_dev_data";
	const struct rte_memzone *mz;
	size_t size;

	if (dma_devices_shared_data != NULL)
		return 0;

	size = sizeof(*dma_devices_shared_data) +
		sizeof(struct rte_dma_dev_data) * dma_devices_max;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		mz = rte_memzone_reserve(mz_name, size, rte_socket_id(), 0);
	else
		mz = rte_memzone_lookup(mz_name);
	if (mz == NULL)
		return -ENOMEM;

	dma_devices_shared_data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		memset(dma_devices_shared_data, 0, size);
		dma_devices_shared_data->dev_max = dma_devices_max;
	} else {
		dma_devices_max = dma_devices_shared_data->dev_max;
	}

	return 0;
}

static int
dma_data_prepare(void)
{
	int ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (dma_devices_max == 0)
			dma_devices_max = RTE_DMADEV_DEFAULT_MAX;
		ret = dma_fp_data_prepare();
		if (ret)
			return ret;
		ret = dma_dev_data_prepare();
		if (ret)
			return ret;
		ret = dma_shared_data_prepare();
		if (ret)
			return ret;
	} else {
		ret = dma_shared_data_prepare();
		if (ret)
			return ret;
		ret = dma_fp_data_prepare();
		if (ret)
			return ret;
		ret = dma_dev_data_prepare();
		if (ret)
			return ret;
	}

	return 0;
}

static struct rte_dma_dev *
dma_allocate_primary(const char *name, int numa_node, size_t private_data_size)
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
	dev->data = &dma_devices_shared_data->data[dev_id];
	rte_strscpy(dev->data->dev_name, name, sizeof(dev->data->dev_name));
	dev->data->dev_id = dev_id;
	dev->data->numa_node = numa_node;
	dev->data->dev_private = dev_private;

	return dev;
}

static struct rte_dma_dev *
dma_attach_secondary(const char *name)
{
	struct rte_dma_dev *dev;
	int16_t i;
	int ret;

	ret = dma_data_prepare();
	if (ret < 0) {
		RTE_DMA_LOG(ERR, "Cannot initialize dmadevs data");
		return NULL;
	}

	for (i = 0; i < dma_devices_max; i++) {
		if (!strcmp(dma_devices_shared_data->data[i].dev_name, name))
			break;
	}
	if (i == dma_devices_max) {
		RTE_DMA_LOG(ERR,
			"Device %s is not driven by the primary process",
			name);
		return NULL;
	}

	dev = &rte_dma_devices[i];
	dev->data = &dma_devices_shared_data->data[i];

	return dev;
}

static struct rte_dma_dev *
dma_allocate(const char *name, int numa_node, size_t private_data_size)
{
	struct rte_dma_dev *dev;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		dev = dma_allocate_primary(name, numa_node, private_data_size);
	else
		dev = dma_attach_secondary(name);

	if (dev) {
		dev->fp_obj = &rte_dma_fp_objs[dev->data->dev_id];
		dma_fp_object_dummy(dev->fp_obj);
	}

	return dev;
}

static void
dma_release(struct rte_dma_dev *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		rte_free(dev->data->dev_private);
		memset(dev->data, 0, sizeof(struct rte_dma_dev_data));
	}

	dma_fp_object_dummy(dev->fp_obj);
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
		return rte_dma_close(dev->data->dev_id);

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

	return dev->data->dev_id;
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

	if (*dev->dev_ops->dev_info_get == NULL)
		return -ENOTSUP;
	memset(dev_info, 0, sizeof(struct rte_dma_info));
	ret = (*dev->dev_ops->dev_info_get)(dev, dev_info,
					    sizeof(struct rte_dma_info));
	if (ret != 0)
		return ret;

	dev_info->dev_name = dev->data->dev_name;
	dev_info->numa_node = dev->device->numa_node;
	dev_info->nb_vchans = dev->data->dev_conf.nb_vchans;

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

	if (dev->data->dev_started != 0) {
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

	if (*dev->dev_ops->dev_configure == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->dev_configure)(dev, dev_conf,
					     sizeof(struct rte_dma_conf));
	if (ret == 0)
		memcpy(&dev->data->dev_conf, dev_conf,
		       sizeof(struct rte_dma_conf));

	return ret;
}

int
rte_dma_start(int16_t dev_id)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	int ret;

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	if (dev->data->dev_conf.nb_vchans == 0) {
		RTE_DMA_LOG(ERR, "Device %d must be configured first", dev_id);
		return -EINVAL;
	}

	if (dev->data->dev_started != 0) {
		RTE_DMA_LOG(WARNING, "Device %d already started", dev_id);
		return 0;
	}

	if (dev->dev_ops->dev_start == NULL)
		goto mark_started;

	ret = (*dev->dev_ops->dev_start)(dev);
	if (ret != 0)
		return ret;

mark_started:
	dev->data->dev_started = 1;
	return 0;
}

int
rte_dma_stop(int16_t dev_id)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];
	int ret;

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	if (dev->data->dev_started == 0) {
		RTE_DMA_LOG(WARNING, "Device %d already stopped", dev_id);
		return 0;
	}

	if (dev->dev_ops->dev_stop == NULL)
		goto mark_stopped;

	ret = (*dev->dev_ops->dev_stop)(dev);
	if (ret != 0)
		return ret;

mark_stopped:
	dev->data->dev_started = 0;
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
	if (dev->data->dev_started == 1) {
		RTE_DMA_LOG(ERR,
			"Device %d must be stopped before closing", dev_id);
		return -EBUSY;
	}

	if (*dev->dev_ops->dev_close == NULL)
		return -ENOTSUP;
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

	if (dev->data->dev_started != 0) {
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
	if (dev->data->dev_conf.nb_vchans == 0) {
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

	if (*dev->dev_ops->vchan_setup == NULL)
		return -ENOTSUP;
	return (*dev->dev_ops->vchan_setup)(dev, vchan, conf,
					sizeof(struct rte_dma_vchan_conf));
}

int
rte_dma_stats_get(int16_t dev_id, uint16_t vchan, struct rte_dma_stats *stats)
{
	const struct rte_dma_dev *dev = &rte_dma_devices[dev_id];

	if (!rte_dma_is_valid(dev_id) || stats == NULL)
		return -EINVAL;

	if (vchan >= dev->data->dev_conf.nb_vchans &&
	    vchan != RTE_DMA_ALL_VCHAN) {
		RTE_DMA_LOG(ERR,
			"Device %d vchan %u out of range", dev_id, vchan);
		return -EINVAL;
	}

	if (*dev->dev_ops->stats_get == NULL)
		return -ENOTSUP;
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

	if (vchan >= dev->data->dev_conf.nb_vchans &&
	    vchan != RTE_DMA_ALL_VCHAN) {
		RTE_DMA_LOG(ERR,
			"Device %d vchan %u out of range", dev_id, vchan);
		return -EINVAL;
	}

	if (*dev->dev_ops->stats_reset == NULL)
		return -ENOTSUP;
	return (*dev->dev_ops->stats_reset)(dev, vchan);
}

int
rte_dma_vchan_status(int16_t dev_id, uint16_t vchan, enum rte_dma_vchan_status *status)
{
	struct rte_dma_dev *dev = &rte_dma_devices[dev_id];

	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;

	if (vchan >= dev->data->dev_conf.nb_vchans) {
		RTE_DMA_LOG(ERR, "Device %u vchan %u out of range\n", dev_id, vchan);
		return -EINVAL;
	}

	if (*dev->dev_ops->vchan_status == NULL)
		return -ENOTSUP;
	return (*dev->dev_ops->vchan_status)(dev, vchan, status);
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
		{ RTE_DMA_CAPA_HANDLES_ERRORS, "handles_errors" },
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
		dev->data->dev_id,
		dev->data->dev_name,
		dev->data->dev_started ? "started" : "stopped");
	dma_dump_capability(f, dev_info.dev_capa);
	(void)fprintf(f, "  max_vchans_supported: %u\n", dev_info.max_vchans);
	(void)fprintf(f, "  nb_vchans_configured: %u\n", dev_info.nb_vchans);
	(void)fprintf(f, "  silent_mode: %s\n",
		dev->data->dev_conf.enable_silent ? "on" : "off");

	if (dev->dev_ops->dev_dump != NULL)
		return (*dev->dev_ops->dev_dump)(dev, f);

	return 0;
}

static int
dummy_copy(__rte_unused void *dev_private, __rte_unused uint16_t vchan,
	   __rte_unused rte_iova_t src, __rte_unused rte_iova_t dst,
	   __rte_unused uint32_t length, __rte_unused uint64_t flags)
{
	RTE_DMA_LOG(ERR, "copy is not configured or not supported.");
	return -EINVAL;
}

static int
dummy_copy_sg(__rte_unused void *dev_private, __rte_unused uint16_t vchan,
	      __rte_unused const struct rte_dma_sge *src,
	      __rte_unused const struct rte_dma_sge *dst,
	      __rte_unused uint16_t nb_src, __rte_unused uint16_t nb_dst,
	      __rte_unused uint64_t flags)
{
	RTE_DMA_LOG(ERR, "copy_sg is not configured or not supported.");
	return -EINVAL;
}

static int
dummy_fill(__rte_unused void *dev_private, __rte_unused uint16_t vchan,
	   __rte_unused uint64_t pattern, __rte_unused rte_iova_t dst,
	   __rte_unused uint32_t length, __rte_unused uint64_t flags)
{
	RTE_DMA_LOG(ERR, "fill is not configured or not supported.");
	return -EINVAL;
}

static int
dummy_submit(__rte_unused void *dev_private, __rte_unused uint16_t vchan)
{
	RTE_DMA_LOG(ERR, "submit is not configured or not supported.");
	return -EINVAL;
}

static uint16_t
dummy_completed(__rte_unused void *dev_private,	__rte_unused uint16_t vchan,
		__rte_unused const uint16_t nb_cpls,
		__rte_unused uint16_t *last_idx, __rte_unused bool *has_error)
{
	RTE_DMA_LOG(ERR, "completed is not configured or not supported.");
	return 0;
}

static uint16_t
dummy_completed_status(__rte_unused void *dev_private,
		       __rte_unused uint16_t vchan,
		       __rte_unused const uint16_t nb_cpls,
		       __rte_unused uint16_t *last_idx,
		       __rte_unused enum rte_dma_status_code *status)
{
	RTE_DMA_LOG(ERR,
		    "completed_status is not configured or not supported.");
	return 0;
}

static uint16_t
dummy_burst_capacity(__rte_unused const void *dev_private,
		     __rte_unused uint16_t vchan)
{
	RTE_DMA_LOG(ERR, "burst_capacity is not configured or not supported.");
	return 0;
}

static void
dma_fp_object_dummy(struct rte_dma_fp_object *obj)
{
	obj->dev_private      = NULL;
	obj->copy             = dummy_copy;
	obj->copy_sg          = dummy_copy_sg;
	obj->fill             = dummy_fill;
	obj->submit           = dummy_submit;
	obj->completed        = dummy_completed;
	obj->completed_status = dummy_completed_status;
	obj->burst_capacity   = dummy_burst_capacity;
}

static int
dmadev_handle_dev_list(const char *cmd __rte_unused,
		const char *params __rte_unused,
		struct rte_tel_data *d)
{
	int dev_id;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	for (dev_id = 0; dev_id < dma_devices_max; dev_id++)
		if (rte_dma_is_valid(dev_id))
			rte_tel_data_add_array_int(d, dev_id);

	return 0;
}

#define ADD_CAPA(td, dc, c) rte_tel_data_add_dict_int(td, dma_capability_name(c), !!(dc & c))

static int
dmadev_handle_dev_info(const char *cmd __rte_unused,
		const char *params, struct rte_tel_data *d)
{
	struct rte_dma_info dma_info;
	struct rte_tel_data *dma_caps;
	int dev_id, ret;
	uint64_t dev_capa;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		RTE_DMA_LOG(WARNING, "Extra parameters passed to dmadev telemetry command, ignoring");

	/* Function info_get validates dev_id so we don't need to. */
	ret = rte_dma_info_get(dev_id, &dma_info);
	if (ret < 0)
		return -EINVAL;
	dev_capa = dma_info.dev_capa;

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_string(d, "name", dma_info.dev_name);
	rte_tel_data_add_dict_int(d, "nb_vchans", dma_info.nb_vchans);
	rte_tel_data_add_dict_int(d, "numa_node", dma_info.numa_node);
	rte_tel_data_add_dict_int(d, "max_vchans", dma_info.max_vchans);
	rte_tel_data_add_dict_int(d, "max_desc", dma_info.max_desc);
	rte_tel_data_add_dict_int(d, "min_desc", dma_info.min_desc);
	rte_tel_data_add_dict_int(d, "max_sges", dma_info.max_sges);

	dma_caps = rte_tel_data_alloc();
	if (!dma_caps)
		return -ENOMEM;

	rte_tel_data_start_dict(dma_caps);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_MEM_TO_MEM);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_MEM_TO_DEV);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_DEV_TO_MEM);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_DEV_TO_DEV);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_SVA);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_SILENT);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_HANDLES_ERRORS);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_OPS_COPY);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_OPS_COPY_SG);
	ADD_CAPA(dma_caps, dev_capa, RTE_DMA_CAPA_OPS_FILL);
	rte_tel_data_add_dict_container(d, "capabilities", dma_caps, 0);

	return 0;
}

#define ADD_DICT_STAT(s) rte_tel_data_add_dict_uint(d, #s, dma_stats.s)

static int
dmadev_handle_dev_stats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_dma_info dma_info;
	struct rte_dma_stats dma_stats;
	int dev_id, ret, vchan_id;
	char *end_param;
	const char *vchan_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);

	/* Function info_get validates dev_id so we don't need to. */
	ret = rte_dma_info_get(dev_id, &dma_info);
	if (ret < 0)
		return -EINVAL;

	/* If the device has one vchan the user does not need to supply the
	 * vchan id and only the device id is needed, no extra parameters.
	 */
	if (dma_info.nb_vchans == 1 && *end_param == '\0')
		vchan_id = 0;
	else {
		vchan_param = strtok(end_param, ",");
		if (!vchan_param || strlen(vchan_param) == 0 || !isdigit(*vchan_param))
			return -EINVAL;

		vchan_id = strtoul(vchan_param, &end_param, 0);
	}
	if (*end_param != '\0')
		RTE_DMA_LOG(WARNING, "Extra parameters passed to dmadev telemetry command, ignoring");

	ret = rte_dma_stats_get(dev_id, vchan_id, &dma_stats);
	if (ret < 0)
		return -EINVAL;

	rte_tel_data_start_dict(d);
	ADD_DICT_STAT(submitted);
	ADD_DICT_STAT(completed);
	ADD_DICT_STAT(errors);

	return 0;
}

#ifndef RTE_EXEC_ENV_WINDOWS
static int
dmadev_handle_dev_dump(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	char *buf, *end_param;
	int dev_id, ret;
	FILE *f;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		RTE_DMA_LOG(WARNING, "Extra parameters passed to dmadev telemetry command, ignoring");

	buf = calloc(sizeof(char), RTE_TEL_MAX_SINGLE_STRING_LEN);
	if (buf == NULL)
		return -ENOMEM;

	f = fmemopen(buf, RTE_TEL_MAX_SINGLE_STRING_LEN - 1, "w+");
	if (f == NULL) {
		free(buf);
		return -EINVAL;
	}

	ret = rte_dma_dump(dev_id, f);
	fclose(f);
	if (ret == 0) {
		rte_tel_data_start_dict(d);
		rte_tel_data_string(d, buf);
	}

	free(buf);
	return ret;
}
#endif /* !RTE_EXEC_ENV_WINDOWS */

RTE_INIT(dmadev_init_telemetry)
{
	rte_telemetry_register_cmd("/dmadev/list", dmadev_handle_dev_list,
			"Returns list of available dmadev devices by IDs. No parameters.");
	rte_telemetry_register_cmd("/dmadev/info", dmadev_handle_dev_info,
			"Returns information for a dmadev. Parameters: int dev_id");
	rte_telemetry_register_cmd("/dmadev/stats", dmadev_handle_dev_stats,
			"Returns the stats for a dmadev vchannel. Parameters: int dev_id, vchan_id (Optional if only one vchannel)");
#ifndef RTE_EXEC_ENV_WINDOWS
	rte_telemetry_register_cmd("/dmadev/dump", dmadev_handle_dev_dump,
			"Returns dump information for a dmadev. Parameters: int dev_id");
#endif
}
