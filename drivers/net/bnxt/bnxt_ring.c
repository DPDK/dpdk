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

#include <rte_memzone.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_ring.h"
#include "bnxt_rxr.h"
#include "bnxt_txr.h"

#include "hsi_struct_def_dpdk.h"

/*
 * Generic ring handling
 */

void bnxt_free_ring(struct bnxt_ring *ring)
{
	if (ring->vmem_size && *ring->vmem) {
		memset((char *)*ring->vmem, 0, ring->vmem_size);
		*ring->vmem = NULL;
	}
	rte_memzone_free((const struct rte_memzone *)ring->mem_zone);
}

/*
 * Allocates a completion ring with vmem and stats optionally also allocating
 * a TX and/or RX ring.  Passing NULL as tx_ring_info and/or rx_ring_info
 * to not allocate them.
 *
 * Order in the allocation is:
 * stats - Always non-zero length
 * cp vmem - Always zero-length, supported for the bnxt_ring abstraction
 * tx vmem - Only non-zero length if tx_ring_info is not NULL
 * rx vmem - Only non-zero length if rx_ring_info is not NULL
 * cp bd ring - Always non-zero length
 * tx bd ring - Only non-zero length if tx_ring_info is not NULL
 * rx bd ring - Only non-zero length if rx_ring_info is not NULL
 */
int bnxt_alloc_rings(struct bnxt *bp, uint16_t qidx,
			    struct bnxt_tx_ring_info *tx_ring_info,
			    struct bnxt_rx_ring_info *rx_ring_info,
			    struct bnxt_cp_ring_info *cp_ring_info,
			    const char *suffix)
{
	struct bnxt_ring *cp_ring = cp_ring_info->cp_ring_struct;
	struct bnxt_ring *tx_ring;
	struct bnxt_ring *rx_ring;
	struct rte_pci_device *pdev = bp->pdev;
	const struct rte_memzone *mz = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];

	int stats_len = (tx_ring_info || rx_ring_info) ?
	    RTE_CACHE_LINE_ROUNDUP(sizeof(struct ctx_hw_stats64)) : 0;

	int cp_vmem_start = stats_len;
	int cp_vmem_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size);

	int tx_vmem_start = cp_vmem_start + cp_vmem_len;
	int tx_vmem_len =
	    tx_ring_info ? RTE_CACHE_LINE_ROUNDUP(tx_ring_info->
						tx_ring_struct->vmem_size) : 0;

	int rx_vmem_start = tx_vmem_start + tx_vmem_len;
	int rx_vmem_len = rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->
						rx_ring_struct->vmem_size) : 0;

	int cp_ring_start = rx_vmem_start + rx_vmem_len;
	int cp_ring_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->ring_size *
						 sizeof(struct cmpl_base));

	int tx_ring_start = cp_ring_start + cp_ring_len;
	int tx_ring_len = tx_ring_info ?
	    RTE_CACHE_LINE_ROUNDUP(tx_ring_info->tx_ring_struct->ring_size *
				   sizeof(struct tx_bd_long)) : 0;

	int rx_ring_start = tx_ring_start + tx_ring_len;
	int rx_ring_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->rx_ring_struct->ring_size *
		sizeof(struct rx_prod_pkt_bd)) : 0;

	int total_alloc_len = rx_ring_start + rx_ring_len;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_%04x:%02x:%02x:%02x-%04x_%s", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function, qidx,
		 suffix);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve(mz_name, total_alloc_len,
					 SOCKET_ID_ANY,
					 RTE_MEMZONE_2MB |
					 RTE_MEMZONE_SIZE_HINT_ONLY);
		if (mz == NULL)
			return -ENOMEM;
	}
	memset(mz->addr, 0, mz->len);

	if (tx_ring_info) {
		tx_ring = tx_ring_info->tx_ring_struct;

		tx_ring->bd = ((char *)mz->addr + tx_ring_start);
		tx_ring_info->tx_desc_ring = (struct tx_bd_long *)tx_ring->bd;
		tx_ring->bd_dma = mz->phys_addr + tx_ring_start;
		tx_ring_info->tx_desc_mapping = tx_ring->bd_dma;
		tx_ring->mem_zone = (const void *)mz;

		if (!tx_ring->bd)
			return -ENOMEM;
		if (tx_ring->vmem_size) {
			tx_ring->vmem =
			    (void **)((char *)mz->addr + tx_vmem_start);
			tx_ring_info->tx_buf_ring =
			    (struct bnxt_sw_tx_bd *)tx_ring->vmem;
		}
	}

	if (rx_ring_info) {
		rx_ring = rx_ring_info->rx_ring_struct;

		rx_ring->bd = ((char *)mz->addr + rx_ring_start);
		rx_ring_info->rx_desc_ring =
		    (struct rx_prod_pkt_bd *)rx_ring->bd;
		rx_ring->bd_dma = mz->phys_addr + rx_ring_start;
		rx_ring_info->rx_desc_mapping = rx_ring->bd_dma;
		rx_ring->mem_zone = (const void *)mz;

		if (!rx_ring->bd)
			return -ENOMEM;
		if (rx_ring->vmem_size) {
			rx_ring->vmem =
			    (void **)((char *)mz->addr + rx_vmem_start);
			rx_ring_info->rx_buf_ring =
			    (struct bnxt_sw_rx_bd *)rx_ring->vmem;
		}
	}

	cp_ring->bd = ((char *)mz->addr + cp_ring_start);
	cp_ring->bd_dma = mz->phys_addr + cp_ring_start;
	cp_ring_info->cp_desc_ring = cp_ring->bd;
	cp_ring_info->cp_desc_mapping = cp_ring->bd_dma;
	cp_ring->mem_zone = (const void *)mz;

	if (!cp_ring->bd)
		return -ENOMEM;
	if (cp_ring->vmem_size)
		*cp_ring->vmem = ((char *)mz->addr + stats_len);
	if (stats_len) {
		cp_ring_info->hw_stats = mz->addr;
		cp_ring_info->hw_stats_map = mz->phys_addr;
	}
	cp_ring_info->hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	return 0;
}
