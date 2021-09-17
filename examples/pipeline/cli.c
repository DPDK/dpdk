/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_swx_port_ethdev.h>
#include <rte_swx_port_ring.h>
#include <rte_swx_port_source_sink.h>
#include <rte_swx_port_fd.h>
#include <rte_swx_pipeline.h>
#include <rte_swx_ctl.h>

#include "cli.h"

#include "obj.h"
#include "thread.h"

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

#define skip_white_spaces(pos)			\
({						\
	__typeof__(pos) _p = (pos);		\
	for ( ; isspace(*_p); _p++)		\
		;				\
	_p;					\
})

static int
parser_read_uint64(uint64_t *value, const char *p)
{
	char *next;
	uint64_t val;

	p = skip_white_spaces(p);
	if (!isdigit(*p))
		return -EINVAL;

	val = strtoul(p, &next, 0);
	if (p == next)
		return -EINVAL;

	p = next;
	switch (*p) {
	case 'T':
		val *= 1024ULL;
		/* fall through */
	case 'G':
		val *= 1024ULL;
		/* fall through */
	case 'M':
		val *= 1024ULL;
		/* fall through */
	case 'k':
	case 'K':
		val *= 1024ULL;
		p++;
		break;
	}

	p = skip_white_spaces(p);
	if (*p != '\0')
		return -EINVAL;

	*value = val;
	return 0;
}

static int
parser_read_uint32(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

static int
parser_read_uint16(uint16_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;

	if (val > UINT16_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

#define PARSE_DELIMITER " \f\n\r\t\v"

static int
parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens)
{
	uint32_t i;

	if ((string == NULL) ||
		(tokens == NULL) ||
		(*n_tokens < 1))
		return -EINVAL;

	for (i = 0; i < *n_tokens; i++) {
		tokens[i] = strtok_r(string, PARSE_DELIMITER, &string);
		if (tokens[i] == NULL)
			break;
	}

	if ((i == *n_tokens) && strtok_r(string, PARSE_DELIMITER, &string))
		return -E2BIG;

	*n_tokens = i;
	return 0;
}

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
	size_t out_size,
	void *obj)
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

	mempool = mempool_create(obj, name, &p);
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
	size_t out_size,
	void *obj)
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

	link = link_create(obj, name, &p);
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
		"\tether " RTE_ETHER_ADDR_PRT_FMT " rxqueues %u txqueues %u\n"
		"\tport# %u  speed %s\n"
		"\tRX packets %" PRIu64"  bytes %" PRIu64"\n"
		"\tRX errors %" PRIu64"  missed %" PRIu64"  no-mbuf %" PRIu64"\n"
		"\tTX packets %" PRIu64"  bytes %" PRIu64"\n"
		"\tTX errors %" PRIu64"\n",
		link->name,
		eth_link.link_status == 0 ? "DOWN" : "UP",
		mtu,
		RTE_ETHER_ADDR_BYTES(&mac_addr),
		link->n_rxq,
		link->n_txq,
		link->port_id,
		rte_eth_link_speed_to_str(eth_link.link_speed),
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
cmd_link_show(char **tokens,
	      uint32_t n_tokens,
	      char *out,
	      size_t out_size,
	      void *obj)
{
	struct link *link;
	char *link_name;

	if (n_tokens != 2 && n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (n_tokens == 2) {
		link = link_next(obj, NULL);

		while (link != NULL) {
			out_size = out_size - strlen(out);
			out = &out[strlen(out)];

			print_link_info(link, out, out_size);
			link = link_next(obj, link);
		}
	} else {
		out_size = out_size - strlen(out);
		out = &out[strlen(out)];

		link_name = tokens[2];
		link = link_find(obj, link_name);

		if (link == NULL) {
			snprintf(out, out_size, MSG_ARG_INVALID,
					"Link does not exist");
			return;
		}
		print_link_info(link, out, out_size);
	}
}

static const char cmd_ring_help[] =
"ring <ring_name> size <size> numa <numa_node>\n";

static void
cmd_ring(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct ring_params p;
	char *name;
	struct ring *ring;

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

	if (strcmp(tokens[4], "numa") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "numa");
		return;
	}

	if (parser_read_uint32(&p.numa_node, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	ring = ring_create(obj, name, &p);
	if (!ring) {
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
	size_t out_size,
	void *obj)
{
	struct tap *tap;
	char *name;

	if (n_tokens < 2) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
	name = tokens[1];

	tap = tap_create(obj, name);
	if (tap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_pipeline_create_help[] =
"pipeline <pipeline_name> create <numa_node>\n";

static void
cmd_pipeline_create(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *name;
	uint32_t numa_node;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (parser_read_uint32(&numa_node, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	p = pipeline_create(obj, name, (int)numa_node);
	if (!p) {
		snprintf(out, out_size, "pipeline create error.");
		return;
	}
}

static const char cmd_pipeline_port_in_help[] =
"pipeline <pipeline_name> port in <port_id>\n"
"   link <link_name> rxq <queue_id> bsz <burst_size>\n"
"   ring <ring_name> bsz <burst_size>\n"
"   | source <mempool_name> <file_name> loop <n_loops>\n"
"   | tap <tap_name> mempool <mempool_name> mtu <mtu> bsz <burst_size>\n";

static void
cmd_pipeline_port_in(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	int status;
	uint32_t port_id = 0, t0;

	if (n_tokens < 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

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

	t0 = 5;

	if (strcmp(tokens[t0], "link") == 0) {
		struct rte_swx_port_ethdev_reader_params params;
		struct link *link;

		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in link");
			return;
		}

		link = link_find(obj, tokens[t0 + 1]);
		if (!link) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"link_name");
			return;
		}
		params.dev_name = link->dev_name;

		if (strcmp(tokens[t0 + 2], "rxq") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rxq");
			return;
		}

		if (parser_read_uint16(&params.queue_id, tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"queue_id");
			return;
		}

		if (strcmp(tokens[t0 + 4], "bsz") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
			return;
		}

		if (parser_read_uint32(&params.burst_size, tokens[t0 + 5])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"burst_size");
			return;
		}

		t0 += 6;

		status = rte_swx_pipeline_port_in_config(p->p,
			port_id,
			"ethdev",
			&params);
	} else if (strcmp(tokens[t0], "ring") == 0) {
		struct rte_swx_port_ring_reader_params params;
		struct ring *ring;

		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in ring");
			return;
		}

		ring = ring_find(obj, tokens[t0 + 1]);
		if (!ring) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"ring_name");
			return;
		}
		params.name = ring->name;

		if (strcmp(tokens[t0 + 2], "bsz") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
			return;
		}

		if (parser_read_uint32(&params.burst_size, tokens[t0 + 3])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"burst_size");
			return;
		}

		t0 += 4;

		status = rte_swx_pipeline_port_in_config(p->p,
			port_id,
			"ring",
			&params);
	} else if (strcmp(tokens[t0], "source") == 0) {
		struct rte_swx_port_source_params params;
		struct mempool *mp;

		if (n_tokens < t0 + 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in source");
			return;
		}

		mp = mempool_find(obj, tokens[t0 + 1]);
		if (!mp) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"mempool_name");
			return;
		}
		params.pool = mp->m;

		params.file_name = tokens[t0 + 2];

		if (strcmp(tokens[t0 + 3], "loop") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "loop");
			return;
		}

		if (parser_read_uint64(&params.n_loops, tokens[t0 + 4])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_loops");
			return;
		}

		t0 += 5;

		status = rte_swx_pipeline_port_in_config(p->p,
			port_id,
			"source",
			&params);
	} else if (strcmp(tokens[t0], "tap") == 0) {
		struct rte_swx_port_fd_reader_params params;
		struct tap *tap;
		struct mempool *mp;

		if (n_tokens < t0 + 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port in tap");
			return;
		}

		tap = tap_find(obj, tokens[t0 + 1]);
		if (!tap) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"tap_name");
			return;
		}
		params.fd = tap->fd;

		if (strcmp(tokens[t0 + 2], "mempool") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mempool");
			return;
		}

		mp = mempool_find(obj, tokens[t0 + 3]);
		if (!mp) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"mempool_name");
			return;
		}
		params.mempool = mp->m;

		if (strcmp(tokens[t0 + 4], "mtu") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"mtu");
			return;
		}

		if (parser_read_uint32(&params.mtu, tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "mtu");
			return;
		}

		if (strcmp(tokens[t0 + 6], "bsz") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
			return;
		}

		if (parser_read_uint32(&params.burst_size, tokens[t0 + 7])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"burst_size");
			return;
		}

		t0 += 8;

		status = rte_swx_pipeline_port_in_config(p->p,
			port_id,
			"fd",
			&params);

	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	if (status) {
		snprintf(out, out_size, "port in error.");
		return;
	}

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
}

static const char cmd_pipeline_port_out_help[] =
"pipeline <pipeline_name> port out <port_id>\n"
"   link <link_name> txq <txq_id> bsz <burst_size>\n"
"   ring <ring_name> bsz <burst_size>\n"
"   | sink <file_name> | none\n"
"   | tap <tap_name> bsz <burst_size>\n";

static void
cmd_pipeline_port_out(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	int status;
	uint32_t port_id = 0, t0;

	if (n_tokens < 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

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

	t0 = 5;

	if (strcmp(tokens[t0], "link") == 0) {
		struct rte_swx_port_ethdev_writer_params params;
		struct link *link;

		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out link");
			return;
		}

		link = link_find(obj, tokens[t0 + 1]);
		if (!link) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"link_name");
			return;
		}
		params.dev_name = link->dev_name;

		if (strcmp(tokens[t0 + 2], "txq") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "txq");
			return;
		}

		if (parser_read_uint16(&params.queue_id, tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"queue_id");
			return;
		}

		if (strcmp(tokens[t0 + 4], "bsz") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
			return;
		}

		if (parser_read_uint32(&params.burst_size, tokens[t0 + 5])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"burst_size");
			return;
		}

		t0 += 6;

		status = rte_swx_pipeline_port_out_config(p->p,
			port_id,
			"ethdev",
			&params);
	} else if (strcmp(tokens[t0], "ring") == 0) {
		struct rte_swx_port_ring_writer_params params;
		struct ring *ring;

		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out link");
			return;
		}

		ring = ring_find(obj, tokens[t0 + 1]);
		if (!ring) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"ring_name");
			return;
		}
		params.name = ring->name;

		if (strcmp(tokens[t0 + 2], "bsz") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
			return;
		}

		if (parser_read_uint32(&params.burst_size, tokens[t0 + 3])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"burst_size");
			return;
		}

		t0 += 4;

		status = rte_swx_pipeline_port_out_config(p->p,
			port_id,
			"ring",
			&params);
	} else if (strcmp(tokens[t0], "sink") == 0) {
		struct rte_swx_port_sink_params params;

		params.file_name = strcmp(tokens[t0 + 1], "none") ?
			tokens[t0 + 1] : NULL;

		t0 += 2;

		status = rte_swx_pipeline_port_out_config(p->p,
			port_id,
			"sink",
			&params);
	} else if (strcmp(tokens[t0], "tap") == 0) {
		struct rte_swx_port_fd_writer_params params;
		struct tap *tap;

		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"pipeline port out tap");
			return;
		}

		tap = tap_find(obj, tokens[t0 + 1]);
		if (!tap) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"tap_name");
			return;
		}
		params.fd = tap->fd;

		if (strcmp(tokens[t0 + 2], "bsz") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
			return;
		}

		if (parser_read_uint32(&params.burst_size, tokens[t0 + 3])) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"burst_size");
			return;
		}

		t0 += 4;

		status = rte_swx_pipeline_port_out_config(p->p,
			port_id,
			"fd",
			&params);
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	if (status) {
		snprintf(out, out_size, "port out error.");
		return;
	}

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
}

static const char cmd_pipeline_build_help[] =
"pipeline <pipeline_name> build <spec_file>\n";

static void
cmd_pipeline_build(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p = NULL;
	FILE *spec = NULL;
	uint32_t err_line;
	const char *err_msg;
	int status;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	spec = fopen(tokens[3], "r");
	if (!spec) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[3]);
		return;
	}

	status = rte_swx_pipeline_build_from_spec(p->p,
		spec,
		&err_line,
		&err_msg);
	fclose(spec);
	if (status) {
		snprintf(out, out_size, "Error %d at line %u: %s\n.",
			status, err_line, err_msg);
		return;
	}

	p->ctl = rte_swx_ctl_pipeline_create(p->p);
	if (!p->ctl) {
		snprintf(out, out_size, "Pipeline control create failed.");
		rte_swx_pipeline_free(p->p);
		return;
	}
}

static void
table_entry_free(struct rte_swx_table_entry *entry)
{
	if (!entry)
		return;

	free(entry->key);
	free(entry->key_mask);
	free(entry->action_data);
	free(entry);
}

#ifndef MAX_LINE_SIZE
#define MAX_LINE_SIZE 2048
#endif

static int
pipeline_table_entries_add(struct rte_swx_ctl_pipeline *p,
			   const char *table_name,
			   FILE *file,
			   uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_table_entry_read(p,
							      table_name,
							      line,
							      &is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_table_entry_add(p,
							      table_name,
							      entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	free(line);
	*file_line_number = line_id;
	return status;
}

static const char cmd_pipeline_table_add_help[] =
"pipeline <pipeline_name> table <table_name> add <file_name>\n";

static void
cmd_pipeline_table_add(char **tokens,
		       uint32_t n_tokens,
		       char *out,
		       size_t out_size,
		       void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_table_entries_add(p->ctl,
					    table_name,
					    file,
					    &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static int
pipeline_table_entries_delete(struct rte_swx_ctl_pipeline *p,
			      const char *table_name,
			      FILE *file,
			      uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_table_entry_read(p,
							      table_name,
							      line,
							      &is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_table_entry_delete(p,
								 table_name,
								 entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	*file_line_number = line_id;
	free(line);
	return status;
}

static const char cmd_pipeline_table_delete_help[] =
"pipeline <pipeline_name> table <table_name> delete <file_name>\n";

static void
cmd_pipeline_table_delete(char **tokens,
			  uint32_t n_tokens,
			  char *out,
			  size_t out_size,
			  void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_table_entries_delete(p->ctl,
					       table_name,
					       file,
					       &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static int
pipeline_table_default_entry_add(struct rte_swx_ctl_pipeline *p,
				 const char *table_name,
				 FILE *file,
				 uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_table_entry_read(p,
							      table_name,
							      line,
							      &is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_table_default_entry_add(p,
								      table_name,
								      entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	*file_line_number = line_id;
	free(line);
	return status;
}

static const char cmd_pipeline_table_default_help[] =
"pipeline <pipeline_name> table <table_name> default <file_name>\n";

static void
cmd_pipeline_table_default(char **tokens,
			   uint32_t n_tokens,
			   char *out,
			   size_t out_size,
			   void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_table_default_entry_add(p->ctl,
						  table_name,
						  file,
						  &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static const char cmd_pipeline_table_show_help[] =
"pipeline <pipeline_name> table <table_name> show\n";

static void
cmd_pipeline_table_show(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *table_name;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];
	status = rte_swx_ctl_pipeline_table_fprintf(stdout, p->ctl, table_name);
	if (status)
		snprintf(out, out_size, MSG_ARG_INVALID, "table_name");
}

static const char cmd_pipeline_selector_group_add_help[] =
"pipeline <pipeline_name> selector <selector_name> group add\n";

static void
cmd_pipeline_selector_group_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name;
	uint32_t group_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group add");
		return;
	}

	status = rte_swx_ctl_pipeline_selector_group_add(p->ctl,
		selector_name,
		&group_id);
	if (status)
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
	else
		snprintf(out, out_size, "Group ID: %u\n", group_id);
}

static const char cmd_pipeline_selector_group_delete_help[] =
"pipeline <pipeline_name> selector <selector_name> group delete <group_id>\n";

static void
cmd_pipeline_selector_group_delete(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name;
	uint32_t group_id;
	int status;

	if (n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group delete");
		return;
	}

	if (parser_read_uint32(&group_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "group_id");
		return;
	}

	status = rte_swx_ctl_pipeline_selector_group_delete(p->ctl,
		selector_name,
		group_id);
	if (status)
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
}

#define GROUP_MEMBER_INFO_TOKENS_MAX 6

static int
token_is_comment(const char *token)
{
	if ((token[0] == '#') ||
	    (token[0] == ';') ||
	    ((token[0] == '/') && (token[1] == '/')))
		return 1; /* TRUE. */

	return 0; /* FALSE. */
}

static int
pipeline_selector_group_member_read(const char *string,
				      uint32_t *group_id,
				      uint32_t *member_id,
				      uint32_t *weight,
				      int *is_blank_or_comment)
{
	char *token_array[GROUP_MEMBER_INFO_TOKENS_MAX], **tokens;
	char *s0 = NULL, *s;
	uint32_t n_tokens = 0, group_id_val = 0, member_id_val = 0, weight_val = 0;
	int blank_or_comment = 0;

	/* Check input arguments. */
	if (!string || !string[0])
		goto error;

	/* Memory allocation. */
	s0 = strdup(string);
	if (!s0)
		goto error;

	/* Parse the string into tokens. */
	for (s = s0; ; ) {
		char *token;

		token = strtok_r(s, " \f\n\r\t\v", &s);
		if (!token || token_is_comment(token))
			break;

		if (n_tokens >= GROUP_MEMBER_INFO_TOKENS_MAX)
			goto error;

		token_array[n_tokens] = token;
		n_tokens++;
	}

	if (!n_tokens) {
		blank_or_comment = 1;
		goto error;
	}

	tokens = token_array;

	if (n_tokens < 4 ||
		strcmp(tokens[0], "group") ||
		strcmp(tokens[2], "member"))
		goto error;

	/*
	 * Group ID.
	 */
	if (parser_read_uint32(&group_id_val, tokens[1]) != 0)
		goto error;
	*group_id = group_id_val;

	/*
	 * Member ID.
	 */
	if (parser_read_uint32(&member_id_val, tokens[3]) != 0)
		goto error;
	*member_id = member_id_val;

	tokens += 4;
	n_tokens -= 4;

	/*
	 * Weight.
	 */
	if (n_tokens && !strcmp(tokens[0], "weight")) {
		if (n_tokens < 2)
			goto error;

		if (parser_read_uint32(&weight_val, tokens[1]) != 0)
			goto error;
		*weight = weight_val;

		tokens += 2;
		n_tokens -= 2;
	}

	if (n_tokens)
		goto error;

	free(s0);
	return 0;

error:
	free(s0);
	if (is_blank_or_comment)
		*is_blank_or_comment = blank_or_comment;
	return -EINVAL;
}

static int
pipeline_selector_group_members_add(struct rte_swx_ctl_pipeline *p,
			   const char *selector_name,
			   FILE *file,
			   uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		uint32_t group_id, member_id, weight;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		status = pipeline_selector_group_member_read(line,
							      &group_id,
							      &member_id,
							      &weight,
							      &is_blank_or_comment);
		if (status) {
			if (is_blank_or_comment)
				continue;

			goto error;
		}

		status = rte_swx_ctl_pipeline_selector_group_member_add(p,
			selector_name,
			group_id,
			member_id,
			weight);
		if (status)
			goto error;
	}

error:
	free(line);
	*file_line_number = line_id;
	return status;
}

static const char cmd_pipeline_selector_group_member_add_help[] =
"pipeline <pipeline_name> selector <selector_name> group member add <file_name>";

static void
cmd_pipeline_selector_group_member_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "member") ||
		strcmp(tokens[6], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group member add");
		return;
	}

	file_name = tokens[7];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_selector_group_members_add(p->ctl,
					    selector_name,
					    file,
					    &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static int
pipeline_selector_group_members_delete(struct rte_swx_ctl_pipeline *p,
			   const char *selector_name,
			   FILE *file,
			   uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		uint32_t group_id, member_id, weight;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		status = pipeline_selector_group_member_read(line,
							      &group_id,
							      &member_id,
							      &weight,
							      &is_blank_or_comment);
		if (status) {
			if (is_blank_or_comment)
				continue;

			goto error;
		}

		status = rte_swx_ctl_pipeline_selector_group_member_delete(p,
			selector_name,
			group_id,
			member_id);
		if (status)
			goto error;
	}

error:
	free(line);
	*file_line_number = line_id;
	return status;
}

static const char cmd_pipeline_selector_group_member_delete_help[] =
"pipeline <pipeline_name> selector <selector_name> group member delete <file_name>";

static void
cmd_pipeline_selector_group_member_delete(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "selector") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "selector");
		return;
	}

	selector_name = tokens[3];

	if (strcmp(tokens[4], "group") ||
		strcmp(tokens[5], "member") ||
		strcmp(tokens[6], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "group member delete");
		return;
	}

	file_name = tokens[7];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_selector_group_members_delete(p->ctl,
					    selector_name,
					    file,
					    &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static const char cmd_pipeline_selector_show_help[] =
"pipeline <pipeline_name> selector <selector_name> show\n";

static void
cmd_pipeline_selector_show(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *selector_name;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	selector_name = tokens[3];
	status = rte_swx_ctl_pipeline_selector_fprintf(stdout,
		p->ctl, selector_name);
	if (status)
		snprintf(out, out_size, MSG_ARG_INVALID, "selector_name");
}

static int
pipeline_learner_default_entry_add(struct rte_swx_ctl_pipeline *p,
				   const char *learner_name,
				   FILE *file,
				   uint32_t *file_line_number)
{
	char *line = NULL;
	uint32_t line_id = 0;
	int status = 0;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return -ENOMEM;

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_table_entry *entry;
		int is_blank_or_comment;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		entry = rte_swx_ctl_pipeline_learner_default_entry_read(p,
									learner_name,
									line,
									&is_blank_or_comment);
		if (!entry) {
			if (is_blank_or_comment)
				continue;

			status = -EINVAL;
			goto error;
		}

		status = rte_swx_ctl_pipeline_learner_default_entry_add(p,
									learner_name,
									entry);
		table_entry_free(entry);
		if (status)
			goto error;
	}

error:
	*file_line_number = line_id;
	free(line);
	return status;
}

static const char cmd_pipeline_learner_default_help[] =
"pipeline <pipeline_name> learner <learner_name> default <file_name>\n";

static void
cmd_pipeline_learner_default(char **tokens,
			     uint32_t n_tokens,
			     char *out,
			     size_t out_size,
			     void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *learner_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	learner_name = tokens[3];

	file_name = tokens[5];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		return;
	}

	status = pipeline_learner_default_entry_add(p->ctl,
						    learner_name,
						    file,
						    &file_line_number);
	if (status)
		snprintf(out, out_size, "Invalid entry in file %s at line %u\n",
			 file_name,
			 file_line_number);

	fclose(file);
}

static const char cmd_pipeline_commit_help[] =
"pipeline <pipeline_name> commit\n";

static void
cmd_pipeline_commit(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	status = rte_swx_ctl_pipeline_commit(p->ctl, 1);
	if (status)
		snprintf(out, out_size, "Commit failed. "
			"Use \"commit\" to retry or \"abort\" to discard the pending work.\n");
}

static const char cmd_pipeline_abort_help[] =
"pipeline <pipeline_name> abort\n";

static void
cmd_pipeline_abort(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	rte_swx_ctl_pipeline_abort(p->ctl);
}

static const char cmd_pipeline_regrd_help[] =
"pipeline <pipeline_name> regrd <register_array_name> <index>\n";

static void
cmd_pipeline_regrd(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	const char *name;
	uint64_t value;
	uint32_t idx;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "regrd")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "regrd");
		return;
	}

	name = tokens[3];

	if (parser_read_uint32(&idx, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index");
		return;
	}

	status = rte_swx_ctl_pipeline_regarray_read(p->p, name, idx, &value);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}

	snprintf(out, out_size, "0x%" PRIx64 "\n", value);
}

static const char cmd_pipeline_regwr_help[] =
"pipeline <pipeline_name> regwr <register_array_name> <index> <value>\n";

static void
cmd_pipeline_regwr(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	const char *name;
	uint64_t value;
	uint32_t idx;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "regwr")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "regwr");
		return;
	}

	name = tokens[3];

	if (parser_read_uint32(&idx, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index");
		return;
	}

	if (parser_read_uint64(&value, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "value");
		return;
	}

	status = rte_swx_ctl_pipeline_regarray_write(p->p, name, idx, value);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

static const char cmd_pipeline_meter_profile_add_help[] =
"pipeline <pipeline_name> meter profile <profile_name> add "
	"cir <cir> pir <pir> cbs <cbs> pbs <pbs>\n";

static void
cmd_pipeline_meter_profile_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct rte_meter_trtcm_params params;
	struct pipeline *p;
	const char *profile_name;
	int status;

	if (n_tokens != 14) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[3], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[4];

	if (strcmp(tokens[5], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		return;
	}

	if (strcmp(tokens[6], "cir")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cir");
		return;
	}

	if (parser_read_uint64(&params.cir, tokens[7])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cir");
		return;
	}

	if (strcmp(tokens[8], "pir")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pir");
		return;
	}

	if (parser_read_uint64(&params.pir, tokens[9])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pir");
		return;
	}

	if (strcmp(tokens[10], "cbs")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cbs");
		return;
	}

	if (parser_read_uint64(&params.cbs, tokens[11])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cbs");
		return;
	}

	if (strcmp(tokens[12], "pbs")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pbs");
		return;
	}

	if (parser_read_uint64(&params.pbs, tokens[13])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pbs");
		return;
	}

	status = rte_swx_ctl_meter_profile_add(p->p, profile_name, &params);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

static const char cmd_pipeline_meter_profile_delete_help[] =
"pipeline <pipeline_name> meter profile <profile_name> delete\n";

static void
cmd_pipeline_meter_profile_delete(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	const char *profile_name;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	if (strcmp(tokens[3], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[4];

	if (strcmp(tokens[5], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	status = rte_swx_ctl_meter_profile_delete(p->p, profile_name);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

static const char cmd_pipeline_meter_reset_help[] =
"pipeline <pipeline_name> meter <meter_array_name> from <index0> to <index1> "
	"reset\n";

static void
cmd_pipeline_meter_reset(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	const char *name;
	uint32_t idx0 = 0, idx1 = 0;

	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "from")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&idx0, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index0");
		return;
	}

	if (strcmp(tokens[6], "to")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&idx1, tokens[7]) || (idx1 < idx0)) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index1");
		return;
	}

	if (strcmp(tokens[8], "reset")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "reset");
		return;
	}

	for ( ; idx0 <= idx1; idx0++) {
		int status;

		status = rte_swx_ctl_meter_reset(p->p, name, idx0);
		if (status) {
			snprintf(out, out_size, "Command failed for index %u.\n", idx0);
			return;
		}
	}
}

static const char cmd_pipeline_meter_set_help[] =
"pipeline <pipeline_name> meter <meter_array_name> from <index0> to <index1> "
	"set profile <profile_name>\n";

static void
cmd_pipeline_meter_set(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	const char *name, *profile_name;
	uint32_t idx0 = 0, idx1 = 0;

	if (n_tokens != 11) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "from")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&idx0, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index0");
		return;
	}

	if (strcmp(tokens[6], "to")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&idx1, tokens[7]) || (idx1 < idx0)) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index1");
		return;
	}

	if (strcmp(tokens[8], "set")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "set");
		return;
	}

	if (strcmp(tokens[9], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[10];

	for ( ; idx0 <= idx1; idx0++) {
		int status;

		status = rte_swx_ctl_meter_set(p->p, name, idx0, profile_name);
		if (status) {
			snprintf(out, out_size, "Command failed for index %u.\n", idx0);
			return;
		}
	}
}

static const char cmd_pipeline_meter_stats_help[] =
"pipeline <pipeline_name> meter <meter_array_name> from <index0> to <index1> "
	"stats\n";

static void
cmd_pipeline_meter_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct rte_swx_ctl_meter_stats stats;
	struct pipeline *p;
	const char *name;
	uint32_t idx0 = 0, idx1 = 0;

	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "from")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
		return;
	}

	if (parser_read_uint32(&idx0, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index0");
		return;
	}

	if (strcmp(tokens[6], "to")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
		return;
	}

	if (parser_read_uint32(&idx1, tokens[7]) || (idx1 < idx0)) {
		snprintf(out, out_size, MSG_ARG_INVALID, "index1");
		return;
	}

	if (strcmp(tokens[8], "stats")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	/* Table header. */
	snprintf(out, out_size, "+-%7s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+\n",
		 "-------",
		 "----------------", "----------------", "----------------",
		 "----------------", "----------------", "----------------");
	out_size -= strlen(out);
	out += strlen(out);

	snprintf(out, out_size, "| %4s | %16s | %16s | %16s | %16s | %16s | %16s |\n",
		 "METER #",
		 "GREEN (packets)", "YELLOW (packets)", "RED (packets)",
		 "GREEN (bytes)", "YELLOW (bytes)", "RED (bytes)");
	out_size -= strlen(out);
	out += strlen(out);

	snprintf(out, out_size, "+-%7s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+-%16s-+\n",
		 "-------",
		 "----------------", "----------------", "----------------",
		 "----------------", "----------------", "----------------");
	out_size -= strlen(out);
	out += strlen(out);

	/* Table rows. */
	for ( ; idx0 <= idx1; idx0++) {
		int status;

		status = rte_swx_ctl_meter_stats_read(p->p, name, idx0, &stats);
		if (status) {
			snprintf(out, out_size, "Pipeline meter stats error at index %u.\n", idx0);
			out_size -= strlen(out);
			out += strlen(out);
			return;
		}

		snprintf(out, out_size, "| %7d | %16" PRIx64 " | %16" PRIx64 " | %16" PRIx64
			 " | %16" PRIx64 " | %16" PRIx64 " | %16" PRIx64 " |\n",
			 idx0,
			 stats.n_pkts[RTE_COLOR_GREEN],
			 stats.n_pkts[RTE_COLOR_YELLOW],
			 stats.n_pkts[RTE_COLOR_RED],
			 stats.n_bytes[RTE_COLOR_GREEN],
			 stats.n_bytes[RTE_COLOR_YELLOW],
			 stats.n_bytes[RTE_COLOR_RED]);
		out_size -= strlen(out);
		out += strlen(out);
	}
}

static const char cmd_pipeline_stats_help[] =
"pipeline <pipeline_name> stats\n";

static void
cmd_pipeline_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct rte_swx_ctl_pipeline_info info;
	struct pipeline *p;
	uint32_t i;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = pipeline_find(obj, tokens[1]);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "stats")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	status = rte_swx_ctl_pipeline_info_get(p->p, &info);
	if (status) {
		snprintf(out, out_size, "Pipeline info get error.");
		return;
	}

	snprintf(out, out_size, "Input ports:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_ports_in; i++) {
		struct rte_swx_port_in_stats stats;

		rte_swx_ctl_pipeline_port_in_stats_read(p->p, i, &stats);

		snprintf(out, out_size, "\tPort %u:"
			" packets %" PRIu64
			" bytes %" PRIu64
			" empty %" PRIu64 "\n",
			i, stats.n_pkts, stats.n_bytes, stats.n_empty);
		out_size -= strlen(out);
		out += strlen(out);
	}

	snprintf(out, out_size, "\nOutput ports:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_ports_out; i++) {
		struct rte_swx_port_out_stats stats;

		rte_swx_ctl_pipeline_port_out_stats_read(p->p, i, &stats);

		snprintf(out, out_size, "\tPort %u:"
			" packets %" PRIu64
			" bytes %" PRIu64 "\n",
			i, stats.n_pkts, stats.n_bytes);
		out_size -= strlen(out);
		out += strlen(out);
	}

	snprintf(out, out_size, "\nTables:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_tables; i++) {
		struct rte_swx_ctl_table_info table_info;
		uint64_t n_pkts_action[info.n_actions];
		struct rte_swx_table_stats stats = {
			.n_pkts_hit = 0,
			.n_pkts_miss = 0,
			.n_pkts_action = n_pkts_action,
		};
		uint32_t j;

		status = rte_swx_ctl_table_info_get(p->p, i, &table_info);
		if (status) {
			snprintf(out, out_size, "Table info get error.");
			return;
		}

		status = rte_swx_ctl_pipeline_table_stats_read(p->p, table_info.name, &stats);
		if (status) {
			snprintf(out, out_size, "Table stats read error.");
			return;
		}

		snprintf(out, out_size, "\tTable %s:\n"
			"\t\tHit (packets): %" PRIu64 "\n"
			"\t\tMiss (packets): %" PRIu64 "\n",
			table_info.name,
			stats.n_pkts_hit,
			stats.n_pkts_miss);
		out_size -= strlen(out);
		out += strlen(out);

		for (j = 0; j < info.n_actions; j++) {
			struct rte_swx_ctl_action_info action_info;

			status = rte_swx_ctl_action_info_get(p->p, j, &action_info);
			if (status) {
				snprintf(out, out_size, "Action info get error.");
				return;
			}

			snprintf(out, out_size, "\t\tAction %s (packets): %" PRIu64 "\n",
				action_info.name,
				stats.n_pkts_action[j]);
			out_size -= strlen(out);
			out += strlen(out);
		}
	}

	snprintf(out, out_size, "\nLearner tables:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_learners; i++) {
		struct rte_swx_ctl_learner_info learner_info;
		uint64_t n_pkts_action[info.n_actions];
		struct rte_swx_learner_stats stats = {
			.n_pkts_hit = 0,
			.n_pkts_miss = 0,
			.n_pkts_action = n_pkts_action,
		};
		uint32_t j;

		status = rte_swx_ctl_learner_info_get(p->p, i, &learner_info);
		if (status) {
			snprintf(out, out_size, "Learner table info get error.");
			return;
		}

		status = rte_swx_ctl_pipeline_learner_stats_read(p->p, learner_info.name, &stats);
		if (status) {
			snprintf(out, out_size, "Learner table stats read error.");
			return;
		}

		snprintf(out, out_size, "\tLearner table %s:\n"
			"\t\tHit (packets): %" PRIu64 "\n"
			"\t\tMiss (packets): %" PRIu64 "\n"
			"\t\tLearn OK (packets): %" PRIu64 "\n"
			"\t\tLearn error (packets): %" PRIu64 "\n"
			"\t\tForget (packets): %" PRIu64 "\n",
			learner_info.name,
			stats.n_pkts_hit,
			stats.n_pkts_miss,
			stats.n_pkts_learn_ok,
			stats.n_pkts_learn_err,
			stats.n_pkts_forget);
		out_size -= strlen(out);
		out += strlen(out);

		for (j = 0; j < info.n_actions; j++) {
			struct rte_swx_ctl_action_info action_info;

			status = rte_swx_ctl_action_info_get(p->p, j, &action_info);
			if (status) {
				snprintf(out, out_size, "Action info get error.");
				return;
			}

			snprintf(out, out_size, "\t\tAction %s (packets): %" PRIu64 "\n",
				action_info.name,
				stats.n_pkts_action[j]);
			out_size -= strlen(out);
			out += strlen(out);
		}
	}
}

static const char cmd_thread_pipeline_enable_help[] =
"thread <thread_id> pipeline <pipeline_name> enable\n";

static void
cmd_thread_pipeline_enable(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	char *pipeline_name;
	struct pipeline *p;
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
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[4], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = thread_pipeline_enable(thread_id, obj, pipeline_name);
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
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
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
	p = pipeline_find(obj, pipeline_name);
	if (!p || !p->ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[4], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = thread_pipeline_disable(thread_id, obj, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL,
			"thread pipeline disable");
		return;
	}
}

static void
cmd_help(char **tokens,
	 uint32_t n_tokens,
	 char *out,
	 size_t out_size,
	 void *arg __rte_unused)
{
	tokens++;
	n_tokens--;

	if (n_tokens == 0) {
		snprintf(out, out_size,
			"Type 'help <command>' for command details.\n\n"
			"List of commands:\n"
			"\tmempool\n"
			"\tlink\n"
			"\ttap\n"
			"\tpipeline create\n"
			"\tpipeline port in\n"
			"\tpipeline port out\n"
			"\tpipeline build\n"
			"\tpipeline table add\n"
			"\tpipeline table delete\n"
			"\tpipeline table default\n"
			"\tpipeline table show\n"
			"\tpipeline selector group add\n"
			"\tpipeline selector group delete\n"
			"\tpipeline selector group member add\n"
			"\tpipeline selector group member delete\n"
			"\tpipeline selector show\n"
			"\tpipeline learner default\n"
			"\tpipeline commit\n"
			"\tpipeline abort\n"
			"\tpipeline regrd\n"
			"\tpipeline regwr\n"
			"\tpipeline meter profile add\n"
			"\tpipeline meter profile delete\n"
			"\tpipeline meter reset\n"
			"\tpipeline meter set\n"
			"\tpipeline meter stats\n"
			"\tpipeline stats\n"
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

	if (strcmp(tokens[0], "ring") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_ring_help);
		return;
	}

	if (strcmp(tokens[0], "tap") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_tap_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "create") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_create_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) && (strcmp(tokens[1], "port") == 0)) {
		if (strcmp(tokens[2], "in") == 0) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_in_help);
			return;
		}

		if (strcmp(tokens[2], "out") == 0) {
			snprintf(out, out_size, "\n%s\n",
				cmd_pipeline_port_out_help);
			return;
		}
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "build") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_build_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) &&
		(strcmp(tokens[1], "table") == 0) &&
		(strcmp(tokens[2], "add") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_table_add_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) &&
		(strcmp(tokens[1], "table") == 0) &&
		(strcmp(tokens[2], "delete") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_table_delete_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) &&
		(strcmp(tokens[1], "table") == 0) &&
		(strcmp(tokens[2], "default") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_table_default_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) &&
		(strcmp(tokens[1], "table") == 0) &&
		(strcmp(tokens[2], "show") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_table_show_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 4) &&
		(strcmp(tokens[1], "selector") == 0) &&
		(strcmp(tokens[2], "group") == 0) &&
		(strcmp(tokens[3], "add") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_selector_group_add_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 4) &&
		(strcmp(tokens[1], "selector") == 0) &&
		(strcmp(tokens[2], "group") == 0) &&
		(strcmp(tokens[3], "delete") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_selector_group_delete_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 5) &&
		(strcmp(tokens[1], "selector") == 0) &&
		(strcmp(tokens[2], "group") == 0) &&
		(strcmp(tokens[3], "member") == 0) &&
		(strcmp(tokens[4], "add") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_selector_group_member_add_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 5) &&
		(strcmp(tokens[1], "selector") == 0) &&
		(strcmp(tokens[2], "group") == 0) &&
		(strcmp(tokens[3], "member") == 0) &&
		(strcmp(tokens[4], "delete") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_selector_group_member_delete_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) &&
		(strcmp(tokens[1], "selector") == 0) &&
		(strcmp(tokens[2], "show") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_selector_show_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 3) &&
		(strcmp(tokens[1], "learner") == 0) &&
		(strcmp(tokens[2], "default") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_learner_default_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) &&
		(strcmp(tokens[1], "commit") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_commit_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) &&
		(strcmp(tokens[1], "abort") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_abort_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "regrd") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_regrd_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "regwr") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_regwr_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 4) && !strcmp(tokens[1], "meter")
		&& !strcmp(tokens[2], "profile")
		&& !strcmp(tokens[3], "add")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_meter_profile_add_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 4) && !strcmp(tokens[1], "meter")
		&& !strcmp(tokens[2], "profile")
		&& !strcmp(tokens[3], "delete")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_meter_profile_delete_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 3) && !strcmp(tokens[1], "meter")
		&& !strcmp(tokens[2], "reset")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_meter_reset_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 3) && !strcmp(tokens[1], "meter")
		&& !strcmp(tokens[2], "set")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_meter_set_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 3) && !strcmp(tokens[1], "meter")
		&& !strcmp(tokens[2], "stats")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_meter_stats_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "stats") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_stats_help);
		return;
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
cli_process(char *in, char *out, size_t out_size, void *obj)
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
		cmd_help(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "mempool") == 0) {
		cmd_mempool(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		if ((n_tokens >= 2) && (strcmp(tokens[1], "show") == 0)) {
			cmd_link_show(tokens, n_tokens, out, out_size, obj);
			return;
		}

		cmd_link(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "ring") == 0) {
		cmd_ring(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "tap") == 0) {
		cmd_tap(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "pipeline") == 0) {
		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "create") == 0)) {
			cmd_pipeline_create(tokens, n_tokens, out, out_size,
				obj);
			return;
		}

		if ((n_tokens >= 4) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0)) {
			cmd_pipeline_port_in(tokens, n_tokens, out, out_size,
				obj);
			return;
		}

		if ((n_tokens >= 4) &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "out") == 0)) {
			cmd_pipeline_port_out(tokens, n_tokens, out, out_size,
				obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "build") == 0)) {
			cmd_pipeline_build(tokens, n_tokens, out, out_size,
				obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "add") == 0)) {
			cmd_pipeline_table_add(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "delete") == 0)) {
			cmd_pipeline_table_delete(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "default") == 0)) {
			cmd_pipeline_table_default(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[4], "show") == 0)) {
			cmd_pipeline_table_show(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "selector") == 0) &&
			(strcmp(tokens[4], "group") == 0) &&
			(strcmp(tokens[5], "add") == 0)) {
			cmd_pipeline_selector_group_add(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "selector") == 0) &&
			(strcmp(tokens[4], "group") == 0) &&
			(strcmp(tokens[5], "delete") == 0)) {
			cmd_pipeline_selector_group_delete(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "selector") == 0) &&
			(strcmp(tokens[4], "group") == 0) &&
			(strcmp(tokens[5], "member") == 0) &&
			(strcmp(tokens[6], "add") == 0)) {
			cmd_pipeline_selector_group_member_add(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 7) &&
			(strcmp(tokens[2], "selector") == 0) &&
			(strcmp(tokens[4], "group") == 0) &&
			(strcmp(tokens[5], "member") == 0) &&
			(strcmp(tokens[6], "delete") == 0)) {
			cmd_pipeline_selector_group_member_delete(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "selector") == 0) &&
			(strcmp(tokens[4], "show") == 0)) {
			cmd_pipeline_selector_show(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[2], "learner") == 0) &&
			(strcmp(tokens[4], "default") == 0)) {
			cmd_pipeline_learner_default(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "commit") == 0)) {
			cmd_pipeline_commit(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "abort") == 0)) {
			cmd_pipeline_abort(tokens, n_tokens, out,
				out_size, obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "regrd") == 0)) {
			cmd_pipeline_regrd(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "regwr") == 0)) {
			cmd_pipeline_regwr(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[3], "profile") == 0) &&
			(strcmp(tokens[5], "add") == 0)) {
			cmd_pipeline_meter_profile_add(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 6) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[3], "profile") == 0) &&
			(strcmp(tokens[5], "delete") == 0)) {
			cmd_pipeline_meter_profile_delete(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 9) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[8], "reset") == 0)) {
			cmd_pipeline_meter_reset(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 9) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[8], "set") == 0)) {
			cmd_pipeline_meter_set(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 9) &&
			(strcmp(tokens[2], "meter") == 0) &&
			(strcmp(tokens[8], "stats") == 0)) {
			cmd_pipeline_meter_stats(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "stats") == 0)) {
			cmd_pipeline_stats(tokens, n_tokens, out, out_size,
				obj);
			return;
		}
	}

	if (strcmp(tokens[0], "thread") == 0) {
		if ((n_tokens >= 5) &&
			(strcmp(tokens[4], "enable") == 0)) {
			cmd_thread_pipeline_enable(tokens, n_tokens,
				out, out_size, obj);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[4], "disable") == 0)) {
			cmd_thread_pipeline_disable(tokens, n_tokens,
				out, out_size, obj);
			return;
		}
	}

	snprintf(out, out_size, MSG_CMD_UNKNOWN, tokens[0]);
}

int
cli_script_process(const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max,
	void *obj)
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
			msg_out_len_max,
			obj);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	free(msg_out);
	free(msg_in);
	return 0;
}
