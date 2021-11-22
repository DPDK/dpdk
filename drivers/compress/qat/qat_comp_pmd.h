/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_COMP_PMD_H_
#define _QAT_COMP_PMD_H_

#ifdef RTE_LIB_COMPRESSDEV

#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include "qat_device.h"
#include "qat_comp.h"

/**< Intel(R) QAT Compression PMD name */
#define COMPRESSDEV_NAME_QAT_PMD	compress_qat

/* Private data structure for a QAT compression device capability. */
struct qat_comp_capabilities_info {
	const struct rte_compressdev_capabilities *data;
	uint64_t size;
};

/**
 * Function prototypes for GENx specific compress device operations.
 **/
typedef struct qat_comp_capabilities_info (*get_comp_capabilities_info_t)
		(struct qat_pci_device *qat_dev);

typedef uint16_t (*get_comp_ram_bank_flags_t)(void);

typedef int (*set_comp_slice_cfg_word_t)(struct qat_comp_xform *qat_xform,
		const struct rte_comp_xform *xform,
		enum rte_comp_op_type op_type, uint32_t *comp_slice_cfg_word);

typedef unsigned int (*get_comp_num_im_bufs_required_t)(void);

typedef uint64_t (*get_comp_feature_flags_t)(void);

struct qat_comp_gen_dev_ops {
	struct rte_compressdev_ops *compressdev_ops;
	get_comp_feature_flags_t qat_comp_get_feature_flags;
	get_comp_capabilities_info_t qat_comp_get_capabilities;
	get_comp_ram_bank_flags_t qat_comp_get_ram_bank_flags;
	set_comp_slice_cfg_word_t qat_comp_set_slice_cfg_word;
	get_comp_num_im_bufs_required_t qat_comp_get_num_im_bufs_required;
};

extern struct qat_comp_gen_dev_ops qat_comp_gen_dev_ops[];

/** private data structure for a QAT compression device.
 * This QAT device is a device offering only a compression service,
 * there can be one of these on each qat_pci_device (VF).
 */
struct qat_comp_dev_private {
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
	struct rte_compressdev *compressdev;
	/**< The pointer to this compression device structure */
	const struct rte_compressdev_capabilities *qat_dev_capabilities;
	/* QAT device compression capabilities */
	const struct rte_memzone *interm_buff_mz;
	/**< The device's memory for intermediate buffers */
	struct rte_mempool *xformpool;
	/**< The device's pool for qat_comp_xforms */
	struct rte_mempool *streampool;
	/**< The device's pool for qat_comp_streams */
	const struct rte_memzone *capa_mz;
	/* Shared memzone for storing capabilities */
	uint16_t min_enq_burst_threshold;
};

int
qat_comp_dev_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config);

int
qat_comp_dev_start(struct rte_compressdev *dev __rte_unused);

void
qat_comp_dev_stop(struct rte_compressdev *dev __rte_unused);

int
qat_comp_dev_close(struct rte_compressdev *dev);

void
qat_comp_dev_info_get(struct rte_compressdev *dev,
		struct rte_compressdev_info *info);

void
qat_comp_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats);

void
qat_comp_stats_reset(struct rte_compressdev *dev);

int
qat_comp_qp_release(struct rte_compressdev *dev, uint16_t queue_pair_id);

int
qat_comp_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id);

const struct rte_memzone *
qat_comp_setup_inter_buffers(struct qat_comp_dev_private *comp_dev,
		uint32_t buff_size);

int
qat_comp_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param);

int
qat_comp_dev_destroy(struct qat_pci_device *qat_pci_dev);


static __rte_always_inline unsigned int
qat_comp_get_num_im_bufs_required(enum qat_device_gen gen)
{
	return (*qat_comp_gen_dev_ops[gen].qat_comp_get_num_im_bufs_required)();
}

#endif
#endif /* _QAT_COMP_PMD_H_ */
