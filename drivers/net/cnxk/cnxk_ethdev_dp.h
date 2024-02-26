/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */
#ifndef __CNXK_ETHDEV_DP_H__
#define __CNXK_ETHDEV_DP_H__

#include <rte_mbuf.h>

/* If PTP is enabled additional SEND MEM DESC is required which
 * takes 2 words, hence max 7 iova address are possible
 */
#if defined(RTE_LIBRTE_IEEE1588)
#define CNXK_NIX_TX_NB_SEG_MAX 7
#else
#define CNXK_NIX_TX_NB_SEG_MAX 9
#endif

#define CNXK_NIX_TX_MSEG_SG_DWORDS                                             \
	((RTE_ALIGN_MUL_CEIL(CNXK_NIX_TX_NB_SEG_MAX, 3) / 3) +                 \
	 CNXK_NIX_TX_NB_SEG_MAX)

/* Default mark value used when none is provided. */
#define CNXK_FLOW_ACTION_FLAG_DEFAULT 0xffff
#define CNXK_NIX_TIMESYNC_RX_OFFSET 8

#define PTYPE_NON_TUNNEL_WIDTH	  16
#define PTYPE_TUNNEL_WIDTH	  12
#define PTYPE_NON_TUNNEL_ARRAY_SZ BIT(PTYPE_NON_TUNNEL_WIDTH)
#define PTYPE_TUNNEL_ARRAY_SZ	  BIT(PTYPE_TUNNEL_WIDTH)
#define PTYPE_ARRAY_SZ                                                         \
	((PTYPE_NON_TUNNEL_ARRAY_SZ + PTYPE_TUNNEL_ARRAY_SZ) * sizeof(uint16_t))

/* NIX_RX_PARSE_S's ERRCODE + ERRLEV (12 bits) */
#define ERRCODE_ERRLEN_WIDTH 12
#define ERR_ARRAY_SZ	     ((BIT(ERRCODE_ERRLEN_WIDTH)) * sizeof(uint32_t))

#define SA_BASE_TBL_SZ	(RTE_MAX_ETHPORTS * sizeof(uintptr_t))
#define MEMPOOL_TBL_SZ	(RTE_MAX_ETHPORTS * sizeof(uintptr_t))

#define CNXK_NIX_UDP_TUN_BITMASK                                               \
	((1ull << (RTE_MBUF_F_TX_TUNNEL_VXLAN >> 45)) |                               \
	 (1ull << (RTE_MBUF_F_TX_TUNNEL_GENEVE >> 45)))

/* Subtype from inline outbound error event */
#define CNXK_ETHDEV_SEC_OUTB_EV_SUB 0xFFUL

/* SPI will be in 20 bits of tag */
#define CNXK_ETHDEV_SPI_TAG_MASK 0xFFFFFUL

#define CNXK_NIX_PFC_CHAN_COUNT 16

#define CNXK_TM_MARK_VLAN_DEI BIT_ULL(0)
#define CNXK_TM_MARK_IP_DSCP  BIT_ULL(1)
#define CNXK_TM_MARK_IP_ECN   BIT_ULL(2)

#define CNXK_TM_MARK_MASK                                                      \
	(CNXK_TM_MARK_VLAN_DEI | CNXK_TM_MARK_IP_DSCP | CNXK_TM_MARK_IP_ECN)

#define CNXK_TX_MARK_FMT_MASK (0xFFFFFFFFFFFFull)

struct cnxk_eth_txq_comp {
	uintptr_t desc_base;
	uintptr_t cq_door;
	int64_t *cq_status;
	uint64_t wdata;
	uint32_t head;
	uint32_t qmask;
	uint32_t nb_desc_mask;
	uint32_t available;
	uint32_t sqe_id;
	bool ena;
	struct rte_mbuf **ptr;
	rte_spinlock_t ext_buf_lock;
};

struct cnxk_timesync_info {
	uint8_t rx_ready;
	uint64_t rx_tstamp;
	uint64_t rx_tstamp_dynflag;
	int tstamp_dynfield_offset;
	rte_iova_t tx_tstamp_iova;
	uint64_t *tx_tstamp;
} __plt_cache_aligned;

/* Inlines */
static __rte_always_inline uint64_t
cnxk_pktmbuf_detach(struct rte_mbuf *m, uint64_t *aura)
{
	struct rte_mempool *mp = m->pool;
	uint32_t mbuf_size, buf_len;
	struct rte_mbuf *md;
	uint16_t priv_size;
	uint16_t refcount;

	/* Update refcount of direct mbuf */
	md = rte_mbuf_from_indirect(m);
	if (aura)
		*aura = roc_npa_aura_handle_to_aura(md->pool->pool_id);
	refcount = rte_mbuf_refcnt_update(md, -1);

	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = (uint32_t)(sizeof(struct rte_mbuf) + priv_size);
	buf_len = rte_pktmbuf_data_room_size(mp);

	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	rte_mbuf_iova_set(m, rte_mempool_virt2iova(m) + mbuf_size);
	m->buf_len = (uint16_t)buf_len;
	rte_pktmbuf_reset_headroom(m);
	m->data_len = 0;
	m->ol_flags = 0;
	m->next = NULL;
	m->nb_segs = 1;

	/* Now indirect mbuf is safe to free */
	rte_pktmbuf_free(m);

	if (refcount == 0) {
		rte_mbuf_refcnt_set(md, 1);
		md->data_len = 0;
		md->ol_flags = 0;
		md->next = NULL;
		md->nb_segs = 1;
		return 0;
	} else {
		return 1;
	}
}

static __rte_always_inline uint64_t
cnxk_nix_prefree_seg(struct rte_mbuf *m, uint64_t *aura)
{
	if (likely(rte_mbuf_refcnt_read(m) == 1)) {
		if (!RTE_MBUF_DIRECT(m))
			return cnxk_pktmbuf_detach(m, aura);

		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	} else if (rte_mbuf_refcnt_update(m, -1) == 0) {
		if (!RTE_MBUF_DIRECT(m))
			return cnxk_pktmbuf_detach(m, aura);

		rte_mbuf_refcnt_set(m, 1);
		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	}

	/* Mbuf is having refcount more than 1 so need not to be freed */
	return 1;
}

static inline rte_mbuf_timestamp_t *
cnxk_nix_timestamp_dynfield(struct rte_mbuf *mbuf,
			    struct cnxk_timesync_info *info)
{
	return RTE_MBUF_DYNFIELD(mbuf, info->tstamp_dynfield_offset,
				 rte_mbuf_timestamp_t *);
}

static __rte_always_inline uintptr_t
cnxk_nix_sa_base_get(uint16_t port, const void *lookup_mem)
{
	uintptr_t sa_base_tbl;

	sa_base_tbl = (uintptr_t)lookup_mem;
	sa_base_tbl += PTYPE_ARRAY_SZ + ERR_ARRAY_SZ;
	return *((const uintptr_t *)sa_base_tbl + port);
}

static __rte_always_inline uintptr_t
cnxk_nix_inl_metapool_get(uint16_t port, const void *lookup_mem)
{
	uintptr_t metapool_tbl;

	metapool_tbl = (uintptr_t)lookup_mem;
	metapool_tbl += PTYPE_ARRAY_SZ + ERR_ARRAY_SZ + SA_BASE_TBL_SZ;
	return *((const uintptr_t *)metapool_tbl + port);
}

#endif /* __CNXK_ETHDEV_DP_H__ */
