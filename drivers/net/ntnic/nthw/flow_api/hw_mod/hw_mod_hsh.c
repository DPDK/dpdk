/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "HSH"
#define _VER_ be->hsh.ver

bool nthw_mod_hsh_present(struct flow_api_backend_s *be)
{
	return be->iface->get_hsh_present(be->be_dev);
}

int nthw_mod_hsh_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_hsh_version(be->be_dev);
	NT_LOG(DBG, FILTER, "HSH MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	/* detect number of HSH categories supported by FPGA */
	nb = be->iface->get_nb_hsh_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(hsh_categories);
		return COUNT_ERROR;
	}

	be->hsh.nb_rcp = (uint32_t)nb;

	/* detect if Toeplitz hashing function is supported by FPGA */
	nb = be->iface->get_nb_hsh_toeplitz(be->be_dev);

	if (nb < 0) {
		COUNT_ERROR_LOG(hsh_toeplitz);
		return COUNT_ERROR;
	}

	be->hsh.toeplitz = (uint32_t)nb;

	switch (_VER_) {
	case 5:
		if (!nthw_callocate_mod((struct common_func_s *)&be->hsh, 1, &be->hsh.v5.rcp,
				be->hsh.nb_rcp, sizeof(struct hsh_v5_rcp_s)))
			return -1;

		break;

	/* end case 5 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void nthw_mod_hsh_free(struct flow_api_backend_s *be)
{
	if (be->hsh.base) {
		free(be->hsh.base);
		be->hsh.base = NULL;
	}
}

int nthw_mod_hsh_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->hsh));

	NT_LOG(DBG, FILTER, "INIT HSH RCP");
	return nthw_mod_hsh_rcp_flush(be, 0, be->hsh.nb_rcp);
}

int nthw_mod_hsh_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->hsh.nb_rcp;

	if ((start_idx + count) > (int)be->hsh.nb_rcp)  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->hsh_rcp_flush(be->be_dev, &be->hsh, start_idx, count);
}

static int hw_mod_hsh_rcp_mod(struct flow_api_backend_s *be, enum hw_hsh_e field, uint32_t index,
	uint32_t word_off, uint32_t *value, int get)
{
	if (index >= be->hsh.nb_rcp) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 5:
		switch (field) {
		case HW_HSH_RCP_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->hsh.v5.rcp[index], (uint8_t)*value,
				sizeof(struct hsh_v5_rcp_s));
			break;

		case HW_HSH_RCP_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if ((unsigned int)word_off >= be->hsh.nb_rcp) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}
			/* Size of the structure */
			size_t element_size = sizeof(struct hsh_v5_rcp_s);
			/* Size of the buffer */
			size_t buffer_size = sizeof(be->hsh.v5.rcp);

			/* Calculate the maximum valid index (number of elements in the buffer) */
			size_t max_idx = buffer_size / element_size;

			/* Check that both indices are within bounds before calling the macro */
			if (index < max_idx && word_off < max_idx) {
				DO_COMPARE_INDEXS(be->hsh.v5.rcp, struct hsh_v5_rcp_s, index,
					word_off);

			} else {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}
			break;

		case HW_HSH_RCP_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if ((unsigned int)word_off >= be->hsh.nb_rcp) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->hsh.v5.rcp, struct hsh_v5_rcp_s, index, word_off,
				be->hsh.nb_rcp);
			break;

		case HW_HSH_RCP_LOAD_DIST_TYPE:
			GET_SET(be->hsh.v5.rcp[index].load_dist_type, value);
			break;

		case HW_HSH_RCP_MAC_PORT_MASK:
			if (word_off > HSH_RCP_MAC_PORT_MASK_SIZE) {
				WORD_OFF_TOO_LARGE_LOG;
				return WORD_OFF_TOO_LARGE;
			}

			GET_SET(be->hsh.v5.rcp[index].mac_port_mask[word_off], value);
			break;

		case HW_HSH_RCP_SORT:
			GET_SET(be->hsh.v5.rcp[index].sort, value);
			break;

		case HW_HSH_RCP_QW0_PE:
			GET_SET(be->hsh.v5.rcp[index].qw0_pe, value);
			break;

		case HW_HSH_RCP_QW0_OFS:
			GET_SET_SIGNED(be->hsh.v5.rcp[index].qw0_ofs, value);
			break;

		case HW_HSH_RCP_QW4_PE:
			GET_SET(be->hsh.v5.rcp[index].qw4_pe, value);
			break;

		case HW_HSH_RCP_QW4_OFS:
			GET_SET_SIGNED(be->hsh.v5.rcp[index].qw4_ofs, value);
			break;

		case HW_HSH_RCP_W8_PE:
			GET_SET(be->hsh.v5.rcp[index].w8_pe, value);
			break;

		case HW_HSH_RCP_W8_OFS:
			GET_SET_SIGNED(be->hsh.v5.rcp[index].w8_ofs, value);
			break;

		case HW_HSH_RCP_W8_SORT:
			GET_SET(be->hsh.v5.rcp[index].w8_sort, value);
			break;

		case HW_HSH_RCP_W9_PE:
			GET_SET(be->hsh.v5.rcp[index].w9_pe, value);
			break;

		case HW_HSH_RCP_W9_OFS:
			GET_SET_SIGNED(be->hsh.v5.rcp[index].w9_ofs, value);
			break;

		case HW_HSH_RCP_W9_SORT:
			GET_SET(be->hsh.v5.rcp[index].w9_sort, value);
			break;

		case HW_HSH_RCP_W9_P:
			GET_SET(be->hsh.v5.rcp[index].w9_p, value);
			break;

		case HW_HSH_RCP_P_MASK:
			GET_SET(be->hsh.v5.rcp[index].p_mask, value);
			break;

		case HW_HSH_RCP_WORD_MASK:
			if (word_off >= HSH_RCP_WORD_MASK_SIZE) {
				WORD_OFF_TOO_LARGE_LOG;
				return WORD_OFF_TOO_LARGE;
			}

			GET_SET(be->hsh.v5.rcp[index].word_mask[word_off], value);
			break;

		case HW_HSH_RCP_SEED:
			GET_SET(be->hsh.v5.rcp[index].seed, value);
			break;

		case HW_HSH_RCP_TNL_P:
			GET_SET(be->hsh.v5.rcp[index].tnl_p, value);
			break;

		case HW_HSH_RCP_HSH_VALID:
			GET_SET(be->hsh.v5.rcp[index].hsh_valid, value);
			break;

		case HW_HSH_RCP_HSH_TYPE:
			GET_SET(be->hsh.v5.rcp[index].hsh_type, value);
			break;

		case HW_HSH_RCP_TOEPLITZ:
			GET_SET(be->hsh.v5.rcp[index].toeplitz, value);
			break;

		case HW_HSH_RCP_K:
			if (word_off > HSH_RCP_KEY_SIZE) {
				WORD_OFF_TOO_LARGE_LOG;
				return WORD_OFF_TOO_LARGE;
			}

			GET_SET(be->hsh.v5.rcp[index].k[word_off], value);
			break;

		case HW_HSH_RCP_AUTO_IPV4_MASK:
			GET_SET(be->hsh.v5.rcp[index].auto_ipv4_mask, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 5 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_hsh_rcp_set(struct flow_api_backend_s *be, enum hw_hsh_e field, uint32_t index,
	uint32_t word_off, uint32_t value)
{
	return hw_mod_hsh_rcp_mod(be, field, index, word_off, &value, 0);
}
