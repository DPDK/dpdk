/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>

#include "hw_mod_backend.h"
#include "flow_api_engine.h"
#include "nt_util.h"
#include "flow_hasher.h"

#define MAX_QWORDS 2
#define MAX_SWORDS 2

#define CUCKOO_MOVE_MAX_DEPTH 8

#define NUM_CAM_MASKS (ARRAY_SIZE(cam_masks))

#define CAM_DIST_IDX(bnk, rec) ((bnk) * km->be->km.nb_cam_records + (rec))
#define CAM_KM_DIST_IDX(bnk)                                                                      \
	({                                                                                        \
		int _temp_bnk = (bnk);                                                            \
		CAM_DIST_IDX(_temp_bnk, km->record_indexes[_temp_bnk]);                           \
	})

#define TCAM_DIST_IDX(bnk, rec) ((bnk) * km->be->km.nb_tcam_bank_width + (rec))

#define CAM_ENTRIES                                                                               \
	(km->be->km.nb_cam_banks * km->be->km.nb_cam_records * sizeof(struct cam_distrib_s))
#define TCAM_ENTRIES                                                                              \
	(km->be->km.nb_tcam_bank_width * km->be->km.nb_tcam_banks * sizeof(struct tcam_distrib_s))

/*
 * CAM structures and defines
 */
struct cam_distrib_s {
	struct km_flow_def_s *km_owner;
};

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

static int cam_addr_reserved_stack[CUCKOO_MOVE_MAX_DEPTH];

/*
 * TCAM structures and defines
 */
struct tcam_distrib_s {
	struct km_flow_def_s *km_owner;
};

static int tcam_find_mapping(struct km_flow_def_s *km);

void nthw_km_attach_ndev_resource_management(struct km_flow_def_s *km, void **handle)
{
	/*
	 * KM entries occupied in CAM - to manage the cuckoo shuffling
	 * and manage CAM population and usage
	 * KM entries occupied in TCAM - to manage population and usage
	 */
	if (!*handle) {
		*handle = calloc(1,
			(size_t)CAM_ENTRIES + sizeof(uint32_t) + (size_t)TCAM_ENTRIES +
			sizeof(struct hasher_s));
		if (!*handle) {
			NT_LOG(ERR, FILTER, "Failed to allocate CAM and TCAM record manager");
			return;
		}
		NT_LOG(DBG, FILTER, "Allocate NIC DEV CAM and TCAM record manager");
	}

	km->cam_dist = (struct cam_distrib_s *)*handle;
	km->cuckoo_moves = (uint32_t *)((char *)km->cam_dist + CAM_ENTRIES);
	km->tcam_dist =
		(struct tcam_distrib_s *)((char *)km->cam_dist + CAM_ENTRIES + sizeof(uint32_t));

	km->hsh = (struct hasher_s *)((char *)km->tcam_dist + TCAM_ENTRIES);
	nthw_init_hasher(km->hsh, km->be->km.nb_cam_banks, km->be->km.nb_cam_records);
}

void nthw_km_free_ndev_resource_management(void **handle)
{
	if (*handle) {
		free(*handle);
		NT_LOG(DBG, FILTER, "Free NIC DEV CAM and TCAM record manager");
	}

	*handle = NULL;
}

int nthw_km_add_match_elem(struct km_flow_def_s *km, uint32_t e_word[4], uint32_t e_mask[4],
	uint32_t word_len, enum frame_offs_e start_id, int8_t offset)
{
	/* valid word_len 1,2,4 */
	if (word_len == 3) {
		word_len = 4;
		e_word[3] = 0;
		e_mask[3] = 0;
	}

	if (word_len < 1 || word_len > 4) {
		RTE_ASSERT(0);
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

static int get_word(struct km_flow_def_s *km, uint32_t size, int marked[])
{
	for (int i = 0; i < km->num_ftype_elem; i++)
		if (!marked[i] && !(km->match[i].extr_start_offs_id & SWX_INFO) &&
			km->match[i].word_len == size)
			return i;

	return -1;
}

int nthw_km_key_create(struct km_flow_def_s *km, uint32_t port_id)
{
	/*
	 * Create combined extractor mappings
	 *  if key fields may be changed to cover un-mappable otherwise?
	 *  split into cam and tcam and use synergy mode when available
	 */
	int match_marked[MAX_MATCH_FIELDS];
	int idx = 0;
	int next = 0;
	int m_idx;
	int size;

	memset(match_marked, 0, sizeof(match_marked));

	/* build QWords */
	for (int qwords = 0; qwords < MAX_QWORDS; qwords++) {
		size = 4;
		m_idx = get_word(km, size, match_marked);

		if (m_idx < 0) {
			size = 2;
			m_idx = get_word(km, size, match_marked);

			if (m_idx < 0) {
				size = 1;
				m_idx = get_word(km, 1, match_marked);
			}
		}

		if (m_idx < 0) {
			/* no more defined */
			break;
		}

		match_marked[m_idx] = 1;

		/* build match map list and set final extractor to use */
		km->match_map[next] = &km->match[m_idx];
		km->match[m_idx].extr = KM_USE_EXTRACTOR_QWORD;

		/* build final entry words and mask array */
		for (int i = 0; i < size; i++) {
			km->entry_word[idx + i] = km->match[m_idx].e_word[i];
			km->entry_mask[idx + i] = km->match[m_idx].e_mask[i];
		}

		idx += size;
		next++;
	}

	m_idx = get_word(km, 4, match_marked);

	if (m_idx >= 0) {
		/* cannot match more QWords */
		return -1;
	}

	/*
	 * On km v6+ we have DWORDs here instead. However, we only use them as SWORDs for now
	 * No match would be able to exploit these as DWORDs because of maximum length of 12 words
	 * in CAM The last 2 words are taken by KCC-ID/SWX and Color. You could have one or none
	 * QWORDs where then both these DWORDs were possible in 10 words, but we don't have such
	 * use case built in yet
	 */
	/* build SWords */
	for (int swords = 0; swords < MAX_SWORDS; swords++) {
		m_idx = get_word(km, 1, match_marked);

		if (m_idx < 0) {
			/* no more defined */
			break;
		}

		match_marked[m_idx] = 1;
		/* build match map list and set final extractor to use */
		km->match_map[next] = &km->match[m_idx];
		km->match[m_idx].extr = KM_USE_EXTRACTOR_SWORD;

		/* build final entry words and mask array */
		km->entry_word[idx] = km->match[m_idx].e_word[0];
		km->entry_mask[idx] = km->match[m_idx].e_mask[0];
		idx++;
		next++;
	}

	/*
	 * Make sure we took them all
	 */
	m_idx = get_word(km, 1, match_marked);

	if (m_idx >= 0) {
		/* cannot match more SWords */
		return -1;
	}

	/*
	 * Handle SWX words specially
	 */
	int swx_found = 0;

	for (int i = 0; i < km->num_ftype_elem; i++) {
		if (km->match[i].extr_start_offs_id & SWX_INFO) {
			km->match_map[next] = &km->match[i];
			km->match[i].extr = KM_USE_EXTRACTOR_SWORD;
			/* build final entry words and mask array */
			km->entry_word[idx] = km->match[i].e_word[0];
			km->entry_mask[idx] = km->match[i].e_mask[0];
			idx++;
			next++;
			swx_found = 1;
		}
	}

	RTE_ASSERT(next == km->num_ftype_elem);

	km->key_word_size = idx;
	km->port_id = port_id;

	km->target = KM_CAM;

	/*
	 * Finally decide if we want to put this match->action into the TCAM
	 * When SWX word used we need to put it into CAM always, no matter what mask pattern
	 * Later, when synergy mode is applied, we can do a split
	 */
	if (!swx_found && km->key_word_size <= 6) {
		for (int i = 0; i < km->num_ftype_elem; i++) {
			if (km->match_map[i]->masked_for_tcam) {
				/* At least one */
				km->target = KM_TCAM;
			}
		}
	}

	NT_LOG(DBG, FILTER, "This flow goes into %s", (km->target == KM_TCAM) ? "TCAM" : "CAM");

	if (km->target == KM_TCAM) {
		if (km->key_word_size > 10) {
			/* do not support SWX in TCAM */
			return -1;
		}

		/*
		 * adjust for unsupported key word size in TCAM
		 */
		if ((km->key_word_size == 5 || km->key_word_size == 7 || km->key_word_size == 9)) {
			km->entry_mask[km->key_word_size] = 0;
			km->key_word_size++;
		}

		/*
		 * 1. the fact that the length of a key cannot change among the same used banks
		 *
		 *  calculate possible start indexes
		 *  unfortunately restrictions in TCAM lookup
		 *  makes it hard to handle key lengths larger than 6
		 *  when other sizes should be possible too
		 */
		switch (km->key_word_size) {
		case 1:
			for (int i = 0; i < 4; i++)
				km->start_offsets[0] = 8 + i;

			km->num_start_offsets = 4;
			break;

		case 2:
			km->start_offsets[0] = 6;
			km->num_start_offsets = 1;
			break;

		case 3:
			km->start_offsets[0] = 0;
			km->num_start_offsets = 1;
			/* enlarge to 6 */
			km->entry_mask[km->key_word_size++] = 0;
			km->entry_mask[km->key_word_size++] = 0;
			km->entry_mask[km->key_word_size++] = 0;
			break;

		case 4:
			km->start_offsets[0] = 0;
			km->num_start_offsets = 1;
			/* enlarge to 6 */
			km->entry_mask[km->key_word_size++] = 0;
			km->entry_mask[km->key_word_size++] = 0;
			break;

		case 6:
			km->start_offsets[0] = 0;
			km->num_start_offsets = 1;
			break;

		default:
			NT_LOG(DBG, FILTER, "Final Key word size too large: %i",
				km->key_word_size);
			return -1;
		}
	}

	return 0;
}

int nthw_km_key_compare(struct km_flow_def_s *km, struct km_flow_def_s *km1)
{
	if (km->target != km1->target || km->num_ftype_elem != km1->num_ftype_elem ||
		km->key_word_size != km1->key_word_size || km->info_set != km1->info_set)
		return 0;

	/*
	 * before KCC-CAM:
	 * if port is added to match, then we can have different ports in CAT
	 * that reuses this flow type
	 */
	int port_match_included = 0, kcc_swx_used = 0;

	for (int i = 0; i < km->num_ftype_elem; i++) {
		if (km->match[i].extr_start_offs_id == SB_MAC_PORT) {
			port_match_included = 1;
			break;
		}

		if (km->match_map[i]->extr_start_offs_id == SB_KCC_ID) {
			kcc_swx_used = 1;
			break;
		}
	}

	/*
	 * If not using KCC and if port match is not included in CAM,
	 * we need to have same port_id to reuse
	 */
	if (!kcc_swx_used && !port_match_included && km->port_id != km1->port_id)
		return 0;

	for (int i = 0; i < km->num_ftype_elem; i++) {
		/* using same extractor types in same sequence */
		if (km->match_map[i]->extr_start_offs_id !=
			km1->match_map[i]->extr_start_offs_id ||
			km->match_map[i]->rel_offs != km1->match_map[i]->rel_offs ||
			km->match_map[i]->extr != km1->match_map[i]->extr ||
			km->match_map[i]->word_len != km1->match_map[i]->word_len) {
			return 0;
		}
	}

	if (km->target == KM_CAM) {
		/* in CAM must exactly match on all masks */
		for (int i = 0; i < km->key_word_size; i++)
			if (km->entry_mask[i] != km1->entry_mask[i])
				return 0;

		/* Would be set later if not reusing from km1 */
		km->cam_paired = km1->cam_paired;

	} else if (km->target == KM_TCAM) {
		/*
		 * If TCAM, we must make sure Recipe Key Mask does not
		 * mask out enable bits in masks
		 * Note: it is important that km1 is the original creator
		 * of the KM Recipe, since it contains its true masks
		 */
		for (int i = 0; i < km->key_word_size; i++)
			if ((km->entry_mask[i] & km1->entry_mask[i]) != km->entry_mask[i])
				return 0;

		km->tcam_start_bank = km1->tcam_start_bank;
		km->tcam_record = -1;	/* needs to be found later */

	} else {
		NT_LOG(DBG, FILTER, "ERROR - KM target not defined or supported");
		return 0;
	}

	/*
	 * Check for a flow clash. If already programmed return with -1
	 */
	int double_match = 1;

	for (int i = 0; i < km->key_word_size; i++) {
		if ((km->entry_word[i] & km->entry_mask[i]) !=
			(km1->entry_word[i] & km1->entry_mask[i])) {
			double_match = 0;
			break;
		}
	}

	if (double_match)
		return -1;

	/*
	 * Note that TCAM and CAM may reuse same RCP and flow type
	 * when this happens, CAM entry wins on overlap
	 */

	/* Use same KM Recipe and same flow type - return flow type */
	return km1->flow_type;
}

int nthw_km_rcp_set(struct km_flow_def_s *km, int index)
{
	int qw = 0;
	int sw = 0;
	int swx = 0;

	nthw_mod_km_rcp_set(km->be, HW_KM_RCP_PRESET_ALL, index, 0, 0);

	/* set extractor words, offs, contrib */
	for (int i = 0; i < km->num_ftype_elem; i++) {
		switch (km->match_map[i]->extr) {
		case KM_USE_EXTRACTOR_SWORD:
			if (km->match_map[i]->extr_start_offs_id & SWX_INFO) {
				if (km->target == KM_CAM && swx == 0) {
					/* SWX */
					if (km->match_map[i]->extr_start_offs_id == SB_VNI) {
						NT_LOG(DBG, FILTER, "Set KM SWX sel A - VNI");
						nthw_mod_km_rcp_set(km->be,
							HW_KM_RCP_SWX_CCH, index, 0, 1);
						nthw_mod_km_rcp_set(km->be, HW_KM_RCP_SWX_SEL_A,
							index, 0, SWX_SEL_ALL32);

					} else if (km->match_map[i]->extr_start_offs_id ==
						SB_MAC_PORT) {
						NT_LOG(DBG, FILTER,
							"Set KM SWX sel A - PTC + MAC");
						nthw_mod_km_rcp_set(km->be, HW_KM_RCP_SWX_SEL_A,
							index, 0, SWX_SEL_ALL32);

					} else if (km->match_map[i]->extr_start_offs_id ==
						SB_KCC_ID) {
						NT_LOG(DBG, FILTER, "Set KM SWX sel A - KCC ID");
						nthw_mod_km_rcp_set(km->be,
							HW_KM_RCP_SWX_CCH, index, 0, 1);
						nthw_mod_km_rcp_set(km->be, HW_KM_RCP_SWX_SEL_A,
							index, 0, SWX_SEL_ALL32);

					} else {
						return -1;
					}

				} else {
					return -1;
				}

				swx++;

			} else {
				if (sw == 0) {
					/* DW8 */
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_DW8_DYN, index, 0,
						km->match_map[i]->extr_start_offs_id);
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_DW8_OFS, index, 0,
						km->match_map[i]->rel_offs);
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_DW8_SEL_A, index, 0,
						DW8_SEL_FIRST32);
					NT_LOG(DBG, FILTER,
						"Set KM DW8 sel A: dyn: %i, offs: %i",
						km->match_map[i]->extr_start_offs_id,
						km->match_map[i]->rel_offs);

				} else if (sw == 1) {
					/* DW10 */
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_DW10_DYN, index, 0,
						km->match_map[i]->extr_start_offs_id);
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_DW10_OFS, index, 0,
						km->match_map[i]->rel_offs);
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_DW10_SEL_A, index, 0,
						DW10_SEL_FIRST32);
					NT_LOG(DBG, FILTER,
						"Set KM DW10 sel A: dyn: %i, offs: %i",
						km->match_map[i]->extr_start_offs_id,
						km->match_map[i]->rel_offs);

				} else {
					return -1;
				}

				sw++;
			}

			break;

		case KM_USE_EXTRACTOR_QWORD:
			if (qw == 0) {
				nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW0_DYN, index, 0,
					km->match_map[i]->extr_start_offs_id);
				nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW0_OFS, index, 0,
					km->match_map[i]->rel_offs);

				switch (km->match_map[i]->word_len) {
				case 1:
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW0_SEL_A, index, 0,
						QW0_SEL_FIRST32);
					break;

				case 2:
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW0_SEL_A, index, 0,
						QW0_SEL_FIRST64);
					break;

				case 4:
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW0_SEL_A, index, 0,
						QW0_SEL_ALL128);
					break;

				default:
					return -1;
				}

				NT_LOG(DBG, FILTER,
					"Set KM QW0 sel A: dyn: %i, offs: %i, size: %" PRIu32 "",
					km->match_map[i]->extr_start_offs_id,
					km->match_map[i]->rel_offs, km->match_map[i]->word_len);

			} else if (qw == 1) {
				nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW4_DYN, index, 0,
					km->match_map[i]->extr_start_offs_id);
				nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW4_OFS, index, 0,
					km->match_map[i]->rel_offs);

				switch (km->match_map[i]->word_len) {
				case 1:
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW4_SEL_A, index, 0,
						QW4_SEL_FIRST32);
					break;

				case 2:
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW4_SEL_A, index, 0,
						QW4_SEL_FIRST64);
					break;

				case 4:
					nthw_mod_km_rcp_set(km->be, HW_KM_RCP_QW4_SEL_A, index, 0,
						QW4_SEL_ALL128);
					break;

				default:
					return -1;
				}

				NT_LOG(DBG, FILTER,
					"Set KM QW4 sel A: dyn: %i, offs: %i, size: %" PRIu32 "",
					km->match_map[i]->extr_start_offs_id,
					km->match_map[i]->rel_offs, km->match_map[i]->word_len);

			} else {
				return -1;
			}

			qw++;
			break;

		default:
			return -1;
		}
	}

	/* set mask A */
	for (int i = 0; i < km->key_word_size; i++) {
		nthw_mod_km_rcp_set(km->be, HW_KM_RCP_MASK_A, index,
			(km->be->km.nb_km_rcp_mask_a_word_size - 1) - i,
			km->entry_mask[i]);
		NT_LOG(DBG, FILTER, "Set KM mask A: %08x", km->entry_mask[i]);
	}

	if (km->target == KM_CAM) {
		/* set info - Color */
		if (km->info_set) {
			nthw_mod_km_rcp_set(km->be, HW_KM_RCP_INFO_A, index, 0, 1);
			NT_LOG(DBG, FILTER, "Set KM info A");
		}

		/* set key length A */
		nthw_mod_km_rcp_set(km->be, HW_KM_RCP_EL_A, index, 0,
			km->key_word_size + !!km->info_set - 1);	/* select id is -1 */
		/* set Flow Type for Key A */
		NT_LOG(DBG, FILTER, "Set KM EL A: %i", km->key_word_size + !!km->info_set - 1);

		nthw_mod_km_rcp_set(km->be, HW_KM_RCP_FTM_A, index, 0, 1 << km->flow_type);

		NT_LOG(DBG, FILTER, "Set KM FTM A - ft: %i", km->flow_type);

		/* Set Paired - only on the CAM part though... TODO split CAM and TCAM */
		if ((uint32_t)(km->key_word_size + !!km->info_set) >
			km->be->km.nb_cam_record_words) {
			nthw_mod_km_rcp_set(km->be, HW_KM_RCP_PAIRED, index, 0, 1);
			NT_LOG(DBG, FILTER, "Set KM CAM Paired");
			km->cam_paired = 1;
		}

	} else if (km->target == KM_TCAM) {
		uint32_t bank_bm = 0;

		if (tcam_find_mapping(km) < 0) {
			/* failed mapping into TCAM */
			NT_LOG(DBG, FILTER, "INFO: TCAM mapping flow failed");
			return -1;
		}

		RTE_ASSERT((uint32_t)(km->tcam_start_bank + km->key_word_size) <=
			km->be->km.nb_tcam_banks);

		for (int i = 0; i < km->key_word_size; i++) {
			bank_bm |=
				(1 << (km->be->km.nb_tcam_banks - 1 - (km->tcam_start_bank + i)));
		}

		/* Set BANK_A */
		nthw_mod_km_rcp_set(km->be, HW_KM_RCP_BANK_A, index, 0, bank_bm);
		/* Set Kl_A */
		nthw_mod_km_rcp_set(km->be, HW_KM_RCP_KL_A, index, 0, km->key_word_size - 1);

	} else {
		return -1;
	}

	return 0;
}

static int cam_populate(struct km_flow_def_s *km, int bank)
{
	int res = 0;
	int cnt = km->key_word_size + !!km->info_set;

	for (uint32_t i = 0; i < km->be->km.nb_cam_record_words && cnt; i++, cnt--) {
		res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_W0 + i, bank,
				km->record_indexes[bank], km->entry_word[i]);
		res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_FT0 + i, bank,
				km->record_indexes[bank], km->flow_type);
	}

	km->cam_dist[CAM_KM_DIST_IDX(bank)].km_owner = km;

	if (cnt) {
		RTE_ASSERT(km->cam_paired);

		for (uint32_t i = 0; i < km->be->km.nb_cam_record_words && cnt; i++, cnt--) {
			res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_W0 + i, bank,
				km->record_indexes[bank] + 1,
				km->entry_word[km->be->km.nb_cam_record_words + i]);
			res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_FT0 + i, bank,
				km->record_indexes[bank] + 1, km->flow_type);
		}

		km->cam_dist[CAM_KM_DIST_IDX(bank) + 1].km_owner = km;
	}

	res |= nthw_mod_km_cam_flush(km->be, bank, km->record_indexes[bank],
			km->cam_paired ? 2 : 1);

	return res;
}

static int cam_reset_entry(struct km_flow_def_s *km, int bank)
{
	int res = 0;
	int cnt = km->key_word_size + !!km->info_set;

	for (uint32_t i = 0; i < km->be->km.nb_cam_record_words && cnt; i++, cnt--) {
		res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_W0 + i, bank,
				km->record_indexes[bank], 0);
		res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_FT0 + i, bank,
				km->record_indexes[bank], 0);
	}

	km->cam_dist[CAM_KM_DIST_IDX(bank)].km_owner = NULL;

	if (cnt) {
		RTE_ASSERT(km->cam_paired);

		for (uint32_t i = 0; i < km->be->km.nb_cam_record_words && cnt; i++, cnt--) {
			res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_W0 + i, bank,
				km->record_indexes[bank] + 1, 0);
			res |= nthw_mod_km_cam_set(km->be, HW_KM_CAM_FT0 + i, bank,
				km->record_indexes[bank] + 1, 0);
		}

		km->cam_dist[CAM_KM_DIST_IDX(bank) + 1].km_owner = NULL;
	}

	res |= nthw_mod_km_cam_flush(km->be, bank, km->record_indexes[bank],
			km->cam_paired ? 2 : 1);
	return res;
}

static int move_cuckoo_index(struct km_flow_def_s *km)
{
	RTE_ASSERT(km->cam_dist[CAM_KM_DIST_IDX(km->bank_used)].km_owner);

	for (uint32_t bank = 0; bank < km->be->km.nb_cam_banks; bank++) {
		/* It will not select itself */
		if (km->cam_dist[CAM_KM_DIST_IDX(bank)].km_owner == NULL) {
			if (km->cam_paired) {
				if (km->cam_dist[CAM_KM_DIST_IDX(bank) + 1].km_owner != NULL)
					continue;
			}

			/*
			 * Populate in new position
			 */
			int res = cam_populate(km, bank);

			if (res) {
				NT_LOG(DBG, FILTER,
					"Error: failed to write to KM CAM in cuckoo move");
				return 0;
			}

			/*
			 * Reset/free entry in old bank
			 * HW flushes are really not needed, the old addresses are always taken
			 * over by the caller If you change this code in future updates, this may
			 * no longer be true then!
			 */
			km->cam_dist[CAM_KM_DIST_IDX(km->bank_used)].km_owner = NULL;

			if (km->cam_paired)
				km->cam_dist[CAM_KM_DIST_IDX(km->bank_used) + 1].km_owner = NULL;

			NT_LOG(DBG, FILTER,
				"KM Cuckoo hash moved from bank %i to bank %" PRIu32 " (%04X => %04X)",
				km->bank_used, bank, CAM_KM_DIST_IDX(km->bank_used),
				CAM_KM_DIST_IDX(bank));
			km->bank_used = bank;
			(*km->cuckoo_moves)++;
			return 1;
		}
	}

	return 0;
}

static int move_cuckoo_index_level(struct km_flow_def_s *km_parent, int bank_idx, int levels,
	int cam_adr_list_len)
{
	struct km_flow_def_s *km = km_parent->cam_dist[bank_idx].km_owner;

	RTE_ASSERT(levels <= CUCKOO_MOVE_MAX_DEPTH);

	/*
	 * Only move if same pairness
	 * Can be extended later to handle both move of paired and single entries
	 */
	if (!km || km_parent->cam_paired != km->cam_paired)
		return 0;

	if (move_cuckoo_index(km))
		return 1;

	if (levels <= 1)
		return 0;

	RTE_ASSERT(cam_adr_list_len < CUCKOO_MOVE_MAX_DEPTH);

	cam_addr_reserved_stack[cam_adr_list_len++] = bank_idx;

	for (uint32_t i = 0; i < km->be->km.nb_cam_banks; i++) {
		int reserved = 0;
		int new_idx = CAM_KM_DIST_IDX(i);

		for (int i_reserved = 0; i_reserved < cam_adr_list_len; i_reserved++) {
			if (cam_addr_reserved_stack[i_reserved] == new_idx) {
				reserved = 1;
				break;
			}
		}

		if (reserved)
			continue;

		int res = move_cuckoo_index_level(km, new_idx, levels - 1, cam_adr_list_len);

		if (res) {
			if (move_cuckoo_index(km))
				return 1;

			RTE_ASSERT(0);
		}
	}

	return 0;
}

static int km_write_data_to_cam(struct km_flow_def_s *km)
{
	int res = 0;
	int val[MAX_BANKS];
	RTE_ASSERT(km->be->km.nb_cam_banks <= MAX_BANKS);
	RTE_ASSERT(km->cam_dist);

	/* word list without info set */
	nthw_gethash(km->hsh, km->entry_word, val);

	for (uint32_t i = 0; i < km->be->km.nb_cam_banks; i++) {
		/* if paired we start always on an even address - reset bit 0 */
		km->record_indexes[i] = (km->cam_paired) ? val[i] & ~1 : val[i];
	}

	NT_LOG(DBG, FILTER, "KM HASH [%03X, %03X, %03X]", km->record_indexes[0],
		km->record_indexes[1], km->record_indexes[2]);

	if (km->info_set)
		km->entry_word[km->key_word_size] = km->info;	/* finally set info */

	int bank = -1;

	/*
	 * first step, see if any of the banks are free
	 */
	for (uint32_t i_bank = 0; i_bank < km->be->km.nb_cam_banks; i_bank++) {
		if (km->cam_dist[CAM_KM_DIST_IDX(i_bank)].km_owner == NULL) {
			if (km->cam_paired == 0 ||
				km->cam_dist[CAM_KM_DIST_IDX(i_bank) + 1].km_owner == NULL) {
				bank = i_bank;
				break;
			}
		}
	}

	if (bank < 0) {
		/*
		 * Second step - cuckoo move existing flows if possible
		 */
		for (uint32_t i_bank = 0; i_bank < km->be->km.nb_cam_banks; i_bank++) {
			if (move_cuckoo_index_level(km, CAM_KM_DIST_IDX(i_bank), 4, 0)) {
				bank = i_bank;
				break;
			}
		}
	}

	if (bank < 0)
		return -1;

	/* populate CAM */
	NT_LOG(DBG, FILTER, "KM Bank = %i (addr %04X)", bank, CAM_KM_DIST_IDX(bank));
	res = cam_populate(km, bank);

	if (res == 0) {
		km->flushed_to_target = 1;
		km->bank_used = bank;
	}

	return res;
}

/*
 * TCAM
 */
static int tcam_find_free_record(struct km_flow_def_s *km, int start_bank)
{
	for (uint32_t rec = 0; rec < km->be->km.nb_tcam_bank_width; rec++) {
		if (km->tcam_dist[TCAM_DIST_IDX(start_bank, rec)].km_owner == NULL) {
			int pass = 1;

			for (int ii = 1; ii < km->key_word_size; ii++) {
				if (km->tcam_dist[TCAM_DIST_IDX(start_bank + ii, rec)].km_owner !=
					NULL) {
					pass = 0;
					break;
				}
			}

			if (pass) {
				km->tcam_record = rec;
				return 1;
			}
		}
	}

	return 0;
}

static int tcam_find_mapping(struct km_flow_def_s *km)
{
	/* Search record and start index for this flow */
	for (int bs_idx = 0; bs_idx < km->num_start_offsets; bs_idx++) {
		if (tcam_find_free_record(km, km->start_offsets[bs_idx])) {
			km->tcam_start_bank = km->start_offsets[bs_idx];
			NT_LOG(DBG, FILTER, "Found space in TCAM start bank %i, record %i",
				km->tcam_start_bank, km->tcam_record);
			return 0;
		}
	}

	return -1;
}

static int tcam_write_word(struct km_flow_def_s *km, int bank, int record, uint32_t word,
	uint32_t mask)
{
	int err = 0;
	uint32_t all_recs[3];

	int rec_val = record / 32;
	int rec_bit_shft = record % 32;
	uint32_t rec_bit = (1 << rec_bit_shft);

	RTE_ASSERT((km->be->km.nb_tcam_bank_width + 31) / 32 <= 3);

	for (int byte = 0; byte < 4; byte++) {
		uint8_t a = (uint8_t)((word >> (24 - (byte * 8))) & 0xff);
		uint8_t a_m = (uint8_t)((mask >> (24 - (byte * 8))) & 0xff);
		/* calculate important value bits */
		a = a & a_m;

		for (int val = 0; val < 256; val++) {
			err |= nthw_mod_km_tcam_get(km->be, HW_KM_TCAM_T,
				bank, byte, val, all_recs);

			if ((val & a_m) == a)
				all_recs[rec_val] |= rec_bit;
			else
				all_recs[rec_val] &= ~rec_bit;

			err |= nthw_mod_km_tcam_set(km->be, HW_KM_TCAM_T, bank,
				byte, val, all_recs);

			if (err)
				break;
		}
	}

	/* flush bank */
	err |= nthw_mod_km_tcam_flush(km->be, bank, ALL_BANK_ENTRIES);

	if (err == 0) {
		RTE_ASSERT(km->tcam_dist[TCAM_DIST_IDX(bank, record)].km_owner == NULL);
		km->tcam_dist[TCAM_DIST_IDX(bank, record)].km_owner = km;
	}

	return err;
}

static int km_write_data_to_tcam(struct km_flow_def_s *km)
{
	int err = 0;

	if (km->tcam_record < 0) {
		tcam_find_free_record(km, km->tcam_start_bank);

		if (km->tcam_record < 0) {
			NT_LOG(DBG, FILTER, "FAILED to find space in TCAM for flow");
			return -1;
		}

		NT_LOG(DBG, FILTER, "Reused RCP: Found space in TCAM start bank %i, record %i",
			km->tcam_start_bank, km->tcam_record);
	}

	/* Write KM_TCI */
	err |= nthw_mod_km_tci_set(km->be, HW_KM_TCI_COLOR, km->tcam_start_bank, km->tcam_record,
		km->info);
	err |= nthw_mod_km_tci_set(km->be, HW_KM_TCI_FT, km->tcam_start_bank, km->tcam_record,
		km->flow_type);
	err |= nthw_mod_km_tci_flush(km->be, km->tcam_start_bank, km->tcam_record, 1);

	for (int i = 0; i < km->key_word_size && !err; i++) {
		err = tcam_write_word(km, km->tcam_start_bank + i, km->tcam_record,
			km->entry_word[i], km->entry_mask[i]);
	}

	if (err == 0)
		km->flushed_to_target = 1;

	return err;
}

static int tcam_reset_bank(struct km_flow_def_s *km, int bank, int record)
{
	int err = 0;
	uint32_t all_recs[3];

	int rec_val = record / 32;
	int rec_bit_shft = record % 32;
	uint32_t rec_bit = (1 << rec_bit_shft);

	RTE_ASSERT((km->be->km.nb_tcam_bank_width + 31) / 32 <= 3);

	for (int byte = 0; byte < 4; byte++) {
		for (int val = 0; val < 256; val++) {
			err = nthw_mod_km_tcam_get(km->be, HW_KM_TCAM_T, bank, byte, val, all_recs);

			if (err)
				break;

			all_recs[rec_val] &= ~rec_bit;
			err = nthw_mod_km_tcam_set(km->be, HW_KM_TCAM_T, bank, byte, val, all_recs);

			if (err)
				break;
		}
	}

	if (err)
		return err;

	/* flush bank */
	err = nthw_mod_km_tcam_flush(km->be, bank, ALL_BANK_ENTRIES);
	km->tcam_dist[TCAM_DIST_IDX(bank, record)].km_owner = NULL;

	NT_LOG(DBG, FILTER, "Reset TCAM bank %i, rec_val %i rec bit %08x", bank, rec_val,
		rec_bit);

	return err;
}

static int tcam_reset_entry(struct km_flow_def_s *km)
{
	int err = 0;

	if (km->tcam_start_bank < 0 || km->tcam_record < 0) {
		NT_LOG(DBG, FILTER, "FAILED to find space in TCAM for flow");
		return -1;
	}

	/* Write KM_TCI */
	nthw_mod_km_tci_set(km->be, HW_KM_TCI_COLOR, km->tcam_start_bank, km->tcam_record, 0);
	nthw_mod_km_tci_set(km->be, HW_KM_TCI_FT, km->tcam_start_bank, km->tcam_record, 0);
	nthw_mod_km_tci_flush(km->be, km->tcam_start_bank, km->tcam_record, 1);

	for (int i = 0; i < km->key_word_size && !err; i++)
		err = tcam_reset_bank(km, km->tcam_start_bank + i, km->tcam_record);

	return err;
}

int nthw_km_write_data_match_entry(struct km_flow_def_s *km, uint32_t color)
{
	int res = -1;

	km->info = color;
	NT_LOG(DBG, FILTER, "Write Data entry Color: %08x", color);

	switch (km->target) {
	case KM_CAM:
		res = km_write_data_to_cam(km);
		break;

	case KM_TCAM:
		res = km_write_data_to_tcam(km);
		break;

	case KM_SYNERGY:
	default:
		break;
	}

	return res;
}

int nthw_km_clear_data_match_entry(struct km_flow_def_s *km)
{
	int res = 0;

	if (km->root) {
		struct km_flow_def_s *km1 = km->root;

		while (km1->reference != km)
			km1 = km1->reference;

		km1->reference = km->reference;

		km->flushed_to_target = 0;
		km->bank_used = 0;

	} else if (km->reference) {
		km->reference->root = NULL;

		switch (km->target) {
		case KM_CAM:
			km->cam_dist[CAM_KM_DIST_IDX(km->bank_used)].km_owner = km->reference;

			if (km->key_word_size + !!km->info_set > 1) {
				RTE_ASSERT(km->cam_paired);
				km->cam_dist[CAM_KM_DIST_IDX(km->bank_used) + 1].km_owner =
					km->reference;
			}

			break;

		case KM_TCAM:
			for (int i = 0; i < km->key_word_size; i++) {
				km->tcam_dist[TCAM_DIST_IDX(km->tcam_start_bank + i,
					km->tcam_record)]
				.km_owner = km->reference;
			}

			break;

		case KM_SYNERGY:
		default:
			res = -1;
			break;
		}

		km->flushed_to_target = 0;
		km->bank_used = 0;

	} else if (km->flushed_to_target) {
		switch (km->target) {
		case KM_CAM:
			res = cam_reset_entry(km, km->bank_used);
			break;

		case KM_TCAM:
			res = tcam_reset_entry(km);
			break;

		case KM_SYNERGY:
		default:
			res = -1;
			break;
		}

		km->flushed_to_target = 0;
		km->bank_used = 0;
	}

	return res;
}
