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

#ifndef _BNXT_FILTER_H_
#define _BNXT_FILTER_H_

#include <rte_ether.h>

struct bnxt;
struct bnxt_filter_info {
	STAILQ_ENTRY(bnxt_filter_info)	next;
	uint64_t		fw_l2_filter_id;
#define INVALID_MAC_INDEX	((uint16_t)-1)
	uint16_t		mac_index;

	/* Filter Characteristics */
	uint32_t		flags;
	uint32_t		enables;
	uint8_t			l2_addr[ETHER_ADDR_LEN];
	uint8_t			l2_addr_mask[ETHER_ADDR_LEN];
	uint16_t		l2_ovlan;
	uint16_t		l2_ovlan_mask;
	uint16_t		l2_ivlan;
	uint16_t		l2_ivlan_mask;
	uint8_t			t_l2_addr[ETHER_ADDR_LEN];
	uint8_t			t_l2_addr_mask[ETHER_ADDR_LEN];
	uint16_t		t_l2_ovlan;
	uint16_t		t_l2_ovlan_mask;
	uint16_t		t_l2_ivlan;
	uint16_t		t_l2_ivlan_mask;
	uint8_t			tunnel_type;
	uint16_t		mirror_vnic_id;
	uint32_t		vni;
	uint8_t			pri_hint;
	uint64_t		l2_filter_id_hint;
};

struct bnxt_filter_info *bnxt_alloc_filter(struct bnxt *bp);
void bnxt_init_filters(struct bnxt *bp);
void bnxt_free_all_filters(struct bnxt *bp);
void bnxt_free_filter_mem(struct bnxt *bp);
int bnxt_alloc_filter_mem(struct bnxt *bp);

#endif
