/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "rte_telemetry_internal.h"
#include "rte_compat.h"

#ifndef _RTE_TELEMETRY_PARSER_H_
#define _RTE_TELEMETRY_PARSER_H_

__rte_experimental
int32_t
rte_telemetry_parse(struct telemetry_impl *telemetry, char *socket_rx_data);

#endif
