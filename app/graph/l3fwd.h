/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_L3FWD_H
#define APP_GRAPH_L3FWD_H

int usecase_l3fwd_configure(struct rte_node_ethdev_config *conf, uint16_t nb_conf,
			    uint16_t nb_graphs);

#endif
