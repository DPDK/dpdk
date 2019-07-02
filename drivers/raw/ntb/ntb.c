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

static int
ntb_set_mw(struct rte_rawdev *dev, int mw_idx, uint64_t mw_size)
{
	struct ntb_hw *hw = dev->dev_private;
	char mw_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int ret = 0;

	if (hw->ntb_ops->mw_set_trans == NULL) {
		NTB_LOG(ERR, "Not supported to set mw.");
		return -ENOTSUP;
	}

	snprintf(mw_name, sizeof(mw_name), "ntb_%d_mw_%d",
		 dev->dev_id, mw_idx);

	mz = rte_memzone_lookup(mw_name);
	if (mz)
		return 0;

	/**
	 * Hardware requires that mapped memory base address should be
	 * aligned with EMBARSZ and needs continuous memzone.
	 */
	mz = rte_memzone_reserve_aligned(mw_name, mw_size, dev->socket_id,
				RTE_MEMZONE_IOVA_CONTIG, hw->mw_size[mw_idx]);
	if (!mz) {
		NTB_LOG(ERR, "Cannot allocate aligned memzone.");
		return -EIO;
	}
	hw->mz[mw_idx] = mz;

	ret = (*hw->ntb_ops->mw_set_trans)(dev, mw_idx, mz->iova, mw_size);
	if (ret) {
		NTB_LOG(ERR, "Cannot set mw translation.");
		return ret;
	}

	return ret;
}

static void
ntb_link_cleanup(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	int status, i;

	if (hw->ntb_ops->spad_write == NULL ||
	    hw->ntb_ops->mw_set_trans == NULL) {
		NTB_LOG(ERR, "Not supported to clean up link.");
		return;
	}

	/* Clean spad registers. */
	for (i = 0; i < hw->spad_cnt; i++) {
		status = (*hw->ntb_ops->spad_write)(dev, i, 0, 0);
		if (status)
			NTB_LOG(ERR, "Failed to clean local spad.");
	}

	/* Clear mw so that peer cannot access local memory.*/
	for (i = 0; i < hw->mw_cnt; i++) {
		status = (*hw->ntb_ops->mw_set_trans)(dev, i, 0, 0);
		if (status)
			NTB_LOG(ERR, "Failed to clean mw.");
	}
}

static void
ntb_dev_intr_handler(void *param)
{
	struct rte_rawdev *dev = (struct rte_rawdev *)param;
	struct ntb_hw *hw = dev->dev_private;
	uint32_t mw_size_h, mw_size_l;
	uint64_t db_bits = 0;
	int i = 0;

	if (hw->ntb_ops->db_read == NULL ||
	    hw->ntb_ops->db_clear == NULL ||
	    hw->ntb_ops->peer_db_set == NULL) {
		NTB_LOG(ERR, "Doorbell is not supported.");
		return;
	}

	db_bits = (*hw->ntb_ops->db_read)(dev);
	if (!db_bits)
		NTB_LOG(ERR, "No doorbells");

	/* Doorbell 0 is for peer device ready. */
	if (db_bits & 1) {
		NTB_LOG(DEBUG, "DB0: Peer device is up.");
		/* Clear received doorbell. */
		(*hw->ntb_ops->db_clear)(dev, 1);

		/**
		 * Peer dev is already up. All mw settings are already done.
		 * Skip them.
		 */
		if (hw->peer_dev_up)
			return;

		if (hw->ntb_ops->spad_read == NULL ||
		    hw->ntb_ops->spad_write == NULL) {
			NTB_LOG(ERR, "Scratchpad is not supported.");
			return;
		}

		hw->peer_mw_cnt = (*hw->ntb_ops->spad_read)
				  (dev, SPAD_NUM_MWS, 0);
		hw->peer_mw_size = rte_zmalloc("uint64_t",
				   hw->peer_mw_cnt * sizeof(uint64_t), 0);
		for (i = 0; i < hw->mw_cnt; i++) {
			mw_size_h = (*hw->ntb_ops->spad_read)
				    (dev, SPAD_MW0_SZ_H + 2 * i, 0);
			mw_size_l = (*hw->ntb_ops->spad_read)
				    (dev, SPAD_MW0_SZ_L + 2 * i, 0);
			hw->peer_mw_size[i] = ((uint64_t)mw_size_h << 32) |
					      mw_size_l;
			NTB_LOG(DEBUG, "Peer %u mw size: 0x%"PRIx64"", i,
					hw->peer_mw_size[i]);
		}

		hw->peer_dev_up = 1;

		/**
		 * Handshake with peer. Spad_write only works when both
		 * devices are up. So write spad again when db is received.
		 * And set db again for the later device who may miss
		 * the 1st db.
		 */
		for (i = 0; i < hw->mw_cnt; i++) {
			(*hw->ntb_ops->spad_write)(dev, SPAD_NUM_MWS,
						   1, hw->mw_cnt);
			mw_size_h = hw->mw_size[i] >> 32;
			(*hw->ntb_ops->spad_write)(dev, SPAD_MW0_SZ_H + 2 * i,
						   1, mw_size_h);

			mw_size_l = hw->mw_size[i];
			(*hw->ntb_ops->spad_write)(dev, SPAD_MW0_SZ_L + 2 * i,
						   1, mw_size_l);
		}
		(*hw->ntb_ops->peer_db_set)(dev, 0);

		/* To get the link info. */
		if (hw->ntb_ops->get_link_status == NULL) {
			NTB_LOG(ERR, "Not supported to get link status.");
			return;
		}
		(*hw->ntb_ops->get_link_status)(dev);
		NTB_LOG(INFO, "Link is up. Link speed: %u. Link width: %u",
			hw->link_speed, hw->link_width);
		return;
	}

	if (db_bits & (1 << 1)) {
		NTB_LOG(DEBUG, "DB1: Peer device is down.");
		/* Clear received doorbell. */
		(*hw->ntb_ops->db_clear)(dev, 2);

		/* Peer device will be down, So clean local side too. */
		ntb_link_cleanup(dev);

		hw->peer_dev_up = 0;
		/* Response peer's dev_stop request. */
		(*hw->ntb_ops->peer_db_set)(dev, 2);
		return;
	}

	if (db_bits & (1 << 2)) {
		NTB_LOG(DEBUG, "DB2: Peer device agrees dev to be down.");
		/* Clear received doorbell. */
		(*hw->ntb_ops->db_clear)(dev, (1 << 2));
		hw->peer_dev_up = 0;
		return;
	}
}

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
	/* Not FIFO right now. Just for testing memory write. */
	struct ntb_hw *hw = dev->dev_private;
	unsigned int i;
	void *bar_addr;
	size_t size;

	if (hw->ntb_ops->get_peer_mw_addr == NULL)
		return -ENOTSUP;
	bar_addr = (*hw->ntb_ops->get_peer_mw_addr)(dev, 0);
	size = (size_t)context;

	for (i = 0; i < count; i++)
		rte_memcpy(bar_addr, buffers[i]->buf_addr, size);
	return 0;
}

static int
ntb_dequeue_bufs(struct rte_rawdev *dev,
		 struct rte_rawdev_buf **buffers,
		 unsigned int count,
		 rte_rawdev_obj_t context)
{
	/* Not FIFO. Just for testing memory read. */
	struct ntb_hw *hw = dev->dev_private;
	unsigned int i;
	size_t size;

	size = (size_t)context;

	for (i = 0; i < count; i++)
		rte_memcpy(buffers[i]->buf_addr, hw->mz[i]->addr, size);
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
	struct ntb_hw *hw = dev->dev_private;
	int ret, i;

	/* TODO: init queues and start queues. */

	/* Map memory of bar_size to remote. */
	hw->mz = rte_zmalloc("struct rte_memzone *",
			     hw->mw_cnt * sizeof(struct rte_memzone *), 0);
	for (i = 0; i < hw->mw_cnt; i++) {
		ret = ntb_set_mw(dev, i, hw->mw_size[i]);
		if (ret) {
			NTB_LOG(ERR, "Fail to set mw.");
			return ret;
		}
	}

	dev->started = 1;

	return 0;
}

static void
ntb_dev_stop(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t time_out;
	int status;

	/* TODO: stop rx/tx queues. */

	if (!hw->peer_dev_up)
		goto clean;

	ntb_link_cleanup(dev);

	/* Notify the peer that device will be down. */
	if (hw->ntb_ops->peer_db_set == NULL) {
		NTB_LOG(ERR, "Peer doorbell setting is not supported.");
		return;
	}
	status = (*hw->ntb_ops->peer_db_set)(dev, 1);
	if (status) {
		NTB_LOG(ERR, "Failed to tell peer device is down.");
		return;
	}

	/*
	 * Set time out as 1s in case that the peer is stopped accidently
	 * without any notification.
	 */
	time_out = 1000000;

	/* Wait for cleanup work down before db mask clear. */
	while (hw->peer_dev_up && time_out) {
		time_out -= 10;
		rte_delay_us(10);
	}

clean:
	/* Clear doorbells mask. */
	if (hw->ntb_ops->db_set_mask == NULL) {
		NTB_LOG(ERR, "Doorbell mask setting is not supported.");
		return;
	}
	status = (*hw->ntb_ops->db_set_mask)(dev,
				(((uint64_t)1 << hw->db_cnt) - 1));
	if (status)
		NTB_LOG(ERR, "Failed to clear doorbells.");

	dev->started = 0;
}

static int
ntb_dev_close(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	struct rte_intr_handle *intr_handle;
	int ret = 0;

	if (dev->started)
		ntb_dev_stop(dev);

	/* TODO: free queues. */

	intr_handle = &hw->pci_dev->intr_handle;
	/* Clean datapath event and vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
	/* Disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* Unregister callback func to eal lib */
	rte_intr_callback_unregister(intr_handle,
				     ntb_dev_intr_handler, dev);

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
	struct rte_intr_handle *intr_handle;
	uint32_t val;
	int ret, i;

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

	/* Init doorbell. */
	hw->db_valid_mask = RTE_LEN2MASK(hw->db_cnt, uint64_t);

	intr_handle = &pci_dev->intr_handle;
	/* Register callback func to eal lib */
	rte_intr_callback_register(intr_handle,
				   ntb_dev_intr_handler, dev);

	ret = rte_intr_efd_enable(intr_handle, hw->db_cnt);
	if (ret)
		return ret;

	/* To clarify, the interrupt for each doorbell is already mapped
	 * by default for intel gen3. They are mapped to msix vec 1-32,
	 * and hardware intr is mapped to 0. Map all to 0 for uio.
	 */
	if (!rte_intr_cap_multiple(intr_handle)) {
		for (i = 0; i < hw->db_cnt; i++) {
			if (hw->ntb_ops->vector_bind == NULL)
				return -ENOTSUP;
			ret = (*hw->ntb_ops->vector_bind)(dev, i, 0);
			if (ret)
				return ret;
		}
	}

	if (hw->ntb_ops->db_set_mask == NULL ||
	    hw->ntb_ops->peer_db_set == NULL) {
		NTB_LOG(ERR, "Doorbell is not supported.");
		return -ENOTSUP;
	}
	hw->db_mask = 0;
	ret = (*hw->ntb_ops->db_set_mask)(dev, hw->db_mask);
	if (ret) {
		NTB_LOG(ERR, "Unable to enable intr for all dbs.");
		return ret;
	}

	/* enable uio intr after callback register */
	rte_intr_enable(intr_handle);

	if (hw->ntb_ops->spad_write == NULL) {
		NTB_LOG(ERR, "Scratchpad is not supported.");
		return -ENOTSUP;
	}
	/* Tell peer the mw_cnt of local side. */
	ret = (*hw->ntb_ops->spad_write)(dev, SPAD_NUM_MWS, 1, hw->mw_cnt);
	if (ret) {
		NTB_LOG(ERR, "Failed to tell peer mw count.");
		return ret;
	}

	/* Tell peer each mw size on local side. */
	for (i = 0; i < hw->mw_cnt; i++) {
		NTB_LOG(DEBUG, "Local %u mw size: 0x%"PRIx64"", i,
				hw->mw_size[i]);
		val = hw->mw_size[i] >> 32;
		ret = (*hw->ntb_ops->spad_write)
				(dev, SPAD_MW0_SZ_H + 2 * i, 1, val);
		if (ret) {
			NTB_LOG(ERR, "Failed to tell peer mw size.");
			return ret;
		}

		val = hw->mw_size[i];
		ret = (*hw->ntb_ops->spad_write)
				(dev, SPAD_MW0_SZ_L + 2 * i, 1, val);
		if (ret) {
			NTB_LOG(ERR, "Failed to tell peer mw size.");
			return ret;
		}
	}

	/* Ring doorbell 0 to tell peer the device is ready. */
	ret = (*hw->ntb_ops->peer_db_set)(dev, 0);
	if (ret) {
		NTB_LOG(ERR, "Failed to tell peer device is probed.");
		return ret;
	}

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
