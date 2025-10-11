/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
#include <stdbool.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_eal.h>

#include "virtio_cryptodev.h"
#include "virtqueue.h"
#include "virtio_crypto_algs.h"
#include "virtio_crypto_capabilities.h"

static int virtio_crypto_dev_configure(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);
static int virtio_crypto_dev_start(struct rte_cryptodev *dev);
static void virtio_crypto_dev_stop(struct rte_cryptodev *dev);
static int virtio_crypto_dev_close(struct rte_cryptodev *dev);
static void virtio_crypto_dev_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info);
static void virtio_crypto_dev_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats);
static void virtio_crypto_dev_stats_reset(struct rte_cryptodev *dev);
static int virtio_crypto_qp_setup(struct rte_cryptodev *dev,
		uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id);
static int virtio_crypto_qp_release(struct rte_cryptodev *dev,
		uint16_t queue_pair_id);
static void virtio_crypto_dev_free_mbufs(struct rte_cryptodev *dev);
static unsigned int virtio_crypto_sym_get_session_private_size(
		struct rte_cryptodev *dev);
static void virtio_crypto_sym_clear_session(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess);
static int virtio_crypto_sym_configure_session(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *session);
static void virtio_crypto_asym_clear_session(struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess);
static int virtio_crypto_asym_configure_session(struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *session);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_crypto_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_CRYPTO_PCI_VENDORID,
				VIRTIO_CRYPTO_PCI_DEVICEID) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_cryptodev_capabilities virtio_capabilities[] = {
	VIRTIO_SYM_CAPABILITIES,
	VIRTIO_ASYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

uint8_t cryptodev_virtio_driver_id;

void
virtio_crypto_queue_release(struct virtqueue *vq)
{
	struct virtio_crypto_hw *hw;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	if (vq) {
		hw = vq->hw;
		/* Select and deactivate the queue */
		VTPCI_OPS(hw)->del_queue(hw, vq);

		hw->vqs[vq->vq_queue_index] = NULL;
		rte_memzone_free(vq->mz);
		rte_mempool_free(vq->mpool);
		for (i = 0; i < vq->vq_nentries; i++)
			rte_free(vq->vq_descx[i].cookie);

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
	unsigned int vq_size;
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

	/*
	 * Using part of the vring entries is permitted, but the maximum
	 * is vq_size
	 */
	if (nb_desc == 0 || nb_desc > vq_size)
		nb_desc = vq_size;

	if (hw->vqs[vtpci_queue_idx])
		vq = hw->vqs[vtpci_queue_idx];
	else
		vq = virtcrypto_queue_alloc(hw, vtpci_queue_idx, nb_desc,
				socket_id, vq_name);
	if (vq == NULL) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("Can not allocate virtqueue");
		return -ENOMEM;
	}

	hw->vqs[vtpci_queue_idx] = vq;

	if (queue_type == VTCRYPTO_DATAQ) {
		/* pre-allocate a mempool and use it in the data plane to
		 * improve performance
		 */
		vq->mpool = rte_mempool_lookup(mpool_name);
		if (vq->mpool == NULL)
			vq->mpool = rte_mempool_create(mpool_name,
					nb_desc,
					sizeof(struct virtio_crypto_op_cookie),
					RTE_CACHE_LINE_SIZE, 0,
					NULL, NULL, NULL, NULL, socket_id,
					0);
		if (!vq->mpool) {
			VIRTIO_CRYPTO_DRV_LOG_ERR("Virtio Crypto PMD "
					"Cannot create mempool");
			goto mpool_create_err;
		}
		for (i = 0; i < nb_desc; i++) {
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

	*pvq = vq;

	return 0;

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

static void
virtio_crypto_free_queues(struct rte_cryptodev *dev)
{
	unsigned int i;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	/* data queue release */
	for (i = 0; i < hw->max_dataqueues; i++) {
		virtio_crypto_queue_release(dev->data->queue_pairs[i]);
		dev->data->queue_pairs[i] = NULL;
	}
}

static int
virtio_crypto_dev_close(struct rte_cryptodev *dev __rte_unused)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	/* control queue release */
	if (hw->cvq)
		virtio_crypto_queue_release(virtcrypto_cq_to_vq(hw->cvq));

	hw->cvq = NULL;
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

	.stats_get			 = virtio_crypto_dev_stats_get,
	.stats_reset			 = virtio_crypto_dev_stats_reset,

	.queue_pair_setup                = virtio_crypto_qp_setup,
	.queue_pair_release              = virtio_crypto_qp_release,

	/* Crypto related operations */
	.sym_session_get_size		= virtio_crypto_sym_get_session_private_size,
	.sym_session_configure		= virtio_crypto_sym_configure_session,
	.sym_session_clear		= virtio_crypto_sym_clear_session,
	.asym_session_get_size		= virtio_crypto_sym_get_session_private_size,
	.asym_session_configure		= virtio_crypto_asym_configure_session,
	.asym_session_clear		= virtio_crypto_asym_clear_session
};

static void
virtio_crypto_update_stats(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	unsigned int i;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (stats == NULL) {
		VIRTIO_CRYPTO_DRV_LOG_ERR("invalid pointer");
		return;
	}

	for (i = 0; i < hw->max_dataqueues; i++) {
		const struct virtqueue *data_queue
			= dev->data->queue_pairs[i];
		if (data_queue == NULL)
			continue;

		stats->enqueued_count += data_queue->packets_sent_total;
		stats->enqueue_err_count += data_queue->packets_sent_failed;

		stats->dequeued_count += data_queue->packets_received_total;
		stats->dequeue_err_count
			+= data_queue->packets_received_failed;
	}
}

static void
virtio_crypto_dev_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	PMD_INIT_FUNC_TRACE();

	virtio_crypto_update_stats(dev, stats);
}

static void
virtio_crypto_dev_stats_reset(struct rte_cryptodev *dev)
{
	unsigned int i;
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < hw->max_dataqueues; i++) {
		struct virtqueue *data_queue = dev->data->queue_pairs[i];
		if (data_queue == NULL)
			continue;

		data_queue->packets_sent_total = 0;
		data_queue->packets_sent_failed = 0;

		data_queue->packets_received_total = 0;
		data_queue->packets_received_failed = 0;
	}
}

static int
virtio_crypto_qp_setup(struct rte_cryptodev *dev, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
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
			"virtio crypto data queue initialization failed");
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
	dev->data->queue_pairs[queue_pair_id] = NULL;
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

static void
virtio_control_queue_notify(struct virtqueue *vq, __rte_unused void *cookie)
{
	virtqueue_notify(vq);
}

static int
virtio_crypto_init_queue(struct rte_cryptodev *dev, uint16_t queue_idx)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;
	int queue_type = virtio_get_queue_type(hw, queue_idx);
	int numa_node = dev->device->numa_node;
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	unsigned int vq_size;
	struct virtqueue *vq;
	int ret;

	PMD_INIT_LOG(INFO, "setting up queue: %u on NUMA node %d",
			queue_idx, numa_node);

	/*
	 * Read the virtqueue size from the Queue Size field
	 * Always power of 2 and if 0 virtqueue does not exist
	 */
	vq_size = VTPCI_OPS(hw)->get_queue_num(hw, queue_idx);
	PMD_INIT_LOG(DEBUG, "vq_size: %u", vq_size);
	if (vq_size == 0) {
		PMD_INIT_LOG(ERR, "virtqueue does not exist");
		return -EINVAL;
	}

	if (!rte_is_power_of_2(vq_size)) {
		PMD_INIT_LOG(ERR, "split virtqueue size is not power of 2");
		return -EINVAL;
	}

	snprintf(vq_name, sizeof(vq_name), "dev%d_vq%d", dev->data->dev_id, queue_idx);

	vq = virtcrypto_queue_alloc(hw, queue_idx, vq_size, numa_node, vq_name);
	if (!vq) {
		PMD_INIT_LOG(ERR, "virtqueue init failed");
		return -ENOMEM;
	}

	hw->vqs[queue_idx] = vq;

	if (queue_type == VTCRYPTO_CTRLQ) {
		hw->cvq = &vq->cq;
		vq->cq.notify_queue = &virtio_control_queue_notify;
	}

	if (VTPCI_OPS(hw)->setup_queue(hw, vq) < 0) {
		PMD_INIT_LOG(ERR, "setup_queue failed");
		ret = -EINVAL;
		goto clean_vq;
	}

	return 0;

clean_vq:
	if (queue_type == VTCRYPTO_CTRLQ)
		hw->cvq = NULL;
	virtcrypto_queue_free(vq);
	hw->vqs[queue_idx] = NULL;

	return ret;
}

static int
virtio_crypto_alloc_queues(struct rte_cryptodev *dev)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = hw->max_dataqueues + 1;
	uint16_t i;
	int ret;

	hw->vqs = rte_zmalloc(NULL, sizeof(struct virtqueue *) * nr_vq, 0);
	if (!hw->vqs) {
		PMD_INIT_LOG(ERR, "failed to allocate vqs");
		return -ENOMEM;
	}

	for (i = 0; i < nr_vq; i++) {
		ret = virtio_crypto_init_queue(dev, i);
		if (ret < 0) {
			virtio_crypto_free_queues(dev);
			return ret;
		}
	}

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

int
crypto_virtio_dev_init(struct rte_cryptodev *cryptodev, uint64_t features,
		struct rte_pci_device *pci_dev)
{
	struct virtio_crypto_hw *hw;

	cryptodev->dev_ops = &virtio_crypto_dev_ops;

	cryptodev->enqueue_burst = virtio_crypto_pkt_tx_burst;
	cryptodev->dequeue_burst = virtio_crypto_pkt_rx_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT |
		RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
		RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	hw = cryptodev->data->dev_private;
	hw->dev_id = cryptodev->data->dev_id;
	hw->virtio_dev_capabilities = virtio_capabilities;

	if (pci_dev) {
		/* pci device init */
		VIRTIO_CRYPTO_INIT_LOG_DBG("dev %d vendorID=0x%x deviceID=0x%x",
			cryptodev->data->dev_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

		if (vtpci_cryptodev_init(pci_dev, hw))
			return -1;
	}

	if (virtio_crypto_init_device(cryptodev, features) < 0)
		return -1;

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

	PMD_INIT_FUNC_TRACE();

	cryptodev = rte_cryptodev_pmd_create(name, &pci_dev->device,
					init_params);
	if (cryptodev == NULL)
		return -ENODEV;

	cryptodev->driver_id = cryptodev_virtio_driver_id;
	if (crypto_virtio_dev_init(cryptodev, VIRTIO_CRYPTO_PMD_GUEST_FEATURES,
			pci_dev) < 0)
		return -1;

	rte_cryptodev_pmd_probing_finish(cryptodev);

	return 0;
}

static int
virtio_crypto_dev_uninit(struct rte_cryptodev *cryptodev)
{
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

	rte_cryptodev_pmd_release_device(cryptodev);

	VIRTIO_CRYPTO_DRV_LOG_INFO("dev_uninit completed");

	return 0;
}

static int
virtio_crypto_dev_configure(struct rte_cryptodev *cryptodev,
	struct rte_cryptodev_config *config __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	if (virtio_crypto_init_device(cryptodev,
			VIRTIO_CRYPTO_PMD_GUEST_FEATURES) < 0)
		return -1;

	/* setup control queue
	 * [0, 1, ... ,(config->max_dataqueues - 1)] are data queues
	 * config->max_dataqueues is the control queue
	 */
	if (virtio_crypto_alloc_queues(cryptodev) < 0) {
		VIRTIO_CRYPTO_DRV_LOG_ERR("failed to create virtqueues");
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

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
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

static unsigned int
virtio_crypto_sym_get_session_private_size(
		struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return RTE_ALIGN_CEIL(sizeof(struct virtio_crypto_session), 16);
}

static int
virtio_crypto_check_sym_session_paras(
		struct rte_cryptodev *dev)
{
	struct virtio_crypto_hw *hw;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(dev == NULL)) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("dev is NULL");
		return -1;
	}
	if (unlikely(dev->data == NULL)) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("dev->data is NULL");
		return -1;
	}
	hw = dev->data->dev_private;
	if (unlikely(hw == NULL)) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("hw is NULL");
		return -1;
	}
	if (unlikely(hw->cvq == NULL)) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("vq is NULL");
		return -1;
	}

	return 0;
}

static int
virtio_crypto_check_sym_clear_session_paras(
		struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	PMD_INIT_FUNC_TRACE();

	if (sess == NULL) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("sym_session is NULL");
		return -1;
	}

	return virtio_crypto_check_sym_session_paras(dev);
}

#define NUM_ENTRY_SYM_CLEAR_SESSION 2

static void
virtio_crypto_clear_session(
		struct rte_cryptodev *dev,
		struct virtio_crypto_op_ctrl_req *ctrl)
{
	struct virtio_crypto_hw *hw;
	struct virtqueue *vq;
	struct vring_desc *desc;
	uint8_t *status;
	uint8_t needed = 1;
	uint32_t head;
	uint64_t malloc_phys_addr;
	uint8_t len_inhdr = sizeof(struct virtio_crypto_inhdr);
	uint32_t len_op_ctrl_req = sizeof(struct virtio_crypto_op_ctrl_req);
	uint32_t desc_offset = len_op_ctrl_req + len_inhdr;
	uint64_t session_id = ctrl->u.destroy_session.session_id;

	hw = dev->data->dev_private;
	vq = virtcrypto_cq_to_vq(hw->cvq);

	VIRTIO_CRYPTO_SESSION_LOG_INFO("vq->vq_desc_head_idx = %d, "
			"vq = %p", vq->vq_desc_head_idx, vq);

	if (vq->vq_free_cnt < needed) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"vq->vq_free_cnt = %d is less than %d, "
				"not enough", vq->vq_free_cnt, needed);
		return;
	}

	malloc_phys_addr = rte_malloc_virt2iova(ctrl);
	if (malloc_phys_addr == RTE_BAD_IOVA) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("malloc_phys_addr is invalid");
		return;
	}

	/* status part */
	status = &(((struct virtio_crypto_inhdr *)
		((uint8_t *)ctrl + len_op_ctrl_req))->status);
	*status = VIRTIO_CRYPTO_ERR;

	/* indirect desc vring part */
	desc = (struct vring_desc *)((uint8_t *)ctrl + desc_offset);

	/* ctrl request part */
	desc[0].addr = malloc_phys_addr;
	desc[0].len = len_op_ctrl_req;
	desc[0].flags = VRING_DESC_F_NEXT;
	desc[0].next = 1;

	/* status part */
	desc[1].addr = malloc_phys_addr + len_op_ctrl_req;
	desc[1].len = len_inhdr;
	desc[1].flags = VRING_DESC_F_WRITE;

	/* use only a single desc entry */
	head = vq->vq_desc_head_idx;
	vq->vq_split.ring.desc[head].flags = VRING_DESC_F_INDIRECT;
	vq->vq_split.ring.desc[head].addr = malloc_phys_addr + desc_offset;
	vq->vq_split.ring.desc[head].len
		= NUM_ENTRY_SYM_CLEAR_SESSION
		* sizeof(struct vring_desc);
	vq->vq_free_cnt -= needed;

	vq->vq_desc_head_idx = vq->vq_split.ring.desc[head].next;

	vq_update_avail_ring(vq, head);
	vq_update_avail_idx(vq);

	VIRTIO_CRYPTO_INIT_LOG_DBG("vq->vq_queue_index = %d",
					vq->vq_queue_index);

	virtqueue_notify(vq);

	rte_rmb();
	while (vq->vq_used_cons_idx == vq->vq_split.ring.used->idx) {
		rte_rmb();
		usleep(100);
	}

	while (vq->vq_used_cons_idx != vq->vq_split.ring.used->idx) {
		uint32_t idx, desc_idx, used_idx;
		struct vring_used_elem *uep;

		used_idx = (uint32_t)(vq->vq_used_cons_idx
				& (vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];
		idx = (uint32_t) uep->id;
		desc_idx = idx;
		while (vq->vq_split.ring.desc[desc_idx].flags
				& VRING_DESC_F_NEXT) {
			desc_idx = vq->vq_split.ring.desc[desc_idx].next;
			vq->vq_free_cnt++;
		}

		vq->vq_split.ring.desc[desc_idx].next = vq->vq_desc_head_idx;
		vq->vq_desc_head_idx = idx;
		vq->vq_used_cons_idx++;
		vq->vq_free_cnt++;
	}

	if (*status != VIRTIO_CRYPTO_OK) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Close session failed "
				"status=%"PRIu32", session_id=%"PRIu64"",
				*status, session_id);
		rte_free(ctrl);
		return;
	}

	VIRTIO_CRYPTO_INIT_LOG_DBG("vq->vq_free_cnt=%d", vq->vq_free_cnt);
	VIRTIO_CRYPTO_INIT_LOG_DBG("vq->vq_desc_head_idx=%d", vq->vq_desc_head_idx);

	VIRTIO_CRYPTO_SESSION_LOG_INFO("Close session %"PRIu64" successfully ",
			session_id);

	rte_free(ctrl);
}

static void
virtio_crypto_clear_session_packed(
		struct rte_cryptodev *dev,
		struct virtio_crypto_op_ctrl_req *ctrl)
{
	struct virtio_crypto_hw *hw;
	struct virtqueue *vq;
	struct vring_packed_desc *desc;
	uint8_t *status;
	uint8_t needed = 1;
	uint32_t head;
	uint64_t malloc_phys_addr;
	uint8_t len_inhdr = sizeof(struct virtio_crypto_inhdr);
	uint32_t len_op_ctrl_req = sizeof(struct virtio_crypto_op_ctrl_req);
	uint64_t session_id = ctrl->u.destroy_session.session_id;
	uint16_t flags;
	uint8_t nb_descs = 0;

	hw = dev->data->dev_private;
	vq = virtcrypto_cq_to_vq(hw->cvq);
	head = vq->vq_avail_idx;
	flags = vq->vq_packed.cached_flags;

	VIRTIO_CRYPTO_SESSION_LOG_INFO("vq->vq_desc_head_idx = %d, "
			"vq = %p", vq->vq_desc_head_idx, vq);

	if (vq->vq_free_cnt < needed) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"vq->vq_free_cnt = %d is less than %d, "
				"not enough", vq->vq_free_cnt, needed);
		return;
	}

	malloc_phys_addr = rte_malloc_virt2iova(ctrl);
	if (malloc_phys_addr == RTE_BAD_IOVA) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("malloc_phys_addr is invalid");
		return;
	}

	/* status part */
	status = &(((struct virtio_crypto_inhdr *)
		((uint8_t *)ctrl + len_op_ctrl_req))->status);
	*status = VIRTIO_CRYPTO_ERR;

	/* indirect desc vring part */
	desc = vq->vq_packed.ring.desc;

	/* ctrl request part */
	desc[head].addr = malloc_phys_addr;
	desc[head].len = len_op_ctrl_req;
	desc[head].flags = VRING_DESC_F_NEXT | vq->vq_packed.cached_flags;
	vq->vq_free_cnt--;
	nb_descs++;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	/* status part */
	desc[vq->vq_avail_idx].addr = malloc_phys_addr + len_op_ctrl_req;
	desc[vq->vq_avail_idx].len = len_inhdr;
	desc[vq->vq_avail_idx].flags = VRING_DESC_F_WRITE;
	vq->vq_free_cnt--;
	nb_descs++;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	virtqueue_store_flags_packed(&desc[head], VRING_DESC_F_NEXT | flags,
			vq->hw->weak_barriers);

	virtio_wmb(vq->hw->weak_barriers);
	virtqueue_notify(vq);

	/* wait for used desc in virtqueue
	 * desc_is_used has a load-acquire or rte_io_rmb inside
	 */
	rte_rmb();
	while (!desc_is_used(&desc[head], vq)) {
		rte_rmb();
		usleep(100);
	}

	/* now get used descriptors */
	vq->vq_free_cnt += nb_descs;
	vq->vq_used_cons_idx += nb_descs;
	if (vq->vq_used_cons_idx >= vq->vq_nentries) {
		vq->vq_used_cons_idx -= vq->vq_nentries;
		vq->vq_packed.used_wrap_counter ^= 1;
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d "
			"vq->vq_queue_idx=%d "
			"vq->vq_avail_idx=%d "
			"vq->vq_used_cons_idx=%d "
			"vq->vq_packed.cached_flags=0x%x "
			"vq->vq_packed.used_wrap_counter=%d",
			vq->vq_free_cnt,
			vq->vq_queue_index,
			vq->vq_avail_idx,
			vq->vq_used_cons_idx,
			vq->vq_packed.cached_flags,
			vq->vq_packed.used_wrap_counter);

	if (*status != VIRTIO_CRYPTO_OK) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Close session failed "
				"status=%"PRIu32", session_id=%"PRIu64"",
				*status, session_id);
		rte_free(ctrl);
		return;
	}

	VIRTIO_CRYPTO_INIT_LOG_DBG("vq->vq_free_cnt=%d "
			"vq->vq_desc_head_idx=%d",
			vq->vq_free_cnt, vq->vq_desc_head_idx);

	VIRTIO_CRYPTO_SESSION_LOG_INFO("Close session %"PRIu64" successfully ",
			session_id);

	rte_free(ctrl);
}

static void
virtio_crypto_sym_clear_session(
		struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint32_t len_op_ctrl_req = sizeof(struct virtio_crypto_op_ctrl_req);
	uint8_t len_inhdr = sizeof(struct virtio_crypto_inhdr);
	struct virtio_crypto_op_ctrl_req *ctrl;
	struct virtio_crypto_session *session;
	uint8_t *malloc_virt_addr;

	PMD_INIT_FUNC_TRACE();

	if (virtio_crypto_check_sym_clear_session_paras(dev, sess) < 0)
		return;

	session = CRYPTODEV_GET_SYM_SESS_PRIV(sess);

	/*
	 * malloc memory to store information of ctrl request op,
	 * returned status and desc vring
	 */
	malloc_virt_addr = rte_malloc(NULL, len_op_ctrl_req + len_inhdr
		+ NUM_ENTRY_SYM_CLEAR_SESSION
		* sizeof(struct vring_desc), RTE_CACHE_LINE_SIZE);
	if (malloc_virt_addr == NULL) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("not enough heap room");
		return;
	}

	/* assign ctrl request op part */
	ctrl = (struct virtio_crypto_op_ctrl_req *)malloc_virt_addr;
	ctrl->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
	/* default data virtqueue is 0 */
	ctrl->header.queue_id = 0;
	ctrl->u.destroy_session.session_id = session->session_id;

	if (vtpci_with_packed_queue(dev->data->dev_private))
		return virtio_crypto_clear_session_packed(dev, ctrl);

	return virtio_crypto_clear_session(dev, ctrl);
}

static void
virtio_crypto_asym_clear_session(
		struct rte_cryptodev *dev,
		struct rte_cryptodev_asym_session *sess)
{
	uint32_t len_op_ctrl_req = sizeof(struct virtio_crypto_op_ctrl_req);
	uint8_t len_inhdr = sizeof(struct virtio_crypto_inhdr);
	struct virtio_crypto_op_ctrl_req *ctrl;
	struct virtio_crypto_session *session;
	uint8_t *malloc_virt_addr;

	PMD_INIT_FUNC_TRACE();

	session = CRYPTODEV_GET_ASYM_SESS_PRIV(sess);

	/*
	 * malloc memory to store information of ctrl request op,
	 * returned status and desc vring
	 */
	malloc_virt_addr = rte_malloc(NULL, len_op_ctrl_req + len_inhdr
		+ NUM_ENTRY_SYM_CLEAR_SESSION
		* sizeof(struct vring_desc), RTE_CACHE_LINE_SIZE);
	if (malloc_virt_addr == NULL) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("not enough heap room");
		return;
	}

	/* assign ctrl request op part */
	ctrl = (struct virtio_crypto_op_ctrl_req *)malloc_virt_addr;
	ctrl->header.opcode = VIRTIO_CRYPTO_AKCIPHER_DESTROY_SESSION;
	/* default data virtqueue is 0 */
	ctrl->header.queue_id = 0;
	ctrl->u.destroy_session.session_id = session->session_id;

	if (vtpci_with_packed_queue(dev->data->dev_private))
		return virtio_crypto_clear_session_packed(dev, ctrl);

	return virtio_crypto_clear_session(dev, ctrl);
}

static struct rte_crypto_cipher_xform *
virtio_crypto_get_cipher_xform(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return &xform->cipher;

		xform = xform->next;
	} while (xform);

	return NULL;
}

static struct rte_crypto_auth_xform *
virtio_crypto_get_auth_xform(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return &xform->auth;

		xform = xform->next;
	} while (xform);

	return NULL;
}

/** Get xform chain order */
static int
virtio_crypto_get_chain_order(struct rte_crypto_sym_xform *xform)
{
	if (xform == NULL)
		return -1;

	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next == NULL)
		return VIRTIO_CRYPTO_CMD_CIPHER;

	/* Authentication Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next == NULL)
		return VIRTIO_CRYPTO_CMD_AUTH;

	/* Authenticate then Cipher */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
		return VIRTIO_CRYPTO_CMD_HASH_CIPHER;

	/* Cipher then Authenticate */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
		return VIRTIO_CRYPTO_CMD_CIPHER_HASH;

	return -1;
}

static int
virtio_crypto_sym_pad_cipher_param(
		struct virtio_crypto_cipher_session_para *para,
		struct rte_crypto_cipher_xform *cipher_xform)
{
	switch (cipher_xform->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		para->algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
		break;
	default:
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Crypto: Unsupported "
				"Cipher alg %u", cipher_xform->algo);
		return -1;
	}

	para->keylen = cipher_xform->key.length;
	switch (cipher_xform->op) {
	case RTE_CRYPTO_CIPHER_OP_ENCRYPT:
		para->op = VIRTIO_CRYPTO_OP_ENCRYPT;
		break;
	case RTE_CRYPTO_CIPHER_OP_DECRYPT:
		para->op = VIRTIO_CRYPTO_OP_DECRYPT;
		break;
	default:
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Unsupported cipher operation "
					"parameter");
		return -1;
	}

	return 0;
}

static int
virtio_crypto_sym_pad_auth_param(
		struct virtio_crypto_op_ctrl_req *ctrl,
		struct rte_crypto_auth_xform *auth_xform)
{
	uint32_t *algo;
	struct virtio_crypto_alg_chain_session_para *para =
		&(ctrl->u.sym_create_session.u.chain.para);

	switch (ctrl->u.sym_create_session.u.chain.para.hash_mode) {
	case VIRTIO_CRYPTO_SYM_HASH_MODE_PLAIN:
		algo = &(para->u.hash_param.algo);
		break;
	case VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH:
		algo = &(para->u.mac_param.algo);
		break;
	default:
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Unsupported hash mode %u "
			"specified",
			ctrl->u.sym_create_session.u.chain.para.hash_mode);
		return -1;
	}

	switch (auth_xform->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		*algo = VIRTIO_CRYPTO_MAC_HMAC_SHA1;
		break;
	default:
		VIRTIO_CRYPTO_SESSION_LOG_ERR(
			"Crypto: Undefined Hash algo %u specified",
			auth_xform->algo);
		return -1;
	}

	return 0;
}

static int
virtio_crypto_sym_pad_op_ctrl_req(
		struct virtio_crypto_op_ctrl_req *ctrl,
		struct rte_crypto_sym_xform *xform, bool is_chainned,
		uint8_t *cipher_key_data, uint8_t *auth_key_data,
		struct virtio_crypto_session *session)
{
	int ret;
	struct rte_crypto_auth_xform *auth_xform = NULL;
	struct rte_crypto_cipher_xform *cipher_xform = NULL;

	/* Get cipher xform from crypto xform chain */
	cipher_xform = virtio_crypto_get_cipher_xform(xform);
	if (cipher_xform) {
		if (cipher_xform->key.length > VIRTIO_CRYPTO_MAX_KEY_SIZE) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"cipher key size cannot be longer than %u",
				VIRTIO_CRYPTO_MAX_KEY_SIZE);
			return -1;
		}
		if (cipher_xform->iv.length > VIRTIO_CRYPTO_MAX_IV_SIZE) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"cipher IV size cannot be longer than %u",
				VIRTIO_CRYPTO_MAX_IV_SIZE);
			return -1;
		}
		if (is_chainned)
			ret = virtio_crypto_sym_pad_cipher_param(
				&ctrl->u.sym_create_session.u.chain.para
						.cipher_param, cipher_xform);
		else
			ret = virtio_crypto_sym_pad_cipher_param(
				&ctrl->u.sym_create_session.u.cipher.para,
				cipher_xform);

		if (ret < 0) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"pad cipher parameter failed");
			return -1;
		}

		memcpy(cipher_key_data, cipher_xform->key.data,
				cipher_xform->key.length);

		session->iv.offset = cipher_xform->iv.offset;
		session->iv.length = cipher_xform->iv.length;
	}

	/* Get auth xform from crypto xform chain */
	auth_xform = virtio_crypto_get_auth_xform(xform);
	if (auth_xform) {
		/* FIXME: support VIRTIO_CRYPTO_SYM_HASH_MODE_NESTED */
		struct virtio_crypto_alg_chain_session_para *para =
			&(ctrl->u.sym_create_session.u.chain.para);
		if (auth_xform->key.length) {
			if (auth_xform->key.length >
					VIRTIO_CRYPTO_MAX_KEY_SIZE) {
				VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"auth key size cannot be longer than %u",
					VIRTIO_CRYPTO_MAX_KEY_SIZE);
				return -1;
			}
			para->hash_mode = VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH;
			para->u.mac_param.auth_key_len =
				(uint32_t)auth_xform->key.length;
			para->u.mac_param.hash_result_len =
				auth_xform->digest_length;
			memcpy(auth_key_data, auth_xform->key.data,
					auth_xform->key.length);
		} else {
			para->hash_mode	= VIRTIO_CRYPTO_SYM_HASH_MODE_PLAIN;
			para->u.hash_param.hash_result_len =
				auth_xform->digest_length;
		}

		ret = virtio_crypto_sym_pad_auth_param(ctrl, auth_xform);
		if (ret < 0) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR("pad auth parameter "
						"failed");
			return -1;
		}
	}

	return 0;
}

static int
virtio_crypto_check_sym_configure_session_paras(
		struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sym_sess)
{
	if (unlikely(xform == NULL) || unlikely(sym_sess == NULL)) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("NULL pointer");
		return -1;
	}

	if (virtio_crypto_check_sym_session_paras(dev) < 0)
		return -1;

	return 0;
}

static int
virtio_crypto_check_asym_configure_session_paras(
		struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *asym_sess)
{
	if (unlikely(xform == NULL) || unlikely(asym_sess == NULL)) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("NULL pointer");
		return -1;
	}

	if (virtio_crypto_check_sym_session_paras(dev) < 0)
		return -1;

	return 0;
}

static int
virtio_crypto_sym_configure_session(
		struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t cipher_key_data[VIRTIO_CRYPTO_MAX_KEY_SIZE] = {0};
	uint8_t auth_key_data[VIRTIO_CRYPTO_MAX_KEY_SIZE] = {0};
	struct virtio_crypto_op_ctrl_req *ctrl_req;
	struct virtio_crypto_session_input *input;
	struct virtio_crypto_session *session;
	enum virtio_crypto_cmd_id cmd_id;
	struct virtio_crypto_hw *hw;
	struct virtio_pmd_ctrl *ctrl;
	int dlen[2], dnum;
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = virtio_crypto_check_sym_configure_session_paras(dev, xform,
			sess);
	if (ret < 0) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Invalid parameters");
		return ret;
	}
	session = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	memset(session, 0, sizeof(struct virtio_crypto_session));
	ctrl = &session->ctrl;
	ctrl_req = &ctrl->hdr;
	ctrl_req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
	/* FIXME: support multiqueue */
	ctrl_req->header.queue_id = 0;

	hw = dev->data->dev_private;

	cmd_id = virtio_crypto_get_chain_order(xform);
	if (cmd_id == VIRTIO_CRYPTO_CMD_CIPHER_HASH)
		ctrl_req->u.sym_create_session.u.chain.para.alg_chain_order
			= VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;
	if (cmd_id == VIRTIO_CRYPTO_CMD_HASH_CIPHER)
		ctrl_req->u.sym_create_session.u.chain.para.alg_chain_order
			= VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER;

	switch (cmd_id) {
	case VIRTIO_CRYPTO_CMD_CIPHER_HASH:
	case VIRTIO_CRYPTO_CMD_HASH_CIPHER: {
		struct rte_crypto_cipher_xform *cipher_xform = NULL;
		struct rte_crypto_auth_xform *auth_xform = NULL;

		cipher_xform = virtio_crypto_get_cipher_xform(xform);
		auth_xform = virtio_crypto_get_auth_xform(xform);

		ctrl_req->u.sym_create_session.op_type
			= VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING;

		ret = virtio_crypto_sym_pad_op_ctrl_req(ctrl_req,
			xform, true, cipher_key_data, auth_key_data, session);
		if (ret < 0) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"padding sym op ctrl req failed");
			goto error_out;
		}

		dlen[0] = cipher_xform->key.length;
		memcpy(ctrl->data, cipher_key_data, dlen[0]);
		dlen[1] = auth_xform->key.length;
		memcpy(ctrl->data + dlen[0], auth_key_data, dlen[1]);
		dnum = 2;
		break;
	}
	case VIRTIO_CRYPTO_CMD_CIPHER: {
		struct rte_crypto_cipher_xform *cipher_xform = NULL;

		cipher_xform = virtio_crypto_get_cipher_xform(xform);

		ctrl_req->u.sym_create_session.op_type
			= VIRTIO_CRYPTO_SYM_OP_CIPHER;
		ret = virtio_crypto_sym_pad_op_ctrl_req(ctrl_req, xform,
			false, cipher_key_data, auth_key_data, session);
		if (ret < 0) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR(
				"padding sym op ctrl req failed");
			goto error_out;
		}

		dlen[0] = cipher_xform->key.length;
		memcpy(ctrl->data, cipher_key_data, dlen[0]);
		dnum = 1;
		break;
	}
	default:
		ret = -ENOTSUP;
		VIRTIO_CRYPTO_SESSION_LOG_ERR(
			"Unsupported operation chain order parameter");
		goto error_out;
	}

	input = &ctrl->input;
	input->status = VIRTIO_CRYPTO_ERR;
	input->session_id = ~0ULL;

	ret = virtio_crypto_send_command(hw->cvq, ctrl, dlen, dnum);
	if (ret < 0) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("create session failed: %d", ret);
		goto error_out;
	}

	ctrl = hw->cvq->hdr_mz->addr;
	input = &ctrl->input;
	if (input->status != VIRTIO_CRYPTO_OK) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Something wrong on backend! "
				"status=%u, session_id=%" PRIu64 "",
				input->status, input->session_id);
		goto error_out;
	} else {
		session->session_id = input->session_id;
		VIRTIO_CRYPTO_SESSION_LOG_INFO("Create session successfully, "
				"session_id=%" PRIu64 "", input->session_id);
	}

	return 0;
error_out:
	return ret;
}

static size_t
tlv_encode(uint8_t *tlv, uint8_t type, uint8_t *data, size_t len)
{
	uint8_t *lenval = tlv;
	size_t lenval_n = 0;

	if (len > 65535) {
		goto _exit;
	} else if (len > 255) {
		lenval_n = 4 + len;
		lenval[0] = type;
		lenval[1] = 0x82;
		lenval[2] = (len & 0xFF00) >> 8;
		lenval[3] = (len & 0xFF);
		rte_memcpy(&lenval[4], data, len);
	} else if (len > 127) {
		lenval_n = 3 + len;
		lenval[0] = type;
		lenval[1] = 0x81;
		lenval[2] = len;
		rte_memcpy(&lenval[3], data, len);
	} else {
		lenval_n = 2 + len;
		lenval[0] = type;
		lenval[1] = len;
		rte_memcpy(&lenval[2], data, len);
	}

_exit:
	return lenval_n;
}

static int
virtio_crypto_asym_rsa_xform_to_der(
		struct rte_crypto_asym_xform *xform,
		uint8_t *der)
{
	uint8_t data[VIRTIO_CRYPTO_MAX_CTRL_DATA];
	uint8_t ver[3] = {0x02, 0x01, 0x00};
	size_t tlen, len;
	uint8_t *tlv;

	if (xform->xform_type != RTE_CRYPTO_ASYM_XFORM_RSA)
		return -EINVAL;

	tlv = data;
	rte_memcpy(tlv, ver, RTE_DIM(ver));
	tlen = RTE_DIM(ver);
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.n.data, xform->rsa.n.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.e.data, xform->rsa.e.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.d.data, xform->rsa.d.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.qt.p.data, xform->rsa.qt.p.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.qt.q.data, xform->rsa.qt.q.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.qt.dP.data, xform->rsa.qt.dP.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.qt.dQ.data, xform->rsa.qt.dQ.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.qt.qInv.data, xform->rsa.qt.qInv.length);
	tlen += len;

	RTE_ASSERT(tlen < VIRTIO_CRYPTO_MAX_CTRL_DATA);
	len = tlv_encode(der, 0x30, data, tlen);
	return len;
}

static int
virtio_crypto_asym_rsa_xform_to_public_der(
		struct rte_crypto_asym_xform *xform,
		uint8_t *der)
{
	uint8_t data[VIRTIO_CRYPTO_MAX_CTRL_DATA];
	size_t tlen = 0, len;
	uint8_t *tlv;

	if (xform->xform_type != RTE_CRYPTO_ASYM_XFORM_RSA)
		return -EINVAL;

	tlv = data;
	len = tlv_encode(tlv, 0x02, xform->rsa.n.data, xform->rsa.n.length);
	tlen += len;
	len = tlv_encode(tlv + tlen, 0x02, xform->rsa.e.data, xform->rsa.e.length);
	tlen += len;

	RTE_ASSERT(tlen < VIRTIO_CRYPTO_MAX_CTRL_DATA);
	len = tlv_encode(der, 0x30, data, tlen);
	return len;
}

static int
virtio_crypto_asym_rsa_configure_session(
		struct rte_crypto_rsa_xform *rsa,
		struct virtio_crypto_akcipher_session_para *para)
{
	para->algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
	if (rsa->key_type == RTE_RSA_KEY_TYPE_EXP)
		para->keytype = VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PUBLIC;
	else
		para->keytype = VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PRIVATE;

	if (rsa->padding.type == RTE_CRYPTO_RSA_PADDING_NONE) {
		para->u.rsa.padding_algo = VIRTIO_CRYPTO_RSA_RAW_PADDING;
	} else if (rsa->padding.type == RTE_CRYPTO_RSA_PADDING_PKCS1_5) {
		para->u.rsa.padding_algo = VIRTIO_CRYPTO_RSA_PKCS1_PADDING;
		switch (rsa->padding.hash) {
		case  RTE_CRYPTO_AUTH_SHA1:
			para->u.rsa.hash_algo = VIRTIO_CRYPTO_RSA_SHA1;
			break;
		case  RTE_CRYPTO_AUTH_SHA224:
			para->u.rsa.hash_algo = VIRTIO_CRYPTO_RSA_SHA224;
			break;
		case  RTE_CRYPTO_AUTH_SHA256:
			para->u.rsa.hash_algo = VIRTIO_CRYPTO_RSA_SHA256;
			break;
		case  RTE_CRYPTO_AUTH_SHA512:
			para->u.rsa.hash_algo = VIRTIO_CRYPTO_RSA_SHA512;
			break;
		case  RTE_CRYPTO_AUTH_MD5:
			para->u.rsa.hash_algo = VIRTIO_CRYPTO_RSA_MD5;
			break;
		default:
			para->u.rsa.hash_algo = VIRTIO_CRYPTO_RSA_NO_HASH;
		}
	} else {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Invalid padding type");
		return -EINVAL;
	}

	return 0;
}

static int
virtio_crypto_asym_configure_session(
		struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *sess)
{
	struct virtio_crypto_akcipher_session_para *para;
	struct virtio_crypto_op_ctrl_req *ctrl_req;
	struct virtio_crypto_session_input *input;
	struct virtio_crypto_session *session;
	struct virtio_crypto_hw *hw;
	struct virtio_pmd_ctrl *ctrl;
	int dlen[1];
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = virtio_crypto_check_asym_configure_session_paras(dev, xform,
			sess);
	if (ret < 0) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Invalid parameters");
		return ret;
	}

	session = CRYPTODEV_GET_ASYM_SESS_PRIV(sess);
	memset(session, 0, sizeof(struct virtio_crypto_session));
	ctrl = &session->ctrl;
	ctrl_req = &ctrl->hdr;
	ctrl_req->header.opcode = VIRTIO_CRYPTO_AKCIPHER_CREATE_SESSION;
	ctrl_req->header.queue_id = 0;
	para = &ctrl_req->u.akcipher_create_session.para;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ctrl_req->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
		ret = virtio_crypto_asym_rsa_configure_session(&xform->rsa, para);
		if (ret < 0) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR("Invalid RSA parameters");
			return ret;
		}

		if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_EXP) {
			ret = virtio_crypto_asym_rsa_xform_to_public_der(
					xform, ctrl->data);
		} else {
			ret = virtio_crypto_asym_rsa_xform_to_der(xform,
					ctrl->data);
		}
		if (ret <= 0) {
			VIRTIO_CRYPTO_SESSION_LOG_ERR("Invalid RSA primitives");
			return ret;
		}

		ctrl_req->u.akcipher_create_session.para.keylen = ret;
		break;
	default:
		para->algo = VIRTIO_CRYPTO_NO_AKCIPHER;
	}

	dlen[0] = ret;
	input = &ctrl->input;
	input->status = VIRTIO_CRYPTO_ERR;
	input->session_id = ~0ULL;

	hw = dev->data->dev_private;
	ret = virtio_crypto_send_command(hw->cvq, ctrl, dlen, 1);
	if (ret < 0) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("create session failed: %d", ret);
		goto error_out;
	}

	ctrl = hw->cvq->hdr_mz->addr;
	input = &ctrl->input;
	if (input->status != VIRTIO_CRYPTO_OK) {
		VIRTIO_CRYPTO_SESSION_LOG_ERR("Something wrong on backend! "
				"status=%u, session_id=%" PRIu64 "",
				input->status, input->session_id);
		goto error_out;
	} else {
		session->session_id = input->session_id;
		VIRTIO_CRYPTO_SESSION_LOG_INFO("Create session successfully, "
				"session_id=%" PRIu64 "", input->session_id);
	}

	return 0;
error_out:
	return -1;
}

static void
virtio_crypto_dev_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *info)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (info != NULL) {
		info->driver_id = dev->driver_id;
		info->feature_flags = dev->feature_flags;
		info->max_nb_queue_pairs = hw->max_dataqueues;
		/* No limit of number of sessions */
		info->sym.max_nb_sessions = 0;
		info->capabilities = hw->virtio_dev_capabilities;
	}
}

static int
crypto_virtio_pci_probe(
	struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = pci_dev->device.numa_node,
		.private_data_size = sizeof(struct virtio_crypto_hw)
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
RTE_LOG_REGISTER_SUFFIX(virtio_crypto_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(virtio_crypto_logtype_session, session, NOTICE);
RTE_LOG_REGISTER_SUFFIX(virtio_crypto_logtype_rx, rx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(virtio_crypto_logtype_tx, tx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(virtio_crypto_logtype_driver, driver, NOTICE);
