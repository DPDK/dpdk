/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_MIP_H__
#define __NFP_MIP_H__

#include "nfp_nffw.h"

struct nfp_mip;

struct nfp_mip *nfp_mip_open(struct nfp_cpp *cpp);
void nfp_mip_close(struct nfp_mip *mip);

const char *nfp_mip_name(const struct nfp_mip *mip);
void nfp_mip_symtab(const struct nfp_mip *mip, uint32_t *addr, uint32_t *size);
void nfp_mip_strtab(const struct nfp_mip *mip, uint32_t *addr, uint32_t *size);
int nfp_nffw_info_mip_first(struct nfp_nffw_info *state, uint32_t *cpp_id,
			    uint64_t *off);
#endif
