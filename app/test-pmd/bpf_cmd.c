/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_bpf_ethdev.h>

#include <cmdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "testpmd.h"

static const struct rte_bpf_xsym bpf_xsym[] = {
	{
		.name = RTE_STR(stdout),
		.type = RTE_BPF_XTYPE_VAR,
		.var = {
			.val = (void *)(uintptr_t)&stdout,
			.desc = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(stdout),
			},
		},
	},
	{
		.name = RTE_STR(rte_pktmbuf_dump),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (void *)rte_pktmbuf_dump,
			.nb_args = 3,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uintptr_t),
				},
				[1] = {
					.type = RTE_BPF_ARG_PTR_MBUF,
					.size = sizeof(struct rte_mbuf),
				},
				[2] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint32_t),
				},
			},
		},
	},
};

/* *** load BPF program *** */
struct cmd_bpf_ld_result {
	cmdline_fixed_string_t bpf;
	cmdline_fixed_string_t dir;
	uint16_t port;
	uint16_t queue;
	cmdline_fixed_string_t op;
	cmdline_fixed_string_t flags;
	cmdline_fixed_string_t prm;
};

static void
bpf_parse_flags(const char *str, struct rte_bpf_arg *arg, uint32_t *flags)
{
	uint32_t i, v;

	*flags = RTE_BPF_ETH_F_NONE;
	arg->type = RTE_BPF_ARG_PTR;
	arg->size = mbuf_data_size[0];

	for (i = 0; str[i] != 0; i++) {
		v = toupper(str[i]);
		if (v == 'J')
			*flags |= RTE_BPF_ETH_F_JIT;
		else if (v == 'M') {
			arg->type = RTE_BPF_ARG_PTR_MBUF;
			arg->size = sizeof(struct rte_mbuf);
			arg->buf_size = mbuf_data_size[0];
		} else if (v == '-')
			continue;
		else
			fprintf(stderr, "unknown flag: \'%c\'", v);
	}
}

static void cmd_operate_bpf_ld_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	int32_t rc;
	uint32_t flags;
	struct cmd_bpf_ld_result *res;
	struct rte_bpf_prm prm;
	const char *fname, *sname;

	res = parsed_result;
	memset(&prm, 0, sizeof(prm));
	prm.xsym = bpf_xsym;
	prm.nb_xsym = RTE_DIM(bpf_xsym);

	bpf_parse_flags(res->flags, &prm.prog_arg, &flags);
	fname = res->prm;
	sname = ".text";

	if (strcmp(res->dir, "rx") == 0) {
		rc = rte_bpf_eth_rx_elf_load(res->port, res->queue, &prm,
			fname, sname, flags);
		printf("%d:%s\n", rc, strerror(-rc));
	} else if (strcmp(res->dir, "tx") == 0) {
		rc = rte_bpf_eth_tx_elf_load(res->port, res->queue, &prm,
			fname, sname, flags);
		printf("%d:%s\n", rc, strerror(-rc));
	} else
		fprintf(stderr, "invalid value: %s\n", res->dir);
}

cmdline_parse_token_string_t cmd_load_bpf_start =
	TOKEN_STRING_INITIALIZER(struct cmd_bpf_ld_result,
			bpf, "bpf-load");
cmdline_parse_token_string_t cmd_load_bpf_dir =
	TOKEN_STRING_INITIALIZER(struct cmd_bpf_ld_result,
			dir, "rx#tx");
cmdline_parse_token_num_t cmd_load_bpf_port =
	TOKEN_NUM_INITIALIZER(struct cmd_bpf_ld_result, port, RTE_UINT8);
cmdline_parse_token_num_t cmd_load_bpf_queue =
	TOKEN_NUM_INITIALIZER(struct cmd_bpf_ld_result, queue, RTE_UINT16);
cmdline_parse_token_string_t cmd_load_bpf_flags =
	TOKEN_STRING_INITIALIZER(struct cmd_bpf_ld_result,
			flags, NULL);
cmdline_parse_token_string_t cmd_load_bpf_prm =
	TOKEN_STRING_INITIALIZER(struct cmd_bpf_ld_result,
			prm, NULL);

cmdline_parse_inst_t cmd_operate_bpf_ld_parse = {
	.f = cmd_operate_bpf_ld_parsed,
	.data = NULL,
	.help_str = "bpf-load rx|tx <port> <queue> <J|M|B> <file_name>",
	.tokens = {
		(void *)&cmd_load_bpf_start,
		(void *)&cmd_load_bpf_dir,
		(void *)&cmd_load_bpf_port,
		(void *)&cmd_load_bpf_queue,
		(void *)&cmd_load_bpf_flags,
		(void *)&cmd_load_bpf_prm,
		NULL,
	},
};

/* *** unload BPF program *** */
struct cmd_bpf_unld_result {
	cmdline_fixed_string_t bpf;
	cmdline_fixed_string_t dir;
	uint16_t port;
	uint16_t queue;
};

static void cmd_operate_bpf_unld_parsed(void *parsed_result,
				__rte_unused struct cmdline *cl,
				__rte_unused void *data)
{
	struct cmd_bpf_unld_result *res;

	res = parsed_result;

	if (strcmp(res->dir, "rx") == 0)
		rte_bpf_eth_rx_unload(res->port, res->queue);
	else if (strcmp(res->dir, "tx") == 0)
		rte_bpf_eth_tx_unload(res->port, res->queue);
	else
		fprintf(stderr, "invalid value: %s\n", res->dir);
}

cmdline_parse_token_string_t cmd_unload_bpf_start =
	TOKEN_STRING_INITIALIZER(struct cmd_bpf_unld_result,
			bpf, "bpf-unload");
cmdline_parse_token_string_t cmd_unload_bpf_dir =
	TOKEN_STRING_INITIALIZER(struct cmd_bpf_unld_result,
			dir, "rx#tx");
cmdline_parse_token_num_t cmd_unload_bpf_port =
	TOKEN_NUM_INITIALIZER(struct cmd_bpf_unld_result, port, RTE_UINT8);
cmdline_parse_token_num_t cmd_unload_bpf_queue =
	TOKEN_NUM_INITIALIZER(struct cmd_bpf_unld_result, queue, RTE_UINT16);

cmdline_parse_inst_t cmd_operate_bpf_unld_parse = {
	.f = cmd_operate_bpf_unld_parsed,
	.data = NULL,
	.help_str = "bpf-unload rx|tx <port> <queue>",
	.tokens = {
		(void *)&cmd_unload_bpf_start,
		(void *)&cmd_unload_bpf_dir,
		(void *)&cmd_unload_bpf_port,
		(void *)&cmd_unload_bpf_queue,
		NULL,
	},
};
