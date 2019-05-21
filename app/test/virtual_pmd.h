/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __VIRTUAL_ETHDEV_H_
#define __VIRTUAL_ETHDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ether.h>

int
virtual_ethdev_init(void);

int
virtual_ethdev_create(const char *name, struct rte_ether_addr *mac_addr,
		uint8_t socket_id, uint8_t isr_support);

void
virtual_ethdev_set_link_status(uint16_t port_id, uint8_t link_status);

void
virtual_ethdev_simulate_link_status_interrupt(uint16_t port_id,
		uint8_t link_status);

int
virtual_ethdev_add_mbufs_to_rx_queue(uint16_t port_id,
		struct rte_mbuf **pkts_burst, int burst_length);

int
virtual_ethdev_get_mbufs_from_tx_queue(uint16_t port_id,
		struct rte_mbuf **pkt_burst, int burst_length);

/** Control methods for the dev_ops functions pointer to control the behavior
 *  of the Virtual PMD */

void
virtual_ethdev_start_fn_set_success(uint16_t port_id, uint8_t success);

void
virtual_ethdev_stop_fn_set_success(uint16_t port_id, uint8_t success);

void
virtual_ethdev_configure_fn_set_success(uint16_t port_id, uint8_t success);

void
virtual_ethdev_rx_queue_setup_fn_set_success(uint16_t port_id,
					      uint8_t success);

void
virtual_ethdev_tx_queue_setup_fn_set_success(uint16_t port_id,
					      uint8_t success);

void
virtual_ethdev_link_update_fn_set_success(uint16_t port_id, uint8_t success);

void
virtual_ethdev_rx_burst_fn_set_success(uint16_t port_id, uint8_t success);

void
virtual_ethdev_tx_burst_fn_set_success(uint16_t port_id, uint8_t success);

/* if a value greater than zero is set for packet_fail_count then virtual
 * device tx burst function will fail that many packet from burst or all
 * packets if packet_fail_count is greater than the number of packets in the
 * burst */
void
virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(uint16_t port_id,
		uint8_t packet_fail_count);

#ifdef __cplusplus
}
#endif

#endif /* __VIRTUAL_ETHDEV_H_ */
