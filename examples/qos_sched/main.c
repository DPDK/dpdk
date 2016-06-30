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

#include <unistd.h>
#include <stdint.h>

#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_branch_prediction.h>

#include <rte_sched.h>

#include "main.h"

#define APP_MODE_NONE 0
#define APP_RX_MODE   1
#define APP_WT_MODE   2
#define APP_TX_MODE   4

uint8_t interactive = APP_INTERACTIVE_DEFAULT;
uint32_t qavg_period = APP_QAVG_PERIOD;
uint32_t qavg_ntimes = APP_QAVG_NTIMES;

/* main processing loop */
static int
app_main_loop(__attribute__((unused))void *dummy)
{
	uint32_t lcore_id;
	uint32_t i, mode;
	uint32_t rx_idx = 0;
	uint32_t wt_idx = 0;
	uint32_t tx_idx = 0;
	struct thread_conf *rx_confs[MAX_DATA_STREAMS];
	struct thread_conf *wt_confs[MAX_DATA_STREAMS];
	struct thread_conf *tx_confs[MAX_DATA_STREAMS];

	memset(rx_confs, 0, sizeof(rx_confs));
	memset(wt_confs, 0, sizeof(wt_confs));
	memset(tx_confs, 0, sizeof(tx_confs));


	mode = APP_MODE_NONE;
	lcore_id = rte_lcore_id();

	for (i = 0; i < nb_pfc; i++) {
		struct flow_conf *flow = &qos_conf[i];

		if (flow->rx_core == lcore_id) {
			flow->rx_thread.rx_port = flow->rx_port;
			flow->rx_thread.rx_ring =  flow->rx_ring;
			flow->rx_thread.rx_queue = flow->rx_queue;

			rx_confs[rx_idx++] = &flow->rx_thread;

			mode |= APP_RX_MODE;
		}
		if (flow->tx_core == lcore_id) {
			flow->tx_thread.tx_port = flow->tx_port;
			flow->tx_thread.tx_ring =  flow->tx_ring;
			flow->tx_thread.tx_queue = flow->tx_queue;

			tx_confs[tx_idx++] = &flow->tx_thread;

			mode |= APP_TX_MODE;
		}
		if (flow->wt_core == lcore_id) {
			flow->wt_thread.rx_ring =  flow->rx_ring;
			flow->wt_thread.tx_ring =  flow->tx_ring;
			flow->wt_thread.tx_port =  flow->tx_port;
			flow->wt_thread.sched_port =  flow->sched_port;

			wt_confs[wt_idx++] = &flow->wt_thread;

			mode |= APP_WT_MODE;
		}
	}

	if (mode == APP_MODE_NONE) {
		RTE_LOG(INFO, APP, "lcore %u has nothing to do\n", lcore_id);
		return -1;
	}

	if (mode == (APP_RX_MODE | APP_WT_MODE)) {
		RTE_LOG(INFO, APP, "lcore %u was configured for both RX and WT !!!\n",
				 lcore_id);
		return -1;
	}

	RTE_LOG(INFO, APP, "entering main loop on lcore %u\n", lcore_id);
	/* initialize mbuf memory */
	if (mode == APP_RX_MODE) {
		for (i = 0; i < rx_idx; i++) {
			RTE_LOG(INFO, APP, "flow %u lcoreid %u "
					"reading port %"PRIu8"\n",
					i, lcore_id, rx_confs[i]->rx_port);
		}

		app_rx_thread(rx_confs);
	}
	else if (mode == (APP_TX_MODE | APP_WT_MODE)) {
		for (i = 0; i < wt_idx; i++) {
			wt_confs[i]->m_table = rte_malloc("table_wt", sizeof(struct rte_mbuf *)
					* burst_conf.tx_burst, RTE_CACHE_LINE_SIZE);

			if (wt_confs[i]->m_table == NULL)
				rte_panic("flow %u unable to allocate memory buffer\n", i);

			RTE_LOG(INFO, APP, "flow %u lcoreid %u sched+write "
					"port %"PRIu8"\n",
					i, lcore_id, wt_confs[i]->tx_port);
		}

		app_mixed_thread(wt_confs);
	}
	else if (mode == APP_TX_MODE) {
		for (i = 0; i < tx_idx; i++) {
			tx_confs[i]->m_table = rte_malloc("table_tx", sizeof(struct rte_mbuf *)
					* burst_conf.tx_burst, RTE_CACHE_LINE_SIZE);

			if (tx_confs[i]->m_table == NULL)
				rte_panic("flow %u unable to allocate memory buffer\n", i);

			RTE_LOG(INFO, APP, "flow %u lcoreid %u "
					"writing port %"PRIu8"\n",
					i, lcore_id, tx_confs[i]->tx_port);
		}

		app_tx_thread(tx_confs);
	}
	else if (mode == APP_WT_MODE){
		for (i = 0; i < wt_idx; i++) {
			RTE_LOG(INFO, APP, "flow %u lcoreid %u scheduling \n", i, lcore_id);
		}

		app_worker_thread(wt_confs);
	}

	return 0;
}

void
app_stat(void)
{
	uint32_t i;
	struct rte_eth_stats stats;
	static struct rte_eth_stats rx_stats[MAX_DATA_STREAMS];
	static struct rte_eth_stats tx_stats[MAX_DATA_STREAMS];

	/* print statistics */
	for(i = 0; i < nb_pfc; i++) {
		struct flow_conf *flow = &qos_conf[i];

		rte_eth_stats_get(flow->rx_port, &stats);
		printf("\nRX port %"PRIu8": rx: %"PRIu64 " err: %"PRIu64
				" no_mbuf: %"PRIu64 "\n",
				flow->rx_port,
				stats.ipackets - rx_stats[i].ipackets,
				stats.ierrors - rx_stats[i].ierrors,
				stats.rx_nombuf - rx_stats[i].rx_nombuf);
		memcpy(&rx_stats[i], &stats, sizeof(stats));

		rte_eth_stats_get(flow->tx_port, &stats);
		printf("TX port %"PRIu8": tx: %" PRIu64 " err: %" PRIu64 "\n",
				flow->tx_port,
				stats.opackets - tx_stats[i].opackets,
				stats.oerrors - tx_stats[i].oerrors);
		memcpy(&tx_stats[i], &stats, sizeof(stats));

#if APP_COLLECT_STAT
		printf("-------+------------+------------+\n");
		printf("       |  received  |   dropped  |\n");
		printf("-------+------------+------------+\n");
		printf("  RX   | %10" PRIu64 " | %10" PRIu64 " |\n",
			flow->rx_thread.stat.nb_rx,
			flow->rx_thread.stat.nb_drop);
		printf("QOS+TX | %10" PRIu64 " | %10" PRIu64 " |   pps: %"PRIu64 " \n",
			flow->wt_thread.stat.nb_rx,
			flow->wt_thread.stat.nb_drop,
			flow->wt_thread.stat.nb_rx - flow->wt_thread.stat.nb_drop);
		printf("-------+------------+------------+\n");

		memset(&flow->rx_thread.stat, 0, sizeof(struct thread_stat));
		memset(&flow->wt_thread.stat, 0, sizeof(struct thread_stat));
#endif
	}
}

int
main(int argc, char **argv)
{
	int ret;

	ret = app_parse_args(argc, argv);
	if (ret < 0)
		return -1;

	ret = app_init();
	if (ret < 0)
		return -1;

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_main_loop, NULL, SKIP_MASTER);

	if (interactive) {
		sleep(1);
		prompt();
	}
	else {
		/* print statistics every second */
		while(1) {
			sleep(1);
			app_stat();
		}
	}

	return 0;
}
