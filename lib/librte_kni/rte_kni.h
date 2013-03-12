/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 * 
 */

#ifndef _RTE_KNI_H_
#define _RTE_KNI_H_

/**
 * @file
 * RTE KNI
 *
 * The KNI library provides the ability to create and destroy kernel NIC
 * interfaces that may be used by the RTE application to receive/transmit
 * packets from/to Linux kernel net interfaces.
 *
 * This library provide two APIs to burst receive packets from KNI interfaces,
 * and burst transmit packets to KNI interfaces.
 */

#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_kni;

/**
 * Structure which has the function pointers for KNI interface.
 */
struct rte_kni_ops {
	/* Pointer to function of changing MTU */
	int (*change_mtu)(uint8_t port_id, unsigned new_mtu);

	/* Pointer to function of configuring network interface */
	int (*config_network_if)(uint8_t port_id, uint8_t if_up);
};

/**
 * Create kni interface according to the port id. It will create a paired KNI
 * interface in the kernel space for each NIC port. The KNI interface created
 * in the kernel space is the net interface the traditional Linux application
 * talking to.
 *
 * @param port_id
 *  The port id.
 * @param pktmbuf_pool
 *  The mempool for allocting mbufs for packets.
 * @param mbuf_size
 *  The mbuf size to store a packet.
 *
 * @return
 *  - The pointer to the context of a kni interface.
 *  - NULL indicate error.
 */
extern struct rte_kni *rte_kni_create(uint8_t port_id, unsigned mbuf_size,
		struct rte_mempool *pktmbuf_pool, struct rte_kni_ops *ops);

/**
 * Retrieve a burst of packets from a kni interface. The retrieved packets are
 * stored in rte_mbuf structures whose pointers are supplied in the array of
 * mbufs, and the maximum number is indicated by num. It handles the freeing of
 * the mbufs in the free queue of kni interface.
 *
 * @param kni
 *  The kni interface context.
 * @param mbufs
 *  The array to store the pointers of mbufs.
 * @param num
 *  The maximum number per burst.
 *
 * @return
 *  The actual number of packets retrieved.
 */
extern unsigned rte_kni_rx_burst(struct rte_kni *kni,
		struct rte_mbuf **mbufs, unsigned num);

/**
 * Send a burst of packets to a kni interface. The packets to be sent out are
 * stored in rte_mbuf structures whose pointers are supplied in the array of
 * mbufs, and the maximum number is indicated by num. It handles allocating
 * the mbufs for kni interface alloc queue.
 *
 * @param kni
 *  The kni interface context.
 * @param mbufs
 *  The array to store the pointers of mbufs.
 * @param num
 *  The maximum number per burst.
 *
 * @return
 *  The actual number of packets sent.
 */
extern unsigned rte_kni_tx_burst(struct rte_kni *kni,
		struct rte_mbuf **mbufs, unsigned num);

/**
 * Get the port id from kni interface.
 *
 * @param kni
 *  The kni interface context.
 *
 * @return
 *  On success: The port id.
 *  On failure: ~0x0
 */
extern uint8_t rte_kni_get_port_id(struct rte_kni *kni);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_KNI_H_ */

