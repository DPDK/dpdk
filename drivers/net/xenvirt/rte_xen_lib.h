/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#ifndef _RTE_XEN_DUMMY_PMD_H
#define _RTE_XEN_DUMMY_PMD_H

#include <stdint.h>

#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_ether.h>

#define	PAGEMAP_FNAME           "/proc/self/pagemap"
#define XEN_GNTALLOC_FNAME      "/dev/xen/gntalloc"
#define DPDK_XENSTORE_PATH      "/control/dpdk/"
#define DPDK_XENSTORE_NODE      "/control/dpdk"
/*format 0_mempool_gref = "1537,1524,1533" */
#define MEMPOOL_XENSTORE_STR    "_mempool_gref"
/*format 0_mempool_va = 0x80340000 */
#define MEMPOOL_VA_XENSTORE_STR "_mempool_va"
/*format 0_rx_vring_gref  = "1537,1524,1533" */
#define RXVRING_XENSTORE_STR    "_rx_vring_gref"
/*format 0_tx_vring_gref  = "1537,1524,1533" */
#define TXVRING_XENSTORE_STR    "_tx_vring_gref"
#define VRING_FLAG_STR          "_vring_flag"
/*format: event_type_start_0 = 1*/
#define EVENT_TYPE_START_STR    "event_type_start_"

#define DOM0_DOMID 0
/*
 * the pfn (page frame number) are bits 0-54 (see pagemap.txt in linux
 * Documentation).
 */
#define PAGEMAP_PFN_BITS	54
#define PAGEMAP_PFN_MASK	RTE_LEN2MASK(PAGEMAP_PFN_BITS, phys_addr_t)

#define MAP_FLAG	0xA5

#define RTE_ETH_XENVIRT_PAIRS_DELIM ';'
#define RTE_ETH_XENVIRT_KEY_VALUE_DELIM '='
#define RTE_ETH_XENVIRT_MAX_ARGS 1
#define RTE_ETH_XENVIRT_MAC_PARAM "mac"
struct xenvirt_dict {
	uint8_t addr_valid;
	struct ether_addr addr;
};

extern int gntalloc_fd;

int
gntalloc_open(void);

void
gntalloc_close(void);

void *
gntalloc(size_t sz, uint32_t *gref, uint64_t *start_index);

void
gntfree(void *va, size_t sz, uint64_t start_index);

int
xenstore_init(void);

int
xenstore_uninit(void);

int
xenstore_write(const char *key_str, const char *val_str);

int
get_phys_map(void *va, phys_addr_t pa[], uint32_t pg_num, uint32_t pg_sz);

void *
get_xen_virtual(size_t size, size_t page_sz);

int
grefwatch_from_alloc(uint32_t *gref, void **pptr);


int grant_node_create(uint32_t pg_num, uint32_t *gref_arr, phys_addr_t *pa_arr, char *val_str, size_t str_size);

int
grant_gntalloc_mbuf_pool(struct rte_mempool *mpool, uint32_t pg_num, uint32_t *gref_arr, phys_addr_t *pa_arr, int mempool_idx);

#endif
