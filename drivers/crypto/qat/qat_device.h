/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_DEVICE_H_
#define _QAT_DEVICE_H_

#include <rte_cryptodev_pmd.h>
#include <rte_bus_pci.h>
#include "qat_common.h"
#include "qat_logs.h"
#include "adf_transport_access_macros.h"
#include "qat_qp.h"


#define QAT_DETACHED  (0)
#define QAT_ATTACHED  (1)

#define QAT_MAX_PCI_DEVICES	48
#define QAT_DEV_NAME_MAX_LEN	64


extern uint8_t cryptodev_qat_driver_id;

extern int qat_sym_qp_release(struct rte_cryptodev *dev,
	uint16_t queue_pair_id);

/*
 * This struct holds all the data about a QAT pci device
 * including data about all services it supports.
 * It contains
 *  - hw_data
 *  - config data
 *  - runtime data
 */
struct qat_pci_device {

	/* data used by all services */
	char name[QAT_DEV_NAME_MAX_LEN];
	/**< Name of qat pci device */
	struct rte_pci_device *pci_dev;
	/**< PCI information. */
	enum qat_device_gen qat_dev_gen;
	/**< QAT device generation */
	rte_spinlock_t arb_csr_lock;
	/* protects accesses to the arbiter CSR */
	__extension__
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */

	/* data relating to symmetric crypto service */
	struct qat_pmd_private *sym_dev;
	/**< link back to cryptodev private data */
	unsigned int max_nb_sym_queue_pairs;
	/**< Max number of queue pairs supported by device */

	/* data relating to compression service */

	/* data relating to asymmetric crypto service */

};

/** private data structure for a QAT device.
 * This QAT device is a device offering only symmetric crypto service,
 * there can be one of these on each qat_pci_device (VF),
 * in future there may also be private data structures for other services.
 */
struct qat_pmd_private {
	unsigned int max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned int max_nb_sessions;
	/**< Max number of sessions supported by device */
	enum qat_device_gen qat_dev_gen;
	/**< QAT device generation */
	const struct rte_cryptodev_capabilities *qat_dev_capabilities;
	/* QAT device capabilities */
	struct rte_pci_device *pci_dev;
	/**< PCI information. */
	uint8_t dev_id;
	/**< Device ID for this instance */
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
};

struct qat_gen_hw_data {
	enum qat_device_gen dev_gen;
	const struct qat_qp_hw_data (*qp_hw_data)[ADF_MAX_QPS_PER_BUNDLE];
};

extern struct qat_gen_hw_data qp_gen_config[];

int qat_dev_config(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);
int qat_dev_start(struct rte_cryptodev *dev);
void qat_dev_stop(struct rte_cryptodev *dev);
int qat_dev_close(struct rte_cryptodev *dev);
void qat_dev_info_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_info *info);

struct qat_pci_device *
qat_pci_device_allocate(struct rte_pci_device *pci_dev);
int
qat_pci_device_release(struct rte_pci_device *pci_dev);
struct qat_pci_device *
qat_get_qat_dev_from_pci_dev(struct rte_pci_device *pci_dev);

#endif /* _QAT_DEVICE_H_ */
