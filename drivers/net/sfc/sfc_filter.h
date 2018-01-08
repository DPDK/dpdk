/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_FILTER_H
#define _SFC_FILTER_H

#include "efx.h"

#include "sfc_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_filter {
	/** Number of elements in match_supported array */
	size_t				supported_match_num;
	/** Driver cache of supported filter match masks */
	uint32_t			*supported_match;
	/** List of flow rules */
	struct sfc_flow_list		flow_list;
};

struct sfc_adapter;

int sfc_filter_attach(struct sfc_adapter *sa);
void sfc_filter_detach(struct sfc_adapter *sa);

boolean_t sfc_filter_is_match_supported(struct sfc_adapter *sa, uint32_t match);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FILTER_H */
