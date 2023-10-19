/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_UTILS_H
#define APP_GRAPH_UTILS_H

int parser_uint64_read(uint64_t *value, const char *p);
int parser_uint32_read(uint32_t *value, const char *p);
int parser_ip4_read(uint32_t *value, char *p);
int parser_ip6_read(uint8_t *value, char *p);
int parser_mac_read(uint64_t *value, char *p);

#endif
