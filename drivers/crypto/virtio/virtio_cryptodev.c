/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
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

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_crypto_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_CRYPTO_PCI_VENDORID,
				VIRTIO_CRYPTO_PCI_DEVICEID) },
	{ .vendor_id = 0, /* sentinel */ },
};

uint8_t cryptodev_virtio_driver_id;

/*
 * dev_ops for virtio, bare necessities for basic operation
 */
static struct rte_cryptodev_ops virtio_crypto_dev_ops = {
	/* Device related operations */
	.dev_configure			 = NULL,
	.dev_start			 = NULL,
	.dev_stop			 = NULL,
	.dev_close			 = NULL,
	.dev_infos_get			 = NULL,

	.stats_get			 = NULL,
	.stats_reset			 = NULL,

	.queue_pair_setup                = NULL,
	.queue_pair_release              = NULL,
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

	return 0;
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
