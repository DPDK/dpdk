/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include "sfc.h"
#include "sfc_tbl_meta.h"
#include "sfc_tbl_meta_cache.h"

const struct sfc_tbl_meta *
sfc_tbl_meta_lookup(struct sfc_adapter *sa, efx_table_id_t table_id)
{
	struct sfc_tbl_meta *meta;
	struct sfc_tbls *tables = &sa->hw_tables;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (tables->status != SFC_TBLS_STATUS_SUPPORTED)
		return NULL;

	rc = rte_hash_lookup_data(tables->meta.cache, (const void *)&table_id,
				  (void **)&meta);
	if (rc < 0)
		return NULL;

	SFC_ASSERT(meta != NULL);
	SFC_ASSERT(meta->table_id == table_id);

	return meta;
}

int
sfc_tbl_meta_init(struct sfc_adapter *sa)
{
	struct sfc_tbls *tables = &sa->hw_tables;
	struct sfc_tbl_meta_cache *meta = &tables->meta;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (tables->status != SFC_TBLS_STATUS_SUPPORTED)
		return 0;

	rc = sfc_tbl_meta_cache_ctor(&meta->cache);
	if (rc != 0)
		return rc;

	rc = sfc_tbl_meta_cache_update(meta->cache, sa->nic);
	if (rc != 0) {
		sfc_tbl_meta_cache_dtor(&meta->cache);
		return rc;
	}

	return 0;
}

void
sfc_tbl_meta_fini(struct sfc_adapter *sa)
{
	struct sfc_tbls *tables = &sa->hw_tables;
	struct sfc_tbl_meta_cache *meta = &tables->meta;

	if (meta->cache == NULL)
		return;

	SFC_ASSERT(sfc_adapter_is_locked(sa));
	SFC_ASSERT(tables->status == SFC_TBLS_STATUS_SUPPORTED);

	sfc_tbl_meta_cache_dtor(&meta->cache);
}
