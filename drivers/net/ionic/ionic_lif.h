/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_LIF_H_
#define _IONIC_LIF_H_

#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_ether.h>

#include "ionic_osdep.h"
#include "ionic_dev.h"

#define IONIC_LIF_F_INITED		BIT(0)

#define IONIC_LIF_NAME_MAX_SZ		(32)

struct ionic_lif {
	struct ionic_adapter *adapter;
	struct rte_eth_dev *eth_dev;
	uint16_t port_id;  /**< Device port identifier */
	uint32_t index;
	uint32_t hw_index;
	uint32_t state;
	char name[IONIC_LIF_NAME_MAX_SZ];
	uint32_t info_sz;
	struct ionic_lif_info *info;
	rte_iova_t info_pa;
	const struct rte_memzone *info_z;
};

int ionic_lif_identify(struct ionic_adapter *adapter);
int ionic_lifs_size(struct ionic_adapter *ionic);

int ionic_lif_alloc(struct ionic_lif *lif);
void ionic_lif_free(struct ionic_lif *lif);

int ionic_lif_init(struct ionic_lif *lif);
void ionic_lif_deinit(struct ionic_lif *lif);

int ionic_lif_start(struct ionic_lif *lif);

int ionic_lif_configure(struct ionic_lif *lif);
void ionic_lif_reset(struct ionic_lif *lif);

#endif /* _IONIC_LIF_H_ */
