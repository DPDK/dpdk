/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_NEIGH_H
#define APP_GRAPH_NEIGH_H

extern cmdline_parse_inst_t neigh_v4_cmd_ctx;
extern cmdline_parse_inst_t neigh_v6_cmd_ctx;
extern cmdline_parse_inst_t neigh_help_cmd_ctx;

void neigh4_list_clean(void);
void neigh6_list_clean(void);
int neigh_ip4_add_to_rewrite(void);
int neigh_ip6_add_to_rewrite(void);

#endif
