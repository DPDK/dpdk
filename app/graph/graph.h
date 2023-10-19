/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_H
#define APP_GRAPH_H

#include <cmdline_parse.h>

extern cmdline_parse_inst_t graph_config_cmd_ctx;
extern cmdline_parse_inst_t graph_start_cmd_ctx;
extern cmdline_parse_inst_t graph_stats_cmd_ctx;
extern cmdline_parse_inst_t graph_help_cmd_ctx;

int graph_walk_start(void *conf);
void graph_stats_print(void);
void graph_pcap_config_get(uint8_t *pcap_ena, uint64_t *num_pkts, char **file);
uint64_t graph_coremask_get(void);
bool graph_status_get(void);

#endif
