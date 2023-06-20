/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_DEVICE_H_
#define _QAT_DEVICE_H_

#include <bus_pci_driver.h>

#include "qat_common.h"
#include "qat_logs.h"
#include "qat_qp.h"
#include "adf_transport_access_macros.h"
#include "icp_qat_hw.h"

#define QAT_DETACHED  (0)
#define QAT_ATTACHED  (1)

#define QAT_DEV_NAME_MAX_LEN	64

#define QAT_LEGACY_CAPA "qat_legacy_capa"
#define SYM_ENQ_THRESHOLD_NAME "qat_sym_enq_threshold"
#define ASYM_ENQ_THRESHOLD_NAME "qat_asym_enq_threshold"
#define COMP_ENQ_THRESHOLD_NAME "qat_comp_enq_threshold"
#define SYM_CIPHER_CRC_ENABLE_NAME "qat_sym_cipher_crc_enable"
#define QAT_CMD_SLICE_MAP "qat_cmd_slice_disable"
#define QAT_CMD_SLICE_MAP_POS	5
#define MAX_QP_THRESHOLD_SIZE	32

/**
 * Function prototypes for GENx specific device operations.
 **/
typedef int (*qat_dev_reset_ring_pairs_t)
		(struct qat_pci_device *);
typedef const struct rte_mem_resource* (*qat_dev_get_transport_bar_t)
		(struct rte_pci_device *);
typedef int (*qat_dev_get_misc_bar_t)
		(struct rte_mem_resource **, struct rte_pci_device *);
typedef int (*qat_dev_read_config_t)
		(struct qat_pci_device *);
typedef int (*qat_dev_get_extra_size_t)(void);
typedef int (*qat_dev_get_slice_map_t)(uint32_t *map,
		const struct rte_pci_device *pci_dev);

struct qat_dev_hw_spec_funcs {
	qat_dev_reset_ring_pairs_t	qat_dev_reset_ring_pairs;
	qat_dev_get_transport_bar_t	qat_dev_get_transport_bar;
	qat_dev_get_misc_bar_t		qat_dev_get_misc_bar;
	qat_dev_read_config_t		qat_dev_read_config;
	qat_dev_get_extra_size_t	qat_dev_get_extra_size;
	qat_dev_get_slice_map_t		qat_dev_get_slice_map;
};

extern struct qat_dev_hw_spec_funcs *qat_dev_hw_spec[];

struct qat_dev_cmd_param {
	const char *name;
	uint16_t val;
};

struct qat_device_info {
	const struct rte_memzone *mz;
	/**< mz to store the qat_pci_device so it can be
	 * shared across processes
	 */
	struct rte_pci_device *pci_dev;
	struct rte_device sym_rte_dev;
	/**< This represents the crypto sym subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a crypto-specific name
	 */

	struct rte_device asym_rte_dev;
	/**< This represents the crypto asym subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a crypto-specific name
	 */

	struct rte_device comp_rte_dev;
	/**< This represents the compression subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a compression-specific name
	 */
};

extern struct qat_device_info qat_pci_devs[];

struct qat_cryptodev_private;
struct qat_comp_dev_private;

/*
 * This struct holds all the data about a QAT pci device
 * including data about all services it supports.
 * It contains
 *  - hw_data
 *  - config data
 *  - runtime data
 * Note: as this data can be shared in a multi-process scenario,
 * any pointers in it must also point to shared memory.
 */
struct qat_pci_device {

	/* Data used by all services */
	char name[QAT_DEV_NAME_MAX_LEN];
	/**< Name of qat pci device */
	uint8_t qat_dev_id;
	/**< Id of device instance for this qat pci device */
	enum qat_device_gen qat_dev_gen;
	/**< QAT device generation */
	rte_spinlock_t arb_csr_lock;
	/**< lock to protect accesses to the arbiter CSR */

	struct qat_qp *qps_in_use[QAT_MAX_SERVICES][ADF_MAX_QPS_ON_ANY_SERVICE];
	/**< links to qps set up for each service, index same as on API */

	/* Data relating to symmetric crypto service */
	struct qat_cryptodev_private *sym_dev;
	/**< link back to cryptodev private data */

	int qat_sym_driver_id;
	/**< Symmetric driver id used by this device */

	/* Data relating to asymmetric crypto service */
	struct qat_cryptodev_private *asym_dev;
	/**< link back to cryptodev private data */

	int qat_asym_driver_id;
	/**< Symmetric driver id used by this device */

	/* Data relating to compression service */
	struct qat_comp_dev_private *comp_dev;
	/**< link back to compressdev private data */
	void *misc_bar_io_addr;
	/**< Address of misc bar */
	void *dev_private;
	/**< Per generation specific information */
};

struct qat_gen_hw_data {
	enum qat_device_gen dev_gen;
	const struct qat_qp_hw_data (*qp_hw_data)[ADF_MAX_QPS_ON_ANY_SERVICE];
	struct qat_pf2vf_dev *pf2vf_dev;
};

struct qat_pf2vf_dev {
	uint32_t pf2vf_offset;
	uint32_t vf2pf_offset;
	int pf2vf_type_shift;
	uint32_t pf2vf_type_mask;
	int pf2vf_data_shift;
	uint32_t pf2vf_data_mask;
};

extern struct qat_gen_hw_data qat_gen_config[];

struct qat_pci_device *
qat_pci_device_allocate(struct rte_pci_device *pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param);

struct qat_pci_device *
qat_get_qat_dev_from_pci_dev(struct rte_pci_device *pci_dev);

/* declaration needed for weak functions */
int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused,
		struct qat_dev_cmd_param *qat_dev_cmd_param);

int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev,
		const struct qat_dev_cmd_param *qat_dev_cmd_param);

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused);

int
qat_comp_dev_create(struct qat_pci_device *qat_pci_dev __rte_unused,
		struct qat_dev_cmd_param *qat_dev_cmd_param);

int
qat_comp_dev_destroy(struct qat_pci_device *qat_pci_dev __rte_unused);

#endif /* _QAT_DEVICE_H_ */
