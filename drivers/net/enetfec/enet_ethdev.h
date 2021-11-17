/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#ifndef __ENETFEC_ETHDEV_H__
#define __ENETFEC_ETHDEV_H__

#include <rte_ethdev.h>

#define BD_LEN			49152
#define ENETFEC_TX_FR_SIZE	2048
#define ETH_HLEN		RTE_ETHER_HDR_LEN

/* full duplex */
#define FULL_DUPLEX		0x00

#define MAX_TX_BD_RING_SIZE	512	/* It should be power of 2 */
#define MAX_RX_BD_RING_SIZE	512
#define PKT_MAX_BUF_SIZE	1984
#define OPT_FRAME_SIZE		(PKT_MAX_BUF_SIZE << 16)
#define ENETFEC_MAX_RX_PKT_LEN	3000

#define __iomem
#if defined(RTE_ARCH_ARM)
#if defined(RTE_ARCH_64)
#define dcbf(p) { asm volatile("dc cvac, %0" : : "r"(p) : "memory"); }
#define dcbf_64(p) dcbf(p)

#else /* RTE_ARCH_32 */
#define dcbf(p) RTE_SET_USED(p)
#define dcbf_64(p) dcbf(p)
#endif

#else
#define dcbf(p) RTE_SET_USED(p)
#define dcbf_64(p) dcbf(p)
#endif

/*
 * ENETFEC can support 1 rx and tx queue..
 */

#define ENETFEC_MAX_Q		1

#define writel(v, p) ({*(volatile unsigned int *)(p) = (v); })
#define readl(p) rte_read32(p)

struct bufdesc {
	uint16_t		bd_datlen;  /* buffer data length */
	uint16_t		bd_sc;      /* buffer control & status */
	uint32_t		bd_bufaddr; /* buffer address */
};

struct bufdesc_ex {
	struct			bufdesc desc;
	uint32_t		bd_esc;
	uint32_t		bd_prot;
	uint32_t		bd_bdu;
	uint32_t		ts;
	uint16_t		res0[4];
};

struct bufdesc_prop {
	int			queue_id;
	/* Addresses of Tx and Rx buffers */
	struct bufdesc		*base;
	struct bufdesc		*last;
	struct bufdesc		*cur;
	void __iomem		*active_reg_desc;
	uint64_t		descr_baseaddr_p;
	unsigned short		ring_size;
	unsigned char		d_size;
	unsigned char		d_size_log2;
};

struct enetfec_priv_tx_q {
	struct bufdesc_prop	bd;
	struct rte_mbuf		*tx_mbuf[MAX_TX_BD_RING_SIZE];
	struct bufdesc		*dirty_tx;
	struct rte_mempool	*pool;
	struct enetfec_private	*fep;
};

struct enetfec_priv_rx_q {
	struct bufdesc_prop	bd;
	struct rte_mbuf		*rx_mbuf[MAX_RX_BD_RING_SIZE];
	struct rte_mempool	*pool;
	struct enetfec_private	*fep;
};

struct enetfec_private {
	struct rte_eth_dev	*dev;
	struct rte_eth_stats	stats;
	int			full_duplex;
	int			flag_pause;
	int			flag_csum;
	uint32_t		quirks;
	uint32_t		cbus_size;
	uint32_t		enetfec_e_cntl;
	uint16_t		max_rx_queues;
	uint16_t		max_tx_queues;
	unsigned int		total_tx_ring_size;
	unsigned int		total_rx_ring_size;
	unsigned int		reg_size;
	unsigned int		bd_size;
	bool			bufdesc_ex;
	bool			rgmii_txc_delay;
	bool			rgmii_rxc_delay;
	void			*hw_baseaddr_v;
	void			*bd_addr_v;
	uint32_t		hw_baseaddr_p;
	uint32_t		bd_addr_p;
	uint32_t		bd_addr_p_r[ENETFEC_MAX_Q];
	uint32_t		bd_addr_p_t[ENETFEC_MAX_Q];
	void			*dma_baseaddr_r[ENETFEC_MAX_Q];
	void			*dma_baseaddr_t[ENETFEC_MAX_Q];
	struct enetfec_priv_rx_q *rx_queues[ENETFEC_MAX_Q];
	struct enetfec_priv_tx_q *tx_queues[ENETFEC_MAX_Q];
};

static inline struct
bufdesc *enet_get_nextdesc(struct bufdesc *bdp, struct bufdesc_prop *bd)
{
	return (bdp >= bd->last) ? bd->base
		: (struct bufdesc *)(((uintptr_t)bdp) + bd->d_size);
}

static inline int
fls64(unsigned long word)
{
	return (64 - __builtin_clzl(word)) - 1;
}

static inline struct
bufdesc *enet_get_prevdesc(struct bufdesc *bdp, struct bufdesc_prop *bd)
{
	return (bdp <= bd->base) ? bd->last
		: (struct bufdesc *)(((uintptr_t)bdp) - bd->d_size);
}

static inline int
enet_get_bd_index(struct bufdesc *bdp, struct bufdesc_prop *bd)
{
	return ((const char *)bdp - (const char *)bd->base) >> bd->d_size_log2;
}

uint16_t enetfec_recv_pkts(void *rxq1, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);
uint16_t enetfec_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

#endif /*__ENETFEC_ETHDEV_H__*/
