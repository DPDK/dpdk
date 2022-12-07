/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_MTR_H__
#define __NFP_MTR_H__

#include <rte_mtr.h>

/**
 * The max meter count is determined by firmware.
 * The max count is 65536 defined by OF_METER_COUNT.
 */
#define NFP_MAX_MTR_CNT                65536
#define NFP_MAX_POLICY_CNT             NFP_MAX_MTR_CNT
#define NFP_MAX_PROFILE_CNT            NFP_MAX_MTR_CNT

/**
 * See RFC 2698 for more details.
 * Word[0](Flag options):
 * [15] p(pps) 1 for pps, 0 for bps
 *
 * Meter control message
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-------------------------------+-+---+-----+-+---------+-+---+-+
 * |            Reserved           |p| Y |TYPE |E|  TSHFV  |P| PC|R|
 * +-------------------------------+-+---+-----+-+---------+-+---+-+
 * |                           Profile ID                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Token Bucket Peak                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Token Bucket Committed                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Peak Burst Size                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Committed Burst Size                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Peak Information Rate                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Committed Information Rate                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct nfp_cfg_head {
	rte_be32_t flags_opts;
	rte_be32_t profile_id;
};

/**
 * Struct nfp_profile_conf - profile config, offload to NIC
 * @head:        config head information
 * @bkt_tkn_p:   token bucket peak
 * @bkt_tkn_c:   token bucket committed
 * @pbs:         peak burst size
 * @cbs:         committed burst size
 * @pir:         peak information rate
 * @cir:         committed information rate
 */
struct nfp_profile_conf {
	struct nfp_cfg_head head;
	rte_be32_t bkt_tkn_p;
	rte_be32_t bkt_tkn_c;
	rte_be32_t pbs;
	rte_be32_t cbs;
	rte_be32_t pir;
	rte_be32_t cir;
};

/**
 * Struct nfp_mtr_stats_reply - meter stats, read from firmware
 * @head:          config head information
 * @pass_bytes:    count of passed bytes
 * @pass_pkts:     count of passed packets
 * @drop_bytes:    count of dropped bytes
 * @drop_pkts:     count of dropped packets
 */
struct nfp_mtr_stats_reply {
	struct nfp_cfg_head head;
	rte_be64_t pass_bytes;
	rte_be64_t pass_pkts;
	rte_be64_t drop_bytes;
	rte_be64_t drop_pkts;
};

/**
 * Struct nfp_mtr_profile - meter profile, stored in driver
 * Can only be used by one meter
 * @next:        next meter profile object
 * @profile_id:  meter profile id
 * @conf:        meter profile config
 * @in_use:      if profile is been used by meter
 */
struct nfp_mtr_profile {
	LIST_ENTRY(nfp_mtr_profile) next;
	uint32_t profile_id;
	struct nfp_profile_conf conf;
	bool in_use;
};

/**
 * Struct nfp_mtr_policy - meter policy information
 * @next:        next meter policy object
 * @policy_id:   meter policy id
 * @ref_cnt:     reference count by meter
 * @policy:      RTE_FLOW policy information
 */
struct nfp_mtr_policy {
	LIST_ENTRY(nfp_mtr_policy) next;
	uint32_t policy_id;
	uint32_t ref_cnt;
	struct rte_mtr_meter_policy_params policy;
};

/**
 * Struct nfp_mtr_stats - meter stats information
 * @pass_bytes:        count of passed bytes for meter
 * @pass_pkts:         count of passed packets for meter
 * @drop_bytes:        count of dropped bytes for meter
 * @drop_pkts:         count of dropped packets for meter
 */
struct nfp_mtr_stats {
	uint64_t pass_bytes;
	uint64_t pass_pkts;
	uint64_t drop_bytes;
	uint64_t drop_pkts;
};

/**
 * Struct nfp_mtr - meter object information
 * @next:        next meter object
 * @mtr_id:      meter id
 * @ref_cnt:     reference count by flow
 * @shared:      if meter can be used by multiple flows
 * @enable:      if meter is enable to use
 * @mtr_profile: the pointer of profile
 * @mtr_policy:  the pointer of policy
 * @stats_mask:  supported meter stats mask
 * @curr:        current meter stats
 * @prev:        previous meter stats
 */
struct nfp_mtr {
	LIST_ENTRY(nfp_mtr) next;
	uint32_t mtr_id;
	uint32_t ref_cnt;
	bool shared;
	bool enable;
	struct nfp_mtr_profile *mtr_profile;
	struct nfp_mtr_policy *mtr_policy;
	uint64_t stats_mask;
	struct {
		struct nfp_mtr_stats curr;
		struct nfp_mtr_stats prev;
	} mtr_stats;
};

/**
 * Struct nfp_mtr_priv - meter private data
 * @profiles:        the head node of profile list
 * @policies:        the head node of policy list
 * @mtrs:            the head node of mtrs list
 * @mtr_stats_lock:  spinlock for meter stats
 * @drain_tsc:       clock period
 */
struct nfp_mtr_priv {
	LIST_HEAD(, nfp_mtr_profile) profiles;
	LIST_HEAD(, nfp_mtr_policy) policies;
	LIST_HEAD(, nfp_mtr) mtrs;
	rte_spinlock_t mtr_stats_lock;
	uint64_t drain_tsc;
};

int nfp_net_mtr_ops_get(struct rte_eth_dev *dev, void *arg);
int nfp_mtr_priv_init(struct nfp_pf_dev *pf_dev);
void nfp_mtr_priv_uninit(struct nfp_pf_dev *pf_dev);
struct nfp_mtr *nfp_mtr_find_by_mtr_id(struct nfp_mtr_priv *priv,
		uint32_t mtr_id);
struct nfp_mtr *nfp_mtr_find_by_profile_id(struct nfp_mtr_priv *priv,
		uint32_t profile_id);
int nfp_mtr_update_ref_cnt(struct nfp_mtr_priv *priv,
		uint32_t mtr_id, bool add);

#endif /* __NFP_MTR_H__ */
