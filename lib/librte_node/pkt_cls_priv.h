/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell.
 */
#ifndef __INCLUDE_PKT_CLS_PRIV_H__
#define __INCLUDE_PKT_CLS_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>

struct pkt_cls_node_ctx {
	uint16_t l2l3_type;
};

enum pkt_cls_next_nodes {
	PKT_CLS_NEXT_PKT_DROP,
	PKT_CLS_NEXT_IP4_LOOKUP,
	PKT_CLS_NEXT_MAX,
};

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_PKT_CLS_PRIV_H__ */
