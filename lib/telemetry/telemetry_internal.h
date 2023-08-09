/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_TELEMETRY_INTERNAL_H_
#define _RTE_TELEMETRY_INTERNAL_H_

#include <rte_compat.h>
#include <rte_os.h>
#include "rte_telemetry.h"

/**
 * @internal
 *
 * @file
 * RTE Telemetry Legacy and internal definitions
 */

/**
 * @internal
 * Value representing if data is required for the command
 */
enum rte_telemetry_legacy_data_req {
	DATA_NOT_REQ = 0,
	DATA_REQ
};

/**
 * This telemetry callback is used when registering a legacy telemetry command.
 * It handles getting and formatting stats to be returned to telemetry when
 * requested. Stats up to buf_len in length are put in the buffer.
 *
 * @param cmd
 * The cmd that was requested by the client.
 * @param params
 * Contains data required by the callback function.
 * @param buffer
 * A buffer to hold the formatted response.
 * @param buf_len
 * Length of the buffer.
 *
 * @return
 * Length of buffer used on success.
 * @return
 * Negative integer on error.
 */
typedef int (*telemetry_legacy_cb)(const char *cmd, const char *params,
		char *buffer, int buf_len);

/**
 * @internal
 * Counter for the number of registered legacy callbacks
 */
extern int num_legacy_callbacks;

/**
 * @internal
 * Used for handling data received over the legacy telemetry socket.
 *
 * @return
 * Void.
 */
void *
legacy_client_handler(void *sock_id);

/**
 * @internal
 *
 * Used when registering a command and callback function with
 * telemetry legacy support.
 *
 * @return
 *  0 on success.
 * @return
 *  -EINVAL for invalid parameters failure.
 *  @return
 *  -ENOENT if max callbacks limit has been reached.
 */
__rte_internal
int
rte_telemetry_legacy_register(const char *cmd,
		enum rte_telemetry_legacy_data_req data_req,
		telemetry_legacy_cb fn);

/**
 * @internal
 * Log function type, to allow passing as parameter if necessary
 */
typedef int (*rte_log_fn)(uint32_t level, uint32_t logtype, const char *format, ...);

/**
 * @internal
 * Initialize Telemetry.
 *
 * @param runtime_dir
 * The runtime directory of DPDK.
 * @param cpuset
 * The CPU set to be used for setting the thread affinity.
 * @param log_fn
 * Function pointer to the rte_log function for logging use
 * @param registered_logtype
 * The registered log type to use for logging
 *
 * @return
 *  0 on success.
 * @return
 *  -1 on failure.
 */
__rte_internal
int
rte_telemetry_init(const char *runtime_dir, const char *rte_version, rte_cpuset_t *cpuset);

#endif
