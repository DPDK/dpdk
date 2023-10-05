/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_HWINFO_H__
#define __NFP_HWINFO_H__

#include "nfp_cpp.h"

struct nfp_hwinfo;

struct nfp_hwinfo *nfp_hwinfo_read(struct nfp_cpp *cpp);

const char *nfp_hwinfo_lookup(struct nfp_hwinfo *hwinfo, const char *lookup);

#endif /* __NFP_HWINFO_H__ */
