/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_swx_port_ethdev.h>
#include <rte_swx_port_ring.h>
#include <rte_swx_port_source_sink.h>
#include <rte_swx_port_fd.h>
#include <rte_swx_pipeline.h>
#include <rte_swx_ctl.h>
#include <rte_swx_ipsec.h>

#include "cli.h"

#include "obj.h"
#include "thread.h"

#ifndef CMD_MAX_TOKENS
#define CMD_MAX_TOKENS     256
#endif

#ifndef MAX_LINE_SIZE
#define MAX_LINE_SIZE 2048
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

static struct rte_swx_table_entry *
parse_table_entry(struct rte_swx_ctl_pipeline *p,
		  char *table_name,
		  char **tokens,
		  uint32_t n_tokens)
{
	struct rte_swx_table_entry *entry;
	char *line;
	uint32_t i;

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line)
		return NULL;

	/* Copy tokens to buffer. Since the tokens were initially part of a buffer of size
	 * MAX_LINE_LENGTH, it is guaranteed that putting back some of them into a buffer of the
	 * same size separated by a single space will not result in buffer overrun.
	 */
	line[0] = 0;
	for (i = 0; i < n_tokens; i++) {
		if (i)
			strcat(line, " ");

		strcat(line, tokens[i]);
	}

	/* Read the table entry from the input buffer. */
	entry = rte_swx_ctl_pipeline_table_entry_read(p, table_name, line, NULL);

	/* Buffer free. */
	free(line);

	return entry;
}

static const char cmd_mempool_help[] =
"mempool <mempool_name> "
"meta <mbuf_private_size> "
"pkt <pkt_buffer_size> "
"pool <pool_size> "
"cache <cache_size> "
"numa <numa_node>\n";

static void
cmd_mempool(char **tokens,
	    uint32_t n_tokens,
	    char *out,
	    size_t out_size,
	    void *obj __rte_unused)
{
	struct rte_mempool *mp;
	char *mempool_name;
	uint32_t mbuf_private_size, pkt_buffer_size, pool_size, cache_size, numa_node;

	if (n_tokens != 12) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	mempool_name = tokens[1];

	if (strcmp(tokens[2], "meta")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meta");
		return;
	}

	if (parser_read_uint32(&mbuf_private_size, tokens[3])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "mbuf_private_size");
		return;
	}

	if (strcmp(tokens[4], "pkt")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pkt");
		return;
	}

	if (parser_read_uint32(&pkt_buffer_size, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pkt_buffer_size");
		return;
	}

	if (strcmp(tokens[6], "pool")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pool");
		return;
	}

	if (parser_read_uint32(&pool_size, tokens[7])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pool_size");
		return;
	}

	if (strcmp(tokens[8], "cache")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cache");
		return;
	}

	if (parser_read_uint32(&cache_size, tokens[9])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cache_size");
		return;
	}

	if (strcmp(tokens[10], "numa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "numa");
		return;
	}

	if (parser_read_uint32(&numa_node, tokens[11])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	mp = rte_pktmbuf_pool_create(mempool_name,
				     pool_size,
				     cache_size,
				     mbuf_private_size,
				     pkt_buffer_size,
				     numa_node);
	if (!mp) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_ethdev_help[] =
"ethdev <ethdev_name>\n"
"   rxq <n_queues> <queue_size> <mempool_name>\n"
"   txq <n_queues> <queue_size>\n"
"   promiscuous on | off\n"
"   [rss <qid_0> ... <qid_n>]\n";

static void
cmd_ethdev(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct ethdev_params p;
	struct ethdev_params_rss rss;
	char *name;
	int status;

	memset(&p, 0, sizeof(p));
	memset(&rss, 0, sizeof(rss));

	if (n_tokens < 11 || n_tokens > 12 + ETHDEV_RXQ_RSS_MAX) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
	name = tokens[1];

	if (strcmp(tokens[2], "rxq") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rxq");
		return;
	}

	if (parser_read_uint32(&p.rx.n_queues, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queues");
		return;
	}
	if (parser_read_uint32(&p.rx.queue_size, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	p.rx.mempool_name = tokens[5];

	if (strcmp(tokens[6], "txq") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "txq");
		return;
	}

	if (parser_read_uint32(&p.tx.n_queues, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queues");
		return;
	}

	if (parser_read_uint32(&p.tx.queue_size, tokens[8]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	if (strcmp(tokens[9], "promiscuous") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "promiscuous");
		return;
	}

	if (strcmp(tokens[10], "on") == 0)
		p.promiscuous = 1;
	else if (strcmp(tokens[10], "off") == 0)
		p.promiscuous = 0;
	else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "on or off");
		return;
	}

	/* RSS */
	p.rx.rss = NULL;
	if (n_tokens > 11) {
		uint32_t queue_id, i;

		if (strcmp(tokens[11], "rss") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rss");
			return;
		}

		p.rx.rss = &rss;

		rss.n_queues = 0;
		for (i = 12; i < n_tokens; i++) {
			if (parser_read_uint32(&queue_id, tokens[i]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID,
					"queue_id");
				return;
			}

			rss.queue_id[rss.n_queues] = queue_id;
			rss.n_queues++;
		}
	}

	status = ethdev_config(name, &p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static void
ethdev_show(uint16_t port_id, char **out, size_t *out_size)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_eth_dev_info info;
	struct rte_eth_stats stats;
	struct rte_ether_addr addr;
	struct rte_eth_link link;
	uint32_t length;
	uint16_t mtu = 0;

	if (!rte_eth_dev_is_valid_port(port_id))
		return;

	rte_eth_dev_get_name_by_port(port_id, name);
	rte_eth_dev_info_get(port_id, &info);
	rte_eth_stats_get(port_id, &stats);
	rte_eth_macaddr_get(port_id, &addr);
	rte_eth_link_get(port_id, &link);
	rte_eth_dev_get_mtu(port_id, &mtu);

	snprintf(*out, *out_size,
		 "%s: flags=<%s> mtu %u\n"
		 "\tether " RTE_ETHER_ADDR_PRT_FMT " rxqueues %u txqueues %u\n"
		 "\tport# %u  speed %s\n"
		 "\tRX packets %" PRIu64"  bytes %" PRIu64"\n"
		 "\tRX errors %" PRIu64"  missed %" PRIu64"  no-mbuf %" PRIu64"\n"
		 "\tTX packets %" PRIu64"  bytes %" PRIu64"\n"
		 "\tTX errors %" PRIu64"\n\n",
		 name,
		 link.link_status ? "UP" : "DOWN",
		 mtu,
		 RTE_ETHER_ADDR_BYTES(&addr),
		 info.nb_rx_queues,
		 info.nb_tx_queues,
		 port_id,
		 rte_eth_link_speed_to_str(link.link_speed),
		 stats.ipackets,
		 stats.ibytes,
		 stats.ierrors,
		 stats.imissed,
		 stats.rx_nombuf,
		 stats.opackets,
		 stats.obytes,
		 stats.oerrors);

	length = strlen(*out);
	*out_size -= length;
	*out += length;
}


static char cmd_ethdev_show_help[] =
"ethdev show [ <ethdev_name> ]\n";

static void
cmd_ethdev_show(char **tokens,
	      uint32_t n_tokens,
	      char *out,
	      size_t out_size,
	      void *obj __rte_unused)
{
	uint16_t port_id;

	if (n_tokens != 2 && n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	/* Single device. */
	if (n_tokens == 3) {
		int status;

		status = rte_eth_dev_get_port_by_name(tokens[2], &port_id);
		if (status)
			snprintf(out, out_size, "Error: Invalid Ethernet device name.\n");

		ethdev_show(port_id, &out, &out_size);
		return;
	}

	/*  All devices. */
	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
		if (rte_eth_dev_is_valid_port(port_id))
			ethdev_show(port_id, &out, &out_size);
}

static const char cmd_ring_help[] =
"ring <ring_name> size <size> numa <numa_node>\n";

static void
cmd_ring(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_ring *r;
	char *name;
	uint32_t size, numa_node;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "size")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
		return;
	}

	if (parser_read_uint32(&size, tokens[3])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "size");
		return;
	}

	if (strcmp(tokens[4], "numa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "numa");
		return;
	}

	if (parser_read_uint32(&numa_node, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	r = rte_ring_create(
		name,
		size,
		(int)numa_node,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (!r) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

static const char cmd_cryptodev_help[] =
"cryptodev <cryptodev_name> queues <n_queue_pairs> qsize <queue_size>\n";

static void
cmd_cryptodev(char **tokens,
	      uint32_t n_tokens,
	      char *out,
	      size_t out_size,
	      void *obj __rte_unused)
{
	struct cryptodev_params params;
	char *cryptodev_name;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[0], "cryptodev")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cryptodev");
		return;
	}

	cryptodev_name = tokens[1];

	if (strcmp(tokens[2], "queues")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "queues");
		return;
	}

	if (parser_read_uint32(&params.n_queue_pairs, tokens[3])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_queue_pairs");
		return;
	}

	if (strcmp(tokens[4], "qsize")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "qsize");
		return;
	}


	if (parser_read_uint32(&params.queue_size, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "queue_size");
		return;
	}

	status = cryptodev_config(cryptodev_name, &params);
	if (status)
		snprintf(out, out_size, "Crypto device configuration failed (%d).\n", status);
}

static const char cmd_pipeline_codegen_help[] =
"pipeline codegen <spec_file> <code_file>\n";

static void
cmd_pipeline_codegen(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	FILE *spec_file = NULL;
	FILE *code_file = NULL;
	uint32_t err_line;
	const char *err_msg;
	int status;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	spec_file = fopen(tokens[2], "r");
	if (!spec_file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[2]);
		return;
	}

	code_file = fopen(tokens[3], "w");
	if (!code_file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[3]);
		fclose(spec_file);
		return;
	}

	status = rte_swx_pipeline_codegen(spec_file,
					  code_file,
					  &err_line,
					  &err_msg);

	fclose(spec_file);
	fclose(code_file);

	if (status) {
		snprintf(out, out_size, "Error %d at line %u: %s\n.",
			status, err_line, err_msg);
		return;
	}
}

static const char cmd_pipeline_libbuild_help[] =
"pipeline libbuild <code_file> <lib_file>\n";

static void
cmd_pipeline_libbuild(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	char *code_file, *lib_file, *obj_file = NULL, *log_file = NULL;
	char *install_dir, *cwd = NULL, *buffer = NULL;
	size_t length;
	int status = 0;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		goto free;
	}

	install_dir = getenv("RTE_INSTALL_DIR");
	if (!install_dir) {
		cwd = malloc(MAX_LINE_SIZE);
		if (!cwd) {
			snprintf(out, out_size, MSG_OUT_OF_MEMORY);
			goto free;
		}

		install_dir = getcwd(cwd, MAX_LINE_SIZE);
		if (!install_dir) {
			snprintf(out, out_size, "Error: Path too long.\n");
			goto free;
		}
	}

	snprintf(out, out_size, "Using DPDK source code from \"%s\".\n", install_dir);
	out_size -= strlen(out);
	out += strlen(out);

	code_file = tokens[2];
	length = strnlen(code_file, MAX_LINE_SIZE);
	if ((length < 3) ||
	    (code_file[length - 2] != '.') ||
	    (code_file[length - 1] != 'c')) {
		snprintf(out, out_size, MSG_ARG_INVALID, "code_file");
		goto free;
	}

	lib_file = tokens[3];
	length = strnlen(lib_file, MAX_LINE_SIZE);
	if ((length < 4) ||
	    (lib_file[length - 3] != '.') ||
	    (lib_file[length - 2] != 's') ||
	    (lib_file[length - 1] != 'o')) {
		snprintf(out, out_size, MSG_ARG_INVALID, "lib_file");
		goto free;
	}

	obj_file = malloc(length);
	log_file = malloc(length + 2);
	if (!obj_file || !log_file) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		goto free;
	}

	memcpy(obj_file, lib_file, length - 2);
	obj_file[length - 2] = 'o';
	obj_file[length - 1] = 0;

	memcpy(log_file, lib_file, length - 2);
	log_file[length - 2] = 'l';
	log_file[length - 1] = 'o';
	log_file[length] = 'g';
	log_file[length + 1] = 0;

	buffer = malloc(MAX_LINE_SIZE);
	if (!buffer) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		goto free;
	}

	snprintf(buffer,
		 MAX_LINE_SIZE,
		 "gcc -c -O3 -fpic -Wno-deprecated-declarations -o %s %s "
		 "-I %s/lib/pipeline "
		 "-I %s/lib/eal/include "
		 "-I %s/lib/eal/x86/include "
		 "-I %s/lib/eal/include/generic "
		 "-I %s/lib/meter "
		 "-I %s/lib/port "
		 "-I %s/lib/table "
		 "-I %s/lib/pipeline "
		 "-I %s/config "
		 "-I %s/build "
		 "-I %s/lib/eal/linux/include "
		 ">%s 2>&1 "
		 "&& "
		 "gcc -shared %s -o %s "
		 ">>%s 2>&1",
		 obj_file,
		 code_file,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 install_dir,
		 log_file,
		 obj_file,
		 lib_file,
		 log_file);

	status = system(buffer);
	if (status) {
		snprintf(out,
			 out_size,
			 "Library build failed, see file \"%s\" for details.\n",
			 log_file);
		goto free;
	}

free:
	free(cwd);
	free(obj_file);
	free(log_file);
	free(buffer);
}

static const char cmd_pipeline_build_help[] =
"pipeline <pipeline_name> build lib <lib_file> io <iospec_file> numa <numa_node>\n";

static void
cmd_pipeline_build(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_pipeline *p = NULL;
	struct rte_swx_ctl_pipeline *ctl = NULL;
	char *pipeline_name, *lib_file_name, *iospec_file_name;
	FILE *iospec_file = NULL;
	uint32_t numa_node = 0;
	int status = 0;

	/* Parsing. */
	if (n_tokens != 9) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];

	if (strcmp(tokens[2], "build")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "build");
		return;
	}

	if (strcmp(tokens[3], "lib")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "lib");
		return;
	}

	lib_file_name = tokens[4];

	if (strcmp(tokens[5], "io")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "io");
		return;
	}

	iospec_file_name = tokens[6];

	if (strcmp(tokens[7], "numa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "numa");
		return;
	}

	if (parser_read_uint32(&numa_node, tokens[8])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	/* I/O spec file open. */
	iospec_file = fopen(iospec_file_name, "r");
	if (!iospec_file) {
		snprintf(out, out_size, "Cannot open file \"%s\".\n", iospec_file_name);
		return;
	}

	status = rte_swx_pipeline_build_from_lib(&p,
						 pipeline_name,
						 lib_file_name,
						 iospec_file,
						 (int)numa_node);
	if (status) {
		snprintf(out, out_size, "Pipeline build failed (%d).", status);
		goto free;
	}

	ctl = rte_swx_ctl_pipeline_create(p);
	if (!ctl) {
		snprintf(out, out_size, "Pipeline control create failed.");
		goto free;
	}

free:
	if (status)
		rte_swx_pipeline_free(p);

	if (iospec_file)
		fclose(iospec_file);
}

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
		       void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = pipeline_table_entries_add(ctl,
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
			  void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = pipeline_table_entries_delete(ctl,
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
			   void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *table_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = pipeline_table_default_entry_add(ctl,
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
"pipeline <pipeline_name> table <table_name> show [filename]\n";

static void
cmd_pipeline_table_show(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *table_name;
	FILE *file = NULL;
	int status;

	if (n_tokens != 5 && n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	table_name = tokens[3];
	file = (n_tokens == 6) ? fopen(tokens[5], "w") : stdout;
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[5]);
		return;
	}

	status = rte_swx_ctl_pipeline_table_fprintf(file, ctl, table_name);
	if (status)
		snprintf(out, out_size, MSG_ARG_INVALID, "table_name");

	if (file)
		fclose(file);
}

static const char cmd_pipeline_selector_group_add_help[] =
"pipeline <pipeline_name> selector <selector_name> group add\n";

static void
cmd_pipeline_selector_group_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *selector_name;
	uint32_t group_id;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = rte_swx_ctl_pipeline_selector_group_add(ctl,
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
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *selector_name;
	uint32_t group_id;
	int status;

	if (n_tokens != 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = rte_swx_ctl_pipeline_selector_group_delete(ctl,
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
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *selector_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = pipeline_selector_group_members_add(ctl,
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
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *selector_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = pipeline_selector_group_members_delete(ctl,
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
"pipeline <pipeline_name> selector <selector_name> show [filename]\n";

static void
cmd_pipeline_selector_show(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *selector_name;
	FILE *file = NULL;
	int status;

	if (n_tokens != 5 && n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	selector_name = tokens[3];

	file = (n_tokens == 6) ? fopen(tokens[5], "w") : stdout;
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", tokens[5]);
		return;
	}

	status = rte_swx_ctl_pipeline_selector_fprintf(file, ctl, selector_name);
	if (status)
		snprintf(out, out_size, MSG_ARG_INVALID, "selector_name");

	if (file)
		fclose(file);
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
			     void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name, *learner_name, *file_name;
	FILE *file = NULL;
	uint32_t file_line_number = 0;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
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

	status = pipeline_learner_default_entry_add(ctl,
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
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	status = rte_swx_ctl_pipeline_commit(ctl, 1);
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
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline *ctl;
	char *pipeline_name;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	rte_swx_ctl_pipeline_abort(ctl);
}

static const char cmd_pipeline_regrd_help[] =
"pipeline <pipeline_name> regrd <register_array_name>\n"
	"index <index>\n"
	" | table <table_name> match <field0> ...\n";

static void
cmd_pipeline_regrd(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_pipeline *p;
	struct rte_swx_ctl_pipeline *ctl;
	const char *pipeline_name, *name;
	uint64_t value;
	int status;

	if (n_tokens < 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!p || !ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "regrd")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "regrd");
		return;
	}

	name = tokens[3];

	/* index. */
	if (!strcmp(tokens[4], "index")) {
		uint32_t idx = 0;

		if (n_tokens != 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (parser_read_uint32(&idx, tokens[5])) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index");
			return;
		}

		status = rte_swx_ctl_pipeline_regarray_read(p, name, idx, &value);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
			return;
		}

		snprintf(out, out_size, "0x%" PRIx64 "\n", value);
		return;
	}

	/* table. */
	if (!strcmp(tokens[4], "table")) {
		struct rte_swx_table_entry *entry;
		char *table_name;

		if (n_tokens < 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		table_name = tokens[5];

		if (strcmp(tokens[6], "match")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
			return;
		}

		entry = parse_table_entry(ctl, table_name, &tokens[6], n_tokens - 6);
		if (!entry) {
			snprintf(out, out_size, "Invalid match tokens.\n");
			return;
		}

		status = rte_swx_ctl_pipeline_regarray_read_with_key(p,
								     name,
								     table_name,
								     entry->key,
								     &value);
		table_entry_free(entry);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
			return;
		}

		snprintf(out, out_size, "0x%" PRIx64 "\n", value);
		return;
	}

	/* anything else. */
	snprintf(out, out_size, "Invalid token %s\n.", tokens[4]);
	return;
}

static const char cmd_pipeline_regwr_help[] =
"pipeline <pipeline_name> regwr <register_array_name> value <value>\n"
	"index <index>\n"
	" | table <table_name> match <field0> ...\n";

static void
cmd_pipeline_regwr(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_pipeline *p;
	struct rte_swx_ctl_pipeline *ctl;
	const char *pipeline_name, *name;
	uint64_t value = 0;
	int status;

	if (n_tokens < 7) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!p || !ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "regwr")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "regwr");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "value")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "value");
		return;
	}

	if (parser_read_uint64(&value, tokens[5])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "value");
		return;
	}

	/* index. */
	if (!strcmp(tokens[6], "index")) {
		uint32_t idx = 0;

		if (n_tokens != 8) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (parser_read_uint32(&idx, tokens[7])) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index");
			return;
		}

		status = rte_swx_ctl_pipeline_regarray_write(p, name, idx, value);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
			return;
		}

		snprintf(out, out_size, "0x%" PRIx64 "\n", value);
		return;
	}

	/* table. */
	if (!strcmp(tokens[6], "table")) {
		struct rte_swx_table_entry *entry;
		char *table_name;

		if (n_tokens < 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		table_name = tokens[7];

		if (strcmp(tokens[8], "match")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
			return;
		}

		entry = parse_table_entry(ctl, table_name, &tokens[8], n_tokens - 8);
		if (!entry) {
			snprintf(out, out_size, "Invalid match tokens.\n");
			return;
		}

		status = rte_swx_ctl_pipeline_regarray_write_with_key(p,
								      name,
								      table_name,
								      entry->key,
								      value);
		table_entry_free(entry);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
			return;
		}

		return;
	}

	/* anything else. */
	snprintf(out, out_size, "Invalid token %s\n.", tokens[6]);
	return;
}

static const char cmd_pipeline_meter_profile_add_help[] =
"pipeline <pipeline_name> meter profile <profile_name> add "
	"cir <cir> pir <pir> cbs <cbs> pbs <pbs>\n";

static void
cmd_pipeline_meter_profile_add(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_meter_trtcm_params params;
	struct rte_swx_pipeline *p;
	const char *profile_name;
	int status;

	if (n_tokens != 14) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = rte_swx_pipeline_find(tokens[1]);
	if (!p) {
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

	status = rte_swx_ctl_meter_profile_add(p, profile_name, &params);
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
	void *obj __rte_unused)
{
	struct rte_swx_pipeline *p;
	const char *profile_name;
	int status;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = rte_swx_pipeline_find(tokens[1]);
	if (!p) {
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

	status = rte_swx_ctl_meter_profile_delete(p, profile_name);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

static const char cmd_pipeline_meter_reset_help[] =
"pipeline <pipeline_name> meter <meter_array_name> reset\n"
	"index from <index0> to <index1>\n"
	" | table <table_name> match <field0> ...\n";

static void
cmd_pipeline_meter_reset(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_pipeline *p;
	struct rte_swx_ctl_pipeline *ctl;
	const char *pipeline_name, *name;

	if (n_tokens < 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!p || !ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "reset")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "reset");
		return;
	}

	/* index. */
	if (!strcmp(tokens[5], "index")) {
		uint32_t idx0 = 0, idx1 = 0;

		if (n_tokens != 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (strcmp(tokens[6], "from")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
			return;
		}

		if (parser_read_uint32(&idx0, tokens[7])) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index0");
			return;
		}

		if (strcmp(tokens[8], "to")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
			return;
		}

		if (parser_read_uint32(&idx1, tokens[9]) || (idx1 < idx0)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index1");
			return;
		}

		for ( ; idx0 <= idx1; idx0++) {
			int status;

			status = rte_swx_ctl_meter_reset(p, name, idx0);
			if (status) {
				snprintf(out, out_size, "Command failed for index %u.\n", idx0);
				return;
			}
		}

		return;
	}

	/* table. */
	if (!strcmp(tokens[5], "table")) {
		struct rte_swx_table_entry *entry;
		char *table_name;
		int status;

		if (n_tokens < 9) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		table_name = tokens[6];

		if (strcmp(tokens[7], "match")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
			return;
		}

		entry = parse_table_entry(ctl, table_name, &tokens[7], n_tokens - 7);
		if (!entry) {
			snprintf(out, out_size, "Invalid match tokens.\n");
			return;
		}

		status = rte_swx_ctl_meter_reset_with_key(p, name, table_name, entry->key);
		table_entry_free(entry);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
			return;
		}

		return;
	}

	/* anything else. */
	snprintf(out, out_size, "Invalid token %s\n.", tokens[5]);
	return;
}

static const char cmd_pipeline_meter_set_help[] =
"pipeline <pipeline_name> meter <meter_array_name> set profile <profile_name>\n"
	"index from <index0> to <index1>\n"
	" | table <table_name> match <field0> ...\n";

static void
cmd_pipeline_meter_set(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_pipeline *p;
	struct rte_swx_ctl_pipeline *ctl;
	const char *pipeline_name, *name, *profile_name;

	if (n_tokens < 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!p || !ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "set")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "set");
		return;
	}

	if (strcmp(tokens[5], "profile")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "profile");
		return;
	}

	profile_name = tokens[6];

	/* index. */
	if (!strcmp(tokens[7], "index")) {
		uint32_t idx0 = 0, idx1 = 0;

		if (n_tokens != 12) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (strcmp(tokens[8], "from")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
			return;
		}

		if (parser_read_uint32(&idx0, tokens[9])) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index0");
			return;
		}

		if (strcmp(tokens[10], "to")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
			return;
		}

		if (parser_read_uint32(&idx1, tokens[11]) || (idx1 < idx0)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index1");
			return;
		}

		for ( ; idx0 <= idx1; idx0++) {
			int status;

			status = rte_swx_ctl_meter_set(p, name, idx0, profile_name);
			if (status) {
				snprintf(out, out_size, "Command failed for index %u.\n", idx0);
				return;
			}
		}

		return;
	}

	/* table. */
	if (!strcmp(tokens[7], "table")) {
		struct rte_swx_table_entry *entry;
		char *table_name;
		int status;

		if (n_tokens < 11) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		table_name = tokens[8];

		if (strcmp(tokens[9], "match")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
			return;
		}

		entry = parse_table_entry(ctl, table_name, &tokens[9], n_tokens - 9);
		if (!entry) {
			snprintf(out, out_size, "Invalid match tokens.\n");
			return;
		}

		status = rte_swx_ctl_meter_set_with_key(p,
							name,
							table_name,
							entry->key,
							profile_name);
		table_entry_free(entry);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
			return;
		}

		return;
	}

	/* anything else. */
	snprintf(out, out_size, "Invalid token %s\n.", tokens[7]);
	return;
}

static const char cmd_pipeline_meter_stats_help[] =
"pipeline <pipeline_name> meter <meter_array_name> stats\n"
	"index from <index0> to <index1>\n"
	" | table <table_name> match <field0> ...\n";

static void
cmd_pipeline_meter_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_ctl_meter_stats stats;
	struct rte_swx_pipeline *p;
	struct rte_swx_ctl_pipeline *ctl;
	const char *pipeline_name, *name;

	if (n_tokens < 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	ctl = rte_swx_ctl_pipeline_find(pipeline_name);
	if (!p || !ctl) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "meter")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "meter");
		return;
	}

	name = tokens[3];

	if (strcmp(tokens[4], "stats")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	/* index. */
	if (!strcmp(tokens[5], "index")) {
		uint32_t idx0 = 0, idx1 = 0;

		if (n_tokens != 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		if (strcmp(tokens[6], "from")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "from");
			return;
		}

		if (parser_read_uint32(&idx0, tokens[7])) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index0");
			return;
		}

		if (strcmp(tokens[8], "to")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "to");
			return;
		}

		if (parser_read_uint32(&idx1, tokens[9]) || (idx1 < idx0)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "index1");
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

			status = rte_swx_ctl_meter_stats_read(p, name, idx0, &stats);
			if (status) {
				snprintf(out, out_size, "Meter stats error at index %u.\n", idx0);
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

		return;
	}

	/* table. */
	if (!strcmp(tokens[5], "table")) {
		struct rte_swx_table_entry *entry;
		char *table_name;
		int status;

		if (n_tokens < 9) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
			return;
		}

		table_name = tokens[6];

		if (strcmp(tokens[7], "match")) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "match");
			return;
		}

		entry = parse_table_entry(ctl, table_name, &tokens[7], n_tokens - 7);
		if (!entry) {
			snprintf(out, out_size, "Invalid match tokens.\n");
			return;
		}

		status = rte_swx_ctl_meter_stats_read_with_key(p,
							name,
							table_name,
							entry->key,
							&stats);
		table_entry_free(entry);
		if (status) {
			snprintf(out, out_size, "Command failed.\n");
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

		/* Table row. */
		snprintf(out, out_size, "| %7d | %16" PRIx64 " | %16" PRIx64 " | %16" PRIx64
			 " | %16" PRIx64 " | %16" PRIx64 " | %16" PRIx64 " |\n",
			 0,
			 stats.n_pkts[RTE_COLOR_GREEN],
			 stats.n_pkts[RTE_COLOR_YELLOW],
			 stats.n_pkts[RTE_COLOR_RED],
			 stats.n_bytes[RTE_COLOR_GREEN],
			 stats.n_bytes[RTE_COLOR_YELLOW],
			 stats.n_bytes[RTE_COLOR_RED]);
		out_size -= strlen(out);
		out += strlen(out);

		return;
	}

	/* anything else. */
	snprintf(out, out_size, "Invalid token %s\n.", tokens[5]);
	return;
}

static const char cmd_pipeline_rss_help[] =
"pipeline <pipeline_name> rss <rss_obj_name> key <key_byte0> ...\n";

static void
cmd_pipeline_rss(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	uint8_t rss_key[CMD_MAX_TOKENS];
	struct rte_swx_pipeline *p;
	const char *rss_obj_name;
	uint32_t rss_key_size, i;
	int status;

	if (n_tokens < 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = rte_swx_pipeline_find(tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "rss")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "rss");
		return;
	}

	rss_obj_name = tokens[3];

	if (strcmp(tokens[4], "key")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
		return;
	}

	tokens += 5;
	n_tokens -= 5;
	rss_key_size = n_tokens;

	for (i = 0; i < rss_key_size; i++) {
		uint32_t key_byte;

		if (parser_read_uint32(&key_byte, tokens[i]) || (key_byte >= UINT8_MAX)) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key byte");
			return;
		}

		rss_key[i] = (uint8_t)key_byte;
	}

	status = rte_swx_ctl_pipeline_rss_key_write(p, rss_obj_name, rss_key_size, rss_key);
	if (status) {
		snprintf(out, out_size, "Command failed.\n");
		return;
	}
}

static const char cmd_pipeline_stats_help[] =
"pipeline <pipeline_name> stats\n";

static void
cmd_pipeline_stats(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_ctl_pipeline_info info;
	struct rte_swx_pipeline *p;
	uint32_t i;
	int status;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	p = rte_swx_pipeline_find(tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "stats")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
		return;
	}

	status = rte_swx_ctl_pipeline_info_get(p, &info);
	if (status) {
		snprintf(out, out_size, "Pipeline info get error.");
		return;
	}

	snprintf(out, out_size, "Input ports:\n");
	out_size -= strlen(out);
	out += strlen(out);

	for (i = 0; i < info.n_ports_in; i++) {
		struct rte_swx_port_in_stats stats;

		rte_swx_ctl_pipeline_port_in_stats_read(p, i, &stats);

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

		rte_swx_ctl_pipeline_port_out_stats_read(p, i, &stats);

		if (i != info.n_ports_out - 1)
			snprintf(out, out_size, "\tPort %u:", i);
		else
			snprintf(out, out_size, "\tDROP:");

		out_size -= strlen(out);
		out += strlen(out);

		snprintf(out,
			out_size,
			" packets %" PRIu64
			" bytes %" PRIu64
			" packets dropped %" PRIu64
			" bytes dropped %" PRIu64
			" clone %" PRIu64
			" clonerr %" PRIu64 "\n",
			stats.n_pkts,
			stats.n_bytes,
			stats.n_pkts_drop,
			stats.n_bytes_drop,
			stats.n_pkts_clone,
			stats.n_pkts_clone_err);

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

		status = rte_swx_ctl_table_info_get(p, i, &table_info);
		if (status) {
			snprintf(out, out_size, "Table info get error.");
			return;
		}

		status = rte_swx_ctl_pipeline_table_stats_read(p, table_info.name, &stats);
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

			status = rte_swx_ctl_action_info_get(p, j, &action_info);
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

		status = rte_swx_ctl_learner_info_get(p, i, &learner_info);
		if (status) {
			snprintf(out, out_size, "Learner table info get error.");
			return;
		}

		status = rte_swx_ctl_pipeline_learner_stats_read(p, learner_info.name, &stats);
		if (status) {
			snprintf(out, out_size, "Learner table stats read error.");
			return;
		}

		snprintf(out, out_size, "\tLearner table %s:\n"
			"\t\tHit (packets): %" PRIu64 "\n"
			"\t\tMiss (packets): %" PRIu64 "\n"
			"\t\tLearn OK (packets): %" PRIu64 "\n"
			"\t\tLearn error (packets): %" PRIu64 "\n"
			"\t\tRearm (packets): %" PRIu64 "\n"
			"\t\tForget (packets): %" PRIu64 "\n",
			learner_info.name,
			stats.n_pkts_hit,
			stats.n_pkts_miss,
			stats.n_pkts_learn_ok,
			stats.n_pkts_learn_err,
			stats.n_pkts_rearm,
			stats.n_pkts_forget);
		out_size -= strlen(out);
		out += strlen(out);

		for (j = 0; j < info.n_actions; j++) {
			struct rte_swx_ctl_action_info action_info;

			status = rte_swx_ctl_action_info_get(p, j, &action_info);
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

static const char cmd_pipeline_mirror_session_help[] =
"pipeline <pipeline_name> mirror session <session_id> port <port_id> clone fast | slow "
"truncate <truncation_length>\n";

static void
cmd_pipeline_mirror_session(char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size,
	void *obj __rte_unused)
{
	struct rte_swx_pipeline_mirroring_session_params params;
	struct rte_swx_pipeline *p;
	uint32_t session_id = 0;
	int status;

	if (n_tokens != 11) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[0], "pipeline")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pipeline");
		return;
	}

	p = rte_swx_pipeline_find(tokens[1]);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "mirror")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mirror");
		return;
	}

	if (strcmp(tokens[3], "session")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "session");
		return;
	}

	if (parser_read_uint32(&session_id, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "session_id");
		return;
	}

	if (strcmp(tokens[5], "port")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
		return;
	}

	if (parser_read_uint32(&params.port_id, tokens[6])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[7], "clone")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "clone");
		return;
	}

	if (!strcmp(tokens[8], "fast"))
		params.fast_clone = 1;
	else if (!strcmp(tokens[8], "slow"))
		params.fast_clone = 0;
	else {
		snprintf(out, out_size, MSG_ARG_INVALID, "clone");
		return;
	}

	if (strcmp(tokens[9], "truncate")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "truncate");
		return;
	}

	if (parser_read_uint32(&params.truncation_length, tokens[10])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "truncation_length");
		return;
	}

	status = rte_swx_ctl_pipeline_mirroring_session_set(p, session_id, &params);
	if (status) {
		snprintf(out, out_size, "Command failed!\n");
		return;
	}
}

static const char cmd_ipsec_create_help[] =
"ipsec <ipsec_instance_name> create "
"in <ring_in_name> out <ring_out_name> "
"cryptodev <crypto_dev_name> cryptoq <crypto_dev_queue_pair_id> "
"bsz <ring_rd_bsz> <ring_wr_bsz> <crypto_wr_bsz> <crypto_rd_bsz> "
"samax <n_sa_max> "
"numa <numa_node>\n";

static void
cmd_ipsec_create(char **tokens,
		 uint32_t n_tokens,
		 char *out,
		 size_t out_size,
		 void *obj __rte_unused)
{
	struct rte_swx_ipsec_params p;
	struct rte_swx_ipsec *ipsec;
	char *ipsec_instance_name;
	uint32_t numa_node;
	int status;

	if (n_tokens != 20) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ipsec_instance_name = tokens[1];

	if (strcmp(tokens[2], "create")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "create");
		return;
	}

	if (strcmp(tokens[3], "in")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "in");
		return;
	}

	p.ring_in_name = tokens[4];

	if (strcmp(tokens[5], "out")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "out");
		return;
	}

	p.ring_out_name = tokens[6];

	if (strcmp(tokens[7], "cryptodev")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cryptodev");
		return;
	}

	p.crypto_dev_name = tokens[8];

	if (strcmp(tokens[9], "cryptoq")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cryptoq");
		return;
	}

	if (parser_read_uint32(&p.crypto_dev_queue_pair_id, tokens[10])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "crypto_dev_queue_pair_id");
		return;
	}

	if (strcmp(tokens[11], "bsz")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "bsz");
		return;
	}

	if (parser_read_uint32(&p.bsz.ring_rd, tokens[12])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ring_rd_bsz");
		return;
	}

	if (parser_read_uint32(&p.bsz.ring_wr, tokens[13])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ring_wr_bsz");
		return;
	}

	if (parser_read_uint32(&p.bsz.crypto_wr, tokens[14])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "crypto_wr_bsz");
		return;
	}

	if (parser_read_uint32(&p.bsz.crypto_rd, tokens[15])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "crypto_rd_bsz");
		return;
	}

	if (strcmp(tokens[16], "samax")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "samax");
		return;
	}

	if (parser_read_uint32(&p.n_sa_max, tokens[17])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "n_sa_max");
		return;
	}

	if (strcmp(tokens[18], "numa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "numa");
		return;
	}

	if (parser_read_uint32(&numa_node, tokens[19])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "numa_node");
		return;
	}

	status = rte_swx_ipsec_create(&ipsec,
				      ipsec_instance_name,
				      &p,
				      (int)numa_node);
	if (status)
		snprintf(out, out_size, "IPsec instance creation failed (%d).\n", status);
}

static const char cmd_ipsec_sa_add_help[] =
"ipsec <ipsec_instance_name> sa add <file_name>\n";

static void
cmd_ipsec_sa_add(char **tokens,
		 uint32_t n_tokens,
		 char *out,
		 size_t out_size,
		 void *obj __rte_unused)
{
	struct rte_swx_ipsec *ipsec;
	char *ipsec_instance_name, *file_name, *line = NULL;
	FILE *file = NULL;
	uint32_t line_id = 0;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ipsec_instance_name = tokens[1];
	ipsec = rte_swx_ipsec_find(ipsec_instance_name);
	if (!ipsec) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ipsec_instance_name");
		goto free;
	}

	if (strcmp(tokens[2], "sa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "sa");
		goto free;
	}

	if (strcmp(tokens[3], "add")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "add");
		goto free;
	}

	file_name = tokens[4];
	file = fopen(file_name, "r");
	if (!file) {
		snprintf(out, out_size, "Cannot open file %s.\n", file_name);
		goto free;
	}

	/* Buffer allocation. */
	line = malloc(MAX_LINE_SIZE);
	if (!line) {
		snprintf(out, out_size, MSG_OUT_OF_MEMORY);
		goto free;
	}

	/* File read. */
	for (line_id = 1; ; line_id++) {
		struct rte_swx_ipsec_sa_params *sa;
		const char *err_msg;
		uint32_t sa_id = 0;
		int is_blank_or_comment, status = 0;

		if (fgets(line, MAX_LINE_SIZE, file) == NULL)
			break;

		/* Read SA from file. */
		sa = rte_swx_ipsec_sa_read(ipsec, line, &is_blank_or_comment, &err_msg);
		if (!sa) {
			if (is_blank_or_comment)
				continue;

			snprintf(out, out_size, "Invalid SA in file \"%s\" at line %u: \"%s\"\n",
				file_name, line_id, err_msg);
			goto free;
		}

		snprintf(out, out_size, "%s", line);
		out_size -= strlen(out);
		out += strlen(out);

		/* Add the SA to the IPsec instance. Free the SA. */
		status = rte_swx_ipsec_sa_add(ipsec, sa, &sa_id);
		if (status)
			snprintf(out, out_size, "\t: Error (%d)\n", status);
		else
			snprintf(out, out_size, "\t: OK (SA ID = %u)\n", sa_id);
		out_size -= strlen(out);
		out += strlen(out);

		free(sa);
		if (status)
			goto free;
	}

free:
	if (file)
		fclose(file);
	free(line);
}

static const char cmd_ipsec_sa_delete_help[] =
"ipsec <ipsec_instance_name> sa delete <sa_id>\n";

static void
cmd_ipsec_sa_delete(char **tokens,
		    uint32_t n_tokens,
		    char *out,
		    size_t out_size,
		    void *obj __rte_unused)
{
	struct rte_swx_ipsec *ipsec;
	char *ipsec_instance_name;
	uint32_t sa_id;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ipsec_instance_name = tokens[1];
	ipsec = rte_swx_ipsec_find(ipsec_instance_name);
	if (!ipsec) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ipsec_instance_name");
		return;
	}

	if (strcmp(tokens[2], "sa")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "sa");
		return;
	}

	if (strcmp(tokens[3], "delete")) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "delete");
		return;
	}

	if (parser_read_uint32(&sa_id, tokens[4])) {
		snprintf(out, out_size, MSG_ARG_INVALID, "sa_id");
		return;
	}

	rte_swx_ipsec_sa_delete(ipsec, sa_id);
}

static const char cmd_pipeline_enable_help[] =
"pipeline <pipeline_name> enable thread <thread_id>\n";

static void
cmd_pipeline_enable(char **tokens,
		    uint32_t n_tokens,
		    char *out,
		    size_t out_size,
		    void *obj __rte_unused)
{
	char *pipeline_name;
	struct rte_swx_pipeline *p;
	uint32_t thread_id;
	int status;

	if (n_tokens != 5) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	if (strcmp(tokens[3], "thread") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "thread");
		return;
	}

	if (parser_read_uint32(&thread_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	status = pipeline_enable(p, thread_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, "pipeline enable");
		return;
	}
}

static const char cmd_pipeline_disable_help[] =
"pipeline <pipeline_name> disable\n";

static void
cmd_pipeline_disable(char **tokens,
		     uint32_t n_tokens,
		     char *out,
		     size_t out_size,
		     void *obj __rte_unused)
{
	struct rte_swx_pipeline *p;
	char *pipeline_name;

	if (n_tokens != 3) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	pipeline_name = tokens[1];
	p = rte_swx_pipeline_find(pipeline_name);
	if (!p) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pipeline_name");
		return;
	}

	if (strcmp(tokens[2], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	pipeline_disable(p);
}

static const char cmd_block_enable_help[] =
"block type <block_type> instance <block_name> enable thread <thread_id>\n";

static void
cmd_block_enable(char **tokens,
		 uint32_t n_tokens,
		 char *out,
		 size_t out_size,
		 void *obj __rte_unused)
{
	char *block_type, *block_name;
	block_run_f block_func = NULL;
	void *block = NULL;
	uint32_t thread_id;
	int status;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "type") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "type");
		return;
	}

	block_type = tokens[2];

	if (strcmp(tokens[3], "instance") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "instance");
		return;
	}

	block_name = tokens[4];

	if (strcmp(tokens[5], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	if (strcmp(tokens[6], "thread") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "thread");
		return;
	}

	if (parser_read_uint32(&thread_id, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "thread_id");
		return;
	}

	if (!strcmp(block_type, "ipsec")) {
		struct rte_swx_ipsec *ipsec;

		ipsec = rte_swx_ipsec_find(block_name);
		if (!ipsec) {
			snprintf(out, out_size, MSG_ARG_INVALID, "block_name");
			return;
		}

		block_func = (block_run_f)rte_swx_ipsec_run;
		block = (void *)ipsec;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, "block_type");
		return;
	}

	status = block_enable(block_func, block, thread_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, "block enable");
		return;
	}
}

static const char cmd_block_disable_help[] =
"block type <block_type> instance <block_name> disable\n";

static void
cmd_block_disable(char **tokens,
		  uint32_t n_tokens,
		  char *out,
		  size_t out_size,
		  void *obj __rte_unused)
{
	char *block_type, *block_name;
	void *block = NULL;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	if (strcmp(tokens[1], "type") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "type");
		return;
	}

	block_type = tokens[2];

	if (strcmp(tokens[3], "instance") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "instance");
		return;
	}

	block_name = tokens[4];

	if (strcmp(tokens[5], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	if (!strcmp(block_type, "ipsec")) {
		struct rte_swx_ipsec *ipsec;

		ipsec = rte_swx_ipsec_find(block_name);
		if (!ipsec) {
			snprintf(out, out_size, MSG_ARG_INVALID, "block_name");
			return;
		}

		block = (void *)ipsec;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, "block_type");
		return;
	}

	block_disable(block);
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
			"\tethdev\n"
			"\tethdev show\n"
			"\tring\n"
			"\tcryptodev\n"
			"\tpipeline codegen\n"
			"\tpipeline libbuild\n"
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
			"\tpipeline rss\n"
			"\tpipeline stats\n"
			"\tpipeline mirror session\n"
			"\tpipeline enable\n"
			"\tpipeline disable\n\n"
			"\tipsec create\n"
			"\tipsec sa add\n"
			"\tipsec sa delete\n"
			"\tblock enable\n"
			"\tblock disable\n"
			);
		return;
	}

	if (strcmp(tokens[0], "mempool") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_mempool_help);
		return;
	}

	if (!strcmp(tokens[0], "ethdev")) {
		if (n_tokens == 1) {
			snprintf(out, out_size, "\n%s\n", cmd_ethdev_help);
			return;
		}

		if (n_tokens == 2 && !strcmp(tokens[1], "show")) {
			snprintf(out, out_size, "\n%s\n", cmd_ethdev_show_help);
			return;
		}
	}

	if (strcmp(tokens[0], "ring") == 0) {
		snprintf(out, out_size, "\n%s\n", cmd_ring_help);
		return;
	}

	if (!strcmp(tokens[0], "cryptodev")) {
		snprintf(out, out_size, "\n%s\n", cmd_cryptodev_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "codegen") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_codegen_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "libbuild") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_libbuild_help);
		return;
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

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 2) && !strcmp(tokens[1], "rss")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_rss_help);
		return;
	}

	if ((strcmp(tokens[0], "pipeline") == 0) &&
		(n_tokens == 2) && (strcmp(tokens[1], "stats") == 0)) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_stats_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 3) && !strcmp(tokens[1], "mirror")
		&& !strcmp(tokens[2], "session")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_mirror_session_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 2) && !strcmp(tokens[1], "enable")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_enable_help);
		return;
	}

	if (!strcmp(tokens[0], "pipeline") &&
		(n_tokens == 2) && !strcmp(tokens[1], "disable")) {
		snprintf(out, out_size, "\n%s\n", cmd_pipeline_disable_help);
		return;
	}

	if (!strcmp(tokens[0], "ipsec") &&
		(n_tokens == 2) && !strcmp(tokens[1], "create")) {
		snprintf(out, out_size, "\n%s\n", cmd_ipsec_create_help);
		return;
	}

	if (!strcmp(tokens[0], "ipsec") &&
		(n_tokens == 3) && !strcmp(tokens[1], "sa")
		&& !strcmp(tokens[2], "add")) {
		snprintf(out, out_size, "\n%s\n", cmd_ipsec_sa_add_help);
		return;
	}

	if (!strcmp(tokens[0], "ipsec") &&
		(n_tokens == 3) && !strcmp(tokens[1], "sa")
		&& !strcmp(tokens[2], "delete")) {
		snprintf(out, out_size, "\n%s\n", cmd_ipsec_sa_delete_help);
		return;
	}

	if (!strcmp(tokens[0], "block") &&
		(n_tokens == 2) && !strcmp(tokens[1], "enable")) {
		snprintf(out, out_size, "\n%s\n", cmd_block_enable_help);
		return;
	}

	if (!strcmp(tokens[0], "block") &&
		(n_tokens == 2) && !strcmp(tokens[1], "disable")) {
		snprintf(out, out_size, "\n%s\n", cmd_block_disable_help);
		return;
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

	if (strcmp(tokens[0], "ethdev") == 0) {
		if ((n_tokens >= 2) && (strcmp(tokens[1], "show") == 0)) {
			cmd_ethdev_show(tokens, n_tokens, out, out_size, obj);
			return;
		}

		cmd_ethdev(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "ring") == 0) {
		cmd_ring(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (!strcmp(tokens[0], "cryptodev")) {
		cmd_cryptodev(tokens, n_tokens, out, out_size, obj);
		return;
	}

	if (strcmp(tokens[0], "pipeline") == 0) {
		if ((n_tokens >= 3) &&
			(strcmp(tokens[1], "codegen") == 0)) {
			cmd_pipeline_codegen(tokens, n_tokens, out, out_size,
				obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[1], "libbuild") == 0)) {
			cmd_pipeline_libbuild(tokens, n_tokens, out, out_size,
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

		if (n_tokens >= 9 && !strcmp(tokens[2], "meter") && !strcmp(tokens[4], "reset")) {
			cmd_pipeline_meter_reset(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 9 && !strcmp(tokens[2], "meter") && !strcmp(tokens[4], "set")) {
			cmd_pipeline_meter_set(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 9 && !strcmp(tokens[2], "meter") && !strcmp(tokens[4], "stats")) {
			cmd_pipeline_meter_stats(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "rss")) {
			cmd_pipeline_rss(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if ((n_tokens >= 3) &&
			(strcmp(tokens[2], "stats") == 0)) {
			cmd_pipeline_stats(tokens, n_tokens, out, out_size,
				obj);
			return;
		}

		if ((n_tokens >= 4) &&
			(strcmp(tokens[2], "mirror") == 0) &&
			(strcmp(tokens[3], "session") == 0)) {
			cmd_pipeline_mirror_session(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "enable")) {
			cmd_pipeline_enable(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 3 && !strcmp(tokens[2], "disable")) {
			cmd_pipeline_disable(tokens, n_tokens, out, out_size, obj);
			return;
		}
	}

	if (!strcmp(tokens[0], "ipsec")) {
		if (n_tokens >= 3 && !strcmp(tokens[2], "create")) {
			cmd_ipsec_create(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 4 && !strcmp(tokens[2], "sa") && !strcmp(tokens[3], "add")) {
			cmd_ipsec_sa_add(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 4 && !strcmp(tokens[2], "sa") && !strcmp(tokens[3], "delete")) {
			cmd_ipsec_sa_delete(tokens, n_tokens, out, out_size, obj);
			return;
		}
	}

	if (!strcmp(tokens[0], "block")) {
		if (n_tokens >= 6 && !strcmp(tokens[5], "enable")) {
			cmd_block_enable(tokens, n_tokens, out, out_size, obj);
			return;
		}

		if (n_tokens >= 6 && !strcmp(tokens[5], "disable")) {
			cmd_block_disable(tokens, n_tokens, out, out_size, obj);
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
