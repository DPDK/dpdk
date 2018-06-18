/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _THUNDERX_NICVF_HW_H
#define _THUNDERX_NICVF_HW_H

#include <stdint.h>

#include "nicvf_hw_defs.h"

#define	PCI_VENDOR_ID_CAVIUM				0x177D
#define	PCI_DEVICE_ID_THUNDERX_CN88XX_PASS1_NICVF	0x0011
#define	PCI_DEVICE_ID_THUNDERX_NICVF			0xA034
#define	PCI_SUB_DEVICE_ID_CN88XX_PASS1_NICVF		0xA11E
#define	PCI_SUB_DEVICE_ID_CN88XX_PASS2_NICVF		0xA134
#define	PCI_SUB_DEVICE_ID_CN81XX_NICVF			0xA234
#define	PCI_SUB_DEVICE_ID_CN83XX_NICVF			0xA334

#define NICVF_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define NICVF_GET_RX_STATS(reg) \
	nicvf_reg_read(nic, NIC_VNIC_RX_STAT_0_13 | (reg << 3))
#define NICVF_GET_TX_STATS(reg) \
	nicvf_reg_read(nic, NIC_VNIC_TX_STAT_0_4 | (reg << 3))

#define NICVF_CAP_TUNNEL_PARSING	(1ULL << 0)
/* Additional word in Rx descriptor to hold optional tunneling extension info */
#define NICVF_CAP_CQE_RX2		(1ULL << 1)
/* The device capable of setting NIC_CQE_RX_S[APAD] == 0 */
#define NICVF_CAP_DISABLE_APAD		(1ULL << 2)

enum nicvf_tns_mode {
	NIC_TNS_BYPASS_MODE,
	NIC_TNS_MODE,
};

enum nicvf_err_e {
	NICVF_OK,
	NICVF_ERR_SET_QS = -8191,/* -8191 */
	NICVF_ERR_RESET_QS,      /* -8190 */
	NICVF_ERR_REG_POLL,      /* -8189 */
	NICVF_ERR_RBDR_RESET,    /* -8188 */
	NICVF_ERR_RBDR_DISABLE,  /* -8187 */
	NICVF_ERR_RBDR_PREFETCH, /* -8186 */
	NICVF_ERR_RBDR_RESET1,   /* -8185 */
	NICVF_ERR_RBDR_RESET2,   /* -8184 */
	NICVF_ERR_RQ_CLAIM,      /* -8183 */
	NICVF_ERR_RQ_PF_CFG,	 /* -8182 */
	NICVF_ERR_RQ_BP_CFG,	 /* -8181 */
	NICVF_ERR_RQ_DROP_CFG,	 /* -8180 */
	NICVF_ERR_CQ_DISABLE,	 /* -8179 */
	NICVF_ERR_CQ_RESET,	 /* -8178 */
	NICVF_ERR_SQ_DISABLE,	 /* -8177 */
	NICVF_ERR_SQ_RESET,	 /* -8176 */
	NICVF_ERR_SQ_PF_CFG,	 /* -8175 */
	NICVF_ERR_LOOPBACK_CFG,  /* -8174 */
	NICVF_ERR_BASE_INIT,     /* -8173 */
	NICVF_ERR_RSS_TBL_UPDATE,/* -8172 */
	NICVF_ERR_RSS_GET_SZ,    /* -8171 */
};

typedef nicvf_iova_addr_t (*rbdr_pool_get_handler)(void *dev, void *opaque);

struct nicvf_hw_rx_qstats {
	uint64_t q_rx_bytes;
	uint64_t q_rx_packets;
};

struct nicvf_hw_tx_qstats {
	uint64_t q_tx_bytes;
	uint64_t q_tx_packets;
};

struct nicvf_hw_stats {
	uint64_t rx_bytes;
	uint64_t rx_ucast_frames;
	uint64_t rx_bcast_frames;
	uint64_t rx_mcast_frames;
	uint64_t rx_fcs_errors;
	uint64_t rx_l2_errors;
	uint64_t rx_drop_red;
	uint64_t rx_drop_red_bytes;
	uint64_t rx_drop_overrun;
	uint64_t rx_drop_overrun_bytes;
	uint64_t rx_drop_bcast;
	uint64_t rx_drop_mcast;
	uint64_t rx_drop_l3_bcast;
	uint64_t rx_drop_l3_mcast;

	uint64_t tx_bytes_ok;
	uint64_t tx_ucast_frames_ok;
	uint64_t tx_bcast_frames_ok;
	uint64_t tx_mcast_frames_ok;
	uint64_t tx_drops;
};

struct nicvf_rss_reta_info {
	uint8_t hash_bits;
	uint16_t rss_size;
	uint8_t ind_tbl[NIC_MAX_RSS_IDR_TBL_SIZE];
};

/* Common structs used in DPDK and base layer are defined in DPDK layer */
#include "../nicvf_struct.h"

NICVF_STATIC_ASSERT(sizeof(struct nicvf_rbdr) <= 128);
NICVF_STATIC_ASSERT(sizeof(struct nicvf_txq) <= 128);
NICVF_STATIC_ASSERT(sizeof(struct nicvf_rxq) <= 128);

static inline void
nicvf_reg_write(struct nicvf *nic, uint32_t offset, uint64_t val)
{
	nicvf_addr_write(nic->reg_base + offset, val);
}

static inline uint64_t
nicvf_reg_read(struct nicvf *nic, uint32_t offset)
{
	return nicvf_addr_read(nic->reg_base + offset);
}

static inline uintptr_t
nicvf_qset_base(struct nicvf *nic, uint32_t qidx)
{
	return nic->reg_base + (qidx << NIC_Q_NUM_SHIFT);
}

static inline void
nicvf_queue_reg_write(struct nicvf *nic, uint32_t offset, uint32_t qidx,
		      uint64_t val)
{
	nicvf_addr_write(nicvf_qset_base(nic, qidx) + offset, val);
}

static inline uint64_t
nicvf_queue_reg_read(struct nicvf *nic, uint32_t offset, uint32_t qidx)
{
	return	nicvf_addr_read(nicvf_qset_base(nic, qidx) + offset);
}

static inline void
nicvf_disable_all_interrupts(struct nicvf *nic)
{
	nicvf_reg_write(nic, NIC_VF_ENA_W1C, NICVF_INTR_ALL_MASK);
	nicvf_reg_write(nic, NIC_VF_INT, NICVF_INTR_ALL_MASK);
}

static inline uint32_t
nicvf_hw_version(struct nicvf *nic)
{
	return nic->subsystem_device_id;
}

static inline uint64_t
nicvf_hw_cap(struct nicvf *nic)
{
	return nic->hwcap;
}

int nicvf_base_init(struct nicvf *nic);

int nicvf_reg_get_count(void);
int nicvf_reg_poll_interrupts(struct nicvf *nic);
int nicvf_reg_dump(struct nicvf *nic, uint64_t *data);

int nicvf_qset_config(struct nicvf *nic);
int nicvf_qset_reclaim(struct nicvf *nic);

int nicvf_qset_rbdr_config(struct nicvf *nic, uint16_t qidx);
int nicvf_qset_rbdr_reclaim(struct nicvf *nic, uint16_t qidx);
int nicvf_qset_rbdr_precharge(void *dev, struct nicvf *nic,
			      uint16_t ridx, rbdr_pool_get_handler handler,
			      uint32_t max_buffs);
int nicvf_qset_rbdr_active(struct nicvf *nic, uint16_t qidx);

int nicvf_qset_rq_config(struct nicvf *nic, uint16_t qidx,
			 struct nicvf_rxq *rxq);
int nicvf_qset_rq_reclaim(struct nicvf *nic, uint16_t qidx);

int nicvf_qset_cq_config(struct nicvf *nic, uint16_t qidx,
			 struct nicvf_rxq *rxq);
int nicvf_qset_cq_reclaim(struct nicvf *nic, uint16_t qidx);

int nicvf_qset_sq_config(struct nicvf *nic, uint16_t qidx,
			 struct nicvf_txq *txq);
int nicvf_qset_sq_reclaim(struct nicvf *nic, uint16_t qidx);

uint32_t nicvf_qsize_rbdr_roundup(uint32_t val);
uint32_t nicvf_qsize_cq_roundup(uint32_t val);
uint32_t nicvf_qsize_sq_roundup(uint32_t val);

void nicvf_vlan_hw_strip(struct nicvf *nic, bool enable);

void nicvf_apad_config(struct nicvf *nic, bool enable);
void nicvf_first_skip_config(struct nicvf *nic, uint8_t dwords);

int nicvf_rss_config(struct nicvf *nic, uint32_t  qcnt, uint64_t cfg);
int nicvf_rss_term(struct nicvf *nic);

int nicvf_rss_reta_update(struct nicvf *nic, uint8_t *tbl, uint32_t max_count);
int nicvf_rss_reta_query(struct nicvf *nic, uint8_t *tbl, uint32_t max_count);

void nicvf_rss_set_key(struct nicvf *nic, uint8_t *key);
void nicvf_rss_get_key(struct nicvf *nic, uint8_t *key);

void nicvf_rss_set_cfg(struct nicvf *nic, uint64_t val);
uint64_t nicvf_rss_get_cfg(struct nicvf *nic);

int nicvf_loopback_config(struct nicvf *nic, bool enable);

void nicvf_hw_get_stats(struct nicvf *nic, struct nicvf_hw_stats *stats);
void nicvf_hw_get_rx_qstats(struct nicvf *nic,
			    struct nicvf_hw_rx_qstats *qstats, uint16_t qidx);
void nicvf_hw_get_tx_qstats(struct nicvf *nic,
			    struct nicvf_hw_tx_qstats *qstats, uint16_t qidx);

#endif /* _THUNDERX_NICVF_HW_H */
