/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/major.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>

#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <ethdev_vdev.h>
#include <bus_vdev_driver.h>

#include "virtio_ethdev.h"
#include "virtio_logs.h"
#include "virtio.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"
#include "virtio_user/virtio_user_dev.h"
#include "virtio_user/vhost.h"

/* Virtio-user multiprocess communication channel */
#define VIRTIO_USER_MP_NAME "virtio_user_mp"

/*
 * How long a secondary waits for the primary to reply with eventfds.
 * The primary's MP handler does no blocking work, so a slow reply only
 * means the EAL MP thread is backed up; 5s gives ample margin without
 * stalling probe for too long if the primary is gone.
 */
#define VIRTIO_USER_MP_TIMEOUT_SEC 5

struct virtio_user_mp_param {
	char port_name[RTE_DEV_NAME_MAX_LEN];
};

/*
 * Per-process private data, referenced by eth_dev->process_private which (unlike
 * dev_private) is NOT shared between primary and secondary processes. A secondary
 * stores the kick/call fds it receives from the primary here, so it never mutates
 * the primary-owned shared dev->kickfds/dev->callfds arrays. callfds are kept for
 * a complete per-process view of the backend fds; only kickfds are used by the
 * secondary data path today.
 */
struct virtio_user_proc_priv {
	uint32_t nr_queues;
	int *kickfds;
	int *callfds;
};

/* Guards one-time registration of the process-wide MP action. */
static rte_spinlock_t virtio_user_mp_lock = RTE_SPINLOCK_INITIALIZER;
static bool virtio_user_mp_registered;

#define virtio_user_get_dev(hwp) container_of(hwp, struct virtio_user_dev, hw)

static void
virtio_user_read_dev_config(struct virtio_hw *hw, size_t offset,
		     void *dst, int length)
{
	int i;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (offset == offsetof(struct virtio_net_config, mac) &&
	    length == RTE_ETHER_ADDR_LEN) {
		for (i = 0; i < RTE_ETHER_ADDR_LEN; ++i)
			((uint8_t *)dst)[i] = dev->mac_addr[i];
		return;
	}

	if (offset == offsetof(struct virtio_net_config, status)) {
		virtio_user_dev_update_link_state(dev);

		*(uint16_t *)dst = dev->net_status;
	}

	if (offset == offsetof(struct virtio_net_config, max_virtqueue_pairs))
		*(uint16_t *)dst = dev->max_queue_pairs;

	if (offset >= offsetof(struct virtio_net_config, rss_max_key_size))
		virtio_user_dev_get_rss_config(dev, dst, offset, length);
}

static void
virtio_user_write_dev_config(struct virtio_hw *hw, size_t offset,
		      const void *src, int length)
{
	int i;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if ((offset == offsetof(struct virtio_net_config, mac)) &&
	    (length == RTE_ETHER_ADDR_LEN)) {
		for (i = 0; i < RTE_ETHER_ADDR_LEN; ++i)
			dev->mac_addr[i] = ((const uint8_t *)src)[i];
		virtio_user_dev_set_mac(dev);
		virtio_user_dev_get_mac(dev);
	} else {
		PMD_DRV_LOG(ERR, "not supported offset=%zu, len=%d",
			    offset, length);
	}
}

static void
virtio_user_reset(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (dev->status & VIRTIO_CONFIG_STATUS_DRIVER_OK)
		virtio_user_stop_device(dev);
}

static void
virtio_user_set_status(struct virtio_hw *hw, uint8_t status)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);
	uint8_t old_status = dev->status;

	if (status & VIRTIO_CONFIG_STATUS_FEATURES_OK &&
			~old_status & VIRTIO_CONFIG_STATUS_FEATURES_OK)
		virtio_user_dev_set_features(dev);

	if (status & VIRTIO_CONFIG_STATUS_DRIVER_OK) {
		if (virtio_user_start_device(dev)) {
			virtio_user_dev_update_status(dev);
			return;
		}
	} else if (status == VIRTIO_CONFIG_STATUS_RESET) {
		virtio_user_reset(hw);
	}

	virtio_user_dev_set_status(dev, status);
}

static uint8_t
virtio_user_get_status(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	virtio_user_dev_update_status(dev);

	return dev->status;
}

static uint64_t
virtio_user_get_features(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	/* unmask feature bits defined in vhost user protocol */
	return (dev->device_features | dev->frontend_features) &
		VIRTIO_PMD_SUPPORTED_GUEST_FEATURES;
}

static void
virtio_user_set_features(struct virtio_hw *hw, uint64_t features)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	dev->features = features & (dev->device_features | dev->frontend_features);
}

static int
virtio_user_features_ok(struct virtio_hw *hw __rte_unused)
{
	return 0;
}

static uint8_t
virtio_user_get_isr(struct virtio_hw *hw __rte_unused)
{
	/* rxq interrupts and config interrupt are separated in virtio-user,
	 * here we only report config change.
	 */
	return VIRTIO_ISR_CONFIG;
}

static uint16_t
virtio_user_set_config_irq(struct virtio_hw *hw __rte_unused,
		    uint16_t vec __rte_unused)
{
	return 0;
}

static uint16_t
virtio_user_set_queue_irq(struct virtio_hw *hw __rte_unused,
			  struct virtqueue *vq __rte_unused,
			  uint16_t vec)
{
	/* pretend we have done that */
	return vec;
}

/* This function is to get the queue size, aka, number of descs, of a specified
 * queue. Different with the VHOST_USER_GET_QUEUE_NUM, which is used to get the
 * max supported queues.
 */
static uint16_t
virtio_user_get_queue_num(struct virtio_hw *hw, uint16_t queue_id __rte_unused)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	/* Currently, each queue has same queue size */
	return dev->queue_size;
}

static void
virtio_user_setup_queue_packed(struct virtqueue *vq,
			       struct virtio_user_dev *dev)
{
	uint16_t queue_idx = vq->vq_queue_index;
	struct vring_packed *vring;
	uint64_t desc_addr;
	uint64_t avail_addr;
	uint64_t used_addr;
	uint16_t i;

	vring  = &dev->vrings.packed[queue_idx];
	desc_addr = (uintptr_t)vq->vq_ring_virt_mem;
	avail_addr = desc_addr + vq->vq_nentries *
		sizeof(struct vring_packed_desc);
	used_addr = RTE_ALIGN_CEIL(avail_addr +
			   sizeof(struct vring_packed_desc_event),
			   VIRTIO_VRING_ALIGN);
	vring->num = vq->vq_nentries;
	vring->desc_iova = vq->vq_ring_mem;
	vring->desc = (void *)(uintptr_t)desc_addr;
	vring->driver = (void *)(uintptr_t)avail_addr;
	vring->device = (void *)(uintptr_t)used_addr;
	dev->packed_queues[queue_idx].avail_wrap_counter = true;
	dev->packed_queues[queue_idx].used_wrap_counter = true;
	dev->packed_queues[queue_idx].used_idx = 0;

	for (i = 0; i < vring->num; i++)
		vring->desc[i].flags = 0;
}

static void
virtio_user_setup_queue_split(struct virtqueue *vq, struct virtio_user_dev *dev)
{
	uint16_t queue_idx = vq->vq_queue_index;
	uint64_t desc_addr, avail_addr, used_addr;

	desc_addr = (uintptr_t)vq->vq_ring_virt_mem;
	avail_addr = desc_addr + vq->vq_nentries * sizeof(struct vring_desc);
	used_addr = RTE_ALIGN_CEIL(avail_addr + offsetof(struct vring_avail,
							 ring[vq->vq_nentries]),
				   VIRTIO_VRING_ALIGN);

	dev->vrings.split[queue_idx].num = vq->vq_nentries;
	dev->vrings.split[queue_idx].desc_iova = vq->vq_ring_mem;
	dev->vrings.split[queue_idx].desc = (void *)(uintptr_t)desc_addr;
	dev->vrings.split[queue_idx].avail = (void *)(uintptr_t)avail_addr;
	dev->vrings.split[queue_idx].used = (void *)(uintptr_t)used_addr;
}

static int
virtio_user_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (virtio_with_packed_queue(hw))
		virtio_user_setup_queue_packed(vq, dev);
	else
		virtio_user_setup_queue_split(vq, dev);

	if (dev->notify_area)
		vq->notify_addr = dev->notify_area[vq->vq_queue_index];

	if (dev->hw_cvq && hw->cvq && (virtnet_cq_to_vq(hw->cvq) == vq))
		return virtio_user_dev_create_shadow_cvq(dev, vq);

	return 0;
}

static void
virtio_user_del_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	/* For legacy devices, write 0 to VIRTIO_PCI_QUEUE_PFN port, QEMU
	 * correspondingly stops the ioeventfds, and reset the status of
	 * the device.
	 * For modern devices, set queue desc, avail, used in PCI bar to 0,
	 * not see any more behavior in QEMU.
	 *
	 * Here we just care about what information to deliver to vhost-user
	 * or vhost-kernel. So we just close ioeventfd for now.
	 */
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	close(dev->callfds[vq->vq_queue_index]);
	close(dev->kickfds[vq->vq_queue_index]);

	if (hw->cvq && (virtnet_cq_to_vq(hw->cvq) == vq))
		virtio_user_dev_destroy_shadow_cvq(dev);
}

/*
 * Return the kick fd to notify the backend for a queue in the running process.
 * The secondary uses its own fds (process_private); the primary owns dev->kickfds.
 */
static int
virtio_user_get_kickfd(struct virtio_hw *hw, struct virtio_user_dev *dev,
		      uint16_t queue_idx)
{
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct rte_eth_dev *eth_dev = &rte_eth_devices[hw->port_id];
		struct virtio_user_proc_priv *pp = eth_dev->process_private;

		if (pp == NULL || queue_idx >= pp->nr_queues)
			return -1;
		return pp->kickfds[queue_idx];
	}

	return dev->kickfds[queue_idx];
}

static void
virtio_user_notify_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);
	uint64_t notify_data = 1;

	if (hw->cvq && (virtnet_cq_to_vq(hw->cvq) == vq)) {
		virtio_user_handle_cq(dev, vq->vq_queue_index);

		return;
	}

	if (!dev->notify_area) {
		int kickfd = virtio_user_get_kickfd(hw, dev, vq->vq_queue_index);

		if (kickfd < 0 || write(kickfd, &notify_data,
				sizeof(notify_data)) < 0)
			PMD_DRV_LOG(ERR, "failed to kick backend: %s",
				    strerror(errno));
		return;
	} else if (!virtio_with_feature(hw, VIRTIO_F_NOTIFICATION_DATA)) {
		rte_write16(vq->vq_queue_index, vq->notify_addr);
		return;
	}

	if (virtio_with_packed_queue(hw)) {
		/* Bit[0:15]: vq queue index
		 * Bit[16:30]: avail index
		 * Bit[31]: avail wrap counter
		 */
		notify_data = ((uint32_t)(!!(vq->vq_packed.cached_flags &
				VRING_PACKED_DESC_F_AVAIL)) << 31) |
				((uint32_t)vq->vq_avail_idx << 16) |
				vq->vq_queue_index;
	} else {
		/* Bit[0:15]: vq queue index
		 * Bit[16:31]: avail index
		 */
		notify_data = ((uint32_t)vq->vq_avail_idx << 16) |
				vq->vq_queue_index;
	}
	rte_write32(notify_data, vq->notify_addr);
}

static int
virtio_user_dev_close(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	virtio_user_dev_uninit(dev);

	return 0;
}

const struct virtio_ops virtio_user_ops = {
	.read_dev_cfg	= virtio_user_read_dev_config,
	.write_dev_cfg	= virtio_user_write_dev_config,
	.get_status	= virtio_user_get_status,
	.set_status	= virtio_user_set_status,
	.get_features	= virtio_user_get_features,
	.set_features	= virtio_user_set_features,
	.features_ok	= virtio_user_features_ok,
	.get_isr	= virtio_user_get_isr,
	.set_config_irq	= virtio_user_set_config_irq,
	.set_queue_irq	= virtio_user_set_queue_irq,
	.get_queue_num	= virtio_user_get_queue_num,
	.setup_queue	= virtio_user_setup_queue,
	.del_queue	= virtio_user_del_queue,
	.notify_queue	= virtio_user_notify_queue,
	.dev_close	= virtio_user_dev_close,
};

static const char *valid_args[] = {
#define VIRTIO_USER_ARG_QUEUES_NUM     "queues"
	VIRTIO_USER_ARG_QUEUES_NUM,
#define VIRTIO_USER_ARG_CQ_NUM         "cq"
	VIRTIO_USER_ARG_CQ_NUM,
#define VIRTIO_USER_ARG_MAC            "mac"
	VIRTIO_USER_ARG_MAC,
#define VIRTIO_USER_ARG_PATH           "path"
	VIRTIO_USER_ARG_PATH,
#define VIRTIO_USER_ARG_QUEUE_SIZE     "queue_size"
	VIRTIO_USER_ARG_QUEUE_SIZE,
#define VIRTIO_USER_ARG_INTERFACE_NAME "iface"
	VIRTIO_USER_ARG_INTERFACE_NAME,
#define VIRTIO_USER_ARG_SERVER_MODE    "server"
	VIRTIO_USER_ARG_SERVER_MODE,
#define VIRTIO_USER_ARG_MRG_RXBUF      "mrg_rxbuf"
	VIRTIO_USER_ARG_MRG_RXBUF,
#define VIRTIO_USER_ARG_IN_ORDER       "in_order"
	VIRTIO_USER_ARG_IN_ORDER,
#define VIRTIO_USER_ARG_PACKED_VQ      "packed_vq"
	VIRTIO_USER_ARG_PACKED_VQ,
#define VIRTIO_USER_ARG_SPEED          "speed"
	VIRTIO_USER_ARG_SPEED,
#define VIRTIO_USER_ARG_VECTORIZED     "vectorized"
	VIRTIO_USER_ARG_VECTORIZED,
	NULL
};

#define VIRTIO_USER_DEF_CQ_EN	0
#define VIRTIO_USER_DEF_Q_NUM	1
#define VIRTIO_USER_DEF_Q_SZ	256
#define VIRTIO_USER_DEF_SERVER_MODE	0

static int
get_string_arg(const char *key __rte_unused,
	       const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	if (!*(char **)extra_args)
		return -ENOMEM;

	return 0;
}

static int
get_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	uint64_t integer = 0;
	if (!value || !extra_args)
		return -EINVAL;
	errno = 0;
	integer = strtoull(value, NULL, 0);
	/* extra_args keeps default value, it should be replaced
	 * only in case of successful parsing of the 'value' arg
	 */
	if (errno == 0)
		*(uint64_t *)extra_args = integer;
	return -errno;
}

static uint32_t
vdpa_dynamic_major_num(void)
{
	FILE *fp;
	char *line = NULL;
	size_t size = 0;
	char name[11];
	bool found = false;
	uint32_t num;

	fp = fopen("/proc/devices", "r");
	if (fp == NULL) {
		PMD_INIT_LOG(ERR, "Cannot open /proc/devices: %s",
			     strerror(errno));
		return UNNAMED_MAJOR;
	}

	while (getline(&line, &size, fp) > 0) {
		char *stripped = line + strspn(line, " ");
		if ((sscanf(stripped, "%u %10s", &num, name) == 2) &&
		    (strncmp(name, "vhost-vdpa", 10) == 0)) {
			found = true;
			break;
		}
	}
	free(line);
	fclose(fp);
	return found ? num : UNNAMED_MAJOR;
}

static enum virtio_user_backend_type
virtio_user_backend_type(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) == -1) {
		if (errno == ENOENT)
			return VIRTIO_USER_BACKEND_VHOST_USER;

		PMD_INIT_LOG(ERR, "Stat fails: %s (%s)", path,
			     strerror(errno));
		return VIRTIO_USER_BACKEND_UNKNOWN;
	}

	if (S_ISSOCK(sb.st_mode)) {
		return VIRTIO_USER_BACKEND_VHOST_USER;
	} else if (S_ISCHR(sb.st_mode)) {
		if (major(sb.st_rdev) == MISC_MAJOR)
			return VIRTIO_USER_BACKEND_VHOST_KERNEL;
		if (major(sb.st_rdev) == vdpa_dynamic_major_num())
			return VIRTIO_USER_BACKEND_VHOST_VDPA;
	}
	return VIRTIO_USER_BACKEND_UNKNOWN;
}

static struct rte_eth_dev *
virtio_user_eth_dev_alloc(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_data *data;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;

	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*dev));
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "cannot alloc rte_eth_dev");
		return NULL;
	}

	data = eth_dev->data;
	dev = eth_dev->data->dev_private;
	hw = &dev->hw;

	hw->port_id = data->port_id;
	VIRTIO_OPS(hw) = &virtio_user_ops;

	hw->intr_lsc = 1;
	hw->use_vec_rx = 0;
	hw->use_vec_tx = 0;
	hw->use_inorder_rx = 0;
	hw->use_inorder_tx = 0;

	return eth_dev;
}

static void
virtio_user_eth_dev_free(struct rte_eth_dev *eth_dev)
{
	rte_eth_dev_release_port(eth_dev);
}

/* Close and free a secondary's per-process eventfd storage. */
static void
virtio_user_free_proc_priv(struct rte_eth_dev *eth_dev)
{
	struct virtio_user_proc_priv *pp = eth_dev->process_private;
	uint32_t i;

	if (pp == NULL)
		return;

	for (i = 0; i < pp->nr_queues; i++) {
		if (pp->kickfds != NULL && pp->kickfds[i] >= 0)
			close(pp->kickfds[i]);
		if (pp->callfds != NULL && pp->callfds[i] >= 0)
			close(pp->callfds[i]);
	}

	rte_free(pp->kickfds);
	rte_free(pp->callfds);
	rte_free(pp);
	eth_dev->process_private = NULL;
}

/*
 * Primary-side MP handler: reply with this port's kick/call eventfds so the
 * requesting secondary can talk to the vhost backend. Always sends a reply
 * (num_fds == 0 on error) so the secondary fails fast instead of timing out.
 */
static int
virtio_user_mp_primary_handler(const struct rte_mp_msg *msg, const void *peer)
{
	const struct virtio_user_mp_param *param =
		(const struct virtio_user_mp_param *)msg->param;
	int eventfds[VIRTIO_USER_MAX_EVENTFDS];
	struct rte_eth_dev *eth_dev;
	struct virtio_user_dev *dev;
	struct rte_mp_msg reply;
	int num_fds;
	int i;

	memset(&reply, 0, sizeof(reply));
	strlcpy(reply.name, msg->name, sizeof(reply.name));
	reply.len_param = 0;
	reply.num_fds = 0;

	eth_dev = rte_eth_dev_get_by_name(param->port_name);
	if (eth_dev == NULL || eth_dev->data->dev_private == NULL) {
		PMD_INIT_LOG(ERR, "Failed to find virtio_user port: %s",
			     param->port_name);
		return rte_mp_reply(&reply, peer);
	}

	dev = eth_dev->data->dev_private;

	/* serialize against control-path changes to the fd arrays */
	pthread_mutex_lock(&dev->mutex);
	num_fds = virtio_user_get_eventfds_from_dev(dev, eventfds);
	if (num_fds >= 0 && num_fds <= RTE_MP_MAX_FD_NUM) {
		reply.num_fds = num_fds;
		for (i = 0; i < num_fds; i++)
			reply.fds[i] = eventfds[i];
	} else {
		PMD_INIT_LOG(ERR, "Cannot share eventfds for %s (ret=%d)",
			     param->port_name, num_fds);
	}
	pthread_mutex_unlock(&dev->mutex);

	return rte_mp_reply(&reply, peer);
}

/*
 * Secondary-side: request the primary's kick/call eventfds and store them in
 * this process's eth_dev->process_private. The shared dev->kickfds/dev->callfds
 * arrays (owned by the primary) are never touched.
 */
static int
virtio_user_sync_eventfds(struct rte_eth_dev *eth_dev, struct virtio_user_dev *dev)
{
	struct rte_mp_msg mp_req, *mp_rep;
	struct rte_mp_reply mp_reply = {0};
	struct virtio_user_mp_param *req_param;
	struct timespec ts = {.tv_sec = VIRTIO_USER_MP_TIMEOUT_SEC, .tv_nsec = 0};
	struct virtio_user_proc_priv *pp;
	uint32_t total_queues, i;
	int nr_fds, ret = 0;

	if (dev == NULL)
		return -EINVAL;

	if (rte_eal_process_type() != RTE_PROC_SECONDARY)
		return -EINVAL;

	total_queues = dev->max_queue_pairs * 2 + (dev->hw_cvq ? 1 : 0);

	pp = rte_zmalloc("virtio_user_proc_priv", sizeof(*pp), 0);
	if (pp == NULL)
		return -ENOMEM;

	pp->kickfds = rte_calloc("virtio_user_proc_priv",
				 total_queues, sizeof(int), 0);
	pp->callfds = rte_calloc("virtio_user_proc_priv",
				 total_queues, sizeof(int), 0);
	if (pp->kickfds == NULL || pp->callfds == NULL) {
		ret = -ENOMEM;
		goto err_free;
	}
	for (i = 0; i < total_queues; i++) {
		pp->kickfds[i] = -1;
		pp->callfds[i] = -1;
	}

	memset(&mp_req, 0, sizeof(mp_req));
	req_param = (struct virtio_user_mp_param *)mp_req.param;
	strlcpy(req_param->port_name, eth_dev->data->name,
		sizeof(req_param->port_name));
	strlcpy(mp_req.name, VIRTIO_USER_MP_NAME, RTE_MP_MAX_NAME_LEN);
	mp_req.len_param = sizeof(*req_param);
	mp_req.num_fds = 0;

	if (rte_mp_request_sync(&mp_req, &mp_reply, &ts) < 0 ||
	    mp_reply.nb_received != 1) {
		PMD_INIT_LOG(ERR, "Failed to request eventfds from primary");
		free(mp_reply.msgs);
		ret = -EIO;
		goto err_free;
	}

	mp_rep = &mp_reply.msgs[0];
	nr_fds = mp_rep->num_fds;

	/* a partially-synced device cannot work: treat any mismatch as fatal */
	if (nr_fds != (int)(total_queues * VIRTIO_USER_EVENTFDS_PER_QUEUE)) {
		PMD_INIT_LOG(ERR, "Expected %u eventfds, received %d",
			     total_queues * VIRTIO_USER_EVENTFDS_PER_QUEUE, nr_fds);
		for (i = 0; i < (uint32_t)nr_fds; i++)
			close(mp_rep->fds[i]);
		free(mp_reply.msgs);
		ret = -EPROTO;
		goto err_free;
	}

	for (i = 0; i < total_queues; i++) {
		pp->kickfds[i] = mp_rep->fds[i * VIRTIO_USER_EVENTFDS_PER_QUEUE];
		pp->callfds[i] = mp_rep->fds[i * VIRTIO_USER_EVENTFDS_PER_QUEUE + 1];
	}
	pp->nr_queues = total_queues;
	free(mp_reply.msgs);

	eth_dev->process_private = pp;
	PMD_INIT_LOG(DEBUG, "Synced %u queue eventfds for secondary port %s",
		     total_queues, eth_dev->data->name);
	return 0;

err_free:
	rte_free(pp->kickfds);
	rte_free(pp->callfds);
	rte_free(pp);
	return ret;
}

/* Dev initialization routine. Invoked once for each virtio vdev at
 * EAL init time, see rte_bus_probe().
 * Returns 0 on success.
 */
static int
virtio_user_pmd_probe(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist = NULL;
	struct rte_eth_dev *eth_dev;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;
	enum virtio_user_backend_type backend_type = VIRTIO_USER_BACKEND_UNKNOWN;
	uint64_t queues = VIRTIO_USER_DEF_Q_NUM;
	uint64_t cq = VIRTIO_USER_DEF_CQ_EN;
	uint64_t queue_size = VIRTIO_USER_DEF_Q_SZ;
	uint64_t server_mode = VIRTIO_USER_DEF_SERVER_MODE;
	uint64_t mrg_rxbuf = 1;
	uint64_t in_order = 1;
	uint64_t packed_vq = 0;
	uint64_t vectorized = 0;
	char *path = NULL;
	char *ifname = NULL;
	char *mac_addr = NULL;
	int ret = -1;

	RTE_BUILD_BUG_ON(offsetof(struct virtio_user_dev, hw) != 0);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		const char *name = rte_vdev_device_name(vdev);
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_INIT_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}

		dev = eth_dev->data->dev_private;
		hw = &dev->hw;
		VIRTIO_OPS(hw) = &virtio_user_ops;

		if (eth_virtio_dev_init(eth_dev) < 0) {
			PMD_INIT_LOG(ERR, "eth_virtio_dev_init fails");
			rte_eth_dev_release_port(eth_dev);
			return -1;
		}

		eth_dev->dev_ops = &virtio_user_secondary_eth_dev_ops;
		eth_dev->device = &vdev->device;

		/* populate this process's eventfds before announcing the port */
		ret = virtio_user_sync_eventfds(eth_dev, dev);
		if (ret < 0) {
			PMD_INIT_LOG(ERR,
				     "Failed to sync eventfds in secondary: %d",
				     ret);
			rte_eth_dev_release_port(eth_dev);
			return ret;
		}

		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	kvlist = rte_kvargs_parse(rte_vdev_device_args(vdev), valid_args);
	if (!kvlist) {
		PMD_INIT_LOG(ERR, "error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_PATH) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_PATH,
				       &get_string_arg, &path) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_PATH);
			goto end;
		}
	} else {
		PMD_INIT_LOG(ERR, "arg %s is mandatory for virtio_user",
			     VIRTIO_USER_ARG_PATH);
		goto end;
	}

	backend_type = virtio_user_backend_type(path);
	if (backend_type == VIRTIO_USER_BACKEND_UNKNOWN) {
		PMD_INIT_LOG(ERR,
			     "unable to determine backend type for path %s",
			path);
		goto end;
	}
	PMD_INIT_LOG(INFO, "Backend type detected: %s",
		     virtio_user_backend_strings[backend_type]);

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_INTERFACE_NAME) == 1) {
		if (backend_type != VIRTIO_USER_BACKEND_VHOST_KERNEL) {
			PMD_INIT_LOG(ERR,
				"arg %s applies only to vhost-kernel backend",
				VIRTIO_USER_ARG_INTERFACE_NAME);
			goto end;
		}

		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_INTERFACE_NAME,
				       &get_string_arg, &ifname) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_INTERFACE_NAME);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_MAC) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_MAC,
				       &get_string_arg, &mac_addr) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_MAC);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUE_SIZE) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUE_SIZE,
				       &get_integer_arg, &queue_size) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_QUEUE_SIZE);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUES_NUM) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUES_NUM,
				       &get_integer_arg, &queues) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_QUEUES_NUM);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_SERVER_MODE) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_SERVER_MODE,
				       &get_integer_arg, &server_mode) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_SERVER_MODE);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_CQ_NUM) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_CQ_NUM,
				       &get_integer_arg, &cq) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_CQ_NUM);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_PACKED_VQ) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_PACKED_VQ,
				       &get_integer_arg, &packed_vq) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_PACKED_VQ);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_VECTORIZED) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_VECTORIZED,
				       &get_integer_arg, &vectorized) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_VECTORIZED);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_MRG_RXBUF) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_MRG_RXBUF,
				       &get_integer_arg, &mrg_rxbuf) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_MRG_RXBUF);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_IN_ORDER) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_IN_ORDER,
				       &get_integer_arg, &in_order) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_IN_ORDER);
			goto end;
		}
	}

	eth_dev = virtio_user_eth_dev_alloc(vdev);
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "virtio_user fails to alloc device");
		goto end;
	}

	dev = eth_dev->data->dev_private;
	hw = &dev->hw;
	if (virtio_user_dev_init(dev, path, (uint16_t)queues, cq,
			 queue_size, mac_addr, &ifname, server_mode,
			 mrg_rxbuf, in_order, packed_vq, backend_type) < 0) {
		PMD_INIT_LOG(ERR, "virtio_user_dev_init fails");
		virtio_user_eth_dev_free(eth_dev);
		goto end;
	}

	/*
	 * Virtio-user requires using virtual addresses for the descriptors
	 * buffers, whatever other devices require
	 */
	if (backend_type == VIRTIO_USER_BACKEND_VHOST_VDPA)
		/* vDPA backend requires using IOVA for the buffers
		 * to make it work in IOVA as PA mode also.
		 */
		hw->use_va = false;
	else
		hw->use_va = true;

	/* previously called by pci probing for physical dev */
	if (eth_virtio_dev_init(eth_dev) < 0) {
		PMD_INIT_LOG(ERR, "eth_virtio_dev_init fails");
		virtio_user_dev_uninit(dev);
		virtio_user_eth_dev_free(eth_dev);
		goto end;
	}

	if (vectorized) {
		if (packed_vq) {
#if defined(CC_AVX512_SUPPORT) || defined(RTE_ARCH_ARM)
			hw->use_vec_rx = 1;
			hw->use_vec_tx = 1;
#else
			PMD_INIT_LOG(INFO,
				"building environment do not support packed ring vectorized");
#endif
		} else {
			hw->use_vec_rx = 1;
		}
	}

	/*
	 * Register the process-wide MP action once so secondaries can fetch a
	 * port's eventfds by name. It is intentionally left registered for the
	 * lifetime of the process (cleaned up at exit): unregistering per device
	 * cannot drain handler calls already dispatched on the EAL MP thread.
	 */
	rte_spinlock_lock(&virtio_user_mp_lock);
	if (!virtio_user_mp_registered) {
		ret = rte_mp_action_register(VIRTIO_USER_MP_NAME,
					     virtio_user_mp_primary_handler);
		if (ret < 0 && rte_errno != EEXIST) {
			rte_spinlock_unlock(&virtio_user_mp_lock);
			if (rte_errno == ENOTSUP) {
				PMD_INIT_LOG(WARNING,
					"MP unsupported, secondary eventfd sharing disabled");
				rte_eth_dev_probing_finish(eth_dev);
				ret = 0;
				goto end;
			}
			PMD_INIT_LOG(ERR, "Failed to register MP handler: %s",
				     strerror(rte_errno));
			virtio_user_dev_uninit(dev);
			virtio_user_eth_dev_free(eth_dev);
			ret = -1;
			goto end;
		}
		virtio_user_mp_registered = true;
	}
	rte_spinlock_unlock(&virtio_user_mp_lock);

	rte_eth_dev_probing_finish(eth_dev);
	ret = 0;

end:
	rte_kvargs_free(kvlist);
	free(path);
	free(mac_addr);
	free(ifname);
	return ret;
}

static int
virtio_user_pmd_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eth_dev *eth_dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Un-Initializing %s", name);
	eth_dev = rte_eth_dev_allocated(name);
	/* Port has already been released by close. */
	if (!eth_dev)
		return 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		virtio_user_free_proc_priv(eth_dev);
		return rte_eth_dev_release_port(eth_dev);
	}

	/* make sure the device is stopped, queues freed */
	return rte_eth_dev_close(eth_dev->data->port_id);
}

static int virtio_user_pmd_dma_map(struct rte_vdev_device *vdev, void *addr,
		uint64_t iova, size_t len)
{
	const char *name;
	struct rte_eth_dev *eth_dev;
	struct virtio_user_dev *dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	eth_dev = rte_eth_dev_allocated(name);
	/* Port has already been released by close. */
	if (!eth_dev)
		return 0;

	dev = eth_dev->data->dev_private;

	if (dev->ops->dma_map)
		return dev->ops->dma_map(dev, addr, iova, len);

	return 0;
}

static int virtio_user_pmd_dma_unmap(struct rte_vdev_device *vdev, void *addr,
		uint64_t iova, size_t len)
{
	const char *name;
	struct rte_eth_dev *eth_dev;
	struct virtio_user_dev *dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	eth_dev = rte_eth_dev_allocated(name);
	/* Port has already been released by close. */
	if (!eth_dev)
		return 0;

	dev = eth_dev->data->dev_private;

	if (dev->ops->dma_unmap)
		return dev->ops->dma_unmap(dev, addr, iova, len);

	return 0;
}

static struct rte_vdev_driver virtio_user_driver = {
	.probe = virtio_user_pmd_probe,
	.remove = virtio_user_pmd_remove,
	.dma_map = virtio_user_pmd_dma_map,
	.dma_unmap = virtio_user_pmd_dma_unmap,
};

RTE_PMD_REGISTER_VDEV(net_virtio_user, virtio_user_driver);
RTE_PMD_REGISTER_ALIAS(net_virtio_user, virtio_user);
RTE_PMD_REGISTER_PARAM_STRING(net_virtio_user,
	"path=<path> "
	"mac=<mac addr> "
	"cq=<int> "
	"queue_size=<int> "
	"queues=<int> "
	"iface=<string> "
	"server=<0|1> "
	"mrg_rxbuf=<0|1> "
	"in_order=<0|1> "
	"packed_vq=<0|1> "
	"speed=<int> "
	"vectorized=<0|1>");
