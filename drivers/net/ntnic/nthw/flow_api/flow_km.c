/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <assert.h>
#include <stdlib.h>

#include "hw_mod_backend.h"
#include "flow_api_engine.h"
#include "nt_util.h"

#define NUM_CAM_MASKS (ARRAY_SIZE(cam_masks))

static const struct cam_match_masks_s {
	uint32_t word_len;
	uint32_t key_mask[4];
} cam_masks[] = {
	{ 4, { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff } },	/* IP6_SRC, IP6_DST */
	{ 4, { 0xffffffff, 0xffffffff, 0xffffffff, 0xffff0000 } },	/* DMAC,SMAC,ethtype */
	{ 4, { 0xffffffff, 0xffff0000, 0x00000000, 0xffff0000 } },	/* DMAC,ethtype */
	{ 4, { 0x00000000, 0x0000ffff, 0xffffffff, 0xffff0000 } },	/* SMAC,ethtype */
	{ 4, { 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000 } },	/* ETH_128 */
	{ 2, { 0xffffffff, 0xffffffff, 0x00000000, 0x00000000 } },	/* IP4_COMBINED */
	/*
	 * ETH_TYPE, IP4_TTL_PROTO, IP4_SRC, IP4_DST, IP6_FLOW_TC,
	 * IP6_NEXT_HDR_HOP, TP_PORT_COMBINED, SIDEBAND_VNI
	 */
	{ 1, { 0xffffffff, 0x00000000, 0x00000000, 0x00000000 } },
	/* IP4_IHL_TOS, TP_PORT_SRC32_OR_ICMP, TCP_CTRL */
	{ 1, { 0xffff0000, 0x00000000, 0x00000000, 0x00000000 } },
	{ 1, { 0x0000ffff, 0x00000000, 0x00000000, 0x00000000 } },	/* TP_PORT_DST32 */
	/* IPv4 TOS mask bits used often by OVS */
	{ 1, { 0x00030000, 0x00000000, 0x00000000, 0x00000000 } },
	/* IPv6 TOS mask bits used often by OVS */
	{ 1, { 0x00300000, 0x00000000, 0x00000000, 0x00000000 } },
};

void km_free_ndev_resource_management(void **handle)
{
	if (*handle) {
		free(*handle);
		NT_LOG(DBG, FILTER, "Free NIC DEV CAM and TCAM record manager");
	}

	*handle = NULL;
}

int km_add_match_elem(struct km_flow_def_s *km, uint32_t e_word[4], uint32_t e_mask[4],
	uint32_t word_len, enum frame_offs_e start_id, int8_t offset)
{
	/* valid word_len 1,2,4 */
	if (word_len == 3) {
		word_len = 4;
		e_word[3] = 0;
		e_mask[3] = 0;
	}

	if (word_len < 1 || word_len > 4) {
		assert(0);
		return -1;
	}

	for (unsigned int i = 0; i < word_len; i++) {
		km->match[km->num_ftype_elem].e_word[i] = e_word[i];
		km->match[km->num_ftype_elem].e_mask[i] = e_mask[i];
	}

	km->match[km->num_ftype_elem].word_len = word_len;
	km->match[km->num_ftype_elem].rel_offs = offset;
	km->match[km->num_ftype_elem].extr_start_offs_id = start_id;

	/*
	 * Determine here if this flow may better be put into TCAM
	 * Otherwise it will go into CAM
	 * This is dependent on a cam_masks list defined above
	 */
	km->match[km->num_ftype_elem].masked_for_tcam = 1;

	for (unsigned int msk = 0; msk < NUM_CAM_MASKS; msk++) {
		if (word_len == cam_masks[msk].word_len) {
			int match = 1;

			for (unsigned int wd = 0; wd < word_len; wd++) {
				if (e_mask[wd] != cam_masks[msk].key_mask[wd]) {
					match = 0;
					break;
				}
			}

			if (match) {
				/* Can go into CAM */
				km->match[km->num_ftype_elem].masked_for_tcam = 0;
			}
		}
	}

	km->num_ftype_elem++;
	return 0;
}
