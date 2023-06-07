/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include "efx.h"
#include "efx_impl.h"

/* List of HW tables that have support in efx */
static const efx_table_id_t efx_supported_table_ids[] = {
	EFX_TABLE_ID_CONNTRACK,
};

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

	__checkReturn		size_t
efx_table_supported_num_get(
	__in			void)
{
	return (EFX_ARRAY_SIZE(efx_supported_table_ids));
}

	__checkReturn		boolean_t
efx_table_is_supported(
	__in			efx_table_id_t table_id)
{
	size_t i;

	for (i = 0; i < efx_table_supported_num_get(); i++) {
		if (efx_supported_table_ids[i] == table_id)
			return (B_TRUE);
	}

	return (B_FALSE);
}

static	__checkReturn			efx_rc_t
efx_table_ct_desc_fields_check(
	__in_ecount(n_fields_descs)	efx_table_field_descriptor_t *fields_descsp,
	__in				unsigned int n_fields_descs)
{
	unsigned int i;
	efx_rc_t rc;

	for (i = 0; i < n_fields_descs; i++) {
		switch (fields_descsp[i].field_id) {
		case EFX_TABLE_FIELD_ID_ETHER_TYPE:
		case EFX_TABLE_FIELD_ID_SRC_IP:
		case EFX_TABLE_FIELD_ID_DST_IP:
		case EFX_TABLE_FIELD_ID_IP_PROTO:
		case EFX_TABLE_FIELD_ID_SRC_PORT:
		case EFX_TABLE_FIELD_ID_DST_PORT:
			if (fields_descsp[i].mask_type != EFX_TABLE_FIELD_MASK_EXACT) {
				rc = EINVAL;
				goto fail1;
			}
			break;
		/*
		 * TODO:
		 * All fields in the CT table have EXACT mask.
		 * All the response field descriptors must have the EXACT mask.
		 * In the current implementation, only the Ethertype, source and
		 * destination IP address, IP protocol, and source and destination IP
		 * are used for the lookup by the key.
		 * FW could use the NEVER mask for the fields in the key that are not
		 * used for the lookup.
		 * As an alternative, a new mask could be added for these fields,
		 * like EXACT_NOT_USED.
		 */
		default:
			if ((fields_descsp[i].mask_type != EFX_TABLE_FIELD_MASK_NEVER) &&
			    (fields_descsp[i].mask_type != EFX_TABLE_FIELD_MASK_EXACT)) {
				rc = EINVAL;
				goto fail2;
			}
			break;
		}
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_table_desc_fields_check(
	__in				efx_table_id_t table_id,
	__in_ecount(n_fields_descs)	efx_table_field_descriptor_t *fields_descsp,
	__in				unsigned int n_fields_descs)
{
	efx_rc_t rc;

	switch (table_id) {
	case EFX_TABLE_ID_CONNTRACK:
		rc = efx_table_ct_desc_fields_check(fields_descsp, n_fields_descs);
		if (rc != 0)
			goto fail1;
		break;
	default:
		break;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static					void
efx_table_desc_fields_get(
	__in				const efx_mcdi_req_t *req,
	__out_ecount(n_fields_descs)	efx_table_field_descriptor_t *fields_descsp,
	__in				unsigned int n_fields_descs)
{
	unsigned int i;

	for (i = 0; i < n_fields_descs; i++) {
		fields_descsp[i].field_id = (efx_table_field_id_t)
		    MCDI_OUT_INDEXED_QWORD_FIELD(*req,
			TABLE_DESCRIPTOR_OUT_FIELDS, i, TABLE_FIELD_DESCR_FIELD_ID);

		fields_descsp[i].lbn =
		    MCDI_OUT_INDEXED_QWORD_FIELD(*req,
			TABLE_DESCRIPTOR_OUT_FIELDS, i, TABLE_FIELD_DESCR_LBN);

		fields_descsp[i].width =
		    MCDI_OUT_INDEXED_QWORD_FIELD(*req,
			TABLE_DESCRIPTOR_OUT_FIELDS, i, TABLE_FIELD_DESCR_WIDTH);

		fields_descsp[i].mask_type = (efx_table_field_mask_type_t)
		    MCDI_OUT_INDEXED_QWORD_FIELD(*req,
			TABLE_DESCRIPTOR_OUT_FIELDS, i, TABLE_FIELD_DESCR_MASK_TYPE);

		fields_descsp[i].scheme =
		    MCDI_OUT_INDEXED_QWORD_FIELD(*req,
			TABLE_DESCRIPTOR_OUT_FIELDS, i, TABLE_FIELD_DESCR_SCHEME);
	}
}

	__checkReturn				efx_rc_t
efx_table_describe(
	__in					efx_nic_t *enp,
	__in					efx_table_id_t table_id,
	__in					uint32_t field_offset,
	__out_opt				efx_table_descriptor_t *table_descp,
	__out_ecount_opt(n_field_descs)		efx_table_field_descriptor_t *fields_descs,
	__in					unsigned int n_field_descs,
	__out_opt				unsigned int *n_field_descs_writtenp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	unsigned int n_entries;
	efx_mcdi_req_t req;
	unsigned int i;
	efx_rc_t rc;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_TABLE_DESCRIPTOR_IN_LEN,
	    MC_CMD_TABLE_DESCRIPTOR_OUT_LENMAX_MCDI2);

	/* Ensure EFX and MCDI use same values for table types */
	EFX_STATIC_ASSERT(EFX_TABLE_TYPE_BCAM == MC_CMD_TABLE_DESCRIPTOR_OUT_TYPE_BCAM);

	/* Ensure EFX and MCDI use same values for table fields */
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_UNUSED == TABLE_FIELD_ID_UNUSED);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_COUNTER_ID == TABLE_FIELD_ID_COUNTER_ID);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_ETHER_TYPE == TABLE_FIELD_ID_ETHER_TYPE);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_SRC_IP == TABLE_FIELD_ID_SRC_IP);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_DST_IP == TABLE_FIELD_ID_DST_IP);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_IP_PROTO == TABLE_FIELD_ID_IP_PROTO);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_SRC_PORT == TABLE_FIELD_ID_SRC_PORT);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_DST_PORT == TABLE_FIELD_ID_DST_PORT);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_NAT_PORT == TABLE_FIELD_ID_NAT_PORT);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_NAT_IP == TABLE_FIELD_ID_NAT_IP);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_NAT_DIR == TABLE_FIELD_ID_NAT_DIR);
	EFX_STATIC_ASSERT(EFX_TABLE_FIELD_ID_CT_MARK == TABLE_FIELD_ID_CT_MARK);

	if (encp->enc_table_api_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (!efx_table_is_supported(table_id)) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((n_field_descs != 0) &&
	    ((fields_descs == NULL) || (n_field_descs_writtenp == NULL))) {
		rc = EINVAL;
		goto fail3;
	}

	req.emr_cmd = MC_CMD_TABLE_DESCRIPTOR;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_TABLE_DESCRIPTOR_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_TABLE_DESCRIPTOR_OUT_LENMAX_MCDI2;

	MCDI_IN_SET_DWORD(req, TABLE_DESCRIPTOR_IN_TABLE_ID, (uint32_t)table_id);
	MCDI_IN_SET_DWORD(req, TABLE_DESCRIPTOR_IN_FIRST_FIELDS_INDEX, field_offset);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail4;
	}

	if (req.emr_out_length_used < MC_CMD_TABLE_DESCRIPTOR_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail5;
	}

	if (table_descp != NULL) {
		table_descp->type = (efx_table_type_t)MCDI_OUT_WORD(
		    req, TABLE_DESCRIPTOR_OUT_TYPE);
		table_descp->key_width = MCDI_OUT_WORD(
		    req, TABLE_DESCRIPTOR_OUT_KEY_WIDTH);
		table_descp->resp_width = MCDI_OUT_WORD(
		    req, TABLE_DESCRIPTOR_OUT_RESP_WIDTH);
		table_descp->n_key_fields = MCDI_OUT_WORD(
		    req, TABLE_DESCRIPTOR_OUT_N_KEY_FIELDS);
		table_descp->n_resp_fields = MCDI_OUT_WORD(
		    req, TABLE_DESCRIPTOR_OUT_N_RESP_FIELDS);
	}

	n_entries = MC_CMD_TABLE_DESCRIPTOR_OUT_FIELDS_NUM(req.emr_out_length_used);

	if (fields_descs != NULL) {
		if (n_entries > n_field_descs) {
			rc = ENOMEM;
			goto fail6;
		}

		efx_table_desc_fields_get(&req, fields_descs, n_entries);
		rc = efx_table_desc_fields_check(table_id, fields_descs, n_entries);
		if (rc != 0)
			goto fail7;
	}

	if (n_field_descs_writtenp != NULL)
		*n_field_descs_writtenp = n_entries;

	return (0);

fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
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

	__checkReturn			efx_rc_t
efx_table_entry_insert(
	__in				efx_nic_t *enp,
	__in				efx_table_id_t table_id,
	__in				uint16_t priority,
	__in				uint16_t mask_id,
	__in				uint16_t key_width,
	__in				uint16_t mask_width,
	__in				uint16_t resp_width,
	__in_bcount(data_size)		uint8_t *entry_datap,
	__in				unsigned int data_size)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	unsigned int n_dwords;
	efx_mcdi_req_t req;
	efx_rc_t rc;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_TABLE_INSERT_IN_LENMAX_MCDI2,
	    MC_CMD_TABLE_INSERT_OUT_LEN);

	/*
	 * Ensure  MCDI number of 32bit units matches EFX maximum possible
	 * data in bytes.
	 */
	EFX_STATIC_ASSERT((MC_CMD_TABLE_INSERT_IN_LENMAX  * sizeof(uint32_t)) ==
	    EFX_TABLE_ENTRY_LENGTH_MAX);

	if (encp->enc_table_api_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((data_size % sizeof(uint32_t)) != 0) {
		rc = EINVAL;
		goto fail2;
	}

	if ((data_size == 0) || (data_size > EFX_TABLE_ENTRY_LENGTH_MAX)) {
		rc = EINVAL;
		goto fail3;
	}

	n_dwords = data_size / sizeof(uint32_t);

	req.emr_cmd = MC_CMD_TABLE_INSERT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_TABLE_INSERT_IN_LEN(n_dwords);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_TABLE_INSERT_OUT_LEN;

	MCDI_IN_SET_DWORD(req, TABLE_INSERT_IN_TABLE_ID, (uint32_t)table_id);
	MCDI_IN_SET_WORD(req, TABLE_INSERT_IN_PRIORITY, priority);
	MCDI_IN_SET_WORD(req, TABLE_INSERT_IN_MASK_ID, mask_id);
	MCDI_IN_SET_WORD(req, TABLE_INSERT_IN_KEY_WIDTH, key_width);
	MCDI_IN_SET_WORD(req, TABLE_INSERT_IN_MASK_WIDTH, mask_width);
	MCDI_IN_SET_WORD(req, TABLE_INSERT_IN_RESP_WIDTH, resp_width);

	memcpy(MCDI_IN2(req, uint8_t, TABLE_INSERT_IN_DATA), entry_datap, data_size);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail4;
	}

	return (0);

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

	__checkReturn			efx_rc_t
efx_table_entry_delete(
	__in				efx_nic_t *enp,
	__in				efx_table_id_t table_id,
	__in				uint16_t mask_id,
	__in				uint16_t key_width,
	__in				uint16_t mask_width,
	__in_bcount(data_size)		uint8_t *entry_datap,
	__in				unsigned int data_size)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	unsigned int n_dwords;
	efx_mcdi_req_t req;
	efx_rc_t rc;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_TABLE_DELETE_IN_LENMAX_MCDI2,
	    MC_CMD_TABLE_DELETE_OUT_LEN);

	/*
	 * Ensure  MCDI number of 32bit units matches EFX maximum possible
	 * data in bytes.
	 */
	EFX_STATIC_ASSERT((MC_CMD_TABLE_DELETE_IN_LENMAX  * sizeof(uint32_t)) ==
		EFX_TABLE_ENTRY_LENGTH_MAX);

	if (encp->enc_table_api_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((data_size % sizeof(uint32_t)) != 0) {
		rc = EINVAL;
		goto fail2;
	}

	if ((data_size == 0) || (data_size > EFX_TABLE_ENTRY_LENGTH_MAX)) {
		rc = EINVAL;
		goto fail3;
	}

	n_dwords = data_size / sizeof(uint32_t);

	req.emr_cmd = MC_CMD_TABLE_DELETE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_TABLE_DELETE_IN_LEN(n_dwords);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_TABLE_DELETE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, TABLE_DELETE_IN_TABLE_ID, (uint32_t)table_id);
	MCDI_IN_SET_WORD(req, TABLE_DELETE_IN_MASK_ID, mask_id);
	MCDI_IN_SET_WORD(req, TABLE_DELETE_IN_KEY_WIDTH, key_width);
	MCDI_IN_SET_WORD(req, TABLE_DELETE_IN_MASK_WIDTH, mask_width);


	memcpy(MCDI_IN2(req, uint8_t, TABLE_DELETE_IN_DATA), entry_datap, data_size);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail4;
	}

	return (0);

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
