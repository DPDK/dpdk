/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_udp.h>

#include "i40e_logs.h"
#include "i40e/i40e_prototype.h"
#include "i40e/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

#define I40E_MIN_RING_DESC     64
#define I40E_MAX_RING_DESC     4096
#define I40E_ALIGN             128
#define DEFAULT_TX_RS_THRESH   32
#define DEFAULT_TX_FREE_THRESH 32
#define I40E_MAX_PKT_TYPE      256

#define I40E_VLAN_TAG_SIZE 4
#define I40E_TX_MAX_BURST  32

#define I40E_DMA_MEM_ALIGN 4096

#define I40E_SIMPLE_FLAGS ((uint32_t)ETH_TXQ_FLAGS_NOMULTSEGS | \
					ETH_TXQ_FLAGS_NOOFFLOADS)

#define I40E_TXD_CMD (I40E_TX_DESC_CMD_EOP | I40E_TX_DESC_CMD_RS)

#define RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mb) \
	(uint64_t) ((mb)->buf_physaddr + RTE_PKTMBUF_HEADROOM)

#define RTE_MBUF_DATA_DMA_ADDR(mb) \
	((uint64_t)((mb)->buf_physaddr + \
	(uint64_t)((char *)((mb)->pkt.data) - \
	(char *)(mb)->buf_addr)))

static const struct rte_memzone *
i40e_ring_dma_zone_reserve(struct rte_eth_dev *dev,
			   const char *ring_name,
			   uint16_t queue_id,
			   uint32_t ring_size,
			   int socket_id);
static void i40e_reset_rx_queue(struct i40e_rx_queue *rxq);
static void i40e_reset_tx_queue(struct i40e_tx_queue *txq);
static void i40e_tx_queue_release_mbufs(struct i40e_tx_queue *txq);
static uint16_t i40e_xmit_pkts_simple(void *tx_queue,
				      struct rte_mbuf **tx_pkts,
				      uint16_t nb_pkts);

/* Translate the rx descriptor status to pkt flags */
static inline uint16_t
i40e_rxd_status_to_pkt_flags(uint64_t qword)
{
	uint16_t flags;

	/* Check if VLAN packet */
	flags = (uint16_t)(qword & (1 << I40E_RX_DESC_STATUS_L2TAG1P_SHIFT) ?
							PKT_RX_VLAN_PKT : 0);

	/* Check if RSS_HASH */
	flags |= (uint16_t)((((qword >> I40E_RX_DESC_STATUS_FLTSTAT_SHIFT) &
					I40E_RX_DESC_FLTSTAT_RSS_HASH) ==
			I40E_RX_DESC_FLTSTAT_RSS_HASH) ? PKT_RX_RSS_HASH : 0);

	return flags;
}

static inline uint16_t
i40e_rxd_error_to_pkt_flags(uint64_t qword)
{
	uint16_t flags = 0;
	uint64_t error_bits = (qword >> I40E_RXD_QW1_ERROR_SHIFT);

#define I40E_RX_ERR_BITS 0x3f
	if (likely((error_bits & I40E_RX_ERR_BITS) == 0))
		return flags;
	/* If RXE bit set, all other status bits are meaningless */
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_RXE_SHIFT))) {
		flags |= PKT_RX_MAC_ERR;
		return flags;
	}

	/* If RECIPE bit set, all other status indications should be ignored */
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_RECIPE_SHIFT))) {
		flags |= PKT_RX_RECIP_ERR;
		return flags;
	}
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_HBO_SHIFT)))
		flags |= PKT_RX_HBUF_OVERFLOW;
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_IPE_SHIFT)))
		flags |= PKT_RX_IP_CKSUM_BAD;
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_L4E_SHIFT)))
		flags |= PKT_RX_L4_CKSUM_BAD;
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_EIPE_SHIFT)))
		flags |= PKT_RX_EIP_CKSUM_BAD;
	if (unlikely(error_bits & (1 << I40E_RX_DESC_ERROR_OVERSIZE_SHIFT)))
		flags |= PKT_RX_OVERSIZE;

	return flags;
}

/* Translate pkt types to pkt flags */
static inline uint16_t
i40e_rxd_ptype_to_pkt_flags(uint64_t qword)
{
	uint8_t ptype = (uint8_t)((qword & I40E_RXD_QW1_PTYPE_MASK) >>
					I40E_RXD_QW1_PTYPE_SHIFT);
	static const uint16_t ip_ptype_map[I40E_MAX_PKT_TYPE] = {
		0, /* PTYPE 0 */
		0, /* PTYPE 1 */
		0, /* PTYPE 2 */
		0, /* PTYPE 3 */
		0, /* PTYPE 4 */
		0, /* PTYPE 5 */
		0, /* PTYPE 6 */
		0, /* PTYPE 7 */
		0, /* PTYPE 8 */
		0, /* PTYPE 9 */
		0, /* PTYPE 10 */
		0, /* PTYPE 11 */
		0, /* PTYPE 12 */
		0, /* PTYPE 13 */
		0, /* PTYPE 14 */
		0, /* PTYPE 15 */
		0, /* PTYPE 16 */
		0, /* PTYPE 17 */
		0, /* PTYPE 18 */
		0, /* PTYPE 19 */
		0, /* PTYPE 20 */
		0, /* PTYPE 21 */
		PKT_RX_IPV4_HDR, /* PTYPE 22 */
		PKT_RX_IPV4_HDR, /* PTYPE 23 */
		PKT_RX_IPV4_HDR, /* PTYPE 24 */
		0, /* PTYPE 25 */
		PKT_RX_IPV4_HDR, /* PTYPE 26 */
		PKT_RX_IPV4_HDR, /* PTYPE 27 */
		PKT_RX_IPV4_HDR, /* PTYPE 28 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 29 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 30 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 31 */
		0, /* PTYPE 32 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 33 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 34 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 35 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 36 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 37 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 38 */
		0, /* PTYPE 39 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 40 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 41 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 42 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 43 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 44 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 45 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 46 */
		0, /* PTYPE 47 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 48 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 49 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 50 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 51 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 52 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 53 */
		0, /* PTYPE 54 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 55 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 56 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 57 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 58 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 59 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 60 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 61 */
		0, /* PTYPE 62 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 63 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 64 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 65 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 66 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 67 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 68 */
		0, /* PTYPE 69 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 70 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 71 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 72 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 73 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 74 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 75 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 76 */
		0, /* PTYPE 77 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 78 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 79 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 80 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 81 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 82 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 83 */
		0, /* PTYPE 84 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 85 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 86 */
		PKT_RX_IPV4_HDR_EXT, /* PTYPE 87 */
		PKT_RX_IPV6_HDR, /* PTYPE 88 */
		PKT_RX_IPV6_HDR, /* PTYPE 89 */
		PKT_RX_IPV6_HDR, /* PTYPE 90 */
		0, /* PTYPE 91 */
		PKT_RX_IPV6_HDR, /* PTYPE 92 */
		PKT_RX_IPV6_HDR, /* PTYPE 93 */
		PKT_RX_IPV6_HDR, /* PTYPE 94 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 95 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 96 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 97 */
		0, /* PTYPE 98 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 99 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 100 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 101 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 102 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 103 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 104 */
		0, /* PTYPE 105 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 106 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 107 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 108 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 109 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 110 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 111 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 112 */
		0, /* PTYPE 113 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 114 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 115 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 116 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 117 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 118 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 119 */
		0, /* PTYPE 120 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 121 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 122 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 123 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 124 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 125 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 126 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 127 */
		0, /* PTYPE 128 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 129 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 130 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 131 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 132 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 133 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 134 */
		0, /* PTYPE 135 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 136 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 137 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 138 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 139 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 140 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 141 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 142 */
		0, /* PTYPE 143 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 144 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 145 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 146 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 147 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 148 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 149 */
		0, /* PTYPE 150 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 151 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 152 */
		PKT_RX_IPV6_HDR_EXT, /* PTYPE 153 */
		0, /* PTYPE 154 */
		0, /* PTYPE 155 */
		0, /* PTYPE 156 */
		0, /* PTYPE 157 */
		0, /* PTYPE 158 */
		0, /* PTYPE 159 */
		0, /* PTYPE 160 */
		0, /* PTYPE 161 */
		0, /* PTYPE 162 */
		0, /* PTYPE 163 */
		0, /* PTYPE 164 */
		0, /* PTYPE 165 */
		0, /* PTYPE 166 */
		0, /* PTYPE 167 */
		0, /* PTYPE 168 */
		0, /* PTYPE 169 */
		0, /* PTYPE 170 */
		0, /* PTYPE 171 */
		0, /* PTYPE 172 */
		0, /* PTYPE 173 */
		0, /* PTYPE 174 */
		0, /* PTYPE 175 */
		0, /* PTYPE 176 */
		0, /* PTYPE 177 */
		0, /* PTYPE 178 */
		0, /* PTYPE 179 */
		0, /* PTYPE 180 */
		0, /* PTYPE 181 */
		0, /* PTYPE 182 */
		0, /* PTYPE 183 */
		0, /* PTYPE 184 */
		0, /* PTYPE 185 */
		0, /* PTYPE 186 */
		0, /* PTYPE 187 */
		0, /* PTYPE 188 */
		0, /* PTYPE 189 */
		0, /* PTYPE 190 */
		0, /* PTYPE 191 */
		0, /* PTYPE 192 */
		0, /* PTYPE 193 */
		0, /* PTYPE 194 */
		0, /* PTYPE 195 */
		0, /* PTYPE 196 */
		0, /* PTYPE 197 */
		0, /* PTYPE 198 */
		0, /* PTYPE 199 */
		0, /* PTYPE 200 */
		0, /* PTYPE 201 */
		0, /* PTYPE 202 */
		0, /* PTYPE 203 */
		0, /* PTYPE 204 */
		0, /* PTYPE 205 */
		0, /* PTYPE 206 */
		0, /* PTYPE 207 */
		0, /* PTYPE 208 */
		0, /* PTYPE 209 */
		0, /* PTYPE 210 */
		0, /* PTYPE 211 */
		0, /* PTYPE 212 */
		0, /* PTYPE 213 */
		0, /* PTYPE 214 */
		0, /* PTYPE 215 */
		0, /* PTYPE 216 */
		0, /* PTYPE 217 */
		0, /* PTYPE 218 */
		0, /* PTYPE 219 */
		0, /* PTYPE 220 */
		0, /* PTYPE 221 */
		0, /* PTYPE 222 */
		0, /* PTYPE 223 */
		0, /* PTYPE 224 */
		0, /* PTYPE 225 */
		0, /* PTYPE 226 */
		0, /* PTYPE 227 */
		0, /* PTYPE 228 */
		0, /* PTYPE 229 */
		0, /* PTYPE 230 */
		0, /* PTYPE 231 */
		0, /* PTYPE 232 */
		0, /* PTYPE 233 */
		0, /* PTYPE 234 */
		0, /* PTYPE 235 */
		0, /* PTYPE 236 */
		0, /* PTYPE 237 */
		0, /* PTYPE 238 */
		0, /* PTYPE 239 */
		0, /* PTYPE 240 */
		0, /* PTYPE 241 */
		0, /* PTYPE 242 */
		0, /* PTYPE 243 */
		0, /* PTYPE 244 */
		0, /* PTYPE 245 */
		0, /* PTYPE 246 */
		0, /* PTYPE 247 */
		0, /* PTYPE 248 */
		0, /* PTYPE 249 */
		0, /* PTYPE 250 */
		0, /* PTYPE 251 */
		0, /* PTYPE 252 */
		0, /* PTYPE 253 */
		0, /* PTYPE 254 */
		0, /* PTYPE 255 */
	};

	return ip_ptype_map[ptype];
}

static inline void
i40e_txd_enable_checksum(uint32_t ol_flags,
			uint32_t *td_cmd,
			uint32_t *td_offset,
			uint8_t l2_len,
			uint8_t l3_len)
{
	if (!l2_len) {
		PMD_DRV_LOG(DEBUG, "L2 length set to 0\n");
		return;
	}
	*td_offset |= (l2_len >> 1) << I40E_TX_DESC_LENGTH_MACLEN_SHIFT;

	if (!l3_len) {
		PMD_DRV_LOG(DEBUG, "L3 length set to 0\n");
		return;
	}

	/* Enable L3 checksum offloads */
	if (ol_flags & PKT_TX_IPV4_CSUM) {
		*td_cmd |= I40E_TX_DESC_CMD_IIPT_IPV4_CSUM;
		*td_offset |= (l3_len >> 2) << I40E_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & PKT_TX_IPV4) {
		*td_cmd |= I40E_TX_DESC_CMD_IIPT_IPV4;
		*td_offset |= (l3_len >> 2) << I40E_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & PKT_TX_IPV6) {
		*td_cmd |= I40E_TX_DESC_CMD_IIPT_IPV6;
		*td_offset |= (l3_len >> 2) << I40E_TX_DESC_LENGTH_IPLEN_SHIFT;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & PKT_TX_L4_MASK) {
	case PKT_TX_TCP_CKSUM:
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (sizeof(struct tcp_hdr) >> 2) <<
				I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case PKT_TX_SCTP_CKSUM:
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_SCTP;
		*td_offset |= (sizeof(struct sctp_hdr) >> 2) <<
				I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case PKT_TX_UDP_CKSUM:
		*td_cmd |= I40E_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (sizeof(struct udp_hdr) >> 2) <<
				I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		break;
	}
}

static inline struct rte_mbuf *
rte_rxmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	m = __rte_mbuf_raw_alloc(mp);
	__rte_mbuf_sanity_check_raw(m, RTE_MBUF_PKT, 0);

	return m;
}

/* Construct the tx flags */
static inline uint64_t
i40e_build_ctob(uint32_t td_cmd,
		uint32_t td_offset,
		unsigned int size,
		uint32_t td_tag)
{
	return rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)td_cmd  << I40E_TXD_QW1_CMD_SHIFT) |
			((uint64_t)td_offset << I40E_TXD_QW1_OFFSET_SHIFT) |
			((uint64_t)size  << I40E_TXD_QW1_TX_BUF_SZ_SHIFT) |
			((uint64_t)td_tag  << I40E_TXD_QW1_L2TAG1_SHIFT));
}

static inline int
i40e_xmit_cleanup(struct i40e_tx_queue *txq)
{
	struct i40e_tx_entry *sw_ring = txq->sw_ring;
	volatile struct i40e_tx_desc *txd = txq->tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if (!(txd[desc_to_clean_to].cmd_type_offset_bsz &
		rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE))) {
		PMD_TX_FREE_LOG(DEBUG, "TX descriptor %4u is not done "
			"(port=%d queue=%d)", desc_to_clean_to,
				txq->port_id, txq->queue_id);
		return -1;
	}

	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
							desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
					last_desc_cleaned);

	txd[desc_to_clean_to].cmd_type_offset_bsz = 0;

	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	return 0;
}

static inline int
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
check_rx_burst_bulk_alloc_preconditions(struct i40e_rx_queue *rxq)
#else
check_rx_burst_bulk_alloc_preconditions(__rte_unused struct i40e_rx_queue *rxq)
#endif
{
	int ret = 0;

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	if (!(rxq->rx_free_thresh >= RTE_PMD_I40E_RX_MAX_BURST))
		ret = -EINVAL;
	else if (!(rxq->rx_free_thresh < rxq->nb_rx_desc))
		ret = -EINVAL;
	else if (!(rxq->nb_rx_desc % rxq->rx_free_thresh) == 0)
		ret = -EINVAL;
	else if (!(rxq->nb_rx_desc < (I40E_MAX_RING_DESC -
				RTE_PMD_I40E_RX_MAX_BURST)))
		ret = -EINVAL;
#else
	ret = -EINVAL;
#endif

	return ret;
}

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
#define I40E_LOOK_AHEAD 8
#if (I40E_LOOK_AHEAD != 8)
#error "PMD I40E: I40E_LOOK_AHEAD must be 8\n"
#endif
static inline int
i40e_rx_scan_hw_ring(struct i40e_rx_queue *rxq)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	uint64_t qword1;
	uint32_t rx_status;
	int32_t s[I40E_LOOK_AHEAD], nb_dd;
	int32_t i, j, nb_rx = 0;
	uint16_t pkt_flags;

	rxdp = &rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
	rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
				I40E_RXD_QW1_STATUS_SHIFT;

	/* Make sure there is at least 1 packet to receive */
	if (!(rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/**
	 * Scan LOOK_AHEAD descriptors at a time to determine which
	 * descriptors reference packets that are ready to be received.
	 */
	for (i = 0; i < RTE_PMD_I40E_RX_MAX_BURST; i+=I40E_LOOK_AHEAD,
			rxdp += I40E_LOOK_AHEAD, rxep += I40E_LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = I40E_LOOK_AHEAD - 1; j >= 0; j--) {
			qword1 = rte_le_to_cpu_64(\
				rxdp[j].wb.qword1.status_error_len);
			s[j] = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
					I40E_RXD_QW1_STATUS_SHIFT;
		}

		/* Compute how many status bits were set */
		for (j = 0, nb_dd = 0; j < I40E_LOOK_AHEAD; j++)
			nb_dd += s[j] & (1 << I40E_RX_DESC_STATUS_DD_SHIFT);

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf parameters */
		for (j = 0; j < nb_dd; j++) {
			mb = rxep[j].mbuf;
			qword1 = rte_le_to_cpu_64(\
				rxdp[j].wb.qword1.status_error_len);
			rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
						I40E_RXD_QW1_STATUS_SHIFT;
			pkt_len = ((qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
				I40E_RXD_QW1_LENGTH_PBUF_SHIFT) - rxq->crc_len;
			mb->pkt.data_len = pkt_len;
			mb->pkt.pkt_len = pkt_len;
			mb->pkt.vlan_macip.f.vlan_tci = rx_status &
				(1 << I40E_RX_DESC_STATUS_L2TAG1P_SHIFT) ?
			rte_le_to_cpu_16(\
				rxdp[j].wb.qword0.lo_dword.l2tag1) : 0;
			pkt_flags = i40e_rxd_status_to_pkt_flags(qword1);
			pkt_flags |= i40e_rxd_error_to_pkt_flags(qword1);
			pkt_flags |= i40e_rxd_ptype_to_pkt_flags(qword1);
			mb->ol_flags = pkt_flags;
			if (pkt_flags & PKT_RX_RSS_HASH)
				mb->pkt.hash.rss = rte_le_to_cpu_32(\
					rxdp->wb.qword0.hi_dword.rss);
		}

		for (j = 0; j < I40E_LOOK_AHEAD; j++)
			rxq->rx_stage[i + j] = rxep[j].mbuf;

		if (nb_dd != I40E_LOOK_AHEAD)
			break;
	}

	/* Clear software ring entries */
	for (i = 0; i < nb_rx; i++)
		rxq->sw_ring[rxq->rx_tail + i].mbuf = NULL;

	return nb_rx;
}

static inline uint16_t
i40e_rx_fill_from_stage(struct i40e_rx_queue *rxq,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	uint16_t i;
	struct rte_mbuf **stage = &rxq->rx_stage[rxq->rx_next_avail];

	nb_pkts = (uint16_t)RTE_MIN(nb_pkts, rxq->rx_nb_avail);

	for (i = 0; i < nb_pkts; i++)
		rx_pkts[i] = stage[i];

	rxq->rx_nb_avail = (uint16_t)(rxq->rx_nb_avail - nb_pkts);
	rxq->rx_next_avail = (uint16_t)(rxq->rx_next_avail + nb_pkts);

	return nb_pkts;
}

static inline int
i40e_rx_alloc_bufs(struct i40e_rx_queue *rxq)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx, i;
	uint64_t dma_addr;
	int diag;

	/* Allocate buffers in bulk */
	alloc_idx = (uint16_t)(rxq->rx_free_trigger -
				(rxq->rx_free_thresh - 1));
	rxep = &(rxq->sw_ring[alloc_idx]);
	diag = rte_mempool_get_bulk(rxq->mp, (void *)rxep,
					rxq->rx_free_thresh);
	if (unlikely(diag != 0)) {
		PMD_DRV_LOG(ERR, "Failed to get mbufs in bulk\n");
		return -ENOMEM;
	}

	rxdp = &rxq->rx_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; i++) {
		mb = rxep[i].mbuf;
		rte_mbuf_refcnt_set(mb, 1);
		mb->type = RTE_MBUF_PKT;
		mb->pkt.next = NULL;
		mb->pkt.data = (char *)mb->buf_addr + RTE_PKTMBUF_HEADROOM;
		mb->pkt.nb_segs = 1;
		mb->pkt.in_port = rxq->port_id;
		dma_addr = rte_cpu_to_le_64(\
			RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mb));
		rxdp[i].read.hdr_addr = dma_addr;
		rxdp[i].read.pkt_addr = dma_addr;
	}

	/* Update rx tail regsiter */
	rte_wmb();
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->rx_free_trigger);

	rxq->rx_free_trigger =
		(uint16_t)(rxq->rx_free_trigger + rxq->rx_free_thresh);
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	return 0;
}

static inline uint16_t
rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq = (struct i40e_rx_queue *)rx_queue;
	uint16_t nb_rx = 0;

	if (!nb_pkts)
		return 0;

	if (rxq->rx_nb_avail)
		return i40e_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	nb_rx = (uint16_t)i40e_rx_scan_hw_ring(rxq);
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	if (rxq->rx_tail > rxq->rx_free_trigger) {
		if (i40e_rx_alloc_bufs(rxq) != 0) {
			uint16_t i, j;

			PMD_RX_LOG(DEBUG, "Rx mbuf alloc failed for "
					"port_id=%u, queue_id=%u\n",
					rxq->port_id, rxq->queue_id);
			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; i++, j++)
				rxq->sw_ring[j].mbuf = rxq->rx_stage[i];

			return 0;
		}
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	if (rxq->rx_nb_avail)
		return i40e_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

static uint16_t
i40e_recv_pkts_bulk_alloc(void *rx_queue,
			  struct rte_mbuf **rx_pkts,
			  uint16_t nb_pkts)
{
	uint16_t nb_rx = 0, n, count;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= RTE_PMD_I40E_RX_MAX_BURST))
		return rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	while (nb_pkts) {
		n = RTE_MIN(nb_pkts, RTE_PMD_I40E_RX_MAX_BURST);
		count = rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + count);
		nb_pkts = (uint16_t)(nb_pkts - count);
		if (count < n)
			break;
	}

	return nb_rx;
}
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */

uint16_t
i40e_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq;
	volatile union i40e_rx_desc *rx_ring;
	volatile union i40e_rx_desc *rxdp;
	union i40e_rx_desc rxd;
	struct i40e_rx_entry *sw_ring;
	struct i40e_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	uint16_t nb_rx;
	uint32_t rx_status;
	uint64_t qword1;
	uint16_t rx_packet_len;
	uint16_t rx_id, nb_hold;
	uint64_t dma_addr;
	uint16_t pkt_flags;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
		rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK)
				>> I40E_RXD_QW1_STATUS_SHIFT;
		/* Check the DD bit first */
		if (!(rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
			break;

		nmb = rte_rxmbuf_alloc(rxq->mp);
		if (unlikely(!nmb))
			break;
		rxd = *rxdp;

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(sw_ring[rx_id].mbuf);

		/**
		 * When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR_DEFAULT(nmb));
		rxdp->read.hdr_addr = dma_addr;
		rxdp->read.pkt_addr = dma_addr;

		rx_packet_len = ((qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
				I40E_RXD_QW1_LENGTH_PBUF_SHIFT) - rxq->crc_len;

		rxm->pkt.data = (char *)rxm->buf_addr + RTE_PKTMBUF_HEADROOM;
		rte_prefetch0(rxm->pkt.data);
		rxm->pkt.nb_segs = 1;
		rxm->pkt.next = NULL;
		rxm->pkt.pkt_len = rx_packet_len;
		rxm->pkt.data_len = rx_packet_len;
		rxm->pkt.in_port = rxq->port_id;

		rxm->pkt.vlan_macip.f.vlan_tci = rx_status &
			(1 << I40E_RX_DESC_STATUS_L2TAG1P_SHIFT) ?
			rte_le_to_cpu_16(rxd.wb.qword0.lo_dword.l2tag1) : 0;
		pkt_flags = i40e_rxd_status_to_pkt_flags(qword1);
		pkt_flags |= i40e_rxd_error_to_pkt_flags(qword1);
		pkt_flags |= i40e_rxd_ptype_to_pkt_flags(qword1);
		rxm->ol_flags = pkt_flags;
		if (pkt_flags & PKT_RX_RSS_HASH)
			rxm->pkt.hash.rss =
				rte_le_to_cpu_32(rxd.wb.qword0.hi_dword.rss);

		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	/**
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the receive tail register of queue.
	 * Update that register with the value of the last processed RX
	 * descriptor minus 1.
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rx_id = (uint16_t) ((rx_id == 0) ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		I40E_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

uint16_t
i40e_recv_scattered_pkts(void *rx_queue,
			 struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq = rx_queue;
	volatile union i40e_rx_desc *rx_ring = rxq->rx_ring;
	volatile union i40e_rx_desc *rxdp;
	union i40e_rx_desc rxd;
	struct i40e_rx_entry *sw_ring = rxq->sw_ring;
	struct i40e_rx_entry *rxe;
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg = rxq->pkt_last_seg;
	struct rte_mbuf *nmb, *rxm;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0, nb_hold = 0, rx_packet_len, pkt_flags;
	uint32_t rx_status;
	uint64_t qword1;
	uint64_t dma_addr;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
		rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK) >>
					I40E_RXD_QW1_STATUS_SHIFT;
		/* Check the DD bit */
		if (!(rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
			break;

		nmb = rte_rxmbuf_alloc(rxq->mp);
		if (unlikely(!nmb))
			break;
		rxd = *rxdp;
		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(sw_ring[rx_id].mbuf);

		/**
		 * When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR_DEFAULT(nmb));

		/* Set data buffer address and data length of the mbuf */
		rxdp->read.hdr_addr = dma_addr;
		rxdp->read.pkt_addr = dma_addr;
		rx_packet_len = (qword1 & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
					I40E_RXD_QW1_LENGTH_PBUF_SHIFT;
		rxm->pkt.data_len = rx_packet_len;
		rxm->pkt.data = (char *)rxm->buf_addr + RTE_PKTMBUF_HEADROOM;

		/**
		 * If this is the first buffer of the received packet, set the
		 * pointer to the first mbuf of the packet and initialize its
		 * context. Otherwise, update the total length and the number
		 * of segments of the current scattered packet, and update the
		 * pointer to the last mbuf of the current packet.
		 */
		if (!first_seg) {
			first_seg = rxm;
			first_seg->pkt.nb_segs = 1;
			first_seg->pkt.pkt_len = rx_packet_len;
		} else {
			first_seg->pkt.pkt_len =
				(uint16_t)(first_seg->pkt.pkt_len +
						rx_packet_len);
			first_seg->pkt.nb_segs++;
			last_seg->pkt.next = rxm;
		}

		/**
		 * If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (!(rx_status & (1 << I40E_RX_DESC_STATUS_EOF_SHIFT))) {
			last_seg = rxm;
			continue;
		}

		/**
		 * This is the last buffer of the received packet. If the CRC
		 * is not stripped by the hardware:
		 *  - Subtract the CRC length from the total packet length.
		 *  - If the last buffer only contains the whole CRC or a part
		 *  of it, free the mbuf associated to the last buffer. If part
		 *  of the CRC is also contained in the previous mbuf, subtract
		 *  the length of that CRC part from the data length of the
		 *  previous mbuf.
		 */
		rxm->pkt.next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt.pkt_len -= ETHER_CRC_LEN;
			if (rx_packet_len <= ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->pkt.nb_segs--;
				last_seg->pkt.data_len =
					(uint16_t)(last_seg->pkt.data_len -
					(ETHER_CRC_LEN - rx_packet_len));
				last_seg->pkt.next = NULL;
			} else
				rxm->pkt.data_len = (uint16_t)(rx_packet_len -
								ETHER_CRC_LEN);
		}

		first_seg->pkt.in_port = rxq->port_id;
		first_seg->pkt.vlan_macip.f.vlan_tci = (rx_status &
			(1 << I40E_RX_DESC_STATUS_L2TAG1P_SHIFT)) ?
			rte_le_to_cpu_16(rxd.wb.qword0.lo_dword.l2tag1) : 0;
		pkt_flags = i40e_rxd_status_to_pkt_flags(qword1);
		pkt_flags |= i40e_rxd_error_to_pkt_flags(qword1);
		pkt_flags |= i40e_rxd_ptype_to_pkt_flags(qword1);
		first_seg->ol_flags = pkt_flags;
		if (pkt_flags & PKT_RX_RSS_HASH)
			rxm->pkt.hash.rss =
				rte_le_to_cpu_32(rxd.wb.qword0.hi_dword.rss);

		/* Prefetch data of first segment, if configured to do so. */
		rte_prefetch0(first_seg->pkt.data);
		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
	}

	/* Record index of the next RX descriptor to probe. */
	rxq->rx_tail = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	/**
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register. Update the RDT with the value of the last processed RX
	 * descriptor minus 1, to guarantee that the RDT register is never
	 * equal to the RDH register, which creates a "full" ring situtation
	 * from the hardware point of view.
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rx_id = (uint16_t)(rx_id == 0 ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		I40E_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

/* Check if the context descriptor is needed for TX offloading */
static inline uint16_t
i40e_calc_context_desc(uint16_t flags)
{
	uint16_t mask = 0;

#ifdef RTE_LIBRTE_IEEE1588
	mask |= PKT_TX_IEEE1588_TMST;
#endif
	if (flags & mask)
		return 1;

	return 0;
}

uint16_t
i40e_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct i40e_tx_queue *txq;
	struct i40e_tx_entry *sw_ring;
	struct i40e_tx_entry *txe, *txn;
	volatile struct i40e_tx_desc *txd;
	volatile struct i40e_tx_desc *txr;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint16_t tx_id;
	uint16_t nb_tx;
	uint32_t td_cmd;
	uint32_t td_offset;
	uint32_t tx_flags;
	uint32_t td_tag;
	uint16_t ol_flags;
	uint8_t l2_len;
	uint8_t l3_len;
	uint16_t nb_used;
	uint16_t nb_ctx;
	uint16_t tx_last;
	uint16_t slen;
	uint64_t buf_dma_addr;

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr = txq->tx_ring;
	tx_id = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Check if the descriptor ring needs to be cleaned. */
	if ((txq->nb_tx_desc - txq->nb_tx_free) > txq->tx_free_thresh)
		i40e_xmit_cleanup(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		td_cmd = 0;
		td_tag = 0;
		td_offset = 0;
		tx_flags = 0;

		tx_pkt = *tx_pkts++;
		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		ol_flags = tx_pkt->ol_flags;
		l2_len = tx_pkt->pkt.vlan_macip.f.l2_len;
		l3_len = tx_pkt->pkt.vlan_macip.f.l3_len;

		/* Calculate the number of context descriptors needed. */
		nb_ctx = i40e_calc_context_desc(ol_flags);

		/**
		 * The number of descriptors that must be allocated for
		 * a packet equals to the number of the segments of that
		 * packet plus 1 context descriptor if needed.
		 */
		nb_used = (uint16_t)(tx_pkt->pkt.nb_segs + nb_ctx);
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		if (nb_used > txq->nb_tx_free) {
			if (i40e_xmit_cleanup(txq) != 0) {
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
			if (unlikely(nb_used > txq->tx_rs_thresh)) {
				while (nb_used > txq->nb_tx_free) {
					if (i40e_xmit_cleanup(txq) != 0) {
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		/* Descriptor based VLAN insertion */
		if (ol_flags & PKT_TX_VLAN_PKT) {
			tx_flags |= tx_pkt->pkt.vlan_macip.f.vlan_tci <<
						I40E_TX_FLAG_L2TAG1_SHIFT;
			tx_flags |= I40E_TX_FLAG_INSERT_VLAN;
			td_cmd |= I40E_TX_DESC_CMD_IL2TAG1;
			td_tag = (tx_flags & I40E_TX_FLAG_L2TAG1_MASK) >>
						I40E_TX_FLAG_L2TAG1_SHIFT;
		}

		/* Always enable CRC offload insertion */
		td_cmd |= I40E_TX_DESC_CMD_ICRC;

		/* Enable checksum offloading */
		i40e_txd_enable_checksum(ol_flags, &td_cmd, &td_offset,
							l2_len, l3_len);

		if (unlikely(nb_ctx)) {
			/* Setup TX context descriptor if required */
			volatile struct i40e_tx_context_desc *ctx_txd =
				(volatile struct i40e_tx_context_desc *)\
							&txr[tx_id];
			uint32_t cd_tunneling_params = 0;
			uint16_t cd_l2tag2 = 0;
			uint64_t cd_type_cmd_tso_mss =
				I40E_TX_DESC_DTYPE_CONTEXT;

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf != NULL) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}
#ifdef RTE_LIBRTE_IEEE1588
			if (ol_flags & PKT_TX_IEEE1588_TMST)
				cd_type_cmd_tso_mss |=
					((uint64_t)I40E_TX_CTX_DESC_TSYN <<
						I40E_TXD_CTX_QW1_CMD_SHIFT);
#endif
			ctx_txd->tunneling_params =
				rte_cpu_to_le_32(cd_tunneling_params);
			ctx_txd->l2tag2 = rte_cpu_to_le_16(cd_l2tag2);
			ctx_txd->type_cmd_tso_mss =
				rte_cpu_to_le_64(cd_type_cmd_tso_mss);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
		}

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Setup TX Descriptor */
			slen = m_seg->pkt.data_len;
			buf_dma_addr = RTE_MBUF_DATA_DMA_ADDR(m_seg);
			txd->buffer_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->cmd_type_offset_bsz = i40e_build_ctob(td_cmd,
						td_offset, slen, td_tag);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->pkt.next;
		} while (m_seg != NULL);

		/* The last packet data descriptor needs End Of Packet (EOP) */
		td_cmd |= I40E_TX_DESC_CMD_EOP;
		txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		if (txq->nb_tx_used >= txq->tx_rs_thresh) {
			PMD_TX_FREE_LOG(DEBUG,
					"Setting RS bit on TXD id="
					"%4u (port=%d queue=%d)",
					tx_last, txq->port_id, txq->queue_id);

			td_cmd |= I40E_TX_DESC_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_tx_used = 0;
		}

		txd->cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)td_cmd) <<
					I40E_TXD_QW1_CMD_SHIFT);
	}

end_of_tx:
	rte_wmb();

	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (unsigned) txq->port_id, (unsigned) txq->queue_id,
		   (unsigned) tx_id, (unsigned) nb_tx);

	I40E_PCI_REG_WRITE(txq->qtx_tail, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

static inline int __attribute__((always_inline))
i40e_tx_free_bufs(struct i40e_tx_queue *txq)
{
	struct i40e_tx_entry *txep;
	uint16_t i;

	if (!(txq->tx_ring[txq->tx_next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE)))
		return 0;

	txep = &(txq->sw_ring[txq->tx_next_dd - (txq->tx_rs_thresh - 1)]);

	for (i = 0; i < txq->tx_rs_thresh; i++)
		rte_prefetch0((txep + i)->mbuf);

	if (!(txq->txq_flags & (uint32_t)ETH_TXQ_FLAGS_NOREFCOUNT)) {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_mempool_put(txep->mbuf->pool, txep->mbuf);
			txep->mbuf = NULL;
		}
	} else {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_pktmbuf_free_seg(txep->mbuf);
			txep->mbuf = NULL;
		}
	}

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

#define I40E_TD_CMD (I40E_TX_DESC_CMD_ICRC |\
		     I40E_TX_DESC_CMD_EOP)

/* Populate 4 descriptors with data from 4 mbufs */
static inline void
tx4(volatile struct i40e_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t dma_addr;
	uint32_t i;

	for (i = 0; i < 4; i++, txdp++, pkts++) {
		dma_addr = RTE_MBUF_DATA_DMA_ADDR(*pkts);
		txdp->buffer_addr = rte_cpu_to_le_64(dma_addr);
		txdp->cmd_type_offset_bsz =
			i40e_build_ctob((uint32_t)I40E_TD_CMD, 0,
					(*pkts)->pkt.data_len, 0);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile struct i40e_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t dma_addr;

	dma_addr = RTE_MBUF_DATA_DMA_ADDR(*pkts);
	txdp->buffer_addr = rte_cpu_to_le_64(dma_addr);
	txdp->cmd_type_offset_bsz =
		i40e_build_ctob((uint32_t)I40E_TD_CMD, 0,
				(*pkts)->pkt.data_len, 0);
}

/* Fill hardware descriptor ring with mbuf data */
static inline void
i40e_tx_fill_hw_ring(struct i40e_tx_queue *txq,
		     struct rte_mbuf **pkts,
		     uint16_t nb_pkts)
{
	volatile struct i40e_tx_desc *txdp = &(txq->tx_ring[txq->tx_tail]);
	struct i40e_tx_entry *txep = &(txq->sw_ring[txq->tx_tail]);
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP - 1;
	int mainpart, leftover;
	int i, j;

	mainpart = (nb_pkts & ((uint32_t) ~N_PER_LOOP_MASK));
	leftover = (nb_pkts & ((uint32_t)  N_PER_LOOP_MASK));
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		for (j = 0; j < N_PER_LOOP; ++j) {
			(txep + i + j)->mbuf = *(pkts + i + j);
		}
		tx4(txdp + i, pkts + i);
	}
	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			(txep + mainpart + i)->mbuf = *(pkts + mainpart + i);
			tx1(txdp + mainpart + i, pkts + mainpart + i);
		}
	}
}

static inline uint16_t
tx_xmit_pkts(struct i40e_tx_queue *txq,
	     struct rte_mbuf **tx_pkts,
	     uint16_t nb_pkts)
{
	volatile struct i40e_tx_desc *txr = txq->tx_ring;
	uint16_t n = 0;

	/**
	 * Begin scanning the H/W ring for done descriptors when the number
	 * of available descriptors drops below tx_free_thresh. For each done
	 * descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		i40e_tx_free_bufs(txq);

	/* Use available descriptor only */
	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(!nb_pkts))
		return 0;

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);
	if ((txq->tx_tail + nb_pkts) > txq->nb_tx_desc) {
		n = (uint16_t)(txq->nb_tx_desc - txq->tx_tail);
		i40e_tx_fill_hw_ring(txq, tx_pkts, n);
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
		txq->tx_tail = 0;
	}

	/* Fill hardware descriptor ring with mbuf data */
	i40e_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/* Determin if RS bit needs to be set */
	if (txq->tx_tail > txq->tx_next_rs) {
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
		if (txq->tx_next_rs >= txq->nb_tx_desc)
			txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
	}

	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	/* Update the tx tail register */
	rte_wmb();
	I40E_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static uint16_t
i40e_xmit_pkts_simple(void *tx_queue,
		      struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;

	if (likely(nb_pkts <= I40E_TX_MAX_BURST))
		return tx_xmit_pkts((struct i40e_tx_queue *)tx_queue,
						tx_pkts, nb_pkts);

	while (nb_pkts) {
		uint16_t ret, num = (uint16_t)RTE_MIN(nb_pkts,
						I40E_TX_MAX_BURST);

		ret = tx_xmit_pkts((struct i40e_tx_queue *)tx_queue,
						&tx_pkts[nb_tx], num);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < num)
			break;
	}

	return nb_tx;
}

int
i40e_dev_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct i40e_vsi *vsi = I40E_DEV_PRIVATE_TO_VSI(dev->data->dev_private);
	struct i40e_rx_queue *rxq;
	const struct rte_memzone *rz;
	uint32_t ring_size;
	uint16_t len;
	int use_def_burst_func = 1;

	if (!vsi || queue_idx >= vsi->nb_qps) {
		PMD_DRV_LOG(ERR, "VSI not available or queue "
				"index exceeds the maximum\n");
		return I40E_ERR_PARAM;
	}
	if (((nb_desc * sizeof(union i40e_rx_desc)) % I40E_ALIGN) != 0 ||
					(nb_desc > I40E_MAX_RING_DESC) ||
					(nb_desc < I40E_MIN_RING_DESC)) {
		PMD_DRV_LOG(ERR, "Number (%u) of receive descriptors is "
						"invalid\n", nb_desc);
		return I40E_ERR_PARAM;
	}

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		i40e_dev_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("i40e rx queue",
				 sizeof(struct i40e_rx_queue),
				 CACHE_LINE_SIZE,
				 socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
					"rx queue data structure\n");
		return (-ENOMEM);
	}
	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = vsi->base_queue + queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->crc_len = (uint8_t) ((dev->data->dev_conf.rxmode.hw_strip_crc) ?
							0 : ETHER_CRC_LEN);
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->vsi = vsi;

	/* Allocate the maximun number of RX ring hardware descriptor. */
	ring_size = sizeof(union i40e_rx_desc) * I40E_MAX_RING_DESC;
	ring_size = RTE_ALIGN(ring_size, I40E_DMA_MEM_ALIGN);
	rz = i40e_ring_dma_zone_reserve(dev,
					"rx_ring",
					queue_idx,
					ring_size,
					socket_id);
	if (!rz) {
		i40e_dev_rx_queue_release(rxq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX\n");
		return (-ENOMEM);
	}

	/* Zero all the descriptors in the ring. */
	memset(rz->addr, 0, ring_size);

#ifdef RTE_LIBRTE_XEN_DOM0
	rxq->rx_ring_phys_addr = rte_mem_phy2mch(rz->memseg_id, rz->phys_addr);
#else
	rxq->rx_ring_phys_addr = (uint64_t)rz->phys_addr;
#endif

	rxq->rx_ring = (union i40e_rx_desc *)rz->addr;

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	len = (uint16_t)(nb_desc + RTE_PMD_I40E_RX_MAX_BURST);
#else
	len = nb_desc;
#endif

	/* Allocate the software ring. */
	rxq->sw_ring =
		rte_zmalloc_socket("i40e rx sw ring",
				   sizeof(struct i40e_rx_entry) * len,
				   CACHE_LINE_SIZE,
				   socket_id);
	if (!rxq->sw_ring) {
		i40e_dev_rx_queue_release(rxq);
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW ring\n");
		return (-ENOMEM);
	}

	i40e_reset_rx_queue(rxq);
	rxq->q_set = TRUE;
	dev->data->rx_queues[queue_idx] = rxq;

	use_def_burst_func = check_rx_burst_bulk_alloc_preconditions(rxq);

	if (!use_def_burst_func && !dev->data->scattered_rx) {
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			"satisfied. Rx Burst Bulk Alloc function will be "
					"used on port=%d, queue=%d.\n",
					rxq->port_id, rxq->queue_id);
		dev->rx_pkt_burst = i40e_recv_pkts_bulk_alloc;
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
				"not satisfied, Scattered Rx is requested, "
				"or RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC is "
					"not enabled on port=%d, queue=%d.\n",
						rxq->port_id, rxq->queue_id);
	}

	return 0;
}

void
i40e_dev_rx_queue_release(void *rxq)
{
	struct i40e_rx_queue *q = (struct i40e_rx_queue *)rxq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL\n");
		return;
	}

	i40e_rx_queue_release_mbufs(q);
	rte_free(q->sw_ring);
	rte_free(q);
}

uint32_t
i40e_dev_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
#define I40E_RXQ_SCAN_INTERVAL 4
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_queue *rxq;
	uint16_t desc = 0;

	if (unlikely(rx_queue_id >= dev->data->nb_rx_queues)) {
		PMD_DRV_LOG(ERR, "Invalid RX queue id %u\n", rx_queue_id);
		return 0;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	rxdp = &(rxq->rx_ring[rxq->rx_tail]);
	while ((desc < rxq->nb_rx_desc) &&
		((rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len) &
		I40E_RXD_QW1_STATUS_MASK) >> I40E_RXD_QW1_STATUS_SHIFT) &
				(1 << I40E_RX_DESC_STATUS_DD_SHIFT)) {
		/**
		 * Check the DD bit of a rx descriptor of each 4 in a group,
		 * to avoid checking too frequently and downgrading performance
		 * too much.
		 */
		desc += I40E_RXQ_SCAN_INTERVAL;
		rxdp += I40E_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
					desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
i40e_dev_rx_descriptor_done(void *rx_queue, uint16_t offset)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_queue *rxq = rx_queue;
	uint16_t desc;
	int ret;

	if (unlikely(offset >= rxq->nb_rx_desc)) {
		PMD_DRV_LOG(ERR, "Invalid RX queue id %u\n", offset);
		return 0;
	}

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	rxdp = &(rxq->rx_ring[desc]);

	ret = !!(((rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len) &
		I40E_RXD_QW1_STATUS_MASK) >> I40E_RXD_QW1_STATUS_SHIFT) &
				(1 << I40E_RX_DESC_STATUS_DD_SHIFT));

	return ret;
}

int
i40e_dev_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_txconf *tx_conf)
{
	struct i40e_vsi *vsi = I40E_DEV_PRIVATE_TO_VSI(dev->data->dev_private);
	struct i40e_tx_queue *txq;
	const struct rte_memzone *tz;
	uint32_t ring_size;
	uint16_t tx_rs_thresh, tx_free_thresh;

	if (!vsi || queue_idx >= vsi->nb_qps) {
		PMD_DRV_LOG(ERR, "VSI is NULL, or queue index (%u) "
				"exceeds the maximum\n", queue_idx);
		return I40E_ERR_PARAM;
	}

	if (((nb_desc * sizeof(struct i40e_tx_desc)) % I40E_ALIGN) != 0 ||
					(nb_desc > I40E_MAX_RING_DESC) ||
					(nb_desc < I40E_MIN_RING_DESC)) {
		PMD_DRV_LOG(ERR, "Number (%u) of transmit descriptors is "
                                                "invalid\n", nb_desc);
		return I40E_ERR_PARAM;
	}

	/**
	 * The following two parameters control the setting of the RS bit on
	 * transmit descriptors. TX descriptors will have their RS bit set
	 * after txq->tx_rs_thresh descriptors have been used. The TX
	 * descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required to
	 * transmit a packet is greater than the number of free TX descriptors.
	 *
	 * The following constraints must be satisfied:
	 *  - tx_rs_thresh must be greater than 0.
	 *  - tx_rs_thresh must be less than the size of the ring minus 2.
	 *  - tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  - tx_rs_thresh must be a divisor of the ring size.
	 *  - tx_free_thresh must be greater than 0.
	 *  - tx_free_thresh must be less than the size of the ring minus 3.
	 *
	 * One descriptor in the TX ring is used as a sentinel to avoid a H/W
	 * race condition, hence the maximum threshold constraints. When set
	 * to zero use default values.
	 */
	tx_rs_thresh = (uint16_t)((tx_conf->tx_rs_thresh) ?
		tx_conf->tx_rs_thresh : DEFAULT_TX_RS_THRESH);
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
		tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	if (tx_rs_thresh >= (nb_desc - 2)) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be less than the "
				"number of TX descriptors minus 2. "
				"(tx_rs_thresh=%u port=%d queue=%d)\n",
					(unsigned int)tx_rs_thresh,
					(int)dev->data->port_id,
						(int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be less than the "
				"tx_free_thresh must be less than the "
				"number of TX descriptors minus 3. "
				"(tx_free_thresh=%u port=%d queue=%d)\n",
					(unsigned int)tx_free_thresh,
						(int)dev->data->port_id,
							(int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be less than or "
				"equal to tx_free_thresh. (tx_free_thresh=%u"
				" tx_rs_thresh=%u port=%d queue=%d)\n",
						(unsigned int)tx_free_thresh,
						(unsigned int)tx_rs_thresh,
						(int)dev->data->port_id,
							(int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be a divisor of the "
				"number of TX descriptors. (tx_rs_thresh=%u"
						" port=%d queue=%d)\n",
						(unsigned int)tx_rs_thresh,
						(int)dev->data->port_id,
							(int)queue_idx);
		return I40E_ERR_PARAM;
	}
	if ((tx_rs_thresh > 1) && (tx_conf->tx_thresh.wthresh != 0)) {
		RTE_LOG(ERR, PMD, "TX WTHRESH must be set to 0 if "
				"tx_rs_thresh is greater than 1. "
				"(tx_rs_thresh=%u port=%d queue=%d)\n",
					(unsigned int)tx_rs_thresh,
					(int)dev->data->port_id,
						(int)queue_idx);
		return I40E_ERR_PARAM;
	}

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx]) {
		i40e_dev_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("i40e tx queue",
				  sizeof(struct i40e_tx_queue),
				  CACHE_LINE_SIZE,
				  socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
					"tx queue structure\n");
		return (-ENOMEM);
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct i40e_tx_desc) * I40E_MAX_RING_DESC;
	ring_size = RTE_ALIGN(ring_size, I40E_DMA_MEM_ALIGN);
	tz = i40e_ring_dma_zone_reserve(dev,
					"tx_ring",
					queue_idx,
					ring_size,
					socket_id);
	if (!tz) {
		i40e_dev_tx_queue_release(txq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX\n");
		return (-ENOMEM);
	}

	txq->nb_tx_desc = nb_desc;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->tx_free_thresh = tx_free_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;
	txq->reg_idx = vsi->base_queue + queue_idx;
	txq->port_id = dev->data->port_id;
	txq->txq_flags = tx_conf->txq_flags;
	txq->vsi = vsi;

#ifdef RTE_LIBRTE_XEN_DOM0
	txq->tx_ring_phys_addr = rte_mem_phy2mch(tz->memseg_id, tz->phys_addr);
#else
	txq->tx_ring_phys_addr = (uint64_t)tz->phys_addr;
#endif
	txq->tx_ring = (struct i40e_tx_desc *)tz->addr;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket("i40e tx sw ring",
				   sizeof(struct i40e_tx_entry) * nb_desc,
				   CACHE_LINE_SIZE,
				   socket_id);
	if (!txq->sw_ring) {
		i40e_dev_tx_queue_release(txq);
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW TX ring\n");
		return (-ENOMEM);
	}

	i40e_reset_tx_queue(txq);
	txq->q_set = TRUE;
	dev->data->tx_queues[queue_idx] = txq;

	/* Use a simple TX queue without offloads or multi segs if possible */
	if (((txq->txq_flags & I40E_SIMPLE_FLAGS) == I40E_SIMPLE_FLAGS) &&
				(txq->tx_rs_thresh >= I40E_TX_MAX_BURST)) {
		PMD_INIT_LOG(INFO, "Using simple tx path\n");
		dev->tx_pkt_burst = i40e_xmit_pkts_simple;
	} else {
		PMD_INIT_LOG(INFO, "Using full-featured tx path\n");
		dev->tx_pkt_burst = i40e_xmit_pkts;
	}

	return 0;
}

void
i40e_dev_tx_queue_release(void *txq)
{
	struct i40e_tx_queue *q = (struct i40e_tx_queue *)txq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to TX queue is NULL\n");
		return;
	}

	i40e_tx_queue_release_mbufs(q);
	rte_free(q->sw_ring);
	rte_free(q);
}

static const struct rte_memzone *
i40e_ring_dma_zone_reserve(struct rte_eth_dev *dev,
			   const char *ring_name,
			   uint16_t queue_id,
			   uint32_t ring_size,
			   int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
			dev->driver->pci_drv.name, ring_name,
				dev->data->port_id, queue_id);
	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

#ifdef RTE_LIBRTE_XEN_DOM0
	return rte_memzone_reserve_bounded(z_name, ring_size,
		socket_id, 0, I40E_ALIGN, RTE_PGSIZE_2M);
#else
	return rte_memzone_reserve_aligned(z_name, ring_size,
				socket_id, 0, I40E_ALIGN);
#endif
}

void
i40e_rx_queue_release_mbufs(struct i40e_rx_queue *rxq)
{
	uint16_t i;

	if (!rxq || !rxq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq or sw_ring is NULL\n");
		return;
	}

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
			rxq->sw_ring[i].mbuf = NULL;
		}
	}
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	if (rxq->rx_nb_avail == 0)
		return;
	for (i = 0; i < rxq->rx_nb_avail; i++) {
		struct rte_mbuf *mbuf;

		mbuf = rxq->rx_stage[rxq->rx_next_avail + i];
		rte_pktmbuf_free_seg(mbuf);
	}
	rxq->rx_nb_avail = 0;
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
}

static void
i40e_reset_rx_queue(struct i40e_rx_queue *rxq)
{
	unsigned i;
	uint16_t len;

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	if (check_rx_burst_bulk_alloc_preconditions(rxq) == 0)
		len = (uint16_t)(rxq->nb_rx_desc + RTE_PMD_I40E_RX_MAX_BURST);
	else
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
		len = rxq->nb_rx_desc;

	for (i = 0; i < len * sizeof(union i40e_rx_desc); i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = 0; i < RTE_PMD_I40E_RX_MAX_BURST; ++i)
		rxq->sw_ring[rxq->nb_rx_desc + i].mbuf = &rxq->fake_mbuf;

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
#endif /* RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC */
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

static void
i40e_tx_queue_release_mbufs(struct i40e_tx_queue *txq)
{
	uint16_t i;

	if (!txq || !txq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq or sw_ring is NULL\n");
		return;
	}

	for (i = 0; i < txq->nb_tx_desc; i++) {
		if (txq->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
	}
}

static void
i40e_reset_tx_queue(struct i40e_tx_queue *txq)
{
	struct i40e_tx_entry *txe;
	uint16_t i, prev, size;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL\n");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct i40e_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile struct i40e_tx_desc *txd = &txq->tx_ring[i];

		txd[i].cmd_type_offset_bsz =
			rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE);
		txe[i].mbuf =  NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;

	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
}

/* Init the TX queue in hardware */
int
i40e_tx_queue_init(struct i40e_tx_queue *txq)
{
	enum i40e_status_code err = I40E_SUCCESS;
	struct i40e_vsi *vsi = txq->vsi;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t pf_q = txq->reg_idx;
	struct i40e_hmc_obj_txq tx_ctx;
	uint32_t qtx_ctl;

	/* clear the context structure first */
	memset(&tx_ctx, 0, sizeof(tx_ctx));
	tx_ctx.new_context = 1;
	tx_ctx.base = txq->tx_ring_phys_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->nb_tx_desc;
	tx_ctx.rdylist = rte_le_to_cpu_16(vsi->info.qs_handle[0]);

	err = i40e_clear_lan_tx_queue_context(hw, pf_q);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failure of clean lan tx queue context\n");
		return err;
	}

	err = i40e_set_lan_tx_queue_context(hw, pf_q, &tx_ctx);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failure of set lan tx queue context\n");
		return err;
	}

	/* Now associate this queue with this PCI function */
	qtx_ctl = I40E_QTX_CTL_PF_QUEUE;
	qtx_ctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
					I40E_QTX_CTL_PF_INDX_MASK);
	I40E_WRITE_REG(hw, I40E_QTX_CTL(pf_q), qtx_ctl);
	I40E_WRITE_FLUSH(hw);

	txq->qtx_tail = hw->hw_addr + I40E_QTX_TAIL(pf_q);

	return err;
}

int
i40e_alloc_rx_queue_mbufs(struct i40e_rx_queue *rxq)
{
	struct i40e_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union i40e_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_rxmbuf_alloc(rxq->mp);

		if (unlikely(!mbuf)) {
			PMD_DRV_LOG(ERR, "Failed to allocate mbuf for RX\n");
			return -ENOMEM;
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->type = RTE_MBUF_PKT;
		mbuf->pkt.next = NULL;
		mbuf->pkt.data = (char *)mbuf->buf_addr + RTE_PKTMBUF_HEADROOM;
		mbuf->pkt.nb_segs = 1;
		mbuf->pkt.in_port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mbuf));

		rxd = &rxq->rx_ring[i];
		rxd->read.pkt_addr = dma_addr;
		rxd->read.hdr_addr = dma_addr;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif /* RTE_LIBRTE_I40E_16BYTE_RX_DESC */

		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/*
 * Calculate the buffer length, and check the jumbo frame
 * and maximum packet length.
 */
static int
i40e_rx_queue_config(struct i40e_rx_queue *rxq)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(rxq->vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(rxq->vsi);
	struct rte_eth_dev_data *data = pf->dev_data;
	struct rte_pktmbuf_pool_private *mbp_priv =
			rte_mempool_get_priv(rxq->mp);
	uint16_t buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size -
						RTE_PKTMBUF_HEADROOM);
	uint16_t len;

	switch (pf->flags & (I40E_FLAG_HEADER_SPLIT_DISABLED |
			I40E_FLAG_HEADER_SPLIT_ENABLED)) {
	case I40E_FLAG_HEADER_SPLIT_ENABLED: /* Not supported */
		rxq->rx_hdr_len = RTE_ALIGN(I40E_RXBUF_SZ_1024,
				(1 << I40E_RXQ_CTX_HBUFF_SHIFT));
		rxq->rx_buf_len = RTE_ALIGN(I40E_RXBUF_SZ_2048,
				(1 << I40E_RXQ_CTX_DBUFF_SHIFT));
		rxq->hs_mode = i40e_header_split_enabled;
		break;
	case I40E_FLAG_HEADER_SPLIT_DISABLED:
	default:
		rxq->rx_hdr_len = 0;
		rxq->rx_buf_len = RTE_ALIGN(buf_size,
			(1 << I40E_RXQ_CTX_DBUFF_SHIFT));
		rxq->hs_mode = i40e_header_split_none;
		break;
	}

	len = hw->func_caps.rx_buf_chain_len * rxq->rx_buf_len;
	rxq->max_pkt_len = RTE_MIN(len, data->dev_conf.rxmode.max_rx_pkt_len);
	if (data->dev_conf.rxmode.jumbo_frame == 1) {
		if (rxq->max_pkt_len <= ETHER_MAX_LEN ||
			rxq->max_pkt_len > I40E_FRAME_SIZE_MAX) {
			PMD_DRV_LOG(ERR, "maximum packet length must "
				"be larger than %u and smaller than %u,"
					"as jumbo frame is enabled\n",
						(uint32_t)ETHER_MAX_LEN,
					(uint32_t)I40E_FRAME_SIZE_MAX);
			return I40E_ERR_CONFIG;
		}
	} else {
		if (rxq->max_pkt_len < ETHER_MIN_LEN ||
			rxq->max_pkt_len > ETHER_MAX_LEN) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
					"larger than %u and smaller than %u, "
					"as jumbo frame is disabled\n",
						(uint32_t)ETHER_MIN_LEN,
						(uint32_t)ETHER_MAX_LEN);
			return I40E_ERR_CONFIG;
		}
	}

	return 0;
}

/* Init the RX queue in hardware */
int
i40e_rx_queue_init(struct i40e_rx_queue *rxq)
{
	int err = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(rxq->vsi);
	struct rte_eth_dev_data *dev_data = I40E_VSI_TO_DEV_DATA(rxq->vsi);
	struct rte_eth_dev *dev = I40E_VSI_TO_ETH_DEV(rxq->vsi);
	uint16_t pf_q = rxq->reg_idx;
	uint16_t buf_size;
	struct i40e_hmc_obj_rxq rx_ctx;
	struct rte_pktmbuf_pool_private *mbp_priv;

	err = i40e_rx_queue_config(rxq);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Failed to config RX queue\n");
		return err;
	}

	/* Clear the context structure first */
	memset(&rx_ctx, 0, sizeof(struct i40e_hmc_obj_rxq));
	rx_ctx.dbuff = rxq->rx_buf_len >> I40E_RXQ_CTX_DBUFF_SHIFT;
	rx_ctx.hbuff = rxq->rx_hdr_len >> I40E_RXQ_CTX_HBUFF_SHIFT;

	rx_ctx.base = rxq->rx_ring_phys_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	rx_ctx.dsize = 1;
#endif
	rx_ctx.dtype = rxq->hs_mode;
	if (rxq->hs_mode)
		rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_ALL;
	else
		rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_NONE;
	rx_ctx.rxmax = rxq->max_pkt_len;
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	rx_ctx.lrxqthresh = 2;
	rx_ctx.crcstrip = (rxq->crc_len == 0) ? 1 : 0;
	rx_ctx.l2tsel = 1;
	rx_ctx.showiv = 1;
	rx_ctx.prefena = 1;

	err = i40e_clear_lan_rx_queue_context(hw, pf_q);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to clear LAN RX queue context\n");
		return err;
	}
	err = i40e_set_lan_rx_queue_context(hw, pf_q, &rx_ctx);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to set LAN RX queue context\n");
		return err;
	}

	rxq->qrx_tail = hw->hw_addr + I40E_QRX_TAIL(pf_q);
	err = i40e_alloc_rx_queue_mbufs(rxq);
	mbp_priv = rte_mempool_get_priv(rxq->mp);
	buf_size = (uint16_t)(mbp_priv->mbuf_data_room_size -
					RTE_PKTMBUF_HEADROOM);

	/* Check if scattered RX needs to be used. */
	if ((rxq->max_pkt_len + 2 * I40E_VLAN_TAG_SIZE) > buf_size) {
		dev_data->scattered_rx = 1;
		dev->rx_pkt_burst = i40e_recv_scattered_pkts;
	}

	rte_wmb();

	/* Init the RX tail regieter. */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, 0);
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	if (err)
		PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf\n");

	return err;
}

void
i40e_dev_clear_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		i40e_tx_queue_release_mbufs(dev->data->tx_queues[i]);
		i40e_reset_tx_queue(dev->data->tx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		i40e_rx_queue_release_mbufs(dev->data->rx_queues[i]);
		i40e_reset_rx_queue(dev->data->rx_queues[i]);
	}
}
