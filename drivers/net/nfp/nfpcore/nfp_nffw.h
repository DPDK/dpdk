/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NFFW_H__
#define __NFP_NFFW_H__

#include "nfp_cpp.h"

struct nfp_nffw_info;

struct nfp_nffw_info *nfp_nffw_info_open(struct nfp_cpp *cpp);
void nfp_nffw_info_close(struct nfp_nffw_info *state);
int nfp_nffw_info_mip_first(struct nfp_nffw_info *state, uint32_t *cpp_id,
		uint64_t *offset);

#endif /* __NFP_NFFW_H__ */
