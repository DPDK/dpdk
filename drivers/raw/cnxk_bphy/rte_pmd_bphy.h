/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_BPHY_H_
#define _CNXK_BPHY_H_

#include "cnxk_bphy_irq.h"

enum cnxk_bphy_cgx_msg_type {
	CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO,
	CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE,
	CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE,
	CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE,
	CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE,
	CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_MODE,
	CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE,
	CNXK_BPHY_CGX_MSG_TYPE_START_RXTX,
	CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX,
	CNXK_BPHY_CGX_MSG_TYPE_GET_SUPPORTED_FEC,
	CNXK_BPHY_CGX_MSG_TYPE_SET_FEC,
};

enum cnxk_bphy_cgx_eth_link_speed {
	CNXK_BPHY_CGX_ETH_LINK_SPEED_NONE,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_10M,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_100M,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_1G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_2HG,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_5G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_10G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_20G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_25G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_40G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_50G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_80G,
	CNXK_BPHY_CGX_ETH_LINK_SPEED_100G,
	__CNXK_BPHY_CGX_ETH_LINK_SPEED_MAX
};

enum cnxk_bphy_cgx_eth_link_fec {
	CNXK_BPHY_CGX_ETH_LINK_FEC_NONE,
	CNXK_BPHY_CGX_ETH_LINK_FEC_BASE_R,
	CNXK_BPHY_CGX_ETH_LINK_FEC_RS,
	__CNXK_BPHY_CGX_ETH_LINK_FEC_MAX
};

enum cnxk_bphy_cgx_eth_link_mode {
	CNXK_BPHY_CGX_ETH_LINK_MODE_SGMII_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_1000_BASEX_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_QSGMII_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_10G_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_10G_C2M_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_10G_KR_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_20G_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_C2M_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_2_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_CR_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_KR_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_C2M_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_CR4_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_KR4_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_40GAUI_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_C2M_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_4_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_CR_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_KR_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_80GAUI_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_C2C_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_C2M_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_CR4_BIT,
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_KR4_BIT,
	__CNXK_BPHY_CGX_ETH_LINK_MODE_MAX
};

struct cnxk_bphy_cgx_msg_link_mode {
	bool full_duplex;
	bool autoneg;
	enum cnxk_bphy_cgx_eth_link_speed speed;
	enum cnxk_bphy_cgx_eth_link_mode mode;
};

struct cnxk_bphy_cgx_msg_link_info {
	bool link_up;
	bool full_duplex;
	enum cnxk_bphy_cgx_eth_link_speed speed;
	bool autoneg;
	enum cnxk_bphy_cgx_eth_link_fec fec;
	enum cnxk_bphy_cgx_eth_link_mode mode;
};

struct cnxk_bphy_cgx_msg_set_link_state {
	bool state; /* up or down */
};

struct cnxk_bphy_cgx_msg {
	enum cnxk_bphy_cgx_msg_type type;
	/*
	 * data depends on message type and whether
	 * it's a request or a response
	 */
	void *data;
};

#define cnxk_bphy_mem	    bphy_mem
#define CNXK_BPHY_DEF_QUEUE 0

enum cnxk_bphy_irq_msg_type {
	CNXK_BPHY_IRQ_MSG_TYPE_INIT,
	CNXK_BPHY_IRQ_MSG_TYPE_FINI,
	CNXK_BPHY_IRQ_MSG_TYPE_REGISTER,
	CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER,
	CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET,
};

struct cnxk_bphy_irq_msg {
	enum cnxk_bphy_irq_msg_type type;
	/*
	 * The data field, depending on message type, may point to
	 * - (enq) full struct cnxk_bphy_irq_info for registration request
	 * - (enq) struct cnxk_bphy_irq_info with irq_num set for unregistration
	 * - (deq) struct cnxk_bphy_mem for memory range request response
	 * - (xxx) NULL
	 */
	void *data;
};

struct cnxk_bphy_irq_info {
	int irq_num;
	cnxk_bphy_intr_handler_t handler;
	void *data;
	int cpu;
};

static __rte_always_inline int
rte_pmd_bphy_intr_init(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_INIT,
	};
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;

	buf.buf_addr = &msg;
	bufs[0] = &buf;

	return rte_rawdev_enqueue_buffers(dev_id, bufs, 1, CNXK_BPHY_DEF_QUEUE);
}

static __rte_always_inline void
rte_pmd_bphy_intr_fini(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_FINI,
	};
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;

	buf.buf_addr = &msg;
	bufs[0] = &buf;

	rte_rawdev_enqueue_buffers(dev_id, bufs, 1, CNXK_BPHY_DEF_QUEUE);
}

static __rte_always_inline int
rte_pmd_bphy_intr_register(uint16_t dev_id, int irq_num,
			   cnxk_bphy_intr_handler_t handler, void *data,
			   int cpu)
{
	struct cnxk_bphy_irq_info info = {
		.irq_num = irq_num,
		.handler = handler,
		.data = data,
		.cpu = cpu,
	};
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_REGISTER,
		.data = &info
	};
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;

	buf.buf_addr = &msg;
	bufs[0] = &buf;

	return rte_rawdev_enqueue_buffers(dev_id, bufs, 1, CNXK_BPHY_DEF_QUEUE);
}

static __rte_always_inline void
rte_pmd_bphy_intr_unregister(uint16_t dev_id, int irq_num)
{
	struct cnxk_bphy_irq_info info = {
		.irq_num = irq_num,
	};
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER,
		.data = &info
	};
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;

	buf.buf_addr = &msg;
	bufs[0] = &buf;

	rte_rawdev_enqueue_buffers(dev_id, bufs, 1, 0);
}

static __rte_always_inline struct cnxk_bphy_mem *
rte_pmd_bphy_intr_mem_get(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET,
	};
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;
	int ret;

	buf.buf_addr = &msg;
	bufs[0] = &buf;

	ret = rte_rawdev_enqueue_buffers(dev_id, bufs, 1, CNXK_BPHY_DEF_QUEUE);
	if (ret)
		return NULL;

	ret = rte_rawdev_dequeue_buffers(dev_id, bufs, 1, CNXK_BPHY_DEF_QUEUE);
	if (ret)
		return NULL;

	return buf.buf_addr;
}

#endif /* _CNXK_BPHY_H_ */
