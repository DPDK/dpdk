/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_L2FWD_H
#define APP_GRAPH_L2FWD_H

int usecase_l2fwd_configure(struct rte_node_ethdev_config *conf, uint16_t nb_conf,
			    uint16_t nb_graphs);

#endif
