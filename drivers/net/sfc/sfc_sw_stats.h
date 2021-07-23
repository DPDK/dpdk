/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */
#ifndef _SFC_SW_STATS_H
#define _SFC_SW_STATS_H

#include <rte_dev.h>

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

void sfc_sw_xstats_get_vals(struct sfc_adapter *sa,
			    struct rte_eth_xstat *xstats,
			    unsigned int xstats_count, unsigned int *nb_written,
			    unsigned int *nb_supported);

int sfc_sw_xstats_get_names(struct sfc_adapter *sa,
			    struct rte_eth_xstat_name *xstats_names,
			    unsigned int xstats_count, unsigned int *nb_written,
			    unsigned int *nb_supported);

void sfc_sw_xstats_get_vals_by_id(struct sfc_adapter *sa, const uint64_t *ids,
				  uint64_t *values, unsigned int n,
				  unsigned int *nb_supported);

int sfc_sw_xstats_get_names_by_id(struct sfc_adapter *sa, const uint64_t *ids,
				  struct rte_eth_xstat_name *xstats_names,
				  unsigned int size,
				  unsigned int *nb_supported);

unsigned int sfc_sw_xstats_get_nb_supported(struct sfc_adapter *sa);

int sfc_sw_xstats_configure(struct sfc_adapter *sa);

void sfc_sw_xstats_reset(struct sfc_adapter *sa);

int sfc_sw_xstats_init(struct sfc_adapter *sa);

void sfc_sw_xstats_close(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_SW_STATS_H */
