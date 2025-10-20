/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "PDB"
#define _VER_ be->pdb.ver

bool nthw_mod_pdb_present(struct flow_api_backend_s *be)
{
	return be->iface->get_pdb_present(be->be_dev);
}

int nthw_mod_pdb_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_pdb_version(be->be_dev);
	NT_LOG(DBG, FILTER, "PDB MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	nb = be->iface->get_nb_pdb_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(pdb_categories);
		return COUNT_ERROR;
	}

	be->pdb.nb_pdb_rcp_categories = (uint32_t)nb;

	switch (_VER_) {
	case 9:
		if (!nthw_callocate_mod((struct common_func_s *)&be->pdb, 2, &be->pdb.v9.rcp,
				be->pdb.nb_pdb_rcp_categories, sizeof(struct pdb_v9_rcp_s),
				&be->pdb.v9.config, 1, sizeof(struct pdb_v9_config_s)))
			return -1;

		break;

	/* end case 9 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void nthw_mod_pdb_free(struct flow_api_backend_s *be)
{
	if (be->pdb.base) {
		free(be->pdb.base);
		be->pdb.base = NULL;
	}
}

int nthw_mod_pdb_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->pdb.nb_pdb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->pdb.nb_pdb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->pdb_rcp_flush(be->be_dev, &be->pdb, start_idx, count);
}

static int hw_mod_pdb_rcp_mod(struct flow_api_backend_s *be, enum hw_pdb_e field, uint32_t index,
	uint32_t *value, int get)
{
	if (index >= be->pdb.nb_pdb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 9:
		switch (field) {
		case HW_PDB_RCP_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->pdb.v9.rcp[index], (uint8_t)*value,
				sizeof(struct pdb_v9_rcp_s));
			break;

		case HW_PDB_RCP_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->pdb.nb_pdb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->pdb.v9.rcp, struct pdb_v9_rcp_s, index, *value,
				be->pdb.nb_pdb_rcp_categories);
			break;

		case HW_PDB_RCP_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->pdb.nb_pdb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}
			/* Size of the structure */
			size_t element_size = sizeof(struct pdb_v9_rcp_s);
			/* Size of the buffer */
			size_t buffer_size = sizeof(be->pdb.v9.rcp);

			/* Calculate the maximum valid index (number of elements in the buffer) */
			size_t max_idx = buffer_size / element_size;

			/* Check that both indices are within bounds before calling the macro */
			if (index < max_idx && *value < max_idx) {
				DO_COMPARE_INDEXS(be->pdb.v9.rcp, struct pdb_v9_rcp_s, index,
					*value);
			} else {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}
			break;

		case HW_PDB_RCP_DESCRIPTOR:
			GET_SET(be->pdb.v9.rcp[index].descriptor, value);
			break;

		case HW_PDB_RCP_DESC_LEN:
			GET_SET(be->pdb.v9.rcp[index].desc_len, value);
			break;

		case HW_PDB_RCP_TX_PORT:
			GET_SET(be->pdb.v9.rcp[index].tx_port, value);
			break;

		case HW_PDB_RCP_TX_IGNORE:
			GET_SET(be->pdb.v9.rcp[index].tx_ignore, value);
			break;

		case HW_PDB_RCP_TX_NOW:
			GET_SET(be->pdb.v9.rcp[index].tx_now, value);
			break;

		case HW_PDB_RCP_CRC_OVERWRITE:
			GET_SET(be->pdb.v9.rcp[index].crc_overwrite, value);
			break;

		case HW_PDB_RCP_ALIGN:
			GET_SET(be->pdb.v9.rcp[index].align, value);
			break;

		case HW_PDB_RCP_OFS0_DYN:
			GET_SET(be->pdb.v9.rcp[index].ofs0_dyn, value);
			break;

		case HW_PDB_RCP_OFS0_REL:
			GET_SET_SIGNED(be->pdb.v9.rcp[index].ofs0_rel, value);
			break;

		case HW_PDB_RCP_OFS1_DYN:
			GET_SET(be->pdb.v9.rcp[index].ofs1_dyn, value);
			break;

		case HW_PDB_RCP_OFS1_REL:
			GET_SET_SIGNED(be->pdb.v9.rcp[index].ofs1_rel, value);
			break;

		case HW_PDB_RCP_OFS2_DYN:
			GET_SET(be->pdb.v9.rcp[index].ofs2_dyn, value);
			break;

		case HW_PDB_RCP_OFS2_REL:
			GET_SET_SIGNED(be->pdb.v9.rcp[index].ofs2_rel, value);
			break;

		case HW_PDB_RCP_IP_PROT_TNL:
			GET_SET(be->pdb.v9.rcp[index].ip_prot_tnl, value);
			break;

		case HW_PDB_RCP_PPC_HSH:
			GET_SET(be->pdb.v9.rcp[index].ppc_hsh, value);
			break;

		case HW_PDB_RCP_DUPLICATE_EN:
			GET_SET(be->pdb.v9.rcp[index].duplicate_en, value);
			break;

		case HW_PDB_RCP_DUPLICATE_BIT:
			GET_SET(be->pdb.v9.rcp[index].duplicate_bit, value);
			break;

		case HW_PDB_RCP_PCAP_KEEP_FCS:
			GET_SET(be->pdb.v9.rcp[index].pcap_keep_fcs, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 9 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_pdb_rcp_set(struct flow_api_backend_s *be, enum hw_pdb_e field, uint32_t index,
	uint32_t value)
{
	return hw_mod_pdb_rcp_mod(be, field, index, &value, 0);
}

static int hw_mod_pdb_config_flush(struct flow_api_backend_s *be)
{
	return be->iface->pdb_config_flush(be->be_dev, &be->pdb);
}

int nthw_mod_pdb_reset(struct flow_api_backend_s *be)
{
	int err = 0;
	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->hsh));

	NT_LOG(DBG, FILTER, "INIT PDB RCP");
	err |= nthw_mod_pdb_rcp_flush(be, 0, ALL_ENTRIES);

	NT_LOG(DBG, FILTER, "INIT PDB CONFIG");
	err |= hw_mod_pdb_config_flush(be);
	return err;
}
