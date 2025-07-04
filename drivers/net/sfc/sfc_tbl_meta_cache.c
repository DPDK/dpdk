/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include <rte_malloc.h>
#include <rte_jhash.h>

#include "sfc_tbl_meta_cache.h"
#include "sfc_debug.h"

/* The minimal size of the table meta cache */
#define SFC_TBL_META_CACHE_SIZE_MIN	8

int
sfc_tbl_meta_cache_ctor(struct rte_hash **ref_cache)
{
	size_t cache_size = RTE_MAX((unsigned int)SFC_TBL_META_CACHE_SIZE_MIN,
				    efx_table_supported_num_get());
	struct rte_hash *cache = NULL;
	const struct rte_hash_parameters hash_params = {
		.name       = "meta_hash_table",
		.hash_func  = rte_jhash,
		.entries    = cache_size,
		.socket_id  = rte_socket_id(),
		.key_len    = sizeof(efx_table_id_t),
	};

	cache = rte_hash_create(&hash_params);
	if (cache == NULL)
		return -ENOMEM;

	*ref_cache = cache;

	return 0;
}

static void
sfc_tbl_meta_cache_free(struct sfc_tbl_meta *meta)
{
	SFC_ASSERT(meta != NULL);

	rte_free(meta->keys);
	rte_free(meta->responses);
	rte_free(meta);
}

void
sfc_tbl_meta_cache_dtor(struct rte_hash **ref_cache)
{
	struct rte_hash *cache = *ref_cache;
	const void *next_key;
	uint32_t iter = 0;
	void *next_meta;

	if (cache == NULL)
		return;

	while (rte_hash_iterate(cache, &next_key, &next_meta, &iter) >= 0)
		sfc_tbl_meta_cache_free((struct sfc_tbl_meta *)next_meta);

	rte_hash_free(cache);

	*ref_cache = NULL;
}

/**
 * Table descriptor contains information about the table's
 * fields that can be associated with both the key and the response.
 * Save these fields by separating the key from the response in
 * appropriate places based on their lengths.
 */
static int
sfc_tbl_meta_desc_fields_copy(struct sfc_tbl_meta *meta, efx_nic_t *enp,
			      efx_table_field_descriptor_t *fields,
			      unsigned int total_n_fields)
{
	uint16_t n_key_fields = meta->descriptor.n_key_fields;
	size_t field_size = sizeof(*fields);
	unsigned int n_field_descs_written;
	uint32_t field_offset;
	int rc;

	for (n_field_descs_written = 0, field_offset = 0;
	     field_offset < total_n_fields;
	     field_offset += n_field_descs_written) {
		rc = efx_table_describe(enp, meta->table_id, field_offset, NULL,
					fields, total_n_fields, &n_field_descs_written);
		if (rc != 0)
			return rc;

		if (field_offset + n_field_descs_written > total_n_fields)
			return -EINVAL;

		if (field_offset < n_key_fields &&
		    field_offset + n_field_descs_written > n_key_fields) {
			/*
			 * Some of the descriptors belong to key,
			 * the other to response.
			 */
			rte_memcpy(RTE_PTR_ADD(meta->keys, field_offset * field_size),
				   fields, (n_key_fields - field_offset) * field_size);
			rte_memcpy(meta->responses,
				   RTE_PTR_ADD(fields,
				   (n_key_fields - field_offset) * field_size),
				   (field_offset + n_field_descs_written - n_key_fields) *
				   field_size);
		} else if (field_offset < n_key_fields) {
			/* All fields belong to the key */
			rte_memcpy(RTE_PTR_ADD(meta->keys, field_offset * field_size),
				   fields, n_field_descs_written * field_size);
		} else {
			/* All fields belong to the response */
			rte_memcpy(RTE_PTR_ADD(meta->responses,
				   (field_offset - n_key_fields) * field_size),
				   fields, n_field_descs_written * field_size);
		}
	}

	return 0;
}

static int
sfc_tbl_meta_desc_read(struct sfc_tbl_meta *meta, efx_nic_t *enp,
		       efx_table_id_t table_id)
{
	efx_table_field_descriptor_t *fields;
	unsigned int total_n_fields;
	int rc;

	rc = efx_table_describe(enp, table_id, 0, &meta->descriptor, NULL, 0, NULL);
	if (rc != 0)
		return rc;

	total_n_fields = meta->descriptor.n_key_fields + meta->descriptor.n_resp_fields;

	fields = rte_calloc(NULL, total_n_fields, sizeof(*fields), 0);
	if (fields == NULL)
		return -ENOMEM;

	meta->table_id = table_id;

	meta->keys = rte_calloc("efx_table_key_field_descs",
				meta->descriptor.n_key_fields,
				sizeof(*meta->keys), 0);
	if (meta->keys == NULL) {
		rc = -ENOMEM;
		goto fail_alloc_keys;
	}

	meta->responses = rte_calloc("efx_table_response_field_descs",
				     meta->descriptor.n_resp_fields,
				     sizeof(*meta->responses), 0);
	if (meta->responses == NULL) {
		rc = -ENOMEM;
		goto fail_alloc_responses;
	}

	rc = sfc_tbl_meta_desc_fields_copy(meta, enp, fields, total_n_fields);
	if (rc != 0)
		goto fail_copy_fields;

	return 0;

fail_copy_fields:
	rte_free(meta->responses);
fail_alloc_responses:
	rte_free(meta->keys);
fail_alloc_keys:
	rte_free(fields);

	return rc;
}

static int
sfc_tbl_meta_cache_add(struct rte_hash *cache, efx_nic_t *enp,
		       efx_table_id_t table_id)
{
	struct sfc_tbl_meta *meta = NULL;
	int rc;

	meta = rte_zmalloc("sfc_tbl_meta", sizeof(*meta), 0);
	if (meta == NULL)
		return -ENOMEM;

	rc = sfc_tbl_meta_desc_read(meta, enp, table_id);
	if (rc != 0)
		goto fail_read_meta;

	rc = rte_hash_add_key_data(cache, &table_id, meta);
	if (rc != 0)
		goto fail_add_key;

	return 0;

fail_add_key:
	rte_free(meta->keys);
	rte_free(meta->responses);
fail_read_meta:
	rte_free(meta);

	return rc;
}

int
sfc_tbl_meta_cache_update(struct rte_hash *cache, efx_nic_t *enp)
{
	efx_table_id_t *table_ids = NULL;
	unsigned int n_table_ids_written;
	unsigned int total_n_tables;
	unsigned int n_table_ids;
	uint32_t table_index;
	unsigned int i;
	int rc = 0;

	rc = efx_table_list(enp, 0, &total_n_tables, NULL, 0, NULL);
	if (rc != 0)
		return rc;

	table_ids = rte_calloc(NULL, total_n_tables, sizeof(*table_ids), 0);
	if (table_ids == NULL)
		return -ENOMEM;

	n_table_ids = total_n_tables;

	for (table_index = 0, n_table_ids_written = 0;
	     table_index < total_n_tables;
	     table_index += n_table_ids_written) {
		rc = efx_table_list(enp, table_index, NULL,
				    table_ids, n_table_ids, &n_table_ids_written);
		if (rc != 0)
			goto out;

		if (table_index + n_table_ids_written > total_n_tables) {
			rc = -EINVAL;
			goto out;
		}

		for (i = 0; i < n_table_ids_written; i++, table_index++) {
			if (!efx_table_is_supported(table_ids[i]))
				continue;

			rc = sfc_tbl_meta_cache_add(cache, enp, table_ids[i]);
			if (rc != 0)
				goto out;
		}
	}

out:
	rte_free(table_ids);

	return rc;
}
