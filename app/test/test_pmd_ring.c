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
#include "test.h"

#include <stdio.h>

#include <rte_eth_ring.h>
#include <rte_ethdev.h>

static struct rte_mempool *mp;

#define TX_PORT 0
#define RX_PORT 1
#define RXTX_PORT 2
#define RXTX_PORT2 3
#define RXTX_PORT3 4
#define SOCKET0 0

#define RING_SIZE 256

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   512

static int
test_ethdev_configure(void)
{
	struct rte_eth_conf null_conf;
	struct rte_eth_link link;

	memset(&null_conf, 0, sizeof(struct rte_eth_conf));

	if ((TX_PORT >= RTE_MAX_ETHPORTS) || (RX_PORT >= RTE_MAX_ETHPORTS)\
		|| (RXTX_PORT >= RTE_MAX_ETHPORTS)) {
		printf(" TX/RX port exceed max eth ports\n");
		return -1;
	}
	if (rte_eth_dev_configure(TX_PORT, 1, 2, &null_conf) < 0) {
		printf("Configure failed for TX port\n");
		return -1;
	}

	/* Test queue release */
	if (rte_eth_dev_configure(TX_PORT, 1, 1, &null_conf) < 0) {
		printf("Configure failed for TX port\n");
		return -1;
	}
	if (rte_eth_dev_configure(RX_PORT, 1, 1, &null_conf) < 0) {
		printf("Configure failed for RX port\n");
		return -1;
	}
	if (rte_eth_dev_configure(RXTX_PORT, 1, 1, &null_conf) < 0) {
		printf("Configure failed for RXTX port\n");
		return -1;
	}

	if (rte_eth_tx_queue_setup(TX_PORT, 0, RING_SIZE, SOCKET0, NULL) < 0) {
		printf("TX queue setup failed\n");
		return -1;
	}
	if (rte_eth_rx_queue_setup(RX_PORT, 0, RING_SIZE, SOCKET0,
			NULL, mp) < 0) {
		printf("RX queue setup failed\n");
		return -1;
	}
	if (rte_eth_tx_queue_setup(RXTX_PORT, 0, RING_SIZE, SOCKET0, NULL) < 0) {
		printf("TX queue setup failed\n");
		return -1;
	}
	if (rte_eth_rx_queue_setup(RXTX_PORT, 0, RING_SIZE, SOCKET0,
			NULL, mp) < 0) {
		printf("RX queue setup failed\n");
		return -1;
	}

	if (rte_eth_dev_start(TX_PORT) < 0) {
		printf("Error starting TX port\n");
		return -1;
	}
	if (rte_eth_dev_start(RX_PORT) < 0) {
		printf("Error starting RX port\n");
		return -1;
	}
	if (rte_eth_dev_start(RXTX_PORT) < 0) {
		printf("Error starting RX port\n");
		return -1;
	}

	rte_eth_link_get(TX_PORT, &link);
	rte_eth_link_get(RX_PORT, &link);
	rte_eth_link_get(RXTX_PORT, &link);

	return 0;
}

static int
test_send_basic_packets(void)
{
	struct rte_mbuf  bufs[RING_SIZE];
	struct rte_mbuf *pbufs[RING_SIZE];
	int i;

	printf("Testing ring pmd RX/TX\n");

	for (i = 0; i < RING_SIZE/2; i++)
		pbufs[i] = &bufs[i];

	if (rte_eth_tx_burst(TX_PORT, 0, pbufs, RING_SIZE/2) < RING_SIZE/2) {
		printf("Failed to transmit packet burst\n");
		return -1;
	}

	if (rte_eth_rx_burst(RX_PORT, 0, pbufs, RING_SIZE) != RING_SIZE/2) {
		printf("Failed to receive packet burst\n");
		return -1;
	}

	for (i = 0; i < RING_SIZE/2; i++)
		if (pbufs[i] != &bufs[i]) {
			printf("Error: received data does not match that transmitted\n");
			return -1;
		}

	return 0;
}

static int
test_get_stats(void)
{
	struct rte_eth_stats stats;
	struct rte_mbuf buf, *pbuf = &buf;

	printf("Testing ring PMD stats\n");

	/* check stats of RXTX port, should all be zero */
	rte_eth_stats_get(RXTX_PORT, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not zero\n");
		return -1;
	}

	/* send and receive 1 packet and check for stats update */
	if (rte_eth_tx_burst(RXTX_PORT, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to RXTX port\n");
		return -1;
	}
	if (rte_eth_rx_burst(RXTX_PORT, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from RXTX port\n");
		return -1;
	}

	rte_eth_stats_get(RXTX_PORT, &stats);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}
	return 0;
}

static int
test_stats_reset(void)
{
	struct rte_eth_stats stats;
	struct rte_mbuf buf, *pbuf = &buf;

	printf("Testing ring PMD stats reset\n");

	rte_eth_stats_reset(RXTX_PORT);

	/* check stats of RXTX port, should all be zero */
	rte_eth_stats_get(RXTX_PORT, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not zero\n");
		return -1;
	}

	/* send and receive 1 packet and check for stats update */
	if (rte_eth_tx_burst(RXTX_PORT, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to RXTX port\n");
		return -1;
	}

	if (rte_eth_rx_burst(RXTX_PORT, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from RXTX port\n");
		return -1;
	}

	rte_eth_stats_get(RXTX_PORT, &stats);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	rte_eth_stats_reset(RXTX_PORT);

	/* check stats of RXTX port, should all be zero */
	rte_eth_stats_get(RXTX_PORT, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not zero\n");
		return -1;
	}

	return 0;
}

static int
test_pmd_ring_pair_create_attach(void)
{
	struct rte_eth_stats stats, stats2;
	struct rte_mbuf buf, *pbuf = &buf;
	struct rte_eth_conf null_conf;

	if ((RXTX_PORT2 >= RTE_MAX_ETHPORTS) || (RXTX_PORT3 >= RTE_MAX_ETHPORTS)) {
		printf(" TX/RX port exceed max eth ports\n");
		return -1;
	}
	if ((rte_eth_dev_configure(RXTX_PORT2, 1, 1, &null_conf) < 0)
		|| (rte_eth_dev_configure(RXTX_PORT3, 1, 1, &null_conf) < 0)) {
		printf("Configure failed for RXTX port\n");
		return -1;
	}

	if ((rte_eth_tx_queue_setup(RXTX_PORT2, 0, RING_SIZE, SOCKET0, NULL) < 0)
		|| (rte_eth_tx_queue_setup(RXTX_PORT3, 0, RING_SIZE, SOCKET0, NULL) < 0)) {
		printf("TX queue setup failed\n");
		return -1;
	}

	if ((rte_eth_rx_queue_setup(RXTX_PORT2, 0, RING_SIZE, SOCKET0, NULL, mp) < 0)
		|| (rte_eth_rx_queue_setup(RXTX_PORT3, 0, RING_SIZE, SOCKET0, NULL, mp) < 0)) {
		printf("RX queue setup failed\n");
		return -1;
	}

	if ((rte_eth_dev_start(RXTX_PORT2) < 0)
		|| (rte_eth_dev_start(RXTX_PORT3) < 0)) {
		printf("Error starting RXTX port\n");
		return -1;
	}

	/*
	 * send and receive 1 packet (RXTX_PORT2 -> RXTX_PORT3)
	 * and check for stats update
	 */
	if (rte_eth_tx_burst(RXTX_PORT2, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to RXTX port\n");
		return -1;
	}

	if (rte_eth_rx_burst(RXTX_PORT3, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from RXTX port\n");
		return -1;
	}

	rte_eth_stats_get(RXTX_PORT2, &stats);
	rte_eth_stats_get(RXTX_PORT3, &stats2);
	if (stats.ipackets != 0 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 0 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	/*
	 * send and receive 1 packet (RXTX_PORT3 -> RXTX_PORT2)
	 * and check for stats update
	 */
	if (rte_eth_tx_burst(RXTX_PORT3, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to RXTX port\n");
		return -1;
	}

	if (rte_eth_rx_burst(RXTX_PORT2, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from RXTX port\n");
		return -1;
	}

	rte_eth_stats_get(RXTX_PORT2, &stats);
	rte_eth_stats_get(RXTX_PORT3, &stats2);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 1 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	/*
	 * send and receive 1 packet (RXTX_PORT2 -> RXTX_PORT2)
	 * and check for stats update
	 */
	if (rte_eth_tx_burst(RXTX_PORT2, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to RXTX port\n");
		return -1;
	}

	if (rte_eth_rx_burst(RXTX_PORT2, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from RXTX port\n");
		return -1;
	}

	rte_eth_stats_get(RXTX_PORT2, &stats);
	rte_eth_stats_get(RXTX_PORT3, &stats2);
	if (stats.ipackets != 2 || stats.opackets != 2 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 1 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	/*
	 * send and receive 1 packet (RXTX_PORT3 -> RXTX_PORT3)
	 * and check for stats update
	 */
	if (rte_eth_tx_burst(RXTX_PORT3, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to RXTX port\n");
		return -1;
	}

	if (rte_eth_rx_burst(RXTX_PORT3, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from RXTX port\n");
		return -1;
	}

	rte_eth_stats_get(RXTX_PORT2, &stats);
	rte_eth_stats_get(RXTX_PORT3, &stats2);
	if (stats.ipackets != 2 || stats.opackets != 2 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	if (stats2.ipackets != 2 || stats2.opackets != 2 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: RXTX port stats are not as expected\n");
		return -1;
	}

	rte_eth_dev_stop(RXTX_PORT2);
	rte_eth_dev_stop(RXTX_PORT3);

	return 0;
}

static int
test_pmd_ring(void)
{
	mp = rte_mempool_create("mbuf_pool", NB_MBUF,
			MBUF_SIZE, 32,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL,
			rte_pktmbuf_init, NULL,
			rte_socket_id(), 0);
	if (mp == NULL)
		return -1;

	if ((TX_PORT >= RTE_MAX_ETHPORTS) || (RX_PORT >= RTE_MAX_ETHPORTS)\
		|| (RXTX_PORT >= RTE_MAX_ETHPORTS)) {
		printf(" TX/RX port exceed max eth ports\n");
		return -1;
	}

	if (test_ethdev_configure() < 0)
		return -1;

	if (test_send_basic_packets() < 0)
		return -1;

	if (test_get_stats() < 0)
		return -1;

	if (test_stats_reset() < 0)
		return -1;

	rte_eth_dev_stop(RX_PORT);
	rte_eth_dev_stop(TX_PORT);
	rte_eth_dev_stop(RXTX_PORT);

	if (test_pmd_ring_pair_create_attach() < 0)
		return -1;

	return 0;
}

static struct test_command ring_pmd_cmd = {
	.command = "ring_pmd_autotest",
	.callback = test_pmd_ring,
};
REGISTER_TEST_COMMAND(ring_pmd_cmd);
