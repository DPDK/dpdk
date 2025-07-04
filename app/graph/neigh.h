/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_NEIGH_H
#define APP_GRAPH_NEIGH_H

void neigh4_list_clean(void);
void neigh6_list_clean(void);
int neigh_ip4_add_to_rewrite(void);
int neigh_ip6_add_to_rewrite(void);

#endif
