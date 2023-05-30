/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_PROCESS_H
#define PDCP_PROCESS_H

#include <rte_pdcp.h>

int
pdcp_process_func_set(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf);

#endif /* PDCP_PROCESS_H */
