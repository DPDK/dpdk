/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
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
 *     * Neither the name of Broadcom Corporation nor the names of its
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

#ifndef _BNXT_RING_H_
#define _BNXT_RING_H_

#include <inttypes.h>

#include <rte_memory.h>

#define RING_NEXT(ring, idx)		(((idx) + 1) & (ring)->ring_mask)

#define RTE_MBUF_DATA_DMA_ADDR(mb) \
	((uint64_t)((mb)->buf_physaddr + (mb)->data_off))

#define DB_IDX_MASK						0xffffff
#define DB_IDX_VALID						(0x1 << 26)
#define DB_IRQ_DIS						(0x1 << 27)
#define DB_KEY_TX						(0x0 << 28)
#define DB_KEY_RX						(0x1 << 28)
#define DB_KEY_CP						(0x2 << 28)
#define DB_KEY_ST						(0x3 << 28)
#define DB_KEY_TX_PUSH						(0x4 << 28)
#define DB_LONG_TX_PUSH						(0x2 << 24)

#define DEFAULT_CP_RING_SIZE	256
#define DEFAULT_RX_RING_SIZE	256
#define DEFAULT_TX_RING_SIZE	256

#define BNXT_TPA_MAX		64
#define AGG_RING_SIZE_FACTOR	2

/* These assume 4k pages */
#define MAX_RX_DESC_CNT (8 * 1024)
#define MAX_TX_DESC_CNT (4 * 1024)
#define MAX_CP_DESC_CNT (16 * 1024)

#define INVALID_HW_RING_ID      ((uint16_t)-1)
#define INVALID_STATS_CTX_ID		((uint16_t)-1)

struct bnxt_ring {
	void			*bd;
	phys_addr_t		bd_dma;
	uint32_t		ring_size;
	uint32_t		ring_mask;

	int			vmem_size;
	void			**vmem;

	uint16_t		fw_ring_id; /* Ring id filled by Chimp FW */
	const void		*mem_zone;
};

struct bnxt_ring_grp_info {
	uint16_t	fw_stats_ctx;
	uint16_t	fw_grp_id;
	uint16_t	rx_fw_ring_id;
	uint16_t	cp_fw_ring_id;
	uint16_t	ag_fw_ring_id;
};

struct bnxt;
struct bnxt_tx_ring_info;
struct bnxt_rx_ring_info;
struct bnxt_cp_ring_info;
void bnxt_free_ring(struct bnxt_ring *ring);
void bnxt_init_ring_grps(struct bnxt *bp);
int bnxt_alloc_rings(struct bnxt *bp, uint16_t qidx,
			    struct bnxt_tx_ring_info *tx_ring_info,
			    struct bnxt_rx_ring_info *rx_ring_info,
			    struct bnxt_cp_ring_info *cp_ring_info,
			    const char *suffix);
int bnxt_alloc_hwrm_rings(struct bnxt *bp);

#endif
