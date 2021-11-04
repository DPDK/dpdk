/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <cryptodev_pmd.h>

#include "qat_logs.h"

#include "qat_crypto.h"
#include "qat_asym.h"
#include "qat_asym_pmd.h"

uint8_t qat_asym_driver_id;
struct qat_crypto_gen_dev_ops qat_asym_gen_dev_ops[QAT_N_GENS];

void
qat_asym_init_op_cookie(void *op_cookie)
{
	int j;
	struct qat_asym_op_cookie *cookie = op_cookie;

	cookie->input_addr = rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_asym_op_cookie,
					input_params_ptrs);

	cookie->output_addr = rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_asym_op_cookie,
					output_params_ptrs);

	for (j = 0; j < 8; j++) {
		cookie->input_params_ptrs[j] =
				rte_mempool_virt2iova(cookie) +
				offsetof(struct qat_asym_op_cookie,
						input_array[j]);
		cookie->output_params_ptrs[j] =
				rte_mempool_virt2iova(cookie) +
				offsetof(struct qat_asym_op_cookie,
						output_array[j]);
	}
}

static struct rte_cryptodev_ops crypto_qat_ops = {

	/* Device related operations */
	.dev_configure		= qat_cryptodev_config,
	.dev_start		= qat_cryptodev_start,
	.dev_stop		= qat_cryptodev_stop,
	.dev_close		= qat_cryptodev_close,
	.dev_infos_get		= qat_cryptodev_info_get,

	.stats_get		= qat_cryptodev_stats_get,
	.stats_reset		= qat_cryptodev_stats_reset,
	.queue_pair_setup	= qat_cryptodev_qp_setup,
	.queue_pair_release	= qat_cryptodev_qp_release,

	/* Crypto related operations */
	.asym_session_get_size	= qat_asym_session_get_private_size,
	.asym_session_configure	= qat_asym_session_configure,
	.asym_session_clear	= qat_asym_session_clear
};

uint16_t qat_asym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
				       uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, (void **)ops, nb_ops);
}

uint16_t qat_asym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
				       uint16_t nb_ops)
{
	return qat_dequeue_op_burst(qp, (void **)ops, nb_ops);
}

/* An rte_driver is needed in the registration of both the device and the driver
 * with cryptodev.
 * The actual qat pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the crypto part of the pci device.
 */
static const char qat_asym_drv_name[] = RTE_STR(CRYPTODEV_NAME_QAT_ASYM_PMD);
static const struct rte_driver cryptodev_qat_asym_driver = {
	.name = qat_asym_drv_name,
	.alias = qat_asym_drv_name
};

int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param)
{
	int i = 0;
	struct qat_device_info *qat_dev_instance =
			&qat_pci_devs[qat_pci_dev->qat_dev_id];
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = qat_dev_instance->pci_dev->device.numa_node,
		.private_data_size = sizeof(struct qat_cryptodev_private)
	};
	struct qat_capabilities_info capa_info;
	const struct rte_cryptodev_capabilities *capabilities;
	const struct qat_crypto_gen_dev_ops *gen_dev_ops =
		&qat_asym_gen_dev_ops[qat_pci_dev->qat_dev_gen];
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct qat_cryptodev_private *internals;
	uint64_t capa_size;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "asym");
	QAT_LOG(DEBUG, "Creating QAT ASYM device %s\n", name);

	if (gen_dev_ops->cryptodev_ops == NULL) {
		QAT_LOG(ERR, "Device %s does not support asymmetric crypto",
				name);
		return -EFAULT;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		qat_pci_dev->qat_asym_driver_id =
				qat_asym_driver_id;
	} else if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (qat_pci_dev->qat_asym_driver_id !=
				qat_asym_driver_id) {
			QAT_LOG(ERR,
				"Device %s have different driver id than corresponding device in primary process",
				name);
			return -(EFAULT);
		}
	}

	/* Populate subset device to use in cryptodev device creation */
	qat_dev_instance->asym_rte_dev.driver = &cryptodev_qat_asym_driver;
	qat_dev_instance->asym_rte_dev.numa_node =
			qat_dev_instance->pci_dev->device.numa_node;
	qat_dev_instance->asym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
			&(qat_dev_instance->asym_rte_dev), &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	qat_dev_instance->asym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = qat_asym_driver_id;
	cryptodev->dev_ops = &crypto_qat_ops;

	cryptodev->enqueue_burst = qat_asym_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_asym_pmd_dequeue_op_burst;


	cryptodev->feature_flags = gen_dev_ops->get_feature_flags(qat_pci_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	snprintf(capa_memz_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"QAT_ASYM_CAPA_GEN_%d",
			qat_pci_dev->qat_dev_gen);

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;
	internals->dev_id = cryptodev->data->dev_id;
	internals->service_type = QAT_SERVICE_ASYMMETRIC;

	capa_info = gen_dev_ops->get_capabilities(qat_pci_dev);
	capabilities = capa_info.data;
	capa_size = capa_info.size;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				capa_size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating memzone for capabilities, "
				"destroying PMD for %s",
				name);
			rte_cryptodev_pmd_destroy(cryptodev);
			memset(&qat_dev_instance->asym_rte_dev, 0,
				sizeof(qat_dev_instance->asym_rte_dev));
			return -EFAULT;
		}
	}

	memcpy(internals->capa_mz->addr, capabilities, capa_size);
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	while (1) {
		if (qat_dev_cmd_param[i].name == NULL)
			break;
		if (!strcmp(qat_dev_cmd_param[i].name, ASYM_ENQ_THRESHOLD_NAME))
			internals->min_enq_burst_threshold =
					qat_dev_cmd_param[i].val;
		i++;
	}

	qat_pci_dev->asym_dev = internals;

	rte_cryptodev_pmd_probing_finish(cryptodev);

	QAT_LOG(DEBUG, "Created QAT ASYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->dev_id);
	return 0;
}

int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	if (qat_pci_dev->asym_dev == NULL)
		return 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(qat_pci_dev->asym_dev->capa_mz);

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(
			qat_pci_dev->asym_dev->dev_id);
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_devs[qat_pci_dev->qat_dev_id].asym_rte_dev.name = NULL;
	qat_pci_dev->asym_dev = NULL;

	return 0;
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_asym_driver,
		qat_asym_driver_id);
