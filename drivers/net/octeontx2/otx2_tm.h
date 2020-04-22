/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TM_H__
#define __OTX2_TM_H__

#include <stdbool.h>

#include <rte_tm_driver.h>

#define NIX_TM_DEFAULT_TREE	BIT_ULL(0)
#define NIX_TM_COMMITTED	BIT_ULL(1)
#define NIX_TM_RATE_LIMIT_TREE	BIT_ULL(2)
#define NIX_TM_TL1_NO_SP	BIT_ULL(3)

struct otx2_eth_dev;

void otx2_nix_tm_conf_init(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_init_default(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_fini(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_ops_get(struct rte_eth_dev *eth_dev, void *ops);
int otx2_nix_tm_get_leaf_data(struct otx2_eth_dev *dev, uint16_t sq,
			      uint32_t *rr_quantum, uint16_t *smq);
int otx2_nix_tm_set_queue_rate_limit(struct rte_eth_dev *eth_dev,
				     uint16_t queue_idx, uint16_t tx_rate);
int otx2_nix_sq_flush_pre(void *_txq, bool dev_started);
int otx2_nix_sq_flush_post(void *_txq);
int otx2_nix_sq_enable(void *_txq);
int otx2_nix_get_link(struct otx2_eth_dev *dev);
int otx2_nix_sq_sqb_aura_fc(void *_txq, bool enable);

struct otx2_nix_tm_node {
	TAILQ_ENTRY(otx2_nix_tm_node) node;
	uint32_t id;
	uint32_t hw_id;
	uint32_t priority;
	uint32_t weight;
	uint16_t lvl;
	uint16_t hw_lvl;
	uint32_t rr_prio;
	uint32_t rr_num;
	uint32_t max_prio;
	uint32_t parent_hw_id;
	uint32_t flags:16;
#define NIX_TM_NODE_HWRES	BIT_ULL(0)
#define NIX_TM_NODE_ENABLED	BIT_ULL(1)
#define NIX_TM_NODE_USER	BIT_ULL(2)
#define NIX_TM_NODE_RED_DISCARD BIT_ULL(3)
	/* Shaper algorithm for RED state @NIX_REDALG_E */
	uint32_t red_algo:2;
	uint32_t pkt_mode:1;

	struct otx2_nix_tm_node *parent;
	struct rte_tm_node_params params;

	/* Last stats */
	uint64_t last_pkts;
	uint64_t last_bytes;
};

struct otx2_nix_tm_shaper_profile {
	TAILQ_ENTRY(otx2_nix_tm_shaper_profile) shaper;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params params; /* Rate in bits/sec */
};

struct shaper_params {
	uint64_t burst_exponent;
	uint64_t burst_mantissa;
	uint64_t div_exp;
	uint64_t exponent;
	uint64_t mantissa;
	uint64_t burst;
	uint64_t rate;
};

TAILQ_HEAD(otx2_nix_tm_node_list, otx2_nix_tm_node);
TAILQ_HEAD(otx2_nix_tm_shaper_profile_list, otx2_nix_tm_shaper_profile);

#define MAX_SCHED_WEIGHT ((uint8_t)~0)
#define NIX_TM_RR_QUANTUM_MAX (BIT_ULL(24) - 1)
#define NIX_TM_WEIGHT_TO_RR_QUANTUM(__weight)			\
		((((__weight) & MAX_SCHED_WEIGHT) *             \
		  NIX_TM_RR_QUANTUM_MAX) / MAX_SCHED_WEIGHT)

/* DEFAULT_RR_WEIGHT * NIX_TM_RR_QUANTUM_MAX / MAX_SCHED_WEIGHT  */
/* = NIX_MAX_HW_MTU */
#define DEFAULT_RR_WEIGHT 71

/** NIX rate limits */
#define MAX_RATE_DIV_EXP 12
#define MAX_RATE_EXPONENT 0xf
#define MAX_RATE_MANTISSA 0xff

#define NIX_SHAPER_RATE_CONST ((uint64_t)2E6)

/* NIX rate calculation in Bits/Sec
 *	PIR_ADD = ((256 + NIX_*_PIR[RATE_MANTISSA])
 *		<< NIX_*_PIR[RATE_EXPONENT]) / 256
 *	PIR = (2E6 * PIR_ADD / (1 << NIX_*_PIR[RATE_DIVIDER_EXPONENT]))
 *
 *	CIR_ADD = ((256 + NIX_*_CIR[RATE_MANTISSA])
 *		<< NIX_*_CIR[RATE_EXPONENT]) / 256
 *	CIR = (2E6 * CIR_ADD / (CCLK_TICKS << NIX_*_CIR[RATE_DIVIDER_EXPONENT]))
 */
#define SHAPER_RATE(exponent, mantissa, div_exp) \
	((NIX_SHAPER_RATE_CONST * ((256 + (mantissa)) << (exponent)))\
		/ (((1ull << (div_exp)) * 256)))

/* 96xx rate limits in Bits/Sec */
#define MIN_SHAPER_RATE \
	SHAPER_RATE(0, 0, MAX_RATE_DIV_EXP)

#define MAX_SHAPER_RATE \
	SHAPER_RATE(MAX_RATE_EXPONENT, MAX_RATE_MANTISSA, 0)

/* Min is limited so that NIX_AF_SMQX_CFG[MINLEN]+ADJUST is not -ve */
#define NIX_LENGTH_ADJUST_MIN ((int)-NIX_MIN_HW_FRS + 1)
#define NIX_LENGTH_ADJUST_MAX 255

/** TM Shaper - low level operations */

/** NIX burst limits */
#define MAX_BURST_EXPONENT 0xf
#define MAX_BURST_MANTISSA 0xff

/* NIX burst calculation
 *	PIR_BURST = ((256 + NIX_*_PIR[BURST_MANTISSA])
 *		<< (NIX_*_PIR[BURST_EXPONENT] + 1))
 *			/ 256
 *
 *	CIR_BURST = ((256 + NIX_*_CIR[BURST_MANTISSA])
 *		<< (NIX_*_CIR[BURST_EXPONENT] + 1))
 *			/ 256
 */
#define SHAPER_BURST(exponent, mantissa) \
	(((256 + (mantissa)) << ((exponent) + 1)) / 256)

/** Shaper burst limits */
#define MIN_SHAPER_BURST \
	SHAPER_BURST(0, 0)

#define MAX_SHAPER_BURST \
	SHAPER_BURST(MAX_BURST_EXPONENT,\
		MAX_BURST_MANTISSA)

/* Default TL1 priority and Quantum from AF */
#define TXSCH_TL1_DFLT_RR_QTM  ((1 << 24) - 1)
#define TXSCH_TL1_DFLT_RR_PRIO 1

#define TXSCH_TLX_SP_PRIO_MAX 10

static inline const char *
nix_hwlvl2str(uint32_t hw_lvl)
{
	switch (hw_lvl) {
	case NIX_TXSCH_LVL_MDQ:
		return "SMQ/MDQ";
	case NIX_TXSCH_LVL_TL4:
		return "TL4";
	case NIX_TXSCH_LVL_TL3:
		return "TL3";
	case NIX_TXSCH_LVL_TL2:
		return "TL2";
	case NIX_TXSCH_LVL_TL1:
		return "TL1";
	default:
		break;
	}

	return "???";
}

#endif /* __OTX2_TM_H__ */
