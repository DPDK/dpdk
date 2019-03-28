/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <rte_cryptodev_pmd.h>

#include "qat_logs.h"
#include "qat_sym.h"
#include "qat_sym_session.h"
#include "qat_sym_pmd.h"

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

static int qat_sym_qp_release(struct rte_cryptodev *dev,
	uint16_t queue_pair_id);

static int qat_sym_dev_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

static int qat_sym_dev_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

static void qat_sym_dev_stop(__rte_unused struct rte_cryptodev *dev)
{
	return;
}

static int qat_sym_dev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_sym_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void qat_sym_dev_info_get(struct rte_cryptodev *dev,
			struct rte_cryptodev_info *info)
{
	struct qat_sym_dev_private *internals = dev->data->dev_private;
	const struct qat_qp_hw_data *sym_hw_qps =
		qat_gen_config[internals->qat_dev->qat_dev_gen]
			      .qp_hw_data[QAT_SERVICE_SYMMETRIC];

	if (info != NULL) {
		info->max_nb_queue_pairs =
			qat_qps_per_service(sym_hw_qps, QAT_SERVICE_SYMMETRIC);
		info->feature_flags = dev->feature_flags;
		info->capabilities = internals->qat_dev_capabilities;
		info->driver_id = cryptodev_qat_driver_id;
		/* No limit of number of sessions */
		info->sym.max_nb_sessions = 0;
	}
}

static void qat_sym_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	struct qat_common_stats qat_stats = {0};
	struct qat_sym_dev_private *qat_priv;

	if (stats == NULL || dev == NULL) {
		QAT_LOG(ERR, "invalid ptr: stats %p, dev %p", stats, dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_get(qat_priv->qat_dev, &qat_stats, QAT_SERVICE_SYMMETRIC);
	stats->enqueued_count = qat_stats.enqueued_count;
	stats->dequeued_count = qat_stats.dequeued_count;
	stats->enqueue_err_count = qat_stats.enqueue_err_count;
	stats->dequeue_err_count = qat_stats.dequeue_err_count;
}

static void qat_sym_stats_reset(struct rte_cryptodev *dev)
{
	struct qat_sym_dev_private *qat_priv;

	if (dev == NULL) {
		QAT_LOG(ERR, "invalid cryptodev ptr %p", dev);
		return;
	}
	qat_priv = dev->data->dev_private;

	qat_stats_reset(qat_priv->qat_dev, QAT_SERVICE_SYMMETRIC);

}

static int qat_sym_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	struct qat_sym_dev_private *qat_private = dev->data->dev_private;

	QAT_LOG(DEBUG, "Release sym qp %u on device %d",
				queue_pair_id, dev->data->dev_id);

	qat_private->qat_dev->qps_in_use[QAT_SERVICE_SYMMETRIC][queue_pair_id]
						= NULL;

	return qat_qp_release((struct qat_qp **)
			&(dev->data->queue_pairs[queue_pair_id]));
}

static int qat_sym_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
	const struct rte_cryptodev_qp_conf *qp_conf,
	int socket_id)
{
	struct qat_qp *qp;
	int ret = 0;
	uint32_t i;
	struct qat_qp_config qat_qp_conf;

	struct qat_qp **qp_addr =
			(struct qat_qp **)&(dev->data->queue_pairs[qp_id]);
	struct qat_sym_dev_private *qat_private = dev->data->dev_private;
	const struct qat_qp_hw_data *sym_hw_qps =
			qat_gen_config[qat_private->qat_dev->qat_dev_gen]
				      .qp_hw_data[QAT_SERVICE_SYMMETRIC];
	const struct qat_qp_hw_data *qp_hw_data = sym_hw_qps + qp_id;

	/* If qp is already in use free ring memory and qp metadata. */
	if (*qp_addr != NULL) {
		ret = qat_sym_qp_release(dev, qp_id);
		if (ret < 0)
			return ret;
	}
	if (qp_id >= qat_qps_per_service(sym_hw_qps, QAT_SERVICE_SYMMETRIC)) {
		QAT_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}

	qat_qp_conf.hw = qp_hw_data;
	qat_qp_conf.build_request = qat_sym_build_request;
	qat_qp_conf.cookie_size = sizeof(struct qat_sym_op_cookie);
	qat_qp_conf.nb_descriptors = qp_conf->nb_descriptors;
	qat_qp_conf.socket_id = socket_id;
	qat_qp_conf.service_str = "sym";

	ret = qat_qp_setup(qat_private->qat_dev, qp_addr, qp_id, &qat_qp_conf);
	if (ret != 0)
		return ret;

	/* store a link to the qp in the qat_pci_device */
	qat_private->qat_dev->qps_in_use[QAT_SERVICE_SYMMETRIC][qp_id]
							= *qp_addr;

	qp = (struct qat_qp *)*qp_addr;

	for (i = 0; i < qp->nb_descriptors; i++) {

		struct qat_sym_op_cookie *cookie =
				qp->op_cookies[i];

		cookie->qat_sgl_src_phys_addr =
				rte_mempool_virt2iova(cookie) +
				offsetof(struct qat_sym_op_cookie,
				qat_sgl_src);

		cookie->qat_sgl_dst_phys_addr =
				rte_mempool_virt2iova(cookie) +
				offsetof(struct qat_sym_op_cookie,
				qat_sgl_dst);
	}

	return ret;
}

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
		.queue_pair_count	= NULL,

		/* Crypto related operations */
		.sym_session_get_size	= qat_sym_session_get_private_size,
		.sym_session_configure	= qat_sym_session_configure,
		.sym_session_clear	= qat_sym_session_clear
};

static uint16_t
qat_sym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	return qat_enqueue_op_burst(qp, (void **)ops, nb_ops);
}

static uint16_t
qat_sym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
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
static const char qat_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_QAT_SYM_PMD);
static const struct rte_driver cryptodev_qat_sym_driver = {
	.name = qat_sym_drv_name,
	.alias = qat_sym_drv_name
};

int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
			.name = "",
			.socket_id = qat_pci_dev->pci_dev->device.numa_node,
			.private_data_size = sizeof(struct qat_sym_dev_private)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct qat_sym_dev_private *internals;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "sym");
	QAT_LOG(DEBUG, "Creating QAT SYM device %s", name);

	/* Populate subset device to use in cryptodev device creation */
	qat_pci_dev->sym_rte_dev.driver = &cryptodev_qat_sym_driver;
	qat_pci_dev->sym_rte_dev.numa_node =
				qat_pci_dev->pci_dev->device.numa_node;
	qat_pci_dev->sym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
			&(qat_pci_dev->sym_rte_dev), &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	qat_pci_dev->sym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = cryptodev_qat_driver_id;
	cryptodev->dev_ops = &crypto_qat_ops;

	cryptodev->enqueue_burst = qat_sym_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_sym_pmd_dequeue_op_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;
	qat_pci_dev->sym_dev = internals;

	internals->sym_dev_id = cryptodev->data->dev_id;
	switch (qat_pci_dev->qat_dev_gen) {
	case QAT_GEN1:
		internals->qat_dev_capabilities = qat_gen1_sym_capabilities;
		break;
	case QAT_GEN2:
	case QAT_GEN3:
		internals->qat_dev_capabilities = qat_gen2_sym_capabilities;
		break;
	default:
		internals->qat_dev_capabilities = qat_gen2_sym_capabilities;
		QAT_LOG(DEBUG,
			"QAT gen %d capabilities unknown, default to GEN2",
					qat_pci_dev->qat_dev_gen);
		break;
	}

	QAT_LOG(DEBUG, "Created QAT SYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->sym_dev_id);
	return 0;
}

int
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
	qat_pci_dev->sym_rte_dev.name = NULL;
	qat_pci_dev->sym_dev = NULL;

	return 0;
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_sym_driver,
		cryptodev_qat_driver_id);
