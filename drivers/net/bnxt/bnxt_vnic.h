/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_VNIC_H_
#define _BNXT_VNIC_H_

#include <sys/queue.h>
#include <stdbool.h>

struct bnxt_vnic_info {
	STAILQ_ENTRY(bnxt_vnic_info)	next;
	uint8_t		ff_pool_idx;

	uint16_t	fw_vnic_id; /* returned by Chimp during alloc */
	uint16_t	rss_rule;
	uint16_t	start_grp_id;
	uint16_t	end_grp_id;
	uint16_t	*fw_grp_ids;
	uint16_t	dflt_ring_grp;
	uint16_t	mru;
	uint16_t	hash_type;
	uint8_t		hash_mode;
	rte_iova_t	rss_table_dma_addr;
	uint16_t	*rss_table;
	rte_iova_t	rss_hash_key_dma_addr;
	void		*rss_hash_key;
	rte_iova_t	mc_list_dma_addr;
	char		*mc_list;
	uint32_t	mc_addr_cnt;
#define BNXT_MAX_MC_ADDRS		16
	uint32_t	flags;
#define BNXT_VNIC_INFO_PROMISC			(1 << 0)
#define BNXT_VNIC_INFO_ALLMULTI			(1 << 1)
#define BNXT_VNIC_INFO_BCAST			(1 << 2)
#define BNXT_VNIC_INFO_UCAST			(1 << 3)
#define BNXT_VNIC_INFO_MCAST			(1 << 4)
#define BNXT_VNIC_INFO_TAGGED			(1 << 5)
#define BNXT_VNIC_INFO_UNTAGGED			(1 << 6)

	uint16_t	cos_rule;
	uint16_t	lb_rule;
	bool		vlan_strip;
	bool		func_default;
	bool		bd_stall;
	bool		roce_dual;
	bool		roce_only;
	bool		rss_dflt_cr;

	STAILQ_HEAD(, bnxt_filter_info)	filter;
	STAILQ_HEAD(, rte_flow)	flow_list;
};

struct bnxt;
void bnxt_init_vnics(struct bnxt *bp);
int bnxt_free_vnic(struct bnxt *bp, struct bnxt_vnic_info *vnic,
			  int pool);
struct bnxt_vnic_info *bnxt_alloc_vnic(struct bnxt *bp);
void bnxt_free_all_vnics(struct bnxt *bp);
void bnxt_free_vnic_attributes(struct bnxt *bp);
int bnxt_alloc_vnic_attributes(struct bnxt *bp);
void bnxt_free_vnic_mem(struct bnxt *bp);
int bnxt_alloc_vnic_mem(struct bnxt *bp);
#endif
