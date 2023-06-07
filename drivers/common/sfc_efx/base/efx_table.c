/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include "efx.h"
#include "efx_impl.h"

	__checkReturn				efx_rc_t
efx_table_list(
	__in					efx_nic_t *enp,
	__in					uint32_t entry_ofst,
	__out_opt				unsigned int *total_n_tablesp,
	__out_ecount_opt(n_table_ids)		efx_table_id_t *table_ids,
	__in					unsigned int n_table_ids,
	__out_opt				unsigned int *n_table_ids_writtenp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	unsigned int n_entries;
	efx_mcdi_req_t req;
	unsigned int i;
	efx_rc_t rc;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_TABLE_LIST_IN_LEN,
	    MC_CMD_TABLE_LIST_OUT_LENMAX_MCDI2);

	/* Ensure EFX and MCDI use same values for table IDs */
	EFX_STATIC_ASSERT(EFX_TABLE_ID_CONNTRACK == TABLE_ID_CONNTRACK_TABLE);

	if (encp->enc_table_api_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((n_table_ids != 0) &&
	   ((table_ids == NULL) || (n_table_ids_writtenp == NULL))) {
		rc = EINVAL;
		goto fail2;
	}

	req.emr_cmd = MC_CMD_TABLE_LIST;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_TABLE_LIST_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_TABLE_LIST_OUT_LENMAX_MCDI2;

	MCDI_IN_SET_DWORD(req, TABLE_LIST_IN_FIRST_TABLE_ID_INDEX, entry_ofst);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	if (req.emr_out_length_used < MC_CMD_TABLE_LIST_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail4;
	}

	if (total_n_tablesp != NULL)
		*total_n_tablesp = MCDI_OUT_DWORD(req, TABLE_LIST_OUT_N_TABLES);

	n_entries = MC_CMD_TABLE_LIST_OUT_TABLE_ID_NUM(req.emr_out_length_used);

	if (table_ids != NULL) {
		if (n_entries > n_table_ids) {
			rc = ENOMEM;
			goto fail5;
		}

		for (i = 0; i < n_entries; i++) {
			table_ids[i] = MCDI_OUT_INDEXED_DWORD(req,
			    TABLE_LIST_OUT_TABLE_ID, i);
		}
	}

	if (n_table_ids_writtenp != NULL)
		*n_table_ids_writtenp = n_entries;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
