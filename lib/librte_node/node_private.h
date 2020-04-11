/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __NODE_PRIVATE_H__
#define __NODE_PRIVATE_H__

#include <rte_common.h>
#include <rte_log.h>

extern int rte_node_logtype;
#define NODE_LOG(level, node_name, ...)                                        \
	rte_log(RTE_LOG_##level, rte_node_logtype,                             \
		RTE_FMT("NODE %s: %s():%u " RTE_FMT_HEAD(__VA_ARGS__, ) "\n",  \
			node_name, __func__, __LINE__,                         \
			RTE_FMT_TAIL(__VA_ARGS__, )))

#define node_err(node_name, ...) NODE_LOG(ERR, node_name, __VA_ARGS__)
#define node_info(node_name, ...) NODE_LOG(INFO, node_name, __VA_ARGS__)
#define node_dbg(node_name, ...) NODE_LOG(DEBUG, node_name, __VA_ARGS__)

#endif /* __NODE_PRIVATE_H__ */
