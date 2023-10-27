/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_COMMON_H__
#define __NFP_COMMON_H__

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_io.h>
#include <rte_spinlock.h>

#include "nfp_common_ctrl.h"

#define NFP_QCP_QUEUE_ADDR_SZ   (0x800)

/* Macros for accessing the Queue Controller Peripheral 'CSRs' */
#define NFP_QCP_QUEUE_OFF(_x)                 ((_x) * 0x800)
#define NFP_QCP_QUEUE_ADD_RPTR                  0x0000
#define NFP_QCP_QUEUE_ADD_WPTR                  0x0004
#define NFP_QCP_QUEUE_STS_LO                    0x0008
#define NFP_QCP_QUEUE_STS_LO_READPTR_MASK     (0x3ffff)
#define NFP_QCP_QUEUE_STS_HI                    0x000c
#define NFP_QCP_QUEUE_STS_HI_WRITEPTR_MASK    (0x3ffff)

/* Read or Write Pointer of a queue */
enum nfp_qcp_ptr {
	NFP_QCP_READ_PTR = 0,
	NFP_QCP_WRITE_PTR
};

struct nfp_hw {
	uint8_t *ctrl_bar;
	uint8_t *qcp_cfg;
	uint32_t cap;
	uint32_t cap_ext;
	uint32_t ctrl;
	uint32_t ctrl_ext;
	rte_spinlock_t reconfig_lock;
	struct rte_ether_addr mac_addr;
};

static inline uint8_t
nn_readb(volatile const void *addr)
{
	return rte_read8(addr);
}

static inline void
nn_writeb(uint8_t val,
		volatile void *addr)
{
	rte_write8(val, addr);
}

static inline uint32_t
nn_readl(volatile const void *addr)
{
	return rte_read32(addr);
}

static inline void
nn_writel(uint32_t val,
		volatile void *addr)
{
	rte_write32(val, addr);
}

static inline uint16_t
nn_readw(volatile const void *addr)
{
	return rte_read16(addr);
}

static inline void
nn_writew(uint16_t val,
		volatile void *addr)
{
	rte_write16(val, addr);
}

static inline uint64_t
nn_readq(volatile void *addr)
{
	uint32_t low;
	uint32_t high;
	const volatile uint32_t *p = addr;

	high = nn_readl((volatile const void *)(p + 1));
	low = nn_readl((volatile const void *)p);

	return low + ((uint64_t)high << 32);
}

static inline void
nn_writeq(uint64_t val,
		volatile void *addr)
{
	nn_writel(val >> 32, (volatile char *)addr + 4);
	nn_writel(val, addr);
}

static inline uint8_t
nn_cfg_readb(struct nfp_hw *hw,
		uint32_t off)
{
	return nn_readb(hw->ctrl_bar + off);
}

static inline void
nn_cfg_writeb(struct nfp_hw *hw,
		uint32_t off,
		uint8_t val)
{
	nn_writeb(val, hw->ctrl_bar + off);
}

static inline uint16_t
nn_cfg_readw(struct nfp_hw *hw,
		uint32_t off)
{
	return rte_le_to_cpu_16(nn_readw(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writew(struct nfp_hw *hw,
		uint32_t off,
		uint16_t val)
{
	nn_writew(rte_cpu_to_le_16(val), hw->ctrl_bar + off);
}

static inline uint32_t
nn_cfg_readl(struct nfp_hw *hw,
		uint32_t off)
{
	return rte_le_to_cpu_32(nn_readl(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writel(struct nfp_hw *hw,
		uint32_t off,
		uint32_t val)
{
	nn_writel(rte_cpu_to_le_32(val), hw->ctrl_bar + off);
}

static inline uint64_t
nn_cfg_readq(struct nfp_hw *hw,
		uint32_t off)
{
	return rte_le_to_cpu_64(nn_readq(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writeq(struct nfp_hw *hw,
		uint32_t off,
		uint64_t val)
{
	nn_writeq(rte_cpu_to_le_64(val), hw->ctrl_bar + off);
}

/**
 * Add the value to the selected pointer of a queue.
 *
 * @param queue
 *   Base address for queue structure
 * @param ptr
 *   Add to the read or write pointer
 * @param val
 *   Value to add to the queue pointer
 */
static inline void
nfp_qcp_ptr_add(uint8_t *queue,
		enum nfp_qcp_ptr ptr,
		uint32_t val)
{
	uint32_t off;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_ADD_RPTR;
	else
		off = NFP_QCP_QUEUE_ADD_WPTR;

	nn_writel(rte_cpu_to_le_32(val), queue + off);
}

/**
 * Read the current read/write pointer value for a queue.
 *
 * @param queue
 *   Base address for queue structure
 * @param ptr
 *   Read or Write pointer
 */
static inline uint32_t
nfp_qcp_read(uint8_t *queue,
		enum nfp_qcp_ptr ptr)
{
	uint32_t off;
	uint32_t val;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_STS_LO;
	else
		off = NFP_QCP_QUEUE_STS_HI;

	val = rte_cpu_to_le_32(nn_readl(queue + off));

	if (ptr == NFP_QCP_READ_PTR)
		return val & NFP_QCP_QUEUE_STS_LO_READPTR_MASK;
	else
		return val & NFP_QCP_QUEUE_STS_HI_WRITEPTR_MASK;
}

__rte_internal
int nfp_reconfig_real(struct nfp_hw *hw, uint32_t update);

__rte_internal
int nfp_reconfig(struct nfp_hw *hw, uint32_t ctrl, uint32_t update);

__rte_internal
int nfp_ext_reconfig(struct nfp_hw *hw, uint32_t ctrl_ext, uint32_t update);

__rte_internal
void nfp_read_mac(struct nfp_hw *hw);

__rte_internal
void nfp_write_mac(struct nfp_hw *hw, uint8_t *mac);

__rte_internal
void nfp_enable_queues(struct nfp_hw *hw, uint16_t nb_rx_queues,
		uint16_t nb_tx_queues);

__rte_internal
void nfp_disable_queues(struct nfp_hw *hw);

#endif/* __NFP_COMMON_H__ */
