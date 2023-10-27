/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CPP_BRIDGE_H__
#define __NFP_CPP_BRIDGE_H__

#include "nfp_net_common.h"

int nfp_enable_cpp_service(struct nfp_pf_dev *pf_dev);
int nfp_map_service(uint32_t service_id);

#endif /* __NFP_CPP_BRIDGE_H__ */
