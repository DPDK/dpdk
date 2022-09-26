/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Intel Corporation
 */

#ifndef AFU_PMD_CORE_H
#define AFU_PMD_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <rte_spinlock.h>
#include <rte_cycles.h>
#include <bus_ifpga_driver.h>
#include <rte_rawdev.h>

#include "ifpga_rawdev.h"

#define AFU_RAWDEV_MAX_DRVS  32

struct afu_rawdev;

struct afu_ops {
	int (*init)(struct afu_rawdev *dev);
	int (*config)(struct afu_rawdev *dev, void *config,
		size_t config_size);
	int (*start)(struct afu_rawdev *dev);
	int (*stop)(struct afu_rawdev *dev);
	int (*test)(struct afu_rawdev *dev);
	int (*close)(struct afu_rawdev *dev);
	int (*reset)(struct afu_rawdev *dev);
	int (*dump)(struct afu_rawdev *dev, FILE *f);
};

struct afu_shared_data {
	rte_spinlock_t lock;  /* lock for multi-process access */
};

struct afu_rawdev_drv {
	TAILQ_ENTRY(afu_rawdev_drv) next;
	struct rte_afu_uuid uuid;
	struct afu_ops *ops;
};

struct afu_rawdev {
	struct rte_rawdev *rawdev;  /* point to parent raw device */
	struct afu_shared_data *sd;  /* shared data for multi-process */
	struct afu_ops *ops;  /* device operation functions */
	int port;  /* index of port the AFU attached */
	void *addr;  /* base address of AFU registers */
	void *priv;  /* private driver data */
};

static inline struct afu_rawdev *
afu_rawdev_get_priv(const struct rte_rawdev *rawdev)
{
	return rawdev ? (struct afu_rawdev *)rawdev->dev_private : NULL;
}

#define CLS_TO_SIZE(n)  ((n) << 6)  /* get size of n cache lines */
#define SIZE_TO_CLS(s)  ((s) >> 6)  /* convert size to number of cache lines */
#define MHZ(f)  ((f) * 1000000)

#define dsm_poll_timeout(addr, val, cond, invl, timeout) \
({                                                       \
	uint64_t __wait = 0;                                 \
	uint64_t __invl = (invl);                            \
	uint64_t __timeout = (timeout);                      \
	for (; __wait <= __timeout; __wait += __invl) {      \
		(val) = *(addr);                                 \
		if (cond)                                        \
			break;                                       \
		rte_delay_ms(__invl);                            \
	}                                                    \
	(cond) ? 0 : 1;                                      \
})

void afu_pmd_register(struct afu_rawdev_drv *driver);
void afu_pmd_unregister(struct afu_rawdev_drv *driver);

#define AFU_PMD_REGISTER(drv)\
RTE_INIT(afupmdinitfunc_ ##drv)\
{\
	afu_pmd_register(&drv);\
}

#ifdef __cplusplus
}
#endif

#endif /* AFU_PMD_CORE_H */
