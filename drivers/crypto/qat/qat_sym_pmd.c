/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <cryptodev_pmd.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security_driver.h>
#endif

#include "qat_logs.h"
#include "qat_crypto.h"
#include "qat_sym.h"
#include "qat_sym_session.h"
#include "qat_sym_pmd.h"

#define MIXED_CRYPTO_MIN_FW_VER 0x04090000

uint8_t qat_sym_driver_id;

struct qat_crypto_gen_dev_ops qat_sym_gen_dev_ops[QAT_N_GENS];

void
qat_sym_init_op_cookie(void *op_cookie)
{
	struct qat_sym_op_cookie *cookie = op_cookie;

	cookie->qat_sgl_src_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			qat_sgl_src);

	cookie->qat_sgl_dst_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			qat_sgl_dst);

	cookie->opt.spc_gmac.cd_phys_addr =
			rte_mempool_virt2iova(cookie) +
			offsetof(struct qat_sym_op_cookie,
			opt.spc_gmac.cd_cipher);
}

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
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param __rte_unused)
{
	int i = 0, ret = 0;
	struct qat_device_info *qat_dev_instance =
			&qat_pci_devs[qat_pci_dev->qat_dev_id];
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = qat_dev_instance->pci_dev->device.numa_node,
		.private_data_size = sizeof(struct qat_cryptodev_private)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	char capa_memz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct qat_cryptodev_private *internals;
	struct qat_capabilities_info capa_info;
	const struct rte_cryptodev_capabilities *capabilities;
	const struct qat_crypto_gen_dev_ops *gen_dev_ops =
		&qat_sym_gen_dev_ops[qat_pci_dev->qat_dev_gen];
	uint64_t capa_size;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
			qat_pci_dev->name, "sym");
	QAT_LOG(DEBUG, "Creating QAT SYM device %s", name);

	if (gen_dev_ops->cryptodev_ops == NULL) {
		QAT_LOG(ERR, "Device %s does not support symmetric crypto",
				name);
		return -EFAULT;
	}

	/*
	 * All processes must use same driver id so they can share sessions.
	 * Store driver_id so we can validate that all processes have the same
	 * value, typically they have, but could differ if binaries built
	 * separately.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		qat_pci_dev->qat_sym_driver_id =
				qat_sym_driver_id;
	} else if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (qat_pci_dev->qat_sym_driver_id !=
				qat_sym_driver_id) {
			QAT_LOG(ERR,
				"Device %s have different driver id than corresponding device in primary process",
				name);
			return -(EFAULT);
		}
	}

	/* Populate subset device to use in cryptodev device creation */
	qat_dev_instance->sym_rte_dev.driver = &cryptodev_qat_sym_driver;
	qat_dev_instance->sym_rte_dev.numa_node =
			qat_dev_instance->pci_dev->device.numa_node;
	qat_dev_instance->sym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
			&(qat_dev_instance->sym_rte_dev), &init_params);

	if (cryptodev == NULL)
		return -ENODEV;

	qat_dev_instance->sym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = qat_sym_driver_id;
	cryptodev->dev_ops = gen_dev_ops->cryptodev_ops;

	cryptodev->enqueue_burst = qat_sym_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = qat_sym_pmd_dequeue_op_burst;

	cryptodev->feature_flags = gen_dev_ops->get_feature_flags(qat_pci_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

#ifdef RTE_LIB_SECURITY
	if (gen_dev_ops->create_security_ctx) {
		cryptodev->security_ctx =
			gen_dev_ops->create_security_ctx((void *)cryptodev);
		if (cryptodev->security_ctx == NULL) {
			QAT_LOG(ERR, "rte_security_ctx memory alloc failed");
			ret = -ENOMEM;
			goto error;
		}

		cryptodev->feature_flags |= RTE_CRYPTODEV_FF_SECURITY;
		QAT_LOG(INFO, "Device %s rte_security support enabled", name);
	} else
		QAT_LOG(INFO, "Device %s rte_security support disabled", name);

#endif
	snprintf(capa_memz_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"QAT_SYM_CAPA_GEN_%d",
			qat_pci_dev->qat_dev_gen);

	internals = cryptodev->data->dev_private;
	internals->qat_dev = qat_pci_dev;
	internals->service_type = QAT_SERVICE_SYMMETRIC;
	internals->dev_id = cryptodev->data->dev_id;

	capa_info = gen_dev_ops->get_capabilities(qat_pci_dev);
	capabilities = capa_info.data;
	capa_size = capa_info.size;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				capa_size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating capability memzon for %s",
				name);
			ret = -EFAULT;
			goto error;
		}
	}

	memcpy(internals->capa_mz->addr, capabilities, capa_size);
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	while (1) {
		if (qat_dev_cmd_param[i].name == NULL)
			break;
		if (!strcmp(qat_dev_cmd_param[i].name, SYM_ENQ_THRESHOLD_NAME))
			internals->min_enq_burst_threshold =
					qat_dev_cmd_param[i].val;
		i++;
	}

	qat_pci_dev->sym_dev = internals;
	QAT_LOG(DEBUG, "Created QAT SYM device %s as cryptodev instance %d",
			cryptodev->data->name, internals->dev_id);

	rte_cryptodev_pmd_probing_finish(cryptodev);

	return 0;

error:
#ifdef RTE_LIB_SECURITY
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
#endif
	rte_cryptodev_pmd_destroy(cryptodev);
	memset(&qat_dev_instance->sym_rte_dev, 0,
		sizeof(qat_dev_instance->sym_rte_dev));

	return ret;
}

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev)
{
	struct rte_cryptodev *cryptodev;

	if (qat_pci_dev == NULL)
		return -ENODEV;
	if (qat_pci_dev->sym_dev == NULL)
		return 0;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(qat_pci_dev->sym_dev->capa_mz);

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(qat_pci_dev->sym_dev->dev_id);
#ifdef RTE_LIB_SECURITY
	rte_free(cryptodev->security_ctx);
	cryptodev->security_ctx = NULL;
#endif
	rte_cryptodev_pmd_destroy(cryptodev);
	qat_pci_devs[qat_pci_dev->qat_dev_id].sym_rte_dev.name = NULL;
	qat_pci_dev->sym_dev = NULL;

	return 0;
}

static struct cryptodev_driver qat_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(qat_crypto_drv,
		cryptodev_qat_sym_driver,
		qat_sym_driver_id);
