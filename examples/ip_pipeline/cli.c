/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>

#include "cli.h"

#include "cryptodev.h"
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

#define MSG_OUT_OF_MEMORY   "Not enough memory.\n"
#define MSG_CMD_UNKNOWN     "Unknown command \"%s\".\n"
#define MSG_CMD_UNIMPLEM    "Command \"%s\" not implemented.\n"
#define MSG_ARG_NOT_ENOUGH  "Not enough arguments for command \"%s\".\n"
#define MSG_ARG_TOO_MANY    "Too many arguments for command \"%s\".\n"
#define MSG_ARG_MISMATCH    "Wrong number of arguments for command \"%s\".\n"
#define MSG_ARG_NOT_FOUND   "Argument \"%s\" not found.\n"
#define MSG_ARG_INVALID     "Invalid value for argument \"%s\".\n"
#define MSG_FILE_ERR        "Error in file \"%s\" at line %u.\n"
#define MSG_FILE_NOT_ENOUGH "Not enough rules in file \"%s\".\n"
#define MSG_CMD_FAIL        "Command \"%s\" failed.\n"

static int
is_comment(char *in)
{
	if ((strlen(in) && index("!#%;", in[0])) ||
		(strncmp(in, "//", 2) == 0) ||
		(strncmp(in, "--", 2) == 0))
		return 1;

	return 0;
}

static const char cmd_mempool_help[] =
"mempool <mempool_name>\n"
"   buffer <buffer_size>\n"
"   pool <pool_size>\n"
"   cache <cache_size>\n"
"   cpu <cpu_id>\n";

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

static const char cmd_link_help[] =
"link <link_name>\n"
"   dev <device_name> | port <port_id>\n"
"   rxq <n_queues> <queue_size> <mempool_name>\n"
"   txq <n_queues> <queue_size>\n"
"   promiscuous on | off\n"
"   [rss <qid_0> ... <qid_n>]\n";

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

	memset(&p, 0, sizeof(p));

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

/* Print the link stats and info */
static void
print_link_info(struct link *link, char *out, size_t out_size)
{
	struct rte_eth_stats stats;
	struct rte_ether_addr mac_addr;
	struct rte_eth_link eth_link;
	uint16_t mtu;
	int ret;

	memset(&stats, 0, sizeof(stats));
	rte_eth_stats_get(link->port_id, &stats);

	ret = rte_eth_macaddr_get(link->port_id, &mac_addr);
	if (ret != 0) {
		snprintf(out, out_size, "\n%s: MAC address get failed: %s",
			 link->name, rte_strerror(-ret));
		return;
	}

	ret = rte_eth_link_get(link->port_id, &eth_link);
	if (ret < 0) {
		snprintf(out, out_size, "\n%s: link get failed: %s",
			 link->name, rte_strerror(-ret));
		return;
	}

	rte_eth_dev_get_mtu(link->port_id, &mtu);

	snprintf(out, out_size,
		"\n"
		"%s: flags=<%s> mtu %u\n"
		"\tether %02X:%02X:%02X:%02X:%02X:%02X rxqueues %u txqueues %u\n"
		"\tport# %u  speed %u Mbps\n"
		"\tRX packets %" PRIu64"  bytes %" PRIu64"\n"
		"\tRX errors %" PRIu64"  missed %" PRIu64"  no-mbuf %" PRIu64"\n"
		"\tTX packets %" PRIu64"  bytes %" PRIu64"\n"
		"\tTX errors %" PRIu64"\n",
		link->name,
		eth_link.link_status == 0 ? "DOWN" : "UP",
		mtu,
		mac_addr.addr_bytes[0], mac_addr.addr_bytes[1],
		mac_addr.addr_bytes[2], mac_addr.addr_bytes[3],
		mac_addr.addr_bytes[4], mac_addr.addr_bytes[5],
		link->n_rxq,
		link->n_txq,
		link->port_id,
		eth_link.link_speed,
		stats.ipackets,
		stats.ibytes,
		stats.ierrors,
		stats.imissed,
		stats.rx_nombuf,
		stats.opackets,
		stats.obytes,
		stats.oerrors);
}

/*
 * link show [<link_name>]
 */
static void
cmd_link_show(char **tokens, uint32_t n_tokens, char *out, size_t out_size)
{
	struct link *link;
	char *link_name;

	if (n_tokens != 2 && n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (n_tokens == 2) {
		link = link_next(NULL);

		while (link != NULL) {
			out_size = out_size - strlen(out);
			out = &out[strlen(out)];

			print_link_info(link, out, out_size);
			link = link_next(link);
		}
	} else {
		out_size = out_size - strlen(out);
		out = &out[strlen(out)];

		link_name = tokens[2];
		link = link_find(link_name);

		if (link == NULL) {
			snprintf(out, out_size, MSG_ARG_INVALID,
					"Link does not exist");
			return;
		}
		print_link_info(link, out, out_size);
	}
}

static const char cmd_swq_help[] =
"swq <swq_name>\n"
"   size <size>\n"
"   cpu <cpu_id>\n";

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

static const char cmd_tmgr_subport_profile_help[] =
"tmgr subport profile\n"
"   <tb_rate> <tb_size>\n"
"   <tc0_rate> <tc1_rate> <tc2_rate> <tc3_rate> <tc4_rate>"
"        <tc5_rate> <tc6_rate> <tc7_rate> <tc8_rate>"
"        <tc9_rate> <tc10_rate> <tc11_rate> <tc12_rate>\n"
"   <tc_period>\n"
"   pps <n_pipes_per_subport>\n"
"   qsize <qsize_tc0> <qsize_tc1> <qsize_tc2>"
"       <qsize_tc3> <qsize_tc4> <qsize_tc5> <qsize_tc6>"
"       <qsize_tc7> <qsize_tc8> <qsize_tc9> <qsize_tc10>"
"       <qsize_tc11> <qsize_tc12>";

static void
cmd_tmgr_subport_profile(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_sched_subport_params p;
	int status, i;

	if (n_tokens != 35) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint64(&p.tb_rate, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_rate");
		return;
	}

	if (parser_read_uint64(&p.tb_size, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_size");
		return;
	}

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		if (parser_read_uint64(&p.tc_rate[i], tokens[5 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "tc_rate");
			return;
		}

	if (parser_read_uint64(&p.tc_period, tokens[18]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc_period");
		return;
	}

	if (strcmp(tokens[19], "pps") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pps");
		return;
	}

	if (parser_read_uint32(&p.n_pipes_per_subport_enabled, tokens[20]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_pipes_per_subport");
		return;
	}

	if (strcmp(tokens[21], "qsize") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "qsize");
		return;
	}

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		if (parser_read_uint16(&p.qsize[i], tokens[22 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "qsize");
			return;
		}

	status = tmgr_subport_profile_add(&p);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_tmgr_pipe_profile_help[] =
"tmgr pipe profile\n"
"   <tb_rate> <tb_size>\n"
"   <tc0_rate> <tc1_rate> <tc2_rate> <tc3_rate> <tc4_rate>"
"     <tc5_rate> <tc6_rate> <tc7_rate> <tc8_rate>"
"     <tc9_rate> <tc10_rate> <tc11_rate> <tc12_rate>\n"
"   <tc_period>\n"
"   <tc_ov_weight>\n"
"   <wrr_weight0..3>\n";

static void
cmd_tmgr_pipe_profile(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_sched_pipe_params p;
	int status, i;

	if (n_tokens != 24) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (parser_read_uint64(&p.tb_rate, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_rate");
		return;
	}

	if (parser_read_uint64(&p.tb_size, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tb_size");
		return;
	}

	for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++)
		if (parser_read_uint64(&p.tc_rate[i], tokens[5 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "tc_rate");
			return;
		}

	if (parser_read_uint64(&p.tc_period, tokens[18]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc_period");
		return;
	}

	if (parser_read_uint8(&p.tc_ov_weight, tokens[19]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "tc_ov_weight");
		return;
	}

	for (i = 0; i < RTE_SCHED_BE_QUEUES_PER_PIPE; i++)
		if (parser_read_uint8(&p.wrr_weights[i], tokens[20 + i]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "wrr_weights");
			return;
		}

	status = tmgr_pipe_profile_add(&p);
	if (status != 0) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_tmgr_help[] =
"tmgr <tmgr_name>\n"
"   rate <rate>\n"
"   spp <n_subports_per_port>\n"
"   fo <frame_overhead>\n"
"   mtu <mtu>\n"
"   cpu <cpu_id>\n";

static void
cmd_tmgr(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct tmgr_port_params p;
	char *name;
	struct tmgr_port *tmgr_port;

	if (n_tokens != 12) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "rate") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rate");
		return;
	}

	if (parser_read_uint64(&p.rate, tokens[3]) != 0) {
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

	if (strcmp(tokens[6], "fo") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "fo");
		return;
	}

	if (parser_read_uint32(&p.frame_overhead, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "frame_overhead");
		return;
	}

	if (strcmp(tokens[8], "mtu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mtu");
		return;
	}

	if (parser_read_uint32(&p.mtu, tokens[9]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "mtu");
		return;
	}

	if (strcmp(tokens[10], "cpu") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cpu");
		return;
	}

	if (parser_read_uint32(&p.cpu_id, tokens[11]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cpu_id");
		return;
	}

	tmgr_port = tmgr_port_create(name, &p);
	if (tmgr_port == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_tmgr_subport_help[] =
"tmgr <tmgr_name> subport <subport_id>\n"
"   profile <subport_profile_id>\n";

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


static const char cmd_tmgr_subport_pipe_help[] =
"tmgr <tmgr_name> subport <subport_id> pipe\n"
"   from <pipe_id_first> to <pipe_id_last>\n"
"   profile <pipe_profile_id>\n";

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


static const char cmd_tap_help[] =
"tap <tap_name>\n";

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

static const char cmd_kni_help[] =
"kni <kni_name>\n"
"   link <link_name>\n"
"   mempool <mempool_name>\n"
"   [thread <thread_id>]\n";

static void
cmd_kni(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct kni_params p;
	char *name;
	struct kni *kni;

	memset(&p, 0, sizeof(p));
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

static const char cmd_cryptodev_help[] =
"cryptodev <cryptodev_name>\n"
"   dev <device_name> | dev_id <device_id>\n"
"   queue <n_queues> <queue_size>\n"
"   max_sessions <n_sessions>";

static void
cmd_cryptodev(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct cryptodev_params params;
	char *name;

	memset(&params, 0, sizeof(params));
	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "dev") == 0)
		params.dev_name = tokens[3];
	else if (strcmp(tokens[2], "dev_id") == 0) {
		if (parser_read_uint32(&params.dev_id, tokens[3]) < 0) {
			snprintf(out, out_size,	MSG_ARG_INVALID,
				"dev_id");
			return;
		}
	} else {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"cryptodev");
		return;
	}

	if (strcmp(tokens[4], "queue")) {
		snprintf(out, out_size,	MSG_ARG_NOT_FOUND,
			"queue");
		return;
	}

	if (parser_read_uint32(&params.n_queues, tokens[5]) < 0) {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"q");
		return;
	}

	if (parser_read_uint32(&params.queue_size, tokens[6]) < 0) {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"queue_size");
		return;
	}

	if (strcmp(tokens[7], "max_sessions")) {
		snprintf(out, out_size,	MSG_ARG_NOT_FOUND,
			"max_sessions");
		return;
	}

	if (parser_read_uint32(&params.session_pool_size, tokens[8]) < 0) {
		snprintf(out, out_size,	MSG_ARG_INVALID,
			"queue_size");
		return;
	}

	if (cryptodev_create(name, &params) == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_port_in_action_profile_help[] =
"port in action profile <profile_name>\n"
"   [filter match | mismatch offset <key_offset> mask <key_mask> key <key_value> port <port_id>]\n"
"   [balance offset <key_offset> mask <key_mask> port <port_id0> ... <port_id15>]\n";

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
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"port in action profile balance");
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


static const char cmd_table_action_profile_help[] =
"table action profile <profile_name>\n"
"   ipv4 | ipv6\n"
"   offset <ip_offset>\n"
"   fwd\n"
"   [balance offset <key_offset> mask <key_mask> outoffset <out_offset>]\n"
"   [meter srtcm | trtcm\n"
"       tc <n_tc>\n"
"       stats none | pkts | bytes | both]\n"
"   [tm spp <n_subports_per_port> pps <n_pipes_per_subport>]\n"
"   [encap ether | vlan | qinq | mpls | pppoe | qinq_pppoe \n"
"       vxlan offset <ether_offset> ipv4 | ipv6 vlan on | off]\n"
"   [nat src | dst\n"
"       proto udp | tcp]\n"
"   [ttl drop | fwd\n"
"       stats none | pkts]\n"
"   [stats pkts | bytes | both]\n"
"   [time]\n"
"   [sym_crypto dev <CRYPTODEV_NAME> offset <op_offset>]\n"
"   [tag]\n"
"   [decap]\n";

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
	if ((t0 < n_tokens) && (strcmp(tokens[t0], "balance") == 0)) {
		if (n_tokens < t0 + 7) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "table action profile balance");
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

		if (strcmp(tokens[t0 + 5], "outoffset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "outoffset");
			return;
		}

		if (parser_read_uint32(&p.lb.out_offset, tokens[t0 + 6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "out_offset");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_LB;
		t0 += 7;
	} /* balance */

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
		uint32_t n_extra_tokens = 0;

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
		else if (strcmp(tokens[t0 + 1], "vxlan") == 0) {
			if (n_tokens < t0 + 2 + 5) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					"action profile encap vxlan");
				return;
			}

			if (strcmp(tokens[t0 + 2], "offset") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND,
					"vxlan: offset");
				return;
			}

			if (parser_read_uint32(&p.encap.vxlan.data_offset,
				tokens[t0 + 2 + 1]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"vxlan: ether_offset");
				return;
			}

			if (strcmp(tokens[t0 + 2 + 2], "ipv4") == 0)
				p.encap.vxlan.ip_version = 1;
			else if (strcmp(tokens[t0 + 2 + 2], "ipv6") == 0)
				p.encap.vxlan.ip_version = 0;
			else {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"vxlan: ipv4 or ipv6");
				return;
			}

			if (strcmp(tokens[t0 + 2 + 3], "vlan") != 0) {
				snprintf(out, out_size, MSG_ARG_NOT_FOUND,
					"vxlan: vlan");
				return;
			}

			if (strcmp(tokens[t0 + 2 + 4], "on") == 0)
				p.encap.vxlan.vlan = 1;
			else if (strcmp(tokens[t0 + 2 + 4], "off") == 0)
				p.encap.vxlan.vlan = 0;
			else {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"vxlan: on or off");
				return;
			}

			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_VXLAN;
			n_extra_tokens = 5;
		} else if (strcmp(tokens[t0 + 1], "qinq_pppoe") == 0)
			p.encap.encap_mask =
				1LLU << RTE_TABLE_ACTION_ENCAP_QINQ_PPPOE;
		else {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "encap");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_ENCAP;
		t0 += 2 + n_extra_tokens;
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

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "sym_crypto") == 0)) {
		struct cryptodev *cryptodev;

		if (n_tokens < t0 + 5 ||
				strcmp(tokens[t0 + 1], "dev") ||
				strcmp(tokens[t0 + 3], "offset")) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile sym_crypto");
			return;
		}

		cryptodev = cryptodev_find(tokens[t0 + 2]);
		if (cryptodev == NULL) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"table action profile sym_crypto");
			return;
		}

		p.sym_crypto.cryptodev_id = cryptodev->dev_id;

		if (parser_read_uint32(&p.sym_crypto.op_offset,
				tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
					"table action profile sym_crypto");
			return;
		}

		p.sym_crypto.mp_create = cryptodev->mp_create;
		p.sym_crypto.mp_init = cryptodev->mp_init;

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_SYM_CRYPTO;

		t0 += 5;
	} /* sym_crypto */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "tag") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TAG;
		t0 += 1;
	} /* tag */

	if ((t0 < n_tokens) && (strcmp(tokens[t0], "decap") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_DECAP;
		t0 += 1;
	} /* decap */

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

static const char cmd_pipeline_help[] =
"pipeline <pipeline_name>\n"
"   period <timer_period_ms>\n"
"   offset_port_id <offset_port_id>\n"
"   cpu <cpu_id>\n";

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

static const char cmd_pipeline_port_in_help[] =
"pipeline <pipeline_name> port in\n"
"   bsz <burst_size>\n"
"   link <link_name> rxq <queue_id>\n"
"   | swq <swq_name>\n"
"   | tmgr <tmgr_name>\n"
"   | tap <tap_name> mempool <mempool_name> mtu <mtu>\n"
"   | kni <kni_name>\n"
"   | source mempool <mempool_name> file <file_name> bpp <n_bytes_per_pkt>\n"
"   | cryptodev <cryptodev_name> rxq <queue_id>\n"
"   [action <port_in_action_profile_name>]\n"
"   [disabled]\n";

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
	} else if (strcmp(tokens[t0], "cryptodev") == 0) {
		if (n_tokens < t0 + 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in cryptodev");
			return;
		}

		p.type = PORT_IN_CRYPTODEV;

		p.dev_name = tokens[t0 + 1];
		if (parser_read_uint16(&p.rxq.queue_id, tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"rxq");
			return;
		}

		p.cryptodev.arg_callback = NULL;
		p.cryptodev.f_callback = NULL;

		t0 += 4;
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

static const char cmd_pipeline_port_out_help[] =
"pipeline <pipeline_name> port out\n"
"   bsz <burst_size>\n"
"   link <link_name> txq <txq_id>\n"
"   | swq <swq_name>\n"
"   | tmgr <tmgr_name>\n"
"   | tap <tap_name>\n"
"   | kni <kni_name>\n"
"   | sink [file <file_name> pkts <max_n_pkts>]\n"
"   | cryptodev <cryptodev_name> txq <txq_id> offset <crypto_op_offset>\n";

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

	} else if (strcmp(tokens[6], "cryptodev") == 0) {
		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out cryptodev");
			return;
		}

		p.type = PORT_OUT_CRYPTODEV;

		p.dev_name = tokens[7];

		if (strcmp(tokens[8], "txq")) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out cryptodev");
			return;
		}

		if (parser_read_uint16(&p.cryptodev.queue_id, tokens[9])
				!= 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "queue_id");
			return;
		}

		if (strcmp(tokens[10], "offset")) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out cryptodev");
			return;
		}

		if (parser_read_uint32(&p.cryptodev.op_offset, tokens[11])
				!= 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "queue_id");
			return;
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

static const char cmd_pipeline_table_help[] =
"pipeline <pipeline_name> table\n"
"       match\n"
"       acl\n"
"           ipv4 | ipv6\n"
"           offset <ip_header_offset>\n"
"           size <n_rules>\n"
"       | array\n"
"           offset <key_offset>\n"
"           size <n_keys>\n"
"       | hash\n"
"           ext | lru\n"
"           key <key_size>\n"
"           mask <key_mask>\n"
"           offset <key_offset>\n"
"           buckets <n_buckets>\n"
"           size <n_keys>\n"
"       | lpm\n"
"           ipv4 | ipv6\n"
"           offset <ip_header_offset>\n"
"           size <n_rules>\n"
"       | stub\n"
"   [action <table_action_profile_name>]\n";

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

static const char cmd_pipeline_port_in_table_help[] =
"pipeline <pipeline_name> port in <port_id> table <table_id>\n";

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


static const char cmd_pipeline_port_in_stats_help[] =
"pipeline <pipeline_name> port in <port_id> stats read [clear]\n";

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


static const char cmd_pipeline_port_in_enable_help[] =
"pipeline <pipeline_name> port in <port_id> enable\n";

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


static const char cmd_pipeline_port_in_disable_help[] =
"pipeline <pipeline_name> port in <port_id> disable\n";

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


static const char cmd_pipeline_port_out_stats_help[] =
"pipeline <pipeline_name> port out <port_id> stats read [clear]\n";

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


static const char cmd_pipeline_table_stats_help[] =
"pipeline <pipeline_name> table <table_id> stats read [clear]\n";

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
 * <match> ::=
 *
 * match
 *    acl
 *       priority <priority>
 *       ipv4 | ipv6 <sa> <sa_depth> <da> <da_depth>
 *       <sp0> <sp1> <dp0> <dp1> <proto>
 *    | array <pos>
 *    | hash
 *       raw <key>
 *       | ipv4_5tuple <sa> <da> <sp> <dp> <proto>
 *       | ipv6_5tuple <sa> <da> <sp> <dp> <proto>
 *       | ipv4_addr <addr>
 *       | ipv6_addr <addr>
 *       | qinq <svlan> <cvlan>
 *    | lpm
 *       ipv4 | ipv6 <addr> <depth>
 */
struct pkt_key_qinq {
	uint16_t ethertype_svlan;
	uint16_t svlan;
	uint16_t ethertype_cvlan;
	uint16_t cvlan;
} __rte_packed;

struct pkt_key_ipv4_5tuple {
	uint8_t time_to_live;
	uint8_t proto;
	uint16_t hdr_checksum;
	uint32_t sa;
	uint32_t da;
	uint16_t sp;
	uint16_t dp;
} __rte_packed;

struct pkt_key_ipv6_5tuple {
	uint16_t payload_length;
	uint8_t proto;
	uint8_t hop_limit;
	uint8_t sa[16];
	uint8_t da[16];
	uint16_t sp;
	uint16_t dp;
} __rte_packed;

struct pkt_key_ipv4_addr {
	uint32_t addr;
} __rte_packed;

struct pkt_key_ipv6_addr {
	uint8_t addr[16];
} __rte_packed;

static uint32_t
parse_match(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	struct table_rule_match *m)
{
	memset(m, 0, sizeof(*m));

	if (n_tokens < 2)
		return 0;

	if (strcmp(tokens[0], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return 0;
	}

	if (strcmp(tokens[1], "acl") == 0) {
		if (n_tokens < 14) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_ACL;

		if (strcmp(tokens[2], "priority") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "priority");
			return 0;
		}

		if (parser_read_uint32(&m->match.acl.priority,
			tokens[3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "priority");
			return 0;
		}

		if (strcmp(tokens[4], "ipv4") == 0) {
			struct in_addr saddr, daddr;

			m->match.acl.ip_version = 1;

			if (parse_ipv4_addr(tokens[5], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}
			m->match.acl.ipv4.sa = rte_be_to_cpu_32(saddr.s_addr);

			if (parse_ipv4_addr(tokens[7], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}
			m->match.acl.ipv4.da = rte_be_to_cpu_32(daddr.s_addr);
		} else if (strcmp(tokens[4], "ipv6") == 0) {
			struct in6_addr saddr, daddr;

			m->match.acl.ip_version = 0;

			if (parse_ipv6_addr(tokens[5], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}
			memcpy(m->match.acl.ipv6.sa, saddr.s6_addr, 16);

			if (parse_ipv6_addr(tokens[7], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}
			memcpy(m->match.acl.ipv6.da, daddr.s6_addr, 16);
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return 0;
		}

		if (parser_read_uint32(&m->match.acl.sa_depth,
			tokens[6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "sa_depth");
			return 0;
		}

		if (parser_read_uint32(&m->match.acl.da_depth,
			tokens[8]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "da_depth");
			return 0;
		}

		if (parser_read_uint16(&m->match.acl.sp0, tokens[9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "sp0");
			return 0;
		}

		if (parser_read_uint16(&m->match.acl.sp1, tokens[10]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "sp1");
			return 0;
		}

		if (parser_read_uint16(&m->match.acl.dp0, tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "dp0");
			return 0;
		}

		if (parser_read_uint16(&m->match.acl.dp1, tokens[12]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "dp1");
			return 0;
		}

		if (parser_read_uint8(&m->match.acl.proto, tokens[13]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "proto");
			return 0;
		}

		m->match.acl.proto_mask = 0xff;

		return 14;
	} /* acl */

	if (strcmp(tokens[1], "array") == 0) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_ARRAY;

		if (parser_read_uint32(&m->match.array.pos, tokens[2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "pos");
			return 0;
		}

		return 3;
	} /* array */

	if (strcmp(tokens[1], "hash") == 0) {
		if (n_tokens < 3) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_HASH;

		if (strcmp(tokens[2], "raw") == 0) {
			uint32_t key_size = TABLE_RULE_MATCH_SIZE_MAX;

			if (n_tokens < 4) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (parse_hex_string(tokens[3],
				m->match.hash.key, &key_size) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "key");
				return 0;
			}

			return 4;
		} /* hash raw */

		if (strcmp(tokens[2], "ipv4_5tuple") == 0) {
			struct pkt_key_ipv4_5tuple *ipv4 =
				(struct pkt_key_ipv4_5tuple *) m->match.hash.key;
			struct in_addr saddr, daddr;
			uint16_t sp, dp;
			uint8_t proto;

			if (n_tokens < 8) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (parse_ipv4_addr(tokens[3], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}

			if (parse_ipv4_addr(tokens[4], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}

			if (parser_read_uint16(&sp, tokens[5]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sp");
				return 0;
			}

			if (parser_read_uint16(&dp, tokens[6]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "dp");
				return 0;
			}

			if (parser_read_uint8(&proto, tokens[7]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"proto");
				return 0;
			}

			ipv4->sa = saddr.s_addr;
			ipv4->da = daddr.s_addr;
			ipv4->sp = rte_cpu_to_be_16(sp);
			ipv4->dp = rte_cpu_to_be_16(dp);
			ipv4->proto = proto;

			return 8;
		} /* hash ipv4_5tuple */

		if (strcmp(tokens[2], "ipv6_5tuple") == 0) {
			struct pkt_key_ipv6_5tuple *ipv6 =
				(struct pkt_key_ipv6_5tuple *) m->match.hash.key;
			struct in6_addr saddr, daddr;
			uint16_t sp, dp;
			uint8_t proto;

			if (n_tokens < 8) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (parse_ipv6_addr(tokens[3], &saddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sa");
				return 0;
			}

			if (parse_ipv6_addr(tokens[4], &daddr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "da");
				return 0;
			}

			if (parser_read_uint16(&sp, tokens[5]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "sp");
				return 0;
			}

			if (parser_read_uint16(&dp, tokens[6]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "dp");
				return 0;
			}

			if (parser_read_uint8(&proto, tokens[7]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"proto");
				return 0;
			}

			memcpy(ipv6->sa, saddr.s6_addr, 16);
			memcpy(ipv6->da, daddr.s6_addr, 16);
			ipv6->sp = rte_cpu_to_be_16(sp);
			ipv6->dp = rte_cpu_to_be_16(dp);
			ipv6->proto = proto;

			return 8;
		} /* hash ipv6_5tuple */

		if (strcmp(tokens[2], "ipv4_addr") == 0) {
			struct pkt_key_ipv4_addr *ipv4_addr =
				(struct pkt_key_ipv4_addr *) m->match.hash.key;
			struct in_addr addr;

			if (n_tokens < 4) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (parse_ipv4_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			ipv4_addr->addr = addr.s_addr;

			return 4;
		} /* hash ipv4_addr */

		if (strcmp(tokens[2], "ipv6_addr") == 0) {
			struct pkt_key_ipv6_addr *ipv6_addr =
				(struct pkt_key_ipv6_addr *) m->match.hash.key;
			struct in6_addr addr;

			if (n_tokens < 4) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if (parse_ipv6_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			memcpy(ipv6_addr->addr, addr.s6_addr, 16);

			return 4;
		} /* hash ipv6_5tuple */

		if (strcmp(tokens[2], "qinq") == 0) {
			struct pkt_key_qinq *qinq =
				(struct pkt_key_qinq *) m->match.hash.key;
			uint16_t svlan, cvlan;

			if (n_tokens < 5) {
				snprintf(out, out_size, MSG_ARG_MISMATCH,
					tokens[0]);
				return 0;
			}

			if ((parser_read_uint16(&svlan, tokens[3]) != 0) ||
				(svlan > 0xFFF)) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"svlan");
				return 0;
			}

			if ((parser_read_uint16(&cvlan, tokens[4]) != 0) ||
				(cvlan > 0xFFF)) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"cvlan");
				return 0;
			}

			qinq->svlan = rte_cpu_to_be_16(svlan);
			qinq->cvlan = rte_cpu_to_be_16(cvlan);

			return 5;
		} /* hash qinq */

		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return 0;
	} /* hash */

	if (strcmp(tokens[1], "lpm") == 0) {
		if (n_tokens < 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return 0;
		}

		m->match_type = TABLE_LPM;

		if (strcmp(tokens[2], "ipv4") == 0) {
			struct in_addr addr;

			m->match.lpm.ip_version = 1;

			if (parse_ipv4_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			m->match.lpm.ipv4 = rte_be_to_cpu_32(addr.s_addr);
		} else if (strcmp(tokens[2], "ipv6") == 0) {
			struct in6_addr addr;

			m->match.lpm.ip_version = 0;

			if (parse_ipv6_addr(tokens[3], &addr) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"addr");
				return 0;
			}

			memcpy(m->match.lpm.ipv6, addr.s6_addr, 16);
		} else {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"ipv4 or ipv6");
			return 0;
		}

		if (parser_read_uint8(&m->match.lpm.depth, tokens[4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "depth");
			return 0;
		}

		return 5;
	} /* lpm */

	snprintf(out, out_size, MSG_ARG_MISMATCH,
		"acl or array or hash or lpm");
	return 0;
}

/**
 * table_action ::=
 *
 * action
 *    fwd
 *       drop
 *       | port <port_id>
 *       | meta
 *       | table <table_id>
 *    [balance <out0> ... <out7>]
 *    [meter
 *       tc0 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
 *       [tc1 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
 *       tc2 meter <meter_profile_id> policer g <pa> y <pa> r <pa>
 *       tc3 meter <meter_profile_id> policer g <pa> y <pa> r <pa>]]
 *    [tm subport <subport_id> pipe <pipe_id>]
 *    [encap
 *       ether <da> <sa>
 *       | vlan <da> <sa> <pcp> <dei> <vid>
 *       | qinq <da> <sa> <pcp> <dei> <vid> <pcp> <dei> <vid>
 *       | qinq_pppoe <da> <sa> <pcp> <dei> <vid> <pcp> <dei> <vid> <session_id>
 *       | mpls unicast | multicast
 *          <da> <sa>
 *          label0 <label> <tc> <ttl>
 *          [label1 <label> <tc> <ttl>
 *          [label2 <label> <tc> <ttl>
 *          [label3 <label> <tc> <ttl>]]]
 *       | pppoe <da> <sa> <session_id>
 *       | vxlan ether <da> <sa>
 *          [vlan <pcp> <dei> <vid>]
 *          ipv4 <sa> <da> <dscp> <ttl>
 *          | ipv6 <sa> <da> <flow_label> <dscp> <hop_limit>
 *          udp <sp> <dp>
 *          vxlan <vni>]
 *    [nat ipv4 | ipv6 <addr> <port>]
 *    [ttl dec | keep]
 *    [stats]
 *    [time]
 *    [sym_crypto
 *       encrypt | decrypt
 *       type
 *       | cipher
 *          cipher_algo <algo> cipher_key <key> cipher_iv <iv>
 *       | cipher_auth
 *          cipher_algo <algo> cipher_key <key> cipher_iv <iv>
 *          auth_algo <algo> auth_key <key> digest_size <size>
 *       | aead
 *          aead_algo <algo> aead_key <key> aead_iv <iv> aead_aad <aad>
 *          digest_size <size>
 *       data_offset <data_offset>]
 *    [tag <tag>]
 *    [decap <n>]
 *
 * where:
 *    <pa> ::= g | y | r | drop
 */
static uint32_t
parse_table_action_fwd(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens == 0) || (strcmp(tokens[0], "fwd") != 0))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens && (strcmp(tokens[0], "drop") == 0)) {
		a->fwd.action = RTE_PIPELINE_ACTION_DROP;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 1;
	}

	if (n_tokens && (strcmp(tokens[0], "port") == 0)) {
		uint32_t id;

		if ((n_tokens < 2) ||
			parser_read_uint32(&id, tokens[1]))
			return 0;

		a->fwd.action = RTE_PIPELINE_ACTION_PORT;
		a->fwd.id = id;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 2;
	}

	if (n_tokens && (strcmp(tokens[0], "meta") == 0)) {
		a->fwd.action = RTE_PIPELINE_ACTION_PORT_META;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 1;
	}

	if (n_tokens && (strcmp(tokens[0], "table") == 0)) {
		uint32_t id;

		if ((n_tokens < 2) ||
			parser_read_uint32(&id, tokens[1]))
			return 0;

		a->fwd.action = RTE_PIPELINE_ACTION_TABLE;
		a->fwd.id = id;
		a->action_mask |= 1 << RTE_TABLE_ACTION_FWD;
		return 1 + 2;
	}

	return 0;
}

static uint32_t
parse_table_action_balance(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	uint32_t i;

	if ((n_tokens == 0) || (strcmp(tokens[0], "balance") != 0))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens < RTE_TABLE_ACTION_LB_TABLE_SIZE)
		return 0;

	for (i = 0; i < RTE_TABLE_ACTION_LB_TABLE_SIZE; i++)
		if (parser_read_uint32(&a->lb.out[i], tokens[i]) != 0)
			return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_LB;
	return 1 + RTE_TABLE_ACTION_LB_TABLE_SIZE;

}

static int
parse_policer_action(char *token, enum rte_table_action_policer *a)
{
	if (strcmp(token, "g") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_COLOR_GREEN;
		return 0;
	}

	if (strcmp(token, "y") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_COLOR_YELLOW;
		return 0;
	}

	if (strcmp(token, "r") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_COLOR_RED;
		return 0;
	}

	if (strcmp(token, "drop") == 0) {
		*a = RTE_TABLE_ACTION_POLICER_DROP;
		return 0;
	}

	return -1;
}

static uint32_t
parse_table_action_meter_tc(char **tokens,
	uint32_t n_tokens,
	struct rte_table_action_mtr_tc_params *mtr)
{
	if ((n_tokens < 9) ||
		strcmp(tokens[0], "meter") ||
		parser_read_uint32(&mtr->meter_profile_id, tokens[1]) ||
		strcmp(tokens[2], "policer") ||
		strcmp(tokens[3], "g") ||
		parse_policer_action(tokens[4], &mtr->policer[RTE_COLOR_GREEN]) ||
		strcmp(tokens[5], "y") ||
		parse_policer_action(tokens[6], &mtr->policer[RTE_COLOR_YELLOW]) ||
		strcmp(tokens[7], "r") ||
		parse_policer_action(tokens[8], &mtr->policer[RTE_COLOR_RED]))
		return 0;

	return 9;
}

static uint32_t
parse_table_action_meter(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens == 0) || strcmp(tokens[0], "meter"))
		return 0;

	tokens++;
	n_tokens--;

	if ((n_tokens < 10) ||
		strcmp(tokens[0], "tc0") ||
		(parse_table_action_meter_tc(tokens + 1,
			n_tokens - 1,
			&a->mtr.mtr[0]) == 0))
		return 0;

	tokens += 10;
	n_tokens -= 10;

	if ((n_tokens == 0) || strcmp(tokens[0], "tc1")) {
		a->mtr.tc_mask = 1;
		a->action_mask |= 1 << RTE_TABLE_ACTION_MTR;
		return 1 + 10;
	}

	if ((n_tokens < 30) ||
		(parse_table_action_meter_tc(tokens + 1,
			n_tokens - 1, &a->mtr.mtr[1]) == 0) ||
		strcmp(tokens[10], "tc2") ||
		(parse_table_action_meter_tc(tokens + 11,
			n_tokens - 11, &a->mtr.mtr[2]) == 0) ||
		strcmp(tokens[20], "tc3") ||
		(parse_table_action_meter_tc(tokens + 21,
			n_tokens - 21, &a->mtr.mtr[3]) == 0))
		return 0;

	a->mtr.tc_mask = 0xF;
	a->action_mask |= 1 << RTE_TABLE_ACTION_MTR;
	return 1 + 10 + 3 * 10;
}

static uint32_t
parse_table_action_tm(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	uint32_t subport_id, pipe_id;

	if ((n_tokens < 5) ||
		strcmp(tokens[0], "tm") ||
		strcmp(tokens[1], "subport") ||
		parser_read_uint32(&subport_id, tokens[2]) ||
		strcmp(tokens[3], "pipe") ||
		parser_read_uint32(&pipe_id, tokens[4]))
		return 0;

	a->tm.subport_id = subport_id;
	a->tm.pipe_id = pipe_id;
	a->action_mask |= 1 << RTE_TABLE_ACTION_TM;
	return 5;
}

static uint32_t
parse_table_action_encap(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens == 0) || strcmp(tokens[0], "encap"))
		return 0;

	tokens++;
	n_tokens--;

	/* ether */
	if (n_tokens && (strcmp(tokens[0], "ether") == 0)) {
		if ((n_tokens < 3) ||
			parse_mac_addr(tokens[1], &a->encap.ether.ether.da) ||
			parse_mac_addr(tokens[2], &a->encap.ether.ether.sa))
			return 0;

		a->encap.type = RTE_TABLE_ACTION_ENCAP_ETHER;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 3;
	}

	/* vlan */
	if (n_tokens && (strcmp(tokens[0], "vlan") == 0)) {
		uint32_t pcp, dei, vid;

		if ((n_tokens < 6) ||
			parse_mac_addr(tokens[1], &a->encap.vlan.ether.da) ||
			parse_mac_addr(tokens[2], &a->encap.vlan.ether.sa) ||
			parser_read_uint32(&pcp, tokens[3]) ||
			(pcp > 0x7) ||
			parser_read_uint32(&dei, tokens[4]) ||
			(dei > 0x1) ||
			parser_read_uint32(&vid, tokens[5]) ||
			(vid > 0xFFF))
			return 0;

		a->encap.vlan.vlan.pcp = pcp & 0x7;
		a->encap.vlan.vlan.dei = dei & 0x1;
		a->encap.vlan.vlan.vid = vid & 0xFFF;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_VLAN;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 6;
	}

	/* qinq */
	if (n_tokens && (strcmp(tokens[0], "qinq") == 0)) {
		uint32_t svlan_pcp, svlan_dei, svlan_vid;
		uint32_t cvlan_pcp, cvlan_dei, cvlan_vid;

		if ((n_tokens < 9) ||
			parse_mac_addr(tokens[1], &a->encap.qinq.ether.da) ||
			parse_mac_addr(tokens[2], &a->encap.qinq.ether.sa) ||
			parser_read_uint32(&svlan_pcp, tokens[3]) ||
			(svlan_pcp > 0x7) ||
			parser_read_uint32(&svlan_dei, tokens[4]) ||
			(svlan_dei > 0x1) ||
			parser_read_uint32(&svlan_vid, tokens[5]) ||
			(svlan_vid > 0xFFF) ||
			parser_read_uint32(&cvlan_pcp, tokens[6]) ||
			(cvlan_pcp > 0x7) ||
			parser_read_uint32(&cvlan_dei, tokens[7]) ||
			(cvlan_dei > 0x1) ||
			parser_read_uint32(&cvlan_vid, tokens[8]) ||
			(cvlan_vid > 0xFFF))
			return 0;

		a->encap.qinq.svlan.pcp = svlan_pcp & 0x7;
		a->encap.qinq.svlan.dei = svlan_dei & 0x1;
		a->encap.qinq.svlan.vid = svlan_vid & 0xFFF;
		a->encap.qinq.cvlan.pcp = cvlan_pcp & 0x7;
		a->encap.qinq.cvlan.dei = cvlan_dei & 0x1;
		a->encap.qinq.cvlan.vid = cvlan_vid & 0xFFF;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_QINQ;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 9;
	}

	/* qinq_pppoe */
	if (n_tokens && (strcmp(tokens[0], "qinq_pppoe") == 0)) {
		uint32_t svlan_pcp, svlan_dei, svlan_vid;
		uint32_t cvlan_pcp, cvlan_dei, cvlan_vid;

		if ((n_tokens < 10) ||
			parse_mac_addr(tokens[1],
				&a->encap.qinq_pppoe.ether.da) ||
			parse_mac_addr(tokens[2],
				&a->encap.qinq_pppoe.ether.sa) ||
			parser_read_uint32(&svlan_pcp, tokens[3]) ||
			(svlan_pcp > 0x7) ||
			parser_read_uint32(&svlan_dei, tokens[4]) ||
			(svlan_dei > 0x1) ||
			parser_read_uint32(&svlan_vid, tokens[5]) ||
			(svlan_vid > 0xFFF) ||
			parser_read_uint32(&cvlan_pcp, tokens[6]) ||
			(cvlan_pcp > 0x7) ||
			parser_read_uint32(&cvlan_dei, tokens[7]) ||
			(cvlan_dei > 0x1) ||
			parser_read_uint32(&cvlan_vid, tokens[8]) ||
			(cvlan_vid > 0xFFF) ||
			parser_read_uint16(&a->encap.qinq_pppoe.pppoe.session_id,
				tokens[9]))
			return 0;

		a->encap.qinq_pppoe.svlan.pcp = svlan_pcp & 0x7;
		a->encap.qinq_pppoe.svlan.dei = svlan_dei & 0x1;
		a->encap.qinq_pppoe.svlan.vid = svlan_vid & 0xFFF;
		a->encap.qinq_pppoe.cvlan.pcp = cvlan_pcp & 0x7;
		a->encap.qinq_pppoe.cvlan.dei = cvlan_dei & 0x1;
		a->encap.qinq_pppoe.cvlan.vid = cvlan_vid & 0xFFF;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_QINQ_PPPOE;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 10;

	}

	/* mpls */
	if (n_tokens && (strcmp(tokens[0], "mpls") == 0)) {
		uint32_t label, tc, ttl;

		if (n_tokens < 8)
			return 0;

		if (strcmp(tokens[1], "unicast") == 0)
			a->encap.mpls.unicast = 1;
		else if (strcmp(tokens[1], "multicast") == 0)
			a->encap.mpls.unicast = 0;
		else
			return 0;

		if (parse_mac_addr(tokens[2], &a->encap.mpls.ether.da) ||
			parse_mac_addr(tokens[3], &a->encap.mpls.ether.sa) ||
			strcmp(tokens[4], "label0") ||
			parser_read_uint32(&label, tokens[5]) ||
			(label > 0xFFFFF) ||
			parser_read_uint32(&tc, tokens[6]) ||
			(tc > 0x7) ||
			parser_read_uint32(&ttl, tokens[7]) ||
			(ttl > 0x3F))
			return 0;

		a->encap.mpls.mpls[0].label = label;
		a->encap.mpls.mpls[0].tc = tc;
		a->encap.mpls.mpls[0].ttl = ttl;

		tokens += 8;
		n_tokens -= 8;

		if ((n_tokens == 0) || strcmp(tokens[0], "label1")) {
			a->encap.mpls.mpls_count = 1;
			a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
			a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
			return 1 + 8;
		}

		if ((n_tokens < 4) ||
			parser_read_uint32(&label, tokens[1]) ||
			(label > 0xFFFFF) ||
			parser_read_uint32(&tc, tokens[2]) ||
			(tc > 0x7) ||
			parser_read_uint32(&ttl, tokens[3]) ||
			(ttl > 0x3F))
			return 0;

		a->encap.mpls.mpls[1].label = label;
		a->encap.mpls.mpls[1].tc = tc;
		a->encap.mpls.mpls[1].ttl = ttl;

		tokens += 4;
		n_tokens -= 4;

		if ((n_tokens == 0) || strcmp(tokens[0], "label2")) {
			a->encap.mpls.mpls_count = 2;
			a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
			a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
			return 1 + 8 + 4;
		}

		if ((n_tokens < 4) ||
			parser_read_uint32(&label, tokens[1]) ||
			(label > 0xFFFFF) ||
			parser_read_uint32(&tc, tokens[2]) ||
			(tc > 0x7) ||
			parser_read_uint32(&ttl, tokens[3]) ||
			(ttl > 0x3F))
			return 0;

		a->encap.mpls.mpls[2].label = label;
		a->encap.mpls.mpls[2].tc = tc;
		a->encap.mpls.mpls[2].ttl = ttl;

		tokens += 4;
		n_tokens -= 4;

		if ((n_tokens == 0) || strcmp(tokens[0], "label3")) {
			a->encap.mpls.mpls_count = 3;
			a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
			a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
			return 1 + 8 + 4 + 4;
		}

		if ((n_tokens < 4) ||
			parser_read_uint32(&label, tokens[1]) ||
			(label > 0xFFFFF) ||
			parser_read_uint32(&tc, tokens[2]) ||
			(tc > 0x7) ||
			parser_read_uint32(&ttl, tokens[3]) ||
			(ttl > 0x3F))
			return 0;

		a->encap.mpls.mpls[3].label = label;
		a->encap.mpls.mpls[3].tc = tc;
		a->encap.mpls.mpls[3].ttl = ttl;

		a->encap.mpls.mpls_count = 4;
		a->encap.type = RTE_TABLE_ACTION_ENCAP_MPLS;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 8 + 4 + 4 + 4;
	}

	/* pppoe */
	if (n_tokens && (strcmp(tokens[0], "pppoe") == 0)) {
		if ((n_tokens < 4) ||
			parse_mac_addr(tokens[1], &a->encap.pppoe.ether.da) ||
			parse_mac_addr(tokens[2], &a->encap.pppoe.ether.sa) ||
			parser_read_uint16(&a->encap.pppoe.pppoe.session_id,
				tokens[3]))
			return 0;

		a->encap.type = RTE_TABLE_ACTION_ENCAP_PPPOE;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + 4;
	}

	/* vxlan */
	if (n_tokens && (strcmp(tokens[0], "vxlan") == 0)) {
		uint32_t n = 0;

		n_tokens--;
		tokens++;
		n++;

		/* ether <da> <sa> */
		if ((n_tokens < 3) ||
			strcmp(tokens[0], "ether") ||
			parse_mac_addr(tokens[1], &a->encap.vxlan.ether.da) ||
			parse_mac_addr(tokens[2], &a->encap.vxlan.ether.sa))
			return 0;

		n_tokens -= 3;
		tokens += 3;
		n += 3;

		/* [vlan <pcp> <dei> <vid>] */
		if (strcmp(tokens[0], "vlan") == 0) {
			uint32_t pcp, dei, vid;

			if ((n_tokens < 4) ||
				parser_read_uint32(&pcp, tokens[1]) ||
				(pcp > 7) ||
				parser_read_uint32(&dei, tokens[2]) ||
				(dei > 1) ||
				parser_read_uint32(&vid, tokens[3]) ||
				(vid > 0xFFF))
				return 0;

			a->encap.vxlan.vlan.pcp = pcp;
			a->encap.vxlan.vlan.dei = dei;
			a->encap.vxlan.vlan.vid = vid;

			n_tokens -= 4;
			tokens += 4;
			n += 4;
		}

		/* ipv4 <sa> <da> <dscp> <ttl>
		   | ipv6 <sa> <da> <flow_label> <dscp> <hop_limit> */
		if (strcmp(tokens[0], "ipv4") == 0) {
			struct in_addr sa, da;
			uint8_t dscp, ttl;

			if ((n_tokens < 5) ||
				parse_ipv4_addr(tokens[1], &sa) ||
				parse_ipv4_addr(tokens[2], &da) ||
				parser_read_uint8(&dscp, tokens[3]) ||
				(dscp > 64) ||
				parser_read_uint8(&ttl, tokens[4]))
				return 0;

			a->encap.vxlan.ipv4.sa = rte_be_to_cpu_32(sa.s_addr);
			a->encap.vxlan.ipv4.da = rte_be_to_cpu_32(da.s_addr);
			a->encap.vxlan.ipv4.dscp = dscp;
			a->encap.vxlan.ipv4.ttl = ttl;

			n_tokens -= 5;
			tokens += 5;
			n += 5;
		} else if (strcmp(tokens[0], "ipv6") == 0) {
			struct in6_addr sa, da;
			uint32_t flow_label;
			uint8_t dscp, hop_limit;

			if ((n_tokens < 6) ||
				parse_ipv6_addr(tokens[1], &sa) ||
				parse_ipv6_addr(tokens[2], &da) ||
				parser_read_uint32(&flow_label, tokens[3]) ||
				parser_read_uint8(&dscp, tokens[4]) ||
				(dscp > 64) ||
				parser_read_uint8(&hop_limit, tokens[5]))
				return 0;

			memcpy(a->encap.vxlan.ipv6.sa, sa.s6_addr, 16);
			memcpy(a->encap.vxlan.ipv6.da, da.s6_addr, 16);
			a->encap.vxlan.ipv6.flow_label = flow_label;
			a->encap.vxlan.ipv6.dscp = dscp;
			a->encap.vxlan.ipv6.hop_limit = hop_limit;

			n_tokens -= 6;
			tokens += 6;
			n += 6;
		} else
			return 0;

		/* udp <sp> <dp> */
		if ((n_tokens < 3) ||
			strcmp(tokens[0], "udp") ||
			parser_read_uint16(&a->encap.vxlan.udp.sp, tokens[1]) ||
			parser_read_uint16(&a->encap.vxlan.udp.dp, tokens[2]))
			return 0;

		n_tokens -= 3;
		tokens += 3;
		n += 3;

		/* vxlan <vni> */
		if ((n_tokens < 2) ||
			strcmp(tokens[0], "vxlan") ||
			parser_read_uint32(&a->encap.vxlan.vxlan.vni, tokens[1]) ||
			(a->encap.vxlan.vxlan.vni > 0xFFFFFF))
			return 0;

		n_tokens -= 2;
		tokens += 2;
		n += 2;

		a->encap.type = RTE_TABLE_ACTION_ENCAP_VXLAN;
		a->action_mask |= 1 << RTE_TABLE_ACTION_ENCAP;
		return 1 + n;
	}

	return 0;
}

static uint32_t
parse_table_action_nat(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens < 4) ||
		strcmp(tokens[0], "nat"))
		return 0;

	if (strcmp(tokens[1], "ipv4") == 0) {
		struct in_addr addr;
		uint16_t port;

		if (parse_ipv4_addr(tokens[2], &addr) ||
			parser_read_uint16(&port, tokens[3]))
			return 0;

		a->nat.ip_version = 1;
		a->nat.addr.ipv4 = rte_be_to_cpu_32(addr.s_addr);
		a->nat.port = port;
		a->action_mask |= 1 << RTE_TABLE_ACTION_NAT;
		return 4;
	}

	if (strcmp(tokens[1], "ipv6") == 0) {
		struct in6_addr addr;
		uint16_t port;

		if (parse_ipv6_addr(tokens[2], &addr) ||
			parser_read_uint16(&port, tokens[3]))
			return 0;

		a->nat.ip_version = 0;
		memcpy(a->nat.addr.ipv6, addr.s6_addr, 16);
		a->nat.port = port;
		a->action_mask |= 1 << RTE_TABLE_ACTION_NAT;
		return 4;
	}

	return 0;
}

static uint32_t
parse_table_action_ttl(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens < 2) ||
		strcmp(tokens[0], "ttl"))
		return 0;

	if (strcmp(tokens[1], "dec") == 0)
		a->ttl.decrement = 1;
	else if (strcmp(tokens[1], "keep") == 0)
		a->ttl.decrement = 0;
	else
		return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_TTL;
	return 2;
}

static uint32_t
parse_table_action_stats(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens < 1) ||
		strcmp(tokens[0], "stats"))
		return 0;

	a->stats.n_packets = 0;
	a->stats.n_bytes = 0;
	a->action_mask |= 1 << RTE_TABLE_ACTION_STATS;
	return 1;
}

static uint32_t
parse_table_action_time(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens < 1) ||
		strcmp(tokens[0], "time"))
		return 0;

	a->time.time = rte_rdtsc();
	a->action_mask |= 1 << RTE_TABLE_ACTION_TIME;
	return 1;
}

static void
parse_free_sym_crypto_param_data(struct rte_table_action_sym_crypto_params *p)
{
	struct rte_crypto_sym_xform *xform[2] = {NULL};
	uint32_t i;

	xform[0] = p->xform;
	if (xform[0])
		xform[1] = xform[0]->next;

	for (i = 0; i < 2; i++) {
		if (xform[i] == NULL)
			continue;

		switch (xform[i]->type) {
		case RTE_CRYPTO_SYM_XFORM_CIPHER:
			if (p->cipher_auth.cipher_iv.val)
				free(p->cipher_auth.cipher_iv.val);
			if (p->cipher_auth.cipher_iv_update.val)
				free(p->cipher_auth.cipher_iv_update.val);
			break;
		case RTE_CRYPTO_SYM_XFORM_AUTH:
			if (p->cipher_auth.auth_iv.val)
				free(p->cipher_auth.cipher_iv.val);
			if (p->cipher_auth.auth_iv_update.val)
				free(p->cipher_auth.cipher_iv_update.val);
			break;
		case RTE_CRYPTO_SYM_XFORM_AEAD:
			if (p->aead.iv.val)
				free(p->aead.iv.val);
			if (p->aead.aad.val)
				free(p->aead.aad.val);
			break;
		default:
			continue;
		}
	}

}

static struct rte_crypto_sym_xform *
parse_table_action_cipher(struct rte_table_action_sym_crypto_params *p,
		uint8_t *key, uint32_t max_key_len, char **tokens,
		uint32_t n_tokens, uint32_t encrypt, uint32_t *used_n_tokens)
{
	struct rte_crypto_sym_xform *xform_cipher;
	int status;
	size_t len;

	if (n_tokens < 7 || strcmp(tokens[1], "cipher_algo") ||
			strcmp(tokens[3], "cipher_key") ||
			strcmp(tokens[5], "cipher_iv"))
		return NULL;

	xform_cipher = calloc(1, sizeof(*xform_cipher));
	if (xform_cipher == NULL)
		return NULL;

	xform_cipher->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform_cipher->cipher.op = encrypt ? RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;

	/* cipher_algo */
	status = rte_cryptodev_get_cipher_algo_enum(
			&xform_cipher->cipher.algo, tokens[2]);
	if (status < 0)
		goto error_exit;

	/* cipher_key */
	len = strlen(tokens[4]);
	if (len / 2 > max_key_len) {
		status = -ENOMEM;
		goto error_exit;
	}

	status = parse_hex_string(tokens[4], key, (uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_cipher->cipher.key.data = key;
	xform_cipher->cipher.key.length = (uint16_t)len;

	/* cipher_iv */
	len = strlen(tokens[6]);

	p->cipher_auth.cipher_iv.val = calloc(1, len / 2 + 1);
	if (p->cipher_auth.cipher_iv.val == NULL)
		goto error_exit;

	status = parse_hex_string(tokens[6],
			p->cipher_auth.cipher_iv.val,
			(uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_cipher->cipher.iv.length = (uint16_t)len;
	xform_cipher->cipher.iv.offset = RTE_TABLE_ACTION_SYM_CRYPTO_IV_OFFSET;
	p->cipher_auth.cipher_iv.length = (uint32_t)len;
	*used_n_tokens = 7;

	return xform_cipher;

error_exit:
	if (p->cipher_auth.cipher_iv.val) {
		free(p->cipher_auth.cipher_iv.val);
		p->cipher_auth.cipher_iv.val = NULL;
	}

	free(xform_cipher);

	return NULL;
}

static struct rte_crypto_sym_xform *
parse_table_action_cipher_auth(struct rte_table_action_sym_crypto_params *p,
		uint8_t *key, uint32_t max_key_len, char **tokens,
		uint32_t n_tokens, uint32_t encrypt, uint32_t *used_n_tokens)
{
	struct rte_crypto_sym_xform *xform_cipher;
	struct rte_crypto_sym_xform *xform_auth;
	int status;
	size_t len;

	if (n_tokens < 13 ||
			strcmp(tokens[7], "auth_algo") ||
			strcmp(tokens[9], "auth_key") ||
			strcmp(tokens[11], "digest_size"))
		return NULL;

	xform_auth = calloc(1, sizeof(*xform_auth));
	if (xform_auth == NULL)
		return NULL;

	xform_auth->type = RTE_CRYPTO_SYM_XFORM_AUTH;
	xform_auth->auth.op = encrypt ? RTE_CRYPTO_AUTH_OP_GENERATE :
			RTE_CRYPTO_AUTH_OP_VERIFY;

	/* auth_algo */
	status = rte_cryptodev_get_auth_algo_enum(&xform_auth->auth.algo,
			tokens[8]);
	if (status < 0)
		goto error_exit;

	/* auth_key */
	len = strlen(tokens[10]);
	if (len / 2 > max_key_len) {
		status = -ENOMEM;
		goto error_exit;
	}

	status = parse_hex_string(tokens[10], key, (uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_auth->auth.key.data = key;
	xform_auth->auth.key.length = (uint16_t)len;

	key += xform_auth->auth.key.length;
	max_key_len -= xform_auth->auth.key.length;

	if (strcmp(tokens[11], "digest_size"))
		goto error_exit;

	status = parser_read_uint16(&xform_auth->auth.digest_length,
			tokens[12]);
	if (status < 0)
		goto error_exit;

	xform_cipher = parse_table_action_cipher(p, key, max_key_len, tokens,
			7, encrypt, used_n_tokens);
	if (xform_cipher == NULL)
		goto error_exit;

	*used_n_tokens += 6;

	if (encrypt) {
		xform_cipher->next = xform_auth;
		return xform_cipher;
	} else {
		xform_auth->next = xform_cipher;
		return xform_auth;
	}

error_exit:
	if (p->cipher_auth.auth_iv.val) {
		free(p->cipher_auth.auth_iv.val);
		p->cipher_auth.auth_iv.val = 0;
	}

	free(xform_auth);

	return NULL;
}

static struct rte_crypto_sym_xform *
parse_table_action_aead(struct rte_table_action_sym_crypto_params *p,
		uint8_t *key, uint32_t max_key_len, char **tokens,
		uint32_t n_tokens, uint32_t encrypt, uint32_t *used_n_tokens)
{
	struct rte_crypto_sym_xform *xform_aead;
	int status;
	size_t len;

	if (n_tokens < 11 || strcmp(tokens[1], "aead_algo") ||
			strcmp(tokens[3], "aead_key") ||
			strcmp(tokens[5], "aead_iv") ||
			strcmp(tokens[7], "aead_aad") ||
			strcmp(tokens[9], "digest_size"))
		return NULL;

	xform_aead = calloc(1, sizeof(*xform_aead));
	if (xform_aead == NULL)
		return NULL;

	xform_aead->type = RTE_CRYPTO_SYM_XFORM_AEAD;
	xform_aead->aead.op = encrypt ? RTE_CRYPTO_AEAD_OP_ENCRYPT :
			RTE_CRYPTO_AEAD_OP_DECRYPT;

	/* aead_algo */
	status = rte_cryptodev_get_aead_algo_enum(&xform_aead->aead.algo,
			tokens[2]);
	if (status < 0)
		goto error_exit;

	/* aead_key */
	len = strlen(tokens[4]);
	if (len / 2 > max_key_len) {
		status = -ENOMEM;
		goto error_exit;
	}

	status = parse_hex_string(tokens[4], key, (uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_aead->aead.key.data = key;
	xform_aead->aead.key.length = (uint16_t)len;

	/* aead_iv */
	len = strlen(tokens[6]);
	p->aead.iv.val = calloc(1, len / 2 + 1);
	if (p->aead.iv.val == NULL)
		goto error_exit;

	status = parse_hex_string(tokens[6], p->aead.iv.val,
			(uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_aead->aead.iv.length = (uint16_t)len;
	xform_aead->aead.iv.offset = RTE_TABLE_ACTION_SYM_CRYPTO_IV_OFFSET;
	p->aead.iv.length = (uint32_t)len;

	/* aead_aad */
	len = strlen(tokens[8]);
	p->aead.aad.val = calloc(1, len / 2 + 1);
	if (p->aead.aad.val == NULL)
		goto error_exit;

	status = parse_hex_string(tokens[8], p->aead.aad.val, (uint32_t *)&len);
	if (status < 0)
		goto error_exit;

	xform_aead->aead.aad_length = (uint16_t)len;
	p->aead.aad.length = (uint32_t)len;

	/* digest_size */
	status = parser_read_uint16(&xform_aead->aead.digest_length,
			tokens[10]);
	if (status < 0)
		goto error_exit;

	*used_n_tokens = 11;

	return xform_aead;

error_exit:
	if (p->aead.iv.val) {
		free(p->aead.iv.val);
		p->aead.iv.val = NULL;
	}
	if (p->aead.aad.val) {
		free(p->aead.aad.val);
		p->aead.aad.val = NULL;
	}

	free(xform_aead);

	return NULL;
}


static uint32_t
parse_table_action_sym_crypto(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	struct rte_table_action_sym_crypto_params *p = &a->sym_crypto;
	struct rte_crypto_sym_xform *xform = NULL;
	uint8_t *key = a->sym_crypto_key;
	uint32_t max_key_len = SYM_CRYPTO_MAX_KEY_SIZE;
	uint32_t used_n_tokens;
	uint32_t encrypt;
	int status;

	if ((n_tokens < 12) ||
		strcmp(tokens[0], "sym_crypto") ||
		strcmp(tokens[2], "type"))
		return 0;

	memset(p, 0, sizeof(*p));

	if (strcmp(tokens[1], "encrypt") == 0)
		encrypt = 1;
	else
		encrypt = 0;

	status = parser_read_uint32(&p->data_offset, tokens[n_tokens - 1]);
	if (status < 0)
		return 0;

	if (strcmp(tokens[3], "cipher") == 0) {
		tokens += 3;
		n_tokens -= 3;

		xform = parse_table_action_cipher(p, key, max_key_len, tokens,
				n_tokens, encrypt, &used_n_tokens);
	} else if (strcmp(tokens[3], "cipher_auth") == 0) {
		tokens += 3;
		n_tokens -= 3;

		xform = parse_table_action_cipher_auth(p, key, max_key_len,
				tokens, n_tokens, encrypt, &used_n_tokens);
	} else if (strcmp(tokens[3], "aead") == 0) {
		tokens += 3;
		n_tokens -= 3;

		xform = parse_table_action_aead(p, key, max_key_len, tokens,
				n_tokens, encrypt, &used_n_tokens);
	}

	if (xform == NULL)
		return 0;

	p->xform = xform;

	if (strcmp(tokens[used_n_tokens], "data_offset")) {
		parse_free_sym_crypto_param_data(p);
		return 0;
	}

	a->action_mask |= 1 << RTE_TABLE_ACTION_SYM_CRYPTO;

	return used_n_tokens + 5;
}

static uint32_t
parse_table_action_tag(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens < 2) ||
		strcmp(tokens[0], "tag"))
		return 0;

	if (parser_read_uint32(&a->tag.tag, tokens[1]))
		return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_TAG;
	return 2;
}

static uint32_t
parse_table_action_decap(char **tokens,
	uint32_t n_tokens,
	struct table_rule_action *a)
{
	if ((n_tokens < 2) ||
		strcmp(tokens[0], "decap"))
		return 0;

	if (parser_read_uint16(&a->decap.n, tokens[1]))
		return 0;

	a->action_mask |= 1 << RTE_TABLE_ACTION_DECAP;
	return 2;
}

static uint32_t
parse_table_action(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	struct table_rule_action *a)
{
	uint32_t n_tokens0 = n_tokens;

	memset(a, 0, sizeof(*a));

	if ((n_tokens < 2) ||
		strcmp(tokens[0], "action"))
		return 0;

	tokens++;
	n_tokens--;

	if (n_tokens && (strcmp(tokens[0], "fwd") == 0)) {
		uint32_t n;

		n = parse_table_action_fwd(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action fwd");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "balance") == 0)) {
		uint32_t n;

		n = parse_table_action_balance(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action balance");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "meter") == 0)) {
		uint32_t n;

		n = parse_table_action_meter(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action meter");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "tm") == 0)) {
		uint32_t n;

		n = parse_table_action_tm(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action tm");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "encap") == 0)) {
		uint32_t n;

		n = parse_table_action_encap(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action encap");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "nat") == 0)) {
		uint32_t n;

		n = parse_table_action_nat(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action nat");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "ttl") == 0)) {
		uint32_t n;

		n = parse_table_action_ttl(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action ttl");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "stats") == 0)) {
		uint32_t n;

		n = parse_table_action_stats(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action stats");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "time") == 0)) {
		uint32_t n;

		n = parse_table_action_time(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action time");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "sym_crypto") == 0)) {
		uint32_t n;

		n = parse_table_action_sym_crypto(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action sym_crypto");
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "tag") == 0)) {
		uint32_t n;

		n = parse_table_action_tag(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action tag");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens && (strcmp(tokens[0], "decap") == 0)) {
		uint32_t n;

		n = parse_table_action_decap(tokens, n_tokens, a);
		if (n == 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"action decap");
			return 0;
		}

		tokens += n;
		n_tokens -= n;
	}

	if (n_tokens0 - n_tokens == 1) {
		snprintf(out, out_size, MSG_ARG_INVALID, "action");
		return 0;
	}

	return n_tokens0 - n_tokens;
}


static const char cmd_pipeline_table_rule_add_help[] =
"pipeline <pipeline_name> table <table_id> rule add\n"
"     match <match>\n"
"     action <table_action>\n";

static void
cmd_pipeline_table_rule_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_match m;
	struct table_rule_action a;
	char *pipeline_name;
	uint32_t table_id, t0, n_tokens_parsed;
	int status;

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	t0 = 6;

	/* match */
	n_tokens_parsed = parse_match(tokens + t0,
		n_tokens - t0,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	t0 += n_tokens_parsed;

	/* action */
	n_tokens_parsed = parse_table_action(tokens + t0,
		n_tokens - t0,
		out,
		out_size,
		&a);
	if (n_tokens_parsed == 0)
		return;
	t0 += n_tokens_parsed;

	if (t0 != n_tokens) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	status = pipeline_table_rule_add(pipeline_name, table_id, &m, &a);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	if (a.action_mask & 1 << RTE_TABLE_ACTION_SYM_CRYPTO)
		parse_free_sym_crypto_param_data(&a.sym_crypto);
}


static const char cmd_pipeline_table_rule_add_default_help[] =
"pipeline <pipeline_name> table <table_id> rule add\n"
"     match\n"
"        default\n"
"     action\n"
"        fwd\n"
"           drop\n"
"           | port <port_id>\n"
"           | meta\n"
"           | table <table_id>\n";

static void
cmd_pipeline_table_rule_add_default(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_action action;
	char *pipeline_name;
	uint32_t table_id;
	int status;

	if ((n_tokens != 11) && (n_tokens != 12)) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[6], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "match");
		return;
	}

	if (strcmp(tokens[7], "default") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "default");
		return;
	}

	if (strcmp(tokens[8], "action") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "action");
		return;
	}

	if (strcmp(tokens[9], "fwd") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "fwd");
		return;
	}

	action.action_mask = 1 << RTE_TABLE_ACTION_FWD;

	if (strcmp(tokens[10], "drop") == 0) {
		if (n_tokens != 11) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_DROP;
	} else if (strcmp(tokens[10], "port") == 0) {
		uint32_t id;

		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (parser_read_uint32(&id, tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_PORT;
		action.fwd.id = id;
	} else if (strcmp(tokens[10], "meta") == 0) {
		if (n_tokens != 11) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_PORT_META;
	} else if (strcmp(tokens[10], "table") == 0) {
		uint32_t id;

		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (parser_read_uint32(&id, tokens[11]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
			return;
		}

		action.fwd.action = RTE_PIPELINE_ACTION_TABLE;
		action.fwd.id = id;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID,
			"drop or port or meta or table");
		return;
	}

	status = pipeline_table_rule_add_default(pipeline_name,
		table_id,
		&action);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}


static const char cmd_pipeline_table_rule_add_bulk_help[] =
"pipeline <pipeline_name> table <table_id> rule add bulk <file_name>\n"
"\n"
"  File <file_name>:\n"
"  - line format: match <match> action <action>\n";

static int
cli_rule_file_process(const char *file_name,
	size_t line_len_max,
	struct table_rule_list **rule_list,
	uint32_t *n_rules,
	uint32_t *line_number,
	char *out,
	size_t out_size);

static void
cmd_pipeline_table_rule_add_bulk(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_list *list = NULL;
	char *pipeline_name, *file_name;
	uint32_t table_id, n_rules, n_rules_added, n_rules_not_added, line_number;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[6], "bulk") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "bulk");
		return;
	}

	file_name = tokens[7];

	/* Load rules from file. */
	status = cli_rule_file_process(file_name,
		1024,
		&list,
		&n_rules,
		&line_number,
		out,
		out_size);
	if (status) {
		snprintf(out, out_size, MSG_FILE_ERR, file_name, line_number);
		return;
	}

	/* Rule bulk add */
	status = pipeline_table_rule_add_bulk(pipeline_name,
		table_id,
		list,
		&n_rules_added,
		&n_rules_not_added);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	snprintf(out, out_size, "Added %u rules out of %u.\n",
		n_rules_added,
		n_rules);
}


static const char cmd_pipeline_table_rule_delete_help[] =
"pipeline <pipeline_name> table <table_id> rule delete\n"
"     match <match>\n";

static void
cmd_pipeline_table_rule_delete(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_match m;
	char *pipeline_name;
	uint32_t table_id, n_tokens_parsed, t0;
	int status;

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "delete") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	t0 = 6;

	/* match */
	n_tokens_parsed = parse_match(tokens + t0,
		n_tokens - t0,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	t0 += n_tokens_parsed;

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = pipeline_table_rule_delete(pipeline_name,
		table_id,
		&m);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}


static const char cmd_pipeline_table_rule_delete_default_help[] =
"pipeline <pipeline_name> table <table_id> rule delete\n"
"     match\n"
"        default\n";

static void
cmd_pipeline_table_rule_delete_default(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t table_id;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "delete") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	if (strcmp(tokens[6], "match") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "match");
		return;
	}

	if (strcmp(tokens[7], "default") != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "default");
		return;
	}

	status = pipeline_table_rule_delete_default(pipeline_name,
		table_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static void
ether_addr_show(FILE *f, struct rte_ether_addr *addr)
{
	fprintf(f, "%02x:%02x:%02x:%02x:%02x:%02x",
		(uint32_t)addr->addr_bytes[0], (uint32_t)addr->addr_bytes[1],
		(uint32_t)addr->addr_bytes[2], (uint32_t)addr->addr_bytes[3],
		(uint32_t)addr->addr_bytes[4], (uint32_t)addr->addr_bytes[5]);
}

static void
ipv4_addr_show(FILE *f, uint32_t addr)
{
	fprintf(f, "%u.%u.%u.%u",
		addr >> 24,
		(addr >> 16) & 0xFF,
		(addr >> 8) & 0xFF,
		addr & 0xFF);
}

static void
ipv6_addr_show(FILE *f, uint8_t *addr)
{
	fprintf(f, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
		"%02x%02x:%02x%02x:%02x%02x:%02x%02x:",
		(uint32_t)addr[0], (uint32_t)addr[1],
		(uint32_t)addr[2], (uint32_t)addr[3],
		(uint32_t)addr[4], (uint32_t)addr[5],
		(uint32_t)addr[6], (uint32_t)addr[7],
		(uint32_t)addr[8], (uint32_t)addr[9],
		(uint32_t)addr[10], (uint32_t)addr[11],
		(uint32_t)addr[12], (uint32_t)addr[13],
		(uint32_t)addr[14], (uint32_t)addr[15]);
}

static const char *
policer_action_string(enum rte_table_action_policer action) {
	switch (action) {
		case RTE_TABLE_ACTION_POLICER_COLOR_GREEN: return "G";
		case RTE_TABLE_ACTION_POLICER_COLOR_YELLOW: return "Y";
		case RTE_TABLE_ACTION_POLICER_COLOR_RED: return "R";
		case RTE_TABLE_ACTION_POLICER_DROP: return "D";
		default: return "?";
	}
}

static int
table_rule_show(const char *pipeline_name,
	uint32_t table_id,
	const char *file_name)
{
	struct pipeline *p;
	struct table *table;
	struct table_rule *rule;
	FILE *f = NULL;
	uint32_t i;

	/* Check input params. */
	if ((pipeline_name == NULL) ||
		(file_name == NULL))
		return -1;

	p = pipeline_find(pipeline_name);
	if ((p == NULL) ||
		(table_id >= p->n_tables))
		return -1;

	table = &p->table[table_id];

	/* Open file. */
	f = fopen(file_name, "w");
	if (f == NULL)
		return -1;

	/* Write table rules to file. */
	TAILQ_FOREACH(rule, &table->rules, node) {
		struct table_rule_match *m = &rule->match;
		struct table_rule_action *a = &rule->action;

		fprintf(f, "match ");
		switch (m->match_type) {
		case TABLE_ACL:
			fprintf(f, "acl priority %u ",
				m->match.acl.priority);

			fprintf(f, m->match.acl.ip_version ? "ipv4 " : "ipv6 ");

			if (m->match.acl.ip_version)
				ipv4_addr_show(f, m->match.acl.ipv4.sa);
			else
				ipv6_addr_show(f, m->match.acl.ipv6.sa);

			fprintf(f, "%u",	m->match.acl.sa_depth);

			if (m->match.acl.ip_version)
				ipv4_addr_show(f, m->match.acl.ipv4.da);
			else
				ipv6_addr_show(f, m->match.acl.ipv6.da);

			fprintf(f, "%u",	m->match.acl.da_depth);

			fprintf(f, "%u %u %u %u %u ",
				(uint32_t)m->match.acl.sp0,
				(uint32_t)m->match.acl.sp1,
				(uint32_t)m->match.acl.dp0,
				(uint32_t)m->match.acl.dp1,
				(uint32_t)m->match.acl.proto);
			break;

		case TABLE_ARRAY:
			fprintf(f, "array %u ",
				m->match.array.pos);
			break;

		case TABLE_HASH:
			fprintf(f, "hash raw ");
			for (i = 0; i < table->params.match.hash.key_size; i++)
				fprintf(f, "%02x", m->match.hash.key[i]);
			fprintf(f, " ");
			break;

		case TABLE_LPM:
			fprintf(f, "lpm ");

			fprintf(f, m->match.lpm.ip_version ? "ipv4 " : "ipv6 ");

			if (m->match.acl.ip_version)
				ipv4_addr_show(f, m->match.lpm.ipv4);
			else
				ipv6_addr_show(f, m->match.lpm.ipv6);

			fprintf(f, "%u ",
				(uint32_t)m->match.lpm.depth);
			break;

		default:
			fprintf(f, "unknown ");
		}

		fprintf(f, "action ");
		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_FWD)) {
			fprintf(f, "fwd ");
			switch (a->fwd.action) {
			case RTE_PIPELINE_ACTION_DROP:
				fprintf(f, "drop ");
				break;

			case RTE_PIPELINE_ACTION_PORT:
				fprintf(f, "port %u ", a->fwd.id);
				break;

			case RTE_PIPELINE_ACTION_PORT_META:
				fprintf(f, "meta ");
				break;

			case RTE_PIPELINE_ACTION_TABLE:
			default:
				fprintf(f, "table %u ", a->fwd.id);
			}
		}

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_LB)) {
			fprintf(f, "balance ");
			for (i = 0; i < RTE_DIM(a->lb.out); i++)
				fprintf(f, "%u ", a->lb.out[i]);
		}

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) {
			fprintf(f, "mtr ");
			for (i = 0; i < RTE_TABLE_ACTION_TC_MAX; i++)
				if (a->mtr.tc_mask & (1 << i)) {
					struct rte_table_action_mtr_tc_params *p =
						&a->mtr.mtr[i];
					enum rte_table_action_policer ga =
						p->policer[RTE_COLOR_GREEN];
					enum rte_table_action_policer ya =
						p->policer[RTE_COLOR_YELLOW];
					enum rte_table_action_policer ra =
						p->policer[RTE_COLOR_RED];

					fprintf(f, "tc%u meter %u policer g %s y %s r %s ",
						i,
						a->mtr.mtr[i].meter_profile_id,
						policer_action_string(ga),
						policer_action_string(ya),
						policer_action_string(ra));
				}
		}

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_TM))
			fprintf(f, "tm subport %u pipe %u ",
				a->tm.subport_id,
				a->tm.pipe_id);

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_ENCAP)) {
			fprintf(f, "encap ");
			switch (a->encap.type) {
			case RTE_TABLE_ACTION_ENCAP_ETHER:
				fprintf(f, "ether ");
				ether_addr_show(f, &a->encap.ether.ether.da);
				fprintf(f, " ");
				ether_addr_show(f, &a->encap.ether.ether.sa);
				fprintf(f, " ");
				break;

			case RTE_TABLE_ACTION_ENCAP_VLAN:
				fprintf(f, "vlan ");
				ether_addr_show(f, &a->encap.vlan.ether.da);
				fprintf(f, " ");
				ether_addr_show(f, &a->encap.vlan.ether.sa);
				fprintf(f, " pcp %u dei %u vid %u ",
					a->encap.vlan.vlan.pcp,
					a->encap.vlan.vlan.dei,
					a->encap.vlan.vlan.vid);
				break;

			case RTE_TABLE_ACTION_ENCAP_QINQ:
				fprintf(f, "qinq ");
				ether_addr_show(f, &a->encap.qinq.ether.da);
				fprintf(f, " ");
				ether_addr_show(f, &a->encap.qinq.ether.sa);
				fprintf(f, " pcp %u dei %u vid %u pcp %u dei %u vid %u ",
					a->encap.qinq.svlan.pcp,
					a->encap.qinq.svlan.dei,
					a->encap.qinq.svlan.vid,
					a->encap.qinq.cvlan.pcp,
					a->encap.qinq.cvlan.dei,
					a->encap.qinq.cvlan.vid);
				break;

			case RTE_TABLE_ACTION_ENCAP_MPLS:
				fprintf(f, "mpls %s ", (a->encap.mpls.unicast) ?
					"unicast " : "multicast ");
				ether_addr_show(f, &a->encap.mpls.ether.da);
				fprintf(f, " ");
				ether_addr_show(f, &a->encap.mpls.ether.sa);
				fprintf(f, " ");
				for (i = 0; i < a->encap.mpls.mpls_count; i++) {
					struct rte_table_action_mpls_hdr *l =
						&a->encap.mpls.mpls[i];

					fprintf(f, "label%u %u %u %u ",
						i,
						l->label,
						l->tc,
						l->ttl);
				}
				break;

			case RTE_TABLE_ACTION_ENCAP_PPPOE:
				fprintf(f, "pppoe ");
				ether_addr_show(f, &a->encap.pppoe.ether.da);
				fprintf(f, " ");
				ether_addr_show(f, &a->encap.pppoe.ether.sa);
				fprintf(f, " %u ", a->encap.pppoe.pppoe.session_id);
				break;

			case RTE_TABLE_ACTION_ENCAP_VXLAN:
				fprintf(f, "vxlan ether ");
				ether_addr_show(f, &a->encap.vxlan.ether.da);
				fprintf(f, " ");
				ether_addr_show(f, &a->encap.vxlan.ether.sa);
				if (table->ap->params.encap.vxlan.vlan)
					fprintf(f, " vlan pcp %u dei %u vid %u ",
						a->encap.vxlan.vlan.pcp,
						a->encap.vxlan.vlan.dei,
						a->encap.vxlan.vlan.vid);
				if (table->ap->params.encap.vxlan.ip_version) {
					fprintf(f, " ipv4 ");
					ipv4_addr_show(f, a->encap.vxlan.ipv4.sa);
					fprintf(f, " ");
					ipv4_addr_show(f, a->encap.vxlan.ipv4.da);
					fprintf(f, " %u %u ",
						(uint32_t)a->encap.vxlan.ipv4.dscp,
						(uint32_t)a->encap.vxlan.ipv4.ttl);
				} else {
					fprintf(f, " ipv6 ");
					ipv6_addr_show(f, a->encap.vxlan.ipv6.sa);
					fprintf(f, " ");
					ipv6_addr_show(f, a->encap.vxlan.ipv6.da);
					fprintf(f, " %u %u %u ",
						a->encap.vxlan.ipv6.flow_label,
						(uint32_t)a->encap.vxlan.ipv6.dscp,
						(uint32_t)a->encap.vxlan.ipv6.hop_limit);
					fprintf(f, " udp %u %u vxlan %u ",
						a->encap.vxlan.udp.sp,
						a->encap.vxlan.udp.dp,
						a->encap.vxlan.vxlan.vni);
				}
				break;

			default:
				fprintf(f, "unknown ");
			}
		}

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_NAT)) {
			fprintf(f, "nat %s ", (a->nat.ip_version) ? "ipv4 " : "ipv6 ");
			if (a->nat.ip_version)
				ipv4_addr_show(f, a->nat.addr.ipv4);
			else
				ipv6_addr_show(f, a->nat.addr.ipv6);
			fprintf(f, " %u ", (uint32_t)(a->nat.port));
		}

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_TTL))
			fprintf(f, "ttl %s ", (a->ttl.decrement) ? "dec" : "keep");

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_STATS))
			fprintf(f, "stats ");

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_TIME))
			fprintf(f, "time ");

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_SYM_CRYPTO))
			fprintf(f, "sym_crypto ");

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_TAG))
			fprintf(f, "tag %u ", a->tag.tag);

		if (a->action_mask & (1LLU << RTE_TABLE_ACTION_DECAP))
			fprintf(f, "decap %u ", a->decap.n);

		/* end */
		fprintf(f, "\n");
	}

	/* Write table default rule to file. */
	if (table->rule_default) {
		struct table_rule_action *a = &table->rule_default->action;

		fprintf(f, "# match default action fwd ");

		switch (a->fwd.action) {
		case RTE_PIPELINE_ACTION_DROP:
			fprintf(f, "drop ");
			break;

		case RTE_PIPELINE_ACTION_PORT:
			fprintf(f, "port %u ", a->fwd.id);
			break;

		case RTE_PIPELINE_ACTION_PORT_META:
			fprintf(f, "meta ");
			break;

		case RTE_PIPELINE_ACTION_TABLE:
		default:
			fprintf(f, "table %u ", a->fwd.id);
		}
	} else
		fprintf(f, "# match default action fwd drop ");

	fprintf(f, "\n");

	/* Close file. */
	fclose(f);

	return 0;
}

static const char cmd_pipeline_table_rule_show_help[] =
"pipeline <pipeline_name> table <table_id> rule show\n"
"     file <file_name>\n";

static void
cmd_pipeline_table_rule_show(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *file_name = NULL, *pipeline_name;
	uint32_t table_id;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "show") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "show");
		return;
	}

	if (strcmp(tokens[6], "file") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "file");
		return;
	}

	file_name = tokens[7];

	status = table_rule_show(pipeline_name, table_id, file_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_pipeline_table_rule_stats_read_help[] =
"pipeline <pipeline_name> table <table_id> rule read stats [clear]\n"
"     match <match>\n";

static void
cmd_pipeline_table_rule_stats_read(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_match m;
	struct rte_table_action_stats_counters stats;
	char *pipeline_name;
	uint32_t table_id, n_tokens_parsed;
	int clear = 0, status;

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	if (strcmp(tokens[6], "stats") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	n_tokens -= 7;
	tokens += 7;

	/* clear */
	if (n_tokens && (strcmp(tokens[0], "clear") == 0)) {
		clear = 1;

		n_tokens--;
		tokens++;
	}

	/* match */
	if ((n_tokens == 0) || strcmp(tokens[0], "match")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return;
	}

	n_tokens_parsed = parse_match(tokens,
		n_tokens,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	n_tokens -= n_tokens_parsed;
	tokens += n_tokens_parsed;

	/* end */
	if (n_tokens) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	/* Read table rule stats. */
	status = pipeline_table_rule_stats_read(pipeline_name,
		table_id,
		&m,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	/* Print stats. */
	if (stats.n_packets_valid && stats.n_bytes_valid)
		snprintf(out, out_size, "Packets: %" PRIu64 "; Bytes: %" PRIu64 "\n",
			stats.n_packets,
			stats.n_bytes);

	if (stats.n_packets_valid && !stats.n_bytes_valid)
		snprintf(out, out_size, "Packets: %" PRIu64 "; Bytes: N/A\n",
			stats.n_packets);

	if (!stats.n_packets_valid && stats.n_bytes_valid)
		snprintf(out, out_size, "Packets: N/A; Bytes: %" PRIu64 "\n",
			stats.n_bytes);

	if (!stats.n_packets_valid && !stats.n_bytes_valid)
		snprintf(out, out_size, "Packets: N/A ; Bytes: N/A\n");
}

static const char cmd_pipeline_table_meter_profile_add_help[] =
"pipeline <pipeline_name> table <table_id> meter profile <meter_profile_id>\n"
"   add srtcm cir <cir> cbs <cbs> ebs <ebs>\n"
"   | trtcm cir <cir> pir <pir> cbs <cbs> pbs <pbs>\n";

static void
cmd_pipeline_table_meter_profile_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_table_action_meter_profile p;
	char *pipeline_name;
	uint32_t table_id, meter_profile_id;
	int status;

	if (n_tokens < 9) {
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

	if (strcmp(tokens[4], "meter") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[5], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (parser_read_uint32(&meter_profile_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "meter_profile_id");
		return;
	}

	if (strcmp(tokens[7], "add") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[8], "srtcm") == 0) {
		if (n_tokens != 15) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				tokens[0]);
			return;
		}

		p.alg = RTE_TABLE_ACTION_METER_SRTCM;

		if (strcmp(tokens[9], "cir") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cir");
			return;
		}

		if (parser_read_uint64(&p.srtcm.cir, tokens[10]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cir");
			return;
		}

		if (strcmp(tokens[11], "cbs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cbs");
			return;
		}

		if (parser_read_uint64(&p.srtcm.cbs, tokens[12]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cbs");
			return;
		}

		if (strcmp(tokens[13], "ebs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "ebs");
			return;
		}

		if (parser_read_uint64(&p.srtcm.ebs, tokens[14]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "ebs");
			return;
		}
	} else if (strcmp(tokens[8], "trtcm") == 0) {
		if (n_tokens != 17) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		p.alg = RTE_TABLE_ACTION_METER_TRTCM;

		if (strcmp(tokens[9], "cir") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cir");
			return;
		}

		if (parser_read_uint64(&p.trtcm.cir, tokens[10]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cir");
			return;
		}

		if (strcmp(tokens[11], "pir") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pir");
			return;
		}

		if (parser_read_uint64(&p.trtcm.pir, tokens[12]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "pir");
			return;
		}
		if (strcmp(tokens[13], "cbs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cbs");
			return;
		}

		if (parser_read_uint64(&p.trtcm.cbs, tokens[14]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "cbs");
			return;
		}

		if (strcmp(tokens[15], "pbs") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pbs");
			return;
		}

		if (parser_read_uint64(&p.trtcm.pbs, tokens[16]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "pbs");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = pipeline_table_mtr_profile_add(pipeline_name,
		table_id,
		meter_profile_id,
		&p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}


static const char cmd_pipeline_table_meter_profile_delete_help[] =
"pipeline <pipeline_name> table <table_id>\n"
"   meter profile <meter_profile_id> delete\n";

static void
cmd_pipeline_table_meter_profile_delete(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *pipeline_name;
	uint32_t table_id, meter_profile_id;
	int status;

	if (n_tokens != 8) {
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

	if (strcmp(tokens[4], "meter") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[5], "profile") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	if (parser_read_uint32(&meter_profile_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "meter_profile_id");
		return;
	}

	if (strcmp(tokens[7], "delete") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	status = pipeline_table_mtr_profile_delete(pipeline_name,
		table_id,
		meter_profile_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}


static const char cmd_pipeline_table_rule_meter_read_help[] =
"pipeline <pipeline_name> table <table_id> rule read meter [clear]\n"
"     match <match>\n";

static void
cmd_pipeline_table_rule_meter_read(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_match m;
	struct rte_table_action_mtr_counters stats;
	char *pipeline_name;
	uint32_t table_id, n_tokens_parsed;
	int clear = 0, status;

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	if (strcmp(tokens[6], "meter") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	n_tokens -= 7;
	tokens += 7;

	/* clear */
	if (n_tokens && (strcmp(tokens[0], "clear") == 0)) {
		clear = 1;

		n_tokens--;
		tokens++;
	}

	/* match */
	if ((n_tokens == 0) || strcmp(tokens[0], "match")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return;
	}

	n_tokens_parsed = parse_match(tokens,
		n_tokens,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	n_tokens -= n_tokens_parsed;
	tokens += n_tokens_parsed;

	/* end */
	if (n_tokens) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	/* Read table rule meter stats. */
	status = pipeline_table_rule_mtr_read(pipeline_name,
		table_id,
		&m,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	/* Print stats. */
}


static const char cmd_pipeline_table_dscp_help[] =
"pipeline <pipeline_name> table <table_id> dscp <file_name>\n"
"\n"
" File <file_name>:\n"
"   - exactly 64 lines\n"
"   - line format: <tc_id> <tc_queue_id> <color>, with <color> as: g | y | r\n";

static int
load_dscp_table(struct rte_table_action_dscp_table *dscp_table,
	const char *file_name,
	uint32_t *line_number)
{
	FILE *f = NULL;
	uint32_t dscp, l;

	/* Check input arguments */
	if ((dscp_table == NULL) ||
		(file_name == NULL) ||
		(line_number == NULL)) {
		if (line_number)
			*line_number = 0;
		return -EINVAL;
	}

	/* Open input file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		*line_number = 0;
		return -EINVAL;
	}

	/* Read file */
	for (dscp = 0, l = 1; ; l++) {
		char line[64];
		char *tokens[3];
		enum rte_color color;
		uint32_t tc_id, tc_queue_id, n_tokens = RTE_DIM(tokens);

		if (fgets(line, sizeof(line), f) == NULL)
			break;

		if (is_comment(line))
			continue;

		if (parse_tokenize_string(line, tokens, &n_tokens)) {
			*line_number = l;
			fclose(f);
			return -EINVAL;
		}

		if (n_tokens == 0)
			continue;

		if ((dscp >= RTE_DIM(dscp_table->entry)) ||
			(n_tokens != RTE_DIM(tokens)) ||
			parser_read_uint32(&tc_id, tokens[0]) ||
			(tc_id >= RTE_TABLE_ACTION_TC_MAX) ||
			parser_read_uint32(&tc_queue_id, tokens[1]) ||
			(tc_queue_id >= RTE_TABLE_ACTION_TC_QUEUE_MAX) ||
			(strlen(tokens[2]) != 1)) {
			*line_number = l;
			fclose(f);
			return -EINVAL;
		}

		switch (tokens[2][0]) {
		case 'g':
		case 'G':
			color = RTE_COLOR_GREEN;
			break;

		case 'y':
		case 'Y':
			color = RTE_COLOR_YELLOW;
			break;

		case 'r':
		case 'R':
			color = RTE_COLOR_RED;
			break;

		default:
			*line_number = l;
			fclose(f);
			return -EINVAL;
		}

		dscp_table->entry[dscp].tc_id = tc_id;
		dscp_table->entry[dscp].tc_queue_id = tc_queue_id;
		dscp_table->entry[dscp].color = color;
		dscp++;
	}

	/* Close file */
	fclose(f);
	return 0;
}

static void
cmd_pipeline_table_dscp(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct rte_table_action_dscp_table dscp_table;
	char *pipeline_name, *file_name;
	uint32_t table_id, line_number;
	int status;

	if (n_tokens != 6) {
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

	if (strcmp(tokens[4], "dscp") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "dscp");
		return;
	}

	file_name = tokens[5];

	status = load_dscp_table(&dscp_table, file_name, &line_number);
	if (status) {
		snprintf(out, out_size, MSG_FILE_ERR, file_name, line_number);
		return;
	}

	status = pipeline_table_dscp_table_update(pipeline_name,
		table_id,
		UINT64_MAX,
		&dscp_table);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}


static const char cmd_pipeline_table_rule_ttl_read_help[] =
"pipeline <pipeline_name> table <table_id> rule read ttl [clear]\n"
"     match <match>\n";

static void
cmd_pipeline_table_rule_ttl_read(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_match m;
	struct rte_table_action_ttl_counters stats;
	char *pipeline_name;
	uint32_t table_id, n_tokens_parsed;
	int clear = 0, status;

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	if (strcmp(tokens[6], "ttl") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "ttl");
		return;
	}

	n_tokens -= 7;
	tokens += 7;

	/* clear */
	if (n_tokens && (strcmp(tokens[0], "clear") == 0)) {
		clear = 1;

		n_tokens--;
		tokens++;
	}

	/* match */
	if ((n_tokens == 0) || strcmp(tokens[0], "match")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return;
	}

	n_tokens_parsed = parse_match(tokens,
		n_tokens,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	n_tokens -= n_tokens_parsed;
	tokens += n_tokens_parsed;

	/* end */
	if (n_tokens) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	/* Read table rule TTL stats. */
	status = pipeline_table_rule_ttl_read(pipeline_name,
		table_id,
		&m,
		&stats,
		clear);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	/* Print stats. */
	snprintf(out, out_size, "Packets: %" PRIu64 "\n",
		stats.n_packets);
}

static const char cmd_pipeline_table_rule_time_read_help[] =
"pipeline <pipeline_name> table <table_id> rule read time\n"
"     match <match>\n";

static void
cmd_pipeline_table_rule_time_read(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct table_rule_match m;
	char *pipeline_name;
	uint64_t timestamp;
	uint32_t table_id, n_tokens_parsed;
	int status;

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (parser_read_uint32(&table_id, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	if (strcmp(tokens[4], "rule") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rule");
		return;
	}

	if (strcmp(tokens[5], "read") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "read");
		return;
	}

	if (strcmp(tokens[6], "time") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "time");
		return;
	}

	n_tokens -= 7;
	tokens += 7;

	/* match */
	if ((n_tokens == 0) || strcmp(tokens[0], "match")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
		return;
	}

	n_tokens_parsed = parse_match(tokens,
		n_tokens,
		out,
		out_size,
		&m);
	if (n_tokens_parsed == 0)
		return;
	n_tokens -= n_tokens_parsed;
	tokens += n_tokens_parsed;

	/* end */
	if (n_tokens) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	/* Read table rule timestamp. */
	status = pipeline_table_rule_time_read(pipeline_name,
		table_id,
		&m,
		&timestamp);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}

	/* Print stats. */
	snprintf(out, out_size, "Packets: %" PRIu64 "\n", timestamp);
}

static const char cmd_thread_pipeline_enable_help[] =
"thread <thread_id> pipeline <pipeline_name> enable\n";

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


static const char cmd_thread_pipeline_disable_help[] =
"thread <thread_id> pipeline <pipeline_name> disable\n";

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

static void
cmd_help(char **tokens, uint32_t n_tokens, char *out, size_t out_size)
{
	tokens++;
	n_tokens--;

	if (n_tokens == 0) {
		snprintf(out, out_size,
			"Type 'help <command>' for details on each command.\n\n"
			"List of commands:\n"
			"\tmempool\n"
			"\tlink\n"
			"\tswq\n"
			"\ttmgr subport profile\n"
			"\ttmgr pipe profile\n"
			"\ttmgr\n"
			"\ttmgr subport\n"
			"\ttmgr subport pipe\n"
			"\ttap\n"
			"\tkni\n"
			"\tport in action profile\n"
			"\ttable action profile\n"
			"\tpipeline\n"
			"\tpipeline port in\n"
			"\tpipeline port out\n"
			"\tpipeline table\n"
			"\tpipeline port in table\n"
			"\tpipeline port in stats\n"
			"\tpipeline port in enable\n"
			"\tpipeline port in disable\n"
			"\tpipeline port out stats\n"
			"\tpipeline table stats\n"
			"\tpipeline table rule add\n"
			"\tpipeline table rule add default\n"
			"\tpipeline table rule add bulk\n"
			"\tpipeline table rule delete\n"
			"\tpipeline table rule delete default\n"
			"\tpipeline table rule show\n"
			"\tpipeline table rule stats read\n"
			"\tpipeline table meter profile add\n"
			"\tpipeline table meter profile delete\n"
			"\tpipeline table rule meter read\n"
			"\tpipeline table dscp\n"
			"\tpipeline table rule ttl read\n"
			"\tpipeline table rule time read\n"
			"\tthread pipeline enable\n"
			"\tthread pipeline disable\n\n");
		return;
	}

	if (strcmp(tokens[0], "mempool") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_mempool_help);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_link_help);
		return;
	}

	if (strcmp(tokens[0], "swq") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_swq_help);
		return;
	}

	if (strcmp(tokens[0], "tmgr") == 0) {
		if (n_tokens == 1) {
			snprintf(out, out_size, "\n%s\n", cmd_tmgr_help);
			return;
		}

		if ((n_tokens == 2) &&
			(strcmp(tokens[1], "subport")) == 0) {
			snprintf(out, out_size, "\n%s\n", cmd_tmgr_subport_help);
			return;
		}

		if ((n_tokens == 3) &&
			(strcmp(tokens[1], "subport") == 0) &&
			(strcmp(tokens[2], "profile") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_tmgr_subport_profile_help);
			return;
		}

		if ((n_tokens == 3) &&
			(strcmp(tokens[1], "subport") == 0) &&
			(strcmp(tokens[2], "pipe") == 0)) {
			snprintf(out, out_size, "\n%s\n", cmd_tmgr_subport_pipe_help);
			return;
		}

		if ((n_tokens == 3) &&
			(strcmp(tokens[1], "pipe") == 0) &&
			(strcmp(tokens[2], "profile") == 0)) {
			snprintf(out, out_size, "\n%s\n", cmd_tmgr_pipe_profile_help);
			return;
		}
	}

	if (strcmp(tokens[0], "tap") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_tap_help);
		return;
	}

	if (strcmp(tokens[0], "kni") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_kni_help);
		return;
	}

	if (strcmp(tokens[0], "cryptodev") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_cryptodev_help);
		return;
	}

	if ((n_tokens == 4) &&
		(strcmp(tokens[0], "port") == 0) &&
		(strcmp(tokens[1], "in") == 0) &&
		(strcmp(tokens[2], "action") == 0) &&
		(strcmp(tokens[3], "profile") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_port_in_action_profile_help);
		return;
	}

	if ((n_tokens == 3) &&
		(strcmp(tokens[0], "table") == 0) &&
		(strcmp(tokens[1], "action") == 0) &&
		(strcmp(tokens[2], "profile") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_table_action_profile_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) && (n_tokens == 1)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(strcmp(tokens[1], "port") == 0)) {
		if ((n_tokens == 3) && (strcmp(tokens[2], "in")) == 0) {
			snprintf(out, out_size, "\n%s\n", cmd_pipeline_port_in_help);
			return;
		}

		if ((n_tokens == 3) && (strcmp(tokens[2], "out")) == 0) {
			snprintf(out, out_size, "\n%s\n", cmd_pipeline_port_out_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "in") == 0) &&
			(strcmp(tokens[3], "table") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_in_table_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "in") == 0) &&
			(strcmp(tokens[3], "stats") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_in_stats_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "in") == 0) &&
			(strcmp(tokens[3], "enable") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_in_enable_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "in") == 0) &&
			(strcmp(tokens[3], "disable") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_in_disable_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "out") == 0) &&
			(strcmp(tokens[3], "stats") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_out_stats_help);
			return;
		}
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(strcmp(tokens[1], "table") == 0)) {
		if (n_tokens == 2) {
			snprintf(out, out_size, "\n%s\n", cmd_pipeline_table_help);
			return;
		}

		if ((n_tokens == 3) && strcmp(tokens[2], "stats") == 0) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_stats_help);
			return;
		}

		if ((n_tokens == 3) && strcmp(tokens[2], "dscp") == 0) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_dscp_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "add") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_add_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "add") == 0) &&
			(strcmp(tokens[4], "default") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_add_default_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "add") == 0) &&
			(strcmp(tokens[4], "bulk") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_add_bulk_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "delete") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_delete_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "delete") == 0) &&
			(strcmp(tokens[4], "default") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_delete_default_help);
			return;
		}

		if ((n_tokens == 4) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "show") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_show_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "stats") == 0) &&
			(strcmp(tokens[4], "read") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_stats_read_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[3], "profile") == 0) &&
			(strcmp(tokens[4], "add") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_meter_profile_add_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[3], "profile") == 0) &&
			(strcmp(tokens[4], "delete") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_meter_profile_delete_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "meter") == 0) &&
			(strcmp(tokens[4], "read") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_meter_read_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "ttl") == 0) &&
			(strcmp(tokens[4], "read") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_ttl_read_help);
			return;
		}

		if ((n_tokens == 5) &&
			(strcmp(tokens[2], "rule") == 0) &&
			(strcmp(tokens[3], "time") == 0) &&
			(strcmp(tokens[4], "read") == 0)) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_table_rule_time_read_help);
			return;
		}
	}

	if ((n_tokens == 3) &&
		(strcmp(tokens[0], "thread") == 0) &&
		(strcmp(tokens[1], "pipeline") == 0)) {
		if (strcmp(tokens[2], "enable") == 0) {
			snprintf(out, out_size, "\n%s\n",
				cmd_thread_pipeline_enable_help);
			return;
		}

		if (strcmp(tokens[2], "disable") == 0) {
			snprintf(out, out_size, "\n%s\n",
				cmd_thread_pipeline_disable_help);
			return;
		}
	}

	snprintf(out, out_size, "Invalid command\n");
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

	if (strcmp(tokens[0], "help") == 0) {
		cmd_help(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "mempool") == 0) {
		cmd_mempool(tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		if (strcmp(tokens[1], "show") == 0) {
			cmd_link_show(tokens, n_tokens, out, out_size);
			return;
		}

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

	if (strcmp(tokens[0], "cryptodev") == 0) {
		cmd_cryptodev(tokens, n_tokens, out, out_size);
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

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "add") == 0) &&
			(strcmp(tokens[6], "match") == 0)) {
			if ((n_tokens >= 8) &&
				(strcmp(tokens[7], "default") == 0)) {
				cmd_pipeline_table_rule_add_default(tokens,
					n_tokens, out, out_size);
				return;
			}

			cmd_pipeline_table_rule_add(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "add") == 0) &&
			(strcmp(tokens[6], "bulk") == 0)) {
			cmd_pipeline_table_rule_add_bulk(tokens,
				n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "delete") == 0) &&
			(strcmp(tokens[6], "match") == 0)) {
			if ((n_tokens >= 8) &&
				(strcmp(tokens[7], "default") == 0)) {
				cmd_pipeline_table_rule_delete_default(tokens,
					n_tokens, out, out_size);
				return;
				}

			cmd_pipeline_table_rule_delete(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "show") == 0)) {
			cmd_pipeline_table_rule_show(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "stats") == 0)) {
			cmd_pipeline_table_rule_stats_read(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 8) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "meter") == 0) &&
			(strcmp(tokens[5], "profile") == 0) &&
			(strcmp(tokens[7], "add") == 0)) {
			cmd_pipeline_table_meter_profile_add(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 8) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "meter") == 0) &&
			(strcmp(tokens[5], "profile") == 0) &&
			(strcmp(tokens[7], "delete") == 0)) {
			cmd_pipeline_table_meter_profile_delete(tokens,
				n_tokens, out, out_size);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "meter") == 0)) {
			cmd_pipeline_table_rule_meter_read(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "dscp") == 0)) {
			cmd_pipeline_table_dscp(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "ttl") == 0)) {
			cmd_pipeline_table_rule_ttl_read(tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "rule") == 0) &&
			(strcmp(tokens[5], "read") == 0) &&
			(strcmp(tokens[6], "time") == 0)) {
			cmd_pipeline_table_rule_time_read(tokens, n_tokens,
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

static int
cli_rule_file_process(const char *file_name,
	size_t line_len_max,
	struct table_rule_list **rule_list,
	uint32_t *n_rules,
	uint32_t *line_number,
	char *out,
	size_t out_size)
{
	struct table_rule_list *list = NULL;
	char *line = NULL;
	FILE *f = NULL;
	uint32_t rule_id = 0, line_id = 0;
	int status = 0;

	/* Check input arguments */
	if ((file_name == NULL) ||
		(strlen(file_name) == 0) ||
		(line_len_max == 0) ||
		(rule_list == NULL) ||
		(n_rules == NULL) ||
		(line_number == NULL) ||
		(out == NULL)) {
		status = -EINVAL;
		goto cli_rule_file_process_free;
	}

	/* Memory allocation */
	list = malloc(sizeof(struct table_rule_list));
	if (list == NULL) {
		status = -ENOMEM;
		goto cli_rule_file_process_free;
	}

	TAILQ_INIT(list);

	line = malloc(line_len_max + 1);
	if (line == NULL) {
		status = -ENOMEM;
		goto cli_rule_file_process_free;
	}

	/* Open file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		status = -EIO;
		goto cli_rule_file_process_free;
	}

	/* Read file */
	for (line_id = 1, rule_id = 0; ; line_id++) {
		char *tokens[CMD_MAX_TOKENS];
		struct table_rule *rule = NULL;
		uint32_t n_tokens, n_tokens_parsed, t0;

		/* Read next line from file. */
		if (fgets(line, line_len_max + 1, f) == NULL)
			break;

		/* Comment. */
		if (is_comment(line))
			continue;

		/* Parse line. */
		n_tokens = RTE_DIM(tokens);
		status = parse_tokenize_string(line, tokens, &n_tokens);
		if (status) {
			status = -EINVAL;
			goto cli_rule_file_process_free;
		}

		/* Empty line. */
		if (n_tokens == 0)
			continue;
		t0 = 0;

		/* Rule alloc and insert. */
		rule = calloc(1, sizeof(struct table_rule));
		if (rule == NULL) {
			status = -ENOMEM;
			goto cli_rule_file_process_free;
		}

		TAILQ_INSERT_TAIL(list, rule, node);

		/* Rule match. */
		n_tokens_parsed = parse_match(tokens + t0,
			n_tokens - t0,
			out,
			out_size,
			&rule->match);
		if (n_tokens_parsed == 0) {
			status = -EINVAL;
			goto cli_rule_file_process_free;
		}
		t0 += n_tokens_parsed;

		/* Rule action. */
		n_tokens_parsed = parse_table_action(tokens + t0,
			n_tokens - t0,
			out,
			out_size,
			&rule->action);
		if (n_tokens_parsed == 0) {
			status = -EINVAL;
			goto cli_rule_file_process_free;
		}
		t0 += n_tokens_parsed;

		/* Line completed. */
		if (t0 < n_tokens) {
			status = -EINVAL;
			goto cli_rule_file_process_free;
		}

		/* Increment rule count */
		rule_id++;
	}

	/* Close file */
	fclose(f);

	/* Memory free */
	free(line);

	*rule_list = list;
	*n_rules = rule_id;
	*line_number = line_id;
	return 0;

cli_rule_file_process_free:
	if (rule_list != NULL)
		*rule_list = NULL;

	if (n_rules != NULL)
		*n_rules = rule_id;

	if (line_number != NULL)
		*line_number = line_id;

	if (list != NULL)
		for ( ; ; ) {
			struct table_rule *rule;

			rule = TAILQ_FIRST(list);
			if (rule == NULL)
				break;

			TAILQ_REMOVE(list, rule, node);
			free(rule);
		}

	if (f)
		fclose(f);
	free(line);
	free(list);

	return status;
}
