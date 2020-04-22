/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#define RTE_TRACE_POINT_REGISTER_SELECT

#include <rte_ethdev_trace.h>

RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_configure);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_rxq_setup);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_txq_setup);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_start);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_stop);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_close);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_rx_burst);
RTE_TRACE_POINT_DEFINE(rte_ethdev_trace_tx_burst);

RTE_INIT(ethdev_trace_init)
{
	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_configure,
		lib.ethdev.configure);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rxq_setup,
		lib.ethdev.rxq.setup);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_txq_setup,
		lib.ethdev.txq.setup);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_start,
		lib.ethdev.start);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_stop,
		lib.ethdev.stop);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_close,
		lib.ethdev.close);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_burst,
		lib.ethdev.rx.burst);

	RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_tx_burst,
		lib.ethdev.tx.burst);
}
