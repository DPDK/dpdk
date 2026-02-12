/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>

#include <eal_export.h>
#include <rte_errno.h>
#include "ethdev_trace.h"
#include "rte_ethdev.h"
#include "rte_mtr_driver.h"
#include "rte_mtr.h"

/* Get generic traffic metering & policing operations structure from a port. */
const struct rte_mtr_ops *
rte_mtr_ops_get(uint16_t port_id, struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct rte_mtr_ops *ops;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_mtr_error_set(error,
			ENODEV,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENODEV));
		return NULL;
	}

	if ((dev->dev_ops->mtr_ops_get == NULL) ||
		(dev->dev_ops->mtr_ops_get(dev, &ops) != 0) ||
		(ops == NULL)) {
		rte_mtr_error_set(error,
			ENOSYS,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			rte_strerror(ENOSYS));
		return NULL;
	}

	return ops;
}

#define RTE_MTR_FUNC(port_id, func)			\
__extension__ ({					\
	const struct rte_mtr_ops *ops =			\
		rte_mtr_ops_get(port_id, error);	\
	if (ops == NULL)				\
		return -rte_errno;			\
							\
	if (ops->func == NULL)				\
		return -rte_mtr_error_set(error,	\
			ENOSYS,				\
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,	\
			NULL,				\
			rte_strerror(ENOSYS));		\
							\
	ops->func;					\
})

#define RTE_MTR_HNDL_FUNC(port_id, func)		\
__extension__ ({					\
	const struct rte_mtr_ops *ops =			\
		rte_mtr_ops_get(port_id, error);	\
	if (ops == NULL)				\
		return NULL;				\
							\
	if (ops->func == NULL) {			\
		rte_mtr_error_set(error,		\
			ENOSYS,				\
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,	\
			NULL,				\
			rte_strerror(ENOSYS));		\
		return NULL;				\
	}						\
							\
	ops->func;					\
})

/* MTR capabilities get */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_capabilities_get, 17.11)
int
rte_mtr_capabilities_get(uint16_t port_id,
	struct rte_mtr_capabilities *cap,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, capabilities_get)(dev,
		cap, error);

	rte_mtr_trace_capabilities_get(port_id, cap, ret);

	return ret;
}

/* MTR meter profile add */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_profile_add, 17.11)
int
rte_mtr_meter_profile_add(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_meter_profile *profile,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_profile_add)(dev,
		meter_profile_id, profile, error);

	rte_mtr_trace_meter_profile_add(port_id, meter_profile_id, profile,
					ret);

	return ret;
}

/** MTR meter profile delete */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_profile_delete, 17.11)
int
rte_mtr_meter_profile_delete(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_profile_delete)(dev,
		meter_profile_id, error);

	rte_mtr_trace_meter_profile_delete(port_id, meter_profile_id, ret);

	return ret;
}

/** MTR meter profile get */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_profile_get, 22.11)
struct rte_flow_meter_profile *
rte_mtr_meter_profile_get(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow_meter_profile *ret;
	ret = RTE_MTR_HNDL_FUNC(port_id, meter_profile_get)(dev,
		meter_profile_id, error);

	rte_mtr_trace_meter_profile_get(port_id, meter_profile_id, ret);

	return ret;
}

/* MTR meter policy validate */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_policy_validate, 21.05)
int
rte_mtr_meter_policy_validate(uint16_t port_id,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret =  RTE_MTR_FUNC(port_id, meter_policy_validate)(dev,
		policy, error);

	rte_mtr_trace_meter_policy_validate(port_id, policy, ret);

	return ret;
}

/* MTR meter policy add */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_policy_add, 21.05)
int
rte_mtr_meter_policy_add(uint16_t port_id,
	uint32_t policy_id,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_policy_add)(dev,
		policy_id, policy, error);

	rte_mtr_trace_meter_policy_add(port_id, policy_id, policy, ret);

	return ret;
}

/** MTR meter policy delete */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_policy_delete, 21.05)
int
rte_mtr_meter_policy_delete(uint16_t port_id,
	uint32_t policy_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_policy_delete)(dev,
		policy_id, error);

	rte_mtr_trace_meter_policy_delete(port_id, policy_id, ret);

	return ret;
}

/** MTR meter policy get */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_policy_get, 22.11)
struct rte_flow_meter_policy *
rte_mtr_meter_policy_get(uint16_t port_id,
	uint32_t policy_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_flow_meter_policy *ret;
	ret = RTE_MTR_HNDL_FUNC(port_id, meter_policy_get)(dev,
		policy_id, error);

	rte_mtr_trace_meter_policy_get(port_id, policy_id, ret);

	return ret;
}

/** MTR object create */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_create, 17.11)
int
rte_mtr_create(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_params *params,
	int shared,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, create)(dev,
		mtr_id, params, shared, error);

	rte_mtr_trace_create(port_id, mtr_id, params, shared, ret);

	return ret;
}

/** MTR object destroy */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_destroy, 17.11)
int
rte_mtr_destroy(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, destroy)(dev,
		mtr_id, error);

	rte_mtr_trace_destroy(port_id, mtr_id, ret);

	return ret;
}

/** MTR object meter enable */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_enable, 17.11)
int
rte_mtr_meter_enable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_enable)(dev,
		mtr_id, error);

	rte_mtr_trace_meter_enable(port_id, mtr_id, ret);

	return ret;
}

/** MTR object meter disable */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_disable, 17.11)
int
rte_mtr_meter_disable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_disable)(dev,
		mtr_id, error);

	rte_mtr_trace_meter_disable(port_id, mtr_id, ret);

	return ret;
}

/** MTR object meter profile update */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_profile_update, 17.11)
int
rte_mtr_meter_profile_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_profile_update)(dev,
		mtr_id, meter_profile_id, error);

	rte_mtr_trace_meter_profile_update(port_id, mtr_id, meter_profile_id, ret);

	return ret;
}

/** MTR object meter policy update */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_policy_update, 21.05)
int
rte_mtr_meter_policy_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t meter_policy_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_policy_update)(dev,
		mtr_id, meter_policy_id, error);

	rte_mtr_trace_meter_policy_update(port_id, mtr_id, meter_policy_id, ret);

	return ret;
}

/** MTR object meter DSCP table update */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_dscp_table_update, 17.11)
int
rte_mtr_meter_dscp_table_update(uint16_t port_id,
	uint32_t mtr_id, enum rte_mtr_color_in_protocol proto,
	enum rte_color *dscp_table,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_dscp_table_update)(dev,
		mtr_id, proto, dscp_table, error);

	rte_mtr_trace_meter_dscp_table_update(port_id, mtr_id, dscp_table, ret);

	return ret;
}

/** MTR object meter VLAN table update */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_meter_vlan_table_update, 22.07)
int
rte_mtr_meter_vlan_table_update(uint16_t port_id,
	uint32_t mtr_id, enum rte_mtr_color_in_protocol proto,
	enum rte_color *vlan_table,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, meter_vlan_table_update)(dev,
		mtr_id, proto, vlan_table, error);

	rte_mtr_trace_meter_vlan_table_update(port_id, mtr_id, vlan_table, ret);

	return ret;
}

/** Set the input color protocol on MTR object */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_color_in_protocol_set, 22.07)
int
rte_mtr_color_in_protocol_set(uint16_t port_id,
	uint32_t mtr_id,
	enum rte_mtr_color_in_protocol proto,
	uint32_t priority,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, in_proto_set)(dev,
		mtr_id, proto, priority, error);

	rte_mtr_trace_color_in_protocol_set(port_id, mtr_id, proto, priority, ret);

	return ret;
}

/** Get input color protocols of MTR object */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_color_in_protocol_get, 22.07)
int
rte_mtr_color_in_protocol_get(uint16_t port_id,
	uint32_t mtr_id,
	uint64_t *proto_mask,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, in_proto_get)(dev,
		mtr_id, proto_mask, error);

	rte_mtr_trace_color_in_protocol_get(port_id, mtr_id, ret);

	return ret;
}

/** Get input color protocol priority of MTR object */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_color_in_protocol_priority_get, 22.07)
int
rte_mtr_color_in_protocol_priority_get(uint16_t port_id,
	uint32_t mtr_id,
	enum rte_mtr_color_in_protocol proto,
	uint32_t *priority,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, in_proto_prio_get)(dev,
		mtr_id, proto, priority, error);

	rte_mtr_trace_color_in_protocol_priority_get(port_id, mtr_id, proto, ret);

	return ret;
}

/** MTR object enabled stats update */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_stats_update, 17.11)
int
rte_mtr_stats_update(uint16_t port_id,
	uint32_t mtr_id,
	uint64_t stats_mask,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, stats_update)(dev,
		mtr_id, stats_mask, error);

	rte_mtr_trace_stats_update(port_id, mtr_id, stats_mask, ret);

	return ret;
}

/** MTR object stats read */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mtr_stats_read, 17.11)
int
rte_mtr_stats_read(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;
	ret = RTE_MTR_FUNC(port_id, stats_read)(dev,
		mtr_id, stats, stats_mask, clear, error);

	rte_mtr_trace_stats_read(port_id, mtr_id, stats, *stats_mask, clear,
				 ret);

	return ret;
}
