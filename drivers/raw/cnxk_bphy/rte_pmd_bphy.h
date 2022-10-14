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

/**
 * @file rte_pmd_bphy.h
 *
 * Marvell CGX and BPHY PMD specific structures and interface
 *
 * This API allows applications to manage BPHY memory in user space along with
 * installing interrupt handlers for low latency signal processing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Available message types */
enum cnxk_bphy_cgx_msg_type {
	/** Type used to obtain link information */
	CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO,
	/** Type used to disable internal loopback */
	CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE,
	/** Type used to enable loopback */
	CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE,
	/** Type used to disable PTP on RX */
	CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE,
	/** Type used to enable PTP on RX */
	CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE,
	/** Type used to set link mode */
	CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_MODE,
	/** Type used to set link state */
	CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE,
	/** Type used to start transmission and packet reception */
	CNXK_BPHY_CGX_MSG_TYPE_START_RXTX,
	/** Type used to stop transmission and packet reception */
	CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX,
	/** Type used to obtain supported FEC */
	CNXK_BPHY_CGX_MSG_TYPE_GET_SUPPORTED_FEC,
	/** Type used to set FEC */
	CNXK_BPHY_CGX_MSG_TYPE_SET_FEC,
	/** Type used to switch from eCPRI to CPRI */
	CNXK_BPHY_CGX_MSG_TYPE_CPRI_MODE_CHANGE,
	/** Type used to enable TX for CPRI SERDES */
	CNXK_BPHY_CGX_MSG_TYPE_CPRI_TX_CONTROL,
	/** Type use to change misc CPRI SERDES settings */
	CNXK_BPHY_CGX_MSG_TYPE_CPRI_MODE_MISC,
};

/** Available link speeds */
enum cnxk_bphy_cgx_eth_link_speed {
	CNXK_BPHY_CGX_ETH_LINK_SPEED_NONE, /**<  None */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_10M,  /**<  10 Mbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_100M, /**< 100 Mbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_1G,   /**<   1 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_2HG,  /**< 2.5 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_5G,   /**<   5 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_10G,  /**<  10 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_20G,  /**<  20 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_25G,  /**<  25 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_40G,  /**<  40 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_50G,  /**<  50 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_80G,  /**<  80 Gbps */
	CNXK_BPHY_CGX_ETH_LINK_SPEED_100G, /**< 100 Gbps */
	__CNXK_BPHY_CGX_ETH_LINK_SPEED_MAX
};

/** Available FEC modes */
enum cnxk_bphy_cgx_eth_link_fec {
	/** Disable FEC */
	CNXK_BPHY_CGX_ETH_LINK_FEC_NONE,
	/** Base FEC (IEEE 802.3 CLause 74) */
	CNXK_BPHY_CGX_ETH_LINK_FEC_BASE_R,
	/** Reed-Solomon FEC */
	CNXK_BPHY_CGX_ETH_LINK_FEC_RS,
	__CNXK_BPHY_CGX_ETH_LINK_FEC_MAX
};

/** Available link modes */
enum cnxk_bphy_cgx_eth_link_mode {
	/** SGMII */
	CNXK_BPHY_CGX_ETH_LINK_MODE_SGMII_BIT,
	/** 1000BASE-X */
	CNXK_BPHY_CGX_ETH_LINK_MODE_1000_BASEX_BIT,
	/** QSGMII */
	CNXK_BPHY_CGX_ETH_LINK_MODE_QSGMII_BIT,
	/** 10GBASE-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_10G_C2C_BIT,
	/** 10GBASE-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_10G_C2M_BIT,
	/** 10GBASE-KR */
	CNXK_BPHY_CGX_ETH_LINK_MODE_10G_KR_BIT,
	/** 20GBASE-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_20G_C2C_BIT,
	/** 25GBASE-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_C2C_BIT,
	/** 25GBASE-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_C2M_BIT,
	/** 25GBASE-2-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_2_C2C_BIT,
	/** 25GBASE-CR */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_CR_BIT,
	/** 25GBASE-KR */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25G_KR_BIT,
	/** 40GBASE-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_C2C_BIT,
	/** 40GBASE-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_C2M_BIT,
	/** 40GBASE-CR4 */
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_CR4_BIT,
	/** 40GBASE-KR4 */
	CNXK_BPHY_CGX_ETH_LINK_MODE_40G_KR4_BIT,
	/** 40GAUI-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_40GAUI_C2C_BIT,
	/** 50GBASE-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_C2C_BIT,
	/** 50GBASE-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_C2M_BIT,
	/** 50GBASE-4-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_4_C2C_BIT,
	/** 50GBASE-CR */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_CR_BIT,
	/** 50GBASE-KR */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50G_KR_BIT,
	/** 80GAUI-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_80GAUI_C2C_BIT,
	/** 100GBASE-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_C2C_BIT,
	/** 100GBASE-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_C2M_BIT,
	/** 100GBASE-CR4 */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_CR4_BIT,
	/** 100GBASE-KR4 */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100G_KR4_BIT,
	/** 50GAUI-2-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50GAUI_2_C2C_BIT,
	/** 50GAUI-2-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50GAUI_2_C2M_BIT,
	/** 50GBASE-CR2-C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50GBASE_CR2_C_BIT,
	/** 50GBASE-KR2-C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_50GBASE_KR2_C_BIT,
	/** 100GAUI-2-C2C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100GAUI_2_C2C_BIT,
	/** 100GAUI-2-C2M */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100GAUI_2_C2M_BIT,
	/** 100GBASE-CR2 */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100GBASE_CR2_BIT,
	/** 100GBASE-KR2 */
	CNXK_BPHY_CGX_ETH_LINK_MODE_100GBASE_KR2_BIT,
	/** SFI-1G */
	CNXK_BPHY_CGX_ETH_LINK_MODE_SFI_1G_BIT,
	/** 25GBASE-CR-C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25GBASE_CR_C_BIT,
	/** 25GBASE-KR-C */
	CNXK_BPHY_CGX_ETH_LINK_MODE_25GBASE_KR_C_BIT,
	__CNXK_BPHY_CGX_ETH_LINK_MODE_MAX
};

enum cnxk_bphy_cgx_eth_mode_cpri {
	/** 2.4G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_2_4G_BIT,
	/** 3.1G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_3_1G_BIT,
	/** 4.9G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_4_9G_BIT,
	/** 6.1G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_6_1G_BIT,
	/** 9.8G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_9_8G_BIT,
	/** 10.1G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_10_1_BIT,
	/** 24.3G Lane Rate */
	CNXK_BPHY_CGX_ETH_MODE_CPRI_24_3G_BIT,
};

enum cnxk_bphy_cgx_mode_group {
	/** ETH group */
	CNXK_BPHY_CGX_MODE_GROUP_ETH,
	/** CPRI group */
	CNXK_BPHY_CGX_MODE_GROUP_CPRI = 2,
};

struct cnxk_bphy_cgx_msg_link_mode {
	/** Setting for full-duplex */
	bool full_duplex;
	/** Setting for automatic link negotiation */
	bool autoneg;
	/** Set to true to use port index */
	bool use_portm_idx;
	/** Port index */
	unsigned int portm_idx;
	/** Mode group */
	enum cnxk_bphy_cgx_mode_group mode_group_idx;
	/** Link speed */
	enum cnxk_bphy_cgx_eth_link_speed speed;
	union {
		/** Link mode */
		enum cnxk_bphy_cgx_eth_link_mode mode;
		/** CPRI mode */
		enum cnxk_bphy_cgx_eth_mode_cpri mode_cpri;
	};
};

struct cnxk_bphy_cgx_msg_link_info {
	/** Link state information */
	bool link_up;
	/** Link full duplex state */
	bool full_duplex;
	/** Link speed */
	enum cnxk_bphy_cgx_eth_link_speed speed;
	/** Link auto-negotiation setting */
	bool autoneg;
	/** FEC mode */
	enum cnxk_bphy_cgx_eth_link_fec fec;
	/** Link configuration */
	enum cnxk_bphy_cgx_eth_link_mode mode;
};

struct cnxk_bphy_cgx_msg_set_link_state {
	/** Defines link state result */
	bool state; /* up or down */
};

struct cnxk_bphy_cgx_msg_cpri_mode_change {
	/** SERDES index (0 - 4) */
	int gserc_idx;
	/** Lane index (0 - 1) */
	int lane_idx;
	/** Baud rate (9830/4915/2458/6144/3072) */
	int rate;
	/** Disable LEQ */
	bool disable_leq;
	/** Disable  DFE */
	bool disable_dfe;
};

struct cnxk_bphy_cgx_msg_cpri_mode_tx_ctrl {
	/** SERDES index (0 - 4) */
	int gserc_idx;
	/** Lane index (0 - 1) */
	int lane_idx;
	/** Disable or enable SERDES */
	bool enable;
};

struct cnxk_bphy_cgx_msg_cpri_mode_misc {
	/** SERDES index (0 - 4) */
	int gserc_idx;
	/** Lane index (0 - 1) */
	int lane_idx;
	/** Misc flags (0 - RX Eq, 1 - RX state machine reset) */
	int flags;
};

struct cnxk_bphy_cgx_msg {
	/** Message type */
	enum cnxk_bphy_cgx_msg_type type;
	/**
	 * Data depends on message type and whether
	 * it's a request or a response
	 */
	void *data;
};

#define CNXK_BPHY_DEF_QUEUE 0

/**
 * BPHY interrupt handler
 *
 * @param irq_num
 *   Zero-based interrupt number
 * @param isr_data
 *   Cookie passed to interrupt handler
 */
typedef void (*cnxk_bphy_intr_handler_t)(int irq_num, void *isr_data);

struct cnxk_bphy_mem {
	/** Memory for BAR0 */
	struct rte_mem_resource res0;
	/** Memory for BAR2 */
	struct rte_mem_resource res2;
};

/** Available IRQ configuration commands */
enum cnxk_bphy_irq_msg_type {
	/** Type used to initialize interrupts */
	CNXK_BPHY_IRQ_MSG_TYPE_INIT,
	/** Type used to deinitialize interrupts */
	CNXK_BPHY_IRQ_MSG_TYPE_FINI,
	/** Type used to register interrupt */
	CNXK_BPHY_IRQ_MSG_TYPE_REGISTER,
	/** Type used to unregister interrupt */
	CNXK_BPHY_IRQ_MSG_TYPE_UNREGISTER,
	/** Type used to retrieve BPHY memory */
	CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET,
	/** Type used to retrieve NPA PF function */
	CNXK_BPHY_MSG_TYPE_NPA_PF_FUNC,
	/** Type used to retrieve NPA SSO function */
	CNXK_BPHY_MSG_TYPE_SSO_PF_FUNC,
};

struct cnxk_bphy_irq_msg {
	/** Message command type */
	enum cnxk_bphy_irq_msg_type type;
	/**
	 * Data depends on message type and whether
	 * it is a request or a response
	 */
	void *data;
};

struct cnxk_bphy_irq_info {
	/** Interrupt number */
	int irq_num;
	/** Interrupt handler */
	cnxk_bphy_intr_handler_t handler;
	/** Interrupt handler cookie */
	void *data;
	/** CPU zero-based number for interrupt execution */
	int cpu;
};

/** @internal helper routine for enqueuing/dequeuing messages */
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

/**
 * Initialize BPHY subsystem
 *
 * @param dev_id
 *   The identifier of the device
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_intr_init(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_INIT,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      NULL, 0);
}

/**
 * Deinitialize BPHY subsystem
 *
 * @param dev_id
 *   The identifier of the device
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_intr_fini(uint16_t dev_id)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_FINI,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      NULL, 0);
}

/**
 * Register BPHY interrupt handler
 *
 * @param dev_id
 *   The identifier of the device
 * @param irq_num
 *   Zero-based interrupt number
 * @param handler
 *   Interrupt handler to be executed
 * @param data
 *   Data to be passed to interrupt handler
 * @param cpu
 *   CPU number which will be handling interrupt
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
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

/**
 * Unregister BPHY interrupt handler
 *
 * @param dev_id
 *   The identifier of the device
 * @param irq_num
 *   Zero-based interrupt number used during registration
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
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

/**
 * Obtain BPHY memory
 *
 * @param dev_id
 *   The identifier of the device
 * @param mem
 *   Memory structure which will be filled for memory access
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_intr_mem_get(uint16_t dev_id, struct cnxk_bphy_mem *mem)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_IRQ_MSG_TYPE_MEM_GET,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      mem, sizeof(*mem));
}

/**
 * Obtain NPA PF func
 *
 * @param dev_id
 *   The identifier of the device
 * @param pf_func
 *   NPA PF function to obtain
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_npa_pf_func_get(uint16_t dev_id, uint16_t *pf_func)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_MSG_TYPE_NPA_PF_FUNC,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      pf_func, sizeof(*pf_func));
}

/**
 * Obtain SSO PF func
 *
 * @param dev_id
 *   The identifier of the device
 * @param pf_func
 *   SSO PF function to obtain
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_sso_pf_func_get(uint16_t dev_id, uint16_t *pf_func)
{
	struct cnxk_bphy_irq_msg msg = {
		.type = CNXK_BPHY_MSG_TYPE_SSO_PF_FUNC,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, CNXK_BPHY_DEF_QUEUE, &msg,
				      pf_func, sizeof(*pf_func));
}

/**
 * Obtain link information
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param info
 *   Link information structure
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_get_link_info(uint16_t dev_id, uint16_t lmac,
			       struct cnxk_bphy_cgx_msg_link_info *info)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, info, sizeof(*info));
}

/**
 * Disable loopback mode for an interface
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_intlbk_disable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Enable loopback mode for an interface
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_intlbk_enable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Disable PTP on RX path
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_ptp_rx_disable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Enable PTP on RX path
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_ptp_rx_enable(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Set link mode for a CGX
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param mode
 *   Link mode to set
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
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

/**
 * Set link state for a CGX
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param up
 *   Link state to set
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
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

/**
 * Start CGX
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_start_rxtx(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_START_RXTX,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Stop CGX
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_stop_rxtx(uint16_t dev_id, uint16_t lmac)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Get supported list FEC mode
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param fec
 *   FEC structure which holds information
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_get_supported_fec(uint16_t dev_id, uint16_t lmac,
				   enum cnxk_bphy_cgx_eth_link_fec *fec)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_GET_SUPPORTED_FEC,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, fec, sizeof(*fec));
}

/**
 * Set FEC mode for a device
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param fec
 *   FEC structure which holds information to set
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
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

/**
 * Switch from eCPRI to CPRI and change
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param mode
 *   CPRI structure which holds configuration data
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_cpri_mode_change(uint16_t dev_id, uint16_t lmac,
				  struct cnxk_bphy_cgx_msg_cpri_mode_change *mode)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_CPRI_MODE_CHANGE,
		.data = mode,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Enable TX for SERDES configured in CPRI mode
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param mode
 *   CPRI TX control structure holding control data
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_cpri_tx_control(uint16_t dev_id, uint16_t lmac,
				 struct cnxk_bphy_cgx_msg_cpri_mode_tx_ctrl *mode)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_CPRI_TX_CONTROL,
		.data = mode,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * CPRI misc settings
 *
 * @param dev_id
 *   The identifier of the device
 * @param lmac
 *   LMAC number for operation
 * @param mode
 *   CPRI settings holding misc control data
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_bphy_cgx_cpri_mode_misc(uint16_t dev_id, uint16_t lmac,
				struct cnxk_bphy_cgx_msg_cpri_mode_misc *mode)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_CPRI_MODE_MISC,
		.data = mode,
	};

	return __rte_pmd_bphy_enq_deq(dev_id, lmac, &msg, NULL, 0);
}

/**
 * Obtain NPA PF func
 *
 * @param pf_func
 *   Address of an NPA PF and function for NPA free pointer
 *   requests to obtain
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
int rte_pmd_bphy_npa_pf_func_get_rmt(uint16_t *pf_func);

/**
 * Obtain SSO PF func
 *
 * @param pf_func
 *   Address SSO PF and function for SSO add-work requests to obtain
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
int rte_pmd_bphy_sso_pf_func_get_rmt(uint16_t *pf_func);

#ifdef __cplusplus
}
#endif

#endif /* _CNXK_BPHY_H_ */
