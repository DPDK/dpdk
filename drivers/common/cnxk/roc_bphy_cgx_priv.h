/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BPHY_CGX_PRIV_H_
#define _ROC_BPHY_CGX_PRIV_H_

/* REQUEST ID types. Input to firmware */
enum eth_cmd_id {
	ETH_CMD_INTF_SHUTDOWN = 12,
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
#define SCR0_ETH_LNK_STS_S_ERR_TYPE    GENMASK_ULL(24, 15)

/* scratchx(1) CSR used for non-secure SW->ATF communication
 * This CSR acts as a command register
 */

/* struct eth_cmd */
#define SCR1_ETH_CMD_ID GENMASK_ULL(7, 2)

#define SCR1_OWN_STATUS GENMASK_ULL(1, 0)

#endif /* _ROC_BPHY_CGX_PRIV_H_ */
