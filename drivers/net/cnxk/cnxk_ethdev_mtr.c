/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"
#include <rte_mtr_driver.h>

#define NIX_MTR_COUNT_MAX      73 /* 64(leaf) + 8(mid) + 1(top) */
#define NIX_MTR_COUNT_PER_FLOW 3  /* 1(leaf) + 1(mid) + 1(top) */

static struct rte_mtr_capabilities mtr_capa = {
	.n_max = NIX_MTR_COUNT_MAX,
	.n_shared_max = NIX_MTR_COUNT_PER_FLOW,
	/* .identical = , */
	.shared_identical = true,
	/* .shared_n_flows_per_mtr_max = ,*/
	.chaining_n_mtrs_per_flow_max = NIX_MTR_COUNT_PER_FLOW,
	.chaining_use_prev_mtr_color_supported = true,
	.chaining_use_prev_mtr_color_enforced = true,
	.meter_srtcm_rfc2697_n_max = NIX_MTR_COUNT_MAX,
	.meter_trtcm_rfc2698_n_max = NIX_MTR_COUNT_MAX,
	.meter_trtcm_rfc4115_n_max = NIX_MTR_COUNT_MAX,
	.meter_rate_max = NIX_BPF_RATE_MAX / 8, /* Bytes per second */
	.meter_policy_n_max = NIX_MTR_COUNT_MAX,
	.color_aware_srtcm_rfc2697_supported = true,
	.color_aware_trtcm_rfc2698_supported = true,
	.color_aware_trtcm_rfc4115_supported = true,
	.srtcm_rfc2697_byte_mode_supported = true,
	.srtcm_rfc2697_packet_mode_supported = true,
	.trtcm_rfc2698_byte_mode_supported = true,
	.trtcm_rfc2698_packet_mode_supported = true,
	.trtcm_rfc4115_byte_mode_supported = true,
	.trtcm_rfc4115_packet_mode_supported = true,
	.stats_mask = RTE_MTR_STATS_N_PKTS_GREEN | RTE_MTR_STATS_N_PKTS_YELLOW |
		      RTE_MTR_STATS_N_PKTS_RED | RTE_MTR_STATS_N_PKTS_DROPPED |
		      RTE_MTR_STATS_N_BYTES_GREEN |
		      RTE_MTR_STATS_N_BYTES_YELLOW | RTE_MTR_STATS_N_BYTES_RED |
		      RTE_MTR_STATS_N_BYTES_DROPPED};

static int
cnxk_nix_mtr_capabilities_get(struct rte_eth_dev *dev,
			      struct rte_mtr_capabilities *capa,
			      struct rte_mtr_error *error)
{
	RTE_SET_USED(dev);

	if (!capa)
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					  "NULL input parameter");
	*capa = mtr_capa;
	return 0;
}

const struct rte_mtr_ops nix_mtr_ops = {
	.capabilities_get = cnxk_nix_mtr_capabilities_get,
};

int
cnxk_nix_mtr_ops_get(struct rte_eth_dev *dev, void *ops)
{
	RTE_SET_USED(dev);

	*(const void **)ops = &nix_mtr_ops;
	return 0;
}
