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
int __rte_experimental
rte_mtr_capabilities_get(uint16_t port_id,
	struct rte_mtr_capabilities *cap,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, capabilities_get)(dev,
		cap, error);
}

/* MTR meter profile add */
int __rte_experimental
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
int __rte_experimental
rte_mtr_meter_profile_delete(uint16_t port_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_profile_delete)(dev,
		meter_profile_id, error);
}

/** MTR object create */
int __rte_experimental
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
int __rte_experimental
rte_mtr_destroy(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, destroy)(dev,
		mtr_id, error);
}

/** MTR object meter enable */
int __rte_experimental
rte_mtr_meter_enable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_enable)(dev,
		mtr_id, error);
}

/** MTR object meter disable */
int __rte_experimental
rte_mtr_meter_disable(uint16_t port_id,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_disable)(dev,
		mtr_id, error);
}

/** MTR object meter profile update */
int __rte_experimental
rte_mtr_meter_profile_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_profile_update)(dev,
		mtr_id, meter_profile_id, error);
}

/** MTR object meter DSCP table update */
int __rte_experimental
rte_mtr_meter_dscp_table_update(uint16_t port_id,
	uint32_t mtr_id,
	enum rte_color *dscp_table,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, meter_dscp_table_update)(dev,
		mtr_id, dscp_table, error);
}

/** MTR object policer action update */
int __rte_experimental
rte_mtr_policer_actions_update(uint16_t port_id,
	uint32_t mtr_id,
	uint32_t action_mask,
	enum rte_mtr_policer_action *actions,
	struct rte_mtr_error *error)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	return RTE_MTR_FUNC(port_id, policer_actions_update)(dev,
		mtr_id, action_mask, actions, error);
}

/** MTR object enabled stats update */
int __rte_experimental
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
int __rte_experimental
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
