/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ISAL_COMP_PMD_PRIVATE_H_
#define _ISAL_COMP_PMD_PRIVATE_H_

#define COMPDEV_NAME_ISAL_PMD		compress_isal
/**< ISA-L comp PMD device name */

extern int isal_logtype_driver;
#define ISAL_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, isal_logtype_driver, "%s(): "fmt "\n", \
			__func__, ##args)

/* private data structure for each ISA-L compression device */
struct isal_comp_private {
	struct rte_mempool *priv_xform_mp;
};

/** ISA-L queue pair */
struct isal_comp_qp {
	/* Queue Pair Identifier */
	uint16_t id;
	/* Unique Queue Pair Name */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/* Queue pair statistics */
	struct rte_compressdev_stats qp_stats;
	/* Number of free elements on ring */
	uint16_t num_free_elements;
} __rte_cache_aligned;

/** device specific operations function pointer structure */
extern struct rte_compressdev_ops *isal_compress_pmd_ops;

#endif /* _ISAL_COMP_PMD_PRIVATE_H_ */
