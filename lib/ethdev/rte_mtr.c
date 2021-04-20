/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdint.h>

#include <rte_errno.h>
#include "rte_compat.h"
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
({							\
	const struct rte_mtr_ops *ops =			\
		rte_mtr_ops_get(port_id, error);		\
	if (ops == NULL)					\
		return -rte_errno;			\
							\
	if (ops->func == NULL)				\
		return -rte_mtr_error_set(error,		\
			ENOSYS,				\
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,	\
			NULL,				\
			rte_strerror(ENOSYS));		\
							\
	ops->func;					\
})

/* MTR capabilities get */
int
rte_mtr_capabilities_get(uint16_t port_id,
	struct rte_mtr_capabilities *cap,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, capabilities_get)(dev,
		cap, error);
}

/* MTR meter profile add */
int
rte_mtr_meter_profile_add(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_meter_profile *profile,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_profile_add)(dev,
		meter_profile_id, profile, error);
}

/** MTR meter profile delete */
int
rte_mtr_meter_profile_delete(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_profile_delete)(dev,
		meter_profile_id, error);
}

/* MTR meter policy validate */
int
rte_mtr_meter_policy_validate(uint16_t port_id,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_policy_validate)(dev,
		policy, error);
}

/* MTR meter policy add */
int
rte_mtr_meter_policy_add(uint16_t port_id,
	uint32_t policy_id,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_policy_add)(dev,
		policy_id, policy, error);
}

/** MTR meter policy delete */
int
rte_mtr_meter_policy_delete(uint16_t port_id,
	uint32_t policy_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_policy_delete)(dev,
		policy_id, error);
}

/** MTR object create */
int
rte_mtr_create(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_params *params,
	int shared,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, create)(dev,
		mtr_id, params, shared, error);
}

/** MTR object destroy */
int
rte_mtr_destroy(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, destroy)(dev,
		mtr_id, error);
}

/** MTR object meter enable */
int
rte_mtr_meter_enable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_enable)(dev,
		mtr_id, error);
}

/** MTR object meter disable */
int
rte_mtr_meter_disable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_disable)(dev,
		mtr_id, error);
}

/** MTR object meter profile update */
int
rte_mtr_meter_profile_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_profile_update)(dev,
		mtr_id, meter_profile_id, error);
}

/** MTR object meter policy update */
int
rte_mtr_meter_policy_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t meter_policy_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_policy_update)(dev,
		mtr_id, meter_policy_id, error);
}

/** MTR object meter DSCP table update */
int
rte_mtr_meter_dscp_table_update(uint16_t port_id,
	uint32_t mtr_id,
	enum rte_color *dscp_table,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_dscp_table_update)(dev,
		mtr_id, dscp_table, error);
}

/** MTR object enabled stats update */
int
rte_mtr_stats_update(uint16_t port_id,
	uint32_t mtr_id,
	uint64_t stats_mask,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, stats_update)(dev,
		mtr_id, stats_mask, error);
}

/** MTR object stats read */
int
rte_mtr_stats_read(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, stats_read)(dev,
		mtr_id, stats, stats_mask, clear, error);
}
