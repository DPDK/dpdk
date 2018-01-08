/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_KVARGS_H
#define _SFC_KVARGS_H

#include <rte_kvargs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SFC_KVARG_VALUES_BOOL		"[1|y|yes|on|0|n|no|off]"

#define SFC_KVARG_DEBUG_INIT		"debug_init"

#define SFC_KVARG_MCDI_LOGGING		"mcdi_logging"

#define SFC_KVARG_PERF_PROFILE		"perf_profile"

#define SFC_KVARG_PERF_PROFILE_AUTO		"auto"
#define SFC_KVARG_PERF_PROFILE_THROUGHPUT	"throughput"
#define SFC_KVARG_PERF_PROFILE_LOW_LATENCY	"low-latency"
#define SFC_KVARG_VALUES_PERF_PROFILE \
	"[" SFC_KVARG_PERF_PROFILE_AUTO "|" \
	    SFC_KVARG_PERF_PROFILE_THROUGHPUT "|" \
	    SFC_KVARG_PERF_PROFILE_LOW_LATENCY "]"

#define SFC_KVARG_STATS_UPDATE_PERIOD_MS	"stats_update_period_ms"

#define SFC_KVARG_DATAPATH_EFX		"efx"
#define SFC_KVARG_DATAPATH_EF10		"ef10"
#define SFC_KVARG_DATAPATH_EF10_SIMPLE	"ef10_simple"

#define SFC_KVARG_RX_DATAPATH		"rx_datapath"
#define SFC_KVARG_VALUES_RX_DATAPATH \
	"[" SFC_KVARG_DATAPATH_EFX "|" \
	    SFC_KVARG_DATAPATH_EF10 "]"

#define SFC_KVARG_TX_DATAPATH		"tx_datapath"
#define SFC_KVARG_VALUES_TX_DATAPATH \
	"[" SFC_KVARG_DATAPATH_EFX "|" \
	    SFC_KVARG_DATAPATH_EF10 "|" \
	    SFC_KVARG_DATAPATH_EF10_SIMPLE "]"

struct sfc_adapter;

int sfc_kvargs_parse(struct sfc_adapter *sa);
void sfc_kvargs_cleanup(struct sfc_adapter *sa);

int sfc_kvargs_process(struct sfc_adapter *sa, const char *key_match,
		       arg_handler_t handler, void *opaque_arg);

int sfc_kvarg_bool_handler(const char *key, const char *value_str,
			   void *opaque);
int sfc_kvarg_long_handler(const char *key, const char *value_str,
			   void *opaque);
int sfc_kvarg_string_handler(const char *key, const char *value_str,
			     void *opaque);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_KVARGS_H */
