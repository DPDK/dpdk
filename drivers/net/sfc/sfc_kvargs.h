/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
