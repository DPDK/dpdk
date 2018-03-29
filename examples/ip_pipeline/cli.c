/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>

#include "cli.h"
#include "kni.h"
#include "link.h"
#include "mempool.h"
#include "parser.h"
#include "pipeline.h"
#include "swq.h"
#include "tap.h"
#include "thread.h"
#include "tmgr.h"

#ifndef CMD_MAX_TOKENS
#define CMD_MAX_TOKENS     256
#endif

#define MSG_OUT_OF_MEMORY  "Not enough memory.\n"
#define MSG_CMD_UNKNOWN    "Unknown command \"%s\".\n"
#define MSG_CMD_UNIMPLEM   "Command \"%s\" not implemented.\n"
#define MSG_ARG_NOT_ENOUGH "Not enough arguments for command \"%s\".\n"
#define MSG_ARG_TOO_MANY   "Too many arguments for command \"%s\".\n"
#define MSG_ARG_MISMATCH   "Wrong number of arguments for command \"%s\".\n"
#define MSG_ARG_NOT_FOUND  "Argument \"%s\" not found.\n"
#define MSG_ARG_INVALID    "Invalid value for argument \"%s\".\n"
#define MSG_FILE_ERR       "Error in file \"%s\" at line %u.\n"
#define MSG_CMD_FAIL       "Command \"%s\" failed.\n"

static int
is_comment(char *in)
{
	if ((strlen(in) && index("!#%;", in[0])) ||
		(strncmp(in, "//", 2) == 0) ||
		(strncmp(in, "--", 2) == 0))
		return 1;

	return 0;
}

/**
 * mempool <mempool_name>
 *  buffer <buffer_size>
 *  pool <pool_size>
 *  cache <cache_size>
 *  cpu <cpu_id>
 */
static void
cmd_mempool(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct mempool_params p;
	char *name;
	struct mempool *mempool;

	if (n_tokens != 10) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "buffer") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buffer");
		return;
	}

	if (parser_read_uint32(&p.buffer_size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "buffer_size");
		return;
	}

	if (strcmp(tokens[4], "pool") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pool");
		return;
	}

	if (parser_read_uint32(&p.pool_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pool_size");
		return;
	}

	if (strcmp(tokens[6], "cache") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cache");
		return;
	}

	if (parser_read_uint32(&p.cache_size, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cache_size");
		return;
	}

	if (strcmp(tokens[8], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	mempool = mempool_create(name, &p);
	if (mempool == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * link <link_name>
 *  dev <device_name> | port <port_id>
 *  rxq <n_queues> <queue_size> <mempool_name>
 *  txq <n_queues> <queue_size>
 *  promiscuous on | off
 *  [rss <qid_0> ... <qid_n>]
 */
static void
cmd_link(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct link_params p;
	struct link_params_rss rss;
	struct link *link;
	char *name;

	if ((n_tokens < 13) || (n_tokens > 14 + LINK_RXQ_RSS_MAX)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
	name = tokens[1];

	if (strcmp(tokens[2], "dev") == 0)
		p.dev_name = tokens[3];
	else if (strcmp(tokens[2], "port") == 0) {
		p.dev_name = NULL;

		if (parser_read_uint16(&p.port_id, tokens[3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "dev or port");
		return;
	}

	if (strcmp(tokens[4], "rxq") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rxq");
		return;
	}

	if (parser_read_uint32(&p.rx.n_queues, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queues");
		return;
	}
	if (parser_read_uint32(&p.rx.queue_size, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	p.rx.mempool_name = tokens[7];

	if (strcmp(tokens[8], "txq") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "txq");
		return;
	}

	if (parser_read_uint32(&p.tx.n_queues, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queues");
		return;
	}

	if (parser_read_uint32(&p.tx.queue_size, tokens[10]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	if (strcmp(tokens[11], "promiscuous") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "promiscuous");
		return;
	}

	if (strcmp(tokens[12], "on") == 0)
		p.promiscuous = 1;
	else if (strcmp(tokens[12], "off") == 0)
		p.promiscuous = 0;
	else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "on or off");
		return;
	}

	/* RSS */
	p.rx.rss = NULL;
	if (n_tokens > 13) {
		uint32_t queue_id, i;

		if (strcmp(tokens[13], "rss") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rss");
			return;
		}

		p.rx.rss = &rss;

		rss.n_queues = 0;
		for (i = 14; i < n_tokens; i++) {
			if (parser_read_uint32(&queue_id, tokens[i]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"queue_id");
				return;
			}

			rss.queue_id[rss.n_queues] = queue_id;
			rss.n_queues++;
		}
	}

	link = link_create(name, &p);
	if (link == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * swq <swq_name>
 *  size <size>
 *  cpu <cpu_id>
 */
static void
cmd_swq(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct swq_params p;
	char *name;
	struct swq *swq;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "size") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
		return;
	}

	if (parser_read_uint32(&p.size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "size");
		return;
	}

	if (strcmp(tokens[4], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	swq = swq_create(name, &p);
	if (swq == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr subport profile
 *  <tb_rate> <tb_size>
 *  <tc0_rate> <tc1_rate> <tc2_rate> <tc3_rate>
 *  <tc_period>
 */
static void
cmd_tmgr_subport_profile(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_sched_subport_params p;
	int status, i;

	if (n_tokens != 10) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint32(&p.tb_rate, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_rate");
		return;
	}

	if (parser_read_uint32(&p.tb_size, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_size");
		return;
	}

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		if (parser_read_uint32(&p.tc_rate[i], tokens[5 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "tc_rate");
			return;
		}

	if (parser_read_uint32(&p.tc_period, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc_period");
		return;
	}

	status = tmgr_subport_profile_add(&p);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr pipe profile
 *  <tb_rate> <tb_size>
 *  <tc0_rate> <tc1_rate> <tc2_rate> <tc3_rate>
 *  <tc_period>
 *  <tc_ov_weight>
 *  <wrr_weight0..15>
 */
static void
cmd_tmgr_pipe_profile(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_sched_pipe_params p;
	int status, i;

	if (n_tokens != 27) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint32(&p.tb_rate, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_rate");
		return;
	}

	if (parser_read_uint32(&p.tb_size, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_size");
		return;
	}

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		if (parser_read_uint32(&p.tc_rate[i], tokens[5 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "tc_rate");
			return;
		}

	if (parser_read_uint32(&p.tc_period, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc_period");
		return;
	}

#ifdef RTE_SCHED_SUBPORT_TC_OV
	if (parser_read_uint8(&p.tc_ov_weight, tokens[10]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc_ov_weight");
		return;
	}
#endif

	for (i = 0; i < RTE_SCHED_QUEUES_PER_PIPE; i++)
		if (parser_read_uint8(&p.wrr_weights[i], tokens[11 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "wrr_weights");
			return;
		}

	status = tmgr_pipe_profile_add(&p);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr <tmgr_name>
 *  rate <rate>
 *  spp <n_subports_per_port>
 *  pps <n_pipes_per_subport>
 *  qsize <qsize_tc0> <qsize_tc1> <qsize_tc2> <qsize_tc3>
 *  fo <frame_overhead>
 *  mtu <mtu>
 *  cpu <cpu_id>
 */
static void
cmd_tmgr(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct tmgr_port_params p;
	char *name;
	struct tmgr_port *tmgr_port;
	int i;

	if (n_tokens != 19) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "rate") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rate");
		return;
	}

	if (parser_read_uint32(&p.rate, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "rate");
		return;
	}

	if (strcmp(tokens[4], "spp") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "spp");
		return;
	}

	if (parser_read_uint32(&p.n_subports_per_port, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_subports_per_port");
		return;
	}

	if (strcmp(tokens[6], "pps") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pps");
		return;
	}

	if (parser_read_uint32(&p.n_pipes_per_subport, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_pipes_per_subport");
		return;
	}

	if (strcmp(tokens[8], "qsize") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "qsize");
		return;
	}

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		if (parser_read_uint16(&p.qsize[i], tokens[9 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "qsize");
			return;
		}

	if (strcmp(tokens[13], "fo") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "fo");
		return;
	}

	if (parser_read_uint32(&p.frame_overhead, tokens[14]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "frame_overhead");
		return;
	}

	if (strcmp(tokens[15], "mtu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mtu");
		return;
	}

	if (parser_read_uint32(&p.mtu, tokens[16]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "mtu");
		return;
	}

	if (strcmp(tokens[17], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[18]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	tmgr_port = tmgr_port_create(name, &p);
	if (tmgr_port == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr <tmgr_name> subport <subport_id>
 *  profile <subport_profile_id>
 */
static void
cmd_tmgr_subport(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	uint32_t subport_id, subport_profile_id;
	int status;
	char *name;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (parser_read_uint32(&subport_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "subport_id");
		return;
	}

	if (parser_read_uint32(&subport_profile_id, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "subport_profile_id");
		return;
	}

	status = tmgr_subport_config(name, subport_id, subport_profile_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tmgr <tmgr_name> subport <subport_id> pipe
 *  from <pipe_id_first> to <pipe_id_last>
 *  profile <pipe_profile_id>
 */
static void
cmd_tmgr_subport_pipe(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	uint32_t subport_id, pipe_id_first, pipe_id_last, pipe_profile_id;
	int status;
	char *name;

	if (n_tokens != 11) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (parser_read_uint32(&subport_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "subport_id");
		return;
	}

	if (strcmp(tokens[4], "pipe") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipe");
		return;
	}

	if (strcmp(tokens[5], "from") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&pipe_id_first, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipe_id_first");
		return;
	}

	if (strcmp(tokens[7], "to") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&pipe_id_last, tokens[8]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipe_id_last");
		return;
	}

	if (strcmp(tokens[9], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (parser_read_uint32(&pipe_profile_id, tokens[10]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipe_profile_id");
		return;
	}

	status = tmgr_pipe_config(name, subport_id, pipe_id_first,
			pipe_id_last, pipe_profile_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tap <tap_name>
 */
static void
cmd_tap(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *name;
	struct tap *tap;

	if (n_tokens != 2) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	tap = tap_create(name);
	if (tap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * kni <kni_name>
 *  link <link_name>
 *  mempool <mempool_name>
 *  [thread <thread_id>]
 */
static void
cmd_kni(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct kni_params p;
	char *name;
	struct kni *kni;

	if ((n_tokens != 6) && (n_tokens != 8)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "link") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "link");
		return;
	}

	p.link_name = tokens[3];

	if (strcmp(tokens[4], "mempool") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mempool");
		return;
	}

	p.mempool_name = tokens[5];

	if (n_tokens == 8) {
		if (strcmp(tokens[6], "thread") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "thread");
			return;
		}

		if (parser_read_uint32(&p.thread_id, tokens[7]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
			return;
		}

		p.force_bind = 1;
	} else
		p.force_bind = 0;

	kni = kni_create(name, &p);
	if (kni == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * port in action profile <profile_name>
 *  [filter match | mismatch offset <key_offset> mask <key_mask> key <key_value> port <port_id>]
 *  [balance offset <key_offset> mask <key_mask> port <port_id0> ... <port_id15>]
 */
static void
cmd_port_in_action_profile(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct port_in_action_profile_params p;
	struct port_in_action_profile *ap;
	char *name;
	uint32_t t0;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (strcmp(tokens[2], "action") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "action");
		return;
	}

	if (strcmp(tokens[3], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	name = tokens[4];

	t0 = 5;

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "filter") == 0)) {
		uint32_t size;

		if (n_tokens < t0 + 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "port in action profile filter");
			return;
		}

		if (strcmp(tokens[t0 + 1], "match") == 0)
			p.fltr.filter_on_match = 1;
		else if (strcmp(tokens[t0 + 1], "mismatch") == 0)
			p.fltr.filter_on_match = 0;
		else {
			snprintf(out, out_size, MSG_ARG_INVALID, "match or mismatch");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (parser_read_uint32(&p.fltr.key_offset, tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		size = RTE_PORT_IN_ACTION_FLTR_KEY_SIZE;
		if ((parse_hex_string(tokens[t0 + 5], p.fltr.key_mask, &size) != 0) ||
			(size != RTE_PORT_IN_ACTION_FLTR_KEY_SIZE)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 6], "key") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
			return;
		}

		size = RTE_PORT_IN_ACTION_FLTR_KEY_SIZE;
		if ((parse_hex_string(tokens[t0 + 7], p.fltr.key, &size) != 0) ||
			(size != RTE_PORT_IN_ACTION_FLTR_KEY_SIZE)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_value");
			return;
		}

		if (strcmp(tokens[t0 + 8], "port") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (parser_read_uint32(&p.fltr.port_id, tokens[t0 + 9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}

		p.action_mask |= 1LLU << RTE_PORT_IN_ACTION_FLTR;
		t0 += 10;
	} /* filter */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "balance") == 0)) {
		uint32_t i;

		if (n_tokens < t0 + 22) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "port in action profile balance");
			return;
		}

		if (strcmp(tokens[t0 + 1], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (parser_read_uint32(&p.lb.key_offset, tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		p.lb.key_size = RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX;
		if (parse_hex_string(tokens[t0 + 4], p.lb.key_mask, &p.lb.key_size) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 5], "port") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
			return;
		}

		for (i = 0; i < 16; i++)
			if (parser_read_uint32(&p.lb.port_id[i], tokens[t0 + 6 + i]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
				return;
			}

		p.action_mask |= 1LLU << RTE_PORT_IN_ACTION_LB;
		t0 += 22;
	} /* balance */

	if (t0 < n_tokens) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ap = port_in_action_profile_create(name, &p);
	if (ap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * table action profile <profile_name>
 *  ipv4 | ipv6
 *  offset <ip_offset>
 *  fwd
 *  [meter srtcm | trtcm
 *      tc <n_tc>
 *      stats none | pkts | bytes | both]
 *  [tm spp <n_subports_per_port> pps <n_pipes_per_subport>]
 *  [encap ether | vlan | qinq | mpls | pppoe]
 *  [nat src | dst
 *      proto udp | tcp]
 *  [ttl drop | fwd
 *      stats none | pkts]
 *  [stats pkts | bytes | both]
 *  [time]
 */
static void
cmd_table_action_profile(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_action_profile_params p;
	struct table_action_profile *ap;
	char *name;
	uint32_t t0;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "action") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "action");
		return;
	}

	if (strcmp(tokens[2], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "ipv4") == 0)
		p.common.ip_version = 1;
	else if (strcmp(tokens[4], "ipv6") == 0)
		p.common.ip_version = 0;
	else {
		snprintf(out, out_size, MSG_ARG_INVALID, "ipv4 or ipv6");
		return;
	}

	if (strcmp(tokens[5], "offset") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
		return;
	}

	if (parser_read_uint32(&p.common.ip_offset, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ip_offset");
		return;
	}

	if (strcmp(tokens[7], "fwd") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "fwd");
		return;
	}

	p.action_mask |= 1LLU << RTE_TABLE_ACTION_FWD;

	t0 = 8;
	if ((t0 < n_tokens) && (strcmp(tokens[t0], "meter") == 0)) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile meter");
			return;
		}

		if (strcmp(tokens[t0 + 1], "srtcm") == 0)
			p.mtr.alg = RTE_TABLE_ACTION_METER_SRTCM;
		else if (strcmp(tokens[t0 + 1], "trtcm") == 0)
			p.mtr.alg = RTE_TABLE_ACTION_METER_TRTCM;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"srtcm or trtcm");
			return;
		}

		if (strcmp(tokens[t0 + 2], "tc") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc");
			return;
		}

		if (parser_read_uint32(&p.mtr.n_tc, tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_tc");
			return;
		}

		if (strcmp(tokens[t0 + 4], "stats") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
			return;
		}

		if (strcmp(tokens[t0 + 5], "none") == 0) {
			p.mtr.n_packets_enabled = 0;
			p.mtr.n_bytes_enabled = 0;
		} else if (strcmp(tokens[t0 + 5], "pkts") == 0) {
			p.mtr.n_packets_enabled = 1;
			p.mtr.n_bytes_enabled = 0;
		} else if (strcmp(tokens[t0 + 5], "bytes") == 0) {
			p.mtr.n_packets_enabled = 0;
			p.mtr.n_bytes_enabled = 1;
		} else if (strcmp(tokens[t0 + 5], "both") == 0) {
			p.mtr.n_packets_enabled = 1;
			p.mtr.n_bytes_enabled = 1;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"none or pkts or bytes or both");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_MTR;
		t0 += 6;
	} /* meter */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "tm") == 0)) {
		if (n_tokens < t0 + 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile tm");
			return;
		}

		if (strcmp(tokens[t0 + 1], "spp") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "spp");
			return;
		}

		if (parser_read_uint32(&p.tm.n_subports_per_port,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_subports_per_port");
			return;
		}

		if (strcmp(tokens[t0 + 3], "pps") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pps");
			return;
		}

		if (parser_read_uint32(&p.tm.n_pipes_per_subport,
			tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_pipes_per_subport");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TM;
		t0 += 5;
	} /* tm */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "encap") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"action profile encap");
			return;
		}

		if (strcmp(tokens[t0 + 1], "ether") == 0)
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_ETHER;
		else if (strcmp(tokens[t0 + 1], "vlan") == 0)
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_VLAN;
		else if (strcmp(tokens[t0 + 1], "qinq") == 0)
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_QINQ;
		else if (strcmp(tokens[t0 + 1], "mpls") == 0)
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_MPLS;
		else if (strcmp(tokens[t0 + 1], "pppoe") == 0)
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_PPPOE;
		else {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "encap");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_ENCAP;
		t0 += 2;
	} /* encap */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "nat") == 0)) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile nat");
			return;
		}

		if (strcmp(tokens[t0 + 1], "src") == 0)
			p.nat.source_nat = 1;
		else if (strcmp(tokens[t0 + 1], "dst") == 0)
			p.nat.source_nat = 0;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"src or dst");
			return;
		}

		if (strcmp(tokens[t0 + 2], "proto") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "proto");
			return;
		}

		if (strcmp(tokens[t0 + 3], "tcp") == 0)
			p.nat.proto = 0x06;
		else if (strcmp(tokens[t0 + 3], "udp") == 0)
			p.nat.proto = 0x11;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"tcp or udp");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_NAT;
		t0 += 4;
	} /* nat */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "ttl") == 0)) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile ttl");
			return;
		}

		if (strcmp(tokens[t0 + 1], "drop") == 0)
			p.ttl.drop = 1;
		else if (strcmp(tokens[t0 + 1], "fwd") == 0)
			p.ttl.drop = 0;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"drop or fwd");
			return;
		}

		if (strcmp(tokens[t0 + 2], "stats") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
			return;
		}

		if (strcmp(tokens[t0 + 3], "none") == 0)
			p.ttl.n_packets_enabled = 0;
		else if (strcmp(tokens[t0 + 3], "pkts") == 0)
			p.ttl.n_packets_enabled = 1;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"none or pkts");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TTL;
		t0 += 4;
	} /* ttl */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "stats") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile stats");
			return;
		}

		if (strcmp(tokens[t0 + 1], "pkts") == 0) {
			p.stats.n_packets_enabled = 1;
			p.stats.n_bytes_enabled = 0;
		} else if (strcmp(tokens[t0 + 1], "bytes") == 0) {
			p.stats.n_packets_enabled = 0;
			p.stats.n_bytes_enabled = 1;
		} else if (strcmp(tokens[t0 + 1], "both") == 0) {
			p.stats.n_packets_enabled = 1;
			p.stats.n_bytes_enabled = 1;
		} else {
			snprintf(out, out_size,	MSG_ARG_NOT_FOUND,
				"pkts or bytes or both");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_STATS;
		t0 += 2;
	} /* stats */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "time") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TIME;
		t0 += 1;
	} /* time */

	if (t0 < n_tokens) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ap = table_action_profile_create(name, &p);
	if (ap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name>
 *  period <timer_period_ms>
 *  offset_port_id <offset_port_id>
 *  cpu <cpu_id>
 */
static void
cmd_pipeline(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline_params p;
	char *name;
	struct pipeline *pipeline;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "period") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "period");
		return;
	}

	if (parser_read_uint32(&p.timer_period_ms, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "timer_period_ms");
		return;
	}

	if (strcmp(tokens[4], "offset_port_id") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset_port_id");
		return;
	}

	if (parser_read_uint32(&p.offset_port_id, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "offset_port_id");
		return;
	}

	if (strcmp(tokens[6], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	pipeline = pipeline_create(name, &p);
	if (pipeline == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in
 *  bsz <burst_size>
 *  link <link_name> rxq <queue_id>
 *  | swq <swq_name>
 *  | tmgr <tmgr_name>
 *  | tap <tap_name> mempool <mempool_name> mtu <mtu>
 *  | kni <kni_name>
 *  | source mempool <mempool_name> file <file_name> bpp <n_bytes_per_pkt>
 *  [action <port_in_action_profile_name>]
 *  [disabled]
 */
static void
cmd_pipeline_port_in(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct port_in_params p;
	char *pipeline_name;
	uint32_t t0;
	int enabled, status;

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (strcmp(tokens[4], "bsz") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
		return;
	}

	if (parser_read_uint32(&p.burst_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "burst_size");
		return;
	}

	t0 = 6;

	if (strcmp(tokens[t0], "link") == 0) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in link");
			return;
		}

		p.type = PORT_IN_RXQ;

		p.dev_name = tokens[t0 + 1];

		if (strcmp(tokens[t0 + 2], "rxq") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rxq");
			return;
		}

		if (parser_read_uint16(&p.rxq.queue_id, tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"queue_id");
			return;
		}
		t0 += 4;
	} else if (strcmp(tokens[t0], "swq") == 0) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in swq");
			return;
		}

		p.type = PORT_IN_SWQ;

		p.dev_name = tokens[t0 + 1];

		t0 += 2;
	} else if (strcmp(tokens[t0], "tmgr") == 0) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in tmgr");
			return;
		}

		p.type = PORT_IN_TMGR;

		p.dev_name = tokens[t0 + 1];

		t0 += 2;
	} else if (strcmp(tokens[t0], "tap") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in tap");
			return;
		}

		p.type = PORT_IN_TAP;

		p.dev_name = tokens[t0 + 1];

		if (strcmp(tokens[t0 + 2], "mempool") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mempool");
			return;
		}

		p.tap.mempool_name = tokens[t0 + 3];

		if (strcmp(tokens[t0 + 4], "mtu") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mtu");
			return;
		}

		if (parser_read_uint32(&p.tap.mtu, tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "mtu");
			return;
		}

		t0 += 6;
	} else if (strcmp(tokens[t0], "kni") == 0) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in kni");
			return;
		}

		p.type = PORT_IN_KNI;

		p.dev_name = tokens[t0 + 1];

		t0 += 2;
	} else if (strcmp(tokens[t0], "source") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in source");
			return;
		}

		p.type = PORT_IN_SOURCE;

		p.dev_name = NULL;

		if (strcmp(tokens[t0 + 1], "mempool") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mempool");
			return;
		}

		p.source.mempool_name = tokens[t0 + 2];

		if (strcmp(tokens[t0 + 3], "file") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"file");
			return;
		}

		p.source.file_name = tokens[t0 + 4];

		if (strcmp(tokens[t0 + 5], "bpp") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"bpp");
			return;
		}

		if (parser_read_uint32(&p.source.n_bytes_per_pkt, tokens[t0 + 6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_bytes_per_pkt");
			return;
		}

		t0 += 7;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	p.action_profile_name = NULL;
	if ((n_tokens > t0) && (strcmp(tokens[t0], "action") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "action");
			return;
		}

		p.action_profile_name = tokens[t0 + 1];

		t0 += 2;
	}

	enabled = 1;
	if ((n_tokens > t0) &&
		(strcmp(tokens[t0], "disabled") == 0)) {
		enabled = 0;

		t0 += 1;
	}

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = pipeline_port_in_create(pipeline_name,
		&p, enabled);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port out
 *  bsz <burst_size>
 *  link <link_name> txq <txq_id>
 *  | swq <swq_name>
 *  | tmgr <tmgr_name>
 *  | tap <tap_name>
 *  | kni <kni_name>
 *  | sink [file <file_name> pkts <max_n_pkts>]
 */
static void
cmd_pipeline_port_out(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct port_out_params p;
	char *pipeline_name;
	int status;

	memset(&p, 0, sizeof(p));

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "out") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "out");
		return;
	}

	if (strcmp(tokens[4], "bsz") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
		return;
	}

	if (parser_read_uint32(&p.burst_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "burst_size");
		return;
	}

	if (strcmp(tokens[6], "link") == 0) {
		if (n_tokens != 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out link");
			return;
		}

		p.type = PORT_OUT_TXQ;

		p.dev_name = tokens[7];

		if (strcmp(tokens[8], "txq") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "txq");
			return;
		}

		if (parser_read_uint16(&p.txq.queue_id, tokens[9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "queue_id");
			return;
		}
	} else if (strcmp(tokens[6], "swq") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out swq");
			return;
		}

		p.type = PORT_OUT_SWQ;

		p.dev_name = tokens[7];
	} else if (strcmp(tokens[6], "tmgr") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out tmgr");
			return;
		}

		p.type = PORT_OUT_TMGR;

		p.dev_name = tokens[7];
	} else if (strcmp(tokens[6], "tap") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out tap");
			return;
		}

		p.type = PORT_OUT_TAP;

		p.dev_name = tokens[7];
	} else if (strcmp(tokens[6], "kni") == 0) {
		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out kni");
			return;
		}

		p.type = PORT_OUT_KNI;

		p.dev_name = tokens[7];
	} else if (strcmp(tokens[6], "sink") == 0) {
		if ((n_tokens != 7) && (n_tokens != 11)) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out sink");
			return;
		}

		p.type = PORT_OUT_SINK;

		p.dev_name = NULL;

		if (n_tokens == 7) {
			p.sink.file_name = NULL;
			p.sink.max_n_pkts = 0;
		} else {
			if (strcmp(tokens[7], "file") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND,
					"file");
				return;
			}

			p.sink.file_name = tokens[8];

			if (strcmp(tokens[9], "pkts") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pkts");
				return;
			}

			if (parser_read_uint32(&p.sink.max_n_pkts, tokens[10]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "max_n_pkts");
				return;
			}
		}
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	status = pipeline_port_out_create(pipeline_name, &p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> table
 *      match
 *      acl
 *          ipv4 | ipv6
 *          offset <ip_header_offset>
 *          size <n_rules>
 *      | array
 *          offset <key_offset>
 *          size <n_keys>
 *      | hash
 *          ext | lru
 *          key <key_size>
 *          mask <key_mask>
 *          offset <key_offset>
 *          buckets <n_buckets>
 *          size <n_keys>
 *      | lpm
 *          ipv4 | ipv6
 *          offset <ip_header_offset>
 *          size <n_rules>
 *      | stub
 *  [action <table_action_profile_name>]
 */
static void
cmd_pipeline_table(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	uint8_t key_mask[TABLE_RULE_MATCH_SIZE_MAX];
	struct table_params p;
	char *pipeline_name;
	uint32_t t0;
	int status;

	if (n_tokens < 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (strcmp(tokens[3], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return;
	}

	t0 = 4;
	if (strcmp(tokens[t0], "acl") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table acl");
			return;
		}

		p.match_type = TABLE_ACL;

		if (strcmp(tokens[t0 + 1], "ipv4") == 0)
			p.match.acl.ip_version = 1;
		else if (strcmp(tokens[t0 + 1], "ipv6") == 0)
			p.match.acl.ip_version = 0;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (parser_read_uint32(&p.match.acl.ip_header_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"ip_header_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (parser_read_uint32(&p.match.acl.n_rules,
			tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_rules");
			return;
		}

		t0 += 6;
	} else if (strcmp(tokens[t0], "array") == 0) {
		if (n_tokens < t0 + 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table array");
			return;
		}

		p.match_type = TABLE_ARRAY;

		if (strcmp(tokens[t0 + 1], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (parser_read_uint32(&p.match.array.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (parser_read_uint32(&p.match.array.n_keys,
			tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_keys");
			return;
		}

		t0 += 5;
	} else if (strcmp(tokens[t0], "hash") == 0) {
		uint32_t key_mask_size = TABLE_RULE_MATCH_SIZE_MAX;

		if (n_tokens < t0 + 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table hash");
			return;
		}

		p.match_type = TABLE_HASH;

		if (strcmp(tokens[t0 + 1], "ext") == 0)
			p.match.hash.extendable_bucket = 1;
		else if (strcmp(tokens[t0 + 1], "lru") == 0)
			p.match.hash.extendable_bucket = 0;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ext or lru");
			return;
		}

		if (strcmp(tokens[t0 + 2], "key") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
			return;
		}

		if ((parser_read_uint32(&p.match.hash.key_size,
			tokens[t0 + 3]) != 0) ||
			(p.match.hash.key_size == 0) ||
			(p.match.hash.key_size > TABLE_RULE_MATCH_SIZE_MAX)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_size");
			return;
		}

		if (strcmp(tokens[t0 + 4], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		if ((parse_hex_string(tokens[t0 + 5],
			key_mask, &key_mask_size) != 0) ||
			(key_mask_size != p.match.hash.key_size)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}
		p.match.hash.key_mask = key_mask;

		if (strcmp(tokens[t0 + 6], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (parser_read_uint32(&p.match.hash.key_offset,
			tokens[t0 + 7]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 8], "buckets") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buckets");
			return;
		}

		if (parser_read_uint32(&p.match.hash.n_buckets,
			tokens[t0 + 9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_buckets");
			return;
		}

		if (strcmp(tokens[t0 + 10], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (parser_read_uint32(&p.match.hash.n_keys,
			tokens[t0 + 11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_keys");
			return;
		}

		t0 += 12;
	} else if (strcmp(tokens[t0], "lpm") == 0) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table lpm");
			return;
		}

		p.match_type = TABLE_LPM;

		if (strcmp(tokens[t0 + 1], "ipv4") == 0)
			p.match.lpm.key_size = 4;
		else if (strcmp(tokens[t0 + 1], "ipv6") == 0)
			p.match.lpm.key_size = 16;
		else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (parser_read_uint32(&p.match.lpm.key_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (parser_read_uint32(&p.match.lpm.n_rules,
			tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_rules");
			return;
		}

		t0 += 6;
	} else if (strcmp(tokens[t0], "stub") == 0) {
		if (n_tokens < t0 + 1) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline table stub");
			return;
		}

		p.match_type = TABLE_STUB;

		t0 += 1;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	p.action_profile_name = NULL;
	if ((n_tokens > t0) && (strcmp(tokens[t0], "action") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "action");
			return;
		}

		p.action_profile_name = tokens[t0 + 1];

		t0 += 2;
	}

	if (n_tokens > t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = pipeline_table_create(pipeline_name, &p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> table <table_id>
 */
static void
cmd_pipeline_port_in_table(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t port_id, table_id;
	int status;

	if (n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	status = pipeline_port_in_connect_to_table(pipeline_name,
		port_id,
		table_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> stats read [clear]
 */

#define MSG_PIPELINE_PORT_IN_STATS                         \
	"Pkts in: %" PRIu64 "\n"                           \
	"Pkts dropped by AH: %" PRIu64 "\n"                \
	"Pkts dropped by other: %" PRIu64 "\n"

static void
cmd_pipeline_port_in_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_pipeline_port_in_stats stats;
	char *pipeline_name;
	uint32_t port_id;
	int clear, status;

	if ((n_tokens != 7) && (n_tokens != 8)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	if (strcmp(tokens[6], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	clear = 0;
	if (n_tokens == 8) {
		if (strcmp(tokens[7], "clear") != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "clear");
			return;
		}

		clear = 1;
	}

	status = pipeline_port_in_stats_read(pipeline_name,
		port_id,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, MSG_PIPELINE_PORT_IN_STATS,
		stats.stats.n_pkts_in,
		stats.n_pkts_dropped_by_ah,
		stats.stats.n_pkts_drop);
}

/**
 * pipeline <pipeline_name> port in <port_id> enable
 */
static void
cmd_pipeline_port_in_enable(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t port_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = pipeline_port_in_enable(pipeline_name, port_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> disable
 */
static void
cmd_pipeline_port_in_disable(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t port_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "in") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	if (parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = pipeline_port_in_disable(pipeline_name, port_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port out <port_id> stats read [clear]
 */
#define MSG_PIPELINE_PORT_OUT_STATS                        \
	"Pkts in: %" PRIu64 "\n"                           \
	"Pkts dropped by AH: %" PRIu64 "\n"                \
	"Pkts dropped by other: %" PRIu64 "\n"

static void
cmd_pipeline_port_out_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_pipeline_port_out_stats stats;
	char *pipeline_name;
	uint32_t port_id;
	int clear, status;

	if ((n_tokens != 7) && (n_tokens != 8)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "port") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (strcmp(tokens[3], "out") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "out");
		return;
	}

	if (parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	if (strcmp(tokens[6], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	clear = 0;
	if (n_tokens == 8) {
		if (strcmp(tokens[7], "clear") != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "clear");
			return;
		}

		clear = 1;
	}

	status = pipeline_port_out_stats_read(pipeline_name,
		port_id,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, MSG_PIPELINE_PORT_OUT_STATS,
		stats.stats.n_pkts_in,
		stats.n_pkts_dropped_by_ah,
		stats.stats.n_pkts_drop);
}

/**
 * pipeline <pipeline_name> table <table_id> stats read [clear]
 */
#define MSG_PIPELINE_TABLE_STATS                                     \
	"Pkts in: %" PRIu64 "\n"                                     \
	"Pkts in with lookup miss: %" PRIu64 "\n"                    \
	"Pkts in with lookup hit dropped by AH: %" PRIu64 "\n"       \
	"Pkts in with lookup hit dropped by others: %" PRIu64 "\n"   \
	"Pkts in with lookup miss dropped by AH: %" PRIu64 "\n"      \
	"Pkts in with lookup miss dropped by others: %" PRIu64 "\n"

static void
cmd_pipeline_table_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_pipeline_table_stats stats;
	char *pipeline_name;
	uint32_t table_id;
	int clear, status;

	if ((n_tokens != 6) && (n_tokens != 7)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	if (strcmp(tokens[5], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	clear = 0;
	if (n_tokens == 7) {
		if (strcmp(tokens[6], "clear") != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "clear");
			return;
		}

		clear = 1;
	}

	status = pipeline_table_stats_read(pipeline_name,
		table_id,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, MSG_PIPELINE_TABLE_STATS,
		stats.stats.n_pkts_in,
		stats.stats.n_pkts_lookup_miss,
		stats.n_pkts_dropped_by_lkp_hit_ah,
		stats.n_pkts_dropped_lkp_hit,
		stats.n_pkts_dropped_by_lkp_miss_ah,
		stats.n_pkts_dropped_lkp_miss);
}

/**
 * thread <thread_id> pipeline <pipeline_name> enable
 */
static void
cmd_thread_pipeline_enable(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint32(&thread_id, tokens[1]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (strcmp(tokens[2], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[3];

	if (strcmp(tokens[4], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = thread_pipeline_enable(thread_id, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, "thread pipeline enable");
		return;
	}
}

/**
 * thread <thread_id> pipeline <pipeline_name> disable
 */
static void
cmd_thread_pipeline_disable(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint32(&thread_id, tokens[1]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (strcmp(tokens[2], "pipeline") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	pipeline_name = tokens[3];

	if (strcmp(tokens[4], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = thread_pipeline_disable(thread_id, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL,
			"thread pipeline disable");
		return;
	}
}

void
cli_process(char *in, char *out, size_t out_size)
{
	char *tokens[CMD_MAX_TOKENS];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	if (is_comment(in))
		return;

	status = parse_tokenize_string(in, tokens, &n_tokens);
	if (status) {
		snprintf(out, out_size, MSG_ARG_TOO_MANY, "");
		return;
	}

	if (n_tokens == 0)
		return;

	if (strcmp(tokens[0], "mempool") == 0) {
		cmd_mempool(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		cmd_link(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "swq") == 0) {
		cmd_swq(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "tmgr") == 0) {
		if ((n_tokens >= 3) &&
			(strcmp(tokens[1], "subport") == 0) &&
			(strcmp(tokens[2], "profile") == 0)) {
			cmd_tmgr_subport_profile(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[1], "pipe") == 0) &&
			(strcmp(tokens[2], "profile") == 0)) {
			cmd_tmgr_pipe_profile(tokens, n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "subport") == 0) &&
			(strcmp(tokens[4], "profile") == 0)) {
			cmd_tmgr_subport(tokens, n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "subport") == 0) &&
			(strcmp(tokens[4], "pipe") == 0)) {
			cmd_tmgr_subport_pipe(tokens, n_tokens, out, out_size);
			return;
		}

		cmd_tmgr(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "tap") == 0) {
		cmd_tap(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "kni") == 0) {
		cmd_kni(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "port") == 0) {
		cmd_port_in_action_profile(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "table") == 0) {
		cmd_table_action_profile(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "pipeline") == 0) {
		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "period") == 0)) {
			cmd_pipeline(tokens, n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[4], "bsz") == 0)) {
			cmd_pipeline_port_in(tokens, n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "out") == 0) &&
			(strcmp(tokens[4], "bsz") == 0)) {
			cmd_pipeline_port_out(tokens, n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 4) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[3], "match") == 0)) {
			cmd_pipeline_table(tokens, n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "table") == 0)) {
			cmd_pipeline_port_in_table(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "stats") == 0)) {
			cmd_pipeline_port_in_stats(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "enable") == 0)) {
			cmd_pipeline_port_in_enable(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "disable") == 0)) {
			cmd_pipeline_port_in_disable(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "out") == 0) &&
			(strcmp(tokens[5], "stats") == 0)) {
			cmd_pipeline_port_out_stats(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "stats") == 0)) {
			cmd_pipeline_table_stats(tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	if (strcmp(tokens[0], "thread") == 0) {
		if ((n_tokens >= 5) &&
			(strcmp(tokens[4], "enable") == 0)) {
			cmd_thread_pipeline_enable(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[4], "disable") == 0)) {
			cmd_thread_pipeline_disable(tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	snprintf(out, out_size, MSG_CMD_UNKNOWN, tokens[0]);
}

int
cli_script_process(const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max)
{
	char *msg_in = NULL, *msg_out = NULL;
	FILE *f = NULL;

	/* Check input arguments */
	if ((file_name == NULL) ||
		(strlen(file_name) == 0) ||
		(msg_in_len_max == 0) ||
		(msg_out_len_max == 0))
		return -EINVAL;

	msg_in = malloc(msg_in_len_max + 1);
	msg_out = malloc(msg_out_len_max + 1);
	if ((msg_in == NULL) ||
		(msg_out == NULL)) {
		free(msg_out);
		free(msg_in);
		return -ENOMEM;
	}

	/* Open input file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		free(msg_out);
		free(msg_in);
		return -EIO;
	}

	/* Read file */
	for ( ; ; ) {
		if (fgets(msg_in, msg_in_len_max + 1, f) == NULL)
			break;

		printf("%s", msg_in);
		msg_out[0] = 0;

		cli_process(msg_in,
			msg_out,
			msg_out_len_max);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	free(msg_out);
	free(msg_in);
	return 0;
}
