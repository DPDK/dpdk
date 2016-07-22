/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_kvargs.h>

#include "virtio_ethdev.h"
#include "virtio_logs.h"
#include "virtio_pci.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"
#include "virtio_user/virtio_user_dev.h"

#define virtio_user_get_dev(hw) \
	((struct virtio_user_dev *)(hw)->virtio_user_dev)

static void
virtio_user_read_dev_config(struct virtio_hw *hw, size_t offset,
		     void *dst, int length)
{
	int i;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (offset == offsetof(struct virtio_net_config, mac) &&
	    length == ETHER_ADDR_LEN) {
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			((uint8_t *)dst)[i] = dev->mac_addr[i];
		return;
	}

	if (offset == offsetof(struct virtio_net_config, status))
		*(uint16_t *)dst = dev->status;

	if (offset == offsetof(struct virtio_net_config, max_virtqueue_pairs))
		*(uint16_t *)dst = dev->max_queue_pairs;
}

static void
virtio_user_write_dev_config(struct virtio_hw *hw, size_t offset,
		      const void *src, int length)
{
	int i;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if ((offset == offsetof(struct virtio_net_config, mac)) &&
	    (length == ETHER_ADDR_LEN))
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			dev->mac_addr[i] = ((const uint8_t *)src)[i];
	else
		PMD_DRV_LOG(ERR, "not supported offset=%zu, len=%d\n",
			    offset, length);
}

static void
virtio_user_set_status(struct virtio_hw *hw, uint8_t status)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (status & VIRTIO_CONFIG_STATUS_DRIVER_OK)
		virtio_user_start_device(dev);
	dev->status = status;
}

static void
virtio_user_reset(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	virtio_user_stop_device(dev);
}

static uint8_t
virtio_user_get_status(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	return dev->status;
}

static uint64_t
virtio_user_get_features(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	return dev->features;
}

static void
virtio_user_set_features(struct virtio_hw *hw, uint64_t features)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	dev->features = features;
}

static uint8_t
virtio_user_get_isr(struct virtio_hw *hw __rte_unused)
{
	/* When config interrupt happens, driver calls this function to query
	 * what kinds of change happen. Interrupt mode not supported for now.
	 */
	return 0;
}

static uint16_t
virtio_user_set_config_irq(struct virtio_hw *hw __rte_unused,
		    uint16_t vec __rte_unused)
{
	return VIRTIO_MSI_NO_VECTOR;
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

static int
virtio_user_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);
	uint16_t queue_idx = vq->vq_queue_index;
	uint64_t desc_addr, avail_addr, used_addr;

	desc_addr = (uintptr_t)vq->vq_ring_virt_mem;
	avail_addr = desc_addr + vq->vq_nentries * sizeof(struct vring_desc);
	used_addr = RTE_ALIGN_CEIL(avail_addr + offsetof(struct vring_avail,
							 ring[vq->vq_nentries]),
				   VIRTIO_PCI_VRING_ALIGN);

	dev->vrings[queue_idx].num = vq->vq_nentries;
	dev->vrings[queue_idx].desc = (void *)(uintptr_t)desc_addr;
	dev->vrings[queue_idx].avail = (void *)(uintptr_t)avail_addr;
	dev->vrings[queue_idx].used = (void *)(uintptr_t)used_addr;

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
}

static void
virtio_user_notify_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	uint64_t buf = 1;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (hw->cvq && (hw->cvq->vq == vq)) {
		virtio_user_handle_cq(dev, vq->vq_queue_index);
		return;
	}

	if (write(dev->kickfds[vq->vq_queue_index], &buf, sizeof(buf)) < 0)
		PMD_DRV_LOG(ERR, "failed to kick backend: %s\n",
			    strerror(errno));
}

static const struct virtio_pci_ops virtio_user_ops = {
	.read_dev_cfg	= virtio_user_read_dev_config,
	.write_dev_cfg	= virtio_user_write_dev_config,
	.reset		= virtio_user_reset,
	.get_status	= virtio_user_get_status,
	.set_status	= virtio_user_set_status,
	.get_features	= virtio_user_get_features,
	.set_features	= virtio_user_set_features,
	.get_isr	= virtio_user_get_isr,
	.set_config_irq	= virtio_user_set_config_irq,
	.get_queue_num	= virtio_user_get_queue_num,
	.setup_queue	= virtio_user_setup_queue,
	.del_queue	= virtio_user_del_queue,
	.notify_queue	= virtio_user_notify_queue,
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
	NULL
};

#define VIRTIO_USER_DEF_CQ_EN	0
#define VIRTIO_USER_DEF_Q_NUM	1
#define VIRTIO_USER_DEF_Q_SZ	256

static int
get_string_arg(const char *key __rte_unused,
	       const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	return 0;
}

static int
get_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(uint64_t *)extra_args = strtoull(value, NULL, 0);

	return 0;
}

static struct rte_eth_dev *
virtio_user_eth_dev_alloc(const char *name)
{
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_data *data;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;

	eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_VIRTUAL);
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "cannot alloc rte_eth_dev");
		return NULL;
	}

	data = eth_dev->data;

	hw = rte_zmalloc(NULL, sizeof(*hw), 0);
	if (!hw) {
		PMD_INIT_LOG(ERR, "malloc virtio_hw failed");
		rte_eth_dev_release_port(eth_dev);
		return NULL;
	}

	dev = rte_zmalloc(NULL, sizeof(*dev), 0);
	if (!dev) {
		PMD_INIT_LOG(ERR, "malloc virtio_user_dev failed");
		rte_eth_dev_release_port(eth_dev);
		rte_free(hw);
		return NULL;
	}

	hw->vtpci_ops = &virtio_user_ops;
	hw->use_msix = 0;
	hw->modern   = 0;
	hw->virtio_user_dev = dev;
	data->dev_private = hw;
	data->numa_node = SOCKET_ID_ANY;
	data->kdrv = RTE_KDRV_NONE;
	data->dev_flags = RTE_ETH_DEV_DETACHABLE;
	eth_dev->pci_dev = NULL;
	eth_dev->driver = NULL;
	return eth_dev;
}

/* Dev initialization routine. Invoked once for each virtio vdev at
 * EAL init time, see rte_eal_dev_init().
 * Returns 0 on success.
 */
static int
virtio_user_pmd_devinit(const char *name, const char *params)
{
	struct rte_kvargs *kvlist = NULL;
	struct rte_eth_dev *eth_dev;
	struct virtio_hw *hw;
	uint64_t queues = VIRTIO_USER_DEF_Q_NUM;
	uint64_t cq = VIRTIO_USER_DEF_CQ_EN;
	uint64_t queue_size = VIRTIO_USER_DEF_Q_SZ;
	char *path = NULL;
	char *mac_addr = NULL;
	int ret = -1;

	if (!params || params[0] == '\0') {
		PMD_INIT_LOG(ERR, "arg %s is mandatory for virtio_user",
			  VIRTIO_USER_ARG_QUEUE_SIZE);
		goto end;
	}

	kvlist = rte_kvargs_parse(params, valid_args);
	if (!kvlist) {
		PMD_INIT_LOG(ERR, "error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_PATH) == 1) {
		ret = rte_kvargs_process(kvlist, VIRTIO_USER_ARG_PATH,
					 &get_string_arg, &path);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_PATH);
			goto end;
		}
	} else {
		PMD_INIT_LOG(ERR, "arg %s is mandatory for virtio_user\n",
			  VIRTIO_USER_ARG_QUEUE_SIZE);
		goto end;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_MAC) == 1) {
		ret = rte_kvargs_process(kvlist, VIRTIO_USER_ARG_MAC,
					 &get_string_arg, &mac_addr);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_MAC);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUE_SIZE) == 1) {
		ret = rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUE_SIZE,
					 &get_integer_arg, &queue_size);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_QUEUE_SIZE);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUES_NUM) == 1) {
		ret = rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUES_NUM,
					 &get_integer_arg, &queues);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_QUEUES_NUM);
			goto end;
		}
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_CQ_NUM) == 1) {
		ret = rte_kvargs_process(kvlist, VIRTIO_USER_ARG_CQ_NUM,
					 &get_integer_arg, &cq);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_CQ_NUM);
			goto end;
		}
	} else if (queues > 1) {
		cq = 1;
	}

	if (queues > 1 && cq == 0) {
		PMD_INIT_LOG(ERR, "multi-q requires ctrl-q");
		goto end;
	}

	eth_dev = virtio_user_eth_dev_alloc(name);
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "virtio_user fails to alloc device");
		goto end;
	}

	hw = eth_dev->data->dev_private;
	if (virtio_user_dev_init(hw->virtio_user_dev, path, queues, cq,
				 queue_size, mac_addr) < 0)
		goto end;

	/* previously called by rte_eal_pci_probe() for physical dev */
	if (eth_virtio_dev_init(eth_dev) < 0) {
		PMD_INIT_LOG(ERR, "eth_virtio_dev_init fails");
		goto end;
	}
	ret = 0;

end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (path)
		free(path);
	if (mac_addr)
		free(mac_addr);
	return ret;
}

/** Called by rte_eth_dev_detach() */
static int
virtio_user_pmd_devuninit(const char *name)
{
	struct rte_eth_dev *eth_dev;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;

	if (!name)
		return -EINVAL;

	PMD_DRV_LOG(INFO, "Un-Initializing %s\n", name);
	eth_dev = rte_eth_dev_allocated(name);
	if (!eth_dev)
		return -ENODEV;

	/* make sure the device is stopped, queues freed */
	rte_eth_dev_close(eth_dev->data->port_id);

	hw = eth_dev->data->dev_private;
	dev = hw->virtio_user_dev;
	virtio_user_dev_uninit(dev);

	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_driver virtio_user_driver = {
	.type   = PMD_VDEV,
	.init   = virtio_user_pmd_devinit,
	.uninit = virtio_user_pmd_devuninit,
};

PMD_REGISTER_DRIVER(virtio_user_driver, virtio_user);
DRIVER_REGISTER_PARAM_STRING(virtio_user,
	"path=<path> "
	"mac=<mac addr> "
	"cq=<int> "
	"queue_size=<int> "
	"queues=<int>");
