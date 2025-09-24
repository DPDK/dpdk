/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <rte_ethdev.h>

#include "include/ntdrv_4ga.h"
#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_fpga.h"
#include "stream_binary_flow_api.h"
#include "ntnic_mod_reg.h"

struct rte_nthw_xstats_names_s {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint8_t source;
	unsigned int offset;
};

/*
 * Extended stat for Capture/Inline - implements RMON
 * FLM 0.17
 */
static struct rte_nthw_xstats_names_s nthw_cap_xstats_names_v1[] = {
	{ "rx_drop_events", 1, offsetof(struct port_counters_v2, drop_events) },
	{ "rx_octets", 1, offsetof(struct port_counters_v2, octets) },
	{ "rx_packets", 1, offsetof(struct port_counters_v2, pkts) },
	{ "rx_broadcast_packets", 1, offsetof(struct port_counters_v2, broadcast_pkts) },
	{ "rx_multicast_packets", 1, offsetof(struct port_counters_v2, multicast_pkts) },
	{ "rx_unicast_packets", 1, offsetof(struct port_counters_v2, unicast_pkts) },
	{ "rx_align_errors", 1, offsetof(struct port_counters_v2, pkts_alignment) },
	{ "rx_code_violation_errors", 1, offsetof(struct port_counters_v2, pkts_code_violation) },
	{ "rx_crc_errors", 1, offsetof(struct port_counters_v2, pkts_crc) },
	{ "rx_undersize_packets", 1, offsetof(struct port_counters_v2, undersize_pkts) },
	{ "rx_oversize_packets", 1, offsetof(struct port_counters_v2, oversize_pkts) },
	{ "rx_fragments", 1, offsetof(struct port_counters_v2, fragments) },
	{
		"rx_jabbers_not_truncated", 1,
		offsetof(struct port_counters_v2, jabbers_not_truncated)
	},
	{ "rx_jabbers_truncated", 1, offsetof(struct port_counters_v2, jabbers_truncated) },
	{ "rx_size_64_packets", 1, offsetof(struct port_counters_v2, pkts_64_octets) },
	{
		"rx_size_65_to_127_packets", 1,
		offsetof(struct port_counters_v2, pkts_65_to_127_octets)
	},
	{
		"rx_size_128_to_255_packets", 1,
		offsetof(struct port_counters_v2, pkts_128_to_255_octets)
	},
	{
		"rx_size_256_to_511_packets", 1,
		offsetof(struct port_counters_v2, pkts_256_to_511_octets)
	},
	{
		"rx_size_512_to_1023_packets", 1,
		offsetof(struct port_counters_v2, pkts_512_to_1023_octets)
	},
	{
		"rx_size_1024_to_1518_packets", 1,
		offsetof(struct port_counters_v2, pkts_1024_to_1518_octets)
	},
	{
		"rx_size_1519_to_2047_packets", 1,
		offsetof(struct port_counters_v2, pkts_1519_to_2047_octets)
	},
	{
		"rx_size_2048_to_4095_packets", 1,
		offsetof(struct port_counters_v2, pkts_2048_to_4095_octets)
	},
	{
		"rx_size_4096_to_8191_packets", 1,
		offsetof(struct port_counters_v2, pkts_4096_to_8191_octets)
	},
	{
		"rx_size_8192_to_max_packets", 1,
		offsetof(struct port_counters_v2, pkts_8192_to_max_octets)
	},
	{ "rx_ip_checksum_error", 1, offsetof(struct port_counters_v2, pkts_ip_chksum_error) },
	{ "rx_udp_checksum_error", 1, offsetof(struct port_counters_v2, pkts_udp_chksum_error) },
	{ "rx_tcp_checksum_error", 1, offsetof(struct port_counters_v2, pkts_tcp_chksum_error) },

	{ "tx_drop_events", 2, offsetof(struct port_counters_v2, drop_events) },
	{ "tx_octets", 2, offsetof(struct port_counters_v2, octets) },
	{ "tx_packets", 2, offsetof(struct port_counters_v2, pkts) },
	{ "tx_broadcast_packets", 2, offsetof(struct port_counters_v2, broadcast_pkts) },
	{ "tx_multicast_packets", 2, offsetof(struct port_counters_v2, multicast_pkts) },
	{ "tx_unicast_packets", 2, offsetof(struct port_counters_v2, unicast_pkts) },
	{ "tx_align_errors", 2, offsetof(struct port_counters_v2, pkts_alignment) },
	{ "tx_code_violation_errors", 2, offsetof(struct port_counters_v2, pkts_code_violation) },
	{ "tx_crc_errors", 2, offsetof(struct port_counters_v2, pkts_crc) },
	{ "tx_undersize_packets", 2, offsetof(struct port_counters_v2, undersize_pkts) },
	{ "tx_oversize_packets", 2, offsetof(struct port_counters_v2, oversize_pkts) },
	{ "tx_fragments", 2, offsetof(struct port_counters_v2, fragments) },
	{
		"tx_jabbers_not_truncated", 2,
		offsetof(struct port_counters_v2, jabbers_not_truncated)
	},
	{ "tx_jabbers_truncated", 2, offsetof(struct port_counters_v2, jabbers_truncated) },
	{ "tx_size_64_packets", 2, offsetof(struct port_counters_v2, pkts_64_octets) },
	{
		"tx_size_65_to_127_packets", 2,
		offsetof(struct port_counters_v2, pkts_65_to_127_octets)
	},
	{
		"tx_size_128_to_255_packets", 2,
		offsetof(struct port_counters_v2, pkts_128_to_255_octets)
	},
	{
		"tx_size_256_to_511_packets", 2,
		offsetof(struct port_counters_v2, pkts_256_to_511_octets)
	},
	{
		"tx_size_512_to_1023_packets", 2,
		offsetof(struct port_counters_v2, pkts_512_to_1023_octets)
	},
	{
		"tx_size_1024_to_1518_packets", 2,
		offsetof(struct port_counters_v2, pkts_1024_to_1518_octets)
	},
	{
		"tx_size_1519_to_2047_packets", 2,
		offsetof(struct port_counters_v2, pkts_1519_to_2047_octets)
	},
	{
		"tx_size_2048_to_4095_packets", 2,
		offsetof(struct port_counters_v2, pkts_2048_to_4095_octets)
	},
	{
		"tx_size_4096_to_8191_packets", 2,
		offsetof(struct port_counters_v2, pkts_4096_to_8191_octets)
	},
	{
		"tx_size_8192_to_max_packets", 2,
		offsetof(struct port_counters_v2, pkts_8192_to_max_octets)
	},

	/* FLM 0.17 */
	{ "flm_count_current", 3, offsetof(struct flm_counters_v1, current) },
	{ "flm_count_learn_done", 3, offsetof(struct flm_counters_v1, learn_done) },
	{ "flm_count_learn_ignore", 3, offsetof(struct flm_counters_v1, learn_ignore) },
	{ "flm_count_learn_fail", 3, offsetof(struct flm_counters_v1, learn_fail) },
	{ "flm_count_unlearn_done", 3, offsetof(struct flm_counters_v1, unlearn_done) },
	{ "flm_count_unlearn_ignore", 3, offsetof(struct flm_counters_v1, unlearn_ignore) },
	{ "flm_count_auto_unlearn_done", 3, offsetof(struct flm_counters_v1, auto_unlearn_done) },
	{
		"flm_count_auto_unlearn_ignore", 3,
		offsetof(struct flm_counters_v1, auto_unlearn_ignore)
	},
	{ "flm_count_auto_unlearn_fail", 3, offsetof(struct flm_counters_v1, auto_unlearn_fail) },
	{
		"flm_count_timeout_unlearn_done", 3,
		offsetof(struct flm_counters_v1, timeout_unlearn_done)
	},
	{ "flm_count_rel_done", 3, offsetof(struct flm_counters_v1, rel_done) },
	{ "flm_count_rel_ignore", 3, offsetof(struct flm_counters_v1, rel_ignore) },
	{ "flm_count_prb_done", 3, offsetof(struct flm_counters_v1, prb_done) },
	{ "flm_count_prb_ignore", 3, offsetof(struct flm_counters_v1, prb_ignore) }
};

/*
 * Extended stat for Capture/Inline - implements RMON
 * FLM 0.18
 */
static struct rte_nthw_xstats_names_s nthw_cap_xstats_names_v2[] = {
	{ "rx_drop_events", 1, offsetof(struct port_counters_v2, drop_events) },
	{ "rx_octets", 1, offsetof(struct port_counters_v2, octets) },
	{ "rx_packets", 1, offsetof(struct port_counters_v2, pkts) },
	{ "rx_broadcast_packets", 1, offsetof(struct port_counters_v2, broadcast_pkts) },
	{ "rx_multicast_packets", 1, offsetof(struct port_counters_v2, multicast_pkts) },
	{ "rx_unicast_packets", 1, offsetof(struct port_counters_v2, unicast_pkts) },
	{ "rx_align_errors", 1, offsetof(struct port_counters_v2, pkts_alignment) },
	{ "rx_code_violation_errors", 1, offsetof(struct port_counters_v2, pkts_code_violation) },
	{ "rx_crc_errors", 1, offsetof(struct port_counters_v2, pkts_crc) },
	{ "rx_undersize_packets", 1, offsetof(struct port_counters_v2, undersize_pkts) },
	{ "rx_oversize_packets", 1, offsetof(struct port_counters_v2, oversize_pkts) },
	{ "rx_fragments", 1, offsetof(struct port_counters_v2, fragments) },
	{
		"rx_jabbers_not_truncated", 1,
		offsetof(struct port_counters_v2, jabbers_not_truncated)
	},
	{ "rx_jabbers_truncated", 1, offsetof(struct port_counters_v2, jabbers_truncated) },
	{ "rx_size_64_packets", 1, offsetof(struct port_counters_v2, pkts_64_octets) },
	{
		"rx_size_65_to_127_packets", 1,
		offsetof(struct port_counters_v2, pkts_65_to_127_octets)
	},
	{
		"rx_size_128_to_255_packets", 1,
		offsetof(struct port_counters_v2, pkts_128_to_255_octets)
	},
	{
		"rx_size_256_to_511_packets", 1,
		offsetof(struct port_counters_v2, pkts_256_to_511_octets)
	},
	{
		"rx_size_512_to_1023_packets", 1,
		offsetof(struct port_counters_v2, pkts_512_to_1023_octets)
	},
	{
		"rx_size_1024_to_1518_packets", 1,
		offsetof(struct port_counters_v2, pkts_1024_to_1518_octets)
	},
	{
		"rx_size_1519_to_2047_packets", 1,
		offsetof(struct port_counters_v2, pkts_1519_to_2047_octets)
	},
	{
		"rx_size_2048_to_4095_packets", 1,
		offsetof(struct port_counters_v2, pkts_2048_to_4095_octets)
	},
	{
		"rx_size_4096_to_8191_packets", 1,
		offsetof(struct port_counters_v2, pkts_4096_to_8191_octets)
	},
	{
		"rx_size_8192_to_max_packets", 1,
		offsetof(struct port_counters_v2, pkts_8192_to_max_octets)
	},
	{ "rx_ip_checksum_error", 1, offsetof(struct port_counters_v2, pkts_ip_chksum_error) },
	{ "rx_udp_checksum_error", 1, offsetof(struct port_counters_v2, pkts_udp_chksum_error) },
	{ "rx_tcp_checksum_error", 1, offsetof(struct port_counters_v2, pkts_tcp_chksum_error) },

	{ "tx_drop_events", 2, offsetof(struct port_counters_v2, drop_events) },
	{ "tx_octets", 2, offsetof(struct port_counters_v2, octets) },
	{ "tx_packets", 2, offsetof(struct port_counters_v2, pkts) },
	{ "tx_broadcast_packets", 2, offsetof(struct port_counters_v2, broadcast_pkts) },
	{ "tx_multicast_packets", 2, offsetof(struct port_counters_v2, multicast_pkts) },
	{ "tx_unicast_packets", 2, offsetof(struct port_counters_v2, unicast_pkts) },
	{ "tx_align_errors", 2, offsetof(struct port_counters_v2, pkts_alignment) },
	{ "tx_code_violation_errors", 2, offsetof(struct port_counters_v2, pkts_code_violation) },
	{ "tx_crc_errors", 2, offsetof(struct port_counters_v2, pkts_crc) },
	{ "tx_undersize_packets", 2, offsetof(struct port_counters_v2, undersize_pkts) },
	{ "tx_oversize_packets", 2, offsetof(struct port_counters_v2, oversize_pkts) },
	{ "tx_fragments", 2, offsetof(struct port_counters_v2, fragments) },
	{
		"tx_jabbers_not_truncated", 2,
		offsetof(struct port_counters_v2, jabbers_not_truncated)
	},
	{ "tx_jabbers_truncated", 2, offsetof(struct port_counters_v2, jabbers_truncated) },
	{ "tx_size_64_packets", 2, offsetof(struct port_counters_v2, pkts_64_octets) },
	{
		"tx_size_65_to_127_packets", 2,
		offsetof(struct port_counters_v2, pkts_65_to_127_octets)
	},
	{
		"tx_size_128_to_255_packets", 2,
		offsetof(struct port_counters_v2, pkts_128_to_255_octets)
	},
	{
		"tx_size_256_to_511_packets", 2,
		offsetof(struct port_counters_v2, pkts_256_to_511_octets)
	},
	{
		"tx_size_512_to_1023_packets", 2,
		offsetof(struct port_counters_v2, pkts_512_to_1023_octets)
	},
	{
		"tx_size_1024_to_1518_packets", 2,
		offsetof(struct port_counters_v2, pkts_1024_to_1518_octets)
	},
	{
		"tx_size_1519_to_2047_packets", 2,
		offsetof(struct port_counters_v2, pkts_1519_to_2047_octets)
	},
	{
		"tx_size_2048_to_4095_packets", 2,
		offsetof(struct port_counters_v2, pkts_2048_to_4095_octets)
	},
	{
		"tx_size_4096_to_8191_packets", 2,
		offsetof(struct port_counters_v2, pkts_4096_to_8191_octets)
	},
	{
		"tx_size_8192_to_max_packets", 2,
		offsetof(struct port_counters_v2, pkts_8192_to_max_octets)
	},

	/* FLM 0.17 */
	{ "flm_count_current", 3, offsetof(struct flm_counters_v1, current) },
	{ "flm_count_learn_done", 3, offsetof(struct flm_counters_v1, learn_done) },
	{ "flm_count_learn_ignore", 3, offsetof(struct flm_counters_v1, learn_ignore) },
	{ "flm_count_learn_fail", 3, offsetof(struct flm_counters_v1, learn_fail) },
	{ "flm_count_unlearn_done", 3, offsetof(struct flm_counters_v1, unlearn_done) },
	{ "flm_count_unlearn_ignore", 3, offsetof(struct flm_counters_v1, unlearn_ignore) },
	{ "flm_count_auto_unlearn_done", 3, offsetof(struct flm_counters_v1, auto_unlearn_done) },
	{
		"flm_count_auto_unlearn_ignore", 3,
		offsetof(struct flm_counters_v1, auto_unlearn_ignore)
	},
	{ "flm_count_auto_unlearn_fail", 3, offsetof(struct flm_counters_v1, auto_unlearn_fail) },
	{
		"flm_count_timeout_unlearn_done", 3,
		offsetof(struct flm_counters_v1, timeout_unlearn_done)
	},
	{ "flm_count_rel_done", 3, offsetof(struct flm_counters_v1, rel_done) },
	{ "flm_count_rel_ignore", 3, offsetof(struct flm_counters_v1, rel_ignore) },
	{ "flm_count_prb_done", 3, offsetof(struct flm_counters_v1, prb_done) },
	{ "flm_count_prb_ignore", 3, offsetof(struct flm_counters_v1, prb_ignore) },

	/* FLM 0.20 */
	{ "flm_count_sta_done", 3, offsetof(struct flm_counters_v1, sta_done) },
	{ "flm_count_inf_done", 3, offsetof(struct flm_counters_v1, inf_done) },
	{ "flm_count_inf_skip", 3, offsetof(struct flm_counters_v1, inf_skip) },
	{ "flm_count_pck_hit", 3, offsetof(struct flm_counters_v1, pck_hit) },
	{ "flm_count_pck_miss", 3, offsetof(struct flm_counters_v1, pck_miss) },
	{ "flm_count_pck_unh", 3, offsetof(struct flm_counters_v1, pck_unh) },
	{ "flm_count_pck_dis", 3, offsetof(struct flm_counters_v1, pck_dis) },
	{ "flm_count_csh_hit", 3, offsetof(struct flm_counters_v1, csh_hit) },
	{ "flm_count_csh_miss", 3, offsetof(struct flm_counters_v1, csh_miss) },
	{ "flm_count_csh_unh", 3, offsetof(struct flm_counters_v1, csh_unh) },
	{ "flm_count_cuc_start", 3, offsetof(struct flm_counters_v1, cuc_start) },
	{ "flm_count_cuc_move", 3, offsetof(struct flm_counters_v1, cuc_move) }
};

/*
 * Extended stat for Capture/Inline - implements RMON
 * STA 0.9
 */

static struct rte_nthw_xstats_names_s nthw_cap_xstats_names_v3[] = {
	{ "rx_drop_events", 1, offsetof(struct port_counters_v2, drop_events) },
	{ "rx_octets", 1, offsetof(struct port_counters_v2, octets) },
	{ "rx_packets", 1, offsetof(struct port_counters_v2, pkts) },
	{ "rx_broadcast_packets", 1, offsetof(struct port_counters_v2, broadcast_pkts) },
	{ "rx_multicast_packets", 1, offsetof(struct port_counters_v2, multicast_pkts) },
	{ "rx_unicast_packets", 1, offsetof(struct port_counters_v2, unicast_pkts) },
	{ "rx_align_errors", 1, offsetof(struct port_counters_v2, pkts_alignment) },
	{ "rx_code_violation_errors", 1, offsetof(struct port_counters_v2, pkts_code_violation) },
	{ "rx_crc_errors", 1, offsetof(struct port_counters_v2, pkts_crc) },
	{ "rx_undersize_packets", 1, offsetof(struct port_counters_v2, undersize_pkts) },
	{ "rx_oversize_packets", 1, offsetof(struct port_counters_v2, oversize_pkts) },
	{ "rx_fragments", 1, offsetof(struct port_counters_v2, fragments) },
	{
		"rx_jabbers_not_truncated", 1,
		offsetof(struct port_counters_v2, jabbers_not_truncated)
	},
	{ "rx_jabbers_truncated", 1, offsetof(struct port_counters_v2, jabbers_truncated) },
	{ "rx_size_64_packets", 1, offsetof(struct port_counters_v2, pkts_64_octets) },
	{
		"rx_size_65_to_127_packets", 1,
		offsetof(struct port_counters_v2, pkts_65_to_127_octets)
	},
	{
		"rx_size_128_to_255_packets", 1,
		offsetof(struct port_counters_v2, pkts_128_to_255_octets)
	},
	{
		"rx_size_256_to_511_packets", 1,
		offsetof(struct port_counters_v2, pkts_256_to_511_octets)
	},
	{
		"rx_size_512_to_1023_packets", 1,
		offsetof(struct port_counters_v2, pkts_512_to_1023_octets)
	},
	{
		"rx_size_1024_to_1518_packets", 1,
		offsetof(struct port_counters_v2, pkts_1024_to_1518_octets)
	},
	{
		"rx_size_1519_to_2047_packets", 1,
		offsetof(struct port_counters_v2, pkts_1519_to_2047_octets)
	},
	{
		"rx_size_2048_to_4095_packets", 1,
		offsetof(struct port_counters_v2, pkts_2048_to_4095_octets)
	},
	{
		"rx_size_4096_to_8191_packets", 1,
		offsetof(struct port_counters_v2, pkts_4096_to_8191_octets)
	},
	{
		"rx_size_8192_to_max_packets", 1,
		offsetof(struct port_counters_v2, pkts_8192_to_max_octets)
	},
	{ "rx_ip_checksum_error", 1, offsetof(struct port_counters_v2, pkts_ip_chksum_error) },
	{ "rx_udp_checksum_error", 1, offsetof(struct port_counters_v2, pkts_udp_chksum_error) },
	{ "rx_tcp_checksum_error", 1, offsetof(struct port_counters_v2, pkts_tcp_chksum_error) },

	{ "tx_drop_events", 2, offsetof(struct port_counters_v2, drop_events) },
	{ "tx_octets", 2, offsetof(struct port_counters_v2, octets) },
	{ "tx_packets", 2, offsetof(struct port_counters_v2, pkts) },
	{ "tx_broadcast_packets", 2, offsetof(struct port_counters_v2, broadcast_pkts) },
	{ "tx_multicast_packets", 2, offsetof(struct port_counters_v2, multicast_pkts) },
	{ "tx_unicast_packets", 2, offsetof(struct port_counters_v2, unicast_pkts) },
	{ "tx_align_errors", 2, offsetof(struct port_counters_v2, pkts_alignment) },
	{ "tx_code_violation_errors", 2, offsetof(struct port_counters_v2, pkts_code_violation) },
	{ "tx_crc_errors", 2, offsetof(struct port_counters_v2, pkts_crc) },
	{ "tx_undersize_packets", 2, offsetof(struct port_counters_v2, undersize_pkts) },
	{ "tx_oversize_packets", 2, offsetof(struct port_counters_v2, oversize_pkts) },
	{ "tx_fragments", 2, offsetof(struct port_counters_v2, fragments) },
	{
		"tx_jabbers_not_truncated", 2,
		offsetof(struct port_counters_v2, jabbers_not_truncated)
	},
	{ "tx_jabbers_truncated", 2, offsetof(struct port_counters_v2, jabbers_truncated) },
	{ "tx_size_64_packets", 2, offsetof(struct port_counters_v2, pkts_64_octets) },
	{
		"tx_size_65_to_127_packets", 2,
		offsetof(struct port_counters_v2, pkts_65_to_127_octets)
	},
	{
		"tx_size_128_to_255_packets", 2,
		offsetof(struct port_counters_v2, pkts_128_to_255_octets)
	},
	{
		"tx_size_256_to_511_packets", 2,
		offsetof(struct port_counters_v2, pkts_256_to_511_octets)
	},
	{
		"tx_size_512_to_1023_packets", 2,
		offsetof(struct port_counters_v2, pkts_512_to_1023_octets)
	},
	{
		"tx_size_1024_to_1518_packets", 2,
		offsetof(struct port_counters_v2, pkts_1024_to_1518_octets)
	},
	{
		"tx_size_1519_to_2047_packets", 2,
		offsetof(struct port_counters_v2, pkts_1519_to_2047_octets)
	},
	{
		"tx_size_2048_to_4095_packets", 2,
		offsetof(struct port_counters_v2, pkts_2048_to_4095_octets)
	},
	{
		"tx_size_4096_to_8191_packets", 2,
		offsetof(struct port_counters_v2, pkts_4096_to_8191_octets)
	},
	{
		"tx_size_8192_to_max_packets", 2,
		offsetof(struct port_counters_v2, pkts_8192_to_max_octets)
	},

	/* FLM 0.17 */
	{ "flm_count_current", 3, offsetof(struct flm_counters_v1, current) },
	{ "flm_count_learn_done", 3, offsetof(struct flm_counters_v1, learn_done) },
	{ "flm_count_learn_ignore", 3, offsetof(struct flm_counters_v1, learn_ignore) },
	{ "flm_count_learn_fail", 3, offsetof(struct flm_counters_v1, learn_fail) },
	{ "flm_count_unlearn_done", 3, offsetof(struct flm_counters_v1, unlearn_done) },
	{ "flm_count_unlearn_ignore", 3, offsetof(struct flm_counters_v1, unlearn_ignore) },
	{ "flm_count_auto_unlearn_done", 3, offsetof(struct flm_counters_v1, auto_unlearn_done) },
	{
		"flm_count_auto_unlearn_ignore", 3,
		offsetof(struct flm_counters_v1, auto_unlearn_ignore)
	},
	{ "flm_count_auto_unlearn_fail", 3, offsetof(struct flm_counters_v1, auto_unlearn_fail) },
	{
		"flm_count_timeout_unlearn_done", 3,
		offsetof(struct flm_counters_v1, timeout_unlearn_done)
	},
	{ "flm_count_rel_done", 3, offsetof(struct flm_counters_v1, rel_done) },
	{ "flm_count_rel_ignore", 3, offsetof(struct flm_counters_v1, rel_ignore) },
	{ "flm_count_prb_done", 3, offsetof(struct flm_counters_v1, prb_done) },
	{ "flm_count_prb_ignore", 3, offsetof(struct flm_counters_v1, prb_ignore) },

	/* FLM 0.20 */
	{ "flm_count_sta_done", 3, offsetof(struct flm_counters_v1, sta_done) },
	{ "flm_count_inf_done", 3, offsetof(struct flm_counters_v1, inf_done) },
	{ "flm_count_inf_skip", 3, offsetof(struct flm_counters_v1, inf_skip) },
	{ "flm_count_pck_hit", 3, offsetof(struct flm_counters_v1, pck_hit) },
	{ "flm_count_pck_miss", 3, offsetof(struct flm_counters_v1, pck_miss) },
	{ "flm_count_pck_unh", 3, offsetof(struct flm_counters_v1, pck_unh) },
	{ "flm_count_pck_dis", 3, offsetof(struct flm_counters_v1, pck_dis) },
	{ "flm_count_csh_hit", 3, offsetof(struct flm_counters_v1, csh_hit) },
	{ "flm_count_csh_miss", 3, offsetof(struct flm_counters_v1, csh_miss) },
	{ "flm_count_csh_unh", 3, offsetof(struct flm_counters_v1, csh_unh) },
	{ "flm_count_cuc_start", 3, offsetof(struct flm_counters_v1, cuc_start) },
	{ "flm_count_cuc_move", 3, offsetof(struct flm_counters_v1, cuc_move) },

	/* FLM 0.17 */
	{ "flm_count_load_lps", 3, offsetof(struct flm_counters_v1, load_lps) },
	{ "flm_count_load_aps", 3, offsetof(struct flm_counters_v1, load_aps) },
	{ "flm_count_max_lps", 3, offsetof(struct flm_counters_v1, max_lps) },
	{ "flm_count_max_aps", 3, offsetof(struct flm_counters_v1, max_aps) },

	{ "rx_packet_per_second", 4, offsetof(struct port_load_counters, rx_pps) },
	{ "rx_max_packet_per_second", 4, offsetof(struct port_load_counters, rx_pps_max) },
	{ "rx_bits_per_second", 4, offsetof(struct port_load_counters, rx_bps) },
	{ "rx_max_bits_per_second", 4, offsetof(struct port_load_counters, rx_bps_max) },
	{ "tx_packet_per_second", 4, offsetof(struct port_load_counters, tx_pps) },
	{ "tx_max_packet_per_second", 4, offsetof(struct port_load_counters, tx_pps_max) },
	{ "tx_bits_per_second", 4, offsetof(struct port_load_counters, tx_bps) },
	{ "tx_max_bits_per_second", 4, offsetof(struct port_load_counters, tx_bps_max) }
};

#define NTHW_CAP_XSTATS_NAMES_V1 RTE_DIM(nthw_cap_xstats_names_v1)
#define NTHW_CAP_XSTATS_NAMES_V2 RTE_DIM(nthw_cap_xstats_names_v2)
#define NTHW_CAP_XSTATS_NAMES_V3 RTE_DIM(nthw_cap_xstats_names_v3)

/*
 * Container for the reset values
 */
#define NTHW_XSTATS_SIZE NTHW_CAP_XSTATS_NAMES_V3

static uint64_t nthw_xstats_reset_val[NUM_ADAPTER_PORTS_MAX][NTHW_XSTATS_SIZE] = { 0 };

/*
 * These functions must only be called with stat mutex locked
 */
static int nthw_xstats_get(nt4ga_stat_t *p_nt4ga_stat,
	struct rte_eth_xstat *stats,
	unsigned int n,
	uint8_t port)
{
	unsigned int i;
	uint8_t *pld_ptr;
	uint8_t *flm_ptr;
	uint8_t *rx_ptr;
	uint8_t *tx_ptr;
	uint32_t nb_names;
	struct rte_nthw_xstats_names_s *names;

	pld_ptr = (uint8_t *)&p_nt4ga_stat->mp_port_load[port];
	flm_ptr = (uint8_t *)p_nt4ga_stat->mp_stat_structs_flm;
	rx_ptr = (uint8_t *)&p_nt4ga_stat->cap.mp_stat_structs_port_rx[port];
	tx_ptr = (uint8_t *)&p_nt4ga_stat->cap.mp_stat_structs_port_tx[port];

	if (p_nt4ga_stat->flm_stat_ver < 18) {
		names = nthw_cap_xstats_names_v1;
		nb_names = NTHW_CAP_XSTATS_NAMES_V1;

	} else if (p_nt4ga_stat->mp_nthw_stat->mn_stat_layout_version < 7 ||
		p_nt4ga_stat->flm_stat_ver < 23) {
		names = nthw_cap_xstats_names_v2;
		nb_names = NTHW_CAP_XSTATS_NAMES_V2;

	} else {
		names = nthw_cap_xstats_names_v3;
		nb_names = NTHW_CAP_XSTATS_NAMES_V3;
	}

	for (i = 0; i < n && i < nb_names; i++) {
		stats[i].id = i;

		switch (names[i].source) {
		case 1:
			/* RX stat */
			stats[i].value = *((uint64_t *)&rx_ptr[names[i].offset]) -
				nthw_xstats_reset_val[port][i];
			break;

		case 2:
			/* TX stat */
			stats[i].value = *((uint64_t *)&tx_ptr[names[i].offset]) -
				nthw_xstats_reset_val[port][i];
			break;

		case 3:

			/* FLM stat */
			if (flm_ptr) {
				stats[i].value = *((uint64_t *)&flm_ptr[names[i].offset]) -
					nthw_xstats_reset_val[0][i];

			} else {
				stats[i].value = 0;
			}

			break;

		case 4:

			/* Port Load stat */
			if (pld_ptr) {
				/* No reset */
				stats[i].value = *((uint64_t *)&pld_ptr[names[i].offset]);

			} else {
				stats[i].value = 0;
			}

			break;

		default:
			stats[i].value = 0;
			break;
		}
	}

	return i;
}

static int nthw_xstats_get_by_id(nt4ga_stat_t *p_nt4ga_stat,
	const uint64_t *ids,
	uint64_t *values,
	unsigned int n,
	uint8_t port)
{
	unsigned int i;
	uint8_t *pld_ptr;
	uint8_t *flm_ptr;
	uint8_t *rx_ptr;
	uint8_t *tx_ptr;
	uint32_t nb_names;
	struct rte_nthw_xstats_names_s *names;
	int count = 0;

	pld_ptr = (uint8_t *)&p_nt4ga_stat->mp_port_load[port];
	flm_ptr = (uint8_t *)p_nt4ga_stat->mp_stat_structs_flm;
	rx_ptr = (uint8_t *)&p_nt4ga_stat->cap.mp_stat_structs_port_rx[port];
	tx_ptr = (uint8_t *)&p_nt4ga_stat->cap.mp_stat_structs_port_tx[port];

	if (p_nt4ga_stat->flm_stat_ver < 18) {
		names = nthw_cap_xstats_names_v1;
		nb_names = NTHW_CAP_XSTATS_NAMES_V1;

	} else if (p_nt4ga_stat->mp_nthw_stat->mn_stat_layout_version < 7 ||
		p_nt4ga_stat->flm_stat_ver < 23) {
		names = nthw_cap_xstats_names_v2;
		nb_names = NTHW_CAP_XSTATS_NAMES_V2;

	} else {
		names = nthw_cap_xstats_names_v3;
		nb_names = NTHW_CAP_XSTATS_NAMES_V3;
	}

	for (i = 0; i < n; i++) {
		if (ids[i] < nb_names) {
			switch (names[ids[i]].source) {
			case 1:
				/* RX stat */
				values[i] = *((uint64_t *)&rx_ptr[names[ids[i]].offset]) -
					nthw_xstats_reset_val[port][ids[i]];
				break;

			case 2:
				/* TX stat */
				values[i] = *((uint64_t *)&tx_ptr[names[ids[i]].offset]) -
					nthw_xstats_reset_val[port][ids[i]];
				break;

			case 3:

				/* FLM stat */
				if (flm_ptr) {
					values[i] = *((uint64_t *)&flm_ptr[names[ids[i]].offset]) -
						nthw_xstats_reset_val[0][ids[i]];

				} else {
					values[i] = 0;
				}

				break;

			case 4:
				/* Port Load stat */
				values[i] = *((uint64_t *)&pld_ptr[names[i].offset]);
				break;

			default:
				values[i] = 0;
				break;
			}

			count++;
		}
	}

	return count;
}

static void nthw_xstats_reset(nt4ga_stat_t *p_nt4ga_stat, uint8_t port)
{
	unsigned int i;
	uint8_t *flm_ptr;
	uint8_t *rx_ptr;
	uint8_t *tx_ptr;
	uint32_t nb_names;
	struct rte_nthw_xstats_names_s *names;

	flm_ptr = (uint8_t *)p_nt4ga_stat->mp_stat_structs_flm;
	rx_ptr = (uint8_t *)&p_nt4ga_stat->cap.mp_stat_structs_port_rx[port];
	tx_ptr = (uint8_t *)&p_nt4ga_stat->cap.mp_stat_structs_port_tx[port];

	if (p_nt4ga_stat->flm_stat_ver < 18) {
		names = nthw_cap_xstats_names_v1;
		nb_names = NTHW_CAP_XSTATS_NAMES_V1;

	} else if (p_nt4ga_stat->mp_nthw_stat->mn_stat_layout_version < 7 ||
		p_nt4ga_stat->flm_stat_ver < 23) {
		names = nthw_cap_xstats_names_v2;
		nb_names = NTHW_CAP_XSTATS_NAMES_V2;

	} else {
		names = nthw_cap_xstats_names_v3;
		nb_names = NTHW_CAP_XSTATS_NAMES_V3;
	}

	for (i = 0; i < nb_names; i++) {
		switch (names[i].source) {
		case 1:
			/* RX stat */
			nthw_xstats_reset_val[port][i] = *((uint64_t *)&rx_ptr[names[i].offset]);
			break;

		case 2:
			/* TX stat */
			nthw_xstats_reset_val[port][i] = *((uint64_t *)&tx_ptr[names[i].offset]);
			break;

		case 3:

			/* FLM stat */
			/* Reset makes no sense for flm_count_current */
			/* Reset can't be used for load_lps, load_aps, max_lps and max_aps */
			if (flm_ptr &&
				(strcmp(names[i].name, "flm_count_current") != 0 &&
					strcmp(names[i].name, "flm_count_load_lps") != 0 &&
					strcmp(names[i].name, "flm_count_load_aps") != 0 &&
					strcmp(names[i].name, "flm_count_max_lps") != 0 &&
					strcmp(names[i].name, "flm_count_max_aps") != 0)) {
				nthw_xstats_reset_val[0][i] =
					*((uint64_t *)&flm_ptr[names[i].offset]);
			}

			break;

		case 4:
			/* Port load stat*/
			/* No reset */
			break;

		default:
			break;
		}
	}
}

/*
 * These functions does not require stat mutex locked
 */
static int nthw_xstats_get_names(nt4ga_stat_t *p_nt4ga_stat,
	struct rte_eth_xstat_name *xstats_names,
	unsigned int size)
{
	int count = 0;
	unsigned int i;
	uint32_t nb_names;
	struct rte_nthw_xstats_names_s *names;

	if (p_nt4ga_stat->flm_stat_ver < 18) {
		names = nthw_cap_xstats_names_v1;
		nb_names = NTHW_CAP_XSTATS_NAMES_V1;

	} else if (p_nt4ga_stat->mp_nthw_stat->mn_stat_layout_version < 7 ||
		p_nt4ga_stat->flm_stat_ver < 23) {
		names = nthw_cap_xstats_names_v2;
		nb_names = NTHW_CAP_XSTATS_NAMES_V2;

	} else {
		names = nthw_cap_xstats_names_v3;
		nb_names = NTHW_CAP_XSTATS_NAMES_V3;
	}

	if (!xstats_names)
		return nb_names;

	for (i = 0; i < size && i < nb_names; i++) {
		size_t written =
			strlcpy(xstats_names[i].name, names[i].name, sizeof(xstats_names[i].name));
		if (written >= sizeof(xstats_names[i].name))
			NT_LOG(WRN, NTNIC, "xstats name %s truncated", names[i].name);
		count++;
	}

	return count;
}

static int nthw_xstats_get_names_by_id(nt4ga_stat_t *p_nt4ga_stat,
	struct rte_eth_xstat_name *xstats_names,
	const uint64_t *ids,
	unsigned int size)
{
	int count = 0;
	unsigned int i;

	uint32_t nb_names;
	struct rte_nthw_xstats_names_s *names;

	if (p_nt4ga_stat->flm_stat_ver < 18) {
		names = nthw_cap_xstats_names_v1;
		nb_names = NTHW_CAP_XSTATS_NAMES_V1;

	} else if (p_nt4ga_stat->mp_nthw_stat->mn_stat_layout_version < 7 ||
		p_nt4ga_stat->flm_stat_ver < 23) {
		names = nthw_cap_xstats_names_v2;
		nb_names = NTHW_CAP_XSTATS_NAMES_V2;

	} else {
		names = nthw_cap_xstats_names_v3;
		nb_names = NTHW_CAP_XSTATS_NAMES_V3;
	}

	if (!xstats_names)
		return nb_names;

	for (i = 0; i < size; i++) {
		if (ids[i] < nb_names) {
			size_t written = strlcpy(xstats_names[i].name,
				names[ids[i]].name,
				RTE_ETH_XSTATS_NAME_SIZE);
			if (written >= RTE_ETH_XSTATS_NAME_SIZE)
				NT_LOG(WRN, NTNIC, "xstats name %s truncated", names[ids[i]].name);
		}

		count++;
	}

	return count;
}

static struct ntnic_xstats_ops ops = {
	.nthw_xstats_get_names = nthw_xstats_get_names,
	.nthw_xstats_get = nthw_xstats_get,
	.nthw_xstats_reset = nthw_xstats_reset,
	.nthw_xstats_get_names_by_id = nthw_xstats_get_names_by_id,
	.nthw_xstats_get_by_id = nthw_xstats_get_by_id
};

void nthw_xstats_ops_init(void)
{
	NT_LOG_DBGX(DBG, NTNIC, "xstats module was initialized");
	nthw_reg_xstats_ops(&ops);
}
