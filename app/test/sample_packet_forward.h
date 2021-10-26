/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _SAMPLE_PACKET_FORWARD_H_
#define _SAMPLE_PACKET_FORWARD_H_

#include <stdint.h>

/* MACROS to support virtual ring creation */
#define RING_SIZE 256
#define NUM_QUEUES 1
#define NB_MBUF 512

#define NUM_PACKETS 10

struct rte_mbuf;
struct rte_mempool;
struct rte_ring;

/* Sample test to create virtual rings and tx,rx portid from rings */
int test_ring_setup(struct rte_ring **ring, uint16_t *portid);

/* configure and start device created by test_ring_setup */
int test_dev_start(uint16_t port, struct rte_mempool *mp);

/* Sample test to free the virtual rings */
void test_ring_free(struct rte_ring *rxtx);

/* Sample test to forward packet using virtual port id */
int test_packet_forward(struct rte_mbuf **pbuf, uint16_t portid,
		uint16_t queue_id);

/* sample test to allocate buffer for pkts */
int test_get_mbuf_from_pool(struct rte_mempool **mp, struct rte_mbuf **pbuf,
		char *poolname);

/* Sample test to create the mempool */
int test_get_mempool(struct rte_mempool **mp, char *poolname);

/* sample test to deallocate the allocated buffers and mempool */
void test_put_mbuf_to_pool(struct rte_mempool *mp, struct rte_mbuf **pbuf);

/* Sample test to free the mempool */
void test_mp_free(struct rte_mempool *mp);

/* Sample test to release the vdev */
void test_vdev_uninit(const char *vdev);

#endif /* _SAMPLE_PACKET_FORWARD_H_ */
