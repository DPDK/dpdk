/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
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
	/**
	 * Supports any of ip_proto, remote host or local host
	 * filters. This flag is used for filter match exceptions
	 */
	boolean_t			supports_ip_proto_or_addr_filter;
	/**
	 * Supports any of remote port or local port filters.
	 * This flag is used for filter match exceptions
	 */
	boolean_t			supports_rem_or_local_port_filter;
};

struct sfc_adapter;

int sfc_filter_attach(struct sfc_adapter *sa);
void sfc_filter_detach(struct sfc_adapter *sa);

boolean_t sfc_filter_is_match_supported(struct sfc_adapter *sa, uint32_t match);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FILTER_H */
