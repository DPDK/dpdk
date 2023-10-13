/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_RESOURCE_H__
#define __NFP_RESOURCE_H__

#include "nfp_cpp.h"

/* Netronone Flow Firmware Table */
#define NFP_RESOURCE_NFP_NFFW           "nfp.nffw"

/* NFP Hardware Info Database */
#define NFP_RESOURCE_NFP_HWINFO         "nfp.info"

/* Service Processor */
#define NFP_RESOURCE_NSP                "nfp.sp"

/* Opaque handle to a NFP Resource */
struct nfp_resource;

struct nfp_resource *nfp_resource_acquire(struct nfp_cpp *cpp,
		const char *name);

void nfp_resource_release(struct nfp_resource *res);

uint32_t nfp_resource_cpp_id(const struct nfp_resource *res);

const char *nfp_resource_name(const struct nfp_resource *res);

uint64_t nfp_resource_address(const struct nfp_resource *res);

uint64_t nfp_resource_size(const struct nfp_resource *res);

#endif /* __NFP_RESOURCE_H__ */
