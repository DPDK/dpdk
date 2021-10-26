/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>

#include <rte_eth_ring.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_bus_vdev.h>
#include "rte_lcore.h"
#include "rte_mempool.h"
#include "rte_ring.h"

#include "sample_packet_forward.h"

/*
 * heper function: configure and start test device
 */
int
test_dev_start(uint16_t port, struct rte_mempool *mp)
{
	int32_t rc;
	struct rte_eth_conf pconf;

	memset(&pconf, 0, sizeof(pconf));

	rc =  rte_eth_dev_configure(port, NUM_QUEUES, NUM_QUEUES, &pconf);
	if (rc != 0)
		return rc;

	rc = rte_eth_rx_queue_setup(port, 0, RING_SIZE, SOCKET_ID_ANY,
		NULL, mp);
	if (rc != 0)
		return rc;

	rc = rte_eth_tx_queue_setup(port, 0, RING_SIZE, SOCKET_ID_ANY,
		NULL);
	if (rc != 0)
		return rc;

	rc = rte_eth_dev_start(port);
	return rc;
}

/* Sample test to create virtual rings and tx,rx portid from rings */
int
test_ring_setup(struct rte_ring **ring, uint16_t *portid)
{
	*ring = rte_ring_create("R0", RING_SIZE, rte_socket_id(),
				  RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (*ring == NULL) {
		printf("%s() line %u: rte_ring_create R0 failed",
		       __func__, __LINE__);
		return -1;
	}
	*portid = rte_eth_from_rings("net_ringa", ring, NUM_QUEUES,
			ring, NUM_QUEUES, rte_socket_id());

	return 0;
}

/* Sample test to free the mempool */
void
test_mp_free(struct rte_mempool *mp)
{
	rte_mempool_free(mp);
}

/* Sample test to free the virtual rings */
void
test_ring_free(struct rte_ring *rxtx)
{
	rte_ring_free(rxtx);
}

/* Sample test to release the vdev */
void
test_vdev_uninit(const char *vdev)
{
	rte_vdev_uninit(vdev);
}

/* sample test to allocate the mempool */
int
test_get_mempool(struct rte_mempool **mp, char *poolname)
{
	*mp = rte_pktmbuf_pool_create(poolname, NB_MBUF, 32, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (*mp == NULL)
		return -1;
	return 0;
}

/* sample test to allocate buffer for pkts */
int
test_get_mbuf_from_pool(struct rte_mempool **mp, struct rte_mbuf **pbuf,
		char *poolname)
{
	int ret = 0;

	ret = test_get_mempool(mp, poolname);
	if (ret < 0)
		return -1;
	if (rte_pktmbuf_alloc_bulk(*mp, pbuf, NUM_PACKETS) != 0) {
		printf("%s() line %u: rte_pktmbuf_alloc_bulk failed", __func__,
		       __LINE__);
		return -1;
	}
	return 0;
}

/* sample test to deallocate the allocated buffers and mempool */
void
test_put_mbuf_to_pool(struct rte_mempool *mp, struct rte_mbuf **pbuf)
{
	int itr = 0;

	for (itr = 0; itr < NUM_PACKETS; itr++)
		rte_pktmbuf_free(pbuf[itr]);
	rte_mempool_free(mp);
}

/* Sample test to forward packets using virtual portids */
int
test_packet_forward(struct rte_mbuf **pbuf, uint16_t portid, uint16_t queue_id)
{
	/* send and receive packet and check for stats update */
	if (rte_eth_tx_burst(portid, queue_id, pbuf, NUM_PACKETS)
			< NUM_PACKETS) {
		printf("%s() line %u: Error sending packet to"
		       " port %d\n", __func__, __LINE__, portid);
		return -1;
	}
	if (rte_eth_rx_burst(portid, queue_id, pbuf, NUM_PACKETS)
			< NUM_PACKETS) {
		printf("%s() line %u: Error receiving packet from"
		       " port %d\n", __func__, __LINE__, portid);
		return -1;
	}
	return 0;
}
