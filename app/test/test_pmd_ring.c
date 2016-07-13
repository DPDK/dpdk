/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
static int tx_porta, rx_portb, rxtx_portc, rxtx_portd, rxtx_porte;

#define SOCKET0 0
#define RING_SIZE 256
#define NUM_RINGS 2
#define NB_MBUF 512


static int
test_ethdev_configure_port(int port)
{
	struct rte_eth_conf null_conf;
	struct rte_eth_link link;

	memset(&null_conf, 0, sizeof(struct rte_eth_conf));

	if (rte_eth_dev_configure(port, 1, 2, &null_conf) < 0) {
		printf("Configure failed for port %d\n", port);
		return -1;
	}

	/* Test queue release */
	if (rte_eth_dev_configure(port, 1, 1, &null_conf) < 0) {
		printf("Configure failed for port %d\n", port);
		return -1;
	}

	if (rte_eth_tx_queue_setup(port, 0, RING_SIZE, SOCKET0, NULL) < 0) {
		printf("TX queue setup failed port %d\n", port);
		return -1;
	}

	if (rte_eth_rx_queue_setup(port, 0, RING_SIZE, SOCKET0,
			NULL, mp) < 0) {
		printf("RX queue setup failed port %d\n", port);
		return -1;
	}

	if (rte_eth_dev_start(port) < 0) {
		printf("Error starting port %d\n", port);
		return -1;
	}

	rte_eth_link_get(port, &link);

	return 0;
}

static int
test_send_basic_packets(void)
{
	struct rte_mbuf  bufs[RING_SIZE];
	struct rte_mbuf *pbufs[RING_SIZE];
	int i;

	printf("Testing send and receive RING_SIZE/2 packets (tx_porta -> rx_portb)\n");

	for (i = 0; i < RING_SIZE/2; i++)
		pbufs[i] = &bufs[i];

	if (rte_eth_tx_burst(tx_porta, 0, pbufs, RING_SIZE/2) < RING_SIZE/2) {
		printf("Failed to transmit packet burst port %d\n", tx_porta);
		return -1;
	}

	if (rte_eth_rx_burst(rx_portb, 0, pbufs, RING_SIZE) != RING_SIZE/2) {
		printf("Failed to receive packet burst on port %d\n", rx_portb);
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
test_send_basic_packets_port(int port)
{
	struct rte_mbuf  bufs[RING_SIZE];
	struct rte_mbuf *pbufs[RING_SIZE];
	int i;

	printf("Testing send and receive RING_SIZE/2 packets (cmdl_port0 -> cmdl_port0)\n");

	for (i = 0; i < RING_SIZE/2; i++)
		pbufs[i] = &bufs[i];

	if (rte_eth_tx_burst(port, 0, pbufs, RING_SIZE/2) < RING_SIZE/2) {
		printf("Failed to transmit packet burst port %d\n", port);
		return -1;
	}

	if (rte_eth_rx_burst(port, 0, pbufs, RING_SIZE) != RING_SIZE/2) {
		printf("Failed to receive packet burst on port %d\n", port);
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
test_get_stats(int port)
{
	struct rte_eth_stats stats;
	struct rte_mbuf buf, *pbuf = &buf;

	printf("Testing ring PMD stats_get port %d\n", port);

	/* check stats of RXTX port, should all be zero */

	rte_eth_stats_get(port, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", port);
		return -1;
	}

	/* send and receive 1 packet and check for stats update */
	if (rte_eth_tx_burst(port, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", port);
		return -1;
	}

	if (rte_eth_rx_burst(port, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", port);
		return -1;
	}

	rte_eth_stats_get(port, &stats);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", port);
		return -1;
	}
	return 0;
}

static int
test_stats_reset(int port)
{
	struct rte_eth_stats stats;
	struct rte_mbuf buf, *pbuf = &buf;

	printf("Testing ring PMD stats_reset port %d\n", port);

	rte_eth_stats_reset(port);

	/* check stats of RXTX port, should all be zero */
	rte_eth_stats_get(port, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", port);
		return -1;
	}

	/* send and receive 1 packet and check for stats update */
	if (rte_eth_tx_burst(port, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", port);
		return -1;
	}

	if (rte_eth_rx_burst(port, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", port);
		return -1;
	}

	rte_eth_stats_get(port, &stats);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", port);
		return -1;
	}

	rte_eth_stats_reset(port);

	/* check stats of RXTX port, should all be zero */
	rte_eth_stats_get(port, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", port);
		return -1;
	}

	return 0;
}

static int
test_pmd_ring_pair_create_attach(int portd, int porte)
{
	struct rte_eth_stats stats, stats2;
	struct rte_mbuf buf, *pbuf = &buf;
	struct rte_eth_conf null_conf;

	if ((rte_eth_dev_configure(portd, 1, 1, &null_conf) < 0)
		|| (rte_eth_dev_configure(porte, 1, 1, &null_conf) < 0)) {
		printf("Configure failed for port\n");
		return -1;
	}

	if ((rte_eth_tx_queue_setup(portd, 0, RING_SIZE, SOCKET0, NULL) < 0)
		|| (rte_eth_tx_queue_setup(porte, 0, RING_SIZE, SOCKET0, NULL) < 0)) {
		printf("TX queue setup failed\n");
		return -1;
	}

	if ((rte_eth_rx_queue_setup(portd, 0, RING_SIZE, SOCKET0, NULL, mp) < 0)
		|| (rte_eth_rx_queue_setup(porte, 0, RING_SIZE, SOCKET0, NULL, mp) < 0)) {
		printf("RX queue setup failed\n");
		return -1;
	}

	if ((rte_eth_dev_start(portd) < 0)
		|| (rte_eth_dev_start(porte) < 0)) {
		printf("Error starting port\n");
		return -1;
	}

	rte_eth_stats_reset(portd);
	/* check stats of port, should all be zero */
	rte_eth_stats_get(portd, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", portd);
		return -1;
	}

	rte_eth_stats_reset(porte);
	/* check stats of port, should all be zero */
	rte_eth_stats_get(porte, &stats2);
	if (stats2.ipackets != 0 || stats2.opackets != 0 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", porte);
		return -1;
	}

	/*
	 * send and receive 1 packet (portd -> porte)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet (portd -> porte)\n");
	if (rte_eth_tx_burst(portd, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", portd);
		return -1;
	}

	if (rte_eth_rx_burst(porte, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", porte);
		return -1;
	}

	rte_eth_stats_get(portd, &stats);
	rte_eth_stats_get(porte, &stats2);
	if (stats.ipackets != 0 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", portd);
		return -1;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 0 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", porte);
		return -1;
	}

	/*
	 * send and receive 1 packet (porte -> portd)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet (porte -> portd)\n");
	if (rte_eth_tx_burst(porte, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", porte);
		return -1;
	}

	if (rte_eth_rx_burst(portd, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", portd);
		return -1;
	}

	rte_eth_stats_get(portd, &stats);
	rte_eth_stats_get(porte, &stats2);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", portd);
		return -1;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 1 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", porte);
		return -1;
	}

	/*
	 * send and receive 1 packet (portd -> portd)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet (portd -> portd)\n");
	if (rte_eth_tx_burst(portd, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", portd);
		return -1;
	}

	if (rte_eth_rx_burst(portd, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", porte);
		return -1;
	}

	rte_eth_stats_get(portd, &stats);
	rte_eth_stats_get(porte, &stats2);
	if (stats.ipackets != 2 || stats.opackets != 2 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", portd);
		return -1;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 1 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", porte);
		return -1;
	}

	/*
	 * send and receive 1 packet (porte -> porte)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet (porte -> porte)\n");
	if (rte_eth_tx_burst(porte, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", porte);
		return -1;
	}

	if (rte_eth_rx_burst(porte, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", porte);
		return -1;
	}

	rte_eth_stats_get(portd, &stats);
	rte_eth_stats_get(porte, &stats2);
	if (stats.ipackets != 2 || stats.opackets != 2 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", portd);
		return -1;
	}

	if (stats2.ipackets != 2 || stats2.opackets != 2 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n", porte);
		return -1;
	}

	rte_eth_dev_stop(portd);
	rte_eth_dev_stop(porte);

	return 0;
}

static int
test_pmd_ring(void)
{
	struct rte_ring *rxtx[NUM_RINGS];
	int port, cmdl_port0 = -1;
	uint8_t nb_ports;

	nb_ports = rte_eth_dev_count();
	printf("nb_ports=%d\n", (int)nb_ports);

	/*  create the rings and eth_rings in the test code.
	 *  This does not test the rte_pmd_ring_devinit function.
	 *
	 *  Test with the command line option --vdev=eth_ring0 to test rte_pmd_ring_devinit.
	 */
	rxtx[0] = rte_ring_create("R0", RING_SIZE, SOCKET0, RING_F_SP_ENQ|RING_F_SC_DEQ);
	if (rxtx[0] == NULL) {
		printf("rte_ring_create R0 failed");
		return -1;
	}

	rxtx[1] = rte_ring_create("R1", RING_SIZE, SOCKET0, RING_F_SP_ENQ|RING_F_SC_DEQ);
	if (rxtx[1] == NULL) {
		printf("rte_ring_create R1 failed");
		return -1;
	}

	tx_porta = rte_eth_from_rings("eth_ringa", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rx_portb = rte_eth_from_rings("eth_ringb", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rxtx_portc = rte_eth_from_rings("eth_ringc", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rxtx_portd = rte_eth_from_rings("eth_ringd", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rxtx_porte = rte_eth_from_rings("eth_ringe", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);

	printf("tx_porta=%d rx_portb=%d rxtx_portc=%d rxtx_portd=%d rxtx_porte=%d\n",
			tx_porta, rx_portb, rxtx_portc, rxtx_portd, rxtx_porte);

	if ((tx_porta == -1) || (rx_portb == -1) || (rxtx_portc == -1)
		|| (rxtx_portd == -1) || (rxtx_porte == -1)) {
		printf("rte_eth_from rings failed\n");
		return -1;
	}

	mp = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, 32,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mp == NULL)
		return -1;

	if ((tx_porta >= RTE_MAX_ETHPORTS) || (rx_portb >= RTE_MAX_ETHPORTS)
		|| (rxtx_portc >= RTE_MAX_ETHPORTS)
		|| (rxtx_portd >= RTE_MAX_ETHPORTS)
		|| (rxtx_porte >= RTE_MAX_ETHPORTS)) {
		printf(" port exceed max eth ports\n");
		return -1;
	}

	if (test_ethdev_configure_port(tx_porta) < 0)
		return -1;

	if (test_ethdev_configure_port(rx_portb) < 0)
		return -1;

	if (test_ethdev_configure_port(rxtx_portc) < 0)
		return -1;

	if (test_send_basic_packets() < 0)
		return -1;

	if (test_get_stats(rxtx_portc) < 0)
		return -1;

	if (test_stats_reset(rxtx_portc) < 0)
		return -1;

	rte_eth_dev_stop(tx_porta);
	rte_eth_dev_stop(rx_portb);
	rte_eth_dev_stop(rxtx_portc);

	if (test_pmd_ring_pair_create_attach(rxtx_portd, rxtx_porte) < 0)
		return -1;

	/* find a port created with the --vdev=eth_ring0 command line option */
	for (port = 0; port < nb_ports; port++) {
		struct rte_eth_dev_info dev_info;

		rte_eth_dev_info_get(port, &dev_info);
		if (!strcmp(dev_info.driver_name, "Rings PMD")) {
			printf("found a command line ring port=%d\n", port);
			cmdl_port0 = port;
			break;
		}
	}
	if (cmdl_port0 != -1) {
		if (test_ethdev_configure_port(cmdl_port0) < 0)
			return -1;
		if (test_send_basic_packets_port(cmdl_port0) < 0)
			return -1;
		if (test_stats_reset(cmdl_port0) < 0)
			return -1;
		if (test_get_stats(cmdl_port0) < 0)
			return -1;
		rte_eth_dev_stop(cmdl_port0);
	}
	return 0;
}

REGISTER_TEST_COMMAND(ring_pmd_autotest, test_pmd_ring);
