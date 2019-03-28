/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_DEVICE_H_
#define _QAT_DEVICE_H_

#include <rte_bus_pci.h>

#include "qat_common.h"
#include "qat_logs.h"
#include "adf_transport_access_macros.h"
#include "qat_qp.h"

#define QAT_DETACHED  (0)
#define QAT_ATTACHED  (1)

#define QAT_DEV_NAME_MAX_LEN	64

enum qat_comp_num_im_buffers {
	QAT_NUM_INTERM_BUFS_GEN1 = 12,
	QAT_NUM_INTERM_BUFS_GEN2 = 20,
	QAT_NUM_INTERM_BUFS_GEN3 = 20
};

/*
 * This struct holds all the data about a QAT pci device
 * including data about all services it supports.
 * It contains
 *  - hw_data
 *  - config data
 *  - runtime data
 */
struct qat_sym_dev_private;
struct qat_asym_dev_private;
struct qat_comp_dev_private;

struct qat_pci_device {

	/* Data used by all services */
	char name[QAT_DEV_NAME_MAX_LEN];
	/**< Name of qat pci device */
	uint8_t qat_dev_id;
	/**< Device instance for this qat pci device */
	struct rte_pci_device *pci_dev;
	/**< PCI information. */
	enum qat_device_gen qat_dev_gen;
	/**< QAT device generation */
	rte_spinlock_t arb_csr_lock;
	/**< lock to protect accesses to the arbiter CSR */
	__extension__
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */

	struct qat_qp *qps_in_use[QAT_MAX_SERVICES][ADF_MAX_QPS_ON_ANY_SERVICE];
	/**< links to qps set up for each service, index same as on API */

	/* Data relating to symmetric crypto service */
	struct qat_sym_dev_private *sym_dev;
	/**< link back to cryptodev private data */
	struct rte_device sym_rte_dev;
	/**< This represents the crypto sym subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a crypto-specific name
	 */

	/* Data relating to asymmetric crypto service */
	struct qat_asym_dev_private *asym_dev;
	/**< link back to cryptodev private data */
	struct rte_device asym_rte_dev;
	/**< This represents the crypto asym subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a crypto-specific name
	 */

	/* Data relating to compression service */
	struct qat_comp_dev_private *comp_dev;
	/**< link back to compressdev private data */
	struct rte_device comp_rte_dev;
	/**< This represents the compression subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a compression-specific name
	 */

	/* Data relating to asymmetric crypto service */

};

struct qat_gen_hw_data {
	enum qat_device_gen dev_gen;
	const struct qat_qp_hw_data (*qp_hw_data)[ADF_MAX_QPS_ON_ANY_SERVICE];
	enum qat_comp_num_im_buffers comp_num_im_bufs_required;
};

extern struct qat_gen_hw_data qat_gen_config[];

struct qat_pci_device *
qat_pci_device_allocate(struct rte_pci_device *pci_dev);

int
qat_pci_device_release(struct rte_pci_device *pci_dev);

struct qat_pci_device *
qat_get_qat_dev_from_pci_dev(struct rte_pci_device *pci_dev);

/* declaration needed for weak functions */
int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_comp_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_comp_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused);

#endif /* _QAT_DEVICE_H_ */
