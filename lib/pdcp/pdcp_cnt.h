/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_CNT_H
#define PDCP_CNT_H

#include <rte_common.h>

#include "pdcp_entity.h"

int pdcp_cnt_ring_create(struct rte_pdcp_entity *en, const struct rte_pdcp_entity_conf *conf);

#endif /* PDCP_CNT_H */
