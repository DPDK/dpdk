/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   Copyright 2016 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef RTE_FLOW_DRIVER_H_
#define RTE_FLOW_DRIVER_H_

/**
 * @file
 * RTE generic flow API (driver side)
 *
 * This file provides implementation helpers for internal use by PMDs, they
 * are not intended to be exposed to applications and are not subject to ABI
 * versioning.
 */

#include <stdint.h>

#include <rte_errno.h>
#include "rte_ethdev.h"
#include "rte_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic flow operations structure implemented and returned by PMDs.
 *
 * To implement this API, PMDs must handle the RTE_ETH_FILTER_GENERIC filter
 * type in their .filter_ctrl callback function (struct eth_dev_ops) as well
 * as the RTE_ETH_FILTER_GET filter operation.
 *
 * If successful, this operation must result in a pointer to a PMD-specific
 * struct rte_flow_ops written to the argument address as described below:
 *
 * \code
 *
 * // PMD filter_ctrl callback
 *
 * static const struct rte_flow_ops pmd_flow_ops = { ... };
 *
 * switch (filter_type) {
 * case RTE_ETH_FILTER_GENERIC:
 *     if (filter_op != RTE_ETH_FILTER_GET)
 *         return -EINVAL;
 *     *(const void **)arg = &pmd_flow_ops;
 *     return 0;
 * }
 *
 * \endcode
 *
 * See also rte_flow_ops_get().
 *
 * These callback functions are not supposed to be used by applications
 * directly, which must rely on the API defined in rte_flow.h.
 *
 * Public-facing wrapper functions perform a few consistency checks so that
 * unimplemented (i.e. NULL) callbacks simply return -ENOTSUP. These
 * callbacks otherwise only differ by their first argument (with port ID
 * already resolved to a pointer to struct rte_eth_dev).
 */
struct rte_flow_ops {
	/** See rte_flow_validate(). */
	int (*validate)
		(struct rte_eth_dev *,
		 const struct rte_flow_attr *,
		 const struct rte_flow_item [],
		 const struct rte_flow_action [],
		 struct rte_flow_error *);
	/** See rte_flow_create(). */
	struct rte_flow *(*create)
		(struct rte_eth_dev *,
		 const struct rte_flow_attr *,
		 const struct rte_flow_item [],
		 const struct rte_flow_action [],
		 struct rte_flow_error *);
	/** See rte_flow_destroy(). */
	int (*destroy)
		(struct rte_eth_dev *,
		 struct rte_flow *,
		 struct rte_flow_error *);
	/** See rte_flow_flush(). */
	int (*flush)
		(struct rte_eth_dev *,
		 struct rte_flow_error *);
	/** See rte_flow_query(). */
	int (*query)
		(struct rte_eth_dev *,
		 struct rte_flow *,
		 enum rte_flow_action_type,
		 void *,
		 struct rte_flow_error *);
	/** See rte_flow_isolate(). */
	int (*isolate)
		(struct rte_eth_dev *,
		 int,
		 struct rte_flow_error *);
};

/**
 * Initialize generic flow error structure.
 *
 * This function also sets rte_errno to a given value.
 *
 * @param[out] error
 *   Pointer to flow error structure (may be NULL).
 * @param code
 *   Related error code (rte_errno).
 * @param type
 *   Cause field and error types.
 * @param cause
 *   Object responsible for the error.
 * @param message
 *   Human-readable error message.
 *
 * @return
 *   Error code.
 */
static inline int
rte_flow_error_set(struct rte_flow_error *error,
		   int code,
		   enum rte_flow_error_type type,
		   const void *cause,
		   const char *message)
{
	if (error) {
		*error = (struct rte_flow_error){
			.type = type,
			.cause = cause,
			.message = message,
		};
	}
	rte_errno = code;
	return code;
}

/**
 * Get generic flow operations structure from a port.
 *
 * @param port_id
 *   Port identifier to query.
 * @param[out] error
 *   Pointer to flow error structure.
 *
 * @return
 *   The flow operations structure associated with port_id, NULL in case of
 *   error, in which case rte_errno is set and the error structure contains
 *   additional details.
 */
const struct rte_flow_ops *
rte_flow_ops_get(uint8_t port_id, struct rte_flow_error *error);

#ifdef __cplusplus
}
#endif

#endif /* RTE_FLOW_DRIVER_H_ */
