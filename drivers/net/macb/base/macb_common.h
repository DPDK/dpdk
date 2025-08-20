/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#ifndef _MACB_COMMON_H_
#define _MACB_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_random.h>
#include <rte_io.h>

#include "macb_type.h"
#include "macb_hw.h"
#include "generic_phy.h"
#include "macb_errno.h"
#include "../macb_log.h"
#include "macb_uio.h"

#define BIT(nr)	(1UL << (nr))

#define MACB_MAX_PORT_NUM 4
#define	MACB_MIN_RING_DESC	64
#define	MACB_MAX_RING_DESC	4096
#define MACB_RXD_ALIGN	64
#define MACB_TXD_ALIGN	64

#define TX_RING_BYTES(bp)	(macb_dma_desc_get_size(bp)	\
				 * (bp)->tx_ring_size)
#define RX_RING_BYTES(bp)	(macb_dma_desc_get_size(bp)	\
				 * (bp)->rx_ring_size)
#define MACB_TX_LEN_ALIGN	8
#define MACB_RX_LEN_ALIGN	8


#define MACB_RX_RING_SIZE	256
#define MACB_TX_RING_SIZE	256
#define MAX_JUMBO_FRAME_SIZE	10240
#define MIN_JUMBO_FRAME_SIZE	16

#define RX_BUFFER_MULTIPLE	64  /* bytes */
#define PCLK_HZ_2 20000000
#define PCLK_HZ_4 40000000
#define PCLK_HZ_8 80000000
#define PCLK_HZ_12 120000000
#define PCLK_HZ_16 160000000

#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((u32)(n))
#define cpu_to_le16(x)  (x)
#define cpu_to_le32(x)  (x)

#define MACB_MII_CLK_ENABLE 0x1
#define MACB_MII_CLK_DISABLE 0x0

/* dtb for Phytium MAC */
#define OF_PHYTIUM_GEM1P0_MAC	"cdns,phytium-gem-1.0"	/* Phytium 1.0 MAC */
#define OF_PHYTIUM_GEM2P0_MAC	"cdns,phytium-gem-2.0"	/* Phytium 2.0 MAC */

/* acpi for Phytium MAC */
#define ACPI_PHYTIUM_GEM1P0_MAC	"PHYT0036"	/* Phytium 1.0  MAC */

typedef u64 netdev_features_t;

/**
 * Interface Mode definitions.
 * Warning: must be consistent with dpdk definition !
 */
typedef enum {
	MACB_PHY_INTERFACE_MODE_NA,
	MACB_PHY_INTERFACE_MODE_INTERNAL,
	MACB_PHY_INTERFACE_MODE_MII,
	MACB_PHY_INTERFACE_MODE_GMII,
	MACB_PHY_INTERFACE_MODE_SGMII,
	MACB_PHY_INTERFACE_MODE_TBI,
	MACB_PHY_INTERFACE_MODE_REVMII,
	MACB_PHY_INTERFACE_MODE_RMII,
	MACB_PHY_INTERFACE_MODE_RGMII,
	MACB_PHY_INTERFACE_MODE_RGMII_ID,
	MACB_PHY_INTERFACE_MODE_RGMII_RXID,
	MACB_PHY_INTERFACE_MODE_RGMII_TXID,
	MACB_PHY_INTERFACE_MODE_RTBI,
	MACB_PHY_INTERFACE_MODE_SMII,
	MACB_PHY_INTERFACE_MODE_XGMII,
	MACB_PHY_INTERFACE_MODE_MOCA,
	MACB_PHY_INTERFACE_MODE_QSGMII,
	MACB_PHY_INTERFACE_MODE_TRGMII,
	MACB_PHY_INTERFACE_MODE_100BASEX,
	MACB_PHY_INTERFACE_MODE_1000BASEX,
	MACB_PHY_INTERFACE_MODE_2500BASEX,
	MACB_PHY_INTERFACE_MODE_5GBASER,
	MACB_PHY_INTERFACE_MODE_RXAUI,
	MACB_PHY_INTERFACE_MODE_XAUI,
	/* 10GBASE-R, XFI, SFI - single lane 10G Serdes */
	MACB_PHY_INTERFACE_MODE_10GBASER,
	MACB_PHY_INTERFACE_MODE_USXGMII,
	/* 10GBASE-KR - with Clause 73 AN */
	MACB_PHY_INTERFACE_MODE_10GKR,
	MACB_PHY_INTERFACE_MODE_MAX,
} phy_interface_t;

typedef enum {
	DEV_TYPE_PHYTIUM_GEM1P0_MAC,
	DEV_TYPE_PHYTIUM_GEM2P0_MAC,
	DEV_TYPE_DEFAULT,
} dev_type_t;

struct macb_dma_desc {
	u32	addr;
	u32	ctrl;
};

struct macb_dma_desc_64 {
	u32 addrh;
	u32 resvd;
};

struct macb_dma_desc_ptp {
	u32	ts_1;
	u32	ts_2;
};

struct macb;
struct macb_rx_queue;
struct macb_tx_queue;

struct macb_config {
	u32			caps;
	unsigned int		dma_burst_length;
	int	jumbo_max_len;
	void (*sel_clk_hw)(struct macb *bp);
};

struct macb {
	struct macb_iomem		*iomem;
	uintptr_t		base;
	phys_addr_t		paddr;
	bool			native_io;
	bool			rx_bulk_alloc_allowed;
	bool			rx_vec_allowed;

	size_t			rx_buffer_size;

	unsigned int		rx_ring_size;
	unsigned int		tx_ring_size;

	unsigned int		num_queues;
	unsigned int		queue_mask;

	struct rte_eth_dev	*dev;
	union {
		struct macb_stats	macb;
		struct gem_stats	gem;
	} hw_stats;

	uint16_t phyad;
	uint32_t speed;
	uint16_t link;
	uint16_t duplex;
	uint16_t autoneg;
	uint16_t fixed_link;
	u32			caps;
	unsigned int		dma_burst_length;

	unsigned int		rx_frm_len_mask;
	unsigned int		jumbo_max_len;

	uint8_t hw_dma_cap;

	bool phydrv_used;
	struct phy_device *phydev;

	int	rx_bd_rd_prefetch;
	int	tx_bd_rd_prefetch;

	u32 max_tuples;
	phy_interface_t		phy_interface;
	u32 dev_type;
	u32 data_bus_width;
	/* PHYTIUM  sel clk */
	void (*sel_clk_hw)(struct macb *bp);
};

static inline u32 macb_reg_readl(struct macb *bp, int offset)
{
	return rte_le_to_cpu_32(rte_read32((void *)(bp->base + offset)));
}

static inline void macb_reg_writel(struct macb *bp, int offset, u32 value)
{
	rte_write32(rte_cpu_to_le_32(value), (void *)(bp->base + offset));
}

#define macb_readl(port, reg)		macb_reg_readl((port), MACB_##reg)
#define macb_writel(port, reg, value)	macb_reg_writel((port), MACB_##reg, (value))
#define gem_readl(port, reg)		macb_reg_readl((port), GEM_##reg)
#define gem_writel(port, reg, value)	macb_reg_writel((port), GEM_##reg, (value))
#define queue_readl(queue, reg)		macb_reg_readl((queue)->bp, (queue)->reg)
#define queue_writel(queue, reg, value)	macb_reg_writel((queue)->bp, (queue)->reg, (value))
#define macb_queue_flush(port, reg, value)	macb_reg_writel((port), (reg), (value))
#define gem_readl_n(port, reg, idx)		macb_reg_readl((port), GEM_##reg + idx * 4)
#define gem_writel_n(port, reg, idx, value) \
	macb_reg_writel((port), GEM_##reg + idx * 4, (value))

bool macb_is_gem(struct macb *bp);
bool macb_hw_is_native_io(struct macb *bp);
u32 macb_dbw(struct macb *bp);
void macb_probe_queues(uintptr_t base, bool native_io,
		       unsigned int *queue_mask, unsigned int *num_queues);
void macb_configure_caps(struct macb *bp, const struct macb_config *dt_conf);

int macb_iomem_init(const char *name, struct macb *bp, phys_addr_t paddr);
int macb_iomem_deinit(struct macb *bp);

void macb_get_stats(struct macb *bp);
int macb_mdio_read(struct macb *bp, uint16_t phy_id, uint32_t regnum);
int macb_mdio_write(struct macb *bp, uint16_t phy_id, uint32_t regnum, uint16_t value);

void macb_gem1p0_sel_clk(struct macb *bp);
void macb_gem2p0_sel_clk(struct macb *bp);

int macb_mac_with_pcs_config(struct macb *bp);

int macb_link_change(struct macb *bp);
void macb_check_for_link(struct macb *bp);
void macb_setup_link(struct macb *bp);
void macb_reset_hw(struct macb *bp);

#endif /* _MACB_COMMON_H_ */
