/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_ETHDEV_H__
#define __OTX2_ETHDEV_H__

#include <stdint.h>

#include <rte_common.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_irq.h"
#include "otx2_mempool.h"

struct otx2_eth_dev {
	OTX2_DEV; /* Base class */
} __rte_cache_aligned;

static inline struct otx2_eth_dev *
otx2_eth_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

#endif /* __OTX2_ETHDEV_H__ */
