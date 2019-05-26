/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TM_H__
#define __OTX2_TM_H__

#include <stdbool.h>

#include <rte_tm_driver.h>

#define NIX_TM_DEFAULT_TREE	BIT_ULL(0)

struct otx2_eth_dev;

void otx2_nix_tm_conf_init(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_init_default(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_fini(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_get_leaf_data(struct otx2_eth_dev *dev, uint16_t sq,
			      uint32_t *rr_quantum, uint16_t *smq);
int otx2_nix_tm_sw_xoff(void *_txq, bool dev_started);
int otx2_nix_sq_sqb_aura_fc(void *_txq, bool enable);

struct otx2_nix_tm_node {
	TAILQ_ENTRY(otx2_nix_tm_node) node;
	uint32_t id;
	uint32_t hw_id;
	uint32_t priority;
	uint32_t weight;
	uint16_t level_id;
	uint16_t hw_lvl_id;
	uint32_t rr_prio;
	uint32_t rr_num;
	uint32_t max_prio;
	uint32_t parent_hw_id;
	uint32_t flags;
#define NIX_TM_NODE_HWRES	BIT_ULL(0)
#define NIX_TM_NODE_ENABLED	BIT_ULL(1)
#define NIX_TM_NODE_USER	BIT_ULL(2)
	struct otx2_nix_tm_node *parent;
	struct rte_tm_node_params params;
};

struct otx2_nix_tm_shaper_profile {
	TAILQ_ENTRY(otx2_nix_tm_shaper_profile) shaper;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params profile;
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
#define NIX_TM_RR_QUANTUM_MAX ((1 << 24) - 1)

/* DEFAULT_RR_WEIGHT * NIX_TM_RR_QUANTUM_MAX / MAX_SCHED_WEIGHT  */
/* = NIX_MAX_HW_MTU */
#define DEFAULT_RR_WEIGHT 71

/** NIX rate limits */
#define MAX_RATE_DIV_EXP 12
#define MAX_RATE_EXPONENT 0xf
#define MAX_RATE_MANTISSA 0xff

/** NIX rate limiter time-wheel resolution */
#define L1_TIME_WHEEL_CCLK_TICKS 240
#define LX_TIME_WHEEL_CCLK_TICKS 860

#define CCLK_HZ 1000000000

/* NIX rate calculation
 *	CCLK = coprocessor-clock frequency in MHz
 *	CCLK_TICKS = rate limiter time-wheel resolution
 *
 *	PIR_ADD = ((256 + NIX_*_PIR[RATE_MANTISSA])
 *		<< NIX_*_PIR[RATE_EXPONENT]) / 256
 *	PIR = (CCLK / (CCLK_TICKS << NIX_*_PIR[RATE_DIVIDER_EXPONENT]))
 *		* PIR_ADD
 *
 *	CIR_ADD = ((256 + NIX_*_CIR[RATE_MANTISSA])
 *		<< NIX_*_CIR[RATE_EXPONENT]) / 256
 *	CIR = (CCLK / (CCLK_TICKS << NIX_*_CIR[RATE_DIVIDER_EXPONENT]))
 *		* CIR_ADD
 */
#define SHAPER_RATE(cclk_hz, cclk_ticks, \
			exponent, mantissa, div_exp) \
	(((uint64_t)(cclk_hz) * ((256 + (mantissa)) << (exponent))) \
		/ (((cclk_ticks) << (div_exp)) * 256))

#define L1_SHAPER_RATE(cclk_hz, exponent, mantissa, div_exp) \
	SHAPER_RATE(cclk_hz, L1_TIME_WHEEL_CCLK_TICKS, \
			exponent, mantissa, div_exp)

#define LX_SHAPER_RATE(cclk_hz, exponent, mantissa, div_exp) \
	SHAPER_RATE(cclk_hz, LX_TIME_WHEEL_CCLK_TICKS, \
			exponent, mantissa, div_exp)

/* Shaper rate limits */
#define MIN_SHAPER_RATE(cclk_hz, cclk_ticks) \
	SHAPER_RATE(cclk_hz, cclk_ticks, 0, 0, MAX_RATE_DIV_EXP)

#define MAX_SHAPER_RATE(cclk_hz, cclk_ticks) \
	SHAPER_RATE(cclk_hz, cclk_ticks, MAX_RATE_EXPONENT, \
			MAX_RATE_MANTISSA, 0)

#define MIN_L1_SHAPER_RATE(cclk_hz) \
	MIN_SHAPER_RATE(cclk_hz, L1_TIME_WHEEL_CCLK_TICKS)

#define MAX_L1_SHAPER_RATE(cclk_hz) \
	MAX_SHAPER_RATE(cclk_hz, L1_TIME_WHEEL_CCLK_TICKS)

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

#endif /* __OTX2_TM_H__ */
