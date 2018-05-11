/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_ethdev_vdev.h>
#include <rte_bus_vdev.h>
#include <rte_alarm.h>

#include "virtio_ethdev.h"
#include "virtio_logs.h"
#include "virtio_pci.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"
#include "virtio_user/virtio_user_dev.h"

#define virtio_user_get_dev(hw) \
	((struct virtio_user_dev *)(hw)->virtio_user_dev)

static int
virtio_user_server_reconnect(struct virtio_user_dev *dev)
{
	int ret;
	int flag;
	int connectfd;
	uint64_t features = dev->device_features;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[dev->port_id];

	connectfd = accept(dev->listenfd, NULL, NULL);
	if (connectfd < 0)
		return -1;

	dev->vhostfd = connectfd;
	if (dev->ops->send_request(dev, VHOST_USER_GET_FEATURES,
				   &dev->device_features) < 0) {
		PMD_INIT_LOG(ERR, "get_features failed: %s",
			     strerror(errno));
		return -1;
	}

	features &= ~dev->device_features;
	/* For following bits, vhost-user doesn't really need to know */
	features &= ~(1ull << VIRTIO_NET_F_MAC);
	features &= ~(1ull << VIRTIO_NET_F_CTRL_VLAN);
	features &= ~(1ull << VIRTIO_NET_F_CTRL_MAC_ADDR);
	features &= ~(1ull << VIRTIO_NET_F_STATUS);
	if (features)
		PMD_INIT_LOG(ERR, "WARNING: Some features 0x%" PRIx64 " are not supported by vhost-user!",
			     features);

	dev->features &= dev->device_features;

	flag = fcntl(connectfd, F_GETFD);
	fcntl(connectfd, F_SETFL, flag | O_NONBLOCK);

	ret = virtio_user_start_device(dev);
	if (ret < 0)
		return -1;

	if (dev->queue_pairs > 1) {
		ret = virtio_user_handle_mq(dev, dev->queue_pairs);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Fails to enable multi-queue pairs!");
			return -1;
		}
	}
	if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC) {
		if (rte_intr_disable(eth_dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt disable failed");
			return -1;
		}
		rte_intr_callback_unregister(eth_dev->intr_handle,
					     virtio_interrupt_handler,
					     eth_dev);
		eth_dev->intr_handle->fd = connectfd;
		rte_intr_callback_register(eth_dev->intr_handle,
					   virtio_interrupt_handler, eth_dev);

		if (rte_intr_enable(eth_dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt enable failed");
			return -1;
		}
	}
	PMD_INIT_LOG(NOTICE, "server mode virtio-user reconnection succeeds!");
	return 0;
}

static void
virtio_user_delayed_handler(void *param)
{
	struct virtio_hw *hw = (struct virtio_hw *)param;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[hw->port_id];
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (rte_intr_disable(eth_dev->intr_handle) < 0) {
		PMD_DRV_LOG(ERR, "interrupt disable failed");
		return;
	}
	rte_intr_callback_unregister(eth_dev->intr_handle,
				     virtio_interrupt_handler, eth_dev);
	if (dev->is_server) {
		if (dev->vhostfd >= 0) {
			close(dev->vhostfd);
			dev->vhostfd = -1;
		}
		eth_dev->intr_handle->fd = dev->listenfd;
		rte_intr_callback_register(eth_dev->intr_handle,
					   virtio_interrupt_handler, eth_dev);
		if (rte_intr_enable(eth_dev->intr_handle) < 0) {
			PMD_DRV_LOG(ERR, "interrupt enable failed");
			return;
		}
	}
}

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

	if (offset == offsetof(struct virtio_net_config, status)) {
		char buf[128];

		if (dev->vhostfd >= 0) {
			int r;
			int flags;

			flags = fcntl(dev->vhostfd, F_GETFL);
			if (fcntl(dev->vhostfd, F_SETFL,
					flags | O_NONBLOCK) == -1) {
				PMD_DRV_LOG(ERR, "error setting O_NONBLOCK flag");
				return;
			}
			r = recv(dev->vhostfd, buf, 128, MSG_PEEK);
			if (r == 0 || (r < 0 && errno != EAGAIN)) {
				dev->status &= (~VIRTIO_NET_S_LINK_UP);
				PMD_DRV_LOG(ERR, "virtio-user port %u is down",
					    hw->port_id);

				/* This function could be called in the process
				 * of interrupt handling, callback cannot be
				 * unregistered here, set an alarm to do it.
				 */
				rte_eal_alarm_set(1,
						  virtio_user_delayed_handler,
						  (void *)hw);
			} else {
				dev->status |= VIRTIO_NET_S_LINK_UP;
			}
			if (fcntl(dev->vhostfd, F_SETFL,
					flags & ~O_NONBLOCK) == -1) {
				PMD_DRV_LOG(ERR, "error clearing O_NONBLOCK flag");
				return;
			}
		} else if (dev->is_server) {
			dev->status &= (~VIRTIO_NET_S_LINK_UP);
			if (virtio_user_server_reconnect(dev) >= 0)
				dev->status |= VIRTIO_NET_S_LINK_UP;
		}

		*(uint16_t *)dst = dev->status;
	}

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
		PMD_DRV_LOG(ERR, "not supported offset=%zu, len=%d",
			    offset, length);
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

	if (status & VIRTIO_CONFIG_STATUS_DRIVER_OK)
		virtio_user_start_device(dev);
	else if (status == VIRTIO_CONFIG_STATUS_RESET)
		virtio_user_reset(hw);
	dev->status = status;
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

	/* unmask feature bits defined in vhost user protocol */
	return dev->device_features & VIRTIO_PMD_SUPPORTED_GUEST_FEATURES;
}

static void
virtio_user_set_features(struct virtio_hw *hw, uint64_t features)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	dev->features = features & dev->device_features;
}

static uint8_t
virtio_user_get_isr(struct virtio_hw *hw __rte_unused)
{
	/* rxq interrupts and config interrupt are separated in virtio-user,
	 * here we only report config change.
	 */
	return VIRTIO_PCI_ISR_CONFIG;
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
		PMD_DRV_LOG(ERR, "failed to kick backend: %s",
			    strerror(errno));
}

const struct virtio_pci_ops virtio_user_ops = {
	.read_dev_cfg	= virtio_user_read_dev_config,
	.write_dev_cfg	= virtio_user_write_dev_config,
	.reset		= virtio_user_reset,
	.get_status	= virtio_user_get_status,
	.set_status	= virtio_user_set_status,
	.get_features	= virtio_user_get_features,
	.set_features	= virtio_user_set_features,
	.get_isr	= virtio_user_get_isr,
	.set_config_irq	= virtio_user_set_config_irq,
	.set_queue_irq	= virtio_user_set_queue_irq,
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
#define VIRTIO_USER_ARG_INTERFACE_NAME "iface"
	VIRTIO_USER_ARG_INTERFACE_NAME,
#define VIRTIO_USER_ARG_SERVER_MODE "server"
	VIRTIO_USER_ARG_SERVER_MODE,
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
	if (!value || !extra_args)
		return -EINVAL;

	*(uint64_t *)extra_args = strtoull(value, NULL, 0);

	return 0;
}

static struct rte_vdev_driver virtio_user_driver;

static struct rte_eth_dev *
virtio_user_eth_dev_alloc(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_data *data;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;

	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*hw));
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "cannot alloc rte_eth_dev");
		return NULL;
	}

	data = eth_dev->data;
	hw = eth_dev->data->dev_private;

	dev = rte_zmalloc(NULL, sizeof(*dev), 0);
	if (!dev) {
		PMD_INIT_LOG(ERR, "malloc virtio_user_dev failed");
		rte_eth_dev_release_port(eth_dev);
		rte_free(hw);
		return NULL;
	}

	hw->port_id = data->port_id;
	dev->port_id = data->port_id;
	virtio_hw_internal[hw->port_id].vtpci_ops = &virtio_user_ops;
	/*
	 * MSIX is required to enable LSC (see virtio_init_device).
	 * Here just pretend that we support msix.
	 */
	hw->use_msix = 1;
	hw->modern   = 0;
	hw->use_simple_rx = 0;
	hw->use_simple_tx = 0;
	hw->virtio_user_dev = dev;
	return eth_dev;
}

static void
virtio_user_eth_dev_free(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_dev_data *data = eth_dev->data;
	struct virtio_hw *hw = data->dev_private;

	rte_free(hw->virtio_user_dev);
	rte_free(hw);
	rte_eth_dev_release_port(eth_dev);
}

/* Dev initialization routine. Invoked once for each virtio vdev at
 * EAL init time, see rte_bus_probe().
 * Returns 0 on success.
 */
static int
virtio_user_pmd_probe(struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	struct rte_eth_dev *eth_dev;
	struct virtio_hw *hw;
	uint64_t queues = VIRTIO_USER_DEF_Q_NUM;
	uint64_t cq = VIRTIO_USER_DEF_CQ_EN;
	uint64_t queue_size = VIRTIO_USER_DEF_Q_SZ;
	uint64_t server_mode = VIRTIO_USER_DEF_SERVER_MODE;
	char *path = NULL;
	char *ifname = NULL;
	char *mac_addr = NULL;
	int ret = -1;

	kvlist = rte_kvargs_parse(rte_vdev_device_args(dev), valid_args);
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
			  VIRTIO_USER_ARG_QUEUE_SIZE);
		goto end;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_INTERFACE_NAME) == 1) {
		if (is_vhost_user_by_type(path)) {
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
	} else if (queues > 1) {
		cq = 1;
	}

	if (queues > 1 && cq == 0) {
		PMD_INIT_LOG(ERR, "multi-q requires ctrl-q");
		goto end;
	}

	if (queues > VIRTIO_MAX_VIRTQUEUE_PAIRS) {
		PMD_INIT_LOG(ERR, "arg %s %" PRIu64 " exceeds the limit %u",
			VIRTIO_USER_ARG_QUEUES_NUM, queues,
			VIRTIO_MAX_VIRTQUEUE_PAIRS);
		goto end;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		struct virtio_user_dev *vu_dev;

		eth_dev = virtio_user_eth_dev_alloc(dev);
		if (!eth_dev) {
			PMD_INIT_LOG(ERR, "virtio_user fails to alloc device");
			goto end;
		}

		hw = eth_dev->data->dev_private;
		vu_dev = virtio_user_get_dev(hw);
		if (server_mode == 1)
			vu_dev->is_server = true;
		else
			vu_dev->is_server = false;
		if (virtio_user_dev_init(hw->virtio_user_dev, path, queues, cq,
				 queue_size, mac_addr, &ifname) < 0) {
			PMD_INIT_LOG(ERR, "virtio_user_dev_init fails");
			virtio_user_eth_dev_free(eth_dev);
			goto end;
		}

	} else {
		eth_dev = rte_eth_dev_attach_secondary(rte_vdev_device_name(dev));
		if (!eth_dev)
			goto end;
	}

	/* previously called by rte_pci_probe() for physical dev */
	if (eth_virtio_dev_init(eth_dev) < 0) {
		PMD_INIT_LOG(ERR, "eth_virtio_dev_init fails");
		virtio_user_eth_dev_free(eth_dev);
		goto end;
	}

	rte_eth_dev_probing_finish(eth_dev);
	ret = 0;

end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (path)
		free(path);
	if (mac_addr)
		free(mac_addr);
	if (ifname)
		free(ifname);
	return ret;
}

/** Called by rte_eth_dev_detach() */
static int
virtio_user_pmd_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eth_dev *eth_dev;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Un-Initializing %s", name);
	eth_dev = rte_eth_dev_allocated(name);
	if (!eth_dev)
		return -ENODEV;

	/* make sure the device is stopped, queues freed */
	rte_eth_dev_close(eth_dev->data->port_id);

	hw = eth_dev->data->dev_private;
	dev = hw->virtio_user_dev;
	virtio_user_dev_uninit(dev);

	rte_free(eth_dev->data->dev_private);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_vdev_driver virtio_user_driver = {
	.probe = virtio_user_pmd_probe,
	.remove = virtio_user_pmd_remove,
};

RTE_PMD_REGISTER_VDEV(net_virtio_user, virtio_user_driver);
RTE_PMD_REGISTER_ALIAS(net_virtio_user, virtio_user);
RTE_PMD_REGISTER_PARAM_STRING(net_virtio_user,
	"path=<path> "
	"mac=<mac addr> "
	"cq=<int> "
	"queue_size=<int> "
	"queues=<int> "
	"iface=<string>");
