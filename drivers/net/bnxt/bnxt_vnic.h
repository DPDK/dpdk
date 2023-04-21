/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_VNIC_H_
#define _BNXT_VNIC_H_

#include <sys/queue.h>
#include <stdbool.h>
#include <rte_hash.h>

#define INVALID_VNIC_ID			((uint16_t)-1)
#define BNXT_RSS_LEVEL_INNERMOST	0x2
#define BNXT_RSS_LEVEL_OUTERMOST	0x1
#define BNXT_VNIC_MAX_QUEUE_SIZE	256
#define BNXT_VNIC_MAX_QUEUE_SZ_IN_8BITS	(BNXT_VNIC_MAX_QUEUE_SIZE / 8)
#define BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS (BNXT_VNIC_MAX_QUEUE_SIZE / 64)
/* Limit the number of vnic creations*/
#define BNXT_VNIC_MAX_SUPPORTED_ID	64

#define	BNXT_HASH_MODE_DEFAULT	HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_DEFAULT
#define	BNXT_HASH_MODE_INNERMOST	\
		(HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_4 |	\
		HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_INNERMOST_2)
#define	BNXT_HASH_MODE_OUTERMOST	\
		(HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_4 |	\
		HWRM_VNIC_RSS_CFG_INPUT_HASH_MODE_FLAGS_OUTERMOST_2)
#define	BNXT_VNIC_OUTER_RSS_UNSUPPORTED(bp)					\
	((BNXT_PF(bp) && !((bp)->vnic_cap_flags & BNXT_VNIC_CAP_OUTER_RSS)) ||	\
	 (BNXT_VF(bp) && BNXT_VF_IS_TRUSTED(bp) &&				\
	  !((bp)->vnic_cap_flags & BNXT_VNIC_CAP_OUTER_RSS_TRUSTED_VF)) ||	\
	 (BNXT_VF(bp) && !BNXT_VF_IS_TRUSTED(bp)))

struct bnxt_vnic_info {
	STAILQ_ENTRY(bnxt_vnic_info)	next;
	uint8_t		ff_pool_idx;

	uint16_t	fw_vnic_id; /* returned by Chimp during alloc */
	uint16_t	rss_rule;
	uint16_t	start_grp_id;
	uint16_t	end_grp_id;
	uint16_t	*fw_grp_ids;
	uint16_t	num_lb_ctxts;
	uint16_t	dflt_ring_grp;
	uint16_t	mru;
	uint16_t	hash_type;
	uint8_t		hash_mode;
	uint8_t		prev_hash_mode;
	const struct rte_memzone *rss_mz;
	rte_iova_t	rss_table_dma_addr;
	uint16_t	*rss_table;
	rte_iova_t	rss_hash_key_dma_addr;
	void		*rss_hash_key;
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
	uint16_t	rx_queue_cnt;
	uint16_t	cos_queue_id;
	bool		vlan_strip;
	bool		func_default;
	bool		bd_stall;
	bool		rss_dflt_cr;
	uint16_t	ref_cnt;
	uint64_t	queue_bitmap[BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS];

	STAILQ_HEAD(, bnxt_filter_info)	filter;
	STAILQ_HEAD(, rte_flow)	flow_list;
};

struct bnxt_vnic_queue_db {
	uint16_t	num_queues;
	uint16_t	dflt_vnic_id;
	struct rte_hash *rss_q_db;
};

/* RSS structure to pass values as an structure argument*/
struct bnxt_vnic_rss_info {
	uint32_t rss_level;
	uint64_t rss_types;
	uint32_t key_len; /**< Hash key length in bytes. */
	const uint8_t *key; /**< Hash key. */
	uint32_t queue_num; /**< Number of entries in @p queue. */
	uint64_t queue_list[BNXT_VNIC_MAX_QUEUE_SZ_IN_64BITS];
};

struct bnxt;
int bnxt_free_vnic(struct bnxt *bp, struct bnxt_vnic_info *vnic,
			  int pool);
struct bnxt_vnic_info *bnxt_alloc_vnic(struct bnxt *bp);
void bnxt_free_all_vnics(struct bnxt *bp);
void bnxt_free_vnic_attributes(struct bnxt *bp);
int bnxt_alloc_vnic_attributes(struct bnxt *bp, bool reconfig);
void bnxt_free_vnic_mem(struct bnxt *bp);
int bnxt_alloc_vnic_mem(struct bnxt *bp);
int bnxt_vnic_grp_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic);
void bnxt_prandom_bytes(void *dest_ptr, size_t len);
uint16_t bnxt_rte_to_hwrm_hash_types(uint64_t rte_type);
int bnxt_rte_to_hwrm_hash_level(struct bnxt *bp, uint64_t hash_f, uint32_t lvl);
uint64_t bnxt_hwrm_to_rte_rss_level(struct bnxt *bp, uint32_t mode);

int32_t bnxt_vnic_queue_db_init(struct bnxt *bp);
int32_t bnxt_vnic_queue_db_deinit(struct bnxt *bp);

void bnxt_vnic_queue_db_update_dlft_vnic(struct bnxt *bp);
int32_t
bnxt_vnic_rss_queue_status_update(struct bnxt *bp, struct bnxt_vnic_info *vnic);

int32_t bnxt_vnic_queue_action_alloc(struct bnxt *bp, uint16_t q_index,
				     uint16_t *vnic_idx,
				     uint16_t *vnicid);
int32_t bnxt_vnic_queue_action_free(struct bnxt *bp, uint16_t q_index);

int32_t bnxt_vnic_rss_action_alloc(struct bnxt *bp,
				   struct bnxt_vnic_rss_info *rss_info,
				   uint16_t *queue_id,
				   uint16_t *vnicid);
int32_t bnxt_vnic_rss_action_free(struct bnxt *bp, uint16_t q_index);

int32_t bnxt_vnic_reta_config_update(struct bnxt *bp,
				     struct bnxt_vnic_info *vnic_info,
				     struct rte_eth_rss_reta_entry64 *reta_conf,
				     uint16_t reta_size);
int32_t bnxt_vnic_queue_id_is_valid(struct bnxt_vnic_info *vnic_info,
				    uint16_t queue_id);
void bnxt_vnic_ring_grp_populate(struct bnxt *bp, struct bnxt_vnic_info *vnic);
void bnxt_vnic_rules_init(struct bnxt_vnic_info *vnic);
int32_t bnxt_vnic_mru_config(struct bnxt *bp, uint16_t new_mtu);
struct bnxt_vnic_info *bnxt_vnic_queue_db_get_vnic(struct bnxt *bp,
						   uint16_t vnic_idx);
struct bnxt_vnic_info *
bnxt_vnic_queue_id_get_next(struct bnxt *bp, uint16_t queue_id,
			    uint16_t *vnic_idx);
void bnxt_vnic_tpa_cfg(struct bnxt *bp, uint16_t queue_id, bool flag);

#endif
