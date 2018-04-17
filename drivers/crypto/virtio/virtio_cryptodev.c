/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
#include <rte_errno.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_eal.h>

#include "virtio_cryptodev.h"
#include "virtqueue.h"

int virtio_crypto_logtype_init;
int virtio_crypto_logtype_session;
int virtio_crypto_logtype_rx;
int virtio_crypto_logtype_tx;
int virtio_crypto_logtype_driver;

static int virtio_crypto_dev_configure(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);
static int virtio_crypto_dev_start(struct rte_cryptodev *dev);
static void virtio_crypto_dev_stop(struct rte_cryptodev *dev);
static int virtio_crypto_dev_close(struct rte_cryptodev *dev);
static void virtio_crypto_dev_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info);
static int virtio_crypto_qp_setup(struct rte_cryptodev *dev,
		uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id,
		struct rte_mempool *session_pool);
static int virtio_crypto_qp_release(struct rte_cryptodev *dev,
		uint16_t queue_pair_id);
static void virtio_crypto_dev_free_mbufs(struct rte_cryptodev *dev);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_crypto_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_CRYPTO_PCI_VENDORID,
				VIRTIO_CRYPTO_PCI_DEVICEID) },
	{ .vendor_id = 0, /* sentinel */ },
};

uint8_t cryptodev_virtio_driver_id;

void
virtio_crypto_queue_release(struct virtqueue *vq)
{
	struct virtio_crypto_hw *hw;

	PMD_INIT_FUNC_TRACE();

	if (vq) {
		hw = vq->hw;
		/* Select and deactivate the queue */
		VTPCI_OPS(hw)->del_queue(hw, vq);

		rte_memzone_free(vq->mz);
		rte_mempool_free(vq->mpool);
		rte_free(vq);
	}
}

#define MPOOL_MAX_NAME_SZ 32

int
virtio_crypto_queue_setup(struct rte_cryptodev *dev,
		int queue_type,
		uint16_t vtpci_queue_idx,
		uint16_t nb_desc,
		int socket_id,
		struct virtqueue **pvq)
{
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	char mpool_name[MPOOL_MAX_NAME_SZ];
	const struct rte_memzone *mz;
	unsigned int vq_size, size;
	struct virtio_crypto_hw *hw = dev->data->dev_private;
	struct virtqueue *vq = NULL;
	uint32_t i = 0;
	uint32_t j;

	PMD_INIT_FUNC_TRACE();

	VIRTIO_CRYPTO_INIT_LOG_DBG("setting up queue: %u", vtpci_queue_idx);

	/*
	 * Read the virtqueue size from the Queue Size field
	 * Always power of 2 and if 0 virtqueue does not exist
	 */
	vq_size = VTPCI_OPS(hw)->get_queue_num(hw, vtpci_queue_idx);
	if (vq_size == 0) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("virtqueue does not exist");
		return -EINVAL;
	}
	VIRTIO_CRYPTO_INIT_LOG_DBG("vq_size: %u", vq_size);

	if (!rte_is_power_of_2(vq_size)) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("virtqueue size is not powerof 2");
		return -EINVAL;
	}

	if (queue_type == VTCRYPTO_DATAQ) {
		snprintf(vq_name, sizeof(vq_name), "dev%d_dataqueue%d",
				dev->data->dev_id, vtpci_queue_idx);
		snprintf(mpool_name, sizeof(mpool_name),
				"dev%d_dataqueue%d_mpool",
				dev->data->dev_id, vtpci_queue_idx);
	} else if (queue_type == VTCRYPTO_CTRLQ) {
		snprintf(vq_name, sizeof(vq_name), "dev%d_controlqueue",
				dev->data->dev_id);
		snprintf(mpool_name, sizeof(mpool_name),
				"dev%d_controlqueue_mpool",
				dev->data->dev_id);
	}
	size = RTE_ALIGN_CEIL(sizeof(*vq) +
				vq_size * sizeof(struct vq_desc_extra),
				RTE_CACHE_LINE_SIZE);
	vq = rte_zmalloc_socket(vq_name, size, RTE_CACHE_LINE_SIZE,
				socket_id);
	if (vq == NULL) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("Can not allocate virtqueue");
		return -ENOMEM;
	}

	if (queue_type == VTCRYPTO_DATAQ) {
		/* pre-allocate a mempool and use it in the data plane to
		 * improve performance
		 */
		vq->mpool = rte_mempool_lookup(mpool_name);
		if (vq->mpool == NULL)
			vq->mpool = rte_mempool_create(mpool_name,
					vq_size,
					sizeof(struct virtio_crypto_op_cookie),
					RTE_CACHE_LINE_SIZE, 0,
					NULL, NULL, NULL, NULL, socket_id,
					0);
		if (!vq->mpool) {
			VIRTIO_CRYPTO_DRV_LOG_ERR("Virtio Crypto PMD "
					"Cannot create mempool");
			goto mpool_create_err;
		}
		for (i = 0; i < vq_size; i++) {
			vq->vq_descx[i].cookie =
				rte_zmalloc("crypto PMD op cookie pointer",
					sizeof(struct virtio_crypto_op_cookie),
					RTE_CACHE_LINE_SIZE);
			if (vq->vq_descx[i].cookie == NULL) {
				VIRTIO_CRYPTO_DRV_LOG_ERR("Failed to "
						"alloc mem for cookie");
				goto cookie_alloc_err;
			}
		}
	}

	vq->hw = hw;
	vq->dev_id = dev->data->dev_id;
	vq->vq_queue_index = vtpci_queue_idx;
	vq->vq_nentries = vq_size;

	/*
	 * Using part of the vring entries is permitted, but the maximum
	 * is vq_size
	 */
	if (nb_desc == 0 || nb_desc > vq_size)
		nb_desc = vq_size;
	vq->vq_free_cnt = nb_desc;

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(vq_size, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	VIRTIO_CRYPTO_INIT_LOG_DBG("%s vring_size: %d, rounded_vring_size: %d",
			(queue_type == VTCRYPTO_DATAQ) ? "dataq" : "ctrlq",
			size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
			socket_id, 0, VIRTIO_PCI_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			VIRTIO_CRYPTO_INIT_LOG_ERR("not enough memory");
			goto mz_reserve_err;
		}
	}

	/*
	 * Virtio PCI device VIRTIO_PCI_QUEUE_PF register is 32bit,
	 * and only accepts 32 bit page frame number.
	 * Check if the allocated physical memory exceeds 16TB.
	 */
	if ((mz->phys_addr + vq->vq_ring_size - 1)
				>> (VIRTIO_PCI_QUEUE_ADDR_SHIFT + 32)) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("vring address shouldn't be "
					"above 16TB!");
		goto vring_addr_err;
	}

	memset(mz->addr, 0, sizeof(mz->len));
	vq->mz = mz;
	vq->vq_ring_mem = mz->phys_addr;
	vq->vq_ring_virt_mem = mz->addr;
	VIRTIO_CRYPTO_INIT_LOG_DBG("vq->vq_ring_mem(physical): 0x%"PRIx64,
					(uint64_t)mz->phys_addr);
	VIRTIO_CRYPTO_INIT_LOG_DBG("vq->vq_ring_virt_mem: 0x%"PRIx64,
					(uint64_t)(uintptr_t)mz->addr);

	*pvq = vq;

	return 0;

vring_addr_err:
	rte_memzone_free(mz);
mz_reserve_err:
cookie_alloc_err:
	rte_mempool_free(vq->mpool);
	if (i != 0) {
		for (j = 0; j < i; j++)
			rte_free(vq->vq_descx[j].cookie);
	}
mpool_create_err:
	rte_free(vq);
	return -ENOMEM;
}

static int
virtio_crypto_ctrlq_setup(struct rte_cryptodev *dev, uint16_t queue_idx)
{
	int ret;
	struct virtqueue *vq;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	/* if virtio device has started, do not touch the virtqueues */
	if (dev->data->dev_started)
		return 0;

	PMD_INIT_FUNC_TRACE();

	ret = virtio_crypto_queue_setup(dev, VTCRYPTO_CTRLQ, queue_idx,
			0, SOCKET_ID_ANY, &vq);
	if (ret < 0) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("control vq initialization failed");
		return ret;
	}

	hw->cvq = vq;

	return 0;
}

static void
virtio_crypto_free_queues(struct rte_cryptodev *dev)
{
	unsigned int i;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	/* control queue release */
	virtio_crypto_queue_release(hw->cvq);

	/* data queue release */
	for (i = 0; i < hw->max_dataqueues; i++)
		virtio_crypto_queue_release(dev->data->queue_pairs[i]);
}

static int
virtio_crypto_dev_close(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

/*
 * dev_ops for virtio, bare necessities for basic operation
 */
static struct rte_cryptodev_ops virtio_crypto_dev_ops = {
	/* Device related operations */
	.dev_configure			 = virtio_crypto_dev_configure,
	.dev_start			 = virtio_crypto_dev_start,
	.dev_stop			 = virtio_crypto_dev_stop,
	.dev_close			 = virtio_crypto_dev_close,
	.dev_infos_get			 = virtio_crypto_dev_info_get,

	.stats_get			 = NULL,
	.stats_reset			 = NULL,

	.queue_pair_setup                = virtio_crypto_qp_setup,
	.queue_pair_release              = virtio_crypto_qp_release,
	.queue_pair_start                = NULL,
	.queue_pair_stop                 = NULL,
	.queue_pair_count                = NULL,

	/* Crypto related operations */
	.session_get_size	= NULL,
	.session_configure	= NULL,
	.session_clear		= NULL,
	.qp_attach_session = NULL,
	.qp_detach_session = NULL
};

static int
virtio_crypto_qp_setup(struct rte_cryptodev *dev, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id,
		struct rte_mempool *session_pool __rte_unused)
{
	int ret;
	struct virtqueue *vq;

	PMD_INIT_FUNC_TRACE();

	/* if virtio dev is started, do not touch the virtqueues */
	if (dev->data->dev_started)
		return 0;

	ret = virtio_crypto_queue_setup(dev, VTCRYPTO_DATAQ, queue_pair_id,
			qp_conf->nb_descriptors, socket_id, &vq);
	if (ret < 0) {
		VIRTIO_CRYPTO_INIT_LOG_ERR(
			"virtio crypto data queue initialization failed\n");
		return ret;
	}

	dev->data->queue_pairs[queue_pair_id] = vq;

	return 0;
}

static int
virtio_crypto_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	struct virtqueue *vq
		= (struct virtqueue *)dev->data->queue_pairs[queue_pair_id];

	PMD_INIT_FUNC_TRACE();

	if (vq == NULL) {
		VIRTIO_CRYPTO_DRV_LOG_DBG("vq already freed");
		return 0;
	}

	virtio_crypto_queue_release(vq);
	return 0;
}

static int
virtio_negotiate_features(struct virtio_crypto_hw *hw, uint64_t req_features)
{
	uint64_t host_features;

	PMD_INIT_FUNC_TRACE();

	/* Prepare guest_features: feature that driver wants to support */
	VIRTIO_CRYPTO_INIT_LOG_DBG("guest_features before negotiate = %" PRIx64,
		req_features);

	/* Read device(host) feature bits */
	host_features = VTPCI_OPS(hw)->get_features(hw);
	VIRTIO_CRYPTO_INIT_LOG_DBG("host_features before negotiate = %" PRIx64,
		host_features);

	/*
	 * Negotiate features: Subset of device feature bits are written back
	 * guest feature bits.
	 */
	hw->guest_features = req_features;
	hw->guest_features = vtpci_cryptodev_negotiate_features(hw,
							host_features);
	VIRTIO_CRYPTO_INIT_LOG_DBG("features after negotiate = %" PRIx64,
		hw->guest_features);

	if (hw->modern) {
		if (!vtpci_with_feature(hw, VIRTIO_F_VERSION_1)) {
			VIRTIO_CRYPTO_INIT_LOG_ERR(
				"VIRTIO_F_VERSION_1 features is not enabled.");
			return -1;
		}
		vtpci_cryptodev_set_status(hw,
			VIRTIO_CONFIG_STATUS_FEATURES_OK);
		if (!(vtpci_cryptodev_get_status(hw) &
			VIRTIO_CONFIG_STATUS_FEATURES_OK)) {
			VIRTIO_CRYPTO_INIT_LOG_ERR("failed to set FEATURES_OK "
						"status!");
			return -1;
		}
	}

	hw->req_guest_features = req_features;

	return 0;
}

/* reset device and renegotiate features if needed */
static int
virtio_crypto_init_device(struct rte_cryptodev *cryptodev,
	uint64_t req_features)
{
	struct virtio_crypto_hw *hw = cryptodev->data->dev_private;
	struct virtio_crypto_config local_config;
	struct virtio_crypto_config *config = &local_config;

	PMD_INIT_FUNC_TRACE();

	/* Reset the device although not necessary at startup */
	vtpci_cryptodev_reset(hw);

	/* Tell the host we've noticed this device. */
	vtpci_cryptodev_set_status(hw, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the host we've known how to drive the device. */
	vtpci_cryptodev_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER);
	if (virtio_negotiate_features(hw, req_features) < 0)
		return -1;

	/* Get status of the device */
	vtpci_read_cryptodev_config(hw,
		offsetof(struct virtio_crypto_config, status),
		&config->status, sizeof(config->status));
	if (config->status != VIRTIO_CRYPTO_S_HW_READY) {
		VIRTIO_CRYPTO_DRV_LOG_ERR("accelerator hardware is "
				"not ready");
		return -1;
	}

	/* Get number of data queues */
	vtpci_read_cryptodev_config(hw,
		offsetof(struct virtio_crypto_config, max_dataqueues),
		&config->max_dataqueues,
		sizeof(config->max_dataqueues));
	hw->max_dataqueues = config->max_dataqueues;

	VIRTIO_CRYPTO_INIT_LOG_DBG("hw->max_dataqueues=%d",
		hw->max_dataqueues);

	return 0;
}

/*
 * This function is based on probe() function
 * It returns 0 on success.
 */
static int
crypto_virtio_create(const char *name, struct rte_pci_device *pci_dev,
		struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *cryptodev;
	struct virtio_crypto_hw *hw;

	PMD_INIT_FUNC_TRACE();

	cryptodev = rte_cryptodev_pmd_create(name, &pci_dev->device,
					init_params);
	if (cryptodev == NULL)
		return -ENODEV;

	cryptodev->driver_id = cryptodev_virtio_driver_id;
	cryptodev->dev_ops = &virtio_crypto_dev_ops;

	cryptodev->enqueue_burst = virtio_crypto_pkt_tx_burst;
	cryptodev->dequeue_burst = virtio_crypto_pkt_rx_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING;

	hw = cryptodev->data->dev_private;
	hw->dev_id = cryptodev->data->dev_id;

	VIRTIO_CRYPTO_INIT_LOG_DBG("dev %d vendorID=0x%x deviceID=0x%x",
		cryptodev->data->dev_id, pci_dev->id.vendor_id,
		pci_dev->id.device_id);

	/* pci device init */
	if (vtpci_cryptodev_init(pci_dev, hw))
		return -1;

	if (virtio_crypto_init_device(cryptodev,
			VIRTIO_CRYPTO_PMD_GUEST_FEATURES) < 0)
		return -1;

	return 0;
}

static int
virtio_crypto_dev_uninit(struct rte_cryptodev *cryptodev)
{
	struct virtio_crypto_hw *hw = cryptodev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return -EPERM;

	if (cryptodev->data->dev_started) {
		virtio_crypto_dev_stop(cryptodev);
		virtio_crypto_dev_close(cryptodev);
	}

	cryptodev->dev_ops = NULL;
	cryptodev->enqueue_burst = NULL;
	cryptodev->dequeue_burst = NULL;

	/* release control queue */
	virtio_crypto_queue_release(hw->cvq);

	rte_free(cryptodev->data);
	cryptodev->data = NULL;

	VIRTIO_CRYPTO_DRV_LOG_INFO("dev_uninit completed");

	return 0;
}

static int
virtio_crypto_dev_configure(struct rte_cryptodev *cryptodev,
	struct rte_cryptodev_config *config __rte_unused)
{
	struct virtio_crypto_hw *hw = cryptodev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (virtio_crypto_init_device(cryptodev,
			VIRTIO_CRYPTO_PMD_GUEST_FEATURES) < 0)
		return -1;

	/* setup control queue
	 * [0, 1, ... ,(config->max_dataqueues - 1)] are data queues
	 * config->max_dataqueues is the control queue
	 */
	if (virtio_crypto_ctrlq_setup(cryptodev, hw->max_dataqueues) < 0) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("control queue setup error");
		return -1;
	}
	virtio_crypto_ctrlq_start(cryptodev);

	return 0;
}

static void
virtio_crypto_dev_stop(struct rte_cryptodev *dev)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	VIRTIO_CRYPTO_DRV_LOG_DBG("virtio_dev_stop");

	vtpci_cryptodev_reset(hw);

	virtio_crypto_dev_free_mbufs(dev);
	virtio_crypto_free_queues(dev);

	dev->data->dev_started = 0;
}

static int
virtio_crypto_dev_start(struct rte_cryptodev *dev)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	if (dev->data->dev_started)
		return 0;

	/* Do final configuration before queue engine starts */
	virtio_crypto_dataq_start(dev);
	vtpci_cryptodev_reinit_complete(hw);

	dev->data->dev_started = 1;

	return 0;
}

static void
virtio_crypto_dev_free_mbufs(struct rte_cryptodev *dev)
{
	uint32_t i;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	for (i = 0; i < hw->max_dataqueues; i++) {
		VIRTIO_CRYPTO_INIT_LOG_DBG("Before freeing dataq[%d] used "
			"and unused buf", i);
		VIRTQUEUE_DUMP((struct virtqueue *)
			dev->data->queue_pairs[i]);

		VIRTIO_CRYPTO_INIT_LOG_DBG("queue_pairs[%d]=%p",
				i, dev->data->queue_pairs[i]);

		virtqueue_detatch_unused(dev->data->queue_pairs[i]);

		VIRTIO_CRYPTO_INIT_LOG_DBG("After freeing dataq[%d] used and "
					"unused buf", i);
		VIRTQUEUE_DUMP(
			(struct virtqueue *)dev->data->queue_pairs[i]);
	}
}

static void
virtio_crypto_dev_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *info)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (info != NULL) {
		info->driver_id = cryptodev_virtio_driver_id;
		info->pci_dev = RTE_DEV_TO_PCI(dev->device);
		info->feature_flags = dev->feature_flags;
		info->max_nb_queue_pairs = hw->max_dataqueues;
		info->sym.max_nb_sessions =
			RTE_VIRTIO_CRYPTO_PMD_MAX_NB_SESSIONS;
	}
}

static int
crypto_virtio_pci_probe(
	struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = rte_socket_id(),
		.private_data_size = sizeof(struct virtio_crypto_hw),
		.max_nb_sessions = RTE_VIRTIO_CRYPTO_PMD_MAX_NB_SESSIONS
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];

	VIRTIO_CRYPTO_DRV_LOG_DBG("Found Crypto device at %02x:%02x.%x",
			pci_dev->addr.bus,
			pci_dev->addr.devid,
			pci_dev->addr.function);

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return crypto_virtio_create(name, pci_dev, &init_params);
}

static int
crypto_virtio_pci_remove(
	struct rte_pci_device *pci_dev __rte_unused)
{
	struct rte_cryptodev *cryptodev;
	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, cryptodev_name,
			sizeof(cryptodev_name));

	cryptodev = rte_cryptodev_pmd_get_named_dev(cryptodev_name);
	if (cryptodev == NULL)
		return -ENODEV;

	return virtio_crypto_dev_uninit(cryptodev);
}

static struct rte_pci_driver rte_virtio_crypto_driver = {
	.id_table = pci_id_virtio_crypto_map,
	.drv_flags = 0,
	.probe = crypto_virtio_pci_probe,
	.remove = crypto_virtio_pci_remove
};

static struct cryptodev_driver virtio_crypto_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_VIRTIO_PMD, rte_virtio_crypto_driver);
RTE_PMD_REGISTER_CRYPTO_DRIVER(virtio_crypto_drv,
	rte_virtio_crypto_driver.driver,
	cryptodev_virtio_driver_id);

RTE_INIT(virtio_crypto_init_log);
static void
virtio_crypto_init_log(void)
{
	virtio_crypto_logtype_init = rte_log_register("pmd.crypto.virtio.init");
	if (virtio_crypto_logtype_init >= 0)
		rte_log_set_level(virtio_crypto_logtype_init, RTE_LOG_NOTICE);

	virtio_crypto_logtype_session =
		rte_log_register("pmd.crypto.virtio.session");
	if (virtio_crypto_logtype_session >= 0)
		rte_log_set_level(virtio_crypto_logtype_session,
				RTE_LOG_NOTICE);

	virtio_crypto_logtype_rx = rte_log_register("pmd.crypto.virtio.rx");
	if (virtio_crypto_logtype_rx >= 0)
		rte_log_set_level(virtio_crypto_logtype_rx, RTE_LOG_NOTICE);

	virtio_crypto_logtype_tx = rte_log_register("pmd.crypto.virtio.tx");
	if (virtio_crypto_logtype_tx >= 0)
		rte_log_set_level(virtio_crypto_logtype_tx, RTE_LOG_NOTICE);

	virtio_crypto_logtype_driver =
		rte_log_register("pmd.crypto.virtio.driver");
	if (virtio_crypto_logtype_driver >= 0)
		rte_log_set_level(virtio_crypto_logtype_driver, RTE_LOG_NOTICE);
}
