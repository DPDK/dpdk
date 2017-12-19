/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */


void ethapp_main(void);
void print_stats(void);
void lock_port(int idx_port);
void unlock_port(int idx_port);
void mark_port_inactive(int idx_port);
void mark_port_active(int idx_port);
void mark_port_newmac(int idx_port);
