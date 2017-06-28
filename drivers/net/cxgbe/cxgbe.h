/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2017 Chelsio Communications.
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
 *     * Neither the name of Chelsio Communications nor the names of its
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

#ifndef _CXGBE_H_
#define _CXGBE_H_

#include "common.h"
#include "t4_regs.h"

#define CXGBE_MIN_RING_DESC_SIZE      128  /* Min TX/RX descriptor ring size */
#define CXGBE_MAX_RING_DESC_SIZE      4096 /* Max TX/RX descriptor ring size */

#define CXGBE_DEFAULT_TX_DESC_SIZE    1024 /* Default TX ring size */
#define CXGBE_DEFAULT_RX_DESC_SIZE    1024 /* Default RX ring size */

#define CXGBE_MIN_RX_BUFSIZE ETHER_MIN_MTU /* min buf size */
#define CXGBE_MAX_RX_PKTLEN (9000 + ETHER_HDR_LEN + ETHER_CRC_LEN) /* max pkt */

int cxgbe_probe(struct adapter *adapter);
void cxgbe_get_speed_caps(struct port_info *pi, u32 *speed_caps);
int cxgbe_up(struct adapter *adap);
int cxgbe_down(struct port_info *pi);
void cxgbe_close(struct adapter *adapter);
void cxgbe_stats_get(struct port_info *pi, struct port_stats *stats);
void cxgbe_stats_reset(struct port_info *pi);
int link_start(struct port_info *pi);
void init_rspq(struct adapter *adap, struct sge_rspq *q, unsigned int us,
	       unsigned int cnt, unsigned int size, unsigned int iqe_size);
int setup_sge_fwevtq(struct adapter *adapter);
void cfg_queues(struct rte_eth_dev *eth_dev);
int cfg_queue_count(struct rte_eth_dev *eth_dev);
int setup_rss(struct port_info *pi);
void cxgbe_enable_rx_queues(struct port_info *pi);

#endif /* _CXGBE_H_ */
