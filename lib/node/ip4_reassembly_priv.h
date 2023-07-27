/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#ifndef __INCLUDE_IP4_REASSEMBLY_PRIV_H__
#define __INCLUDE_IP4_REASSEMBLY_PRIV_H__

/**
 * @internal
 *
 * Ip4_reassembly context structure.
 */
struct ip4_reassembly_ctx {
	struct rte_ip_frag_tbl *tbl;
	struct rte_ip_frag_death_row *dr;
};

/**
 * @internal
 *
 * Get the IP4 reassembly node
 *
 * @return
 *   Pointer to the IP4 reassembly node.
 */
struct rte_node_register *ip4_reassembly_node_get(void);

#endif /* __INCLUDE_IP4_REASSEMBLY_PRIV_H__ */
