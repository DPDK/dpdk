/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_BPHY_H_
#define _CNXK_BPHY_H_

#include <stdint.h>

#include <rte_common.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_rawdev.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#define CNXK_BPHY_DEF_QUEUE 0

typedef void (*cnxk_bphy_intr_handler_t)(int irq_num, void *isr_data);

struct cnxk_bphy_mem {
	struct rte_mem_resource res0;
	struct rte_mem_resource res2;
};

enum cnxk_bphy_irq_msg_type {
	CNXK_BPHY_IRQ_MSG_TYPE_INIT,
	CNXK_BPHY_IRQ_MSG_TYPE_FINI,
	CNXK_BPHY_IRQ_MSG_TYPE_REGISTER,
	CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER,
	CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET,
	CNXK_BPHY_MSG_TYPE_NPA_PF_FUNC,
	CNXK_BPHY_MSG_TYPE_SSO_PF_FUNC,
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
__rte_pmd_bphy_enq_deq(uint16_t dev_id, unsigned int queue, void *req,
		       void *rsp, size_t rsp_size)
{
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;
	void *q;
	int ret;

	q = (void *)(size_t)queue;
	buf.buf_addr = req;
	bufs[0] = &buf;

	ret = rte_rawdev_enqueue_buffers(dev_id, bufs, RTE_DIM(bufs), q);
	if (ret < 0)
		return ret;
	if (ret != RTE_DIM(bufs))
		return -EIO;

	if (!rsp)
		return 0;

	ret = rte_rawdev_dequeue_buffers(dev_id, bufs, RTE_DIM(bufs), q);
	if (ret < 0)
		return ret;
	if (ret != RTE_DIM(bufs))
		return -EIO;

	rte_memcpy(rsp, buf.buf_addr, rsp_size);
	rte_free(buf.buf_addr);

	return 0;
}

static __rte_always_inline int
rte_pmd_bphy_intr_init(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_INIT,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_intr_fini(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_FINI,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      NULL, 0);
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

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_intr_unregister(uint16_t dev_id, int irq_num)
{
	struct cnxk_bphy_irq_info info = {
		.irq_num = irq_num,
	};
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER,
		.data = &info
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_intr_mem_get(uint16_t dev_id, struct cnxk_bphy_mem *mem)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      mem, sizeof(*mem));
}

static __rte_always_inline int
rte_pmd_bphy_npa_pf_func_get(uint16_t dev_id, uint16_t *pf_func)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_MSG_TYPE_NPA_PF_FUNC,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      pf_func, sizeof(*pf_func));
}

static __rte_always_inline int
rte_pmd_bphy_sso_pf_func_get(uint16_t dev_id, uint16_t *pf_func)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_MSG_TYPE_SSO_PF_FUNC,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      pf_func, sizeof(*pf_func));
}

static __rte_always_inline int
rte_pmd_bphy_cgx_get_link_info(uint16_t dev_id, uint16_t lmac,
			       struct cnxk_bphy_cgx_msg_link_info *info)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, info, sizeof(*info));
}

static __rte_always_inline int
rte_pmd_bphy_cgx_intlbk_disable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_intlbk_enable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_ptp_rx_disable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_ptp_rx_enable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_set_link_mode(uint16_t dev_id, uint16_t lmac,
			       struct cnxk_bphy_cgx_msg_link_mode *mode)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_MODE,
		.data = mode,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_set_link_state(uint16_t dev_id, uint16_t lmac, bool up)
{
	struct cnxk_bphy_cgx_msg_set_link_state state = {
		.state = up,
	};
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE,
		.data = &state,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_start_rxtx(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_START_RXTX,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_stop_rxtx(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

static __rte_always_inline int
rte_pmd_bphy_cgx_get_supported_fec(uint16_t dev_id, uint16_t lmac,
				   enum cnxk_bphy_cgx_eth_link_fec *fec)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_GET_SUPPORTED_FEC,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, fec, sizeof(*fec));
}

static __rte_always_inline int
rte_pmd_bphy_cgx_set_fec(uint16_t dev_id, uint16_t lmac,
			 enum cnxk_bphy_cgx_eth_link_fec fec)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_SET_FEC,
		.data = &fec,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* _CNXK_BPHY_H_ */
