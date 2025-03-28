/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_DMA_REGS_H_
#define _RNP_DMA_REGS_H_

#define RNP_DMA_VERSION		(0)
#define RNP_DMA_HW_EN		(0x10)
#define RNP_DMA_EN_ALL		(0b1111)
#define RNP_DMA_HW_STATE	(0x14)
/* --- queue register --- */
/* queue enable */
#define RNP_RXQ_START(qid)	_RING_(0x0010 + 0x100 * (qid))
#define RNP_RXQ_READY(qid)	_RING_(0x0014 + 0x100 * (qid))
#define RNP_TXQ_START(qid)	_RING_(0x0018 + 0x100 * (qid))
#define RNP_TXQ_READY(qid)	_RING_(0x001c + 0x100 * (qid))
/* queue irq generate ctrl */
#define RNP_RXTX_IRQ_STAT(qid)	_RING_(0x0020 + 0x100 * (qid))
#define RNP_RXTX_IRQ_MASK(qid)	_RING_(0x0024 + 0x100 * (qid))
#define RNP_TX_IRQ_MASK		RTE_BIT32(1)
#define RNP_RX_IRQ_MASK		RTE_BIT32(0)
#define RNP_RXTX_IRQ_MASK_ALL	(RNP_RX_IRQ_MASK | RNP_TX_IRQ_MASK)
#define RNP_RXTX_IRQ_CLER(qid)	_RING_(0x0028 + 0x100 * (qid))
/* rx-queue setup */
#define RNP_RXQ_BASE_ADDR_HI(qid)	_RING_(0x0030 + 0x100 * (qid))
#define RNP_RXQ_BASE_ADDR_LO(qid)	_RING_(0x0034 + 0x100 * (qid))
#define RNP_RXQ_LEN(qid)		_RING_(0x0038 + 0x100 * (qid))
#define RNP_RXQ_HEAD(qid)		_RING_(0x003c + 0x100 * (qid))
#define RNP_RXQ_TAIL(qid)		_RING_(0x0040 + 0x100 * (qid))
#define RNP_RXQ_DESC_FETCH_CTRL(qid)	_RING_(0x0044 + 0x100 * (qid))
/* rx queue interrupt generate param */
#define RNP_RXQ_INT_DELAY_TIMER(qid)	_RING_(0x0048 + 0x100 * (qid))
#define RNP_RXQ_INT_DELAY_PKTCNT(qid)	_RING_(0x004c + 0x100 * (qid))
#define RNP_RXQ_RX_PRI_LVL(qid)		_RING_(0x0050 + 0x100 * (qid))
#define RNP_RXQ_DROP_TIMEOUT_TH(qid)	_RING_(0x0054 + 0x100 * (qid))
/* tx queue setup */
#define RNP_TXQ_BASE_ADDR_HI(qid)        _RING_(0x0060 + 0x100 * (qid))
#define RNP_TXQ_BASE_ADDR_LO(qid)        _RING_(0x0064 + 0x100 * (qid))
#define RNP_TXQ_LEN(qid)                 _RING_(0x0068 + 0x100 * (qid))
#define RNP_TXQ_HEAD(qid)                _RING_(0x006c + 0x100 * (qid))
#define RNP_TXQ_TAIL(qid)                _RING_(0x0070 + 0x100 * (qid))
#define RNP_TXQ_DESC_FETCH_CTRL(qid)     _RING_(0x0074 + 0x100 * (qid))
#define RNQ_DESC_FETCH_BURST_S		(16)
/* tx queue interrupt generate param */
#define RNP_TXQ_INT_DELAY_TIMER(qid)     _RING_(0x0078 + 0x100 * (qid))
#define RNP_TXQ_INT_DELAY_PKTCNT(qid)    _RING_(0x007c + 0x100 * (qid))
/* veb ctrl register */
#define RNP_VEB_MAC_LO(p, n)	_RING_(0x00a0 + (4 * (p)) + (0x100 * (n)))
#define RNP_VEB_MAC_HI(p, n)	_RING_(0x00b0 + (4 * (p)) + (0x100 * (n)))
#define RNP_VEB_VID_CFG(p, n)	_RING_(0x00c0 + (4 * (p)) + (0x100 * (n)))
#define RNP_VEB_VF_RING(p, n)	_RING_(0x00d0 + (4 * (p)) + (0x100 * (n)))
#define RNP_MAX_VEB_TB		(64)
#define RNP_VEB_RING_CFG_S	(8)
#define RNP_VEB_SWITCH_VF_EN	RTE_BIT32(7)
#define MAX_VEB_TABLES_NUM	(4)

#endif /* _RNP_DMA_REGS_H_ */
