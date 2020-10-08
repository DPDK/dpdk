/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _IOAT_PRIVATE_H_
#define _IOAT_PRIVATE_H_

/**
 * @file idxd_private.h
 *
 * Private data structures for the idxd/DSA part of ioat device driver
 *
 * @warning
 * @b EXPERIMENTAL: these structures and APIs may change without prior notice
 */

#include <rte_spinlock.h>
#include <rte_rawdev_pmd.h>
#include "rte_ioat_rawdev.h"

extern int ioat_pmd_logtype;

#define IOAT_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
		ioat_pmd_logtype, "%s(): " fmt "\n", __func__, ##args)

#define IOAT_PMD_DEBUG(fmt, args...)  IOAT_PMD_LOG(DEBUG, fmt, ## args)
#define IOAT_PMD_INFO(fmt, args...)   IOAT_PMD_LOG(INFO, fmt, ## args)
#define IOAT_PMD_ERR(fmt, args...)    IOAT_PMD_LOG(ERR, fmt, ## args)
#define IOAT_PMD_WARN(fmt, args...)   IOAT_PMD_LOG(WARNING, fmt, ## args)

struct idxd_pci_common {
	rte_spinlock_t lk;
	volatile struct rte_idxd_bar0 *regs;
	volatile struct rte_idxd_wqcfg *wq_regs;
	volatile struct rte_idxd_grpcfg *grp_regs;
	volatile void *portals;
};

struct idxd_rawdev {
	struct rte_idxd_rawdev public; /* the public members, must be first */

	struct rte_rawdev *rawdev;
	const struct rte_memzone *mz;
	uint8_t qid;
	uint16_t max_batches;

	union {
		struct idxd_pci_common *pci;
	} u;
};

extern int idxd_rawdev_create(const char *name, struct rte_device *dev,
		       const struct idxd_rawdev *idxd,
		       const struct rte_rawdev_ops *ops);

extern int idxd_rawdev_close(struct rte_rawdev *dev);

extern int idxd_rawdev_test(uint16_t dev_id);

#endif /* _IOAT_PRIVATE_H_ */
