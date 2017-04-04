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

#ifndef _ARK_ETHDEV_TX_H_
#define _ARK_ETHDEV_TX_H_

#include <stdint.h>

#include <rte_ethdev.h>


uint16_t eth_ark_xmit_pkts_noop(void *vtxq,
				struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts);
uint16_t eth_ark_xmit_pkts(void *vtxq,
			   struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts);
int eth_ark_tx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   const struct rte_eth_txconf *tx_conf);
void eth_ark_tx_queue_release(void *vtx_queue);
int eth_ark_tx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id);
int eth_ark_tx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id);
void eth_tx_queue_stats_get(void *vqueue, struct rte_eth_stats *stats);
void eth_tx_queue_stats_reset(void *vqueue);

#endif
