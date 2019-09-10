/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */
#include "test.h"
#include <string.h>

#include <stdio.h>

#include <rte_eth_ring.h>
#include <rte_ethdev.h>
#include <rte_bus_vdev.h>

#define SOCKET0 0
#define RING_SIZE 256
#define NUM_RINGS 2
#define NB_MBUF 512

static struct rte_mempool *mp;
struct rte_ring *rxtx[NUM_RINGS];
static int tx_porta, rx_portb, rxtx_portc, rxtx_portd, rxtx_porte;

static int
test_ethdev_configure_port(int port)
{
	struct rte_eth_conf null_conf;
	struct rte_eth_link link;
	int ret;

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

	ret = rte_eth_link_get(port, &link);
	if (ret < 0) {
		printf("Link get failed for port %u: %s",
		       port, rte_strerror(-ret));
		return -1;
	}

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
		return TEST_FAILED;
	}

	if (rte_eth_rx_burst(rx_portb, 0, pbufs, RING_SIZE) != RING_SIZE/2) {
		printf("Failed to receive packet burst on port %d\n", rx_portb);
		return TEST_FAILED;
	}

	for (i = 0; i < RING_SIZE/2; i++)
		if (pbufs[i] != &bufs[i]) {
			printf("Error: received data does not match that transmitted\n");
			return TEST_FAILED;
		}

	return TEST_SUCCESS;
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
test_pmd_ring_pair_create_attach(void)
{
	struct rte_eth_stats stats, stats2;
	struct rte_mbuf buf, *pbuf = &buf;
	struct rte_eth_conf null_conf;

	memset(&null_conf, 0, sizeof(struct rte_eth_conf));

	if ((rte_eth_dev_configure(rxtx_portd, 1, 1, &null_conf) < 0)
			|| (rte_eth_dev_configure(rxtx_porte, 1, 1,
					&null_conf) < 0)) {
		printf("Configure failed for port\n");
		return TEST_FAILED;
	}

	if ((rte_eth_tx_queue_setup(rxtx_portd, 0, RING_SIZE,
					SOCKET0, NULL) < 0)
			|| (rte_eth_tx_queue_setup(rxtx_porte, 0, RING_SIZE,
					SOCKET0, NULL) < 0)) {
		printf("TX queue setup failed\n");
		return TEST_FAILED;
	}

	if ((rte_eth_rx_queue_setup(rxtx_portd, 0, RING_SIZE,
					SOCKET0, NULL, mp) < 0)
			|| (rte_eth_rx_queue_setup(rxtx_porte, 0, RING_SIZE,
					SOCKET0, NULL, mp) < 0)) {
		printf("RX queue setup failed\n");
		return TEST_FAILED;
	}

	if ((rte_eth_dev_start(rxtx_portd) < 0)
			|| (rte_eth_dev_start(rxtx_porte) < 0)) {
		printf("Error starting port\n");
		return TEST_FAILED;
	}

	rte_eth_stats_reset(rxtx_portd);
	/* check stats of port, should all be zero */
	rte_eth_stats_get(rxtx_portd, &stats);
	if (stats.ipackets != 0 || stats.opackets != 0 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", rxtx_portd);
		return TEST_FAILED;
	}

	rte_eth_stats_reset(rxtx_porte);
	/* check stats of port, should all be zero */
	rte_eth_stats_get(rxtx_porte, &stats2);
	if (stats2.ipackets != 0 || stats2.opackets != 0 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not zero\n", rxtx_porte);
		return TEST_FAILED;
	}

	/*
	 * send and receive 1 packet (rxtx_portd -> rxtx_porte)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet (rxtx_portd -> rxtx_porte)\n");
	if (rte_eth_tx_burst(rxtx_portd, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", rxtx_portd);
		return TEST_FAILED;
	}

	if (rte_eth_rx_burst(rxtx_porte, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", rxtx_porte);
		return TEST_FAILED;
	}

	rte_eth_stats_get(rxtx_portd, &stats);
	rte_eth_stats_get(rxtx_porte, &stats2);
	if (stats.ipackets != 0 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_portd);
		return TEST_FAILED;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 0 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_porte);
		return TEST_FAILED;
	}

	/*
	 * send and receive 1 packet (rxtx_porte -> rxtx_portd)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet "
			"(rxtx_porte -> rxtx_portd)\n");
	if (rte_eth_tx_burst(rxtx_porte, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", rxtx_porte);
		return TEST_FAILED;
	}

	if (rte_eth_rx_burst(rxtx_portd, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", rxtx_portd);
		return TEST_FAILED;
	}

	rte_eth_stats_get(rxtx_portd, &stats);
	rte_eth_stats_get(rxtx_porte, &stats2);
	if (stats.ipackets != 1 || stats.opackets != 1 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_portd);
		return TEST_FAILED;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 1 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_porte);
		return TEST_FAILED;
	}

	/*
	 * send and receive 1 packet (rxtx_portd -> rxtx_portd)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet "
			"(rxtx_portd -> rxtx_portd)\n");
	if (rte_eth_tx_burst(rxtx_portd, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", rxtx_portd);
		return TEST_FAILED;
	}

	if (rte_eth_rx_burst(rxtx_portd, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", rxtx_porte);
		return TEST_FAILED;
	}

	rte_eth_stats_get(rxtx_portd, &stats);
	rte_eth_stats_get(rxtx_porte, &stats2);
	if (stats.ipackets != 2 || stats.opackets != 2 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_portd);
		return TEST_FAILED;
	}

	if (stats2.ipackets != 1 || stats2.opackets != 1 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_porte);
		return TEST_FAILED;
	}

	/*
	 * send and receive 1 packet (rxtx_porte -> rxtx_porte)
	 * and check for stats update
	 */
	printf("Testing send and receive 1 packet "
			"(rxtx_porte -> rxtx_porte)\n");
	if (rte_eth_tx_burst(rxtx_porte, 0, &pbuf, 1) != 1) {
		printf("Error sending packet to port %d\n", rxtx_porte);
		return TEST_FAILED;
	}

	if (rte_eth_rx_burst(rxtx_porte, 0, &pbuf, 1) != 1) {
		printf("Error receiving packet from port %d\n", rxtx_porte);
		return TEST_FAILED;
	}

	rte_eth_stats_get(rxtx_portd, &stats);
	rte_eth_stats_get(rxtx_porte, &stats2);
	if (stats.ipackets != 2 || stats.opackets != 2 ||
			stats.ibytes != 0 || stats.obytes != 0 ||
			stats.ierrors != 0 || stats.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_portd);
		return TEST_FAILED;
	}

	if (stats2.ipackets != 2 || stats2.opackets != 2 ||
			stats2.ibytes != 0 || stats2.obytes != 0 ||
			stats2.ierrors != 0 || stats2.oerrors != 0) {
		printf("Error: port %d stats are not as expected\n",
				rxtx_porte);
		return TEST_FAILED;
	}

	rte_eth_dev_stop(rxtx_portd);
	rte_eth_dev_stop(rxtx_porte);

	return TEST_SUCCESS;
}

static void
test_cleanup_resources(void)
{
	int itr;
	for (itr = 0; itr < NUM_RINGS; itr++)
		rte_ring_free(rxtx[itr]);

	rte_eth_dev_stop(tx_porta);
	rte_eth_dev_stop(rx_portb);
	rte_eth_dev_stop(rxtx_portc);

	rte_mempool_free(mp);
	rte_vdev_uninit("net_ring_net_ringa");
	rte_vdev_uninit("net_ring_net_ringb");
	rte_vdev_uninit("net_ring_net_ringc");
	rte_vdev_uninit("net_ring_net_ringd");
	rte_vdev_uninit("net_ring_net_ringe");
}

static int
test_pmd_ringcreate_setup(void)
{
	uint8_t nb_ports;

	nb_ports = rte_eth_dev_count_avail();
	printf("nb_ports=%d\n", (int)nb_ports);

	/*  create the rings and eth_rings in the test code.
	 *  This does not test the rte_pmd_ring_devinit function.
	 *
	 *  Test with the command line option --vdev=net_ring0 to test rte_pmd_ring_devinit.
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

	tx_porta = rte_eth_from_rings("net_ringa", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rx_portb = rte_eth_from_rings("net_ringb", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rxtx_portc = rte_eth_from_rings("net_ringc", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rxtx_portd = rte_eth_from_rings("net_ringd", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);
	rxtx_porte = rte_eth_from_rings("net_ringe", rxtx, NUM_RINGS, rxtx, NUM_RINGS, SOCKET0);

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
	return 0;
}

static int
test_command_line_ring_port(void)
{
	int port, cmdl_port0 = -1;
	int ret;

	/* find a port created with the --vdev=net_ring0 command line option */
	RTE_ETH_FOREACH_DEV(port) {
		struct rte_eth_dev_info dev_info;

		ret = rte_eth_dev_info_get(port, &dev_info);
		TEST_ASSERT((ret == 0),
				"Error during getting device (port %d) info: %s\n",
				port, strerror(-ret));

		if (!strcmp(dev_info.driver_name, "Rings PMD")) {
			printf("found a command line ring port=%d\n", port);
			cmdl_port0 = port;
			break;
		}
	}
	if (cmdl_port0 != -1) {
		TEST_ASSERT((test_ethdev_configure_port(cmdl_port0) < 0),
				"test ethdev configure port cmdl_port0 is failed");
		TEST_ASSERT((test_send_basic_packets_port(cmdl_port0) < 0),
				"test send basic packets port cmdl_port0 is failed");
		TEST_ASSERT((test_stats_reset(cmdl_port0) < 0),
				"test stats reset cmdl_port0 is failed");
		TEST_ASSERT((test_get_stats(cmdl_port0) < 0),
				"test get stats cmdl_port0 is failed");
		rte_eth_dev_stop(cmdl_port0);
	}
	return TEST_SUCCESS;
}

static int
test_ethdev_configure_ports(void)
{
	TEST_ASSERT((test_ethdev_configure_port(tx_porta) == 0),
			"test ethdev configure ports tx_porta is failed");
	TEST_ASSERT((test_ethdev_configure_port(rx_portb) == 0),
			"test ethdev configure ports rx_portb is failed");
	TEST_ASSERT((test_ethdev_configure_port(rxtx_portc) == 0),
			"test ethdev configure ports rxtx_portc is failed");

	return TEST_SUCCESS;
}

static int
test_get_stats_for_port(void)
{
	TEST_ASSERT(test_get_stats(rxtx_portc) == 0, "test get stats failed");
	return TEST_SUCCESS;
}

static int
test_stats_reset_for_port(void)
{
	TEST_ASSERT(test_stats_reset(rxtx_portc) == 0, "test stats reset failed");
	return TEST_SUCCESS;
}

static struct
unit_test_suite test_pmd_ring_suite  = {
	.setup = test_pmd_ringcreate_setup,
	.teardown = test_cleanup_resources,
	.suite_name = "Test Pmd Ring Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE(test_ethdev_configure_ports),
		TEST_CASE(test_send_basic_packets),
		TEST_CASE(test_get_stats_for_port),
		TEST_CASE(test_stats_reset_for_port),
		TEST_CASE(test_pmd_ring_pair_create_attach),
		TEST_CASE(test_command_line_ring_port),
		TEST_CASES_END()
	}
};

static int
test_pmd_ring(void)
{
	return unit_test_suite_runner(&test_pmd_ring_suite);
}

REGISTER_TEST_COMMAND(ring_pmd_autotest, test_pmd_ring);
