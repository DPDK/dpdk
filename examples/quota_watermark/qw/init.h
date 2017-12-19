/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _INIT_H_
#define _INIT_H_

void configure_eth_port(uint16_t port_id);
void init_dpdk(void);
void init_ring(int lcore_id, uint16_t port_id);
void pair_ports(void);
void setup_shared_variables(void);

#endif /* _INIT_H_ */
