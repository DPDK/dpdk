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

extern int ioat_rawdev_logtype;

#define IOAT_PMD_LOG(level, fmt, args...) rte_log(RTE_LOG_ ## level, \
		ioat_rawdev_logtype, "IOAT: %s(): " fmt "\n", __func__, ##args)

#define IOAT_PMD_DEBUG(fmt, args...)  IOAT_PMD_LOG(DEBUG, fmt, ## args)
#define IOAT_PMD_INFO(fmt, args...)   IOAT_PMD_LOG(INFO, fmt, ## args)
#define IOAT_PMD_ERR(fmt, args...)    IOAT_PMD_LOG(ERR, fmt, ## args)
#define IOAT_PMD_WARN(fmt, args...)   IOAT_PMD_LOG(WARNING, fmt, ## args)

struct idxd_pci_common {
	rte_spinlock_t lk;

	uint8_t wq_cfg_sz;
	volatile struct rte_idxd_bar0 *regs;
	volatile uint32_t *wq_regs_base;
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
		struct {
			unsigned int dsa_id;
		} vdev;

		struct idxd_pci_common *pci;
	} u;
};

int ioat_xstats_get(const struct rte_rawdev *dev, const unsigned int ids[],
		uint64_t values[], unsigned int n);

int ioat_xstats_get_names(const struct rte_rawdev *dev,
		struct rte_rawdev_xstats_name *names,
		unsigned int size);

int ioat_xstats_reset(struct rte_rawdev *dev, const uint32_t *ids,
		uint32_t nb_ids);

extern int ioat_rawdev_test(uint16_t dev_id);

extern int idxd_rawdev_create(const char *name, struct rte_device *dev,
		       const struct idxd_rawdev *idxd,
		       const struct rte_rawdev_ops *ops);

extern int idxd_rawdev_close(struct rte_rawdev *dev);

extern int idxd_dev_configure(const struct rte_rawdev *dev,
		rte_rawdev_obj_t config, size_t config_size);

extern int idxd_dev_info_get(struct rte_rawdev *dev, rte_rawdev_obj_t dev_info,
		size_t info_size);

extern int idxd_dev_dump(struct rte_rawdev *dev, FILE *f);

#endif /* _IOAT_PRIVATE_H_ */
