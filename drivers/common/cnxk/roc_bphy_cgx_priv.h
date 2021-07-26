/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BPHY_CGX_PRIV_H_
#define _ROC_BPHY_CGX_PRIV_H_

/* LINK speed types */
enum eth_link_speed {
	ETH_LINK_NONE,
	ETH_LINK_10M,
	ETH_LINK_100M,
	ETH_LINK_1G,
	ETH_LINK_2HG, /* 2.5 Gbps */
	ETH_LINK_5G,
	ETH_LINK_10G,
	ETH_LINK_20G,
	ETH_LINK_25G,
	ETH_LINK_40G,
	ETH_LINK_50G,
	ETH_LINK_80G,
	ETH_LINK_100G,
	ETH_LINK_MAX,
};

/* Supported LINK MODE enums
 * Each link mode is a bit mask of these
 * enums which are represented as bits
 */
enum eth_mode {
	ETH_MODE_SGMII_BIT = 0,
	ETH_MODE_1000_BASEX_BIT,
	ETH_MODE_QSGMII_BIT,
	ETH_MODE_10G_C2C_BIT,
	ETH_MODE_10G_C2M_BIT,
	ETH_MODE_10G_KR_BIT, /* = 5 */
	ETH_MODE_20G_C2C_BIT,
	ETH_MODE_25G_C2C_BIT,
	ETH_MODE_25G_C2M_BIT,
	ETH_MODE_25G_2_C2C_BIT,
	ETH_MODE_25G_CR_BIT, /* = 10 */
	ETH_MODE_25G_KR_BIT,
	ETH_MODE_40G_C2C_BIT,
	ETH_MODE_40G_C2M_BIT,
	ETH_MODE_40G_CR4_BIT,
	ETH_MODE_40G_KR4_BIT, /* = 15 */
	ETH_MODE_40GAUI_C2C_BIT,
	ETH_MODE_50G_C2C_BIT,
	ETH_MODE_50G_C2M_BIT,
	ETH_MODE_50G_4_C2C_BIT,
	ETH_MODE_50G_CR_BIT, /* = 20 */
	ETH_MODE_50G_KR_BIT,
	ETH_MODE_80GAUI_C2C_BIT,
	ETH_MODE_100G_C2C_BIT,
	ETH_MODE_100G_C2M_BIT,
	ETH_MODE_100G_CR4_BIT, /* = 25 */
	ETH_MODE_100G_KR4_BIT,
	ETH_MODE_MAX_BIT /* = 27 */
};

/* REQUEST ID types. Input to firmware */
enum eth_cmd_id {
	ETH_CMD_GET_LINK_STS = 4,
	ETH_CMD_LINK_BRING_UP = 5,
	ETH_CMD_LINK_BRING_DOWN = 6,
	ETH_CMD_INTERNAL_LBK = 7,
	ETH_CMD_MODE_CHANGE = 11, /* hot plug support */
	ETH_CMD_INTF_SHUTDOWN = 12,
	ETH_CMD_GET_SUPPORTED_FEC = 18,
	ETH_CMD_SET_FEC = 19,
	ETH_CMD_SET_PTP_MODE = 34,
};

/* event types - cause of interrupt */
enum eth_evt_type {
	ETH_EVT_ASYNC,
	ETH_EVT_CMD_RESP,
};

enum eth_stat {
	ETH_STAT_SUCCESS,
	ETH_STAT_FAIL,
};

enum eth_cmd_own {
	/* default ownership with kernel/uefi/u-boot */
	ETH_OWN_NON_SECURE_SW,
	/* set by kernel/uefi/u-boot after posting a new request to ATF */
	ETH_OWN_FIRMWARE,
};

/* scratchx(0) CSR used for ATF->non-secure SW communication.
 * This acts as the status register
 * Provides details on command ack/status, link status, error details
 */

/* struct eth_evt_sts_s */
#define SCR0_ETH_EVT_STS_S_ACK	    BIT_ULL(0)
#define SCR0_ETH_EVT_STS_S_EVT_TYPE BIT_ULL(1)
#define SCR0_ETH_EVT_STS_S_STAT	    BIT_ULL(2)
#define SCR0_ETH_EVT_STS_S_ID	    GENMASK_ULL(8, 3)

/* struct eth_lnk_sts_s */
#define SCR0_ETH_LNK_STS_S_LINK_UP     BIT_ULL(9)
#define SCR0_ETH_LNK_STS_S_FULL_DUPLEX BIT_ULL(10)
#define SCR0_ETH_LNK_STS_S_SPEED       GENMASK_ULL(14, 11)
#define SCR0_ETH_LNK_STS_S_ERR_TYPE    GENMASK_ULL(24, 15)
#define SCR0_ETH_LNK_STS_S_AN	       BIT_ULL(25)
#define SCR0_ETH_LNK_STS_S_FEC	       GENMASK_ULL(27, 26)
#define SCR0_ETH_LNK_STS_S_LMAC_TYPE   GENMASK_ULL(35, 28)
#define SCR0_ETH_LNK_STS_S_MODE	       GENMASK_ULL(43, 36)

/* struct eth_fec_types_s */
#define SCR0_ETH_FEC_TYPES_S_FEC GENMASK_ULL(10, 9)

/* scratchx(1) CSR used for non-secure SW->ATF communication
 * This CSR acts as a command register
 */

/* struct eth_cmd */
#define SCR1_ETH_CMD_ID GENMASK_ULL(7, 2)

/* struct eth_ctl_args */
#define SCR1_ETH_CTL_ARGS_ENABLE BIT_ULL(8)

/* struct eth_mode_change_args */
#define SCR1_ETH_MODE_CHANGE_ARGS_SPEED	 GENMASK_ULL(11, 8)
#define SCR1_ETH_MODE_CHANGE_ARGS_DUPLEX BIT_ULL(12)
#define SCR1_ETH_MODE_CHANGE_ARGS_AN	 BIT_ULL(13)
#define SCR1_ETH_MODE_CHANGE_ARGS_PORT	 GENMASK_ULL(21, 14)
#define SCR1_ETH_MODE_CHANGE_ARGS_MODE	 GENMASK_ULL(63, 22)

/* struct eth_set_fec_args */
#define SCR1_ETH_SET_FEC_ARGS GENMASK_ULL(9, 8)

#define SCR1_OWN_STATUS GENMASK_ULL(1, 0)

#endif /* _ROC_BPHY_CGX_PRIV_H_ */
