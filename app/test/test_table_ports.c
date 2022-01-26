/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef RTE_EXEC_ENV_WINDOWS

#include "test_table_ports.h"
#include "test_table.h"

port_test port_tests[] = {
	test_port_ring_reader,
	test_port_ring_writer,
};

unsigned n_port_tests = RTE_DIM(port_tests);

/* Port tests */
int
test_port_ring_reader(void)
{
	int status, i;
	struct rte_port_ring_reader_params port_ring_reader_params;
	void *port;

	/* Invalid params */
	port = rte_port_ring_reader_ops.f_create(NULL, 0);
	if (port != NULL)
		return -1;

	status = rte_port_ring_reader_ops.f_free(port);
	if (status >= 0)
		return -2;

	/* Create and free */
	port_ring_reader_params.ring = RING_RX;
	port = rte_port_ring_reader_ops.f_create(&port_ring_reader_params, 0);
	if (port == NULL)
		return -3;

	status = rte_port_ring_reader_ops.f_free(port);
	if (status != 0)
		return -4;

	/* -- Traffic RX -- */
	int expected_pkts, received_pkts;
	struct rte_mbuf *res_mbuf[RTE_PORT_IN_BURST_SIZE_MAX];
	void *mbuf[RTE_PORT_IN_BURST_SIZE_MAX];

	port_ring_reader_params.ring = RING_RX;
	port = rte_port_ring_reader_ops.f_create(&port_ring_reader_params, 0);

	/* Single packet */
	mbuf[0] = (void *)rte_pktmbuf_alloc(pool);

	expected_pkts = rte_ring_sp_enqueue_burst(port_ring_reader_params.ring,
		mbuf, 1, NULL);
	received_pkts = rte_port_ring_reader_ops.f_rx(port, res_mbuf, 1);

	if (received_pkts < expected_pkts)
		return -5;

	rte_pktmbuf_free(res_mbuf[0]);

	/* Multiple packets */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		mbuf[i] = rte_pktmbuf_alloc(pool);

	expected_pkts = rte_ring_sp_enqueue_burst(port_ring_reader_params.ring,
		(void * const *) mbuf, RTE_PORT_IN_BURST_SIZE_MAX, NULL);
	received_pkts = rte_port_ring_reader_ops.f_rx(port, res_mbuf,
		RTE_PORT_IN_BURST_SIZE_MAX);

	if (received_pkts < expected_pkts)
		return -6;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(res_mbuf[i]);

	return 0;
}

int
test_port_ring_writer(void)
{
	int status, i;
	struct rte_port_ring_writer_params port_ring_writer_params;
	void *port;

	/* Invalid params */
	port = rte_port_ring_writer_ops.f_create(NULL, 0);
	if (port != NULL)
		return -1;

	status = rte_port_ring_writer_ops.f_free(port);
	if (status >= 0)
		return -2;

	port_ring_writer_params.ring = NULL;

	port = rte_port_ring_writer_ops.f_create(&port_ring_writer_params, 0);
	if (port != NULL)
		return -3;

	port_ring_writer_params.ring = RING_TX;
	port_ring_writer_params.tx_burst_sz = RTE_PORT_IN_BURST_SIZE_MAX + 1;

	port = rte_port_ring_writer_ops.f_create(&port_ring_writer_params, 0);
	if (port != NULL)
		return -4;

	/* Create and free */
	port_ring_writer_params.ring = RING_TX;
	port_ring_writer_params.tx_burst_sz = RTE_PORT_IN_BURST_SIZE_MAX;

	port = rte_port_ring_writer_ops.f_create(&port_ring_writer_params, 0);
	if (port == NULL)
		return -5;

	status = rte_port_ring_writer_ops.f_free(port);
	if (status != 0)
		return -6;

	/* -- Traffic TX -- */
	int expected_pkts, received_pkts;
	struct rte_mbuf *mbuf[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_mbuf *res_mbuf[RTE_PORT_IN_BURST_SIZE_MAX];

	port_ring_writer_params.ring = RING_TX;
	port_ring_writer_params.tx_burst_sz = RTE_PORT_IN_BURST_SIZE_MAX;
	port = rte_port_ring_writer_ops.f_create(&port_ring_writer_params, 0);

	/* Single packet */
	mbuf[0] = rte_pktmbuf_alloc(pool);

	rte_port_ring_writer_ops.f_tx(port, mbuf[0]);
	rte_port_ring_writer_ops.f_flush(port);
	expected_pkts = 1;
	received_pkts = rte_ring_sc_dequeue_burst(port_ring_writer_params.ring,
		(void **)res_mbuf, port_ring_writer_params.tx_burst_sz, NULL);

	if (received_pkts < expected_pkts)
		return -7;

	rte_pktmbuf_free(res_mbuf[0]);

	/* Multiple packets */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++) {
		mbuf[i] = rte_pktmbuf_alloc(pool);
		rte_port_ring_writer_ops.f_tx(port, mbuf[i]);
	}

	expected_pkts = RTE_PORT_IN_BURST_SIZE_MAX;
	received_pkts = rte_ring_sc_dequeue_burst(port_ring_writer_params.ring,
		(void **)res_mbuf, port_ring_writer_params.tx_burst_sz, NULL);

	if (received_pkts < expected_pkts)
		return -8;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(res_mbuf[i]);

	/* TX Bulk */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		mbuf[i] = rte_pktmbuf_alloc(pool);
	rte_port_ring_writer_ops.f_tx_bulk(port, mbuf, (uint64_t)-1);

	expected_pkts = RTE_PORT_IN_BURST_SIZE_MAX;
	received_pkts = rte_ring_sc_dequeue_burst(port_ring_writer_params.ring,
		(void **)res_mbuf, port_ring_writer_params.tx_burst_sz, NULL);

	if (received_pkts < expected_pkts)
		return -8;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(res_mbuf[i]);

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		mbuf[i] = rte_pktmbuf_alloc(pool);
	rte_port_ring_writer_ops.f_tx_bulk(port, mbuf, (uint64_t)-3);
	rte_port_ring_writer_ops.f_tx_bulk(port, mbuf, (uint64_t)2);

	expected_pkts = RTE_PORT_IN_BURST_SIZE_MAX;
	received_pkts = rte_ring_sc_dequeue_burst(port_ring_writer_params.ring,
		(void **)res_mbuf, port_ring_writer_params.tx_burst_sz, NULL);

	if (received_pkts < expected_pkts)
		return -9;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(res_mbuf[i]);

	return 0;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */
