/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __VIRTUAL_ETHDEV_H_
#define __VIRTUAL_ETHDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ether.h>

int virtual_ethdev_init(void);

int virtual_ethdev_create(const char *name, struct ether_addr *mac_addr, uint8_t socket_id);

void virtual_ethdev_simulate_link_status_interrupt(uint8_t port_id, uint8_t link_status);

void virtual_ethdev_add_mbufs_to_rx_queue(uint8_t port_id, struct rte_mbuf **pkts_burst, int burst_length);


/** Control methods for the dev_ops functions pointer to control the behavior of the Virtual PMD */

void virtual_ethdev_start_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_stop_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_configure_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_rx_queue_setup_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_tx_queue_setup_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_link_update_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_rx_burst_fn_set_success(uint8_t port_id, uint8_t success);

void virtual_ethdev_tx_burst_fn_set_success(uint8_t port_id, uint8_t success);

#ifdef __cplusplus
}
#endif

#endif /* __VIRTUAL_ETHDEV_H_ */
