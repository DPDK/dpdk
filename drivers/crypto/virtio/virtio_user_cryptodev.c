/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell
 */

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <bus_vdev_driver.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_io.h>

#include "virtio_user/virtio_user_dev.h"
#include "virtio_user/vhost.h"
#include "virtio_cryptodev.h"
#include "virtio_logs.h"
#include "virtio_pci.h"
#include "virtqueue.h"

#define virtio_user_get_dev(hwp) container_of(hwp, struct virtio_user_dev, hw)

uint8_t cryptodev_virtio_user_driver_id;

static void
virtio_user_read_dev_config(struct virtio_crypto_hw *hw, size_t offset,
		     void *dst, int length __rte_unused)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (offset == offsetof(struct virtio_crypto_config, status)) {
		crypto_virtio_user_dev_update_link_state(dev);
		*(uint32_t *)dst = dev->crypto_status;
	} else if (offset == offsetof(struct virtio_crypto_config, max_dataqueues))
		*(uint16_t *)dst = dev->max_queue_pairs;
	else if (offset == offsetof(struct virtio_crypto_config, crypto_services))
		*(uint32_t *)dst = dev->crypto_services;
	else if (offset == offsetof(struct virtio_crypto_config, cipher_algo_l))
		*(uint32_t *)dst = dev->cipher_algo & 0xFFFF;
	else if (offset == offsetof(struct virtio_crypto_config, cipher_algo_h))
		*(uint32_t *)dst = dev->cipher_algo >> 32;
	else if (offset == offsetof(struct virtio_crypto_config, hash_algo))
		*(uint32_t *)dst = dev->hash_algo;
	else if (offset == offsetof(struct virtio_crypto_config, mac_algo_l))
		*(uint32_t *)dst = dev->auth_algo & 0xFFFF;
	else if (offset == offsetof(struct virtio_crypto_config, mac_algo_h))
		*(uint32_t *)dst = dev->auth_algo >> 32;
	else if (offset == offsetof(struct virtio_crypto_config, aead_algo))
		*(uint32_t *)dst = dev->aead_algo;
	else if (offset == offsetof(struct virtio_crypto_config, akcipher_algo))
		*(uint32_t *)dst = dev->akcipher_algo;
}

static void
virtio_user_write_dev_config(struct virtio_crypto_hw *hw, size_t offset,
		      const void *src, int length)
{
	RTE_SET_USED(hw);
	RTE_SET_USED(src);

	PMD_DRV_LOG(ERR, "not supported offset=%zu, len=%d",
		    offset, length);
}

static void
virtio_user_reset(struct virtio_crypto_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (dev->status & VIRTIO_CONFIG_STATUS_DRIVER_OK)
		crypto_virtio_user_stop_device(dev);
}

static void
virtio_user_set_status(struct virtio_crypto_hw *hw, uint8_t status)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);
	uint8_t old_status = dev->status;

	if (status & VIRTIO_CONFIG_STATUS_FEATURES_OK &&
			~old_status & VIRTIO_CONFIG_STATUS_FEATURES_OK) {
		crypto_virtio_user_dev_set_features(dev);
		/* Feature negotiation should be only done in probe time.
		 * So we skip any more request here.
		 */
		dev->status |= VIRTIO_CONFIG_STATUS_FEATURES_OK;
	}

	if (status & VIRTIO_CONFIG_STATUS_DRIVER_OK) {
		if (crypto_virtio_user_start_device(dev)) {
			crypto_virtio_user_dev_update_status(dev);
			return;
		}
	} else if (status == VIRTIO_CONFIG_STATUS_RESET) {
		virtio_user_reset(hw);
	}

	crypto_virtio_user_dev_set_status(dev, status);
	if (status & VIRTIO_CONFIG_STATUS_DRIVER_OK && dev->scvq) {
		if (dev->ops->cvq_enable(dev, 1) < 0) {
			PMD_INIT_LOG(ERR, "(%s) Failed to start ctrlq", dev->path);
			crypto_virtio_user_dev_update_status(dev);
			return;
		}
	}
}

static uint8_t
virtio_user_get_status(struct virtio_crypto_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	crypto_virtio_user_dev_update_status(dev);

	return dev->status;
}

#define VIRTIO_USER_CRYPTO_PMD_GUEST_FEATURES   \
	(1ULL << VIRTIO_CRYPTO_SERVICE_CIPHER     | \
	 1ULL << VIRTIO_CRYPTO_SERVICE_AKCIPHER   | \
	 1ULL << VIRTIO_F_VERSION_1               | \
	 1ULL << VIRTIO_F_IN_ORDER                | \
	 1ULL << VIRTIO_F_RING_PACKED             | \
	 1ULL << VIRTIO_F_NOTIFICATION_DATA       | \
	 1ULL << VIRTIO_RING_F_INDIRECT_DESC      | \
	 1ULL << VIRTIO_F_ORDER_PLATFORM)

static uint64_t
virtio_user_get_features(struct virtio_crypto_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	/* unmask feature bits defined in vhost user protocol */
	return (dev->device_features | dev->frontend_features) &
		VIRTIO_USER_CRYPTO_PMD_GUEST_FEATURES;
}

static void
virtio_user_set_features(struct virtio_crypto_hw *hw, uint64_t features)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	dev->features = features & (dev->device_features | dev->frontend_features);
}

static uint8_t
virtio_user_get_isr(struct virtio_crypto_hw *hw __rte_unused)
{
	/* rxq interrupts and config interrupt are separated in virtio-user,
	 * here we only report config change.
	 */
	return VIRTIO_PCI_CAP_ISR_CFG;
}

static uint16_t
virtio_user_set_config_irq(struct virtio_crypto_hw *hw __rte_unused,
		    uint16_t vec __rte_unused)
{
	return 0;
}

static uint16_t
virtio_user_set_queue_irq(struct virtio_crypto_hw *hw __rte_unused,
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
virtio_user_get_queue_num(struct virtio_crypto_hw *hw, uint16_t queue_id __rte_unused)
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
virtio_user_setup_queue(struct virtio_crypto_hw *hw, struct virtqueue *vq)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (vtpci_with_packed_queue(hw))
		virtio_user_setup_queue_packed(vq, dev);
	else
		virtio_user_setup_queue_split(vq, dev);

	if (dev->notify_area)
		vq->notify_addr = dev->notify_area[vq->vq_queue_index];

	if (virtcrypto_cq_to_vq(hw->cvq) == vq)
		dev->scvq = virtcrypto_cq_to_vq(hw->cvq);

	return 0;
}

static void
virtio_user_del_queue(struct virtio_crypto_hw *hw, struct virtqueue *vq)
{
	RTE_SET_USED(hw);
	RTE_SET_USED(vq);
}

static void
virtio_user_notify_queue(struct virtio_crypto_hw *hw, struct virtqueue *vq)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);
	uint64_t notify_data = 1;

	if (!dev->notify_area) {
		if (write(dev->kickfds[vq->vq_queue_index], &notify_data,
			  sizeof(notify_data)) < 0)
			PMD_DRV_LOG(ERR, "failed to kick backend: %s",
				    strerror(errno));
		return;
	} else if (!vtpci_with_feature(hw, VIRTIO_F_NOTIFICATION_DATA)) {
		rte_write16(vq->vq_queue_index, vq->notify_addr);
		return;
	}

	if (vtpci_with_packed_queue(hw)) {
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

const struct virtio_pci_ops crypto_virtio_user_ops = {
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

static const char * const valid_args[] = {
#define VIRTIO_USER_ARG_QUEUES_NUM     "queues"
	VIRTIO_USER_ARG_QUEUES_NUM,
#define VIRTIO_USER_ARG_QUEUE_SIZE     "queue_size"
	VIRTIO_USER_ARG_QUEUE_SIZE,
#define VIRTIO_USER_ARG_PATH           "path"
	VIRTIO_USER_ARG_PATH,
	NULL
};

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

static struct rte_cryptodev *
virtio_user_cryptodev_alloc(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.private_data_size = sizeof(struct virtio_user_dev),
	};
	struct rte_cryptodev_data *data;
	struct rte_cryptodev *cryptodev;
	struct virtio_user_dev *dev;
	struct virtio_crypto_hw *hw;

	init_params.socket_id = vdev->device.numa_node;
	init_params.private_data_size = sizeof(struct virtio_user_dev);
	cryptodev = rte_cryptodev_pmd_create(vdev->device.name, &vdev->device, &init_params);
	if (cryptodev == NULL) {
		PMD_INIT_LOG(ERR, "failed to create cryptodev vdev");
		return NULL;
	}

	data = cryptodev->data;
	dev = data->dev_private;
	hw = &dev->hw;

	hw->dev_id = data->dev_id;
	VTPCI_OPS(hw) = &crypto_virtio_user_ops;

	return cryptodev;
}

static void
virtio_user_cryptodev_free(struct rte_cryptodev *cryptodev)
{
	rte_cryptodev_pmd_destroy(cryptodev);
}

static int
virtio_user_pmd_probe(struct rte_vdev_device *vdev)
{
	uint64_t server_mode = VIRTIO_USER_DEF_SERVER_MODE;
	uint64_t queue_size = VIRTIO_USER_DEF_Q_SZ;
	uint64_t queues = VIRTIO_USER_DEF_Q_NUM;
	struct rte_cryptodev *cryptodev = NULL;
	struct rte_kvargs *kvlist = NULL;
	struct virtio_user_dev *dev;
	char *path = NULL;
	int ret = -1;

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

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUES_NUM) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUES_NUM,
					&get_integer_arg, &queues) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
					VIRTIO_USER_ARG_QUEUES_NUM);
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

	cryptodev = virtio_user_cryptodev_alloc(vdev);
	if (!cryptodev) {
		PMD_INIT_LOG(ERR, "virtio_user fails to alloc device");
		goto end;
	}

	dev = cryptodev->data->dev_private;
	if (crypto_virtio_user_dev_init(dev, path, queues, queue_size,
			server_mode) < 0) {
		PMD_INIT_LOG(ERR, "virtio_user_dev_init fails");
		virtio_user_cryptodev_free(cryptodev);
		goto end;
	}

	cryptodev->driver_id = cryptodev_virtio_user_driver_id;
	if (crypto_virtio_dev_init(cryptodev, VIRTIO_USER_CRYPTO_PMD_GUEST_FEATURES,
			NULL) < 0) {
		PMD_INIT_LOG(ERR, "crypto_virtio_dev_init fails");
		crypto_virtio_user_dev_uninit(dev);
		virtio_user_cryptodev_free(cryptodev);
		goto end;
	}

	rte_cryptodev_pmd_probing_finish(cryptodev);

	ret = 0;
end:
	rte_kvargs_free(kvlist);
	free(path);
	return ret;
}

static int
virtio_user_pmd_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;
	int devid;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Removing %s", name);

	devid = rte_cryptodev_get_dev_id(name);
	if (devid < 0)
		return -EINVAL;

	rte_cryptodev_stop(devid);

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	if (rte_cryptodev_pmd_destroy(cryptodev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to remove %s", name);
		return -EFAULT;
	}

	return 0;
}

static int virtio_user_pmd_dma_map(struct rte_vdev_device *vdev, void *addr,
		uint64_t iova, size_t len)
{
	struct rte_cryptodev *cryptodev;
	struct virtio_user_dev *dev;
	const char *name;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -EINVAL;

	dev = cryptodev->data->dev_private;

	if (dev->ops->dma_map)
		return dev->ops->dma_map(dev, addr, iova, len);

	return 0;
}

static int virtio_user_pmd_dma_unmap(struct rte_vdev_device *vdev, void *addr,
		uint64_t iova, size_t len)
{
	struct rte_cryptodev *cryptodev;
	struct virtio_user_dev *dev;
	const char *name;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -EINVAL;

	dev = cryptodev->data->dev_private;

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

static struct cryptodev_driver virtio_crypto_drv;

RTE_PMD_REGISTER_VDEV(crypto_virtio_user, virtio_user_driver);
RTE_PMD_REGISTER_CRYPTO_DRIVER(virtio_crypto_drv,
	virtio_user_driver.driver,
	cryptodev_virtio_user_driver_id);
RTE_PMD_REGISTER_PARAM_STRING(crypto_virtio_user,
	"path=<path> "
	"queues=<int> "
	"queue_size=<int>");
