/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "KM"
#define _VER_ be->km.ver

#define KM_TCQ_ENTRIES 2048
#define KM_RCP_MASK_A_SIZE 11
#define KM_RCP_MASK_D_A_SIZE 12	/* Mask for double size word extractors for DW8/DW10 */
#define KM_RCP_MASK_B_SIZE 6

bool hw_mod_km_present(struct flow_api_backend_s *be)
{
	return be->iface->get_km_present(be->be_dev);
}

int hw_mod_km_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_km_version(be->be_dev);
	NT_LOG(DBG, FILTER, "KM  MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	nb = be->iface->get_nb_km_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(km_categories);
		return COUNT_ERROR;
	}
	be->km.nb_categories = (uint32_t)nb;

	nb = be->iface->get_nb_km_cam_banks(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cam_banks);
		return COUNT_ERROR;
	}

	be->km.nb_cam_banks = (uint32_t)nb;

	nb = be->iface->get_nb_km_cam_records(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cam_records);
		return COUNT_ERROR;
	}
	be->km.nb_cam_records = (uint32_t)nb;

	nb = be->iface->get_nb_km_cam_record_words(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cam_record_words);
		return COUNT_ERROR;
	}

	be->km.nb_cam_record_words = (uint32_t)nb;

	nb = be->iface->get_nb_km_tcam_banks(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tcam_banks);
		return COUNT_ERROR;
	}

	be->km.nb_tcam_banks = (uint32_t)nb;

	nb = be->iface->get_nb_km_tcam_bank_width(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tcam_bank_width);
		return COUNT_ERROR;
	}
	be->km.nb_tcam_bank_width = (uint32_t)nb;

	switch (_VER_) {
	case 7:
		be->km.nb_km_rcp_mask_a_word_size = 12;
		be->km.nb_km_rcp_mask_b_word_size = 6;

		if (!callocate_mod((struct common_func_s *)&be->km, 5, &be->km.v7.rcp,
				be->km.nb_categories, sizeof(struct km_v7_rcp_s),
				&be->km.v7.cam, be->km.nb_cam_banks * be->km.nb_cam_records,
				sizeof(struct km_v7_cam_s), &be->km.v7.tcam,
				be->km.nb_tcam_banks * 4 * 256, sizeof(struct km_v7_tcam_s),
				&be->km.v7.tci,
				be->km.nb_tcam_banks * be->km.nb_tcam_bank_width,
				sizeof(struct km_v7_tci_s), &be->km.v7.tcq, KM_TCQ_ENTRIES,
				sizeof(struct km_v7_tcq_s)))
			return -1;

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void hw_mod_km_free(struct flow_api_backend_s *be)
{
	if (be->km.base) {
		free(be->km.base);
		be->km.base = NULL;
	}
}

int hw_mod_km_reset(struct flow_api_backend_s *be)
{
	uint32_t tcam_v_set[3] = { 0x00000000, 0x00000000, 0x00000000 };

	/* Zero entire cache area */
	zero_module_cache((struct common_func_s *)(&be->km));

	NT_LOG(DBG, FILTER, "INIT KM RCP");
	hw_mod_km_rcp_flush(be, 0, ALL_ENTRIES);

	/* init CAM - all zero */
	NT_LOG(DBG, FILTER, "INIT KM CAM");
	hw_mod_km_cam_flush(be, 0, 0, ALL_ENTRIES);

	/* init TCAM - all zero */
	NT_LOG(DBG, FILTER, "INIT KM TCAM");

	for (unsigned int i = 0; i < be->km.nb_tcam_banks; i++) {
		/* TCAM entries are cache controlled,
		 * thus need to hard reset initially to sync cache with HW
		 */
		hw_mod_km_tcam_set(be, HW_KM_TCAM_BANK_RESET, i, 0, 0, tcam_v_set);
	}

	hw_mod_km_tcam_flush(be, 0, ALL_ENTRIES);

	/* init TCI - all zero */
	NT_LOG(DBG, FILTER, "INIT KM TCI");
	hw_mod_km_tci_flush(be, 0, 0, ALL_ENTRIES);

	NT_LOG(DBG, FILTER, "INIT KM TCQ");

	for (unsigned int i = 0; i < be->km.nb_tcam_bank_width; i++)
		hw_mod_km_tcq_flush(be, 0, i, be->km.nb_tcam_banks);

	return 0;
}

int hw_mod_km_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->km.nb_categories;

	if ((unsigned int)(start_idx + count) > be->km.nb_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->km_rcp_flush(be->be_dev, &be->km, start_idx, count);
}

int hw_mod_km_cam_flush(struct flow_api_backend_s *be, int start_bank, int start_record, int count)
{
	if (count == ALL_ENTRIES)
		count = be->km.nb_cam_records * be->km.nb_cam_banks;

	unsigned int end = start_bank * be->km.nb_cam_records + start_record + count;

	if (end > (be->km.nb_cam_banks * be->km.nb_cam_records)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->km_cam_flush(be->be_dev, &be->km, start_bank, start_record, count);
}

int hw_mod_km_tcam_flush(struct flow_api_backend_s *be, int start_bank, int count)
{
	if (count == ALL_ENTRIES)
		count = be->km.nb_tcam_banks * 4 * 256;

	else if (count == ALL_BANK_ENTRIES)
		count = 4 * 256;

	unsigned int end = start_bank * 4 * 256 + count;

	if (end > (be->km.nb_tcam_banks * 4 * 256)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->km_tcam_flush(be->be_dev, &be->km, start_bank, 0, 0, count);
}

static int hw_mod_km_tcam_mod(struct flow_api_backend_s *be, enum hw_km_e field, int bank,
	int byte, int byte_val, uint32_t *value_set, int get)
{
	unsigned int start_index = bank * 4 * 256 + (int)byte * 256 + byte_val;

	if (start_index >= (be->km.nb_tcam_banks * 4 * 256)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 7:
		switch (field) {
		case HW_KM_TCAM_BANK_RESET:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			{
				int start_idx = bank * 4 * 256;

				for (int i = 0; i < 4 * 256; i++) {
					be->km.v7.tcam[start_idx + i].t[0] = value_set[0];
					be->km.v7.tcam[start_idx + i].t[1] = value_set[1];
					be->km.v7.tcam[start_idx + i].t[2] = value_set[2];
					be->km.v7.tcam[start_idx + i].dirty = 1;
				}
			}
			break;

		case HW_KM_TCAM_T: {
			int index = bank * 4 * 256 + byte * 256 + byte_val;

			if (get) {
				value_set[0] = be->km.v7.tcam[index].t[0];
				value_set[1] = be->km.v7.tcam[index].t[1];
				value_set[2] = be->km.v7.tcam[index].t[2];

			} else {
				/* only change if any bits has to be changed */
				if (be->km.v7.tcam[index].t[0] != value_set[0] ||
					be->km.v7.tcam[index].t[1] != value_set[1] ||
					be->km.v7.tcam[index].t[2] != value_set[2]) {
					be->km.v7.tcam[index].t[0] = value_set[0];
					be->km.v7.tcam[index].t[1] = value_set[1];
					be->km.v7.tcam[index].t[2] = value_set[2];
					be->km.v7.tcam[index].dirty = 1;
				}
			}
		}
		break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int hw_mod_km_tcam_set(struct flow_api_backend_s *be, enum hw_km_e field, int bank, int byte,
	int byte_val, uint32_t *value_set)
{
	return hw_mod_km_tcam_mod(be, field, bank, byte, byte_val, value_set, 0);
}

int hw_mod_km_tci_flush(struct flow_api_backend_s *be, int start_bank, int start_record, int count)
{
	if (count == ALL_ENTRIES)
		count = be->km.nb_tcam_banks * be->km.nb_tcam_bank_width;

	unsigned int end = (int)start_bank * be->km.nb_tcam_bank_width + start_record + count;

	if (end > (be->km.nb_tcam_banks * be->km.nb_tcam_bank_width)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->km_tci_flush(be->be_dev, &be->km, start_bank, start_record, count);
}

int hw_mod_km_tcq_flush(struct flow_api_backend_s *be, int start_bank, int start_record, int count)
{
	if (count == ALL_ENTRIES)
		count = be->km.nb_tcam_banks * be->km.nb_tcam_bank_width;

	unsigned int end = (int)start_bank * be->km.nb_tcam_bank_width + start_record + count;

	if (end > (be->km.nb_tcam_banks * be->km.nb_tcam_bank_width)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->km_tcq_flush(be->be_dev, &be->km, start_bank, start_record, count);
}
