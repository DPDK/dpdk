/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _HIF_H_
#define _HIF_H_

/* @file hif.h.
 * hif - PFE hif block control and status register.
 * Mapped on CBUS and accessible from all PE's and ARM.
 */
#define HIF_VERSION	(HIF_BASE_ADDR + 0x00)
#define HIF_TX_CTRL	(HIF_BASE_ADDR + 0x04)
#define HIF_TX_CURR_BD_ADDR	(HIF_BASE_ADDR + 0x08)
#define HIF_TX_ALLOC	(HIF_BASE_ADDR + 0x0c)
#define HIF_TX_BDP_ADDR	(HIF_BASE_ADDR + 0x10)
#define HIF_TX_STATUS	(HIF_BASE_ADDR + 0x14)
#define HIF_RX_CTRL	(HIF_BASE_ADDR + 0x20)
#define HIF_RX_BDP_ADDR	(HIF_BASE_ADDR + 0x24)
#define HIF_RX_STATUS	(HIF_BASE_ADDR + 0x30)
#define HIF_INT_SRC	(HIF_BASE_ADDR + 0x34)
#define HIF_INT_ENABLE	(HIF_BASE_ADDR + 0x38)
#define HIF_POLL_CTRL	(HIF_BASE_ADDR + 0x3c)
#define HIF_RX_CURR_BD_ADDR	(HIF_BASE_ADDR + 0x40)
#define HIF_RX_ALLOC	(HIF_BASE_ADDR + 0x44)
#define HIF_TX_DMA_STATUS	(HIF_BASE_ADDR + 0x48)
#define HIF_RX_DMA_STATUS	(HIF_BASE_ADDR + 0x4c)
#define HIF_INT_COAL	(HIF_BASE_ADDR + 0x50)

/* HIF_INT_SRC/ HIF_INT_ENABLE control bits */
#define HIF_INT		BIT(0)
#define HIF_RXBD_INT	BIT(1)
#define HIF_RXPKT_INT	BIT(2)
#define HIF_TXBD_INT	BIT(3)
#define HIF_TXPKT_INT	BIT(4)

/* HIF_TX_CTRL bits */
#define HIF_CTRL_DMA_EN			BIT(0)
#define HIF_CTRL_BDP_POLL_CTRL_EN	BIT(1)
#define HIF_CTRL_BDP_CH_START_WSTB	BIT(2)

/* HIF_RX_STATUS bits */
#define BDP_CSR_RX_DMA_ACTV     BIT(16)

/* HIF_INT_ENABLE bits */
#define HIF_INT_EN		BIT(0)
#define HIF_RXBD_INT_EN		BIT(1)
#define HIF_RXPKT_INT_EN	BIT(2)
#define HIF_TXBD_INT_EN		BIT(3)
#define HIF_TXPKT_INT_EN	BIT(4)

/* HIF_POLL_CTRL bits*/
#define HIF_RX_POLL_CTRL_CYCLE	0x0400
#define HIF_TX_POLL_CTRL_CYCLE	0x0400

/* HIF_INT_COAL bits*/
#define HIF_INT_COAL_ENABLE	BIT(31)

/* Buffer descriptor control bits */
#define BD_CTRL_BUFLEN_MASK	0x3fff
#define BD_BUF_LEN(x)	((x) & BD_CTRL_BUFLEN_MASK)
#define BD_CTRL_CBD_INT_EN	BIT(16)
#define BD_CTRL_PKT_INT_EN	BIT(17)
#define BD_CTRL_LIFM		BIT(18)
#define BD_CTRL_LAST_BD		BIT(19)
#define BD_CTRL_DIR		BIT(20)
#define BD_CTRL_LMEM_CPY	BIT(21) /* Valid only for HIF_NOCPY */
#define BD_CTRL_PKT_XFER	BIT(24)
#define BD_CTRL_DESC_EN		BIT(31)
#define BD_CTRL_PARSE_DISABLE	BIT(25)
#define BD_CTRL_BRFETCH_DISABLE	BIT(26)
#define BD_CTRL_RTFETCH_DISABLE	BIT(27)

/* Buffer descriptor status bits*/
#define BD_STATUS_CONN_ID(x)	((x) & 0xffff)
#define BD_STATUS_DIR_PROC_ID	BIT(16)
#define BD_STATUS_CONN_ID_EN	BIT(17)
#define BD_STATUS_PE2PROC_ID(x)	(((x) & 7) << 18)
#define BD_STATUS_LE_DATA	BIT(21)
#define BD_STATUS_CHKSUM_EN	BIT(22)

/* HIF Buffer descriptor status bits */
#define DIR_PROC_ID	BIT(16)
#define PROC_ID(id)	((id) << 18)

#endif /* _HIF_H_ */
