/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

#ifndef _NFP_CPP_BRIDGE_H_
#define _NFP_CPP_BRIDGE_H_

#include "nfp_common.h"

int nfp_enable_cpp_service(struct nfp_pf_dev *pf_dev);
int nfp_map_service(uint32_t service_id);

#endif /* _NFP_CPP_BRIDGE_H_ */
