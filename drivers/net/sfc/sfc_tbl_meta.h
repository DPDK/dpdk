/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#ifndef _SFC_TBL_META_H
#define _SFC_TBL_META_H

#include <rte_hash.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Metadata about table layout */
struct sfc_tbl_meta {
	efx_table_id_t			table_id;
	efx_table_descriptor_t		descriptor;
	efx_table_field_descriptor_t	*keys;
	efx_table_field_descriptor_t	*responses;
};

struct sfc_tbl_meta_cache {
	struct rte_hash	*cache;
};

struct sfc_adapter;

const struct sfc_tbl_meta *sfc_tbl_meta_lookup(struct sfc_adapter *sa,
					       efx_table_id_t table_id);

int sfc_tbl_meta_init(struct sfc_adapter *sa);
void sfc_tbl_meta_fini(struct sfc_adapter *sa);

#endif /* _SFC_TBL_META_H */
