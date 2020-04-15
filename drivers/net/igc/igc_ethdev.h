/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#ifndef _IGC_ETHDEV_H_
#define _IGC_ETHDEV_H_

#include <rte_ethdev.h>

#include "base/igc_osdep.h"
#include "base/igc_hw.h"
#include "base/igc_i225.h"
#include "base/igc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IGC_QUEUE_PAIRS_NUM		4

#define IGC_HKEY_MAX_INDEX		10
#define IGC_RSS_RDT_SIZD		128

/*
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary.
 * This will also optimize cache line size effect.
 * H/W supports up to cache line size 128.
 */
#define IGC_ALIGN			128

#define IGC_TX_DESCRIPTOR_MULTIPLE	8
#define IGC_RX_DESCRIPTOR_MULTIPLE	8

#define IGC_RXD_ALIGN	((uint16_t)(IGC_ALIGN / \
		sizeof(union igc_adv_rx_desc)))
#define IGC_TXD_ALIGN	((uint16_t)(IGC_ALIGN / \
		sizeof(union igc_adv_tx_desc)))
#define IGC_MIN_TXD	IGC_TX_DESCRIPTOR_MULTIPLE
#define IGC_MAX_TXD	((uint16_t)(0x80000 / sizeof(union igc_adv_tx_desc)))
#define IGC_MIN_RXD	IGC_RX_DESCRIPTOR_MULTIPLE
#define IGC_MAX_RXD	((uint16_t)(0x80000 / sizeof(union igc_adv_rx_desc)))

#define IGC_TX_MAX_SEG		UINT8_MAX
#define IGC_TX_MAX_MTU_SEG	UINT8_MAX

#define IGC_RX_OFFLOAD_ALL	(    \
	DEV_RX_OFFLOAD_IPV4_CKSUM  | \
	DEV_RX_OFFLOAD_UDP_CKSUM   | \
	DEV_RX_OFFLOAD_TCP_CKSUM   | \
	DEV_RX_OFFLOAD_SCTP_CKSUM  | \
	DEV_RX_OFFLOAD_JUMBO_FRAME | \
	DEV_RX_OFFLOAD_KEEP_CRC    | \
	DEV_RX_OFFLOAD_SCATTER)

#define IGC_TX_OFFLOAD_ALL	(    \
	DEV_TX_OFFLOAD_VLAN_INSERT | \
	DEV_TX_OFFLOAD_IPV4_CKSUM  | \
	DEV_TX_OFFLOAD_UDP_CKSUM   | \
	DEV_TX_OFFLOAD_TCP_CKSUM   | \
	DEV_TX_OFFLOAD_SCTP_CKSUM  | \
	DEV_TX_OFFLOAD_TCP_TSO     | \
	DEV_TX_OFFLOAD_UDP_TSO	   | \
	DEV_TX_OFFLOAD_MULTI_SEGS)

#define IGC_RSS_OFFLOAD_ALL	(    \
	ETH_RSS_IPV4               | \
	ETH_RSS_NONFRAG_IPV4_TCP   | \
	ETH_RSS_NONFRAG_IPV4_UDP   | \
	ETH_RSS_IPV6               | \
	ETH_RSS_NONFRAG_IPV6_TCP   | \
	ETH_RSS_NONFRAG_IPV6_UDP   | \
	ETH_RSS_IPV6_EX            | \
	ETH_RSS_IPV6_TCP_EX        | \
	ETH_RSS_IPV6_UDP_EX)

/* structure for interrupt relative data */
struct igc_interrupt {
	uint32_t flags;
	uint32_t mask;
};

/* Union of RSS redirect table register */
union igc_rss_reta_reg {
	uint32_t dword;
	uint8_t  bytes[4];
};

/* Structure to per-queue statics */
struct igc_hw_queue_stats {
	u64	pqgprc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good packets received count */
	u64	pqgptc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good packets transmitted count */
	u64	pqgorc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good octets received count */
	u64	pqgotc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good octets transmitted count */
	u64	pqmprc[IGC_QUEUE_PAIRS_NUM];
	/* per queue multicast packets received count */
	u64	rqdpc[IGC_QUEUE_PAIRS_NUM];
	/* per receive queue drop packet count */
	u64	tqdpc[IGC_QUEUE_PAIRS_NUM];
	/* per transmit queue drop packet count */
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct igc_adapter {
	struct igc_hw		hw;
	struct igc_hw_stats	stats;
	struct igc_hw_queue_stats queue_stats;
	int16_t txq_stats_map[IGC_QUEUE_PAIRS_NUM];
	int16_t rxq_stats_map[IGC_QUEUE_PAIRS_NUM];

	struct igc_interrupt	intr;
	bool		stopped;
};

#define IGC_DEV_PRIVATE(_dev)	((_dev)->data->dev_private)

#define IGC_DEV_PRIVATE_HW(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->hw)

#define IGC_DEV_PRIVATE_STATS(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->stats)

#define IGC_DEV_PRIVATE_QUEUE_STATS(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->queue_stats)

#define IGC_DEV_PRIVATE_INTR(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->intr)

static inline void
igc_read_reg_check_set_bits(struct igc_hw *hw, uint32_t reg, uint32_t bits)
{
	uint32_t reg_val = IGC_READ_REG(hw, reg);

	bits |= reg_val;
	if (bits == reg_val)
		return;	/* no need to write back */

	IGC_WRITE_REG(hw, reg, bits);
}

static inline void
igc_read_reg_check_clear_bits(struct igc_hw *hw, uint32_t reg, uint32_t bits)
{
	uint32_t reg_val = IGC_READ_REG(hw, reg);

	bits = reg_val & ~bits;
	if (bits == reg_val)
		return;	/* no need to write back */

	IGC_WRITE_REG(hw, reg, bits);
}

#ifdef __cplusplus
}
#endif

#endif /* _IGC_ETHDEV_H_ */
