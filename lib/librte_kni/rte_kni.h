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

#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_mempool.h>

#include <exec-env/rte_kni_common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_kni;
struct rte_mbuf;

/**
 * Structure which has the function pointers for KNI interface.
 */
struct rte_kni_ops {
	uint8_t port_id; /* Port ID */

	/* Pointer to function of changing MTU */
	int (*change_mtu)(uint8_t port_id, unsigned new_mtu);

	/* Pointer to function of configuring network interface */
	int (*config_network_if)(uint8_t port_id, uint8_t if_up);
};

/**
 * Structure for configuring KNI device.
 */
struct rte_kni_conf {
	/*
	 * KNI name which will be used in relevant network device.
	 * Let the name as short as possible, as it will be part of
	 * memzone name.
	 */
	char name[RTE_KNI_NAMESIZE];
	uint32_t core_id;   /* Core ID to bind kernel thread on */
	uint16_t group_id;  /* Group ID */
	unsigned mbuf_size; /* mbuf size */
	struct rte_pci_addr addr;
	struct rte_pci_id id;

	uint8_t force_bind : 1; /* Flag to bind kernel thread */
};

/**
 * Initialize and preallocate KNI subsystem
 *
 * This function is to be executed on the MASTER lcore only, after EAL
 * initialization and before any KNI interface is attempted to be
 * allocated
 *
 * @param max_kni_ifaces
 *  The maximum number of KNI interfaces that can coexist concurrently
 */
void rte_kni_init(unsigned int max_kni_ifaces);


/**
 * Allocate KNI interface according to the port id, mbuf size, mbuf pool,
 * configurations and callbacks for kernel requests.The KNI interface created
 * in the kernel space is the net interface the traditional Linux application
 * talking to.
 *
 * The rte_kni_alloc shall not be called before rte_kni_init() has been
 * called. rte_kni_alloc is thread safe.
 *
 * The mempool should have capacity of more than "2 x KNI_FIFO_COUNT_MAX"
 * elements for each KNI interface allocated.
 *
 * @param pktmbuf_pool
 *  The mempool for allocting mbufs for packets.
 * @param conf
 *  The pointer to the configurations of the KNI device.
 * @param ops
 *  The pointer to the callbacks for the KNI kernel requests.
 *
 * @return
 *  - The pointer to the context of a KNI interface.
 *  - NULL indicate error.
 */
struct rte_kni *rte_kni_alloc(struct rte_mempool *pktmbuf_pool,
		const struct rte_kni_conf *conf, struct rte_kni_ops *ops);

/**
 * Release KNI interface according to the context. It will also release the
 * paired KNI interface in kernel space. All processing on the specific KNI
 * context need to be stopped before calling this interface.
 *
 * rte_kni_release is thread safe.
 *
 * @param kni
 *  The pointer to the context of an existent KNI interface.
 *
 * @return
 *  - 0 indicates success.
 *  - negative value indicates failure.
 */
int rte_kni_release(struct rte_kni *kni);

/**
 * It is used to handle the request mbufs sent from kernel space.
 * Then analyzes it and calls the specific actions for the specific requests.
 * Finally constructs the response mbuf and puts it back to the resp_q.
 *
 * @param kni
 *  The pointer to the context of an existent KNI interface.
 *
 * @return
 *  - 0
 *  - negative value indicates failure.
 */
int rte_kni_handle_request(struct rte_kni *kni);

/**
 * Retrieve a burst of packets from a KNI interface. The retrieved packets are
 * stored in rte_mbuf structures whose pointers are supplied in the array of
 * mbufs, and the maximum number is indicated by num. It handles allocating
 * the mbufs for KNI interface alloc queue.
 *
 * @param kni
 *  The KNI interface context.
 * @param mbufs
 *  The array to store the pointers of mbufs.
 * @param num
 *  The maximum number per burst.
 *
 * @return
 *  The actual number of packets retrieved.
 */
unsigned rte_kni_rx_burst(struct rte_kni *kni, struct rte_mbuf **mbufs,
		unsigned num);

/**
 * Send a burst of packets to a KNI interface. The packets to be sent out are
 * stored in rte_mbuf structures whose pointers are supplied in the array of
 * mbufs, and the maximum number is indicated by num. It handles the freeing of
 * the mbufs in the free queue of KNI interface.
 *
 * @param kni
 *  The KNI interface context.
 * @param mbufs
 *  The array to store the pointers of mbufs.
 * @param num
 *  The maximum number per burst.
 *
 * @return
 *  The actual number of packets sent.
 */
unsigned rte_kni_tx_burst(struct rte_kni *kni, struct rte_mbuf **mbufs,
		unsigned num);

/**
 * Get the KNI context of its name.
 *
 * @param name
 *  pointer to the KNI device name.
 *
 * @return
 *  On success: Pointer to KNI interface.
 *  On failure: NULL.
 */
struct rte_kni *rte_kni_get(const char *name);

/**
 * Get the name given to a KNI device
 *
 * @param kni
 *   The KNI instance to query
 * @return
 *   The pointer to the KNI name
 */
const char *rte_kni_get_name(const struct rte_kni *kni);

/**
 * Register KNI request handling for a specified port,and it can
 * be called by master process or slave process.
 *
 * @param kni
 *  pointer to struct rte_kni.
 * @param ops
 *  ponter to struct rte_kni_ops.
 *
 * @return
 *  On success: 0
 *  On failure: -1
 */
int rte_kni_register_handlers(struct rte_kni *kni, struct rte_kni_ops *ops);

/**
 *  Unregister KNI request handling for a specified port.
 *
 *  @param kni
 *   pointer to struct rte_kni.
 *
 *  @return
 *   On success: 0
 *   On failure: -1
 */
int rte_kni_unregister_handlers(struct rte_kni *kni);

/**
 *  Close KNI device.
 */
void rte_kni_close(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_KNI_H_ */
