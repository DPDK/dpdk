/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "qat_device.h"
#include "adf_transport_access_macros.h"
#include "qat_qp.h"

/* Hardware device information per generation */
__extension__
struct qat_gen_hw_data qp_gen_config[] =  {
	[QAT_GEN1] = {
		.dev_gen = QAT_GEN1,
		.qp_hw_data = qat_gen1_qps,
	},
	[QAT_GEN2] = {
		.dev_gen = QAT_GEN2,
		.qp_hw_data = qat_gen1_qps,
		/* gen2 has same ring layout as gen1 */
	},
};


static struct qat_pci_device qat_pci_devices[QAT_MAX_PCI_DEVICES];
static int qat_nb_pci_devices;

int qat_dev_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

int qat_dev_start(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

void qat_dev_stop(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
}

int qat_dev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_sym_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void qat_dev_info_get(struct rte_cryptodev *dev,
			struct rte_cryptodev_info *info)
{
	struct qat_pmd_private *internals = dev->data->dev_private;
	const struct qat_qp_hw_data *sym_hw_qps =
		qp_gen_config[internals->qat_dev_gen]
			      .qp_hw_data[QAT_SERVICE_SYMMETRIC];

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs =
			qat_qps_per_service(sym_hw_qps, QAT_SERVICE_SYMMETRIC);
		info->feature_flags = dev->feature_flags;
		info->capabilities = internals->qat_dev_capabilities;
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->driver_id = cryptodev_qat_driver_id;
		info->pci_dev = RTE_DEV_TO_PCI(dev->device);
	}
}


static struct qat_pci_device *
qat_pci_get_dev(uint8_t dev_id)
{
	return &qat_pci_devices[dev_id];
}
static struct qat_pci_device *
qat_pci_get_named_dev(const char *name)
{
	struct qat_pci_device *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < QAT_MAX_PCI_DEVICES; i++) {
		dev = &qat_pci_devices[i];

		if ((dev->attached == QAT_ATTACHED) &&
				(strcmp(dev->name, name) == 0))
			return dev;
	}

	return NULL;
}

static uint8_t
qat_pci_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < QAT_MAX_PCI_DEVICES; dev_id++) {
		if (qat_pci_devices[dev_id].attached == QAT_DETACHED)
			break;
	}
	return dev_id;
}

struct qat_pci_device *
qat_get_qat_dev_from_pci_dev(struct rte_pci_device *pci_dev)
{
	char name[QAT_DEV_NAME_MAX_LEN];

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return qat_pci_get_named_dev(name);
}

struct qat_pci_device *
qat_pci_device_allocate(struct rte_pci_device *pci_dev)
{
	struct qat_pci_device *qat_dev;
	uint8_t qat_dev_id;
	char name[QAT_DEV_NAME_MAX_LEN];

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	if (qat_pci_get_named_dev(name) != NULL) {
		PMD_DRV_LOG(ERR, "QAT device with name %s already allocated!",
				name);
		return NULL;
	}

	qat_dev_id = qat_pci_find_free_device_index();
	if (qat_dev_id == QAT_MAX_PCI_DEVICES) {
		PMD_DRV_LOG(ERR, "Reached maximum number of QAT devices");
		return NULL;
	}

	qat_dev = qat_pci_get_dev(qat_dev_id);
	snprintf(qat_dev->name, QAT_DEV_NAME_MAX_LEN, "%s", name);
	qat_dev->pci_dev = pci_dev;
	switch (qat_dev->pci_dev->id.device_id) {
	case 0x0443:
		qat_dev->qat_dev_gen = QAT_GEN1;
		break;
	case 0x37c9:
	case 0x19e3:
	case 0x6f55:
		qat_dev->qat_dev_gen = QAT_GEN2;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid dev_id, can't determine generation");
		return NULL;
	}

	rte_spinlock_init(&qat_dev->arb_csr_lock);

	qat_dev->attached = QAT_ATTACHED;

	qat_nb_pci_devices++;

	PMD_DRV_LOG(DEBUG, "QAT device %d allocated, total QATs %d",
				qat_dev_id, qat_nb_pci_devices);

	return qat_dev;
}

int
qat_pci_device_release(struct rte_pci_device *pci_dev)
{
	struct qat_pci_device *qat_dev;
	char name[QAT_DEV_NAME_MAX_LEN];

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));
	qat_dev = qat_pci_get_named_dev(name);
	if (qat_dev != NULL) {

		/* Check that there are no service devs still on pci device */
		if (qat_dev->sym_dev != NULL)
			return -EBUSY;

		qat_dev->attached = QAT_DETACHED;
		qat_nb_pci_devices--;
	}
	PMD_DRV_LOG(DEBUG, "QAT device %s released, total QATs %d",
				name, qat_nb_pci_devices);
	return 0;
}
