/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARK_ETHDEV_RX_H_
#define _ARK_ETHDEV_RX_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>


int eth_ark_dev_rx_queue_setup(struct rte_eth_dev *dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_rxconf *rx_conf,
			       struct rte_mempool *mp);
uint32_t eth_ark_dev_rx_queue_count(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id);
int eth_ark_rx_stop_queue(struct rte_eth_dev *dev, uint16_t queue_id);
int eth_ark_rx_start_queue(struct rte_eth_dev *dev, uint16_t queue_id);
uint16_t eth_ark_recv_pkts_noop(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts);
uint16_t eth_ark_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
void eth_ark_dev_rx_queue_release(void *rx_queue);
void eth_rx_queue_stats_get(void *vqueue, struct rte_eth_stats *stats);
void eth_rx_queue_stats_reset(void *vqueue);
void eth_ark_rx_dump_queue(struct rte_eth_dev *dev, uint16_t queue_id,
			   const char *msg);
void eth_ark_udm_force_close(struct rte_eth_dev *dev);

#endif
