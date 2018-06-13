/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <rte_cryptodev_pmd.h>

#include "qat_sym.h"
#include "qat_sym_session.h"
#include "qat_logs.h"

uint8_t cryptodev_qat_driver_id;

static const struct rte_cryptodev_capabilities qat_gen1_sym_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities qat_gen2_sym_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	QAT_EXTRA_GEN2_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static struct rte_cryptodev_ops crypto_qat_ops = {

		/* Device related operations */
		.dev_configure		= qat_sym_dev_config,
		.dev_start		= qat_sym_dev_start,
		.dev_stop		= qat_sym_dev_stop,
		.dev_close		= qat_sym_dev_close,
		.dev_infos_get		= qat_sym_dev_info_get,

		.stats_get		= qat_sym_stats_get,
		.stats_reset		= qat_sym_stats_reset,
		.queue_pair_setup	= qat_sym_qp_setup,
		.queue_pair_release	= qat_sym_qp_release,
		.queue_pair_start	= NULL,
		.queue_pair_stop	= NULL,
		.queue_pair_count	= NULL,

		/* Crypto related operations */
		.session_get_size	= qat_sym_session_get_private_size,
		.session_configure	= qat_sym_session_configure,
		.session_clear		= qat_sym_session_clear
};

/*
 * The set of PCI devices this driver supports
 */

static const struct rte_pci_id pci_id_qat_map[] = {
		{
			RTE_PCI_DEVICE(0x8086, 0x0443),
		},
		{
			RTE_PCI_DEVICE(0x8086, 0x37c9),
		},
		{
			RTE_PCI_DEVICE(0x8086, 0x19e3),
		},
		{
			RTE_PCI_DEVICE(0x8086, 0x6f55),
		},
		{.device_id = 0},
};



static int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
			.name = "",
			.socket_id = qat_pci_dev->pci_dev->device.numa_node,
			.private_data_size = sizeof(struct qat_sym_dev_private),
			.max_nb_sessions = RTE_QAT_PMD_MAX_NB_SESSIONS
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct qat_sym_dev_private *internals;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "sym");
	PMD_DRV_LOG(DEBUG, "Creating QAT SYM device %s", name);

	cryptodev = rte_cryptodev_pmd_create(name,
			&qat_pci_dev->pci_dev->device, &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	cryptodev->driver_id = cryptodev_qat_driver_id;
	cryptodev->dev_ops = &crypto_qat_ops;

	cryptodev->enqueue_burst = qat_sym_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_sym_pmd_dequeue_op_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER;

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;
	qat_pci_dev->sym_dev = internals;

	internals->sym_dev_id = cryptodev->data->dev_id;
	switch (qat_pci_dev->qat_dev_gen) {
	case QAT_GEN1:
		internals->qat_dev_capabilities = qat_gen1_sym_capabilities;
		break;
	case QAT_GEN2:
		internals->qat_dev_capabilities = qat_gen2_sym_capabilities;
		break;
	default:
		internals->qat_dev_capabilities = qat_gen2_sym_capabilities;
		PMD_DRV_LOG(DEBUG,
			"QAT gen %d capabilities unknown, default to GEN2",
					qat_pci_dev->qat_dev_gen);
		break;
	}

	PMD_DRV_LOG(DEBUG, "Created QAT SYM device %s as cryptodev instance %d",
			name, internals->sym_dev_id);
	return 0;
}

static int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	if (qat_pci_dev->sym_dev == NULL)
		return 0;

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(qat_pci_dev->sym_dev->sym_dev_id);
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_dev->sym_dev = NULL;

	return 0;
}

static int
qat_comp_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	return 0;
}

static int
qat_comp_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	return 0;
}

static int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	return 0;
}

static int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	return 0;
}

static int
qat_pci_dev_destroy(struct qat_pci_device *qat_pci_dev,
		struct rte_pci_device *pci_dev)
{
	qat_sym_dev_destroy(qat_pci_dev);
	qat_comp_dev_destroy(qat_pci_dev);
	qat_asym_dev_destroy(qat_pci_dev);
	return qat_pci_device_release(pci_dev);
}

static int qat_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	int ret = 0;
	struct qat_pci_device *qat_pci_dev;

	PMD_DRV_LOG(DEBUG, "Found QAT device at %02x:%02x.%x",
			pci_dev->addr.bus,
			pci_dev->addr.devid,
			pci_dev->addr.function);

	qat_pci_dev = qat_pci_device_allocate(pci_dev);
	if (qat_pci_dev == NULL)
		return -ENODEV;

	ret = qat_sym_dev_create(qat_pci_dev);
	if (ret != 0)
		goto error_out;

	ret = qat_comp_dev_create(qat_pci_dev);
	if (ret != 0)
		goto error_out;

	ret = qat_asym_dev_create(qat_pci_dev);
	if (ret != 0)
		goto error_out;

	return 0;

error_out:
	qat_pci_dev_destroy(qat_pci_dev, pci_dev);
	return ret;

}

static int qat_pci_remove(struct rte_pci_device *pci_dev)
{
	struct qat_pci_device *qat_pci_dev;

	if (pci_dev == NULL)
		return -EINVAL;

	qat_pci_dev = qat_get_qat_dev_from_pci_dev(pci_dev);
	if (qat_pci_dev == NULL)
		return 0;

	return qat_pci_dev_destroy(qat_pci_dev, pci_dev);

}


/* An rte_driver is needed in the registration of both the device and the driver
 * with cryptodev.
 * The actual qat pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the crypto part of the pci device.
 */
static const char qat_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_QAT_SYM_PMD);
static struct rte_driver cryptodev_qat_sym_driver = {
	.name = qat_sym_drv_name,
	.alias = qat_sym_drv_name
};
static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv, cryptodev_qat_sym_driver,
		cryptodev_qat_driver_id);

static struct rte_pci_driver rte_qat_pmd = {
	.id_table = pci_id_qat_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = qat_pci_probe,
	.remove = qat_pci_remove
};
RTE_PMD_REGISTER_PCI(QAT_PCI_NAME, rte_qat_pmd);
RTE_PMD_REGISTER_PCI_TABLE(QAT_PCI_NAME, pci_id_qat_map);
