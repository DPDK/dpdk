/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "rte_common.h"

#include "hw_mod_backend.h"
#include "flow_api_engine.h"

#include "flow_api_hw_db_inline.h"
#include "flow_api_profile_inline_config.h"
#include "flow_hsh_cfg.h"
#include "ntlog.h"

#define HW_DB_INLINE_ACTION_SET_NB 512
#define HW_DB_INLINE_MATCH_SET_NB 512

#define HW_DB_FT_LOOKUP_KEY_A 0

#define HW_DB_FT_TYPE_KM 1
#define HW_DB_FT_LOOKUP_KEY_A 0
#define HW_DB_FT_LOOKUP_KEY_C 2

#define HW_DB_FT_TYPE_FLM 0
#define HW_DB_FT_TYPE_KM 1
/******************************************************************************/
/* Handle                                                                     */
/******************************************************************************/

struct hw_db_inline_resource_db {
	/* Actions */
	struct hw_db_inline_resource_db_cot {
		struct hw_db_inline_cot_data data;
		int ref;
	} *cot;

	struct hw_db_inline_resource_db_qsl {
		struct hw_db_inline_qsl_data data;
		int qst_idx;
	} *qsl;

	struct hw_db_inline_resource_db_slc_lr {
		struct hw_db_inline_slc_lr_data data;
		int ref;
	} *slc_lr;

	struct hw_db_inline_resource_db_tpe {
		struct hw_db_inline_tpe_data data;
		int ref;
	} *tpe;

	struct hw_db_inline_resource_db_tpe_ext {
		struct hw_db_inline_tpe_ext_data data;
		int replace_ram_idx;
		int ref;
	} *tpe_ext;

	struct hw_db_inline_resource_db_hsh {
		struct hw_db_inline_hsh_data data;
		int ref;
	} *hsh;

	struct hw_db_inline_resource_db_scrub {
		struct hw_db_inline_scrub_data data;
		int ref;
	} *scrub;

	uint32_t nb_cot;
	uint32_t nb_qsl;
	uint32_t nb_slc_lr;
	uint32_t nb_tpe;
	uint32_t nb_tpe_ext;
	uint32_t nb_hsh;
	uint32_t nb_scrub;

	/* Items */
	struct hw_db_inline_resource_db_cat {
		struct hw_db_inline_cat_data data;
		int ref;
	} *cat;

	struct hw_db_inline_resource_db_flm_rcp {
		struct hw_db_inline_flm_rcp_data data;
		int ref;

		struct hw_db_inline_resource_db_flm_ft {
			struct hw_db_inline_flm_ft_data data;
			struct hw_db_flm_ft idx;
			int ref;
		} *ft;

		struct hw_db_inline_resource_db_flm_match_set {
			struct hw_db_match_set_idx idx;
			int ref;
		} *match_set;

		struct hw_db_inline_resource_db_flm_cfn_map {
			int cfn_idx;
		} *cfn_map;
	} *flm;

	struct hw_db_inline_resource_db_km_rcp {
		struct hw_db_inline_km_rcp_data data;
		int ref;

		struct hw_db_inline_resource_db_km_ft {
			struct hw_db_inline_km_ft_data data;
			int ref;
		} *ft;
	} *km;

	uint32_t nb_cat;
	uint32_t nb_flm_ft;
	uint32_t nb_flm_rcp;
	uint32_t nb_km_ft;
	uint32_t nb_km_rcp;

	/* Hardware */

	struct hw_db_inline_resource_db_cfn {
		uint64_t priority;
		int cfn_hw;
		int ref;
	} *cfn;

	uint32_t cfn_priority_counter;
	uint32_t set_priority_counter;

	struct hw_db_inline_resource_db_action_set {
		struct hw_db_inline_action_set_data data;
		int ref;
	} action_set[HW_DB_INLINE_ACTION_SET_NB];

	struct hw_db_inline_resource_db_match_set {
		struct hw_db_inline_match_set_data data;
		int ref;
		uint32_t set_priority;
	} match_set[HW_DB_INLINE_MATCH_SET_NB];
};

int hw_db_inline_create(struct flow_nic_dev *ndev, void **db_handle)
{
	/* Note: calloc is required for functionality in the hw_db_inline_destroy() */
	struct hw_db_inline_resource_db *db = calloc(1, sizeof(struct hw_db_inline_resource_db));

	if (db == NULL)
		return -1;

	db->nb_cot = ndev->be.cat.nb_cat_funcs;
	db->cot = calloc(db->nb_cot, sizeof(struct hw_db_inline_resource_db_cot));

	if (db->cot == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_qsl = ndev->be.qsl.nb_rcp_categories;
	db->qsl = calloc(db->nb_qsl, sizeof(struct hw_db_inline_resource_db_qsl));

	if (db->qsl == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_slc_lr = ndev->be.max_categories;
	db->slc_lr = calloc(db->nb_slc_lr, sizeof(struct hw_db_inline_resource_db_slc_lr));

	if (db->slc_lr == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_tpe = ndev->be.tpe.nb_rcp_categories;
	db->tpe = calloc(db->nb_tpe, sizeof(struct hw_db_inline_resource_db_tpe));

	if (db->tpe == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_tpe_ext = ndev->be.tpe.nb_rpl_ext_categories;
	db->tpe_ext = calloc(db->nb_tpe_ext, sizeof(struct hw_db_inline_resource_db_tpe_ext));

	if (db->tpe_ext == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_cat = ndev->be.cat.nb_cat_funcs;
	db->cat = calloc(db->nb_cat, sizeof(struct hw_db_inline_resource_db_cat));

	if (db->cat == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}


	db->nb_flm_ft = ndev->be.cat.nb_flow_types;
	db->nb_flm_rcp = ndev->be.flm.nb_categories;
	db->flm = calloc(db->nb_flm_rcp, sizeof(struct hw_db_inline_resource_db_flm_rcp));

	if (db->flm == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	for (uint32_t i = 0; i < db->nb_flm_rcp; ++i) {
		db->flm[i].ft =
			calloc(db->nb_flm_ft, sizeof(struct hw_db_inline_resource_db_flm_ft));

		if (db->flm[i].ft == NULL) {
			hw_db_inline_destroy(db);
			return -1;
		}

		db->flm[i].match_set =
			calloc(db->nb_cat, sizeof(struct hw_db_inline_resource_db_flm_match_set));

		if (db->flm[i].match_set == NULL) {
			hw_db_inline_destroy(db);
			return -1;
		}

		db->flm[i].cfn_map = calloc(db->nb_cat * db->nb_flm_ft,
			sizeof(struct hw_db_inline_resource_db_flm_cfn_map));

		if (db->flm[i].cfn_map == NULL) {
			hw_db_inline_destroy(db);
			return -1;
		}
	}

	db->nb_km_ft = ndev->be.cat.nb_flow_types;
	db->nb_km_rcp = ndev->be.km.nb_categories;
	db->km = calloc(db->nb_km_rcp, sizeof(struct hw_db_inline_resource_db_km_rcp));

	if (db->km == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	for (uint32_t i = 0; i < db->nb_km_rcp; ++i) {
		db->km[i].ft = calloc(db->nb_km_ft * db->nb_cat,
			sizeof(struct hw_db_inline_resource_db_km_ft));

		if (db->km[i].ft == NULL) {
			hw_db_inline_destroy(db);
			return -1;
		}
	}

	db->cfn = calloc(db->nb_cat, sizeof(struct hw_db_inline_resource_db_cfn));

	if (db->cfn == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_hsh = ndev->be.hsh.nb_rcp;
	db->hsh = calloc(db->nb_hsh, sizeof(struct hw_db_inline_resource_db_hsh));

	if (db->hsh == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	db->nb_scrub = ndev->be.flm.nb_scrub_profiles;
	db->scrub = calloc(db->nb_scrub, sizeof(struct hw_db_inline_resource_db_scrub));

	if (db->scrub == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	*db_handle = db;

	/* Preset data */

	db->flm[0].ft[1].idx.type = HW_DB_IDX_TYPE_FLM_FT;
	db->flm[0].ft[1].idx.id1 = 1;
	db->flm[0].ft[1].ref = 1;

	return 0;
}

void hw_db_inline_destroy(void *db_handle)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	free(db->cot);
	free(db->qsl);
	free(db->slc_lr);
	free(db->tpe);
	free(db->tpe_ext);
	free(db->hsh);
	free(db->scrub);

	free(db->cat);

	if (db->flm) {
		for (uint32_t i = 0; i < db->nb_flm_rcp; ++i) {
			free(db->flm[i].ft);
			free(db->flm[i].match_set);
			free(db->flm[i].cfn_map);
		}

		free(db->flm);
	}

	if (db->km) {
		for (uint32_t i = 0; i < db->nb_km_rcp; ++i)
			free(db->km[i].ft);

		free(db->km);
	}

	free(db->cfn);

	free(db);
}

struct hw_db_idx *hw_db_inline_find_idx(struct flow_nic_dev *ndev, void *db_handle,
	enum hw_db_idx_type type, struct hw_db_idx *idxs, uint32_t size)
{
	(void)ndev;
	(void)db_handle;
	for (uint32_t i = 0; i < size; ++i) {
		if (idxs[i].type == type)
			return &idxs[i];
	}

	return NULL;
}

void hw_db_inline_dump(struct flow_nic_dev *ndev, void *db_handle, const struct hw_db_idx *idxs,
	uint32_t size, FILE *file)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	char str_buffer[4096];
	uint16_t rss_buffer_len = sizeof(str_buffer);

	for (uint32_t i = 0; i < size; ++i) {
		switch (idxs[i].type) {
		case HW_DB_IDX_TYPE_NONE:
			break;

		case HW_DB_IDX_TYPE_MATCH_SET: {
			const struct hw_db_inline_match_set_data *data =
					&db->match_set[idxs[i].ids].data;
			fprintf(file, "  MATCH_SET %" PRIu32 ", priority %u\n", idxs[i].ids,
				(unsigned int)data->priority);
			fprintf(file, "    CAT id %" PRIu32 ", KM id %" PRIu32
				", KM_FT id %" PRIu32 ", ACTION_SET id %" PRIu32 "\n",
				data->cat.ids, data->km.id1, data->km_ft.id1,
				data->action_set.ids);

			if (data->jump) {
				uint32_t group_orig = 0;
				if (nthw_flow_group_translate_get_orig_group(ndev->group_handle,
					data->jump, &group_orig) < 0)
					fprintf(file, "    Jumps to %d (encoded)\n", data->jump);
				else
					fprintf(file, "    Jumps to %" PRIu32 "\n", group_orig);
			}
			break;
		}

		case HW_DB_IDX_TYPE_ACTION_SET: {
			const struct hw_db_inline_action_set_data *data =
					&db->action_set[idxs[i].ids].data;
			fprintf(file, "  ACTION_SET %" PRIu32 "\n", idxs[i].ids);

			if (data->contains_jump) {
				uint32_t group_orig = 0;
				if (nthw_flow_group_translate_get_orig_group(ndev->group_handle,
					data->jump, &group_orig) < 0)
					fprintf(file, "    Jumps to %d (encoded)\n", data->jump);

				else
					fprintf(file, "    Jumps to %d\n", group_orig);
			} else {
				fprintf(file,
					"    COT id %" PRIu32 ", QSL id %" PRIu32 ", SLC_LR id %"
					PRIu32 ", TPE id %" PRIu32 ", HSH id %" PRIu32 ", SCRUB id %"
					PRIu32 "\n",
					data->cot.ids, data->qsl.ids, data->slc_lr.ids,
					data->tpe.ids, data->hsh.ids, data->scrub.ids);
			}
			break;
		}

		case HW_DB_IDX_TYPE_CAT: {
			const struct hw_db_inline_cat_data *data = &db->cat[idxs[i].ids].data;
			fprintf(file, "  CAT %" PRIu32 "\n", idxs[i].ids);
			fprintf(file, "    Port msk 0x%02x, VLAN msk 0x%02x\n",
				(int)data->mac_port_mask, (int)data->vlan_mask);
			fprintf(file,
				"    Proto msks: Frag 0x%02x, l2 0x%02x, l3 0x%02x, l4 0x%02x, l3t 0x%02x, l4t 0x%02x\n",
				(int)data->ptc_mask_frag, (int)data->ptc_mask_l2,
				(int)data->ptc_mask_l3, (int)data->ptc_mask_l4,
				(int)data->ptc_mask_l3_tunnel, (int)data->ptc_mask_l4_tunnel);
			fprintf(file, "    IP protocol: pn %u pnt %u\n", data->ip_prot,
				data->ip_prot_tunnel);
			break;
		}

		case HW_DB_IDX_TYPE_QSL: {
			const struct hw_db_inline_qsl_data *data = &db->qsl[idxs[i].ids].data;
			fprintf(file, "  QSL %" PRIu32 "\n", idxs[i].ids);

			if (data->discard) {
				fprintf(file, "    Discard\n");
				break;
			}

			if (data->drop) {
				fprintf(file, "    Drop\n");
				break;
			}

			fprintf(file, "    Table size %" PRIu32 "\n", data->table_size);

			for (uint32_t i = 0;
				i < data->table_size && i < HW_DB_INLINE_MAX_QST_PER_QSL; ++i) {
				fprintf(file, "      %u: Queue %d, TX port %d\n", i,
					(data->table[i].queue_en ? (int)data->table[i].queue : -1),
					(data->table[i].tx_port_en ? (int)data->table[i].tx_port
						: -1));
			}

			break;
		}

		case HW_DB_IDX_TYPE_COT: {
			const struct hw_db_inline_cot_data *data = &db->cot[idxs[i].ids].data;
			fprintf(file, "  COT %" PRIu32 "\n", idxs[i].ids);
			fprintf(file, "    Color contrib %d, frag rcp %d\n",
				(int)data->matcher_color_contrib, (int)data->frag_rcp);
			break;
		}

		case HW_DB_IDX_TYPE_SLC_LR: {
			const struct hw_db_inline_slc_lr_data *data =
					&db->slc_lr[idxs[i].ids].data;
			fprintf(file, "  SLC_LR %" PRIu32 "\n", idxs[i].ids);
			fprintf(file, "    Enable %u, dyn %u, ofs %u\n", data->head_slice_en,
				data->head_slice_dyn, data->head_slice_ofs);
			break;
		}

		case HW_DB_IDX_TYPE_TPE: {
			const struct hw_db_inline_tpe_data *data = &db->tpe[idxs[i].ids].data;
			fprintf(file, "  TPE %" PRIu32 "\n", idxs[i].ids);
			fprintf(file, "    Insert len %u, new outer %u, calc eth %u\n",
				data->insert_len, data->new_outer,
				data->calc_eth_type_from_inner_ip);
			fprintf(file, "    TTL enable %u, dyn %u, ofs %u\n", data->ttl_en,
				data->ttl_dyn, data->ttl_ofs);
			fprintf(file,
				"    Len A enable %u, pos dyn %u, pos ofs %u, add dyn %u, add ofs %u, sub dyn %u\n",
				data->len_a_en, data->len_a_pos_dyn, data->len_a_pos_ofs,
				data->len_a_add_dyn, data->len_a_add_ofs, data->len_a_sub_dyn);
			fprintf(file,
				"    Len B enable %u, pos dyn %u, pos ofs %u, add dyn %u, add ofs %u, sub dyn %u\n",
				data->len_b_en, data->len_b_pos_dyn, data->len_b_pos_ofs,
				data->len_b_add_dyn, data->len_b_add_ofs, data->len_b_sub_dyn);
			fprintf(file,
				"    Len C enable %u, pos dyn %u, pos ofs %u, add dyn %u, add ofs %u, sub dyn %u\n",
				data->len_c_en, data->len_c_pos_dyn, data->len_c_pos_ofs,
				data->len_c_add_dyn, data->len_c_add_ofs, data->len_c_sub_dyn);

			for (uint32_t i = 0; i < 6; ++i)
				if (data->writer[i].en)
					fprintf(file,
						"    Writer %i: Reader %u, dyn %u, ofs %u, len %u\n",
						i, data->writer[i].reader_select,
						data->writer[i].dyn, data->writer[i].ofs,
						data->writer[i].len);

			break;
		}

		case HW_DB_IDX_TYPE_TPE_EXT: {
			const struct hw_db_inline_tpe_ext_data *data =
					&db->tpe_ext[idxs[i].ids].data;
			const int rpl_rpl_length = ((int)data->size + 15) / 16;
			fprintf(file, "  TPE_EXT %" PRIu32 "\n", idxs[i].ids);
			fprintf(file, "    Encap data, size %u\n", data->size);

			for (int i = 0; i < rpl_rpl_length; ++i) {
				fprintf(file, "   ");

				for (int n = 15; n >= 0; --n)
					fprintf(file, " %02x%s", data->hdr8[i * 16 + n],
						n == 8 ? " " : "");

				fprintf(file, "\n");
			}

			break;
		}

		case HW_DB_IDX_TYPE_FLM_RCP: {
			const struct hw_db_inline_flm_rcp_data *data = &db->flm[idxs[i].id1].data;
			fprintf(file, "  FLM_RCP %" PRIu32 "\n", idxs[i].id1);
			fprintf(file, "    QW0 dyn %" PRIu64 ", ofs %" PRIu64
				 ", QW4 dyn %" PRIu64 ", ofs %" PRIu64 "\n",
				(uint64_t)data->qw0_dyn, (uint64_t)data->qw0_ofs,
				(uint64_t)data->qw4_dyn, (uint64_t)data->qw4_ofs);
			fprintf(file, "    SW8 dyn %" PRIu64 ", ofs %" PRIu64
				 ", SW9 dyn %" PRIu64 ", ofs %" PRIu64 "\n",
				(uint64_t)data->sw8_dyn, (uint64_t)data->sw8_ofs,
				(uint64_t)data->sw9_dyn, (uint64_t)data->sw9_ofs);
			fprintf(file, "    Outer prot  %" PRIu64 ", inner prot   %" PRIu64 "\n",
				(uint64_t)data->outer_prot, (uint64_t)data->inner_prot);
			fprintf(file, "    Mask:\n");
			fprintf(file, "      %08x %08x %08x %08x %08x\n",
				(uint32_t)data->mask[0], (uint32_t)data->mask[1],
				(uint32_t)data->mask[2], (uint32_t)data->mask[3],
				(uint32_t)data->mask[4]);
			fprintf(file, "      %08x %08x %08x %08x %08x\n",
				(uint32_t)data->mask[5], (uint32_t)data->mask[6],
				(uint32_t)data->mask[7], (uint32_t)data->mask[8],
				(uint32_t)data->mask[9]);
			break;
		}

		case HW_DB_IDX_TYPE_FLM_FT: {
			const struct hw_db_inline_flm_ft_data *data =
					&db->flm[idxs[i].id2].ft[idxs[i].id1].data;
			fprintf(file, "  FLM_FT %" PRIu32 "\n", idxs[i].id1);

			if (data->is_group_zero)
				fprintf(file, "    Jump to %d\n", data->jump);

			else
				fprintf(file, "    Group %d\n", data->group);

			fprintf(file, "    ACTION_SET id %d\n", data->action_set.ids);
			break;
		}

		case HW_DB_IDX_TYPE_KM_RCP: {
			const struct hw_db_inline_km_rcp_data *data = &db->km[idxs[i].id1].data;
			fprintf(file, "  KM_RCP %" PRIu32 "\n", idxs[i].id1);
			fprintf(file, "    HW id %u\n", data->rcp);
			break;
		}

		case HW_DB_IDX_TYPE_KM_FT: {
			const struct hw_db_inline_km_ft_data *data =
					&db->km[idxs[i].id2].ft[idxs[i].id1].data;
			fprintf(file, "  KM_FT %" PRIu32 "\n", idxs[i].id1);
			fprintf(file, "    ACTION_SET id %d\n", data->action_set.ids);
			fprintf(file, "    KM_RCP id %" PRIu32 "\n", data->km.ids);
			fprintf(file, "    CAT id %" PRIu32 "\n", data->cat.ids);
			break;
		}

		case HW_DB_IDX_TYPE_FLM_SCRUB: {
			const struct hw_db_inline_scrub_data *data = &db->scrub[idxs[i].ids].data;
			fprintf(file, "  FLM_RCP %" PRIu32 "\n", idxs[i].id1);
			fprintf(file, "  SCRUB %" PRIu32 "\n", idxs[i].ids);
			fprintf(file, "    Timeout: %" PRIu32 ", encoded timeout: %" PRIu32 "\n",
				hw_mod_flm_scrub_timeout_decode(data->timeout), data->timeout);
			break;
		}

		case HW_DB_IDX_TYPE_HSH: {
			const struct hw_db_inline_hsh_data *data = &db->hsh[idxs[i].ids].data;
			fprintf(file, "  HSH %" PRIu32 "\n", idxs[i].ids);

			switch (data->func) {
			case RTE_ETH_HASH_FUNCTION_DEFAULT:
				fprintf(file, "    Func: NTH10\n");
				break;

			case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
				fprintf(file, "    Func: Toeplitz\n");
				fprintf(file, "    Key:");

				for (uint8_t i = 0; i < MAX_RSS_KEY_LEN; i++) {
					if (i % 10 == 0)
						fprintf(file, "\n     ");

					fprintf(file, " %02x", data->key[i]);
				}

				fprintf(file, "\n");
				break;

			default:
				fprintf(file, "    Func: %u\n", data->func);
			}

			fprintf(file, "    Hash mask hex:\n");
			fprintf(file, "      %016lx\n", data->hash_mask);

			/* convert hash mask to human readable RTE_ETH_RSS_* form if possible */
			if (sprint_nt_rss_mask(str_buffer, rss_buffer_len, "\n      ",
					data->hash_mask) == 0) {
				fprintf(file, "    Hash mask flags:%s\n", str_buffer);
			}

			break;
		}

		default: {
			fprintf(file, "  Unknown item. Type %u\n", idxs[i].type);
			break;
		}
		}
	}
}

void hw_db_inline_dump_cfn(struct flow_nic_dev *ndev, void *db_handle, FILE *file)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	fprintf(file, "CFN status:\n");

	for (uint32_t id = 0; id < db->nb_cat; ++id)
		if (db->cfn[id].cfn_hw)
			fprintf(file, "  ID %d, HW id %d, priority 0x%" PRIx64 "\n", (int)id,
				db->cfn[id].cfn_hw, db->cfn[id].priority);
}

const void *hw_db_inline_find_data(struct flow_nic_dev *ndev, void *db_handle,
	enum hw_db_idx_type type, struct hw_db_idx *idxs, uint32_t size)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	for (uint32_t i = 0; i < size; ++i) {
		if (idxs[i].type != type)
			continue;

		switch (type) {
		case HW_DB_IDX_TYPE_NONE:
			return NULL;

		case HW_DB_IDX_TYPE_MATCH_SET:
			return &db->match_set[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_ACTION_SET:
			return &db->action_set[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_CAT:
			return &db->cat[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_QSL:
			return &db->qsl[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_COT:
			return &db->cot[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_SLC_LR:
			return &db->slc_lr[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_TPE:
			return &db->tpe[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_TPE_EXT:
			return &db->tpe_ext[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_FLM_RCP:
			return &db->flm[idxs[i].id1].data;

		case HW_DB_IDX_TYPE_FLM_FT:
			return NULL;	/* FTs can't be easily looked up */

		case HW_DB_IDX_TYPE_KM_RCP:
			return &db->km[idxs[i].id1].data;

		case HW_DB_IDX_TYPE_KM_FT:
			return NULL;	/* FTs can't be easily looked up */

		case HW_DB_IDX_TYPE_HSH:
			return &db->hsh[idxs[i].ids].data;

		case HW_DB_IDX_TYPE_FLM_SCRUB:
			return &db->scrub[idxs[i].ids].data;

		default:
			return NULL;
		}
	}

	return NULL;
}

/******************************************************************************/
/* Filter                                                                     */
/******************************************************************************/

/*
 * lookup refers to key A/B/C/D, and can have values 0, 1, 2, and 3.
 */
static void hw_db_set_ft(struct flow_nic_dev *ndev, int type, int cfn_index, int lookup,
	int flow_type, int enable)
{
	(void)type;
	(void)enable;

	const int max_lookups = 4;
	const int cat_funcs = (int)ndev->be.cat.nb_cat_funcs / 8;

	int fte_index = (8 * flow_type + cfn_index / cat_funcs) * max_lookups + lookup;
	int fte_field = cfn_index % cat_funcs;

	uint32_t current_bm = 0;
	uint32_t fte_field_bm = 1 << fte_field;

	switch (type) {
	case HW_DB_FT_TYPE_FLM:
		hw_mod_cat_fte_flm_get(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST, fte_index,
			&current_bm);
		break;

	case HW_DB_FT_TYPE_KM:
		hw_mod_cat_fte_km_get(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST, fte_index,
			&current_bm);
		break;

	default:
		break;
	}

	uint32_t final_bm = enable ? (fte_field_bm | current_bm) : (~fte_field_bm & current_bm);

	if (current_bm != final_bm) {
		switch (type) {
		case HW_DB_FT_TYPE_FLM:
			hw_mod_cat_fte_flm_set(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
				fte_index, final_bm);
			hw_mod_cat_fte_flm_flush(&ndev->be, KM_FLM_IF_FIRST, fte_index, 1);
			break;

		case HW_DB_FT_TYPE_KM:
			hw_mod_cat_fte_km_set(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
				fte_index, final_bm);
			hw_mod_cat_fte_km_flush(&ndev->be, KM_FLM_IF_FIRST, fte_index, 1);
			break;

		default:
			break;
		}
	}
}

/*
 * Setup a filter to match:
 *    All packets in CFN checks
 *    All packets in KM
 *    All packets in FLM with look-up C FT equal to specified argument
 *
 * Setup a QSL recipe to DROP all matching packets
 *
 * Note: QSL recipe 0 uses DISCARD in order to allow for exception paths (UNMQ)
 *       Consequently another QSL recipe with hard DROP is needed
 */
int hw_db_inline_setup_mbr_filter(struct flow_nic_dev *ndev, uint32_t cat_hw_id, uint32_t ft,
	uint32_t qsl_hw_id)
{
	(void)ft;
	(void)qsl_hw_id;
	(void)ft;

	const int offset = ((int)ndev->be.cat.cts_num + 1) / 2;
	(void)offset;

	/* QSL for traffic policing */
	if (hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DROP, qsl_hw_id, 0x3) < 0)
		return -1;

	if (hw_mod_qsl_rcp_flush(&ndev->be, qsl_hw_id, 1) < 0)
		return -1;

	/* Select and enable QSL recipe */
	if (hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 1, qsl_hw_id))
		return -1;

	if (hw_mod_cat_cts_flush(&ndev->be, offset * cat_hw_id, 6))
		return -1;

	if (hw_mod_cat_cte_set(&ndev->be, HW_CAT_CTE_ENABLE_BM, cat_hw_id, 0x8))
		return -1;

	if (hw_mod_cat_cte_flush(&ndev->be, cat_hw_id, 1))
		return -1;

	/* KM: Match all FTs for look-up A */
	for (int i = 0; i < 16; ++i)
		hw_db_set_ft(ndev, HW_DB_FT_TYPE_KM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_A, i, 1);

	/* FLM: Match all FTs for look-up A */
	for (int i = 0; i < 16; ++i)
		hw_db_set_ft(ndev, HW_DB_FT_TYPE_FLM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_A, i, 1);

	/* FLM: Match FT=ft_argument for look-up C */
	hw_db_set_ft(ndev, HW_DB_FT_TYPE_FLM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_C, ft, 1);

	/* Make all CFN checks TRUE */
	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_SET_ALL_DEFAULTS, cat_hw_id, 0, 0))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ENABLE, cat_hw_id, 0, 0x1))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_L3, cat_hw_id, 0, 0x0))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_INV, cat_hw_id, 0, 0x1))
		return -1;

	/* Final match: look-up_A == TRUE && look-up_C == TRUE */
	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_KM0_OR, cat_hw_id, 0, 0x1))
		return -1;

	if (hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_KM1_OR, cat_hw_id, 0, 0x3))
		return -1;

	if (hw_mod_cat_cfn_flush(&ndev->be, cat_hw_id, 1))
		return -1;

	return 0;
}

static void hw_db_inline_setup_default_flm_rcp(struct flow_nic_dev *ndev, int flm_rcp)
{
	uint32_t flm_mask[10];
	memset(flm_mask, 0xff, sizeof(flm_mask));

	hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_PRESET_ALL, flm_rcp, 0x0);
	hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_LOOKUP, flm_rcp, 1);
	hw_mod_flm_rcp_set_mask(&ndev->be, HW_FLM_RCP_MASK, flm_rcp, flm_mask);
	hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_KID, flm_rcp, flm_rcp + 2);

	hw_mod_flm_rcp_flush(&ndev->be, flm_rcp, 1);
}


static void hw_db_copy_ft(struct flow_nic_dev *ndev, int type, int cfn_dst, int cfn_src,
	int lookup, int flow_type)
{
	const int max_lookups = 4;
	const int cat_funcs = (int)ndev->be.cat.nb_cat_funcs / 8;

	int fte_index_dst = (8 * flow_type + cfn_dst / cat_funcs) * max_lookups + lookup;
	int fte_field_dst = cfn_dst % cat_funcs;

	int fte_index_src = (8 * flow_type + cfn_src / cat_funcs) * max_lookups + lookup;
	int fte_field_src = cfn_src % cat_funcs;

	uint32_t current_bm_dst = 0;
	uint32_t current_bm_src = 0;
	uint32_t fte_field_bm_dst = 1 << fte_field_dst;
	uint32_t fte_field_bm_src = 1 << fte_field_src;

	switch (type) {
	case HW_DB_FT_TYPE_FLM:
		hw_mod_cat_fte_flm_get(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
			fte_index_dst, &current_bm_dst);
		hw_mod_cat_fte_flm_get(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
			fte_index_src, &current_bm_src);
		break;

	case HW_DB_FT_TYPE_KM:
		hw_mod_cat_fte_km_get(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
			fte_index_dst, &current_bm_dst);
		hw_mod_cat_fte_km_get(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
			fte_index_src, &current_bm_src);
		break;

	default:
		break;
	}

	uint32_t enable = current_bm_src & fte_field_bm_src;
	uint32_t final_bm_dst = enable ? (fte_field_bm_dst | current_bm_dst)
		: (~fte_field_bm_dst & current_bm_dst);

	if (current_bm_dst != final_bm_dst) {
		switch (type) {
		case HW_DB_FT_TYPE_FLM:
			hw_mod_cat_fte_flm_set(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
				fte_index_dst, final_bm_dst);
			hw_mod_cat_fte_flm_flush(&ndev->be, KM_FLM_IF_FIRST, fte_index_dst, 1);
			break;

		case HW_DB_FT_TYPE_KM:
			hw_mod_cat_fte_km_set(&ndev->be, HW_CAT_FTE_ENABLE_BM, KM_FLM_IF_FIRST,
				fte_index_dst, final_bm_dst);
			hw_mod_cat_fte_km_flush(&ndev->be, KM_FLM_IF_FIRST, fte_index_dst, 1);
			break;

		default:
			break;
		}
	}
}


static int hw_db_inline_filter_apply(struct flow_nic_dev *ndev,
	struct hw_db_inline_resource_db *db,
	int cat_hw_id,
	struct hw_db_match_set_idx match_set_idx,
	struct hw_db_flm_ft flm_ft_idx,
	struct hw_db_action_set_idx action_set_idx)
{
	(void)match_set_idx;
	(void)flm_ft_idx;

	const struct hw_db_inline_match_set_data *match_set =
			&db->match_set[match_set_idx.ids].data;
	const struct hw_db_inline_cat_data *cat = &db->cat[match_set->cat.ids].data;

	const int km_ft = match_set->km_ft.id1;
	const int km_rcp = (int)db->km[match_set->km.id1].data.rcp;

	const int flm_ft = flm_ft_idx.id1;
	const int flm_rcp = flm_ft_idx.id2;

	const struct hw_db_inline_action_set_data *action_set =
			&db->action_set[action_set_idx.ids].data;
	const struct hw_db_inline_cot_data *cot = &db->cot[action_set->cot.ids].data;

	const int qsl_hw_id = action_set->qsl.ids;
	const int slc_lr_hw_id = action_set->slc_lr.ids;
	const int tpe_hw_id = action_set->tpe.ids;
	const int hsh_hw_id = action_set->hsh.ids;

	/* Setup default FLM RCP if needed */
	if (flm_rcp > 0 && db->flm[flm_rcp].ref <= 0)
		hw_db_inline_setup_default_flm_rcp(ndev, flm_rcp);

	/* Setup CAT.CFN */
	{
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_SET_ALL_DEFAULTS, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ENABLE, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_INV, cat_hw_id, 0, 0x0);

		/* Protocol checks */
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_INV, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_ISL, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_CFP, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_MAC, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_L2, cat_hw_id, 0, cat->ptc_mask_l2);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_VNTAG, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_VLAN, cat_hw_id, 0, cat->vlan_mask);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_MPLS, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_L3, cat_hw_id, 0, cat->ptc_mask_l3);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_FRAG, cat_hw_id, 0,
			cat->ptc_mask_frag);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_IP_PROT, cat_hw_id, 0, cat->ip_prot);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_L4, cat_hw_id, 0, cat->ptc_mask_l4);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TUNNEL, cat_hw_id, 0,
			cat->ptc_mask_tunnel);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_L2, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_VLAN, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_MPLS, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_L3, cat_hw_id, 0,
			cat->ptc_mask_l3_tunnel);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_FRAG, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_IP_PROT, cat_hw_id, 0,
			cat->ip_prot_tunnel);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PTC_TNL_L4, cat_hw_id, 0,
			cat->ptc_mask_l4_tunnel);

		/* Error checks */
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_INV, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_CV, cat_hw_id, 0, 0x1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_FCS, cat_hw_id, 0, 0x1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_TRUNC, cat_hw_id, 0, 0x1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_L3_CS, cat_hw_id, 0, 0x1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_L4_CS, cat_hw_id, 0, 0x1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_TNL_L3_CS, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_TNL_L4_CS, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_TTL_EXP, cat_hw_id, 0,
			cat->err_mask_ttl);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ERR_TNL_TTL_EXP, cat_hw_id, 0,
			cat->err_mask_ttl_tunnel);

		/* MAC port check */
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_MAC_PORT, cat_hw_id, 0,
			cat->mac_port_mask);

		/* Pattern match checks */
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_CMP, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_DCT, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_EXT_INV, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_CMB, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_AND_INV, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_OR_INV, cat_hw_id, 0, -1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_PM_INV, cat_hw_id, 0, -1);

		/* Length checks */
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_LC, cat_hw_id, 0, 0x0);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_LC_INV, cat_hw_id, 0, -1);

		/* KM and FLM */
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_KM0_OR, cat_hw_id, 0, 0x1);
		hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_KM1_OR, cat_hw_id, 0, 0x3);

		hw_mod_cat_cfn_flush(&ndev->be, cat_hw_id, 1);
	}

	/* Setup CAT.CTS */
	{
		const int offset = ((int)ndev->be.cat.cts_num + 1) / 2;

		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + 0, cat_hw_id);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 0, 0);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + 1, hsh_hw_id);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 1, qsl_hw_id);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + 2, 0);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 2,
			slc_lr_hw_id);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + 3, 0);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 3, 0);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + 4, 0);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 4, 0);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + 5, tpe_hw_id);
		hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + 5, 0);

		hw_mod_cat_cts_flush(&ndev->be, offset * cat_hw_id, 6);
	}

	/* Setup CAT.CTE */
	{
		hw_mod_cat_cte_set(&ndev->be, HW_CAT_CTE_ENABLE_BM, cat_hw_id,
			0x001 | 0x004 | (qsl_hw_id ? 0x008 : 0) |
			(slc_lr_hw_id ? 0x020 : 0) | 0x040 |
			(tpe_hw_id ? 0x400 : 0));
		hw_mod_cat_cte_flush(&ndev->be, cat_hw_id, 1);
	}

	/* Setup CAT.KM */
	{
		uint32_t bm = 0;

		hw_mod_cat_kcs_km_set(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cat_hw_id,
			km_rcp);
		hw_mod_cat_kcs_km_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id, 1);

		hw_mod_cat_kce_km_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, &bm);
		hw_mod_cat_kce_km_set(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, bm | (1 << (cat_hw_id % 8)));
		hw_mod_cat_kce_km_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id / 8, 1);

		hw_db_set_ft(ndev, HW_DB_FT_TYPE_KM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_A, km_ft, 1);
	}

	/* Setup CAT.FLM */
	{
		uint32_t bm = 0;

		hw_mod_cat_kcs_flm_set(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cat_hw_id,
			flm_rcp);
		hw_mod_cat_kcs_flm_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id, 1);

		hw_mod_cat_kce_flm_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, &bm);
		hw_mod_cat_kce_flm_set(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, bm | (1 << (cat_hw_id % 8)));
		hw_mod_cat_kce_flm_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id / 8, 1);

		hw_db_set_ft(ndev, HW_DB_FT_TYPE_FLM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_A, km_ft, 1);
		hw_db_set_ft(ndev, HW_DB_FT_TYPE_FLM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_C, flm_ft, 1);
	}

	/* Setup CAT.COT */
	{
		hw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_PRESET_ALL, cat_hw_id, 0);
		hw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_COLOR, cat_hw_id, cot->frag_rcp << 10);
		hw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_KM, cat_hw_id,
			cot->matcher_color_contrib);
		hw_mod_cat_cot_flush(&ndev->be, cat_hw_id, 1);
	}

	hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ENABLE, cat_hw_id, 0, 0x1);
	hw_mod_cat_cfn_flush(&ndev->be, cat_hw_id, 1);

	return 0;
}

static void hw_db_inline_filter_clear(struct flow_nic_dev *ndev,
	struct hw_db_inline_resource_db *db,
	int cat_hw_id)
{
	/* Setup CAT.CFN */
	hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_SET_ALL_DEFAULTS, cat_hw_id, 0, 0x0);
	hw_mod_cat_cfn_flush(&ndev->be, cat_hw_id, 1);

	/* Setup CAT.CTS */
	{
		const int offset = ((int)ndev->be.cat.cts_num + 1) / 2;

		for (int i = 0; i < 6; ++i) {
			hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cat_hw_id + i, 0);
			hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cat_hw_id + i, 0);
		}

		hw_mod_cat_cts_flush(&ndev->be, offset * cat_hw_id, 6);
	}

	/* Setup CAT.CTE */
	{
		hw_mod_cat_cte_set(&ndev->be, HW_CAT_CTE_ENABLE_BM, cat_hw_id, 0);
		hw_mod_cat_cte_flush(&ndev->be, cat_hw_id, 1);
	}

	/* Setup CAT.KM */
	{
		uint32_t bm = 0;

		hw_mod_cat_kcs_km_set(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cat_hw_id,
			0);
		hw_mod_cat_kcs_km_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id, 1);

		hw_mod_cat_kce_km_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, &bm);
		hw_mod_cat_kce_km_set(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, bm & ~(1 << (cat_hw_id % 8)));
		hw_mod_cat_kce_km_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id / 8, 1);

		for (int ft = 0; ft < (int)db->nb_km_ft; ++ft) {
			hw_db_set_ft(ndev, HW_DB_FT_TYPE_KM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_A, ft,
				0);
		}
	}

	/* Setup CAT.FLM */
	{
		uint32_t bm = 0;

		hw_mod_cat_kcs_flm_set(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cat_hw_id,
			0);
		hw_mod_cat_kcs_flm_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id, 1);

		hw_mod_cat_kce_flm_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, &bm);
		hw_mod_cat_kce_flm_set(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cat_hw_id / 8, bm & ~(1 << (cat_hw_id % 8)));
		hw_mod_cat_kce_flm_flush(&ndev->be, KM_FLM_IF_FIRST, cat_hw_id / 8, 1);

		for (int ft = 0; ft < (int)db->nb_flm_ft; ++ft) {
			hw_db_set_ft(ndev, HW_DB_FT_TYPE_FLM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_A, ft,
				0);
			hw_db_set_ft(ndev, HW_DB_FT_TYPE_FLM, cat_hw_id, HW_DB_FT_LOOKUP_KEY_C, ft,
				0);
		}
	}

	hw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_PRESET_ALL, cat_hw_id, 0);
	hw_mod_cat_cot_flush(&ndev->be, cat_hw_id, 1);
}

static void hw_db_inline_filter_copy(struct flow_nic_dev *ndev,
	struct hw_db_inline_resource_db *db, int cfn_dst, int cfn_src)
{
	uint32_t val = 0;

	hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_COPY_FROM, cfn_dst, 0, cfn_src);
	hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ENABLE, cfn_dst, 0, 0x0);
	hw_mod_cat_cfn_flush(&ndev->be, cfn_dst, 1);

	/* Setup CAT.CTS */
	{
		const int offset = ((int)ndev->be.cat.cts_num + 1) / 2;

		for (int i = 0; i < offset; ++i) {
			hw_mod_cat_cts_get(&ndev->be, HW_CAT_CTS_CAT_A, offset * cfn_src + i,
				&val);
			hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_A, offset * cfn_dst + i, val);
			hw_mod_cat_cts_get(&ndev->be, HW_CAT_CTS_CAT_B, offset * cfn_src + i,
				&val);
			hw_mod_cat_cts_set(&ndev->be, HW_CAT_CTS_CAT_B, offset * cfn_dst + i, val);
		}

		hw_mod_cat_cts_flush(&ndev->be, offset * cfn_dst, offset);
	}

	/* Setup CAT.CTE */
	{
		hw_mod_cat_cte_get(&ndev->be, HW_CAT_CTE_ENABLE_BM, cfn_src, &val);
		hw_mod_cat_cte_set(&ndev->be, HW_CAT_CTE_ENABLE_BM, cfn_dst, val);
		hw_mod_cat_cte_flush(&ndev->be, cfn_dst, 1);
	}

	/* Setup CAT.KM */
	{
		uint32_t bit_src = 0;

		hw_mod_cat_kcs_km_get(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cfn_src,
			&val);
		hw_mod_cat_kcs_km_set(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cfn_dst,
			val);
		hw_mod_cat_kcs_km_flush(&ndev->be, KM_FLM_IF_FIRST, cfn_dst, 1);

		hw_mod_cat_kce_km_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cfn_src / 8, &val);
		bit_src = (val >> (cfn_src % 8)) & 0x1;

		hw_mod_cat_kce_km_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cfn_dst / 8, &val);
		val &= ~(1 << (cfn_dst % 8));

		hw_mod_cat_kce_km_set(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cfn_dst / 8, val | (bit_src << (cfn_dst % 8)));
		hw_mod_cat_kce_km_flush(&ndev->be, KM_FLM_IF_FIRST, cfn_dst / 8, 1);

		for (int ft = 0; ft < (int)db->nb_km_ft; ++ft) {
			hw_db_copy_ft(ndev, HW_DB_FT_TYPE_KM, cfn_dst, cfn_src,
				HW_DB_FT_LOOKUP_KEY_A, ft);
		}
	}

	/* Setup CAT.FLM */
	{
		uint32_t bit_src = 0;

		hw_mod_cat_kcs_flm_get(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cfn_src,
			&val);
		hw_mod_cat_kcs_flm_set(&ndev->be, HW_CAT_KCS_CATEGORY, KM_FLM_IF_FIRST, cfn_dst,
			val);
		hw_mod_cat_kcs_flm_flush(&ndev->be, KM_FLM_IF_FIRST, cfn_dst, 1);

		hw_mod_cat_kce_flm_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cfn_src / 8, &val);
		bit_src = (val >> (cfn_src % 8)) & 0x1;

		hw_mod_cat_kce_flm_get(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cfn_dst / 8, &val);
		val &= ~(1 << (cfn_dst % 8));

		hw_mod_cat_kce_flm_set(&ndev->be, HW_CAT_KCE_ENABLE_BM, KM_FLM_IF_FIRST,
			cfn_dst / 8, val | (bit_src << (cfn_dst % 8)));
		hw_mod_cat_kce_flm_flush(&ndev->be, KM_FLM_IF_FIRST, cfn_dst / 8, 1);

		for (int ft = 0; ft < (int)db->nb_flm_ft; ++ft) {
			hw_db_copy_ft(ndev, HW_DB_FT_TYPE_FLM, cfn_dst, cfn_src,
				HW_DB_FT_LOOKUP_KEY_A, ft);
			hw_db_copy_ft(ndev, HW_DB_FT_TYPE_FLM, cfn_dst, cfn_src,
				HW_DB_FT_LOOKUP_KEY_C, ft);
		}
	}

	/* Setup CAT.COT */
	{
		hw_mod_cat_cot_set(&ndev->be, HW_CAT_COT_COPY_FROM, cfn_dst, cfn_src);
		hw_mod_cat_cot_flush(&ndev->be, cfn_dst, 1);
	}

	hw_mod_cat_cfn_set(&ndev->be, HW_CAT_CFN_ENABLE, cfn_dst, 0, 0x1);
	hw_mod_cat_cfn_flush(&ndev->be, cfn_dst, 1);
}

/*
 * Algorithm for moving CFN entries to make space with respect of priority.
 * The algorithm will make the fewest possible moves to fit a new CFN entry.
 */
static int hw_db_inline_alloc_prioritized_cfn(struct flow_nic_dev *ndev,
	struct hw_db_inline_resource_db *db,
	struct hw_db_match_set_idx match_set_idx)
{
	const struct hw_db_inline_resource_db_match_set *match_set =
			&db->match_set[match_set_idx.ids];

	uint64_t priority = ((uint64_t)(match_set->data.priority & 0xff) << 56) |
		((uint64_t)(0xffffff - (match_set->set_priority & 0xffffff)) << 32) |
		(0xffffffff - ++db->cfn_priority_counter);

	int db_cfn_idx = -1;

	struct {
		uint64_t priority;
		uint32_t idx;
	} *sorted_priority;

	sorted_priority = calloc(db->nb_cat, sizeof(*sorted_priority));
	if (!sorted_priority)
		return -1;

	uint32_t in_use_count = 0;

	for (uint32_t i = 1; i < db->nb_cat; ++i) {
		if (db->cfn[i].ref > 0) {
			sorted_priority[db->cfn[i].cfn_hw].priority = db->cfn[i].priority;
			sorted_priority[db->cfn[i].cfn_hw].idx = i;
			in_use_count += 1;

		} else if (db_cfn_idx == -1) {
			db_cfn_idx = (int)i;
		}
	}

	if (in_use_count >= db->nb_cat - 1) {
		free(sorted_priority);
		return -1;
	}

	if (in_use_count == 0) {
		db->cfn[db_cfn_idx].ref = 1;
		db->cfn[db_cfn_idx].cfn_hw = 1;
		db->cfn[db_cfn_idx].priority = priority;
		free(sorted_priority);
		return db_cfn_idx;
	}

	int goal = 1;
	int free_before = -1000000;
	int free_after = 1000000;
	int found_smaller = 0;

	for (int i = 1; i < (int)db->nb_cat; ++i) {
		if (sorted_priority[i].priority > priority) {	/* Bigger */
			goal = i + 1;

		} else if (sorted_priority[i].priority == 0) {	/* Not set */
			if (found_smaller) {
				if (free_after > i)
					free_after = i;

			} else {
				free_before = i;
			}

		} else {/* Smaller */
			found_smaller = 1;
		}
	}

	int diff_before = goal - free_before - 1;
	int diff_after = free_after - goal;

	if (goal < (int)db->nb_cat && sorted_priority[goal].priority == 0) {
		db->cfn[db_cfn_idx].ref = 1;
		db->cfn[db_cfn_idx].cfn_hw = goal;
		db->cfn[db_cfn_idx].priority = priority;
		free(sorted_priority);
		return db_cfn_idx;
	}

	if (diff_after <= diff_before) {
		for (int i = free_after; i > goal; --i) {
			int *cfn_hw = &db->cfn[sorted_priority[i - 1].idx].cfn_hw;
			hw_db_inline_filter_copy(ndev, db, i, *cfn_hw);
			hw_db_inline_filter_clear(ndev, db, *cfn_hw);
			*cfn_hw = i;
		}

	} else {
		goal -= 1;

		for (int i = free_before; i < goal; ++i) {
			int *cfn_hw = &db->cfn[sorted_priority[i + 1].idx].cfn_hw;
			hw_db_inline_filter_copy(ndev, db, i, *cfn_hw);
			hw_db_inline_filter_clear(ndev, db, *cfn_hw);
			*cfn_hw = i;
		}
	}

	db->cfn[db_cfn_idx].ref = 1;
	db->cfn[db_cfn_idx].cfn_hw = goal;
	db->cfn[db_cfn_idx].priority = priority;
	free(sorted_priority);

	return db_cfn_idx;
}

static void hw_db_inline_free_prioritized_cfn(struct hw_db_inline_resource_db *db, int cfn_hw)
{
	for (uint32_t i = 0; i < db->nb_cat; ++i) {
		if (db->cfn[i].cfn_hw == cfn_hw) {
			memset(&db->cfn[i], 0x0, sizeof(struct hw_db_inline_resource_db_cfn));
			break;
		}
	}
}

static void hw_db_inline_update_active_filters(struct flow_nic_dev *ndev, void *db_handle,
	int group)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_flm_rcp *flm_rcp = &db->flm[group];
	struct hw_db_inline_resource_db_flm_cfn_map *cell;

	for (uint32_t match_set_idx = 0; match_set_idx < db->nb_cat; ++match_set_idx) {
		for (uint32_t ft_idx = 0; ft_idx < db->nb_flm_ft; ++ft_idx) {
			int active = flm_rcp->ft[ft_idx].ref > 0 &&
				flm_rcp->match_set[match_set_idx].ref > 0;
			cell = &flm_rcp->cfn_map[match_set_idx * db->nb_flm_ft + ft_idx];

			if (active && cell->cfn_idx == 0) {
				/* Setup filter */
				cell->cfn_idx = hw_db_inline_alloc_prioritized_cfn(ndev, db,
					flm_rcp->match_set[match_set_idx].idx);
				hw_db_inline_filter_apply(ndev, db, db->cfn[cell->cfn_idx].cfn_hw,
					flm_rcp->match_set[match_set_idx].idx,
					flm_rcp->ft[ft_idx].idx,
					group == 0
					? db->match_set[flm_rcp->match_set[match_set_idx]
					.idx.ids]
					.data.action_set
					: flm_rcp->ft[ft_idx].data.action_set);
			}

			if (!active && cell->cfn_idx > 0) {
				/* Teardown filter */
				hw_db_inline_filter_clear(ndev, db, db->cfn[cell->cfn_idx].cfn_hw);
				hw_db_inline_free_prioritized_cfn(db,
					db->cfn[cell->cfn_idx].cfn_hw);
				cell->cfn_idx = 0;
			}
		}
	}
}


/******************************************************************************/
/* Match set                                                                  */
/******************************************************************************/

static int hw_db_inline_match_set_compare(const struct hw_db_inline_match_set_data *data1,
	const struct hw_db_inline_match_set_data *data2)
{
	return data1->cat.raw == data2->cat.raw && data1->km.raw == data2->km.raw &&
		data1->km_ft.raw == data2->km_ft.raw && data1->jump == data2->jump;
}

static void hw_db_inline_match_set_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_match_set_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->match_set[idx.ids].ref += 1;
}

struct hw_db_match_set_idx
hw_db_inline_match_set_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_match_set_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_flm_rcp *flm_rcp = &db->flm[data->jump];
	struct hw_db_match_set_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_MATCH_SET;

	for (uint32_t i = 0; i < HW_DB_INLINE_MATCH_SET_NB; ++i) {
		if (!found && db->match_set[i].ref <= 0) {
			found = 1;
			idx.ids = i;
		}

		if (db->match_set[i].ref > 0 &&
			hw_db_inline_match_set_compare(data, &db->match_set[i].data)) {
			idx.ids = i;
			hw_db_inline_match_set_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	found = 0;

	for (uint32_t i = 0; i < db->nb_cat; ++i) {
		if (flm_rcp->match_set[i].ref <= 0) {
			found = 1;
			flm_rcp->match_set[i].ref = 1;
			flm_rcp->match_set[i].idx.raw = idx.raw;
			break;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&db->match_set[idx.ids].data, data, sizeof(struct hw_db_inline_match_set_data));
	db->match_set[idx.ids].ref = 1;
	db->match_set[idx.ids].set_priority = ++db->set_priority_counter;

	hw_db_inline_update_active_filters(ndev, db, data->jump);

	return idx;
}

static void hw_db_inline_match_set_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_match_set_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_flm_rcp *flm_rcp;
	int jump;

	if (idx.error)
		return;

	db->match_set[idx.ids].ref -= 1;

	if (db->match_set[idx.ids].ref > 0)
		return;

	jump = db->match_set[idx.ids].data.jump;
	flm_rcp = &db->flm[jump];

	for (uint32_t i = 0; i < db->nb_cat; ++i) {
		if (flm_rcp->match_set[i].idx.raw == idx.raw) {
			flm_rcp->match_set[i].ref = 0;
			hw_db_inline_update_active_filters(ndev, db, jump);
			memset(&flm_rcp->match_set[i], 0x0,
				sizeof(struct hw_db_inline_resource_db_flm_match_set));
		}
	}

	memset(&db->match_set[idx.ids].data, 0x0, sizeof(struct hw_db_inline_match_set_data));
	db->match_set[idx.ids].ref = 0;
}

/******************************************************************************/
/* Action set                                                                 */
/******************************************************************************/

static int hw_db_inline_action_set_compare(const struct hw_db_inline_action_set_data *data1,
	const struct hw_db_inline_action_set_data *data2)
{
	if (data1->contains_jump)
		return data2->contains_jump && data1->jump == data2->jump;

	return data1->cot.raw == data2->cot.raw && data1->qsl.raw == data2->qsl.raw &&
		data1->slc_lr.raw == data2->slc_lr.raw && data1->tpe.raw == data2->tpe.raw &&
		data1->hsh.raw == data2->hsh.raw && data1->scrub.raw == data2->scrub.raw;
}

static void hw_db_inline_action_set_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_action_set_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->action_set[idx.ids].ref += 1;
}

struct hw_db_action_set_idx
hw_db_inline_action_set_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_action_set_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_action_set_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_ACTION_SET;

	for (uint32_t i = 0; i < HW_DB_INLINE_ACTION_SET_NB; ++i) {
		if (!found && db->action_set[i].ref <= 0) {
			found = 1;
			idx.ids = i;
		}

		if (db->action_set[i].ref > 0 &&
			hw_db_inline_action_set_compare(data, &db->action_set[i].data)) {
			idx.ids = i;
			hw_db_inline_action_set_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&db->action_set[idx.ids].data, data, sizeof(struct hw_db_inline_action_set_data));
	db->action_set[idx.ids].ref = 1;

	return idx;
}

static void hw_db_inline_action_set_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_action_set_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->action_set[idx.ids].ref -= 1;

	if (db->action_set[idx.ids].ref <= 0) {
		memset(&db->action_set[idx.ids].data, 0x0,
			sizeof(struct hw_db_inline_action_set_data));
		db->action_set[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* COT                                                                        */
/******************************************************************************/

static int hw_db_inline_cot_compare(const struct hw_db_inline_cot_data *data1,
	const struct hw_db_inline_cot_data *data2)
{
	return data1->matcher_color_contrib == data2->matcher_color_contrib &&
		data1->frag_rcp == data2->frag_rcp;
}

static void hw_db_inline_cot_ref(struct flow_nic_dev *ndev __rte_unused, void *db_handle,
	struct hw_db_cot_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->cot[idx.ids].ref += 1;
}

struct hw_db_cot_idx hw_db_inline_cot_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cot_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_cot_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_COT;

	for (uint32_t i = 1; i < db->nb_cot; ++i) {
		int ref = db->cot[i].ref;

		if (ref > 0 && hw_db_inline_cot_compare(data, &db->cot[i].data)) {
			idx.ids = i;
			hw_db_inline_cot_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->cot[idx.ids].ref = 1;
	memcpy(&db->cot[idx.ids].data, data, sizeof(struct hw_db_inline_cot_data));

	return idx;
}

static void hw_db_inline_cot_deref(struct flow_nic_dev *ndev __rte_unused, void *db_handle,
	struct hw_db_cot_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->cot[idx.ids].ref -= 1;

	if (db->cot[idx.ids].ref <= 0) {
		memset(&db->cot[idx.ids].data, 0x0, sizeof(struct hw_db_inline_cot_data));
		db->cot[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* QSL                                                                        */
/******************************************************************************/

/* Calculate queue mask for QSL TBL_MSK for given number of queues.
 * NOTE: If number of queues is not power of two, then queue mask will be created
 *       for nearest smaller power of two.
 */
static uint32_t queue_mask(uint32_t nr_queues)
{
	nr_queues |= nr_queues >> 1;
	nr_queues |= nr_queues >> 2;
	nr_queues |= nr_queues >> 4;
	nr_queues |= nr_queues >> 8;
	nr_queues |= nr_queues >> 16;
	return nr_queues >> 1;
}

static int hw_db_inline_qsl_compare(const struct hw_db_inline_qsl_data *data1,
	const struct hw_db_inline_qsl_data *data2)
{
	if (data1->discard != data2->discard || data1->drop != data2->drop ||
		data1->table_size != data2->table_size || data1->retransmit != data2->retransmit) {
		return 0;
	}

	for (int i = 0; i < HW_DB_INLINE_MAX_QST_PER_QSL; ++i) {
		if (data1->table[i].queue != data2->table[i].queue ||
			data1->table[i].queue_en != data2->table[i].queue_en ||
			data1->table[i].tx_port != data2->table[i].tx_port ||
			data1->table[i].tx_port_en != data2->table[i].tx_port_en) {
			return 0;
		}
	}

	return 1;
}

static void hw_db_inline_qsl_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_qsl_idx idx)
{
	(void)db_handle;

	if (!idx.error && idx.ids != 0)
		nthw_flow_nic_ref_resource(ndev, RES_QSL_RCP, idx.ids);
}

struct hw_db_qsl_idx hw_db_inline_qsl_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_qsl_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_qsl_idx qsl_idx = { .raw = 0 };
	uint32_t qst_idx = 0;
	int res;

	qsl_idx.type = HW_DB_IDX_TYPE_QSL;

	if (data->discard) {
		qsl_idx.ids = 0;
		return qsl_idx;
	}

	for (uint32_t i = 1; i < db->nb_qsl; ++i) {
		if (hw_db_inline_qsl_compare(data, &db->qsl[i].data)) {
			qsl_idx.ids = i;
			hw_db_inline_qsl_ref(ndev, db, qsl_idx);
			return qsl_idx;
		}
	}

	res = nthw_flow_nic_alloc_resource(ndev, RES_QSL_RCP, 1);

	if (res < 0) {
		qsl_idx.error = 1;
		return qsl_idx;
	}

	qsl_idx.ids = res & 0xff;

	if (data->table_size > 0) {
		res = nthw_flow_nic_alloc_resource_config(ndev, RES_QSL_QST, data->table_size, 1);

		if (res < 0) {
			nthw_flow_nic_deref_resource(ndev, RES_QSL_RCP, qsl_idx.ids);
			qsl_idx.error = 1;
			return qsl_idx;
		}

		qst_idx = (uint32_t)res;
	}

	memcpy(&db->qsl[qsl_idx.ids].data, data, sizeof(struct hw_db_inline_qsl_data));
	db->qsl[qsl_idx.ids].qst_idx = qst_idx;

	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_PRESET_ALL, qsl_idx.ids, 0x0);

	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DISCARD, qsl_idx.ids, data->discard);
	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_DROP, qsl_idx.ids, data->drop * 0x3);
	hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_LR, qsl_idx.ids, data->retransmit * 0x3);

	if (data->table_size == 0) {
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_LO, qsl_idx.ids, 0x0);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_HI, qsl_idx.ids, 0x0);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_IDX, qsl_idx.ids, 0x0);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_MSK, qsl_idx.ids, 0x0);

	} else {
		const uint32_t table_start = qst_idx;
		const uint32_t table_end = table_start + data->table_size - 1;

		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_LO, qsl_idx.ids, table_start);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_HI, qsl_idx.ids, table_end);

		/* Toeplitz hash function uses TBL_IDX and TBL_MSK. */
		uint32_t msk = queue_mask(table_end - table_start + 1);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_IDX, qsl_idx.ids, table_start);
		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_TBL_MSK, qsl_idx.ids, msk);

		for (uint32_t i = 0; i < data->table_size; ++i) {
			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_PRESET_ALL, table_start + i, 0x0);

			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_TX_PORT, table_start + i,
				data->table[i].tx_port);
			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_LRE, table_start + i,
				data->table[i].tx_port_en);

			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_QUEUE, table_start + i,
				data->table[i].queue);
			hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_EN, table_start + i,
				data->table[i].queue_en);
		}

		hw_mod_qsl_qst_flush(&ndev->be, table_start, data->table_size);
	}

	hw_mod_qsl_rcp_flush(&ndev->be, qsl_idx.ids, 1);

	return qsl_idx;
}

static void hw_db_inline_qsl_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_qsl_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error || idx.ids == 0)
		return;

	if (nthw_flow_nic_deref_resource(ndev, RES_QSL_RCP, idx.ids) == 0) {
		const int table_size = (int)db->qsl[idx.ids].data.table_size;

		hw_mod_qsl_rcp_set(&ndev->be, HW_QSL_RCP_PRESET_ALL, idx.ids, 0x0);
		hw_mod_qsl_rcp_flush(&ndev->be, idx.ids, 1);

		if (table_size > 0) {
			const int table_start = db->qsl[idx.ids].qst_idx;

			for (int i = 0; i < (int)table_size; ++i) {
				hw_mod_qsl_qst_set(&ndev->be, HW_QSL_QST_PRESET_ALL,
					table_start + i, 0x0);
				nthw_flow_nic_free_resource(ndev, RES_QSL_QST, table_start + i);
			}

			hw_mod_qsl_qst_flush(&ndev->be, table_start, table_size);
		}

		memset(&db->qsl[idx.ids].data, 0x0, sizeof(struct hw_db_inline_qsl_data));
		db->qsl[idx.ids].qst_idx = 0;
	}
}

/******************************************************************************/
/* SLC_LR                                                                     */
/******************************************************************************/

static int hw_db_inline_slc_lr_compare(const struct hw_db_inline_slc_lr_data *data1,
	const struct hw_db_inline_slc_lr_data *data2)
{
	if (!data1->head_slice_en)
		return data1->head_slice_en == data2->head_slice_en;

	return data1->head_slice_en == data2->head_slice_en &&
		data1->head_slice_dyn == data2->head_slice_dyn &&
		data1->head_slice_ofs == data2->head_slice_ofs;
}

static void hw_db_inline_slc_lr_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_slc_lr_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->slc_lr[idx.ids].ref += 1;
}

struct hw_db_slc_lr_idx hw_db_inline_slc_lr_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_slc_lr_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_slc_lr_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_SLC_LR;

	for (uint32_t i = 1; i < db->nb_slc_lr; ++i) {
		int ref = db->slc_lr[i].ref;

		if (ref > 0 && hw_db_inline_slc_lr_compare(data, &db->slc_lr[i].data)) {
			idx.ids = i;
			hw_db_inline_slc_lr_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->slc_lr[idx.ids].ref = 1;
	memcpy(&db->slc_lr[idx.ids].data, data, sizeof(struct hw_db_inline_slc_lr_data));

	hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_HEAD_SLC_EN, idx.ids, data->head_slice_en);
	hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_HEAD_DYN, idx.ids, data->head_slice_dyn);
	hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_HEAD_OFS, idx.ids, data->head_slice_ofs);
	hw_mod_slc_lr_rcp_flush(&ndev->be, idx.ids, 1);

	return idx;
}

static void hw_db_inline_slc_lr_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_slc_lr_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->slc_lr[idx.ids].ref -= 1;

	if (db->slc_lr[idx.ids].ref <= 0) {
		hw_mod_slc_lr_rcp_set(&ndev->be, HW_SLC_LR_RCP_PRESET_ALL, idx.ids, 0x0);
		hw_mod_slc_lr_rcp_flush(&ndev->be, idx.ids, 1);

		memset(&db->slc_lr[idx.ids].data, 0x0, sizeof(struct hw_db_inline_slc_lr_data));
		db->slc_lr[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* TPE                                                                        */
/******************************************************************************/

static int hw_db_inline_tpe_compare(const struct hw_db_inline_tpe_data *data1,
	const struct hw_db_inline_tpe_data *data2)
{
	for (int i = 0; i < 6; ++i)
		if (data1->writer[i].en != data2->writer[i].en ||
			data1->writer[i].reader_select != data2->writer[i].reader_select ||
			data1->writer[i].dyn != data2->writer[i].dyn ||
			data1->writer[i].ofs != data2->writer[i].ofs ||
			data1->writer[i].len != data2->writer[i].len)
			return 0;

	return data1->insert_len == data2->insert_len && data1->new_outer == data2->new_outer &&
		data1->calc_eth_type_from_inner_ip == data2->calc_eth_type_from_inner_ip &&
		data1->ttl_en == data2->ttl_en && data1->ttl_dyn == data2->ttl_dyn &&
		data1->ttl_ofs == data2->ttl_ofs && data1->len_a_en == data2->len_a_en &&
		data1->len_a_pos_dyn == data2->len_a_pos_dyn &&
		data1->len_a_pos_ofs == data2->len_a_pos_ofs &&
		data1->len_a_add_dyn == data2->len_a_add_dyn &&
		data1->len_a_add_ofs == data2->len_a_add_ofs &&
		data1->len_a_sub_dyn == data2->len_a_sub_dyn &&
		data1->len_b_en == data2->len_b_en &&
		data1->len_b_pos_dyn == data2->len_b_pos_dyn &&
		data1->len_b_pos_ofs == data2->len_b_pos_ofs &&
		data1->len_b_add_dyn == data2->len_b_add_dyn &&
		data1->len_b_add_ofs == data2->len_b_add_ofs &&
		data1->len_b_sub_dyn == data2->len_b_sub_dyn &&
		data1->len_c_en == data2->len_c_en &&
		data1->len_c_pos_dyn == data2->len_c_pos_dyn &&
		data1->len_c_pos_ofs == data2->len_c_pos_ofs &&
		data1->len_c_add_dyn == data2->len_c_add_dyn &&
		data1->len_c_add_ofs == data2->len_c_add_ofs &&
		data1->len_c_sub_dyn == data2->len_c_sub_dyn;
}

static void hw_db_inline_tpe_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_tpe_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->tpe[idx.ids].ref += 1;
}

struct hw_db_tpe_idx hw_db_inline_tpe_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_tpe_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_tpe_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_TPE;

	for (uint32_t i = 1; i < db->nb_tpe; ++i) {
		int ref = db->tpe[i].ref;

		if (ref > 0 && hw_db_inline_tpe_compare(data, &db->tpe[i].data)) {
			idx.ids = i;
			hw_db_inline_tpe_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->tpe[idx.ids].ref = 1;
	memcpy(&db->tpe[idx.ids].data, data, sizeof(struct hw_db_inline_tpe_data));

	if (data->insert_len > 0) {
		hw_mod_tpe_rpp_rcp_set(&ndev->be, HW_TPE_RPP_RCP_EXP, idx.ids, data->insert_len);
		hw_mod_tpe_rpp_rcp_flush(&ndev->be, idx.ids, 1);

		hw_mod_tpe_ins_rcp_set(&ndev->be, HW_TPE_INS_RCP_DYN, idx.ids, 1);
		hw_mod_tpe_ins_rcp_set(&ndev->be, HW_TPE_INS_RCP_OFS, idx.ids, 0);
		hw_mod_tpe_ins_rcp_set(&ndev->be, HW_TPE_INS_RCP_LEN, idx.ids, data->insert_len);
		hw_mod_tpe_ins_rcp_flush(&ndev->be, idx.ids, 1);

		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_RPL_RCP_DYN, idx.ids, 1);
		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_RPL_RCP_OFS, idx.ids, 0);
		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_RPL_RCP_LEN, idx.ids, data->insert_len);
		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_RPL_RCP_RPL_PTR, idx.ids, 0);
		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_RPL_RCP_EXT_PRIO, idx.ids, 1);
		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_RPL_RCP_ETH_TYPE_WR, idx.ids,
			data->calc_eth_type_from_inner_ip);
		hw_mod_tpe_rpl_rcp_flush(&ndev->be, idx.ids, 1);
	}

	for (uint32_t i = 0; i < 6; ++i) {
		if (data->writer[i].en) {
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_READER_SELECT,
				idx.ids + db->nb_tpe * i,
				data->writer[i].reader_select);
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_DYN,
				idx.ids + db->nb_tpe * i, data->writer[i].dyn);
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_OFS,
				idx.ids + db->nb_tpe * i, data->writer[i].ofs);
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_LEN,
				idx.ids + db->nb_tpe * i, data->writer[i].len);

		} else {
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_READER_SELECT,
				idx.ids + db->nb_tpe * i, 0);
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_DYN,
				idx.ids + db->nb_tpe * i, 0);
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_OFS,
				idx.ids + db->nb_tpe * i, 0);
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_CPY_RCP_LEN,
				idx.ids + db->nb_tpe * i, 0);
		}

		hw_mod_tpe_cpy_rcp_flush(&ndev->be, idx.ids + db->nb_tpe * i, 1);
	}

	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_WR, idx.ids, data->len_a_en);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_OUTER_L4_LEN, idx.ids,
		data->new_outer);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_POS_DYN, idx.ids,
		data->len_a_pos_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_POS_OFS, idx.ids,
		data->len_a_pos_ofs);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_ADD_DYN, idx.ids,
		data->len_a_add_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_ADD_OFS, idx.ids,
		data->len_a_add_ofs);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_A_SUB_DYN, idx.ids,
		data->len_a_sub_dyn);

	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_B_WR, idx.ids, data->len_b_en);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_B_POS_DYN, idx.ids,
		data->len_b_pos_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_B_POS_OFS, idx.ids,
		data->len_b_pos_ofs);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_B_ADD_DYN, idx.ids,
		data->len_b_add_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_B_ADD_OFS, idx.ids,
		data->len_b_add_ofs);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_B_SUB_DYN, idx.ids,
		data->len_b_sub_dyn);

	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_C_WR, idx.ids, data->len_c_en);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_C_POS_DYN, idx.ids,
		data->len_c_pos_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_C_POS_OFS, idx.ids,
		data->len_c_pos_ofs);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_C_ADD_DYN, idx.ids,
		data->len_c_add_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_C_ADD_OFS, idx.ids,
		data->len_c_add_ofs);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_LEN_C_SUB_DYN, idx.ids,
		data->len_c_sub_dyn);

	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_TTL_WR, idx.ids, data->ttl_en);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_TTL_POS_DYN, idx.ids, data->ttl_dyn);
	hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_HFU_RCP_TTL_POS_OFS, idx.ids, data->ttl_ofs);
	hw_mod_tpe_hfu_rcp_flush(&ndev->be, idx.ids, 1);

	hw_mod_tpe_csu_rcp_set(&ndev->be, HW_TPE_CSU_RCP_OUTER_L3_CMD, idx.ids, 3);
	hw_mod_tpe_csu_rcp_set(&ndev->be, HW_TPE_CSU_RCP_OUTER_L4_CMD, idx.ids, 3);
	hw_mod_tpe_csu_rcp_set(&ndev->be, HW_TPE_CSU_RCP_INNER_L3_CMD, idx.ids, 3);
	hw_mod_tpe_csu_rcp_set(&ndev->be, HW_TPE_CSU_RCP_INNER_L4_CMD, idx.ids, 3);
	hw_mod_tpe_csu_rcp_flush(&ndev->be, idx.ids, 1);

	return idx;
}

static void hw_db_inline_tpe_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_tpe_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->tpe[idx.ids].ref -= 1;

	if (db->tpe[idx.ids].ref <= 0) {
		for (uint32_t i = 0; i < 6; ++i) {
			hw_mod_tpe_cpy_rcp_set(&ndev->be, HW_TPE_PRESET_ALL,
				idx.ids + db->nb_tpe * i, 0);
			hw_mod_tpe_cpy_rcp_flush(&ndev->be, idx.ids + db->nb_tpe * i, 1);
		}

		hw_mod_tpe_rpp_rcp_set(&ndev->be, HW_TPE_PRESET_ALL, idx.ids, 0);
		hw_mod_tpe_rpp_rcp_flush(&ndev->be, idx.ids, 1);

		hw_mod_tpe_ins_rcp_set(&ndev->be, HW_TPE_PRESET_ALL, idx.ids, 0);
		hw_mod_tpe_ins_rcp_flush(&ndev->be, idx.ids, 1);

		hw_mod_tpe_rpl_rcp_set(&ndev->be, HW_TPE_PRESET_ALL, idx.ids, 0);
		hw_mod_tpe_rpl_rcp_flush(&ndev->be, idx.ids, 1);

		hw_mod_tpe_hfu_rcp_set(&ndev->be, HW_TPE_PRESET_ALL, idx.ids, 0);
		hw_mod_tpe_hfu_rcp_flush(&ndev->be, idx.ids, 1);

		hw_mod_tpe_csu_rcp_set(&ndev->be, HW_TPE_PRESET_ALL, idx.ids, 0);
		hw_mod_tpe_csu_rcp_flush(&ndev->be, idx.ids, 1);

		memset(&db->tpe[idx.ids].data, 0x0, sizeof(struct hw_db_inline_tpe_data));
		db->tpe[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* TPE_EXT                                                                    */
/******************************************************************************/

static int hw_db_inline_tpe_ext_compare(const struct hw_db_inline_tpe_ext_data *data1,
	const struct hw_db_inline_tpe_ext_data *data2)
{
	return data1->size == data2->size &&
		memcmp(data1->hdr8, data2->hdr8, HW_DB_INLINE_MAX_ENCAP_SIZE) == 0;
}

static void hw_db_inline_tpe_ext_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_tpe_ext_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->tpe_ext[idx.ids].ref += 1;
}

struct hw_db_tpe_ext_idx hw_db_inline_tpe_ext_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_tpe_ext_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_tpe_ext_idx idx = { .raw = 0 };
	int rpl_rpl_length = ((int)data->size + 15) / 16;
	int found = 0, rpl_rpl_index = 0;

	idx.type = HW_DB_IDX_TYPE_TPE_EXT;

	if (data->size > HW_DB_INLINE_MAX_ENCAP_SIZE) {
		idx.error = 1;
		return idx;
	}

	for (uint32_t i = 1; i < db->nb_tpe_ext; ++i) {
		int ref = db->tpe_ext[i].ref;

		if (ref > 0 && hw_db_inline_tpe_ext_compare(data, &db->tpe_ext[i].data)) {
			idx.ids = i;
			hw_db_inline_tpe_ext_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	rpl_rpl_index = nthw_flow_nic_alloc_resource_config(ndev, RES_TPE_RPL, rpl_rpl_length, 1);

	if (rpl_rpl_index < 0) {
		idx.error = 1;
		return idx;
	}

	db->tpe_ext[idx.ids].ref = 1;
	db->tpe_ext[idx.ids].replace_ram_idx = rpl_rpl_index;
	memcpy(&db->tpe_ext[idx.ids].data, data, sizeof(struct hw_db_inline_tpe_ext_data));

	hw_mod_tpe_rpl_ext_set(&ndev->be, HW_TPE_RPL_EXT_RPL_PTR, idx.ids, rpl_rpl_index);
	hw_mod_tpe_rpl_ext_set(&ndev->be, HW_TPE_RPL_EXT_META_RPL_LEN, idx.ids, data->size);
	hw_mod_tpe_rpl_ext_flush(&ndev->be, idx.ids, 1);

	for (int i = 0; i < rpl_rpl_length; ++i) {
		uint32_t rpl_data[4];
		memcpy(rpl_data, data->hdr32 + i * 4, sizeof(rpl_data));
		hw_mod_tpe_rpl_rpl_set(&ndev->be, HW_TPE_RPL_RPL_VALUE, rpl_rpl_index + i,
			rpl_data);
	}

	hw_mod_tpe_rpl_rpl_flush(&ndev->be, rpl_rpl_index, rpl_rpl_length);

	return idx;
}

static void hw_db_inline_tpe_ext_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_tpe_ext_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->tpe_ext[idx.ids].ref -= 1;

	if (db->tpe_ext[idx.ids].ref <= 0) {
		const int rpl_rpl_length = ((int)db->tpe_ext[idx.ids].data.size + 15) / 16;
		const int rpl_rpl_index = db->tpe_ext[idx.ids].replace_ram_idx;

		hw_mod_tpe_rpl_ext_set(&ndev->be, HW_TPE_PRESET_ALL, idx.ids, 0);
		hw_mod_tpe_rpl_ext_flush(&ndev->be, idx.ids, 1);

		for (int i = 0; i < rpl_rpl_length; ++i) {
			uint32_t rpl_zero[] = { 0, 0, 0, 0 };
			hw_mod_tpe_rpl_rpl_set(&ndev->be, HW_TPE_RPL_RPL_VALUE, rpl_rpl_index + i,
				rpl_zero);
			nthw_flow_nic_free_resource(ndev, RES_TPE_RPL, rpl_rpl_index + i);
		}

		hw_mod_tpe_rpl_rpl_flush(&ndev->be, rpl_rpl_index, rpl_rpl_length);

		memset(&db->tpe_ext[idx.ids].data, 0x0, sizeof(struct hw_db_inline_tpe_ext_data));
		db->tpe_ext[idx.ids].ref = 0;
	}
}


/******************************************************************************/
/* CAT                                                                        */
/******************************************************************************/

static void hw_db_inline_cat_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_cat_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->cat[idx.ids].ref += 1;
}

static int hw_db_inline_cat_compare(const struct hw_db_inline_cat_data *data1,
	const struct hw_db_inline_cat_data *data2)
{
	return data1->vlan_mask == data2->vlan_mask &&
		data1->mac_port_mask == data2->mac_port_mask &&
		data1->ptc_mask_frag == data2->ptc_mask_frag &&
		data1->ptc_mask_l2 == data2->ptc_mask_l2 &&
		data1->ptc_mask_l3 == data2->ptc_mask_l3 &&
		data1->ptc_mask_l4 == data2->ptc_mask_l4 &&
		data1->ptc_mask_tunnel == data2->ptc_mask_tunnel &&
		data1->ptc_mask_l3_tunnel == data2->ptc_mask_l3_tunnel &&
		data1->ptc_mask_l4_tunnel == data2->ptc_mask_l4_tunnel &&
		data1->err_mask_ttl_tunnel == data2->err_mask_ttl_tunnel &&
		data1->err_mask_ttl == data2->err_mask_ttl && data1->ip_prot == data2->ip_prot &&
		data1->ip_prot_tunnel == data2->ip_prot_tunnel;
}

struct hw_db_cat_idx hw_db_inline_cat_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cat_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_cat_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_CAT;

	for (uint32_t i = 0; i < db->nb_cat; ++i) {
		int ref = db->cat[i].ref;

		if (ref > 0 && hw_db_inline_cat_compare(data, &db->cat[i].data)) {
			idx.ids = i;
			hw_db_inline_cat_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->cat[idx.ids].ref = 1;
	memcpy(&db->cat[idx.ids].data, data, sizeof(struct hw_db_inline_cat_data));

	return idx;
}

static void hw_db_inline_cat_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_cat_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->cat[idx.ids].ref -= 1;

	if (db->cat[idx.ids].ref <= 0) {
		memset(&db->cat[idx.ids].data, 0x0, sizeof(struct hw_db_inline_cat_data));
		db->cat[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* KM RCP                                                                     */
/******************************************************************************/

static int hw_db_inline_km_compare(const struct hw_db_inline_km_rcp_data *data1,
	const struct hw_db_inline_km_rcp_data *data2)
{
	return data1->rcp == data2->rcp;
}

static void hw_db_inline_km_ref(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_km_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->km[idx.id1].ref += 1;
}

struct hw_db_km_idx hw_db_inline_km_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_km_rcp_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_km_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_KM_RCP;

	for (uint32_t i = 0; i < db->nb_km_rcp; ++i) {
		if (!found && db->km[i].ref <= 0) {
			found = 1;
			idx.id1 = i;
		}

		if (db->km[i].ref > 0 && hw_db_inline_km_compare(data, &db->km[i].data)) {
			idx.id1 = i;
			hw_db_inline_km_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&db->km[idx.id1].data, data, sizeof(struct hw_db_inline_km_rcp_data));
	db->km[idx.id1].ref = 1;

	return idx;
}

static void hw_db_inline_km_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_km_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->flm[idx.id1].ref -= 1;

	if (db->flm[idx.id1].ref <= 0) {
		memset(&db->flm[idx.id1].data, 0x0, sizeof(struct hw_db_inline_km_rcp_data));
		db->flm[idx.id1].ref = 0;
	}
}

/******************************************************************************/
/* KM FT                                                                      */
/******************************************************************************/

static int hw_db_inline_km_ft_compare(const struct hw_db_inline_km_ft_data *data1,
	const struct hw_db_inline_km_ft_data *data2)
{
	return data1->cat.raw == data2->cat.raw && data1->km.raw == data2->km.raw &&
		data1->action_set.raw == data2->action_set.raw;
}

static void hw_db_inline_km_ft_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_km_ft idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error) {
		uint32_t cat_offset = idx.id3 * db->nb_cat;
		db->km[idx.id2].ft[cat_offset + idx.id1].ref += 1;
	}
}

struct hw_db_km_ft hw_db_inline_km_ft_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_km_ft_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_km_rcp *km_rcp = &db->km[data->km.id1];
	struct hw_db_km_ft idx = { .raw = 0 };
	uint32_t cat_offset = data->cat.ids * db->nb_cat;
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_KM_FT;
	idx.id2 = data->km.id1;
	idx.id3 = data->cat.ids;

	if (km_rcp->data.rcp == 0) {
		idx.id1 = 0;
		return idx;
	}

	for (uint32_t i = 1; i < db->nb_km_ft; ++i) {
		const struct hw_db_inline_resource_db_km_ft *km_ft = &km_rcp->ft[cat_offset + i];

		if (!found && km_ft->ref <= 0) {
			found = 1;
			idx.id1 = i;
		}

		if (km_ft->ref > 0 && hw_db_inline_km_ft_compare(data, &km_ft->data)) {
			idx.id1 = i;
			hw_db_inline_km_ft_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&km_rcp->ft[cat_offset + idx.id1].data, data,
		sizeof(struct hw_db_inline_km_ft_data));
	km_rcp->ft[cat_offset + idx.id1].ref = 1;

	return idx;
}

static void hw_db_inline_km_ft_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_km_ft idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_km_rcp *km_rcp = &db->km[idx.id2];
	uint32_t cat_offset = idx.id3 * db->nb_cat;

	if (idx.error)
		return;

	km_rcp->ft[cat_offset + idx.id1].ref -= 1;

	if (km_rcp->ft[cat_offset + idx.id1].ref <= 0) {
		memset(&km_rcp->ft[cat_offset + idx.id1].data, 0x0,
			sizeof(struct hw_db_inline_km_ft_data));
		km_rcp->ft[cat_offset + idx.id1].ref = 0;
	}
}

/******************************************************************************/
/* FLM RCP                                                                    */
/******************************************************************************/

static int hw_db_inline_flm_compare(const struct hw_db_inline_flm_rcp_data *data1,
	const struct hw_db_inline_flm_rcp_data *data2)
{
	if (data1->qw0_dyn != data2->qw0_dyn || data1->qw0_ofs != data2->qw0_ofs ||
		data1->qw4_dyn != data2->qw4_dyn || data1->qw4_ofs != data2->qw4_ofs ||
		data1->sw8_dyn != data2->sw8_dyn || data1->sw8_ofs != data2->sw8_ofs ||
		data1->sw9_dyn != data2->sw9_dyn || data1->sw9_ofs != data2->sw9_ofs ||
		data1->outer_prot != data2->outer_prot || data1->inner_prot != data2->inner_prot) {
		return 0;
	}

	for (int i = 0; i < 10; ++i)
		if (data1->mask[i] != data2->mask[i])
			return 0;

	return 1;
}

static void hw_db_inline_flm_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_flm_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->flm[idx.id1].ref += 1;
}

struct hw_db_flm_idx hw_db_inline_flm_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_flm_rcp_data *data, int group)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_flm_idx idx = { .raw = 0 };

	idx.type = HW_DB_IDX_TYPE_FLM_RCP;
	idx.id1 = group;

	if (group == 0)
		return idx;

	if (db->flm[idx.id1].ref > 0) {
		if (!hw_db_inline_flm_compare(data, &db->flm[idx.id1].data)) {
			idx.error = 1;
			return idx;
		}

		hw_db_inline_flm_ref(ndev, db, idx);
		return idx;
	}

	db->flm[idx.id1].ref = 1;
	memcpy(&db->flm[idx.id1].data, data, sizeof(struct hw_db_inline_flm_rcp_data));

	{
		uint32_t flm_mask[10] = {
			data->mask[0],	/* SW9 */
			data->mask[1],	/* SW8 */
			data->mask[5], data->mask[4], data->mask[3], data->mask[2],	/* QW4 */
			data->mask[9], data->mask[8], data->mask[7], data->mask[6],	/* QW0 */
		};

		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_PRESET_ALL, idx.id1, 0x0);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_LOOKUP, idx.id1, 1);

		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_QW0_DYN, idx.id1, data->qw0_dyn);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_QW0_OFS, idx.id1, data->qw0_ofs);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_QW0_SEL, idx.id1, 0);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_QW4_DYN, idx.id1, data->qw4_dyn);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_QW4_OFS, idx.id1, data->qw4_ofs);

		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_SW8_DYN, idx.id1, data->sw8_dyn);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_SW8_OFS, idx.id1, data->sw8_ofs);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_SW8_SEL, idx.id1, 0);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_SW9_DYN, idx.id1, data->sw9_dyn);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_SW9_OFS, idx.id1, data->sw9_ofs);

		hw_mod_flm_rcp_set_mask(&ndev->be, HW_FLM_RCP_MASK, idx.id1, flm_mask);

		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_KID, idx.id1, idx.id1 + 2);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_OPN, idx.id1, data->outer_prot ? 1 : 0);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_IPN, idx.id1, data->inner_prot ? 1 : 0);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_BYT_DYN, idx.id1, 0);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_BYT_OFS, idx.id1, -20);
		hw_mod_flm_rcp_set(&ndev->be, HW_FLM_RCP_TXPLM, idx.id1, UINT32_MAX);

		hw_mod_flm_rcp_flush(&ndev->be, idx.id1, 1);
	}

	return idx;
}

static void hw_db_inline_flm_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_flm_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	if (idx.id1 > 0) {
		db->flm[idx.id1].ref -= 1;

		if (db->flm[idx.id1].ref <= 0) {
			memset(&db->flm[idx.id1].data, 0x0,
				sizeof(struct hw_db_inline_flm_rcp_data));
			db->flm[idx.id1].ref = 0;

			hw_db_inline_setup_default_flm_rcp(ndev, idx.id1);
		}
	}
}

/******************************************************************************/
/* FLM FT                                                                     */
/******************************************************************************/

static int hw_db_inline_flm_ft_compare(const struct hw_db_inline_flm_ft_data *data1,
	const struct hw_db_inline_flm_ft_data *data2)
{
	return data1->is_group_zero == data2->is_group_zero && data1->jump == data2->jump &&
		data1->action_set.raw == data2->action_set.raw;
}

static void hw_db_inline_flm_ft_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_flm_ft idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error && idx.id3 == 0)
		db->flm[idx.id2].ft[idx.id1].ref += 1;
}

struct hw_db_flm_ft hw_db_inline_flm_ft_default(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_flm_ft_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_flm_rcp *flm_rcp = &db->flm[data->jump];
	struct hw_db_flm_ft idx = { .raw = 0 };

	idx.type = HW_DB_IDX_TYPE_FLM_FT;
	idx.id1 = 0;
	idx.id2 = data->group & 0xff;

	if (data->is_group_zero) {
		idx.error = 1;
		return idx;
	}

	if (flm_rcp->ft[idx.id1].ref > 0) {
		if (!hw_db_inline_flm_ft_compare(data, &flm_rcp->ft[idx.id1].data)) {
			idx.error = 1;
			return idx;
		}

		hw_db_inline_flm_ft_ref(ndev, db, idx);
		return idx;
	}

	memcpy(&flm_rcp->ft[idx.id1].data, data, sizeof(struct hw_db_inline_flm_ft_data));
	flm_rcp->ft[idx.id1].idx.raw = idx.raw;
	flm_rcp->ft[idx.id1].ref = 1;

	hw_db_inline_update_active_filters(ndev, db, data->jump);

	return idx;
}

struct hw_db_flm_ft hw_db_inline_flm_ft_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_flm_ft_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_flm_rcp *flm_rcp = &db->flm[data->group];
	struct hw_db_flm_ft idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_FLM_FT;
	idx.id1 = 0;
	idx.id2 = data->group & 0xff;

	/* RCP 0 always uses FT 1; i.e. use unhandled FT for disabled RCP */
	if (data->group == 0) {
		idx.id1 = 1;
		return idx;
	}

	if (data->is_group_zero) {
		idx.id3 = 1;
		return idx;
	}

	/* FLM_FT records 0, 1 and last (15) are reserved */
	/* NOTE: RES_FLM_FLOW_TYPE resource is global and it cannot be used in _add() and _deref()
	 * to track usage of FLM_FT recipes which are group specific.
	 */
	for (uint32_t i = 2; i < db->nb_flm_ft; ++i) {
		if (!found && flm_rcp->ft[i].ref <= 0 &&
			!flow_nic_is_resource_used(ndev, RES_FLM_FLOW_TYPE, i)) {
			found = 1;
			idx.id1 = i;
		}

		if (flm_rcp->ft[i].ref > 0 &&
			hw_db_inline_flm_ft_compare(data, &flm_rcp->ft[i].data)) {
			idx.id1 = i;
			hw_db_inline_flm_ft_ref(ndev, db, idx);
			return idx;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	memcpy(&flm_rcp->ft[idx.id1].data, data, sizeof(struct hw_db_inline_flm_ft_data));
	flm_rcp->ft[idx.id1].idx.raw = idx.raw;
	flm_rcp->ft[idx.id1].ref = 1;

	hw_db_inline_update_active_filters(ndev, db, data->group);

	return idx;
}

static void hw_db_inline_flm_ft_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_flm_ft idx)
{
	(void)ndev;
	(void)db_handle;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_inline_resource_db_flm_rcp *flm_rcp;

	if (idx.error || idx.id2 == 0 || idx.id3 > 0)
		return;

	flm_rcp = &db->flm[idx.id2];

	flm_rcp->ft[idx.id1].ref -= 1;

	if (flm_rcp->ft[idx.id1].ref > 0)
		return;

	flm_rcp->ft[idx.id1].ref = 0;
	hw_db_inline_update_active_filters(ndev, db, idx.id2);
	memset(&flm_rcp->ft[idx.id1], 0x0, sizeof(struct hw_db_inline_resource_db_flm_ft));
}

/******************************************************************************/
/* HSH                                                                        */
/******************************************************************************/

static int hw_db_inline_hsh_compare(const struct hw_db_inline_hsh_data *data1,
	const struct hw_db_inline_hsh_data *data2)
{
	for (uint32_t i = 0; i < MAX_RSS_KEY_LEN; ++i)
		if (data1->key[i] != data2->key[i])
			return 0;

	return data1->func == data2->func && data1->hash_mask == data2->hash_mask;
}

static void hw_db_inline_hsh_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_hsh_idx idx)
{
	(void)ndev;
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->hsh[idx.ids].ref += 1;
}

struct hw_db_hsh_idx hw_db_inline_hsh_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_hsh_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_hsh_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_HSH;

	/* check if default hash configuration shall be used, i.e. rss_hf is not set */
	/*
	 * NOTE: hsh id 0 is reserved for "default"
	 * HSH used by port configuration; All ports share the same default hash settings.
	 */
	if (data->hash_mask == 0) {
		idx.ids = 0;
		hw_db_inline_hsh_ref(ndev, db, idx);
		return idx;
	}

	for (uint32_t i = 1; i < db->nb_hsh; ++i) {
		int ref = db->hsh[i].ref;

		if (ref > 0 && hw_db_inline_hsh_compare(data, &db->hsh[i].data)) {
			idx.ids = i;
			hw_db_inline_hsh_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	struct nt_eth_rss_conf tmp_rss_conf;

	tmp_rss_conf.rss_hf = data->hash_mask;
	memcpy(tmp_rss_conf.rss_key, data->key, MAX_RSS_KEY_LEN);
	tmp_rss_conf.algorithm = data->func;
	int res = hsh_set(ndev, idx.ids, tmp_rss_conf);

	if (res != 0) {
		idx.error = 1;
		return idx;
	}

	db->hsh[idx.ids].ref = 1;
	memcpy(&db->hsh[idx.ids].data, data, sizeof(struct hw_db_inline_hsh_data));
	flow_nic_mark_resource_used(ndev, RES_HSH_RCP, idx.ids);

	hw_mod_hsh_rcp_flush(&ndev->be, idx.ids, 1);

	return idx;
}

static void hw_db_inline_hsh_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_hsh_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->hsh[idx.ids].ref -= 1;

	if (db->hsh[idx.ids].ref <= 0) {
		/*
		 * NOTE: hsh id 0 is reserved for "default" HSH used by
		 * port configuration, so we shall keep it even if
		 * it is not used by any flow
		 */
		if (idx.ids > 0) {
			int res = hw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_PRESET_ALL,
				idx.ids, 0, 0x0);
			NT_LOG(ERR, FILTER, "Failed to preset HSH RCP %u: %d", idx.ids, res);
			hw_mod_hsh_rcp_flush(&ndev->be, idx.ids, 1);

			memset(&db->hsh[idx.ids].data, 0x0, sizeof(struct hw_db_inline_hsh_data));
			nthw_flow_nic_free_resource(ndev, RES_HSH_RCP, idx.ids);
		}

		db->hsh[idx.ids].ref = 0;
	}
}

/******************************************************************************/
/* FML SCRUB                                                                  */
/******************************************************************************/

static int hw_db_inline_scrub_compare(const struct hw_db_inline_scrub_data *data1,
	const struct hw_db_inline_scrub_data *data2)
{
	return data1->timeout == data2->timeout;
}

static void hw_db_inline_scrub_ref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_flm_scrub_idx idx)
{
	(void)ndev;

	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->scrub[idx.ids].ref += 1;
}

struct hw_db_flm_scrub_idx hw_db_inline_scrub_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_scrub_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_flm_scrub_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_FLM_SCRUB;

	/* NOTE: scrub id 0 is reserved for "default" timeout 0, i.e. flow will never AGE-out */
	if (data->timeout == 0) {
		idx.ids = 0;
		hw_db_inline_scrub_ref(ndev, db, idx);
		return idx;
	}

	for (uint32_t i = 1; i < db->nb_scrub; ++i) {
		int ref = db->scrub[i].ref;

		if (ref > 0 && hw_db_inline_scrub_compare(data, &db->scrub[i].data)) {
			idx.ids = i;
			hw_db_inline_scrub_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	int res = hw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_T, idx.ids, data->timeout);
	res |= hw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_R, idx.ids,
		NTNIC_SCANNER_TIMEOUT_RESOLUTION);
	res |= hw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_DEL, idx.ids, SCRUB_DEL);
	res |= hw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_INF, idx.ids, SCRUB_INF);

	if (res != 0) {
		idx.error = 1;
		return idx;
	}

	db->scrub[idx.ids].ref = 1;
	memcpy(&db->scrub[idx.ids].data, data, sizeof(struct hw_db_inline_scrub_data));
	flow_nic_mark_resource_used(ndev, RES_SCRUB_RCP, idx.ids);

	hw_mod_flm_scrub_flush(&ndev->be, idx.ids, 1);

	return idx;
}

static void hw_db_inline_scrub_deref(struct flow_nic_dev *ndev, void *db_handle,
	struct hw_db_flm_scrub_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->scrub[idx.ids].ref -= 1;

	if (db->scrub[idx.ids].ref <= 0) {
		/* NOTE: scrub id 0 is reserved for "default" timeout 0, which shall not be removed
		 */
		if (idx.ids > 0) {
			hw_mod_flm_scrub_set(&ndev->be, HW_FLM_SCRUB_T, idx.ids, 0);
			hw_mod_flm_scrub_flush(&ndev->be, idx.ids, 1);

			memset(&db->scrub[idx.ids].data, 0x0,
				sizeof(struct hw_db_inline_scrub_data));
			nthw_flow_nic_free_resource(ndev, RES_SCRUB_RCP, idx.ids);
		}

		db->scrub[idx.ids].ref = 0;
	}
}

void hw_db_inline_deref_idxs(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_idx *idxs,
	uint32_t size)
{
	for (uint32_t i = 0; i < size; ++i) {
		switch (idxs[i].type) {
		case HW_DB_IDX_TYPE_NONE:
			break;

		case HW_DB_IDX_TYPE_MATCH_SET:
			hw_db_inline_match_set_deref(ndev, db_handle,
				*(struct hw_db_match_set_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_ACTION_SET:
			hw_db_inline_action_set_deref(ndev, db_handle,
				*(struct hw_db_action_set_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_CAT:
			hw_db_inline_cat_deref(ndev, db_handle, *(struct hw_db_cat_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_COT:
			hw_db_inline_cot_deref(ndev, db_handle, *(struct hw_db_cot_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_QSL:
			hw_db_inline_qsl_deref(ndev, db_handle, *(struct hw_db_qsl_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_SLC_LR:
			hw_db_inline_slc_lr_deref(ndev, db_handle,
				*(struct hw_db_slc_lr_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_TPE:
			hw_db_inline_tpe_deref(ndev, db_handle, *(struct hw_db_tpe_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_TPE_EXT:
			hw_db_inline_tpe_ext_deref(ndev, db_handle,
				*(struct hw_db_tpe_ext_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_FLM_RCP:
			hw_db_inline_flm_deref(ndev, db_handle, *(struct hw_db_flm_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_FLM_FT:
			hw_db_inline_flm_ft_deref(ndev, db_handle,
				*(struct hw_db_flm_ft *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_KM_RCP:
			hw_db_inline_km_deref(ndev, db_handle, *(struct hw_db_km_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_KM_FT:
			hw_db_inline_km_ft_deref(ndev, db_handle, *(struct hw_db_km_ft *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_HSH:
			hw_db_inline_hsh_deref(ndev, db_handle, *(struct hw_db_hsh_idx *)&idxs[i]);
			break;

		case HW_DB_IDX_TYPE_FLM_SCRUB:
			hw_db_inline_scrub_deref(ndev, db_handle,
				*(struct hw_db_flm_scrub_idx *)&idxs[i]);
			break;

		default:
			break;
		}
	}
}
