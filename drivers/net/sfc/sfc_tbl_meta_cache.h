/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#ifndef _SFC_TBL_META_CACHE_H
#define _SFC_TBL_META_CACHE_H

#include <rte_hash.h>

#include "efx.h"

#include "sfc_tbl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

int sfc_tbl_meta_cache_ctor(struct rte_hash **ref_cache);

void sfc_tbl_meta_cache_dtor(struct rte_hash **ref_cache);

int sfc_tbl_meta_cache_update(struct rte_hash *cache, efx_nic_t *enp);

#endif /* _SFC_TBLS_DESC_CACHE_H */
