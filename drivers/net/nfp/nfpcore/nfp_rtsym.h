/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_RTSYM_H__
#define __NFP_RTSYM_H__

#include "nfp_cpp.h"

struct nfp_rtsym;
struct nfp_rtsym_table;

struct nfp_rtsym_table *nfp_rtsym_table_read(struct nfp_cpp *cpp);

int nfp_rtsym_count(struct nfp_rtsym_table *rtbl);

const struct nfp_rtsym *nfp_rtsym_get(struct nfp_rtsym_table *rtbl, int idx);

const struct nfp_rtsym *nfp_rtsym_lookup(struct nfp_rtsym_table *rtbl,
		const char *name);

int nfp_rtsym_read(struct nfp_cpp *cpp, const struct nfp_rtsym *sym,
		uint64_t offset, void *buf, size_t len);
int nfp_rtsym_readl(struct nfp_cpp *cpp, const struct nfp_rtsym *sym,
		uint64_t offset, uint32_t *value);
int nfp_rtsym_readq(struct nfp_cpp *cpp, const struct nfp_rtsym *sym,
		uint64_t offset, uint64_t *value);

int nfp_rtsym_write(struct nfp_cpp *cpp, const struct nfp_rtsym *sym,
		uint64_t offset, void *buf, size_t len);
int nfp_rtsym_writel(struct nfp_cpp *cpp, const struct nfp_rtsym *sym,
		uint64_t offset, uint32_t value);
int nfp_rtsym_writeq(struct nfp_cpp *cpp, const struct nfp_rtsym *sym,
		uint64_t offset, uint64_t value);

uint64_t nfp_rtsym_read_le(struct nfp_rtsym_table *rtbl, const char *name,
		int *error);
int nfp_rtsym_write_le(struct nfp_rtsym_table *rtbl, const char *name,
		uint64_t value);
uint8_t *nfp_rtsym_map(struct nfp_rtsym_table *rtbl, const char *name,
		uint32_t min_size, struct nfp_cpp_area **area);

#endif /* __NFP_RTSYM_H__ */
