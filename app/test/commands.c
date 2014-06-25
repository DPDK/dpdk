/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <termios.h>
#ifndef __linux__
#ifndef __FreeBSD__
#include <net/socket.h>
#endif
#endif
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_devargs.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>

#include "test.h"

/****************/

struct cmd_autotest_result {
	cmdline_fixed_string_t autotest;
};

static void cmd_autotest_parsed(void *parsed_result,
				__attribute__((unused)) struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_autotest_result *res = parsed_result;
	int ret = 0;

	if (!strcmp(res->autotest, "version_autotest"))
		ret = test_version();
	if (!strcmp(res->autotest, "eal_fs_autotest"))
		ret = test_eal_fs();
	if (!strcmp(res->autotest, "debug_autotest"))
		ret = test_debug();
	if (!strcmp(res->autotest, "pci_autotest"))
		ret = test_pci();
	if (!strcmp(res->autotest, "prefetch_autotest"))
		ret = test_prefetch();
	if (!strcmp(res->autotest, "byteorder_autotest"))
		ret = test_byteorder();
	if (!strcmp(res->autotest, "per_lcore_autotest"))
		ret = test_per_lcore();
	if (!strcmp(res->autotest, "atomic_autotest"))
		ret = test_atomic();
	if (!strcmp(res->autotest, "malloc_autotest"))
		ret = test_malloc();
	if (!strcmp(res->autotest, "spinlock_autotest"))
		ret = test_spinlock();
	if (!strcmp(res->autotest, "memory_autotest"))
		ret = test_memory();
	if (!strcmp(res->autotest, "memzone_autotest"))
		ret = test_memzone();
	if (!strcmp(res->autotest, "rwlock_autotest"))
		ret = test_rwlock();
	if (!strcmp(res->autotest, "mbuf_autotest"))
		ret = test_mbuf();
	if (!strcmp(res->autotest, "logs_autotest"))
		ret = test_logs();
	if (!strcmp(res->autotest, "errno_autotest"))
		ret = test_errno();
	if (!strcmp(res->autotest, "hash_autotest"))
		ret = test_hash();
	if (!strcmp(res->autotest, "hash_perf_autotest"))
		ret = test_hash_perf();
	if (!strcmp(res->autotest, "lpm_autotest"))
		ret = test_lpm();
	if (!strcmp(res->autotest, "lpm6_autotest"))
		ret = test_lpm6();
	if (!strcmp(res->autotest, "cpuflags_autotest"))
		ret = test_cpuflags();
	if (!strcmp(res->autotest, "cmdline_autotest"))
		ret = test_cmdline();
	if (!strcmp(res->autotest, "tailq_autotest"))
		ret = test_tailq();
	if (!strcmp(res->autotest, "multiprocess_autotest"))
		ret = test_mp_secondary();
	if (!strcmp(res->autotest, "memcpy_autotest"))
		ret = test_memcpy();
	if (!strcmp(res->autotest, "string_autotest"))
		ret = test_string_fns();
	if (!strcmp(res->autotest, "eal_flags_autotest"))
		ret = test_eal_flags();
	if (!strcmp(res->autotest, "alarm_autotest"))
		ret = test_alarm();
	if (!strcmp(res->autotest, "interrupt_autotest"))
		ret = test_interrupt();
	if (!strcmp(res->autotest, "cycles_autotest"))
		ret = test_cycles();
	if (!strcmp(res->autotest, "ring_autotest"))
		ret = test_ring();
	if (!strcmp(res->autotest, "table_autotest"))
		ret = test_table();
	if (!strcmp(res->autotest, "ring_perf_autotest"))
		ret = test_ring_perf();
	if (!strcmp(res->autotest, "timer_autotest"))
		ret = test_timer();
	if (!strcmp(res->autotest, "timer_perf_autotest"))
		ret = test_timer_perf();
#ifdef RTE_LIBRTE_PMD_BOND
	if (!strcmp(res->autotest, "link_bonding_autotest"))
		ret = test_link_bonding();
#endif
	if (!strcmp(res->autotest, "mempool_autotest"))
		ret = test_mempool();
	if (!strcmp(res->autotest, "mempool_perf_autotest"))
		ret = test_mempool_perf();
	if (!strcmp(res->autotest, "memcpy_perf_autotest"))
		ret = test_memcpy_perf();
	if (!strcmp(res->autotest, "func_reentrancy_autotest"))
		ret = test_func_reentrancy();
	if (!strcmp(res->autotest, "red_autotest"))
		ret = test_red();
	if (!strcmp(res->autotest, "sched_autotest"))
		ret = test_sched();
	if (!strcmp(res->autotest, "meter_autotest"))
		ret = test_meter();
	if (!strcmp(res->autotest, "kni_autotest"))
		ret = test_kni();
	if (!strcmp(res->autotest, "power_autotest"))
		ret = test_power();
	if (!strcmp(res->autotest, "common_autotest"))
		ret = test_common();
	if (!strcmp(res->autotest, "ivshmem_autotest"))
		ret = test_ivshmem();
	if (!strcmp(res->autotest, "distributor_autotest"))
		ret = test_distributor();
	if (!strcmp(res->autotest, "distributor_perf_autotest"))
		ret = test_distributor_perf();
	if (!strcmp(res->autotest, "devargs_autotest"))
		ret = test_devargs();
#ifdef RTE_LIBRTE_PMD_RING
	if (!strcmp(res->autotest, "ring_pmd_autotest"))
		ret = test_pmd_ring();
#endif /* RTE_LIBRTE_PMD_RING */

#ifdef RTE_LIBRTE_ACL
	if (!strcmp(res->autotest, "acl_autotest"))
		ret = test_acl();
#endif /* RTE_LIBRTE_ACL */
#ifdef RTE_LIBRTE_KVARGS
	if (!strcmp(res->autotest, "kvargs_autotest"))
		ret |= test_kvargs();
#endif /* RTE_LIBRTE_KVARGS */

	if (ret == 0)
		printf("Test OK\n");
	else
		printf("Test Failed\n");
	fflush(stdout);
}

cmdline_parse_token_string_t cmd_autotest_autotest =
	TOKEN_STRING_INITIALIZER(struct cmd_autotest_result, autotest,
			"pci_autotest#memory_autotest#"
			"per_lcore_autotest#spinlock_autotest#"
			"rwlock_autotest#atomic_autotest#"
			"byteorder_autotest#prefetch_autotest#"
			"cycles_autotest#logs_autotest#"
			"memzone_autotest#ring_autotest#"
			"mempool_autotest#mbuf_autotest#"
			"timer_autotest#malloc_autotest#"
			"memcpy_autotest#hash_autotest#"
			"lpm_autotest#debug_autotest#"
			"lpm6_autotest#"
			"errno_autotest#tailq_autotest#"
			"string_autotest#multiprocess_autotest#"
			"cpuflags_autotest#eal_flags_autotest#"
			"alarm_autotest#interrupt_autotest#"
			"version_autotest#eal_fs_autotest#"
			"cmdline_autotest#func_reentrancy_autotest#"
#ifdef RTE_LIBRTE_PMD_BOND
			"link_bonding_autotest#"
#endif
			"mempool_perf_autotest#hash_perf_autotest#"
			"memcpy_perf_autotest#ring_perf_autotest#"
			"red_autotest#meter_autotest#sched_autotest#"
			"memcpy_perf_autotest#kni_autotest#"
			"pm_autotest#ivshmem_autotest#"
			"devargs_autotest#table_autotest#"
#ifdef RTE_LIBRTE_ACL
			"acl_autotest#"
#endif
			"power_autotest#"
			"timer_perf_autotest#"
#ifdef RTE_LIBRTE_PMD_RING
			"ring_pmd_autotest#"
#endif
#ifdef RTE_LIBRTE_KVARGS
			"kvargs_autotest#"
#endif
			"common_autotest#"
			"distributor_autotest#distributor_perf_autotest");

cmdline_parse_inst_t cmd_autotest = {
	.f = cmd_autotest_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "launch autotest",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_autotest_autotest,
		NULL,
	},
};

/****************/

struct cmd_dump_result {
	cmdline_fixed_string_t dump;
};

static void
dump_struct_sizes(void)
{
#define DUMP_SIZE(t) printf("sizeof(" #t ") = %u\n", (unsigned)sizeof(t));
	DUMP_SIZE(struct rte_mbuf);
	DUMP_SIZE(struct rte_pktmbuf);
	DUMP_SIZE(struct rte_ctrlmbuf);
	DUMP_SIZE(struct rte_mempool);
	DUMP_SIZE(struct rte_ring);
#undef DUMP_SIZE
}

static void cmd_dump_parsed(void *parsed_result,
			    __attribute__((unused)) struct cmdline *cl,
			    __attribute__((unused)) void *data)
{
	struct cmd_dump_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_physmem"))
		rte_dump_physmem_layout(stdout);
	else if (!strcmp(res->dump, "dump_memzone"))
		rte_memzone_dump(stdout);
	else if (!strcmp(res->dump, "dump_log_history"))
		rte_log_dump_history(stdout);
	else if (!strcmp(res->dump, "dump_struct_sizes"))
		dump_struct_sizes();
	else if (!strcmp(res->dump, "dump_ring"))
		rte_ring_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_mempool"))
		rte_mempool_list_dump(stdout);
	else if (!strcmp(res->dump, "dump_devargs"))
		rte_eal_devargs_dump(stdout);
}

cmdline_parse_token_string_t cmd_dump_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_result, dump,
				 "dump_physmem#dump_memzone#dump_log_history#"
				 "dump_struct_sizes#dump_ring#dump_mempool#"
				 "dump_devargs");

cmdline_parse_inst_t cmd_dump = {
	.f = cmd_dump_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "dump status",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_dump,
		NULL,
	},
};

/****************/

struct cmd_dump_one_result {
	cmdline_fixed_string_t dump;
	cmdline_fixed_string_t name;
};

static void cmd_dump_one_parsed(void *parsed_result, struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_dump_one_result *res = parsed_result;

	if (!strcmp(res->dump, "dump_ring")) {
		struct rte_ring *r;
		r = rte_ring_lookup(res->name);
		if (r == NULL) {
			cmdline_printf(cl, "Cannot find ring\n");
			return;
		}
		rte_ring_dump(stdout, r);
	}
	else if (!strcmp(res->dump, "dump_mempool")) {
		struct rte_mempool *mp;
		mp = rte_mempool_lookup(res->name);
		if (mp == NULL) {
			cmdline_printf(cl, "Cannot find mempool\n");
			return;
		}
		rte_mempool_dump(stdout, mp);
	}
}

cmdline_parse_token_string_t cmd_dump_one_dump =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_one_result, dump,
				 "dump_ring#dump_mempool");

cmdline_parse_token_string_t cmd_dump_one_name =
	TOKEN_STRING_INITIALIZER(struct cmd_dump_one_result, name, NULL);

cmdline_parse_inst_t cmd_dump_one = {
	.f = cmd_dump_one_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "dump one ring/mempool: dump_ring|dump_mempool <name>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_dump_one_dump,
		(void *)&cmd_dump_one_name,
		NULL,
	},
};

/****************/

struct cmd_set_ring_result {
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t name;
	uint32_t value;
};

static void cmd_set_ring_parsed(void *parsed_result, struct cmdline *cl,
				__attribute__((unused)) void *data)
{
	struct cmd_set_ring_result *res = parsed_result;
	struct rte_ring *r;
	int ret;

	r = rte_ring_lookup(res->name);
	if (r == NULL) {
		cmdline_printf(cl, "Cannot find ring\n");
		return;
	}

	if (!strcmp(res->set, "set_watermark")) {
		ret = rte_ring_set_water_mark(r, res->value);
		if (ret != 0)
			cmdline_printf(cl, "Cannot set water mark\n");
	}
}

cmdline_parse_token_string_t cmd_set_ring_set =
	TOKEN_STRING_INITIALIZER(struct cmd_set_ring_result, set,
				 "set_watermark");

cmdline_parse_token_string_t cmd_set_ring_name =
	TOKEN_STRING_INITIALIZER(struct cmd_set_ring_result, name, NULL);

cmdline_parse_token_num_t cmd_set_ring_value =
	TOKEN_NUM_INITIALIZER(struct cmd_set_ring_result, value, UINT32);

cmdline_parse_inst_t cmd_set_ring = {
	.f = cmd_set_ring_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "set watermark: "
			"set_watermark <ring_name> <value>",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_set_ring_set,
		(void *)&cmd_set_ring_name,
		(void *)&cmd_set_ring_value,
		NULL,
	},
};

/****************/

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void
cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit,
				 "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "exit application",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/****************/

cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_autotest,
	(cmdline_parse_inst_t *)&cmd_dump,
	(cmdline_parse_inst_t *)&cmd_dump_one,
	(cmdline_parse_inst_t *)&cmd_set_ring,
	(cmdline_parse_inst_t *)&cmd_quit,
	NULL,
};

