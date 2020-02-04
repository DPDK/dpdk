/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_ETHDEV_SEC_H__
#define __OTX2_ETHDEV_SEC_H__

#include <rte_ethdev.h>

int otx2_eth_sec_ctx_create(struct rte_eth_dev *eth_dev);

void otx2_eth_sec_ctx_destroy(struct rte_eth_dev *eth_dev);

int otx2_eth_sec_init(struct rte_eth_dev *eth_dev);

void otx2_eth_sec_fini(struct rte_eth_dev *eth_dev);

#endif /* __OTX2_ETHDEV_SEC_H__ */
