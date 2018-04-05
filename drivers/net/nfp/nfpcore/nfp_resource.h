/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef NFP_RESOURCE_H
#define NFP_RESOURCE_H

#include "nfp_cpp.h"

#define NFP_RESOURCE_NFP_NFFW           "nfp.nffw"
#define NFP_RESOURCE_NFP_HWINFO         "nfp.info"
#define NFP_RESOURCE_NSP		"nfp.sp"

/**
 * Opaque handle to a NFP Resource
 */
struct nfp_resource;

struct nfp_resource *nfp_resource_acquire(struct nfp_cpp *cpp,
					  const char *name);

/**
 * Release a NFP Resource, and free the handle
 * @param[in]   res     NFP Resource handle
 */
void nfp_resource_release(struct nfp_resource *res);

/**
 * Return the CPP ID of a NFP Resource
 * @param[in]   res     NFP Resource handle
 * @return      CPP ID of the NFP Resource
 */
uint32_t nfp_resource_cpp_id(const struct nfp_resource *res);

/**
 * Return the name of a NFP Resource
 * @param[in]   res     NFP Resource handle
 * @return      Name of the NFP Resource
 */
const char *nfp_resource_name(const struct nfp_resource *res);

/**
 * Return the target address of a NFP Resource
 * @param[in]   res     NFP Resource handle
 * @return      Address of the NFP Resource
 */
uint64_t nfp_resource_address(const struct nfp_resource *res);

uint64_t nfp_resource_size(const struct nfp_resource *res);

#endif /* NFP_RESOURCE_H */
