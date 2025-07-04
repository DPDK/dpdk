/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include "sfc.h"
#include "sfc_mae_ct.h"

/* SF-123102-TC-1A § 10.6.3: Conntrack_Table key */
static void
sfc_mae_ct_key_to_mcdi_key(const sfc_mae_conntrack_key_t *key,
			   const efx_table_field_descriptor_t *fields,
			   unsigned int n_fields, uint32_t *mcdi_key,
			   unsigned int key_size)
{
	unsigned int i;

	for (i = 0; i < n_fields; i++) {
		const efx_table_field_descriptor_t *desc = &fields[i];

		if (desc->mask_type == EFX_TABLE_FIELD_MASK_NEVER)
			continue;

		switch (desc->field_id) {
		case EFX_TABLE_FIELD_ID_IP_PROTO:
			sfc_tbls_field_set_u8(mcdi_key, key_size, desc->lbn,
					      desc->width, key->ip_proto);
			break;
		case EFX_TABLE_FIELD_ID_ETHER_TYPE:
			sfc_tbls_field_set_u16(mcdi_key, key_size, desc->lbn,
					       desc->width, key->ether_type_le);
			break;
		case EFX_TABLE_FIELD_ID_SRC_PORT:
			sfc_tbls_field_set_u16(mcdi_key, key_size, desc->lbn,
					       desc->width, key->src_port_le);
			break;
		case EFX_TABLE_FIELD_ID_DST_PORT:
			sfc_tbls_field_set_u16(mcdi_key, key_size, desc->lbn,
					       desc->width, key->dst_port_le);
			break;
		case EFX_TABLE_FIELD_ID_SRC_IP:
			sfc_tbls_field_set_ip(mcdi_key, key_size, desc->lbn,
					      desc->width,
					      (const uint32_t *)key->src_addr_le);
			break;
		case EFX_TABLE_FIELD_ID_DST_IP:
			sfc_tbls_field_set_ip(mcdi_key, key_size, desc->lbn,
					      desc->width,
					      (const uint32_t *)key->dst_addr_le);
			break;

		default:
			break;
		}
	}
}

/* SF-123102-TC-1A § 10.6.4: Conntrack_Table response */
static void
sfc_mae_ct_response_to_mcdi_response(const sfc_mae_conntrack_response_t *response,
				     const efx_table_field_descriptor_t *fields,
				     unsigned int n_fields, uint32_t *mcdi_resp,
				     unsigned int resp_size)
{
	unsigned int i;

	for (i = 0; i < n_fields; i++) {
		const efx_table_field_descriptor_t *desc = &fields[i];

		if (desc->mask_type == EFX_TABLE_FIELD_MASK_NEVER)
			continue;

		/* Fields of responses are always reported with the EXACT type. */
		SFC_ASSERT(desc->mask_type == EFX_TABLE_FIELD_MASK_EXACT);

		switch (desc->field_id) {
		case EFX_TABLE_FIELD_ID_CT_MARK:
			sfc_tbls_field_set_u32(mcdi_resp, resp_size, desc->lbn,
					       desc->width, response->ct_mark);
			break;
		case EFX_TABLE_FIELD_ID_COUNTER_ID:
			sfc_tbls_field_set_u32(mcdi_resp, resp_size, desc->lbn,
					       desc->width, response->counter_id);
			break;
		case EFX_TABLE_FIELD_ID_NAT_DIR:
			sfc_tbls_field_set_u8(mcdi_resp, resp_size, desc->lbn,
					      desc->width, response->nat.dir_is_dst);
			break;
		case EFX_TABLE_FIELD_ID_NAT_IP:
			sfc_tbls_field_set_u32(mcdi_resp, resp_size, desc->lbn,
					       desc->width, response->nat.ip_le);
			break;
		case EFX_TABLE_FIELD_ID_NAT_PORT:
			sfc_tbls_field_set_u16(mcdi_resp, resp_size, desc->lbn,
					       desc->width, response->nat.port_le);
			break;

		default:
			break;
		}
	}
}

int
sfc_mae_conntrack_insert(struct sfc_adapter *sa,
			 const sfc_mae_conntrack_key_t *key,
			 const sfc_mae_conntrack_response_t *response)
{
	const struct sfc_tbls *tables = &sa->hw_tables;
	uint8_t data[EFX_TABLE_ENTRY_LENGTH_MAX] = {0};
	const struct sfc_tbl_meta *meta = NULL;
	unsigned int response_size;
	uint32_t *response_data;
	unsigned int data_size;
	unsigned int key_size;
	uint32_t *start_data;
	uint16_t resp_width;
	uint16_t key_width;
	uint32_t *key_data;
	uint32_t *end_data;
	int rc = 0;

	if (tables->status != SFC_TBLS_STATUS_SUPPORTED)
		return -ENOTSUP;

	if (!sfc_mae_conntrack_is_supported(sa))
		return -ENOTSUP;

	meta = sfc_mae_conntrack_meta_lookup(sa);
	if (meta == NULL)
		return -ENOENT;

	key_width = meta->descriptor.key_width;
	resp_width = meta->descriptor.resp_width;

	start_data = (uint32_t *)data;
	key_data = start_data;
	response_data = sfc_tbls_next_req_fields(key_data, key_width);
	end_data = sfc_tbls_next_req_fields(response_data, resp_width);

	key_size = RTE_PTR_DIFF(response_data, key_data);
	response_size = RTE_PTR_DIFF(end_data, response_data);
	data_size = RTE_PTR_DIFF(end_data, start_data);
	SFC_ASSERT(data_size <= sizeof(data));

	sfc_mae_ct_key_to_mcdi_key(key, meta->keys,
				   meta->descriptor.n_key_fields, key_data,
				   key_size);
	sfc_mae_ct_response_to_mcdi_response(response, meta->responses,
					     meta->descriptor.n_resp_fields,
					     response_data, response_size);

	rc = sfc_tbls_bcam_entry_insert(sa->nic, EFX_TABLE_ID_CONNTRACK,
					key_width, resp_width, data,
					data_size);

	return rc;
}

int
sfc_mae_conntrack_delete(struct sfc_adapter *sa,
			 const sfc_mae_conntrack_key_t *key)
{
	const struct sfc_tbls *tables = &sa->hw_tables;
	uint8_t data[EFX_TABLE_ENTRY_LENGTH_MAX] = {0};
	const struct sfc_tbl_meta *meta = NULL;
	unsigned int data_size;
	uint32_t *start_data;
	uint16_t key_width;
	uint32_t *key_data;
	uint32_t *end_data;
	int rc = 0;

	if (tables->status != SFC_TBLS_STATUS_SUPPORTED)
		return -ENOTSUP;

	if (!sfc_mae_conntrack_is_supported(sa))
		return -ENOTSUP;

	meta = sfc_mae_conntrack_meta_lookup(sa);
	if (meta == NULL)
		return -ENOENT;

	key_width = meta->descriptor.key_width;

	start_data = (uint32_t *)data;
	key_data = start_data;
	end_data = sfc_tbls_next_req_fields(key_data, key_width);

	data_size = RTE_PTR_DIFF(end_data, start_data);
	SFC_ASSERT(data_size <= sizeof(data));

	sfc_mae_ct_key_to_mcdi_key(key, meta->keys,
				   meta->descriptor.n_key_fields,
				   key_data, data_size);

	rc = sfc_tbls_bcam_entry_delete(sa->nic, EFX_TABLE_ID_CONNTRACK,
					key_width, data, data_size);

	return rc;
}
