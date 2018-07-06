/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rte_eth_softnic_internals.h"
#include "parser.h"

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

/**
 * mempool <mempool_name>
 *  buffer <buffer_size>
 *  pool <pool_size>
 *  cache <cache_size>
 */
static void
cmd_mempool(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_mempool_params p;
	char *name;
	struct softnic_mempool *mempool;

	if (n_tokens != 8) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "buffer") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buffer");
		return;
	}

	if (softnic_parser_read_uint32(&p.buffer_size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "buffer_size");
		return;
	}

	if (strcmp(tokens[4], "pool") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pool");
		return;
	}

	if (softnic_parser_read_uint32(&p.pool_size, tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "pool_size");
		return;
	}

	if (strcmp(tokens[6], "cache") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "cache");
		return;
	}

	if (softnic_parser_read_uint32(&p.cache_size, tokens[7]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "cache_size");
		return;
	}

	mempool = softnic_mempool_create(softnic, name, &p);
	if (mempool == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * link <link_name>
 *    dev <device_name> | port <port_id>
 */
static void
cmd_link(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_link_params p;
	struct softnic_link *link;
	char *name;

	memset(&p, 0, sizeof(p));

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}
	name = tokens[1];

	if (strcmp(tokens[2], "dev") == 0) {
		p.dev_name = tokens[3];
	} else if (strcmp(tokens[2], "port") == 0) {
		p.dev_name = NULL;

		if (softnic_parser_read_uint16(&p.port_id, tokens[3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}
	} else {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "dev or port");
		return;
	}

	link = softnic_link_create(softnic, name, &p);
	if (link == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * swq <swq_name>
 *  size <size>
 */
static void
cmd_swq(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_swq_params p;
	char *name;
	struct softnic_swq *swq;

	if (n_tokens != 4) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "size") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
		return;
	}

	if (softnic_parser_read_uint32(&p.size, tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "size");
		return;
	}

	swq = softnic_swq_create(softnic, name, &p);
	if (swq == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * tap <tap_name>
 */
static void
cmd_tap(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	char *name;
	struct softnic_tap *tap;

	if (n_tokens != 2) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	tap = softnic_tap_create(softnic, name);
	if (tap == NULL) {
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
cmd_port_in_action_profile(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_port_in_action_profile_params p;
	struct softnic_port_in_action_profile *ap;
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

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "filter") == 0)) {
		uint32_t size;

		if (n_tokens < t0 + 10) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "port in action profile filter");
			return;
		}

		if (strcmp(tokens[t0 + 1], "match") == 0) {
			p.fltr.filter_on_match = 1;
		} else if (strcmp(tokens[t0 + 1], "mismatch") == 0) {
			p.fltr.filter_on_match = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_INVALID, "match or mismatch");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.fltr.key_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		size = RTE_PORT_IN_ACTION_FLTR_KEY_SIZE;
		if ((softnic_parse_hex_string(tokens[t0 + 5],
			p.fltr.key_mask, &size) != 0) ||
			size != RTE_PORT_IN_ACTION_FLTR_KEY_SIZE) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 6], "key") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
			return;
		}

		size = RTE_PORT_IN_ACTION_FLTR_KEY_SIZE;
		if ((softnic_parse_hex_string(tokens[t0 + 7],
			p.fltr.key, &size) != 0) ||
			size != RTE_PORT_IN_ACTION_FLTR_KEY_SIZE) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_value");
			return;
		}

		if (strcmp(tokens[t0 + 8], "port") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (softnic_parser_read_uint32(&p.fltr.port_id,
			tokens[t0 + 9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
			return;
		}

		p.action_mask |= 1LLU << RTE_PORT_IN_ACTION_FLTR;
		t0 += 10;
	} /* filter */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "balance") == 0)) {
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

		if (softnic_parser_read_uint32(&p.lb.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		p.lb.key_size = RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX;
		if (softnic_parse_hex_string(tokens[t0 + 4],
			p.lb.key_mask, &p.lb.key_size) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 5], "port") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "port");
			return;
		}

		for (i = 0; i < 16; i++)
			if (softnic_parser_read_uint32(&p.lb.port_id[i],
			tokens[t0 + 6 + i]) != 0) {
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

	ap = softnic_port_in_action_profile_create(softnic, name, &p);
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
 *  [balance offset <key_offset> mask <key_mask> outoffset <out_offset>]
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
cmd_table_action_profile(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_table_action_profile_params p;
	struct softnic_table_action_profile *ap;
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

	if (strcmp(tokens[4], "ipv4") == 0) {
		p.common.ip_version = 1;
	} else if (strcmp(tokens[4], "ipv6") == 0) {
		p.common.ip_version = 0;
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, "ipv4 or ipv6");
		return;
	}

	if (strcmp(tokens[5], "offset") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
		return;
	}

	if (softnic_parser_read_uint32(&p.common.ip_offset,
		tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "ip_offset");
		return;
	}

	if (strcmp(tokens[7], "fwd") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "fwd");
		return;
	}

	p.action_mask |= 1LLU << RTE_TABLE_ACTION_FWD;

	t0 = 8;
	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "balance") == 0)) {
		if (n_tokens < t0 + 7) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "table action profile balance");
			return;
		}

		if (strcmp(tokens[t0 + 1], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.lb.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		p.lb.key_size = RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX;
		if (softnic_parse_hex_string(tokens[t0 + 4],
			p.lb.key_mask, &p.lb.key_size) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}

		if (strcmp(tokens[t0 + 5], "outoffset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "outoffset");
			return;
		}

		if (softnic_parser_read_uint32(&p.lb.out_offset,
			tokens[t0 + 6]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "out_offset");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_LB;
		t0 += 7;
	} /* balance */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "meter") == 0)) {
		if (n_tokens < t0 + 6) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile meter");
			return;
		}

		if (strcmp(tokens[t0 + 1], "srtcm") == 0) {
			p.mtr.alg = RTE_TABLE_ACTION_METER_SRTCM;
		} else if (strcmp(tokens[t0 + 1], "trtcm") == 0) {
			p.mtr.alg = RTE_TABLE_ACTION_METER_TRTCM;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"srtcm or trtcm");
			return;
		}

		if (strcmp(tokens[t0 + 2], "tc") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "tc");
			return;
		}

		if (softnic_parser_read_uint32(&p.mtr.n_tc,
			tokens[t0 + 3]) != 0) {
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

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "tm") == 0)) {
		if (n_tokens < t0 + 5) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile tm");
			return;
		}

		if (strcmp(tokens[t0 + 1], "spp") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "spp");
			return;
		}

		if (softnic_parser_read_uint32(&p.tm.n_subports_per_port,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_subports_per_port");
			return;
		}

		if (strcmp(tokens[t0 + 3], "pps") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "pps");
			return;
		}

		if (softnic_parser_read_uint32(&p.tm.n_pipes_per_subport,
			tokens[t0 + 4]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"n_pipes_per_subport");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TM;
		t0 += 5;
	} /* tm */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "encap") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"action profile encap");
			return;
		}

		if (strcmp(tokens[t0 + 1], "ether") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_ETHER;
		} else if (strcmp(tokens[t0 + 1], "vlan") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_VLAN;
		} else if (strcmp(tokens[t0 + 1], "qinq") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_QINQ;
		} else if (strcmp(tokens[t0 + 1], "mpls") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_MPLS;
		} else if (strcmp(tokens[t0 + 1], "pppoe") == 0) {
			p.encap.encap_mask = 1LLU << RTE_TABLE_ACTION_ENCAP_PPPOE;
		} else {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "encap");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_ENCAP;
		t0 += 2;
	} /* encap */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "nat") == 0)) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile nat");
			return;
		}

		if (strcmp(tokens[t0 + 1], "src") == 0) {
			p.nat.source_nat = 1;
		} else if (strcmp(tokens[t0 + 1], "dst") == 0) {
			p.nat.source_nat = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"src or dst");
			return;
		}

		if (strcmp(tokens[t0 + 2], "proto") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "proto");
			return;
		}

		if (strcmp(tokens[t0 + 3], "tcp") == 0) {
			p.nat.proto = 0x06;
		} else if (strcmp(tokens[t0 + 3], "udp") == 0) {
			p.nat.proto = 0x11;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"tcp or udp");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_NAT;
		t0 += 4;
	} /* nat */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "ttl") == 0)) {
		if (n_tokens < t0 + 4) {
			snprintf(out, out_size, MSG_ARG_MISMATCH,
				"table action profile ttl");
			return;
		}

		if (strcmp(tokens[t0 + 1], "drop") == 0) {
			p.ttl.drop = 1;
		} else if (strcmp(tokens[t0 + 1], "fwd") == 0) {
			p.ttl.drop = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"drop or fwd");
			return;
		}

		if (strcmp(tokens[t0 + 2], "stats") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "stats");
			return;
		}

		if (strcmp(tokens[t0 + 3], "none") == 0) {
			p.ttl.n_packets_enabled = 0;
		} else if (strcmp(tokens[t0 + 3], "pkts") == 0) {
			p.ttl.n_packets_enabled = 1;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"none or pkts");
			return;
		}

		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TTL;
		t0 += 4;
	} /* ttl */

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "stats") == 0)) {
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

	if (t0 < n_tokens &&
		(strcmp(tokens[t0], "time") == 0)) {
		p.action_mask |= 1LLU << RTE_TABLE_ACTION_TIME;
		t0 += 1;
	} /* time */

	if (t0 < n_tokens) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	ap = softnic_table_action_profile_create(softnic, name, &p);
	if (ap == NULL) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name>
 *  period <timer_period_ms>
 *  offset_port_id <offset_port_id>
 */
static void
cmd_pipeline(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct pipeline_params p;
	char *name;
	struct pipeline *pipeline;

	if (n_tokens != 6) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	name = tokens[1];

	if (strcmp(tokens[2], "period") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "period");
		return;
	}

	if (softnic_parser_read_uint32(&p.timer_period_ms,
		tokens[3]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "timer_period_ms");
		return;
	}

	if (strcmp(tokens[4], "offset_port_id") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset_port_id");
		return;
	}

	if (softnic_parser_read_uint32(&p.offset_port_id,
		tokens[5]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "offset_port_id");
		return;
	}

	pipeline = softnic_pipeline_create(softnic, name, &p);
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
 *  | source mempool <mempool_name> file <file_name> bpp <n_bytes_per_pkt>
 *  [action <port_in_action_profile_name>]
 *  [disabled]
 */
static void
cmd_pipeline_port_in(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_port_in_params p;
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

	if (softnic_parser_read_uint32(&p.burst_size, tokens[5]) != 0) {
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

		if (softnic_parser_read_uint16(&p.rxq.queue_id,
			tokens[t0 + 3]) != 0) {
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

		if (softnic_parser_read_uint32(&p.tap.mtu,
			tokens[t0 + 5]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "mtu");
			return;
		}

		t0 += 6;
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

		if (softnic_parser_read_uint32(&p.source.n_bytes_per_pkt,
			tokens[t0 + 6]) != 0) {
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
	if (n_tokens > t0 &&
		(strcmp(tokens[t0], "action") == 0)) {
		if (n_tokens < t0 + 2) {
			snprintf(out, out_size, MSG_ARG_MISMATCH, "action");
			return;
		}

		p.action_profile_name = tokens[t0 + 1];

		t0 += 2;
	}

	enabled = 1;
	if (n_tokens > t0 &&
		(strcmp(tokens[t0], "disabled") == 0)) {
		enabled = 0;

		t0 += 1;
	}

	if (n_tokens != t0) {
		snprintf(out, out_size, MSG_ARG_MISMATCH, tokens[0]);
		return;
	}

	status = softnic_pipeline_port_in_create(softnic,
		pipeline_name,
		&p,
		enabled);
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
 *  | sink [file <file_name> pkts <max_n_pkts>]
 */
static void
cmd_pipeline_port_out(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	struct softnic_port_out_params p;
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

	if (softnic_parser_read_uint32(&p.burst_size, tokens[5]) != 0) {
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

		if (softnic_parser_read_uint16(&p.txq.queue_id,
			tokens[9]) != 0) {
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

			if (softnic_parser_read_uint32(&p.sink.max_n_pkts,
				tokens[10]) != 0) {
				snprintf(out, out_size, MSG_ARG_INVALID, "max_n_pkts");
				return;
			}
		}
	} else {
		snprintf(out, out_size, MSG_ARG_INVALID, tokens[0]);
		return;
	}

	status = softnic_pipeline_port_out_create(softnic, pipeline_name, &p);
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
cmd_pipeline_table(struct pmd_internals *softnic,
	char **tokens,
	uint32_t n_tokens,
	char *out,
	size_t out_size)
{
	uint8_t key_mask[TABLE_RULE_MATCH_SIZE_MAX];
	struct softnic_table_params p;
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

		if (strcmp(tokens[t0 + 1], "ipv4") == 0) {
			p.match.acl.ip_version = 1;
		} else if (strcmp(tokens[t0 + 1], "ipv6") == 0) {
			p.match.acl.ip_version = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.acl.ip_header_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID,
				"ip_header_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.acl.n_rules,
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

		if (softnic_parser_read_uint32(&p.match.array.key_offset,
			tokens[t0 + 2]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 3], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.array.n_keys,
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

		if (strcmp(tokens[t0 + 1], "ext") == 0) {
			p.match.hash.extendable_bucket = 1;
		} else if (strcmp(tokens[t0 + 1], "lru") == 0) {
			p.match.hash.extendable_bucket = 0;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ext or lru");
			return;
		}

		if (strcmp(tokens[t0 + 2], "key") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "key");
			return;
		}

		if ((softnic_parser_read_uint32(&p.match.hash.key_size,
			tokens[t0 + 3]) != 0) ||
			p.match.hash.key_size == 0 ||
			p.match.hash.key_size > TABLE_RULE_MATCH_SIZE_MAX) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_size");
			return;
		}

		if (strcmp(tokens[t0 + 4], "mask") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "mask");
			return;
		}

		if ((softnic_parse_hex_string(tokens[t0 + 5],
			key_mask, &key_mask_size) != 0) ||
			key_mask_size != p.match.hash.key_size) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_mask");
			return;
		}
		p.match.hash.key_mask = key_mask;

		if (strcmp(tokens[t0 + 6], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.hash.key_offset,
			tokens[t0 + 7]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 8], "buckets") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "buckets");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.hash.n_buckets,
			tokens[t0 + 9]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "n_buckets");
			return;
		}

		if (strcmp(tokens[t0 + 10], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.hash.n_keys,
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

		if (strcmp(tokens[t0 + 1], "ipv4") == 0) {
			p.match.lpm.key_size = 4;
		} else if (strcmp(tokens[t0 + 1], "ipv6") == 0) {
			p.match.lpm.key_size = 16;
		} else {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND,
				"ipv4 or ipv6");
			return;
		}

		if (strcmp(tokens[t0 + 2], "offset") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "offset");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.lpm.key_offset,
			tokens[t0 + 3]) != 0) {
			snprintf(out, out_size, MSG_ARG_INVALID, "key_offset");
			return;
		}

		if (strcmp(tokens[t0 + 4], "size") != 0) {
			snprintf(out, out_size, MSG_ARG_NOT_FOUND, "size");
			return;
		}

		if (softnic_parser_read_uint32(&p.match.lpm.n_rules,
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
	if (n_tokens > t0 &&
		(strcmp(tokens[t0], "action") == 0)) {
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

	status = softnic_pipeline_table_create(softnic, pipeline_name, &p);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> table <table_id>
 */
static void
cmd_pipeline_port_in_table(struct pmd_internals *softnic,
	char **tokens,
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

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "table") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "table");
		return;
	}

	if (softnic_parser_read_uint32(&table_id, tokens[6]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "table_id");
		return;
	}

	status = softnic_pipeline_port_in_connect_to_table(softnic,
		pipeline_name,
		port_id,
		table_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> enable
 */
static void
cmd_softnic_pipeline_port_in_enable(struct pmd_internals *softnic,
	char **tokens,
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

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "enable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "enable");
		return;
	}

	status = softnic_pipeline_port_in_enable(softnic, pipeline_name, port_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * pipeline <pipeline_name> port in <port_id> disable
 */
static void
cmd_softnic_pipeline_port_in_disable(struct pmd_internals *softnic,
	char **tokens,
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

	if (softnic_parser_read_uint32(&port_id, tokens[4]) != 0) {
		snprintf(out, out_size, MSG_ARG_INVALID, "port_id");
		return;
	}

	if (strcmp(tokens[5], "disable") != 0) {
		snprintf(out, out_size, MSG_ARG_NOT_FOUND, "disable");
		return;
	}

	status = softnic_pipeline_port_in_disable(softnic, pipeline_name, port_id);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, tokens[0]);
		return;
	}
}

/**
 * thread <thread_id> pipeline <pipeline_name> enable
 */
static void
cmd_softnic_thread_pipeline_enable(struct pmd_internals *softnic,
	char **tokens,
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

	if (softnic_parser_read_uint32(&thread_id, tokens[1]) != 0) {
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

	status = softnic_thread_pipeline_enable(softnic, thread_id, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL, "thread pipeline enable");
		return;
	}
}

/**
 * thread <thread_id> pipeline <pipeline_name> disable
 */
static void
cmd_softnic_thread_pipeline_disable(struct pmd_internals *softnic,
	char **tokens,
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

	if (softnic_parser_read_uint32(&thread_id, tokens[1]) != 0) {
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

	status = softnic_thread_pipeline_disable(softnic, thread_id, pipeline_name);
	if (status) {
		snprintf(out, out_size, MSG_CMD_FAIL,
			"thread pipeline disable");
		return;
	}
}

void
softnic_cli_process(char *in, char *out, size_t out_size, void *arg)
{
	char *tokens[CMD_MAX_TOKENS];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct pmd_internals *softnic = arg;
	int status;

	if (is_comment(in))
		return;

	status = softnic_parse_tokenize_string(in, tokens, &n_tokens);
	if (status) {
		snprintf(out, out_size, MSG_ARG_TOO_MANY, "");
		return;
	}

	if (n_tokens == 0)
		return;

	if (strcmp(tokens[0], "mempool") == 0) {
		cmd_mempool(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "link") == 0) {
		cmd_link(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "swq") == 0) {
		cmd_swq(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "tap") == 0) {
		cmd_tap(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "port") == 0) {
		cmd_port_in_action_profile(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "table") == 0) {
		cmd_table_action_profile(softnic, tokens, n_tokens, out, out_size);
		return;
	}

	if (strcmp(tokens[0], "pipeline") == 0) {
		if (n_tokens >= 3 &&
			(strcmp(tokens[2], "period") == 0)) {
			cmd_pipeline(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[4], "bsz") == 0)) {
			cmd_pipeline_port_in(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 5 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "out") == 0) &&
			(strcmp(tokens[4], "bsz") == 0)) {
			cmd_pipeline_port_out(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 4 &&
			(strcmp(tokens[2], "table") == 0) &&
			(strcmp(tokens[3], "match") == 0)) {
			cmd_pipeline_table(softnic, tokens, n_tokens, out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "table") == 0)) {
			cmd_pipeline_port_in_table(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "enable") == 0)) {
			cmd_softnic_pipeline_port_in_enable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if (n_tokens >= 6 &&
			(strcmp(tokens[2], "port") == 0) &&
			(strcmp(tokens[3], "in") == 0) &&
			(strcmp(tokens[5], "disable") == 0)) {
			cmd_softnic_pipeline_port_in_disable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	if (strcmp(tokens[0], "thread") == 0) {
		if ((n_tokens >= 5) &&
			(strcmp(tokens[4], "enable") == 0)) {
			cmd_softnic_thread_pipeline_enable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}

		if ((n_tokens >= 5) &&
			(strcmp(tokens[4], "disable") == 0)) {
			cmd_softnic_thread_pipeline_disable(softnic, tokens, n_tokens,
				out, out_size);
			return;
		}
	}

	snprintf(out, out_size, MSG_CMD_UNKNOWN, tokens[0]);
}

int
softnic_cli_script_process(struct pmd_internals *softnic,
	const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max)
{
	char *msg_in = NULL, *msg_out = NULL;
	FILE *f = NULL;

	/* Check input arguments */
	if (file_name == NULL ||
		strlen(file_name) == 0 ||
		msg_in_len_max == 0 ||
		msg_out_len_max == 0)
		return -EINVAL;

	msg_in = malloc(msg_in_len_max + 1);
	msg_out = malloc(msg_out_len_max + 1);
	if (msg_in == NULL ||
		msg_out == NULL) {
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

		softnic_cli_process(msg_in,
			msg_out,
			msg_out_len_max,
			softnic);

		if (strlen(msg_out))
			printf("%s", msg_out);
	}

	/* Close file */
	fclose(f);
	free(msg_out);
	free(msg_in);
	return 0;
}
