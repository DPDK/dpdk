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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>

#include "main.h"

int
MAIN(int argc, char **argv)
{
	int ret;

	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;
	argc -= ret;
	argv += ret;

	/* Parse application arguments (after the EAL ones) */
	ret = app_parse_args(argc, argv);
	if (ret < 0) {
		app_print_usage(argv[0]);
		return -1;
	}

	/* Init */
	app_init();

	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);

	return 0;
}

int
app_lcore_main_loop(__attribute__((unused)) void *arg)
{
	uint32_t core_id, i;

	core_id = rte_lcore_id();

	for (i = 0; i < app.n_cores; i++) {
		struct app_core_params *p = &app.cores[i];

		if (p->core_id != core_id)
			continue;

		switch (p->core_type) {
		case APP_CORE_MASTER:
			app_ping();
			app_main_loop_cmdline();
			return 0;
		case APP_CORE_RX:
			app_main_loop_pipeline_rx();
			/* app_main_loop_rx(); */
			return 0;
		case APP_CORE_TX:
			app_main_loop_pipeline_tx();
			/* app_main_loop_tx(); */
			return 0;
		case APP_CORE_PT:
			/* app_main_loop_pipeline_passthrough(); */
			app_main_loop_passthrough();
			return 0;
		case APP_CORE_FC:
			app_main_loop_pipeline_flow_classification();
			return 0;
		case APP_CORE_FW:
		case APP_CORE_RT:
			app_main_loop_pipeline_routing();
			return 0;

#ifdef RTE_LIBRTE_ACL
			app_main_loop_pipeline_firewall();
			return 0;
#else
			rte_exit(EXIT_FAILURE, "ACL not present in build\n");
#endif

#ifdef RTE_MBUF_SCATTER_GATHER
		case APP_CORE_IPV4_FRAG:
			app_main_loop_pipeline_ipv4_frag();
			return 0;
		case APP_CORE_IPV4_RAS:
			app_main_loop_pipeline_ipv4_ras();
			return 0;
#else
			rte_exit(EXIT_FAILURE,
				"mbuf chaining not present in build\n");
#endif

		default:
			rte_panic("%s: Invalid core type for core %u\n",
				__func__, i);
		}
	}

	rte_panic("%s: Algorithmic error\n", __func__);
	return -1;
}
