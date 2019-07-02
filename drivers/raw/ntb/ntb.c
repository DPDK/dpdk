/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_memzone.h>
#include <rte_memcpy.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "ntb_hw_intel.h"
#include "ntb.h"

int ntb_logtype;

static const struct rte_pci_id pci_id_ntb_map[] = {
	{ RTE_PCI_DEVICE(NTB_INTEL_VENDOR_ID, NTB_INTEL_DEV_ID_B2B_SKX) },
	{ .vendor_id = 0, /* sentinel */ },
};

static void
ntb_queue_conf_get(struct rte_rawdev *dev __rte_unused,
		   uint16_t queue_id __rte_unused,
		   rte_rawdev_obj_t queue_conf __rte_unused)
{
}

static int
ntb_queue_setup(struct rte_rawdev *dev __rte_unused,
		uint16_t queue_id __rte_unused,
		rte_rawdev_obj_t queue_conf __rte_unused)
{
	return 0;
}

static int
ntb_queue_release(struct rte_rawdev *dev __rte_unused,
		  uint16_t queue_id __rte_unused)
{
	return 0;
}

static uint16_t
ntb_queue_count(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	return hw->queue_pairs;
}

static int
ntb_enqueue_bufs(struct rte_rawdev *dev,
		 struct rte_rawdev_buf **buffers,
		 unsigned int count,
		 rte_rawdev_obj_t context)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(buffers);
	RTE_SET_USED(count);
	RTE_SET_USED(context);

	return 0;
}

static int
ntb_dequeue_bufs(struct rte_rawdev *dev,
		 struct rte_rawdev_buf **buffers,
		 unsigned int count,
		 rte_rawdev_obj_t context)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(buffers);
	RTE_SET_USED(count);
	RTE_SET_USED(context);

	return 0;
}

static void
ntb_dev_info_get(struct rte_rawdev *dev, rte_rawdev_obj_t dev_info)
{
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_attr *ntb_attrs = dev_info;

	strncpy(ntb_attrs[NTB_TOPO_ID].name, NTB_TOPO_NAME, NTB_ATTR_NAME_LEN);
	switch (hw->topo) {
	case NTB_TOPO_B2B_DSD:
		strncpy(ntb_attrs[NTB_TOPO_ID].value, "B2B DSD",
			NTB_ATTR_VAL_LEN);
		break;
	case NTB_TOPO_B2B_USD:
		strncpy(ntb_attrs[NTB_TOPO_ID].value, "B2B USD",
			NTB_ATTR_VAL_LEN);
		break;
	default:
		strncpy(ntb_attrs[NTB_TOPO_ID].value, "Unsupported",
			NTB_ATTR_VAL_LEN);
	}

	strncpy(ntb_attrs[NTB_LINK_STATUS_ID].name, NTB_LINK_STATUS_NAME,
		NTB_ATTR_NAME_LEN);
	snprintf(ntb_attrs[NTB_LINK_STATUS_ID].value, NTB_ATTR_VAL_LEN,
		 "%d", hw->link_status);

	strncpy(ntb_attrs[NTB_SPEED_ID].name, NTB_SPEED_NAME,
		NTB_ATTR_NAME_LEN);
	snprintf(ntb_attrs[NTB_SPEED_ID].value, NTB_ATTR_VAL_LEN,
		 "%d", hw->link_speed);

	strncpy(ntb_attrs[NTB_WIDTH_ID].name, NTB_WIDTH_NAME,
		NTB_ATTR_NAME_LEN);
	snprintf(ntb_attrs[NTB_WIDTH_ID].value, NTB_ATTR_VAL_LEN,
		 "%d", hw->link_width);

	strncpy(ntb_attrs[NTB_MW_CNT_ID].name, NTB_MW_CNT_NAME,
		NTB_ATTR_NAME_LEN);
	snprintf(ntb_attrs[NTB_MW_CNT_ID].value, NTB_ATTR_VAL_LEN,
		 "%d", hw->mw_cnt);

	strncpy(ntb_attrs[NTB_DB_CNT_ID].name, NTB_DB_CNT_NAME,
		NTB_ATTR_NAME_LEN);
	snprintf(ntb_attrs[NTB_DB_CNT_ID].value, NTB_ATTR_VAL_LEN,
		 "%d", hw->db_cnt);

	strncpy(ntb_attrs[NTB_SPAD_CNT_ID].name, NTB_SPAD_CNT_NAME,
		NTB_ATTR_NAME_LEN);
	snprintf(ntb_attrs[NTB_SPAD_CNT_ID].value, NTB_ATTR_VAL_LEN,
		 "%d", hw->spad_cnt);
}

static int
ntb_dev_configure(const struct rte_rawdev *dev __rte_unused,
		  rte_rawdev_obj_t config __rte_unused)
{
	return 0;
}

static int
ntb_dev_start(struct rte_rawdev *dev)
{
	/* TODO: init queues and start queues. */
	dev->started = 1;

	return 0;
}

static void
ntb_dev_stop(struct rte_rawdev *dev)
{
	/* TODO: stop rx/tx queues. */
	dev->started = 0;
}

static int
ntb_dev_close(struct rte_rawdev *dev)
{
	int ret = 0;

	if (dev->started)
		ntb_dev_stop(dev);

	/* TODO: free queues. */

	return ret;
}

static int
ntb_dev_reset(struct rte_rawdev *rawdev __rte_unused)
{
	return 0;
}

static int
ntb_attr_set(struct rte_rawdev *dev, const char *attr_name,
				 uint64_t attr_value)
{
	struct ntb_hw *hw = dev->dev_private;
	int index = 0;

	if (dev == NULL || attr_name == NULL) {
		NTB_LOG(ERR, "Invalid arguments for setting attributes");
		return -EINVAL;
	}

	if (!strncmp(attr_name, NTB_SPAD_USER, NTB_SPAD_USER_LEN)) {
		if (hw->ntb_ops->spad_write == NULL)
			return -ENOTSUP;
		index = atoi(&attr_name[NTB_SPAD_USER_LEN]);
		(*hw->ntb_ops->spad_write)(dev, hw->spad_user_list[index],
					   1, attr_value);
		NTB_LOG(INFO, "Set attribute (%s) Value (%" PRIu64 ")",
			attr_name, attr_value);
		return 0;
	}

	/* Attribute not found. */
	NTB_LOG(ERR, "Attribute not found.");
	return -EINVAL;
}

static int
ntb_attr_get(struct rte_rawdev *dev, const char *attr_name,
				 uint64_t *attr_value)
{
	struct ntb_hw *hw = dev->dev_private;
	int index = 0;

	if (dev == NULL || attr_name == NULL || attr_value == NULL) {
		NTB_LOG(ERR, "Invalid arguments for getting attributes");
		return -EINVAL;
	}

	if (!strncmp(attr_name, NTB_TOPO_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->topo;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_LINK_STATUS_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->link_status;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_SPEED_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->link_speed;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_WIDTH_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->link_width;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_MW_CNT_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->mw_cnt;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_DB_CNT_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->db_cnt;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_SPAD_CNT_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->spad_cnt;
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_SPAD_USER, NTB_SPAD_USER_LEN)) {
		if (hw->ntb_ops->spad_read == NULL)
			return -ENOTSUP;
		index = atoi(&attr_name[NTB_SPAD_USER_LEN]);
		*attr_value = (*hw->ntb_ops->spad_read)(dev,
				hw->spad_user_list[index], 0);
		NTB_LOG(INFO, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	/* Attribute not found. */
	NTB_LOG(ERR, "Attribute not found.");
	return -EINVAL;
}

static int
ntb_xstats_get(const struct rte_rawdev *dev __rte_unused,
	       const unsigned int ids[] __rte_unused,
	       uint64_t values[] __rte_unused,
	       unsigned int n __rte_unused)
{
	return 0;
}

static int
ntb_xstats_get_names(const struct rte_rawdev *dev __rte_unused,
		     struct rte_rawdev_xstats_name *xstats_names __rte_unused,
		     unsigned int size __rte_unused)
{
	return 0;
}

static uint64_t
ntb_xstats_get_by_name(const struct rte_rawdev *dev __rte_unused,
		       const char *name __rte_unused,
		       unsigned int *id __rte_unused)
{
	return 0;
}

static int
ntb_xstats_reset(struct rte_rawdev *dev __rte_unused,
		 const uint32_t ids[] __rte_unused,
		 uint32_t nb_ids __rte_unused)
{
	return 0;
}

static const struct rte_rawdev_ops ntb_ops = {
	.dev_info_get         = ntb_dev_info_get,
	.dev_configure        = ntb_dev_configure,
	.dev_start            = ntb_dev_start,
	.dev_stop             = ntb_dev_stop,
	.dev_close            = ntb_dev_close,
	.dev_reset            = ntb_dev_reset,

	.queue_def_conf       = ntb_queue_conf_get,
	.queue_setup          = ntb_queue_setup,
	.queue_release        = ntb_queue_release,
	.queue_count          = ntb_queue_count,

	.enqueue_bufs         = ntb_enqueue_bufs,
	.dequeue_bufs         = ntb_dequeue_bufs,

	.attr_get             = ntb_attr_get,
	.attr_set             = ntb_attr_set,

	.xstats_get           = ntb_xstats_get,
	.xstats_get_names     = ntb_xstats_get_names,
	.xstats_get_by_name   = ntb_xstats_get_by_name,
	.xstats_reset         = ntb_xstats_reset,
};

static int
ntb_init_hw(struct rte_rawdev *dev, struct rte_pci_device *pci_dev)
{
	struct ntb_hw *hw = dev->dev_private;
	int ret;

	hw->pci_dev = pci_dev;
	hw->peer_dev_up = 0;
	hw->link_status = NTB_LINK_DOWN;
	hw->link_speed = NTB_SPEED_NONE;
	hw->link_width = NTB_WIDTH_NONE;

	switch (pci_dev->id.device_id) {
	case NTB_INTEL_DEV_ID_B2B_SKX:
		hw->ntb_ops = &intel_ntb_ops;
		break;
	default:
		NTB_LOG(ERR, "Not supported device.");
		return -EINVAL;
	}

	if (hw->ntb_ops->ntb_dev_init == NULL)
		return -ENOTSUP;
	ret = (*hw->ntb_ops->ntb_dev_init)(dev);
	if (ret) {
		NTB_LOG(ERR, "Unable to init ntb dev.");
		return ret;
	}

	if (hw->ntb_ops->set_link == NULL)
		return -ENOTSUP;
	ret = (*hw->ntb_ops->set_link)(dev, 1);
	if (ret)
		return ret;

	return ret;
}

static int
ntb_create(struct rte_pci_device *pci_dev, int socket_id)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev = NULL;
	int ret;

	if (pci_dev == NULL) {
		NTB_LOG(ERR, "Invalid pci_dev.");
		ret = -EINVAL;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "NTB:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	NTB_LOG(INFO, "Init %s on NUMA node %d", name, socket_id);

	/* Allocate device structure. */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct ntb_hw),
					 socket_id);
	if (rawdev == NULL) {
		NTB_LOG(ERR, "Unable to allocate rawdev.");
		ret = -EINVAL;
	}

	rawdev->dev_ops = &ntb_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	ret = ntb_init_hw(rawdev, pci_dev);
	if (ret < 0) {
		NTB_LOG(ERR, "Unable to init ntb hw.");
		goto fail;
	}

	return ret;

fail:
	if (rawdev)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
ntb_destroy(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	int ret;

	if (pci_dev == NULL) {
		NTB_LOG(ERR, "Invalid pci_dev.");
		ret = -EINVAL;
		return ret;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "NTB:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	NTB_LOG(INFO, "Closing %s on NUMA node %d", name, rte_socket_id());

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		NTB_LOG(ERR, "Invalid device name (%s)", name);
		ret = -EINVAL;
		return ret;
	}

	ret = rte_rawdev_pmd_release(rawdev);
	if (ret)
		NTB_LOG(ERR, "Failed to destroy ntb rawdev.");

	return ret;
}

static int
ntb_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return ntb_create(pci_dev, rte_socket_id());
}

static int
ntb_remove(struct rte_pci_device *pci_dev)
{
	return ntb_destroy(pci_dev);
}


static struct rte_pci_driver rte_ntb_pmd = {
	.id_table = pci_id_ntb_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = ntb_probe,
	.remove = ntb_remove,
};

RTE_PMD_REGISTER_PCI(raw_ntb, rte_ntb_pmd);
RTE_PMD_REGISTER_PCI_TABLE(raw_ntb, pci_id_ntb_map);
RTE_PMD_REGISTER_KMOD_DEP(raw_ntb, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_INIT(ntb_init_log)
{
	ntb_logtype = rte_log_register("pmd.raw.ntb");
	if (ntb_logtype >= 0)
		rte_log_set_level(ntb_logtype, RTE_LOG_DEBUG);
}
