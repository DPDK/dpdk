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
#include <rte_swx_port_source_sink.h>
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

	val = strtoul(p, &next, 10);
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
		"\tether %02X:%02X:%02X:%02X:%02X:%02X rxqueues %u txqueues %u\n"
		"\tport# %u  speed %s\n"
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
"   | source <mempool_name> <file_name>\n";

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
	} else if (strcmp(tokens[t0], "source") == 0) {
		struct rte_swx_port_source_params params;
		struct mempool *mp;

		if (n_tokens < t0 + 3) {
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

		t0 += 3;

		status = rte_swx_pipeline_port_in_config(p->p,
			port_id,
			"source",
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
"   | sink <file_name> | none\n";

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
	} else if (strcmp(tokens[t0], "sink") == 0) {
		struct rte_swx_port_sink_params params;

		params.file_name = strcmp(tokens[t0 + 1], "none") ?
			tokens[t0 + 1] : NULL;

		t0 += 2;

		status = rte_swx_pipeline_port_out_config(p->p,
			port_id,
			"sink",
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

static const char cmd_pipeline_table_update_help[] =
"pipeline <pipeline_name> table <table_name> update <file_name_add> "
"<file_name_delete> <file_name_default>";

static void
cmd_pipeline_table_update(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj)
{
	struct pipeline *p;
	char *pipeline_name, *table_name, *line = NULL;
	char *file_name_add, *file_name_delete, *file_name_default;
	FILE *file_add = NULL, *file_delete = NULL, *file_default = NULL;
	uint32_t line_id;
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

	if (strcmp(tokens[2], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	table_name = tokens[3];

	if (strcmp(tokens[4], "update") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "update");
		return;
	}

	file_name_add = tokens[5];
	file_name_delete = tokens[6];
	file_name_default = tokens[7];

	/* File open. */
	if (strcmp(file_name_add, "none")) {
		file_add = fopen(file_name_add, "r");
		if (!file_add) {
			snprintf(out, out_size, "Cannot open file %s",
				file_name_add);
			goto error;
		}
	}

	if (strcmp(file_name_delete, "none")) {
		file_delete = fopen(file_name_delete, "r");
		if (!file_delete) {
			snprintf(out, out_size, "Cannot open file %s",
				file_name_delete);
			goto error;
		}
	}

	if (strcmp(file_name_default, "none")) {
		file_default = fopen(file_name_default, "r");
		if (!file_default) {
			snprintf(out, out_size, "Cannot open file %s",
				file_name_default);
			goto error;
		}
	}

	if (!file_add && !file_delete && !file_default) {
		snprintf(out, out_size, "Nothing to be done.");
		return;
	}

	/* Buffer allocation. */
	line = malloc(2048);
	if (!line) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		goto error;
	}

	/* Add. */
	if (file_add)
		for (line_id = 1; ; line_id++) {
			struct rte_swx_table_entry *entry;

			if (fgets(line, 2048, file_add) == NULL)
				break;

			entry = rte_swx_ctl_pipeline_table_entry_read(p->ctl,
				table_name,
				line);
			if (!entry) {
				snprintf(out, out_size, MSG_FILE_ERR,
					file_name_add, line_id);
				goto error;
			}

			status = rte_swx_ctl_pipeline_table_entry_add(p->ctl,
				table_name,
				entry);
			table_entry_free(entry);
			if (status) {
				snprintf(out, out_size,
					"Invalid entry in file %s at line %u",
					file_name_add, line_id);
				goto error;
			}
		}


	/* Delete. */
	if (file_delete)
		for (line_id = 1; ; line_id++) {
			struct rte_swx_table_entry *entry;

			if (fgets(line, 2048, file_delete) == NULL)
				break;

			entry = rte_swx_ctl_pipeline_table_entry_read(p->ctl,
				table_name,
				line);
			if (!entry) {
				snprintf(out, out_size, MSG_FILE_ERR,
					file_name_delete, line_id);
				goto error;
			}

			status = rte_swx_ctl_pipeline_table_entry_delete(p->ctl,
				table_name,
				entry);
			table_entry_free(entry);
			if (status)  {
				snprintf(out, out_size,
					"Invalid entry in file %s at line %u",
					file_name_delete, line_id);
				goto error;
			}
		}

	/* Default. */
	if (file_default)
		for (line_id = 1; ; line_id++) {
			struct rte_swx_table_entry *entry;

			if (fgets(line, 2048, file_default) == NULL)
				break;

			entry = rte_swx_ctl_pipeline_table_entry_read(p->ctl,
				table_name,
				line);
			if (!entry) {
				snprintf(out, out_size, MSG_FILE_ERR,
					file_name_default, line_id);
				goto error;
			}

			status = rte_swx_ctl_pipeline_table_default_entry_add(p->ctl,
				table_name,
				entry);
			table_entry_free(entry);
			if (status) {
				snprintf(out, out_size,
					"Invalid entry in file %s at line %u",
					file_name_default, line_id);
				goto error;
			}
		}

	status = rte_swx_ctl_pipeline_commit(p->ctl, 1);
	if (status) {
		snprintf(out, out_size, "Commit failed.");
		goto error;
	}


	rte_swx_ctl_pipeline_table_fprintf(stdout, p->ctl, table_name);

	free(line);
	if (file_add)
		fclose(file_add);
	if (file_delete)
		fclose(file_delete);
	if (file_default)
		fclose(file_default);
	return;

error:
	rte_swx_ctl_pipeline_abort(p->ctl);
	free(line);
	if (file_add)
		fclose(file_add);
	if (file_delete)
		fclose(file_delete);
	if (file_default)
		fclose(file_default);
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

	snprintf(out, out_size, "Output ports:\n");
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
			"\tpipeline create\n"
			"\tpipeline port in\n"
			"\tpipeline port out\n"
			"\tpipeline build\n"
			"\tpipeline table update\n"
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
		(strcmp(tokens[2], "update") == 0)) {
		snprintf(out, out_size, "\n%s\n",
			cmd_pipeline_table_update_help);
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

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "table") == 0)) {
			cmd_pipeline_table_update(tokens, n_tokens, out,
				out_size, obj);
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
