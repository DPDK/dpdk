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

/** device specific operations function pointer structure */
extern struct rte_compressdev_ops *isal_compress_pmd_ops;

#endif /* _ISAL_COMP_PMD_PRIVATE_H_ */
