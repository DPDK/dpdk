/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_ETHDEV_H_
#define _NBL_ETHDEV_H_

#include "nbl_core.h"

#define ETH_DEV_TO_NBL_DEV_PF_PRIV(eth_dev) \
	((struct nbl_adapter *)((eth_dev)->data->dev_private))

#define NBL_MAX_JUMBO_FRAME_SIZE		(9600)
#define NBL_RSS_OFFLOAD_TYPE ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_FRAG_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV4_OTHER | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_FRAG_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV6_OTHER | \
	RTE_ETH_RSS_VXLAN | \
	RTE_ETH_RSS_GENEVE | \
	RTE_ETH_RSS_NVGRE)

#endif
