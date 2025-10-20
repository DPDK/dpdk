/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>

#include <rte_common.h>
#include <rte_meter.h>
#include <rte_mtr.h>
#include <rte_mtr_driver.h>
#include <rte_malloc.h>

#include "ntos_drv.h"
#include "ntlog.h"
#include "nt_util.h"
#include "ntos_system.h"
#include "ntnic_mod_reg.h"

static inline uint8_t get_caller_id(uint16_t port)
{
	return MAX_VDPA_PORTS + (uint8_t)(port & 0x7f) + 1;
}

struct qos_integer_fractional {
	uint32_t integer;
	uint32_t fractional;	/* 1/1024 */
};

/*
 * Inline FLM metering
 */

static int eth_mtr_capabilities_get_inline(struct rte_eth_dev *eth_dev,
	struct rte_mtr_capabilities *cap,
	struct rte_mtr_error *error)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	uint8_t caller_id = get_caller_id(eth_dev->data->port_id);

	if (!profile_inline_ops->flow_mtr_supported(internals->flw_dev)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Ethernet device does not support metering");
	}

	if (cap == NULL)
		return -1;

	memset(cap, 0x0, sizeof(struct rte_mtr_capabilities));

	/* MBR records use 28-bit integers */
	cap->n_max = profile_inline_ops->flow_mtr_meters_supported(internals->flw_dev,
		caller_id);
	cap->n_shared_max = cap->n_max;

	cap->identical = 0;
	cap->shared_identical = 0;

	cap->shared_n_flows_per_mtr_max = UINT32_MAX;

	/* Limited by number of MBR record ids per FLM learn record */
	cap->chaining_n_mtrs_per_flow_max = 4;

	cap->chaining_use_prev_mtr_color_supported = 0;
	cap->chaining_use_prev_mtr_color_enforced = 0;

	cap->meter_rate_max = (uint64_t)(0xfff << 0xf) * 1099;

	cap->stats_mask = RTE_MTR_STATS_N_PKTS_GREEN | RTE_MTR_STATS_N_BYTES_GREEN;

	/* Only color-blind mode is supported */
	cap->color_aware_srtcm_rfc2697_supported = 0;
	cap->color_aware_trtcm_rfc2698_supported = 0;
	cap->color_aware_trtcm_rfc4115_supported = 0;

	/* Focused on RFC2698 for now */
	cap->meter_srtcm_rfc2697_n_max = 0;
	cap->meter_trtcm_rfc2698_n_max = cap->n_max;
	cap->meter_trtcm_rfc4115_n_max = 0;

	cap->meter_policy_n_max = profile_inline_ops->flow_mtr_meter_policy_n_max();

	/* Byte mode is supported */
	cap->srtcm_rfc2697_byte_mode_supported = 0;
	cap->trtcm_rfc2698_byte_mode_supported = 1;
	cap->trtcm_rfc4115_byte_mode_supported = 0;

	/* Packet mode not supported */
	cap->srtcm_rfc2697_packet_mode_supported = 0;
	cap->trtcm_rfc2698_packet_mode_supported = 0;
	cap->trtcm_rfc4115_packet_mode_supported = 0;

	return 0;
}

static int eth_mtr_meter_profile_add_inline(struct rte_eth_dev *eth_dev,
	uint32_t meter_profile_id,
	struct rte_mtr_meter_profile *profile,
	struct rte_mtr_error *error __rte_unused)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	if (meter_profile_id >= profile_inline_ops->flow_mtr_meter_policy_n_max())
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
				"Profile id out of range");

	if (profile->packet_mode != 0) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_PACKET_MODE, NULL,
				"Profile packet mode not supported");
	}

	if (profile->alg == RTE_MTR_SRTCM_RFC2697) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"RFC 2697 not supported");
	}

	if (profile->alg == RTE_MTR_TRTCM_RFC4115) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"RFC 4115 not supported");
	}

	if (profile->trtcm_rfc2698.cir != profile->trtcm_rfc2698.pir ||
		profile->trtcm_rfc2698.cbs != profile->trtcm_rfc2698.pbs) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"Profile committed and peak rates must be equal");
	}

	int res = profile_inline_ops->flow_mtr_set_profile(internals->flw_dev, meter_profile_id,
			profile->trtcm_rfc2698.cir,
			profile->trtcm_rfc2698.cbs, 0, 0);

	if (res) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
				"Profile could not be added.");
	}

	return 0;
}

static int eth_mtr_meter_profile_delete_inline(struct rte_eth_dev *eth_dev,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error __rte_unused)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	if (meter_profile_id >= profile_inline_ops->flow_mtr_meter_policy_n_max())
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
				"Profile id out of range");

	profile_inline_ops->flow_mtr_set_profile(internals->flw_dev, meter_profile_id, 0, 0, 0, 0);

	return 0;
}

static int eth_mtr_meter_policy_add_inline(struct rte_eth_dev *eth_dev,
	uint32_t policy_id,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	if (policy_id >= profile_inline_ops->flow_mtr_meter_policy_n_max())
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
				"Policy id out of range");

	const struct rte_flow_action *actions = policy->actions[RTE_COLOR_GREEN];
	int green_action_supported = (actions[0].type == RTE_FLOW_ACTION_TYPE_END) ||
		(actions[0].type == RTE_FLOW_ACTION_TYPE_VOID &&
			actions[1].type == RTE_FLOW_ACTION_TYPE_END) ||
		(actions[0].type == RTE_FLOW_ACTION_TYPE_PASSTHRU &&
			actions[1].type == RTE_FLOW_ACTION_TYPE_END);

	actions = policy->actions[RTE_COLOR_YELLOW];
	int yellow_action_supported = actions[0].type == RTE_FLOW_ACTION_TYPE_DROP &&
		actions[1].type == RTE_FLOW_ACTION_TYPE_END;

	actions = policy->actions[RTE_COLOR_RED];
	int red_action_supported = actions[0].type == RTE_FLOW_ACTION_TYPE_DROP &&
		actions[1].type == RTE_FLOW_ACTION_TYPE_END;

	if (green_action_supported == 0 || yellow_action_supported == 0 ||
		red_action_supported == 0) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
				"Unsupported meter policy actions");
	}

	if (profile_inline_ops->flow_mtr_set_policy(internals->flw_dev, policy_id, 1)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
				"Policy could not be added");
	}

	return 0;
}

static int eth_mtr_meter_policy_delete_inline(struct rte_eth_dev *eth_dev __rte_unused,
	uint32_t policy_id,
	struct rte_mtr_error *error __rte_unused)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	if (policy_id >= profile_inline_ops->flow_mtr_meter_policy_n_max())
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
				"Policy id out of range");

	return 0;
}

static int eth_mtr_create_inline(struct rte_eth_dev *eth_dev,
	uint32_t mtr_id,
	struct rte_mtr_params *params,
	int shared,
	struct rte_mtr_error *error)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	uint8_t caller_id = get_caller_id(eth_dev->data->port_id);

	if (params->use_prev_mtr_color != 0 || params->dscp_table != NULL) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"Only color blind mode is supported");
	}

	uint64_t allowed_stats_mask = RTE_MTR_STATS_N_PKTS_GREEN | RTE_MTR_STATS_N_BYTES_GREEN;

	if ((params->stats_mask & ~allowed_stats_mask) != 0) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"Requested color stats not supported");
	}

	if (params->meter_enable == 0) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"Disabled meters not supported");
	}

	if (shared == 0) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"Only shared mtrs are supported");
	}

	if (params->meter_profile_id >= profile_inline_ops->flow_mtr_meter_policy_n_max())
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
				"Profile id out of range");

	if (params->meter_policy_id >= profile_inline_ops->flow_mtr_meter_policy_n_max())
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_POLICY_ID, NULL,
				"Policy id out of range");

	if (mtr_id >=
		profile_inline_ops->flow_mtr_meters_supported(internals->flw_dev, caller_id)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"MTR id is out of range");
	}

	int res = profile_inline_ops->flow_mtr_create_meter(internals->flw_dev,
			caller_id,
			mtr_id,
			params->meter_profile_id,
			params->meter_policy_id,
			params->stats_mask);

	if (res) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to offload to hardware");
	}

	return 0;
}

static int eth_mtr_destroy_inline(struct rte_eth_dev *eth_dev,
	uint32_t mtr_id,
	struct rte_mtr_error *error __rte_unused)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	uint8_t caller_id = get_caller_id(eth_dev->data->port_id);

	if (mtr_id >=
		profile_inline_ops->flow_mtr_meters_supported(internals->flw_dev, caller_id)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"MTR id is out of range");
	}

	if (profile_inline_ops->flow_mtr_destroy_meter(internals->flw_dev, caller_id, mtr_id)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to offload to hardware");
	}

	return 0;
}

static int eth_mtr_stats_adjust_inline(struct rte_eth_dev *eth_dev,
	uint32_t mtr_id,
	uint64_t adjust_value,
	struct rte_mtr_error *error)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	const uint64_t adjust_bit = 1ULL << 63;
	const uint64_t probe_bit = 1ULL << 62;
	struct pmd_internals *internals = eth_dev->data->dev_private;
	uint8_t caller_id = get_caller_id(eth_dev->data->port_id);

	if (mtr_id >=
		profile_inline_ops->flow_mtr_meters_supported(internals->flw_dev, caller_id)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"MTR id is out of range");
	}

	if (adjust_value & adjust_bit) {
		adjust_value &= adjust_bit - 1;

		if (adjust_value > (uint64_t)UINT32_MAX) {
			return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					NULL, "Adjust value is out of range");
		}

		if (profile_inline_ops->flm_mtr_adjust_stats(internals->flw_dev, caller_id, mtr_id,
				(uint32_t)adjust_value)) {
			return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					NULL, "Failed to adjust offloaded MTR");
		}

		return 0;
	}

	if (adjust_value & probe_bit) {
		if (mtr_id >=
			profile_inline_ops->flow_mtr_meters_supported(internals->flw_dev,
				caller_id)) {
			return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					NULL, "MTR id is out of range");
		}

		if (profile_inline_ops->flow_mtr_probe_meter(internals->flw_dev, caller_id,
				mtr_id)) {
			return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					NULL, "Failed to offload to hardware");
		}

		return 0;
	}

	return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
			"Use of meter stats update requires bit 63 or bit 62 of \"stats_mask\" must be 1.");
}

static int eth_mtr_stats_read_inline(struct rte_eth_dev *eth_dev,
	uint32_t mtr_id,
	struct rte_mtr_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_mtr_error *error)
{
	const struct profile_inline_ops *profile_inline_ops = get_profile_inline_ops();

	if (profile_inline_ops == NULL) {
		NT_LOG(ERR, NTHW, "profile_inline module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = eth_dev->data->dev_private;

	uint8_t caller_id = get_caller_id(eth_dev->data->port_id);

	if (mtr_id >=
		profile_inline_ops->flow_mtr_meters_supported(internals->flw_dev, caller_id)) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
				"MTR id is out of range");
	}

	if (stats == NULL)
		return -1;

	memset(stats, 0x0, sizeof(struct rte_mtr_stats));
	profile_inline_ops->flm_mtr_read_stats(internals->flw_dev, caller_id, mtr_id, stats_mask,
		&stats->n_pkts[RTE_COLOR_GREEN],
		&stats->n_bytes[RTE_COLOR_GREEN], clear);

	return 0;
}

/*
 * Ops setup
 */

static const struct rte_mtr_ops mtr_ops_inline = {
	.capabilities_get = eth_mtr_capabilities_get_inline,
	.meter_profile_add = eth_mtr_meter_profile_add_inline,
	.meter_profile_delete = eth_mtr_meter_profile_delete_inline,
	.create = eth_mtr_create_inline,
	.destroy = eth_mtr_destroy_inline,
	.meter_policy_add = eth_mtr_meter_policy_add_inline,
	.meter_policy_delete = eth_mtr_meter_policy_delete_inline,
	.stats_update = eth_mtr_stats_adjust_inline,
	.stats_read = eth_mtr_stats_read_inline,
};

static int eth_mtr_ops_get(struct rte_eth_dev *eth_dev, void *ops)
{
	struct pmd_internals *internals = eth_dev->data->dev_private;
	ntdrv_4ga_t *p_nt_drv = &internals->p_drv->ntdrv;
	enum fpga_info_profile profile = p_nt_drv->adapter_info.fpga_info.profile;

	switch (profile) {
	case FPGA_INFO_PROFILE_INLINE:
		*(const struct rte_mtr_ops **)ops = &mtr_ops_inline;
		break;

	case FPGA_INFO_PROFILE_UNKNOWN:

	/* fallthrough */
	case FPGA_INFO_PROFILE_CAPTURE:

	/* fallthrough */
	default:
		NT_LOG(ERR, NTHW, "" PCIIDENT_PRINT_STR ": fpga profile not supported",
			PCIIDENT_TO_DOMAIN(p_nt_drv->pciident),
			PCIIDENT_TO_BUSNR(p_nt_drv->pciident),
			PCIIDENT_TO_DEVNR(p_nt_drv->pciident),
			PCIIDENT_TO_FUNCNR(p_nt_drv->pciident));
		return -1;
	}

	return 0;
}

static struct meter_ops_s meter_ops = {
	.eth_mtr_ops_get = eth_mtr_ops_get,
};

void nthw_meter_init(void)
{
	NT_LOG(DBG, NTNIC, "Meter ops initialized");
	nthw_reg_meter_ops(&meter_ops);
}
