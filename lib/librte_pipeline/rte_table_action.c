/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_esp.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "rte_table_action.h"

#define rte_htons rte_cpu_to_be_16
#define rte_htonl rte_cpu_to_be_32

#define rte_ntohs rte_be_to_cpu_16
#define rte_ntohl rte_be_to_cpu_32

/**
 * RTE_TABLE_ACTION_FWD
 */
#define fwd_data rte_pipeline_table_entry

static int
fwd_apply(struct fwd_data *data,
	struct rte_table_action_fwd_params *p)
{
	data->action = p->action;

	if (p->action == RTE_PIPELINE_ACTION_PORT)
		data->port_id = p->id;

	if (p->action == RTE_PIPELINE_ACTION_TABLE)
		data->table_id = p->id;

	return 0;
}

/**
 * RTE_TABLE_ACTION_LB
 */
static int
lb_cfg_check(struct rte_table_action_lb_config *cfg)
{
	if ((cfg == NULL) ||
		(cfg->key_size < RTE_TABLE_ACTION_LB_KEY_SIZE_MIN) ||
		(cfg->key_size > RTE_TABLE_ACTION_LB_KEY_SIZE_MAX) ||
		(!rte_is_power_of_2(cfg->key_size)) ||
		(cfg->f_hash == NULL))
		return -1;

	return 0;
}

struct lb_data {
	uint32_t out[RTE_TABLE_ACTION_LB_TABLE_SIZE];
} __attribute__((__packed__));

static int
lb_apply(struct lb_data *data,
	struct rte_table_action_lb_params *p)
{
	memcpy(data->out, p->out, sizeof(data->out));

	return 0;
}

static __rte_always_inline void
pkt_work_lb(struct rte_mbuf *mbuf,
	struct lb_data *data,
	struct rte_table_action_lb_config *cfg)
{
	uint8_t *pkt_key = RTE_MBUF_METADATA_UINT8_PTR(mbuf, cfg->key_offset);
	uint32_t *out = RTE_MBUF_METADATA_UINT32_PTR(mbuf, cfg->out_offset);
	uint64_t digest, pos;
	uint32_t out_val;

	digest = cfg->f_hash(pkt_key,
		cfg->key_mask,
		cfg->key_size,
		cfg->seed);
	pos = digest & (RTE_TABLE_ACTION_LB_TABLE_SIZE - 1);
	out_val = data->out[pos];

	*out = out_val;
}

/**
 * RTE_TABLE_ACTION_MTR
 */
static int
mtr_cfg_check(struct rte_table_action_mtr_config *mtr)
{
	if ((mtr->alg == RTE_TABLE_ACTION_METER_SRTCM) ||
		((mtr->n_tc != 1) && (mtr->n_tc != 4)) ||
		(mtr->n_bytes_enabled != 0))
		return -ENOTSUP;
	return 0;
}

#define MBUF_SCHED_QUEUE_TC_COLOR(queue, tc, color)        \
	((uint16_t)((((uint64_t)(queue)) & 0x3) |          \
	((((uint64_t)(tc)) & 0x3) << 2) |                  \
	((((uint64_t)(color)) & 0x3) << 4)))

#define MBUF_SCHED_COLOR(sched, color)                     \
	(((sched) & (~0x30LLU)) | ((color) << 4))

struct mtr_trtcm_data {
	struct rte_meter_trtcm trtcm;
	uint64_t stats[e_RTE_METER_COLORS];
} __attribute__((__packed__));

#define MTR_TRTCM_DATA_METER_PROFILE_ID_GET(data)          \
	(((data)->stats[e_RTE_METER_GREEN] & 0xF8LLU) >> 3)

static void
mtr_trtcm_data_meter_profile_id_set(struct mtr_trtcm_data *data,
	uint32_t profile_id)
{
	data->stats[e_RTE_METER_GREEN] &= ~0xF8LLU;
	data->stats[e_RTE_METER_GREEN] |= (profile_id % 32) << 3;
}

#define MTR_TRTCM_DATA_POLICER_ACTION_DROP_GET(data, color)\
	(((data)->stats[(color)] & 4LLU) >> 2)

#define MTR_TRTCM_DATA_POLICER_ACTION_COLOR_GET(data, color)\
	((enum rte_meter_color)((data)->stats[(color)] & 3LLU))

static void
mtr_trtcm_data_policer_action_set(struct mtr_trtcm_data *data,
	enum rte_meter_color color,
	enum rte_table_action_policer action)
{
	if (action == RTE_TABLE_ACTION_POLICER_DROP) {
		data->stats[color] |= 4LLU;
	} else {
		data->stats[color] &= ~7LLU;
		data->stats[color] |= color & 3LLU;
	}
}

static uint64_t
mtr_trtcm_data_stats_get(struct mtr_trtcm_data *data,
	enum rte_meter_color color)
{
	return data->stats[color] >> 8;
}

static void
mtr_trtcm_data_stats_reset(struct mtr_trtcm_data *data,
	enum rte_meter_color color)
{
	data->stats[color] &= 0xFFLU;
}

#define MTR_TRTCM_DATA_STATS_INC(data, color)              \
	((data)->stats[(color)] += (1LLU << 8))

static size_t
mtr_data_size(struct rte_table_action_mtr_config *mtr)
{
	return mtr->n_tc * sizeof(struct mtr_trtcm_data);
}

struct dscp_table_entry_data {
	enum rte_meter_color color;
	uint16_t tc;
	uint16_t queue_tc_color;
};

struct dscp_table_data {
	struct dscp_table_entry_data entry[64];
};

struct meter_profile_data {
	struct rte_meter_trtcm_profile profile;
	uint32_t profile_id;
	int valid;
};

static struct meter_profile_data *
meter_profile_data_find(struct meter_profile_data *mp,
	uint32_t mp_size,
	uint32_t profile_id)
{
	uint32_t i;

	for (i = 0; i < mp_size; i++) {
		struct meter_profile_data *mp_data = &mp[i];

		if (mp_data->valid && (mp_data->profile_id == profile_id))
			return mp_data;
	}

	return NULL;
}

static struct meter_profile_data *
meter_profile_data_find_unused(struct meter_profile_data *mp,
	uint32_t mp_size)
{
	uint32_t i;

	for (i = 0; i < mp_size; i++) {
		struct meter_profile_data *mp_data = &mp[i];

		if (!mp_data->valid)
			return mp_data;
	}

	return NULL;
}

static int
mtr_apply_check(struct rte_table_action_mtr_params *p,
	struct rte_table_action_mtr_config *cfg,
	struct meter_profile_data *mp,
	uint32_t mp_size)
{
	uint32_t i;

	if (p->tc_mask > RTE_LEN2MASK(cfg->n_tc, uint32_t))
		return -EINVAL;

	for (i = 0; i < RTE_TABLE_ACTION_TC_MAX; i++) {
		struct rte_table_action_mtr_tc_params *p_tc = &p->mtr[i];
		struct meter_profile_data *mp_data;

		if ((p->tc_mask & (1LLU << i)) == 0)
			continue;

		mp_data = meter_profile_data_find(mp,
			mp_size,
			p_tc->meter_profile_id);
		if (!mp_data)
			return -EINVAL;
	}

	return 0;
}

static int
mtr_apply(struct mtr_trtcm_data *data,
	struct rte_table_action_mtr_params *p,
	struct rte_table_action_mtr_config *cfg,
	struct meter_profile_data *mp,
	uint32_t mp_size)
{
	uint32_t i;
	int status;

	/* Check input arguments */
	status = mtr_apply_check(p, cfg, mp, mp_size);
	if (status)
		return status;

	/* Apply */
	for (i = 0; i < RTE_TABLE_ACTION_TC_MAX; i++) {
		struct rte_table_action_mtr_tc_params *p_tc = &p->mtr[i];
		struct mtr_trtcm_data *data_tc = &data[i];
		struct meter_profile_data *mp_data;

		if ((p->tc_mask & (1LLU << i)) == 0)
			continue;

		/* Find profile */
		mp_data = meter_profile_data_find(mp,
			mp_size,
			p_tc->meter_profile_id);
		if (!mp_data)
			return -EINVAL;

		memset(data_tc, 0, sizeof(*data_tc));

		/* Meter object */
		status = rte_meter_trtcm_config(&data_tc->trtcm,
			&mp_data->profile);
		if (status)
			return status;

		/* Meter profile */
		mtr_trtcm_data_meter_profile_id_set(data_tc,
			mp_data - mp);

		/* Policer actions */
		mtr_trtcm_data_policer_action_set(data_tc,
			e_RTE_METER_GREEN,
			p_tc->policer[e_RTE_METER_GREEN]);

		mtr_trtcm_data_policer_action_set(data_tc,
			e_RTE_METER_YELLOW,
			p_tc->policer[e_RTE_METER_YELLOW]);

		mtr_trtcm_data_policer_action_set(data_tc,
			e_RTE_METER_RED,
			p_tc->policer[e_RTE_METER_RED]);
	}

	return 0;
}

static __rte_always_inline uint64_t
pkt_work_mtr(struct rte_mbuf *mbuf,
	struct mtr_trtcm_data *data,
	struct dscp_table_data *dscp_table,
	struct meter_profile_data *mp,
	uint64_t time,
	uint32_t dscp,
	uint16_t total_length)
{
	uint64_t drop_mask, sched;
	uint64_t *sched_ptr = (uint64_t *) &mbuf->hash.sched;
	struct dscp_table_entry_data *dscp_entry = &dscp_table->entry[dscp];
	enum rte_meter_color color_in, color_meter, color_policer;
	uint32_t tc, mp_id;

	tc = dscp_entry->tc;
	color_in = dscp_entry->color;
	data += tc;
	mp_id = MTR_TRTCM_DATA_METER_PROFILE_ID_GET(data);
	sched = *sched_ptr;

	/* Meter */
	color_meter = rte_meter_trtcm_color_aware_check(
		&data->trtcm,
		&mp[mp_id].profile,
		time,
		total_length,
		color_in);

	/* Stats */
	MTR_TRTCM_DATA_STATS_INC(data, color_meter);

	/* Police */
	drop_mask = MTR_TRTCM_DATA_POLICER_ACTION_DROP_GET(data, color_meter);
	color_policer =
		MTR_TRTCM_DATA_POLICER_ACTION_COLOR_GET(data, color_meter);
	*sched_ptr = MBUF_SCHED_COLOR(sched, color_policer);

	return drop_mask;
}

/**
 * RTE_TABLE_ACTION_TM
 */
static int
tm_cfg_check(struct rte_table_action_tm_config *tm)
{
	if ((tm->n_subports_per_port == 0) ||
		(rte_is_power_of_2(tm->n_subports_per_port) == 0) ||
		(tm->n_subports_per_port > UINT16_MAX) ||
		(tm->n_pipes_per_subport == 0) ||
		(rte_is_power_of_2(tm->n_pipes_per_subport) == 0))
		return -ENOTSUP;

	return 0;
}

struct tm_data {
	uint16_t queue_tc_color;
	uint16_t subport;
	uint32_t pipe;
} __attribute__((__packed__));

static int
tm_apply_check(struct rte_table_action_tm_params *p,
	struct rte_table_action_tm_config *cfg)
{
	if ((p->subport_id >= cfg->n_subports_per_port) ||
		(p->pipe_id >= cfg->n_pipes_per_subport))
		return -EINVAL;

	return 0;
}

static int
tm_apply(struct tm_data *data,
	struct rte_table_action_tm_params *p,
	struct rte_table_action_tm_config *cfg)
{
	int status;

	/* Check input arguments */
	status = tm_apply_check(p, cfg);
	if (status)
		return status;

	/* Apply */
	data->queue_tc_color = 0;
	data->subport = (uint16_t) p->subport_id;
	data->pipe = p->pipe_id;

	return 0;
}

static __rte_always_inline void
pkt_work_tm(struct rte_mbuf *mbuf,
	struct tm_data *data,
	struct dscp_table_data *dscp_table,
	uint32_t dscp)
{
	struct dscp_table_entry_data *dscp_entry = &dscp_table->entry[dscp];
	struct tm_data *sched_ptr = (struct tm_data *) &mbuf->hash.sched;
	struct tm_data sched;

	sched = *data;
	sched.queue_tc_color = dscp_entry->queue_tc_color;
	*sched_ptr = sched;
}

/**
 * RTE_TABLE_ACTION_ENCAP
 */
static int
encap_valid(enum rte_table_action_encap_type encap)
{
	switch (encap) {
	case RTE_TABLE_ACTION_ENCAP_ETHER:
	case RTE_TABLE_ACTION_ENCAP_VLAN:
	case RTE_TABLE_ACTION_ENCAP_QINQ:
	case RTE_TABLE_ACTION_ENCAP_MPLS:
	case RTE_TABLE_ACTION_ENCAP_PPPOE:
		return 1;
	default:
		return 0;
	}
}

static int
encap_cfg_check(struct rte_table_action_encap_config *encap)
{
	if ((encap->encap_mask == 0) ||
		(__builtin_popcountll(encap->encap_mask) != 1))
		return -ENOTSUP;

	return 0;
}

struct encap_ether_data {
	struct ether_hdr ether;
} __attribute__((__packed__));

#define VLAN(pcp, dei, vid)                                \
	((uint16_t)((((uint64_t)(pcp)) & 0x7LLU) << 13) |  \
	((((uint64_t)(dei)) & 0x1LLU) << 12) |             \
	(((uint64_t)(vid)) & 0xFFFLLU))                    \

struct encap_vlan_data {
	struct ether_hdr ether;
	struct vlan_hdr vlan;
} __attribute__((__packed__));

struct encap_qinq_data {
	struct ether_hdr ether;
	struct vlan_hdr svlan;
	struct vlan_hdr cvlan;
} __attribute__((__packed__));

#define ETHER_TYPE_MPLS_UNICAST                            0x8847

#define ETHER_TYPE_MPLS_MULTICAST                          0x8848

#define MPLS(label, tc, s, ttl)                            \
	((uint32_t)(((((uint64_t)(label)) & 0xFFFFFLLU) << 12) |\
	((((uint64_t)(tc)) & 0x7LLU) << 9) |               \
	((((uint64_t)(s)) & 0x1LLU) << 8) |                \
	(((uint64_t)(ttl)) & 0xFFLLU)))

struct encap_mpls_data {
	struct ether_hdr ether;
	uint32_t mpls[RTE_TABLE_ACTION_MPLS_LABELS_MAX];
	uint32_t mpls_count;
} __attribute__((__packed__));

#define ETHER_TYPE_PPPOE_SESSION                           0x8864

#define PPP_PROTOCOL_IP                                    0x0021

struct pppoe_ppp_hdr {
	uint16_t ver_type_code;
	uint16_t session_id;
	uint16_t length;
	uint16_t protocol;
} __attribute__((__packed__));

struct encap_pppoe_data {
	struct ether_hdr ether;
	struct pppoe_ppp_hdr pppoe_ppp;
} __attribute__((__packed__));

static size_t
encap_data_size(struct rte_table_action_encap_config *encap)
{
	switch (encap->encap_mask) {
	case 1LLU << RTE_TABLE_ACTION_ENCAP_ETHER:
		return sizeof(struct encap_ether_data);

	case 1LLU << RTE_TABLE_ACTION_ENCAP_VLAN:
		return sizeof(struct encap_vlan_data);

	case 1LLU << RTE_TABLE_ACTION_ENCAP_QINQ:
		return sizeof(struct encap_qinq_data);

	case 1LLU << RTE_TABLE_ACTION_ENCAP_MPLS:
		return sizeof(struct encap_mpls_data);

	case 1LLU << RTE_TABLE_ACTION_ENCAP_PPPOE:
		return sizeof(struct encap_pppoe_data);

	default:
		return 0;
	}
}

static int
encap_apply_check(struct rte_table_action_encap_params *p,
	struct rte_table_action_encap_config *cfg)
{
	if ((encap_valid(p->type) == 0) ||
		((cfg->encap_mask & (1LLU << p->type)) == 0))
		return -EINVAL;

	switch (p->type) {
	case RTE_TABLE_ACTION_ENCAP_ETHER:
		return 0;

	case RTE_TABLE_ACTION_ENCAP_VLAN:
		return 0;

	case RTE_TABLE_ACTION_ENCAP_QINQ:
		return 0;

	case RTE_TABLE_ACTION_ENCAP_MPLS:
		if ((p->mpls.mpls_count == 0) ||
			(p->mpls.mpls_count > RTE_TABLE_ACTION_MPLS_LABELS_MAX))
			return -EINVAL;

		return 0;

	case RTE_TABLE_ACTION_ENCAP_PPPOE:
		return 0;

	default:
		return -EINVAL;
	}
}

static int
encap_ether_apply(void *data,
	struct rte_table_action_encap_params *p,
	struct rte_table_action_common_config *common_cfg)
{
	struct encap_ether_data *d = data;
	uint16_t ethertype = (common_cfg->ip_version) ?
		ETHER_TYPE_IPv4 :
		ETHER_TYPE_IPv6;

	/* Ethernet */
	ether_addr_copy(&p->ether.ether.da, &d->ether.d_addr);
	ether_addr_copy(&p->ether.ether.sa, &d->ether.s_addr);
	d->ether.ether_type = rte_htons(ethertype);

	return 0;
}

static int
encap_vlan_apply(void *data,
	struct rte_table_action_encap_params *p,
	struct rte_table_action_common_config *common_cfg)
{
	struct encap_vlan_data *d = data;
	uint16_t ethertype = (common_cfg->ip_version) ?
		ETHER_TYPE_IPv4 :
		ETHER_TYPE_IPv6;

	/* Ethernet */
	ether_addr_copy(&p->vlan.ether.da, &d->ether.d_addr);
	ether_addr_copy(&p->vlan.ether.sa, &d->ether.s_addr);
	d->ether.ether_type = rte_htons(ETHER_TYPE_VLAN);

	/* VLAN */
	d->vlan.vlan_tci = rte_htons(VLAN(p->vlan.vlan.pcp,
		p->vlan.vlan.dei,
		p->vlan.vlan.vid));
	d->vlan.eth_proto = rte_htons(ethertype);

	return 0;
}

static int
encap_qinq_apply(void *data,
	struct rte_table_action_encap_params *p,
	struct rte_table_action_common_config *common_cfg)
{
	struct encap_qinq_data *d = data;
	uint16_t ethertype = (common_cfg->ip_version) ?
		ETHER_TYPE_IPv4 :
		ETHER_TYPE_IPv6;

	/* Ethernet */
	ether_addr_copy(&p->qinq.ether.da, &d->ether.d_addr);
	ether_addr_copy(&p->qinq.ether.sa, &d->ether.s_addr);
	d->ether.ether_type = rte_htons(ETHER_TYPE_QINQ);

	/* SVLAN */
	d->svlan.vlan_tci = rte_htons(VLAN(p->qinq.svlan.pcp,
		p->qinq.svlan.dei,
		p->qinq.svlan.vid));
	d->svlan.eth_proto = rte_htons(ETHER_TYPE_VLAN);

	/* CVLAN */
	d->cvlan.vlan_tci = rte_htons(VLAN(p->qinq.cvlan.pcp,
		p->qinq.cvlan.dei,
		p->qinq.cvlan.vid));
	d->cvlan.eth_proto = rte_htons(ethertype);

	return 0;
}

static int
encap_mpls_apply(void *data,
	struct rte_table_action_encap_params *p)
{
	struct encap_mpls_data *d = data;
	uint16_t ethertype = (p->mpls.unicast) ?
		ETHER_TYPE_MPLS_UNICAST :
		ETHER_TYPE_MPLS_MULTICAST;
	uint32_t i;

	/* Ethernet */
	ether_addr_copy(&p->mpls.ether.da, &d->ether.d_addr);
	ether_addr_copy(&p->mpls.ether.sa, &d->ether.s_addr);
	d->ether.ether_type = rte_htons(ethertype);

	/* MPLS */
	for (i = 0; i < p->mpls.mpls_count - 1; i++)
		d->mpls[i] = rte_htonl(MPLS(p->mpls.mpls[i].label,
			p->mpls.mpls[i].tc,
			0,
			p->mpls.mpls[i].ttl));

	d->mpls[i] = rte_htonl(MPLS(p->mpls.mpls[i].label,
		p->mpls.mpls[i].tc,
		1,
		p->mpls.mpls[i].ttl));

	d->mpls_count = p->mpls.mpls_count;
	return 0;
}

static int
encap_pppoe_apply(void *data,
	struct rte_table_action_encap_params *p)
{
	struct encap_pppoe_data *d = data;

	/* Ethernet */
	ether_addr_copy(&p->pppoe.ether.da, &d->ether.d_addr);
	ether_addr_copy(&p->pppoe.ether.sa, &d->ether.s_addr);
	d->ether.ether_type = rte_htons(ETHER_TYPE_PPPOE_SESSION);

	/* PPPoE and PPP*/
	d->pppoe_ppp.ver_type_code = rte_htons(0x1100);
	d->pppoe_ppp.session_id = rte_htons(p->pppoe.pppoe.session_id);
	d->pppoe_ppp.length = 0; /* not pre-computed */
	d->pppoe_ppp.protocol = rte_htons(PPP_PROTOCOL_IP);

	return 0;
}

static int
encap_apply(void *data,
	struct rte_table_action_encap_params *p,
	struct rte_table_action_encap_config *cfg,
	struct rte_table_action_common_config *common_cfg)
{
	int status;

	/* Check input arguments */
	status = encap_apply_check(p, cfg);
	if (status)
		return status;

	switch (p->type) {
	case RTE_TABLE_ACTION_ENCAP_ETHER:
		return encap_ether_apply(data, p, common_cfg);

	case RTE_TABLE_ACTION_ENCAP_VLAN:
		return encap_vlan_apply(data, p, common_cfg);

	case RTE_TABLE_ACTION_ENCAP_QINQ:
		return encap_qinq_apply(data, p, common_cfg);

	case RTE_TABLE_ACTION_ENCAP_MPLS:
		return encap_mpls_apply(data, p);

	case RTE_TABLE_ACTION_ENCAP_PPPOE:
		return encap_pppoe_apply(data, p);

	default:
		return -EINVAL;
	}
}

static __rte_always_inline void *
encap(void *dst, const void *src, size_t n)
{
	dst = ((uint8_t *) dst) - n;
	return rte_memcpy(dst, src, n);
}

static __rte_always_inline void
pkt_work_encap(struct rte_mbuf *mbuf,
	void *data,
	struct rte_table_action_encap_config *cfg,
	void *ip,
	uint16_t total_length,
	uint32_t ip_offset)
{
	switch (cfg->encap_mask) {
	case 1LLU << RTE_TABLE_ACTION_ENCAP_ETHER:
		encap(ip, data, sizeof(struct encap_ether_data));
		mbuf->data_off = ip_offset - (sizeof(struct rte_mbuf) +
			sizeof(struct encap_ether_data));
		mbuf->pkt_len = mbuf->data_len = total_length +
			sizeof(struct encap_ether_data);
		break;

	case 1LLU << RTE_TABLE_ACTION_ENCAP_VLAN:
		encap(ip, data, sizeof(struct encap_vlan_data));
		mbuf->data_off = ip_offset - (sizeof(struct rte_mbuf) +
			sizeof(struct encap_vlan_data));
		mbuf->pkt_len = mbuf->data_len = total_length +
			sizeof(struct encap_vlan_data);
		break;

	case 1LLU << RTE_TABLE_ACTION_ENCAP_QINQ:
		encap(ip, data, sizeof(struct encap_qinq_data));
		mbuf->data_off = ip_offset - (sizeof(struct rte_mbuf) +
			sizeof(struct encap_qinq_data));
		mbuf->pkt_len = mbuf->data_len = total_length +
			sizeof(struct encap_qinq_data);
		break;

	case 1LLU << RTE_TABLE_ACTION_ENCAP_MPLS:
	{
		struct encap_mpls_data *mpls = data;
		size_t size = sizeof(struct ether_hdr) +
			mpls->mpls_count * 4;

		encap(ip, data, size);
		mbuf->data_off = ip_offset - (sizeof(struct rte_mbuf) + size);
		mbuf->pkt_len = mbuf->data_len = total_length + size;
		break;
	}

	case 1LLU << RTE_TABLE_ACTION_ENCAP_PPPOE:
	{
		struct encap_pppoe_data *pppoe =
			encap(ip, data, sizeof(struct encap_pppoe_data));
		pppoe->pppoe_ppp.length = rte_htons(total_length + 2);
		mbuf->data_off = ip_offset - (sizeof(struct rte_mbuf) +
			sizeof(struct encap_pppoe_data));
		mbuf->pkt_len = mbuf->data_len = total_length +
			sizeof(struct encap_pppoe_data);
		break;
	}

	default:
		break;
	}
}

/**
 * RTE_TABLE_ACTION_NAT
 */
static int
nat_cfg_check(struct rte_table_action_nat_config *nat)
{
	if ((nat->proto != 0x06) &&
		(nat->proto != 0x11))
		return -ENOTSUP;

	return 0;
}

struct nat_ipv4_data {
	uint32_t addr;
	uint16_t port;
} __attribute__((__packed__));

struct nat_ipv6_data {
	uint8_t addr[16];
	uint16_t port;
} __attribute__((__packed__));

static size_t
nat_data_size(struct rte_table_action_nat_config *nat __rte_unused,
	struct rte_table_action_common_config *common)
{
	int ip_version = common->ip_version;

	return (ip_version) ?
		sizeof(struct nat_ipv4_data) :
		sizeof(struct nat_ipv6_data);
}

static int
nat_apply_check(struct rte_table_action_nat_params *p,
	struct rte_table_action_common_config *cfg)
{
	if ((p->ip_version && (cfg->ip_version == 0)) ||
		((p->ip_version == 0) && cfg->ip_version))
		return -EINVAL;

	return 0;
}

static int
nat_apply(void *data,
	struct rte_table_action_nat_params *p,
	struct rte_table_action_common_config *cfg)
{
	int status;

	/* Check input arguments */
	status = nat_apply_check(p, cfg);
	if (status)
		return status;

	/* Apply */
	if (p->ip_version) {
		struct nat_ipv4_data *d = data;

		d->addr = rte_htonl(p->addr.ipv4);
		d->port = rte_htons(p->port);
	} else {
		struct nat_ipv6_data *d = data;

		memcpy(d->addr, p->addr.ipv6, sizeof(d->addr));
		d->port = rte_htons(p->port);
	}

	return 0;
}

static __rte_always_inline uint16_t
nat_ipv4_checksum_update(uint16_t cksum0,
	uint32_t ip0,
	uint32_t ip1)
{
	int32_t cksum1;

	cksum1 = cksum0;
	cksum1 = ~cksum1 & 0xFFFF;

	/* Subtract ip0 (one's complement logic) */
	cksum1 -= (ip0 >> 16) + (ip0 & 0xFFFF);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);

	/* Add ip1 (one's complement logic) */
	cksum1 += (ip1 >> 16) + (ip1 & 0xFFFF);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);

	return (uint16_t)(~cksum1);
}

static __rte_always_inline uint16_t
nat_ipv4_tcp_udp_checksum_update(uint16_t cksum0,
	uint32_t ip0,
	uint32_t ip1,
	uint16_t port0,
	uint16_t port1)
{
	int32_t cksum1;

	cksum1 = cksum0;
	cksum1 = ~cksum1 & 0xFFFF;

	/* Subtract ip0 and port 0 (one's complement logic) */
	cksum1 -= (ip0 >> 16) + (ip0 & 0xFFFF) + port0;
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);

	/* Add ip1 and port1 (one's complement logic) */
	cksum1 += (ip1 >> 16) + (ip1 & 0xFFFF) + port1;
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);

	return (uint16_t)(~cksum1);
}

static __rte_always_inline uint16_t
nat_ipv6_tcp_udp_checksum_update(uint16_t cksum0,
	uint16_t *ip0,
	uint16_t *ip1,
	uint16_t port0,
	uint16_t port1)
{
	int32_t cksum1;

	cksum1 = cksum0;
	cksum1 = ~cksum1 & 0xFFFF;

	/* Subtract ip0 and port 0 (one's complement logic) */
	cksum1 -= ip0[0] + ip0[1] + ip0[2] + ip0[3] +
		ip0[4] + ip0[5] + ip0[6] + ip0[7] + port0;
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);

	/* Add ip1 and port1 (one's complement logic) */
	cksum1 += ip1[0] + ip1[1] + ip1[2] + ip1[3] +
		ip1[4] + ip1[5] + ip1[6] + ip1[7] + port1;
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);
	cksum1 = (cksum1 & 0xFFFF) + (cksum1 >> 16);

	return (uint16_t)(~cksum1);
}

static __rte_always_inline void
pkt_ipv4_work_nat(struct ipv4_hdr *ip,
	struct nat_ipv4_data *data,
	struct rte_table_action_nat_config *cfg)
{
	if (cfg->source_nat) {
		if (cfg->proto == 0x6) {
			struct tcp_hdr *tcp = (struct tcp_hdr *) &ip[1];
			uint16_t ip_cksum, tcp_cksum;

			ip_cksum = nat_ipv4_checksum_update(ip->hdr_checksum,
				ip->src_addr,
				data->addr);

			tcp_cksum = nat_ipv4_tcp_udp_checksum_update(tcp->cksum,
				ip->src_addr,
				data->addr,
				tcp->src_port,
				data->port);

			ip->src_addr = data->addr;
			ip->hdr_checksum = ip_cksum;
			tcp->src_port = data->port;
			tcp->cksum = tcp_cksum;
		} else {
			struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
			uint16_t ip_cksum, udp_cksum;

			ip_cksum = nat_ipv4_checksum_update(ip->hdr_checksum,
				ip->src_addr,
				data->addr);

			udp_cksum = nat_ipv4_tcp_udp_checksum_update(udp->dgram_cksum,
				ip->src_addr,
				data->addr,
				udp->src_port,
				data->port);

			ip->src_addr = data->addr;
			ip->hdr_checksum = ip_cksum;
			udp->src_port = data->port;
			if (udp->dgram_cksum)
				udp->dgram_cksum = udp_cksum;
		}
	} else {
		if (cfg->proto == 0x6) {
			struct tcp_hdr *tcp = (struct tcp_hdr *) &ip[1];
			uint16_t ip_cksum, tcp_cksum;

			ip_cksum = nat_ipv4_checksum_update(ip->hdr_checksum,
				ip->dst_addr,
				data->addr);

			tcp_cksum = nat_ipv4_tcp_udp_checksum_update(tcp->cksum,
				ip->dst_addr,
				data->addr,
				tcp->dst_port,
				data->port);

			ip->dst_addr = data->addr;
			ip->hdr_checksum = ip_cksum;
			tcp->dst_port = data->port;
			tcp->cksum = tcp_cksum;
		} else {
			struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
			uint16_t ip_cksum, udp_cksum;

			ip_cksum = nat_ipv4_checksum_update(ip->hdr_checksum,
				ip->dst_addr,
				data->addr);

			udp_cksum = nat_ipv4_tcp_udp_checksum_update(udp->dgram_cksum,
				ip->dst_addr,
				data->addr,
				udp->dst_port,
				data->port);

			ip->dst_addr = data->addr;
			ip->hdr_checksum = ip_cksum;
			udp->dst_port = data->port;
			if (udp->dgram_cksum)
				udp->dgram_cksum = udp_cksum;
		}
	}
}

static __rte_always_inline void
pkt_ipv6_work_nat(struct ipv6_hdr *ip,
	struct nat_ipv6_data *data,
	struct rte_table_action_nat_config *cfg)
{
	if (cfg->source_nat) {
		if (cfg->proto == 0x6) {
			struct tcp_hdr *tcp = (struct tcp_hdr *) &ip[1];
			uint16_t tcp_cksum;

			tcp_cksum = nat_ipv6_tcp_udp_checksum_update(tcp->cksum,
				(uint16_t *)ip->src_addr,
				(uint16_t *)data->addr,
				tcp->src_port,
				data->port);

			rte_memcpy(ip->src_addr, data->addr, 16);
			tcp->src_port = data->port;
			tcp->cksum = tcp_cksum;
		} else {
			struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
			uint16_t udp_cksum;

			udp_cksum = nat_ipv6_tcp_udp_checksum_update(udp->dgram_cksum,
				(uint16_t *)ip->src_addr,
				(uint16_t *)data->addr,
				udp->src_port,
				data->port);

			rte_memcpy(ip->src_addr, data->addr, 16);
			udp->src_port = data->port;
			udp->dgram_cksum = udp_cksum;
		}
	} else {
		if (cfg->proto == 0x6) {
			struct tcp_hdr *tcp = (struct tcp_hdr *) &ip[1];
			uint16_t tcp_cksum;

			tcp_cksum = nat_ipv6_tcp_udp_checksum_update(tcp->cksum,
				(uint16_t *)ip->dst_addr,
				(uint16_t *)data->addr,
				tcp->dst_port,
				data->port);

			rte_memcpy(ip->dst_addr, data->addr, 16);
			tcp->dst_port = data->port;
			tcp->cksum = tcp_cksum;
		} else {
			struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
			uint16_t udp_cksum;

			udp_cksum = nat_ipv6_tcp_udp_checksum_update(udp->dgram_cksum,
				(uint16_t *)ip->dst_addr,
				(uint16_t *)data->addr,
				udp->dst_port,
				data->port);

			rte_memcpy(ip->dst_addr, data->addr, 16);
			udp->dst_port = data->port;
			udp->dgram_cksum = udp_cksum;
		}
	}
}

/**
 * RTE_TABLE_ACTION_TTL
 */
static int
ttl_cfg_check(struct rte_table_action_ttl_config *ttl)
{
	if (ttl->drop == 0)
		return -ENOTSUP;

	return 0;
}

struct ttl_data {
	uint32_t n_packets;
} __attribute__((__packed__));

#define TTL_INIT(data, decrement)                         \
	((data)->n_packets = (decrement) ? 1 : 0)

#define TTL_DEC_GET(data)                                  \
	((uint8_t)((data)->n_packets & 1))

#define TTL_STATS_RESET(data)                             \
	((data)->n_packets = ((data)->n_packets & 1))

#define TTL_STATS_READ(data)                               \
	((data)->n_packets >> 1)

#define TTL_STATS_ADD(data, value)                        \
	((data)->n_packets =                                  \
		(((((data)->n_packets >> 1) + (value)) << 1) |    \
		((data)->n_packets & 1)))

static int
ttl_apply(void *data,
	struct rte_table_action_ttl_params *p)
{
	struct ttl_data *d = data;

	TTL_INIT(d, p->decrement);

	return 0;
}

static __rte_always_inline uint64_t
pkt_ipv4_work_ttl(struct ipv4_hdr *ip,
	struct ttl_data *data)
{
	uint32_t drop;
	uint16_t cksum = ip->hdr_checksum;
	uint8_t ttl = ip->time_to_live;
	uint8_t ttl_diff = TTL_DEC_GET(data);

	cksum += ttl_diff;
	ttl -= ttl_diff;

	ip->hdr_checksum = cksum;
	ip->time_to_live = ttl;

	drop = (ttl == 0) ? 1 : 0;
	TTL_STATS_ADD(data, drop);

	return drop;
}

static __rte_always_inline uint64_t
pkt_ipv6_work_ttl(struct ipv6_hdr *ip,
	struct ttl_data *data)
{
	uint32_t drop;
	uint8_t ttl = ip->hop_limits;
	uint8_t ttl_diff = TTL_DEC_GET(data);

	ttl -= ttl_diff;

	ip->hop_limits = ttl;

	drop = (ttl == 0) ? 1 : 0;
	TTL_STATS_ADD(data, drop);

	return drop;
}

/**
 * RTE_TABLE_ACTION_STATS
 */
static int
stats_cfg_check(struct rte_table_action_stats_config *stats)
{
	if ((stats->n_packets_enabled == 0) && (stats->n_bytes_enabled == 0))
		return -EINVAL;

	return 0;
}

struct stats_data {
	uint64_t n_packets;
	uint64_t n_bytes;
} __attribute__((__packed__));

static int
stats_apply(struct stats_data *data,
	struct rte_table_action_stats_params *p)
{
	data->n_packets = p->n_packets;
	data->n_bytes = p->n_bytes;

	return 0;
}

static __rte_always_inline void
pkt_work_stats(struct stats_data *data,
	uint16_t total_length)
{
	data->n_packets++;
	data->n_bytes += total_length;
}

/**
 * RTE_TABLE_ACTION_TIME
 */
struct time_data {
	uint64_t time;
} __attribute__((__packed__));

static int
time_apply(struct time_data *data,
	struct rte_table_action_time_params *p)
{
	data->time = p->time;
	return 0;
}

static __rte_always_inline void
pkt_work_time(struct time_data *data,
	uint64_t time)
{
	data->time = time;
}

/**
 * Action profile
 */
static int
action_valid(enum rte_table_action_type action)
{
	switch (action) {
	case RTE_TABLE_ACTION_FWD:
	case RTE_TABLE_ACTION_LB:
	case RTE_TABLE_ACTION_MTR:
	case RTE_TABLE_ACTION_TM:
	case RTE_TABLE_ACTION_ENCAP:
	case RTE_TABLE_ACTION_NAT:
	case RTE_TABLE_ACTION_TTL:
	case RTE_TABLE_ACTION_STATS:
	case RTE_TABLE_ACTION_TIME:
		return 1;
	default:
		return 0;
	}
}


#define RTE_TABLE_ACTION_MAX                      64

struct ap_config {
	uint64_t action_mask;
	struct rte_table_action_common_config common;
	struct rte_table_action_lb_config lb;
	struct rte_table_action_mtr_config mtr;
	struct rte_table_action_tm_config tm;
	struct rte_table_action_encap_config encap;
	struct rte_table_action_nat_config nat;
	struct rte_table_action_ttl_config ttl;
	struct rte_table_action_stats_config stats;
};

static size_t
action_cfg_size(enum rte_table_action_type action)
{
	switch (action) {
	case RTE_TABLE_ACTION_LB:
		return sizeof(struct rte_table_action_lb_config);
	case RTE_TABLE_ACTION_MTR:
		return sizeof(struct rte_table_action_mtr_config);
	case RTE_TABLE_ACTION_TM:
		return sizeof(struct rte_table_action_tm_config);
	case RTE_TABLE_ACTION_ENCAP:
		return sizeof(struct rte_table_action_encap_config);
	case RTE_TABLE_ACTION_NAT:
		return sizeof(struct rte_table_action_nat_config);
	case RTE_TABLE_ACTION_TTL:
		return sizeof(struct rte_table_action_ttl_config);
	case RTE_TABLE_ACTION_STATS:
		return sizeof(struct rte_table_action_stats_config);
	default:
		return 0;
	}
}

static void*
action_cfg_get(struct ap_config *ap_config,
	enum rte_table_action_type type)
{
	switch (type) {
	case RTE_TABLE_ACTION_LB:
		return &ap_config->lb;

	case RTE_TABLE_ACTION_MTR:
		return &ap_config->mtr;

	case RTE_TABLE_ACTION_TM:
		return &ap_config->tm;

	case RTE_TABLE_ACTION_ENCAP:
		return &ap_config->encap;

	case RTE_TABLE_ACTION_NAT:
		return &ap_config->nat;

	case RTE_TABLE_ACTION_TTL:
		return &ap_config->ttl;

	case RTE_TABLE_ACTION_STATS:
		return &ap_config->stats;

	default:
		return NULL;
	}
}

static void
action_cfg_set(struct ap_config *ap_config,
	enum rte_table_action_type type,
	void *action_cfg)
{
	void *dst = action_cfg_get(ap_config, type);

	if (dst)
		memcpy(dst, action_cfg, action_cfg_size(type));

	ap_config->action_mask |= 1LLU << type;
}

struct ap_data {
	size_t offset[RTE_TABLE_ACTION_MAX];
	size_t total_size;
};

static size_t
action_data_size(enum rte_table_action_type action,
	struct ap_config *ap_config)
{
	switch (action) {
	case RTE_TABLE_ACTION_FWD:
		return sizeof(struct fwd_data);

	case RTE_TABLE_ACTION_LB:
		return sizeof(struct lb_data);

	case RTE_TABLE_ACTION_MTR:
		return mtr_data_size(&ap_config->mtr);

	case RTE_TABLE_ACTION_TM:
		return sizeof(struct tm_data);

	case RTE_TABLE_ACTION_ENCAP:
		return encap_data_size(&ap_config->encap);

	case RTE_TABLE_ACTION_NAT:
		return nat_data_size(&ap_config->nat,
			&ap_config->common);

	case RTE_TABLE_ACTION_TTL:
		return sizeof(struct ttl_data);

	case RTE_TABLE_ACTION_STATS:
		return sizeof(struct stats_data);

	case RTE_TABLE_ACTION_TIME:
		return sizeof(struct time_data);

	default:
		return 0;
	}
}


static void
action_data_offset_set(struct ap_data *ap_data,
	struct ap_config *ap_config)
{
	uint64_t action_mask = ap_config->action_mask;
	size_t offset;
	uint32_t action;

	memset(ap_data->offset, 0, sizeof(ap_data->offset));

	offset = 0;
	for (action = 0; action < RTE_TABLE_ACTION_MAX; action++)
		if (action_mask & (1LLU << action)) {
			ap_data->offset[action] = offset;
			offset += action_data_size((enum rte_table_action_type)action,
				ap_config);
		}

	ap_data->total_size = offset;
}

struct rte_table_action_profile {
	struct ap_config cfg;
	struct ap_data data;
	int frozen;
};

struct rte_table_action_profile *
rte_table_action_profile_create(struct rte_table_action_common_config *common)
{
	struct rte_table_action_profile *ap;

	/* Check input arguments */
	if (common == NULL)
		return NULL;

	/* Memory allocation */
	ap = calloc(1, sizeof(struct rte_table_action_profile));
	if (ap == NULL)
		return NULL;

	/* Initialization */
	memcpy(&ap->cfg.common, common, sizeof(*common));

	return ap;
}


int
rte_table_action_profile_action_register(struct rte_table_action_profile *profile,
	enum rte_table_action_type type,
	void *action_config)
{
	int status;

	/* Check input arguments */
	if ((profile == NULL) ||
		profile->frozen ||
		(action_valid(type) == 0) ||
		(profile->cfg.action_mask & (1LLU << type)) ||
		((action_cfg_size(type) == 0) && action_config) ||
		(action_cfg_size(type) && (action_config == NULL)))
		return -EINVAL;

	switch (type) {
	case RTE_TABLE_ACTION_LB:
		status = lb_cfg_check(action_config);
		break;

	case RTE_TABLE_ACTION_MTR:
		status = mtr_cfg_check(action_config);
		break;

	case RTE_TABLE_ACTION_TM:
		status = tm_cfg_check(action_config);
		break;

	case RTE_TABLE_ACTION_ENCAP:
		status = encap_cfg_check(action_config);
		break;

	case RTE_TABLE_ACTION_NAT:
		status = nat_cfg_check(action_config);
		break;

	case RTE_TABLE_ACTION_TTL:
		status = ttl_cfg_check(action_config);
		break;

	case RTE_TABLE_ACTION_STATS:
		status = stats_cfg_check(action_config);
		break;

	default:
		status = 0;
		break;
	}

	if (status)
		return status;

	/* Action enable */
	action_cfg_set(&profile->cfg, type, action_config);

	return 0;
}

int
rte_table_action_profile_freeze(struct rte_table_action_profile *profile)
{
	if (profile->frozen)
		return -EBUSY;

	profile->cfg.action_mask |= 1LLU << RTE_TABLE_ACTION_FWD;
	action_data_offset_set(&profile->data, &profile->cfg);
	profile->frozen = 1;

	return 0;
}

int
rte_table_action_profile_free(struct rte_table_action_profile *profile)
{
	if (profile == NULL)
		return 0;

	free(profile);
	return 0;
}

/**
 * Action
 */
#define METER_PROFILES_MAX                                 32

struct rte_table_action {
	struct ap_config cfg;
	struct ap_data data;
	struct dscp_table_data dscp_table;
	struct meter_profile_data mp[METER_PROFILES_MAX];
};

struct rte_table_action *
rte_table_action_create(struct rte_table_action_profile *profile,
	uint32_t socket_id)
{
	struct rte_table_action *action;

	/* Check input arguments */
	if ((profile == NULL) ||
		(profile->frozen == 0))
		return NULL;

	/* Memory allocation */
	action = rte_zmalloc_socket(NULL,
		sizeof(struct rte_table_action),
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (action == NULL)
		return NULL;

	/* Initialization */
	memcpy(&action->cfg, &profile->cfg, sizeof(profile->cfg));
	memcpy(&action->data, &profile->data, sizeof(profile->data));

	return action;
}

static __rte_always_inline void *
action_data_get(void *data,
	struct rte_table_action *action,
	enum rte_table_action_type type)
{
	size_t offset = action->data.offset[type];
	uint8_t *data_bytes = data;

	return &data_bytes[offset];
}

int
rte_table_action_apply(struct rte_table_action *action,
	void *data,
	enum rte_table_action_type type,
	void *action_params)
{
	void *action_data;

	/* Check input arguments */
	if ((action == NULL) ||
		(data == NULL) ||
		(action_valid(type) == 0) ||
		((action->cfg.action_mask & (1LLU << type)) == 0) ||
		(action_params == NULL))
		return -EINVAL;

	/* Data update */
	action_data = action_data_get(data, action, type);

	switch (type) {
	case RTE_TABLE_ACTION_FWD:
		return fwd_apply(action_data,
			action_params);

	case RTE_TABLE_ACTION_LB:
		return lb_apply(action_data,
			action_params);

	case RTE_TABLE_ACTION_MTR:
		return mtr_apply(action_data,
			action_params,
			&action->cfg.mtr,
			action->mp,
			RTE_DIM(action->mp));

	case RTE_TABLE_ACTION_TM:
		return tm_apply(action_data,
			action_params,
			&action->cfg.tm);

	case RTE_TABLE_ACTION_ENCAP:
		return encap_apply(action_data,
			action_params,
			&action->cfg.encap,
			&action->cfg.common);

	case RTE_TABLE_ACTION_NAT:
		return nat_apply(action_data,
			action_params,
			&action->cfg.common);

	case RTE_TABLE_ACTION_TTL:
		return ttl_apply(action_data,
			action_params);

	case RTE_TABLE_ACTION_STATS:
		return stats_apply(action_data,
			action_params);

	case RTE_TABLE_ACTION_TIME:
		return time_apply(action_data,
			action_params);

	default:
		return -EINVAL;
	}
}

int
rte_table_action_dscp_table_update(struct rte_table_action *action,
	uint64_t dscp_mask,
	struct rte_table_action_dscp_table *table)
{
	uint32_t i;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask & ((1LLU << RTE_TABLE_ACTION_MTR) |
		(1LLU << RTE_TABLE_ACTION_TM))) == 0) ||
		(dscp_mask == 0) ||
		(table == NULL))
		return -EINVAL;

	for (i = 0; i < RTE_DIM(table->entry); i++) {
		struct dscp_table_entry_data *data =
			&action->dscp_table.entry[i];
		struct rte_table_action_dscp_table_entry *entry =
			&table->entry[i];
		uint16_t queue_tc_color =
			MBUF_SCHED_QUEUE_TC_COLOR(entry->tc_queue_id,
				entry->tc_id,
				entry->color);

		if ((dscp_mask & (1LLU << i)) == 0)
			continue;

		data->color = entry->color;
		data->tc = entry->tc_id;
		data->queue_tc_color = queue_tc_color;
	}

	return 0;
}

int
rte_table_action_meter_profile_add(struct rte_table_action *action,
	uint32_t meter_profile_id,
	struct rte_table_action_meter_profile *profile)
{
	struct meter_profile_data *mp_data;
	uint32_t status;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) == 0) ||
		(profile == NULL))
		return -EINVAL;

	if (profile->alg != RTE_TABLE_ACTION_METER_TRTCM)
		return -ENOTSUP;

	mp_data = meter_profile_data_find(action->mp,
		RTE_DIM(action->mp),
		meter_profile_id);
	if (mp_data)
		return -EEXIST;

	mp_data = meter_profile_data_find_unused(action->mp,
		RTE_DIM(action->mp));
	if (!mp_data)
		return -ENOSPC;

	/* Install new profile */
	status = rte_meter_trtcm_profile_config(&mp_data->profile,
		&profile->trtcm);
	if (status)
		return status;

	mp_data->profile_id = meter_profile_id;
	mp_data->valid = 1;

	return 0;
}

int
rte_table_action_meter_profile_delete(struct rte_table_action *action,
	uint32_t meter_profile_id)
{
	struct meter_profile_data *mp_data;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) == 0))
		return -EINVAL;

	mp_data = meter_profile_data_find(action->mp,
		RTE_DIM(action->mp),
		meter_profile_id);
	if (!mp_data)
		return 0;

	/* Uninstall profile */
	mp_data->valid = 0;

	return 0;
}

int
rte_table_action_meter_read(struct rte_table_action *action,
	void *data,
	uint32_t tc_mask,
	struct rte_table_action_mtr_counters *stats,
	int clear)
{
	struct mtr_trtcm_data *mtr_data;
	uint32_t i;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) == 0) ||
		(data == NULL) ||
		(tc_mask > RTE_LEN2MASK(action->cfg.mtr.n_tc, uint32_t)))
		return -EINVAL;

	mtr_data = action_data_get(data, action, RTE_TABLE_ACTION_MTR);

	/* Read */
	if (stats) {
		for (i = 0; i < RTE_TABLE_ACTION_TC_MAX; i++) {
			struct rte_table_action_mtr_counters_tc *dst =
				&stats->stats[i];
			struct mtr_trtcm_data *src = &mtr_data[i];

			if ((tc_mask & (1 << i)) == 0)
				continue;

			dst->n_packets[e_RTE_METER_GREEN] =
				mtr_trtcm_data_stats_get(src, e_RTE_METER_GREEN);

			dst->n_packets[e_RTE_METER_YELLOW] =
				mtr_trtcm_data_stats_get(src, e_RTE_METER_YELLOW);

			dst->n_packets[e_RTE_METER_RED] =
				mtr_trtcm_data_stats_get(src, e_RTE_METER_RED);

			dst->n_packets_valid = 1;
			dst->n_bytes_valid = 0;
		}

		stats->tc_mask = tc_mask;
	}

	/* Clear */
	if (clear)
		for (i = 0; i < RTE_TABLE_ACTION_TC_MAX; i++) {
			struct mtr_trtcm_data *src = &mtr_data[i];

			if ((tc_mask & (1 << i)) == 0)
				continue;

			mtr_trtcm_data_stats_reset(src, e_RTE_METER_GREEN);
			mtr_trtcm_data_stats_reset(src, e_RTE_METER_YELLOW);
			mtr_trtcm_data_stats_reset(src, e_RTE_METER_RED);
		}


	return 0;
}

int
rte_table_action_ttl_read(struct rte_table_action *action,
	void *data,
	struct rte_table_action_ttl_counters *stats,
	int clear)
{
	struct ttl_data *ttl_data;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask &
		(1LLU << RTE_TABLE_ACTION_TTL)) == 0) ||
		(data == NULL))
		return -EINVAL;

	ttl_data = action_data_get(data, action, RTE_TABLE_ACTION_TTL);

	/* Read */
	if (stats)
		stats->n_packets = TTL_STATS_READ(ttl_data);

	/* Clear */
	if (clear)
		TTL_STATS_RESET(ttl_data);

	return 0;
}

int
rte_table_action_stats_read(struct rte_table_action *action,
	void *data,
	struct rte_table_action_stats_counters *stats,
	int clear)
{
	struct stats_data *stats_data;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask &
		(1LLU << RTE_TABLE_ACTION_STATS)) == 0) ||
		(data == NULL))
		return -EINVAL;

	stats_data = action_data_get(data, action,
		RTE_TABLE_ACTION_STATS);

	/* Read */
	if (stats) {
		stats->n_packets = stats_data->n_packets;
		stats->n_bytes = stats_data->n_bytes;
		stats->n_packets_valid = 1;
		stats->n_bytes_valid = 1;
	}

	/* Clear */
	if (clear) {
		stats_data->n_packets = 0;
		stats_data->n_bytes = 0;
	}

	return 0;
}

int
rte_table_action_time_read(struct rte_table_action *action,
	void *data,
	uint64_t *timestamp)
{
	struct time_data *time_data;

	/* Check input arguments */
	if ((action == NULL) ||
		((action->cfg.action_mask &
		(1LLU << RTE_TABLE_ACTION_TIME)) == 0) ||
		(data == NULL) ||
		(timestamp == NULL))
		return -EINVAL;

	time_data = action_data_get(data, action, RTE_TABLE_ACTION_TIME);

	/* Read */
	*timestamp = time_data->time;

	return 0;
}

static __rte_always_inline uint64_t
pkt_work(struct rte_mbuf *mbuf,
	struct rte_pipeline_table_entry *table_entry,
	uint64_t time,
	struct rte_table_action *action,
	struct ap_config *cfg)
{
	uint64_t drop_mask = 0;

	uint32_t ip_offset = action->cfg.common.ip_offset;
	void *ip = RTE_MBUF_METADATA_UINT32_PTR(mbuf, ip_offset);

	uint32_t dscp;
	uint16_t total_length;

	if (cfg->common.ip_version) {
		struct ipv4_hdr *hdr = ip;

		dscp = hdr->type_of_service >> 2;
		total_length = rte_ntohs(hdr->total_length);
	} else {
		struct ipv6_hdr *hdr = ip;

		dscp = (rte_ntohl(hdr->vtc_flow) & 0x0F600000) >> 18;
		total_length =
			rte_ntohs(hdr->payload_len) + sizeof(struct ipv6_hdr);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_LB)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_LB);

		pkt_work_lb(mbuf,
			data,
			&cfg->lb);
	}
	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_MTR);

		drop_mask |= pkt_work_mtr(mbuf,
			data,
			&action->dscp_table,
			action->mp,
			time,
			dscp,
			total_length);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_TM)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_TM);

		pkt_work_tm(mbuf,
			data,
			&action->dscp_table,
			dscp);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_ENCAP)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_ENCAP);

		pkt_work_encap(mbuf,
			data,
			&cfg->encap,
			ip,
			total_length,
			ip_offset);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_NAT)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_NAT);

		if (cfg->common.ip_version)
			pkt_ipv4_work_nat(ip, data, &cfg->nat);
		else
			pkt_ipv6_work_nat(ip, data, &cfg->nat);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_TTL)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_TTL);

		if (cfg->common.ip_version)
			drop_mask |= pkt_ipv4_work_ttl(ip, data);
		else
			drop_mask |= pkt_ipv6_work_ttl(ip, data);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_STATS)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_STATS);

		pkt_work_stats(data, total_length);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_TIME)) {
		void *data =
			action_data_get(table_entry, action, RTE_TABLE_ACTION_TIME);

		pkt_work_time(data, time);
	}

	return drop_mask;
}

static __rte_always_inline uint64_t
pkt4_work(struct rte_mbuf **mbufs,
	struct rte_pipeline_table_entry **table_entries,
	uint64_t time,
	struct rte_table_action *action,
	struct ap_config *cfg)
{
	uint64_t drop_mask0 = 0;
	uint64_t drop_mask1 = 0;
	uint64_t drop_mask2 = 0;
	uint64_t drop_mask3 = 0;

	struct rte_mbuf *mbuf0 = mbufs[0];
	struct rte_mbuf *mbuf1 = mbufs[1];
	struct rte_mbuf *mbuf2 = mbufs[2];
	struct rte_mbuf *mbuf3 = mbufs[3];

	struct rte_pipeline_table_entry *table_entry0 = table_entries[0];
	struct rte_pipeline_table_entry *table_entry1 = table_entries[1];
	struct rte_pipeline_table_entry *table_entry2 = table_entries[2];
	struct rte_pipeline_table_entry *table_entry3 = table_entries[3];

	uint32_t ip_offset = action->cfg.common.ip_offset;
	void *ip0 = RTE_MBUF_METADATA_UINT32_PTR(mbuf0, ip_offset);
	void *ip1 = RTE_MBUF_METADATA_UINT32_PTR(mbuf1, ip_offset);
	void *ip2 = RTE_MBUF_METADATA_UINT32_PTR(mbuf2, ip_offset);
	void *ip3 = RTE_MBUF_METADATA_UINT32_PTR(mbuf3, ip_offset);

	uint32_t dscp0, dscp1, dscp2, dscp3;
	uint16_t total_length0, total_length1, total_length2, total_length3;

	if (cfg->common.ip_version) {
		struct ipv4_hdr *hdr0 = ip0;
		struct ipv4_hdr *hdr1 = ip1;
		struct ipv4_hdr *hdr2 = ip2;
		struct ipv4_hdr *hdr3 = ip3;

		dscp0 = hdr0->type_of_service >> 2;
		dscp1 = hdr1->type_of_service >> 2;
		dscp2 = hdr2->type_of_service >> 2;
		dscp3 = hdr3->type_of_service >> 2;

		total_length0 = rte_ntohs(hdr0->total_length);
		total_length1 = rte_ntohs(hdr1->total_length);
		total_length2 = rte_ntohs(hdr2->total_length);
		total_length3 = rte_ntohs(hdr3->total_length);
	} else {
		struct ipv6_hdr *hdr0 = ip0;
		struct ipv6_hdr *hdr1 = ip1;
		struct ipv6_hdr *hdr2 = ip2;
		struct ipv6_hdr *hdr3 = ip3;

		dscp0 = (rte_ntohl(hdr0->vtc_flow) & 0x0F600000) >> 18;
		dscp1 = (rte_ntohl(hdr1->vtc_flow) & 0x0F600000) >> 18;
		dscp2 = (rte_ntohl(hdr2->vtc_flow) & 0x0F600000) >> 18;
		dscp3 = (rte_ntohl(hdr3->vtc_flow) & 0x0F600000) >> 18;

		total_length0 =
			rte_ntohs(hdr0->payload_len) + sizeof(struct ipv6_hdr);
		total_length1 =
			rte_ntohs(hdr1->payload_len) + sizeof(struct ipv6_hdr);
		total_length2 =
			rte_ntohs(hdr2->payload_len) + sizeof(struct ipv6_hdr);
		total_length3 =
			rte_ntohs(hdr3->payload_len) + sizeof(struct ipv6_hdr);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_LB)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_LB);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_LB);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_LB);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_LB);

		pkt_work_lb(mbuf0,
			data0,
			&cfg->lb);

		pkt_work_lb(mbuf1,
			data1,
			&cfg->lb);

		pkt_work_lb(mbuf2,
			data2,
			&cfg->lb);

		pkt_work_lb(mbuf3,
			data3,
			&cfg->lb);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_MTR);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_MTR);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_MTR);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_MTR);

		drop_mask0 |= pkt_work_mtr(mbuf0,
			data0,
			&action->dscp_table,
			action->mp,
			time,
			dscp0,
			total_length0);

		drop_mask1 |= pkt_work_mtr(mbuf1,
			data1,
			&action->dscp_table,
			action->mp,
			time,
			dscp1,
			total_length1);

		drop_mask2 |= pkt_work_mtr(mbuf2,
			data2,
			&action->dscp_table,
			action->mp,
			time,
			dscp2,
			total_length2);

		drop_mask3 |= pkt_work_mtr(mbuf3,
			data3,
			&action->dscp_table,
			action->mp,
			time,
			dscp3,
			total_length3);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_TM)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_TM);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_TM);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_TM);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_TM);

		pkt_work_tm(mbuf0,
			data0,
			&action->dscp_table,
			dscp0);

		pkt_work_tm(mbuf1,
			data1,
			&action->dscp_table,
			dscp1);

		pkt_work_tm(mbuf2,
			data2,
			&action->dscp_table,
			dscp2);

		pkt_work_tm(mbuf3,
			data3,
			&action->dscp_table,
			dscp3);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_ENCAP)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_ENCAP);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_ENCAP);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_ENCAP);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_ENCAP);

		pkt_work_encap(mbuf0,
			data0,
			&cfg->encap,
			ip0,
			total_length0,
			ip_offset);

		pkt_work_encap(mbuf1,
			data1,
			&cfg->encap,
			ip1,
			total_length1,
			ip_offset);

		pkt_work_encap(mbuf2,
			data2,
			&cfg->encap,
			ip2,
			total_length2,
			ip_offset);

		pkt_work_encap(mbuf3,
			data3,
			&cfg->encap,
			ip3,
			total_length3,
			ip_offset);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_NAT)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_NAT);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_NAT);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_NAT);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_NAT);

		if (cfg->common.ip_version) {
			pkt_ipv4_work_nat(ip0, data0, &cfg->nat);
			pkt_ipv4_work_nat(ip1, data1, &cfg->nat);
			pkt_ipv4_work_nat(ip2, data2, &cfg->nat);
			pkt_ipv4_work_nat(ip3, data3, &cfg->nat);
		} else {
			pkt_ipv6_work_nat(ip0, data0, &cfg->nat);
			pkt_ipv6_work_nat(ip1, data1, &cfg->nat);
			pkt_ipv6_work_nat(ip2, data2, &cfg->nat);
			pkt_ipv6_work_nat(ip3, data3, &cfg->nat);
		}
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_TTL)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_TTL);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_TTL);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_TTL);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_TTL);

		if (cfg->common.ip_version) {
			drop_mask0 |= pkt_ipv4_work_ttl(ip0, data0);
			drop_mask1 |= pkt_ipv4_work_ttl(ip1, data1);
			drop_mask2 |= pkt_ipv4_work_ttl(ip2, data2);
			drop_mask3 |= pkt_ipv4_work_ttl(ip3, data3);
		} else {
			drop_mask0 |= pkt_ipv6_work_ttl(ip0, data0);
			drop_mask1 |= pkt_ipv6_work_ttl(ip1, data1);
			drop_mask2 |= pkt_ipv6_work_ttl(ip2, data2);
			drop_mask3 |= pkt_ipv6_work_ttl(ip3, data3);
		}
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_STATS)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_STATS);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_STATS);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_STATS);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_STATS);

		pkt_work_stats(data0, total_length0);
		pkt_work_stats(data1, total_length1);
		pkt_work_stats(data2, total_length2);
		pkt_work_stats(data3, total_length3);
	}

	if (cfg->action_mask & (1LLU << RTE_TABLE_ACTION_TIME)) {
		void *data0 =
			action_data_get(table_entry0, action, RTE_TABLE_ACTION_TIME);
		void *data1 =
			action_data_get(table_entry1, action, RTE_TABLE_ACTION_TIME);
		void *data2 =
			action_data_get(table_entry2, action, RTE_TABLE_ACTION_TIME);
		void *data3 =
			action_data_get(table_entry3, action, RTE_TABLE_ACTION_TIME);

		pkt_work_time(data0, time);
		pkt_work_time(data1, time);
		pkt_work_time(data2, time);
		pkt_work_time(data3, time);
	}

	return drop_mask0 |
		(drop_mask1 << 1) |
		(drop_mask2 << 2) |
		(drop_mask3 << 3);
}

static __rte_always_inline int
ah(struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	struct rte_pipeline_table_entry **entries,
	struct rte_table_action *action,
	struct ap_config *cfg)
{
	uint64_t pkts_drop_mask = 0;
	uint64_t time = 0;

	if (cfg->action_mask & ((1LLU << RTE_TABLE_ACTION_MTR) |
		(1LLU << RTE_TABLE_ACTION_TIME)))
		time = rte_rdtsc();

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < (n_pkts & (~0x3LLU)); i += 4) {
			uint64_t drop_mask;

			drop_mask = pkt4_work(&pkts[i],
				&entries[i],
				time,
				action,
				cfg);

			pkts_drop_mask |= drop_mask << i;
		}

		for ( ; i < n_pkts; i++) {
			uint64_t drop_mask;

			drop_mask = pkt_work(pkts[i],
				entries[i],
				time,
				action,
				cfg);

			pkts_drop_mask |= drop_mask << i;
		}
	} else
		for ( ; pkts_mask; ) {
			uint32_t pos = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pos;
			uint64_t drop_mask;

			drop_mask = pkt_work(pkts[pos],
				entries[pos],
				time,
				action,
				cfg);

			pkts_mask &= ~pkt_mask;
			pkts_drop_mask |= drop_mask << pos;
		}

	rte_pipeline_ah_packet_drop(p, pkts_drop_mask);

	return 0;
}

static int
ah_default(struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	struct rte_pipeline_table_entry **entries,
	void *arg)
{
	struct rte_table_action *action = arg;

	return ah(p,
		pkts,
		pkts_mask,
		entries,
		action,
		&action->cfg);
}

static rte_pipeline_table_action_handler_hit
ah_selector(struct rte_table_action *action)
{
	if (action->cfg.action_mask == (1LLU << RTE_TABLE_ACTION_FWD))
		return NULL;

	return ah_default;
}

int
rte_table_action_table_params_get(struct rte_table_action *action,
	struct rte_pipeline_table_params *params)
{
	rte_pipeline_table_action_handler_hit f_action_hit;
	uint32_t total_size;

	/* Check input arguments */
	if ((action == NULL) ||
		(params == NULL))
		return -EINVAL;

	f_action_hit = ah_selector(action);
	total_size = rte_align32pow2(action->data.total_size);

	/* Fill in params */
	params->f_action_hit = f_action_hit;
	params->f_action_miss = NULL;
	params->arg_ah = (f_action_hit) ? action : NULL;
	params->action_data_size = total_size -
		sizeof(struct rte_pipeline_table_entry);

	return 0;
}

int
rte_table_action_free(struct rte_table_action *action)
{
	if (action == NULL)
		return 0;

	rte_free(action);

	return 0;
}
