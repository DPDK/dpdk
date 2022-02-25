/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_ETHDEV_H__
#define __CN10K_ETHDEV_H__

#include <cnxk_ethdev.h>
#include <cnxk_security.h>

struct cn10k_eth_txq {
	uint64_t send_hdr_w0;
	int64_t fc_cache_pkts;
	uint64_t *fc_mem;
	uintptr_t lmt_base;
	rte_iova_t io_addr;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
	rte_iova_t cpt_io_addr;
	uint64_t sa_base;
	uint64_t *cpt_fc;
	uint16_t cpt_desc;
	uint64_t lso_tun_fmt;
	uint64_t ts_mem;
	uint64_t mark_flag : 8;
	uint64_t mark_fmt : 48;
} __plt_cache_aligned;

struct cn10k_eth_rxq {
	uint64_t mbuf_initializer;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t data_off;
	uint64_t sa_base;
	uint64_t lmt_base;
	uint64_t aura_handle;
	uint16_t rq;
	struct cnxk_timesync_info *tstamp;
} __plt_cache_aligned;

/* Private data in sw rsvd area of struct roc_ot_ipsec_inb_sa */
struct cn10k_inb_priv_data {
	void *userdata;
	int reass_dynfield_off;
	int reass_dynflag_bit;
	struct cnxk_eth_sec_sess *eth_sec;
};

/* Private data in sw rsvd area of struct roc_ot_ipsec_outb_sa */
struct cn10k_outb_priv_data {
	void *userdata;
	/* Rlen computation data */
	struct cnxk_ipsec_outb_rlens rlens;
	/* Back pointer to eth sec session */
	struct cnxk_eth_sec_sess *eth_sec;
	/* SA index */
	uint32_t sa_idx;
};

struct cn10k_sec_sess_priv {
	union {
		struct {
			uint32_t sa_idx;
			uint8_t inb_sa : 1;
			uint8_t outer_ip_ver : 1;
			uint8_t mode : 1;
			uint8_t roundup_byte : 5;
			uint8_t roundup_len;
			uint16_t partial_len;
		};

		uint64_t u64;
	};
} __rte_packed;

/* Rx and Tx routines */
void cn10k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn10k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

/* Security context setup */
void cn10k_eth_sec_ops_override(void);

/* SSO Work callback */
void cn10k_eth_sec_sso_work_cb(uint64_t *gw, void *args,
			       uint32_t soft_exp_event);

#define LMT_OFF(lmt_addr, lmt_num, offset)                                     \
	(void *)((uintptr_t)(lmt_addr) +                                       \
		 ((uint64_t)(lmt_num) << ROC_LMT_LINE_SIZE_LOG2) + (offset))

#endif /* __CN10K_ETHDEV_H__ */
