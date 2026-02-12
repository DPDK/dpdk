/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_RESOURCES_H_
#define _TFC_RESOURCES_H_
#include <rte_common.h>
#include "bnxt.h"

#ifdef TF_FLOW_SCALE_QUERY
void tfc_resc_usage_query_all(struct bnxt *bp);
#endif /* TF_FLOW_SCALE_QUERY */

#endif /* _TFC_RESOURCES_H_ */
