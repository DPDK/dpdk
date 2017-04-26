/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
