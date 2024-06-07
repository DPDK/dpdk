/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_CRYPTO_H_
#define _IONIC_CRYPTO_H_

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_dev.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_log.h>

#include "ionic_common.h"
#include "ionic_regs.h"

/* Devargs */
/* NONE */

extern int iocpt_logtype;
#define RTE_LOGTYPE_IOCPT iocpt_logtype

#define IOCPT_PRINT(level, ...)						\
	RTE_LOG_LINE_PREFIX(level, IOCPT, "%s(): ", __func__, __VA_ARGS__)

#define IOCPT_PRINT_CALL() IOCPT_PRINT(DEBUG, " >>")

struct iocpt_dev_bars {
	struct ionic_dev_bar bar[IONIC_BARS_MAX];
	uint32_t num_bars;
};

#define IOCPT_DEV_F_INITED		BIT(0)
#define IOCPT_DEV_F_UP			BIT(1)
#define IOCPT_DEV_F_FW_RESET		BIT(2)

/* Combined dev / LIF object */
struct iocpt_dev {
	const char *name;
	struct iocpt_dev_bars bars;

	const struct iocpt_dev_intf *intf;
	void *bus_dev;
	struct rte_cryptodev *crypto_dev;

	uint32_t max_qps;
	uint32_t max_sessions;
	uint16_t state;
	uint8_t driver_id;
	uint8_t socket_id;

	uint64_t features;
	uint32_t hw_features;
};

struct iocpt_dev_intf {
	int  (*setup_bars)(struct iocpt_dev *dev);
	void (*unmap_bars)(struct iocpt_dev *dev);
};

static inline int
iocpt_setup_bars(struct iocpt_dev *dev)
{
	if (dev->intf->setup_bars == NULL)
		return -EINVAL;

	return (*dev->intf->setup_bars)(dev);
}

int iocpt_probe(void *bus_dev, struct rte_device *rte_dev,
	struct iocpt_dev_bars *bars, const struct iocpt_dev_intf *intf,
	uint8_t driver_id, uint8_t socket_id);
int iocpt_remove(struct rte_device *rte_dev);

void iocpt_configure(struct iocpt_dev *dev);
void iocpt_deinit(struct iocpt_dev *dev);

static inline bool
iocpt_is_embedded(void)
{
#if defined(RTE_LIBRTE_IONIC_PMD_EMBEDDED)
	return true;
#else
	return false;
#endif
}

#endif /* _IONIC_CRYPTO_H_ */
