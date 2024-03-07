/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_H
#define APP_GRAPH_H

#include <cmdline_parse.h>

int graph_walk_start(void *conf);
void graph_stats_print(void);
void graph_pcap_config_get(uint8_t *pcap_ena, uint64_t *num_pkts, char **file);
uint64_t graph_coremask_get(void);
bool graph_status_get(void);

#endif
