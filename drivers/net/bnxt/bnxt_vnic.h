/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Broadcom Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#define MAX_NUM_TRAFFIC_CLASSES		8
#define MAX_NUM_RSS_QUEUES_PER_VNIC	16
#define MAX_QUEUES_PER_VNIC	(MAX_NUM_RSS_QUEUES_PER_VNIC + \
				 MAX_NUM_TRAFFIC_CLASSES)
	uint16_t	start_grp_id;
	uint16_t	end_grp_id;
	uint16_t	fw_grp_ids[MAX_QUEUES_PER_VNIC];
	uint16_t	dflt_ring_grp;
	uint16_t	mru;
	uint16_t	hash_type;
	phys_addr_t	rss_table_dma_addr;
	uint16_t	*rss_table;
	phys_addr_t	rss_hash_key_dma_addr;
	void		*rss_hash_key;
	phys_addr_t	mc_list_dma_addr;
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
