/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_RTSYM_H__
#define __NFP_RTSYM_H__

#define NFP_RTSYM_TYPE_NONE             0
#define NFP_RTSYM_TYPE_OBJECT           1
#define NFP_RTSYM_TYPE_FUNCTION         2
#define NFP_RTSYM_TYPE_ABS              3

#define NFP_RTSYM_TARGET_NONE           0
#define NFP_RTSYM_TARGET_LMEM           -1
#define NFP_RTSYM_TARGET_EMU_CACHE      -7

/*
 * Structure describing a run-time NFP symbol.
 *
 * The memory target of the symbol is generally the CPP target number and can be
 * used directly by the nfp_cpp API calls.  However, in some cases (i.e., for
 * local memory or control store) the target is encoded using a negative number.
 *
 * When the target type can not be used to fully describe the location of a
 * symbol the domain field is used to further specify the location (i.e., the
 * specific ME or island number).
 *
 * For ME target resources, 'domain' is an MEID.
 * For Island target resources, 'domain' is an island ID, with the one exception
 * of "sram" symbols for backward compatibility, which are viewed as global.
 */
struct nfp_rtsym {
	const char *name;
	uint64_t addr;
	uint64_t size;
	int type;
	int target;
	int domain;
};

struct nfp_rtsym_table;

struct nfp_rtsym_table *nfp_rtsym_table_read(struct nfp_cpp *cpp);

struct nfp_rtsym_table *
__nfp_rtsym_table_read(struct nfp_cpp *cpp, const struct nfp_mip *mip);

int nfp_rtsym_count(struct nfp_rtsym_table *rtbl);

const struct nfp_rtsym *nfp_rtsym_get(struct nfp_rtsym_table *rtbl, int idx);

const struct nfp_rtsym *
nfp_rtsym_lookup(struct nfp_rtsym_table *rtbl, const char *name);

uint64_t nfp_rtsym_read_le(struct nfp_rtsym_table *rtbl, const char *name,
			   int *error);
uint8_t *
nfp_rtsym_map(struct nfp_rtsym_table *rtbl, const char *name,
	      unsigned int min_size, struct nfp_cpp_area **area);
#endif
