/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_CRYPTO_SCHEDULER_OPERATIONS_H
#define _RTE_CRYPTO_SCHEDULER_OPERATIONS_H

#include <rte_cryptodev.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*rte_cryptodev_scheduler_slave_attach_t)(
		struct rte_cryptodev *dev, uint8_t slave_id);
typedef int (*rte_cryptodev_scheduler_slave_detach_t)(
		struct rte_cryptodev *dev, uint8_t slave_id);

typedef int (*rte_cryptodev_scheduler_start_t)(struct rte_cryptodev *dev);
typedef int (*rte_cryptodev_scheduler_stop_t)(struct rte_cryptodev *dev);

typedef int (*rte_cryptodev_scheduler_config_queue_pair)(
		struct rte_cryptodev *dev, uint16_t qp_id);

typedef int (*rte_cryptodev_scheduler_create_private_ctx)(
		struct rte_cryptodev *dev);

typedef int (*rte_cryptodev_scheduler_config_option_set)(
		struct rte_cryptodev *dev,
		uint32_t option_type,
		void *option);

typedef int (*rte_cryptodev_scheduler_config_option_get)(
		struct rte_cryptodev *dev,
		uint32_t option_type,
		void *option);

struct rte_cryptodev_scheduler_ops {
	rte_cryptodev_scheduler_slave_attach_t slave_attach;
	rte_cryptodev_scheduler_slave_attach_t slave_detach;

	rte_cryptodev_scheduler_start_t scheduler_start;
	rte_cryptodev_scheduler_stop_t scheduler_stop;

	rte_cryptodev_scheduler_config_queue_pair config_queue_pair;

	rte_cryptodev_scheduler_create_private_ctx create_private_ctx;

	rte_cryptodev_scheduler_config_option_set option_set;
	rte_cryptodev_scheduler_config_option_get option_get;
};

#ifdef __cplusplus
}
#endif
#endif /* _RTE_CRYPTO_SCHEDULER_OPERATIONS_H */
