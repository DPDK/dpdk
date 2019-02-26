/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
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
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>

#include "main.h"

int
main(int argc, char **argv)
{
	uint32_t lcore;
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
		app_print_usage();
		return -1;
	}

	/* Init */
	app_init();

	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0)
			return -1;
	}

	return 0;
}

int
app_lcore_main_loop(__attribute__((unused)) void *arg)
{
	unsigned lcore;

	lcore = rte_lcore_id();

	if (lcore == app.core_rx) {
		switch (app.pipeline_type) {
		case e_APP_PIPELINE_ACL:
			app_main_loop_rx();
			return 0;

		default:
			app_main_loop_rx_metadata();
			return 0;
		}
	}

	if (lcore == app.core_worker) {
		switch (app.pipeline_type) {
		case e_APP_PIPELINE_STUB:
			app_main_loop_worker_pipeline_stub();
			return 0;

		case e_APP_PIPELINE_HASH_KEY8_EXT:
		case e_APP_PIPELINE_HASH_KEY8_LRU:
		case e_APP_PIPELINE_HASH_KEY16_EXT:
		case e_APP_PIPELINE_HASH_KEY16_LRU:
		case e_APP_PIPELINE_HASH_KEY32_EXT:
		case e_APP_PIPELINE_HASH_KEY32_LRU:
		case e_APP_PIPELINE_HASH_SPEC_KEY8_EXT:
		case e_APP_PIPELINE_HASH_SPEC_KEY8_LRU:
		case e_APP_PIPELINE_HASH_SPEC_KEY16_EXT:
		case e_APP_PIPELINE_HASH_SPEC_KEY16_LRU:
		case e_APP_PIPELINE_HASH_SPEC_KEY32_EXT:
		case e_APP_PIPELINE_HASH_SPEC_KEY32_LRU:
		/* cases for cuckoo hash table types */
		case e_APP_PIPELINE_HASH_CUCKOO_KEY8:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY16:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY32:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY48:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY64:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY80:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY96:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY112:
		case e_APP_PIPELINE_HASH_CUCKOO_KEY128:
			app_main_loop_worker_pipeline_hash();
			return 0;

		case e_APP_PIPELINE_ACL:
#ifndef RTE_LIBRTE_ACL
			rte_exit(EXIT_FAILURE, "ACL not present in build\n");
#else
			app_main_loop_worker_pipeline_acl();
			return 0;
#endif

		case e_APP_PIPELINE_LPM:
			app_main_loop_worker_pipeline_lpm();
			return 0;

		case e_APP_PIPELINE_LPM_IPV6:
			app_main_loop_worker_pipeline_lpm_ipv6();
			return 0;

		case e_APP_PIPELINE_NONE:
		default:
			app_main_loop_worker();
			return 0;
		}
	}

	if (lcore == app.core_tx) {
		app_main_loop_tx();
		return 0;
	}

	return 0;
}
