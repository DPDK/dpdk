/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <rte_cryptodev_pmd.h>

#include "qat_crypto.h"
#include "qat_logs.h"

uint8_t cryptodev_qat_driver_id;

static const struct rte_cryptodev_capabilities qat_gen1_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities qat_gen2_capabilities[] = {
	QAT_BASE_GEN1_SYM_CAPABILITIES,
	QAT_EXTRA_GEN2_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static struct rte_cryptodev_ops crypto_qat_ops = {

		/* Device related operations */
		.dev_configure		= qat_dev_config,
		.dev_start		= qat_dev_start,
		.dev_stop		= qat_dev_stop,
		.dev_close		= qat_dev_close,
		.dev_infos_get		= qat_dev_info_get,

		.stats_get		= qat_crypto_sym_stats_get,
		.stats_reset		= qat_crypto_sym_stats_reset,
		.queue_pair_setup	= qat_crypto_sym_qp_setup,
		.queue_pair_release	= qat_crypto_sym_qp_release,
		.queue_pair_start	= NULL,
		.queue_pair_stop	= NULL,
		.queue_pair_count	= NULL,

		/* Crypto related operations */
		.session_get_size	= qat_crypto_sym_get_session_private_size,
		.session_configure	= qat_crypto_sym_configure_session,
		.session_clear		= qat_crypto_sym_clear_session
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
crypto_qat_create(const char *name, struct rte_pci_device *pci_dev,
		struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *cryptodev;
	struct qat_pmd_private *internals;

	PMD_INIT_FUNC_TRACE();

	cryptodev = rte_cryptodev_pmd_create(name, &pci_dev->device,
			init_params);
	if (cryptodev == NULL)
		return -ENODEV;

	cryptodev->driver_id = cryptodev_qat_driver_id;
	cryptodev->dev_ops = &crypto_qat_ops;

	cryptodev->enqueue_burst = qat_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_pmd_dequeue_op_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER;

	internals = cryptodev->data->dev_private;
	internals->max_nb_sessions = init_params->max_nb_sessions;
	switch (pci_dev->id.device_id) {
	case 0x0443:
		internals->qat_dev_gen = QAT_GEN1;
		internals->qat_dev_capabilities = qat_gen1_capabilities;
		break;
	case 0x37c9:
	case 0x19e3:
	case 0x6f55:
		internals->qat_dev_gen = QAT_GEN2;
		internals->qat_dev_capabilities = qat_gen2_capabilities;
		break;
	default:
		PMD_DRV_LOG(ERR,
			"Invalid dev_id, can't determine capabilities");
		break;
	}

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(DEBUG, "Device already initialised by primary process");
		return 0;
	}

	return 0;
}

static int crypto_qat_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = pci_dev->device.numa_node,
		.private_data_size = sizeof(struct qat_pmd_private),
		.max_nb_sessions = RTE_QAT_PMD_MAX_NB_SESSIONS
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];

	PMD_DRV_LOG(DEBUG, "Found QAT device at %02x:%02x.%x",
			pci_dev->addr.bus,
			pci_dev->addr.devid,
			pci_dev->addr.function);

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return crypto_qat_create(name, pci_dev, &init_params);
}

static int crypto_qat_pci_remove(struct rte_pci_device *pci_dev)
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

	/* free crypto device */
	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_pci_driver rte_qat_pmd = {
	.id_table = pci_id_qat_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = crypto_qat_pci_probe,
	.remove = crypto_qat_pci_remove
};

static struct cryptodev_driver qat_crypto_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_QAT_SYM_PMD, rte_qat_pmd);
RTE_PMD_REGISTER_PCI_TABLE(CRYPTODEV_NAME_QAT_SYM_PMD, pci_id_qat_map);
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv, rte_qat_pmd.driver,
		cryptodev_qat_driver_id);
