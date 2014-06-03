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

#ifndef _IVSHMEM_COMMON_H_
#define _IVSHMEM_COMMON_H_

#define RTE_LOGTYPE_L2FWD_IVSHMEM RTE_LOGTYPE_USER1

#define CTRL_MZ_NAME "CTRL_MEMZONE"
#define MBUF_MP_NAME "MBUF_MEMPOOL"
#define RX_RING_PREFIX "RX_"
#define TX_RING_PREFIX "TX_"

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000; /* default period is 10 seconds */

#define DIM(x)\
	(sizeof(x)/sizeof(x)[0])

#define MAX_PKT_BURST 32

const struct rte_memzone * ctrl_mz;

enum l2fwd_state {
	STATE_NONE = 0,
	STATE_FWD,
	STATE_EXIT,
	STATE_FAIL
};

/* Per-port statistics struct */
struct port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;

struct mbuf_table {
	unsigned len;
	struct rte_mbuf *m_table[MAX_PKT_BURST * 2]; /**< allow up to two bursts */
};

struct vm_port_param {
	struct rte_ring * rx_ring;         /**< receiving ring for current port */
	struct rte_ring * tx_ring;         /**< transmitting ring for current port */
	struct vm_port_param * dst;        /**< current port's destination port */
	volatile struct port_statistics stats;      /**< statistics for current port */
	struct ether_addr ethaddr;         /**< Ethernet address of the port */
};

/* control structure, to synchronize host and VM */
struct ivshmem_ctrl {
	rte_spinlock_t lock;
	uint8_t nb_ports;                /**< total nr of ports */
	volatile enum l2fwd_state state; /**< report state */
	struct vm_port_param vm_ports[RTE_MAX_ETHPORTS];
};

struct ivshmem_ctrl * ctrl;

static unsigned int l2fwd_ivshmem_rx_queue_per_lcore = 1;

static void sighandler(int __rte_unused s)
{
	ctrl->state = STATE_EXIT;
}

static void sigsetup(void)
{
	   struct sigaction sigIntHandler;

	   sigIntHandler.sa_handler = sighandler;
	   sigemptyset(&sigIntHandler.sa_mask);
	   sigIntHandler.sa_flags = 0;

	   sigaction(SIGINT, &sigIntHandler, NULL);
}

#endif /* _IVSHMEM_COMMON_H_ */
