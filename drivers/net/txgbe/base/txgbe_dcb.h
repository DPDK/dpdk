/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_DCB_H_
#define _TXGBE_DCB_H_

#include "txgbe_type.h"

/* DCB defines */
/* DCB credit calculation defines */
#define TXGBE_DCB_CREDIT_QUANTUM	64
#define TXGBE_DCB_MAX_CREDIT_REFILL	200   /* 200 * 64B = 12800B */
#define TXGBE_DCB_MAX_TSO_SIZE		(32 * 1024) /* Max TSO pkt size in DCB*/
#define TXGBE_DCB_MAX_CREDIT		(2 * TXGBE_DCB_MAX_CREDIT_REFILL)

/* 513 for 32KB TSO packet */
#define TXGBE_DCB_MIN_TSO_CREDIT	\
	((TXGBE_DCB_MAX_TSO_SIZE / TXGBE_DCB_CREDIT_QUANTUM) + 1)

#define TXGBE_DCB_TX_CONFIG		0
#define TXGBE_DCB_RX_CONFIG		1

struct txgbe_dcb_support {
	u32 capabilities; /* DCB capabilities */

	/* Each bit represents a number of TCs configurable in the hw.
	 * If 8 traffic classes can be configured, the value is 0x80.
	 */
	u8 traffic_classes;
	u8 pfc_traffic_classes;
};

enum txgbe_dcb_tsa {
	txgbe_dcb_tsa_ets = 0,
	txgbe_dcb_tsa_group_strict_cee,
	txgbe_dcb_tsa_strict
};

/* Traffic class bandwidth allocation per direction */
struct txgbe_dcb_tc_path {
	u8 bwg_id; /* Bandwidth Group (BWG) ID */
	u8 bwg_percent; /* % of BWG's bandwidth */
	u8 link_percent; /* % of link bandwidth */
	u8 up_to_tc_bitmap; /* User Priority to Traffic Class mapping */
	u16 data_credits_refill; /* Credit refill amount in 64B granularity */
	u16 data_credits_max; /* Max credits for a configured packet buffer
			       * in 64B granularity.
			       */
	enum txgbe_dcb_tsa tsa; /* Link or Group Strict Priority */
};

enum txgbe_dcb_pfc {
	txgbe_dcb_pfc_disabled = 0,
	txgbe_dcb_pfc_enabled,
	txgbe_dcb_pfc_enabled_txonly,
	txgbe_dcb_pfc_enabled_rxonly
};

/* Traffic class configuration */
struct txgbe_dcb_tc_config {
	struct txgbe_dcb_tc_path path[2]; /* One each for Tx/Rx */
	enum txgbe_dcb_pfc pfc; /* Class based flow control setting */

	u16 desc_credits_max; /* For Tx Descriptor arbitration */
	u8 tc; /* Traffic class (TC) */
};

enum txgbe_dcb_pba {
	/* PBA[0-7] each use 64KB FIFO */
	txgbe_dcb_pba_equal = PBA_STRATEGY_EQUAL,
	/* PBA[0-3] each use 80KB, PBA[4-7] each use 48KB */
	txgbe_dcb_pba_80_48 = PBA_STRATEGY_WEIGHTED
};

struct txgbe_dcb_num_tcs {
	u8 pg_tcs;
	u8 pfc_tcs;
};

struct txgbe_dcb_config {
	struct txgbe_dcb_tc_config tc_config[TXGBE_DCB_TC_MAX];
	struct txgbe_dcb_support support;
	struct txgbe_dcb_num_tcs num_tcs;
	u8 bw_percentage[TXGBE_DCB_BWG_MAX][2]; /* One each for Tx/Rx */
	bool pfc_mode_enable;
	bool round_robin_enable;

	enum txgbe_dcb_pba rx_pba_cfg;

	u32 link_speed; /* For bandwidth allocation validation purpose */
	bool vt_mode;
};

int txgbe_dcb_pfc_enable(struct txgbe_hw *hw, u8 tc_num);

/* DCB credits calculation */
s32 txgbe_dcb_calculate_tc_credits_cee(struct txgbe_hw *hw,
				   struct txgbe_dcb_config *dcb_config,
				   u32 max_frame_size, u8 direction);

/* DCB PFC */
s32 txgbe_dcb_config_pfc(struct txgbe_hw *hw, u8 pfc_en, u8 *map);

/* DCB unpack routines */
void txgbe_dcb_unpack_pfc_cee(struct txgbe_dcb_config *cfg,
			      u8 *map, u8 *pfc_up);
void txgbe_dcb_unpack_refill_cee(struct txgbe_dcb_config *cfg, int direction,
			      u16 *refill);
void txgbe_dcb_unpack_max_cee(struct txgbe_dcb_config *cfg, u16 *max);
void txgbe_dcb_unpack_bwgid_cee(struct txgbe_dcb_config *cfg, int direction,
			      u8 *bwgid);
void txgbe_dcb_unpack_tsa_cee(struct txgbe_dcb_config *cfg, int direction,
			      u8 *tsa);
void txgbe_dcb_unpack_map_cee(struct txgbe_dcb_config *cfg, int direction,
			      u8 *map);
u8 txgbe_dcb_get_tc_from_up(struct txgbe_dcb_config *cfg, int direction, u8 up);

#include "txgbe_dcb_hw.h"

#endif /* _TXGBE_DCB_H_ */
