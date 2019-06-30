/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_RX_H__
#define __OTX2_RX_H__

/* Default mark value used when none is provided. */
#define OTX2_FLOW_ACTION_FLAG_DEFAULT	0xffff

#define PTYPE_WIDTH 12
#define PTYPE_NON_TUNNEL_ARRAY_SZ	BIT(PTYPE_WIDTH)
#define PTYPE_TUNNEL_ARRAY_SZ		BIT(PTYPE_WIDTH)
#define PTYPE_ARRAY_SZ			((PTYPE_NON_TUNNEL_ARRAY_SZ +\
					 PTYPE_TUNNEL_ARRAY_SZ) *\
					 sizeof(uint16_t))

#define NIX_RX_OFFLOAD_PTYPE_F         BIT(1)
#define NIX_RX_OFFLOAD_VLAN_STRIP_F    BIT(3)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F   BIT(4)
#define NIX_RX_OFFLOAD_TSTAMP_F        BIT(5)

#define NIX_TIMESYNC_RX_OFFSET		8

struct otx2_timesync_info {
	uint64_t	rx_tstamp;
	rte_iova_t	tx_tstamp_iova;
	uint64_t	*tx_tstamp;
	uint8_t		tx_ready;
	uint8_t		rx_ready;
} __rte_cache_aligned;

#endif /* __OTX2_RX_H__ */
