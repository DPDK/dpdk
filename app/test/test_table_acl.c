/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_hexdump.h>
#include "test_table.h"
#include "test_table_acl.h"

#define IPv4(a, b, c, d) ((uint32_t)(((a) & 0xff) << 24) |		\
	(((b) & 0xff) << 16) |						\
	(((c) & 0xff) << 8) |						\
	((d) & 0xff))

/*
 * Rule and trace formats definitions.
 **/

struct ipv4_5tuple {
	uint8_t  proto;
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
};

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = PROTO_FIELD_IPV4,
		.offset = offsetof(struct ipv4_5tuple, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = SRC_FIELD_IPV4,
		.offset = offsetof(struct ipv4_5tuple, ip_src),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = DST_FIELD_IPV4,
		.offset = offsetof(struct ipv4_5tuple, ip_dst),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = SRCP_FIELD_IPV4,
		.offset = offsetof(struct ipv4_5tuple, port_src),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = SRCP_FIELD_IPV4,
		.offset = offsetof(struct ipv4_5tuple, port_dst),
	},
};

struct rte_table_acl_rule_add_params table_acl_IPv4_rule;

typedef int (*parse_5tuple)(char *text,
	struct rte_table_acl_rule_add_params *rule);

/*
* The order of the fields in the rule string after the initial '@'
*/
enum {
	CB_FLD_SRC_ADDR,
	CB_FLD_DST_ADDR,
	CB_FLD_SRC_PORT_RANGE,
	CB_FLD_DST_PORT_RANGE,
	CB_FLD_PROTO,
	CB_FLD_NUM,
};


#define GET_CB_FIELD(in, fd, base, lim, dlm)				\
do {									\
	unsigned long val;						\
	char *end;							\
									\
	errno = 0;							\
	val = strtoul((in), &end, (base));				\
	if (errno != 0 || end[0] != (dlm) || val > (lim))		\
		return -EINVAL;						\
	(fd) = (typeof(fd)) val;					\
	(in) = end + 1;							\
} while (0)




static int
parse_ipv4_net(const char *in, uint32_t *addr, uint32_t *mask_len)
{
	uint8_t a, b, c, d, m;

	GET_CB_FIELD(in, a, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, b, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, c, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, d, 0, UINT8_MAX, '/');
	GET_CB_FIELD(in, m, 0, sizeof(uint32_t) * CHAR_BIT, 0);

	addr[0] = IPv4(a, b, c, d);
	mask_len[0] = m;

	return 0;
}

static int
parse_port_range(const char *in, uint16_t *port_low, uint16_t *port_high)
{
	uint16_t a, b;

	GET_CB_FIELD(in, a, 0, UINT16_MAX, ':');
	GET_CB_FIELD(in, b, 0, UINT16_MAX, 0);

	port_low[0] = a;
	port_high[0] = b;

	return 0;
}

static int
parse_cb_ipv4_rule(char *str, struct rte_table_acl_rule_add_params *v)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";

	/*
	** Skip leading '@'
	*/
	if (strchr(str, '@') != str)
		return -EINVAL;

	s = str + 1;

	/*
	* Populate the 'in' array with the location of each
	* field in the string we're parsing
	*/
	for (i = 0; i != DIM(in); i++) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
		s = NULL;
	}

	/* Parse x.x.x.x/x */
	rc = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
		&v->field_value[SRC_FIELD_IPV4].value.u32,
		&v->field_value[SRC_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read src address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[SRC_FIELD_IPV4].value.u32,
		v->field_value[SRC_FIELD_IPV4].mask_range.u32);

	/* Parse x.x.x.x/x */
	rc = parse_ipv4_net(in[CB_FLD_DST_ADDR],
		&v->field_value[DST_FIELD_IPV4].value.u32,
		&v->field_value[DST_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read dest address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[DST_FIELD_IPV4].value.u32,
	v->field_value[DST_FIELD_IPV4].mask_range.u32);
	/* Parse n:n */
	rc = parse_port_range(in[CB_FLD_SRC_PORT_RANGE],
		&v->field_value[SRCP_FIELD_IPV4].value.u16,
		&v->field_value[SRCP_FIELD_IPV4].mask_range.u16);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read source port range: %s\n",
			in[CB_FLD_SRC_PORT_RANGE]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[SRCP_FIELD_IPV4].value.u16,
		v->field_value[SRCP_FIELD_IPV4].mask_range.u16);
	/* Parse n:n */
	rc = parse_port_range(in[CB_FLD_DST_PORT_RANGE],
		&v->field_value[DSTP_FIELD_IPV4].value.u16,
		&v->field_value[DSTP_FIELD_IPV4].mask_range.u16);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read dest port range: %s\n",
			in[CB_FLD_DST_PORT_RANGE]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[DSTP_FIELD_IPV4].value.u16,
		v->field_value[DSTP_FIELD_IPV4].mask_range.u16);
	/* parse 0/0xnn */
	GET_CB_FIELD(in[CB_FLD_PROTO],
		v->field_value[PROTO_FIELD_IPV4].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO],
		v->field_value[PROTO_FIELD_IPV4].mask_range.u8,
		0, UINT8_MAX, 0);

	printf("V=%u, mask=%u\n",
		(unsigned int)v->field_value[PROTO_FIELD_IPV4].value.u8,
		v->field_value[PROTO_FIELD_IPV4].mask_range.u8);
	return 0;
}

static int
parse_cb_ipv4_rule_del(char *str, struct rte_table_acl_rule_delete_params *v)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";

	/*
	** Skip leading '@'
	*/
	if (strchr(str, '@') != str)
		return -EINVAL;

	s = str + 1;

	/*
	* Populate the 'in' array with the location of each
	* field in the string we're parsing
	*/
	for (i = 0; i != DIM(in); i++) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
		s = NULL;
	}

	/* Parse x.x.x.x/x */
	rc = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
		&v->field_value[SRC_FIELD_IPV4].value.u32,
		&v->field_value[SRC_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read src address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[SRC_FIELD_IPV4].value.u32,
		v->field_value[SRC_FIELD_IPV4].mask_range.u32);

	/* Parse x.x.x.x/x */
	rc = parse_ipv4_net(in[CB_FLD_DST_ADDR],
		&v->field_value[DST_FIELD_IPV4].value.u32,
		&v->field_value[DST_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read dest address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[DST_FIELD_IPV4].value.u32,
	v->field_value[DST_FIELD_IPV4].mask_range.u32);
	/* Parse n:n */
	rc = parse_port_range(in[CB_FLD_SRC_PORT_RANGE],
		&v->field_value[SRCP_FIELD_IPV4].value.u16,
		&v->field_value[SRCP_FIELD_IPV4].mask_range.u16);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read source port range: %s\n",
			in[CB_FLD_SRC_PORT_RANGE]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[SRCP_FIELD_IPV4].value.u16,
		v->field_value[SRCP_FIELD_IPV4].mask_range.u16);
	/* Parse n:n */
	rc = parse_port_range(in[CB_FLD_DST_PORT_RANGE],
		&v->field_value[DSTP_FIELD_IPV4].value.u16,
		&v->field_value[DSTP_FIELD_IPV4].mask_range.u16);
	if (rc != 0) {
		RTE_LOG(ERR, PIPELINE, "failed to read dest port range: %s\n",
			in[CB_FLD_DST_PORT_RANGE]);
		return rc;
	}

	printf("V=%u, mask=%u\n", v->field_value[DSTP_FIELD_IPV4].value.u16,
		v->field_value[DSTP_FIELD_IPV4].mask_range.u16);
	/* parse 0/0xnn */
	GET_CB_FIELD(in[CB_FLD_PROTO],
		v->field_value[PROTO_FIELD_IPV4].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO],
		v->field_value[PROTO_FIELD_IPV4].mask_range.u8,
		0, UINT8_MAX, 0);

	printf("V=%u, mask=%u\n",
		(unsigned int)v->field_value[PROTO_FIELD_IPV4].value.u8,
		v->field_value[PROTO_FIELD_IPV4].mask_range.u8);
	return 0;
}

/*
 * The format for these rules DO NOT need the port ranges to be
 * separated by ' : ', just ':'. It's a lot more readable and
 * cleaner, IMO.
 */
char lines[][128] = {
	"@0.0.0.0/0 0.0.0.0/0 0:65535 0:65535 2/0xff", /* Protocol check */
	"@192.168.3.1/32 0.0.0.0/0 0:65535 0:65535 0/0", /* Src IP checl */
	"@0.0.0.0/0 10.4.4.1/32 0:65535 0:65535 0/0", /* dst IP check */
	"@0.0.0.0/0 0.0.0.0/0 105:105 0:65535 0/0", /* src port check */
	"@0.0.0.0/0 0.0.0.0/0 0:65535 206:206 0/0", /* dst port check */
};

char line[128];


static int
setup_acl_pipeline(void)
{
	int ret;
	int i;
	struct rte_pipeline_params pipeline_params = {
		.name = "PIPELINE",
		.socket_id = 0,
	};
	uint32_t n;
	struct rte_table_acl_rule_add_params rule_params;
	struct rte_pipeline_table_acl_rule_delete_params *delete_params;
	parse_5tuple parser;
	char acl_name[64];

	/* Pipeline configuration */
	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL) {
		RTE_LOG(INFO, PIPELINE, "%s: Failed to configure pipeline\n",
			__func__);
		goto fail;
	}

	/* Input port configuration */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_port_ring_reader_params port_ring_params = {
			.ring = rings_rx[i],
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ring_reader_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.burst_size = BURST_SIZE,
		};

		/* Put in action for some ports */
		if (i)
			port_params.f_action = port_in_action;

		ret = rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i]);
		if (ret) {
			rte_panic("Unable to configure input port %d, ret:%d\n",
				i, ret);
			goto fail;
		}
	}

	/* output Port configuration */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_port_ring_writer_params port_ring_params = {
			.ring = rings_tx[i],
			.tx_burst_sz = BURST_SIZE,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ring_writer_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.arg_ah = NULL,
		};


		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i])) {
			rte_panic("Unable to configure output port %d\n", i);
			goto fail;
		}
	}

	/* Table configuration  */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_pipeline_table_params table_params;

		/* Set up defaults for stub */
		table_params.ops = &rte_table_stub_ops;
		table_params.arg_create = NULL;
		table_params.f_action_hit = action_handler_hit;
		table_params.f_action_miss = NULL;
		table_params.action_data_size = 0;

		RTE_LOG(INFO, PIPELINE, "miss_action=%x\n",
			table_entry_miss_action);

		printf("RTE_ACL_RULE_SZ(%zu) = %zu\n", DIM(ipv4_defs),
			RTE_ACL_RULE_SZ(DIM(ipv4_defs)));

		struct rte_table_acl_params acl_params;

		acl_params.n_rules = 1 << 5;
		acl_params.n_rule_fields = DIM(ipv4_defs);
		snprintf(acl_name, sizeof(acl_name), "ACL%d", i);
		acl_params.name = acl_name;
		memcpy(acl_params.field_format, ipv4_defs, sizeof(ipv4_defs));

		table_params.ops = &rte_table_acl_ops;
		table_params.arg_create = &acl_params;

		if (rte_pipeline_table_create(p, &table_params, &table_id[i])) {
			rte_panic("Unable to configure table %u\n", i);
			goto fail;
		}

		if (connect_miss_action_to_table) {
			if (rte_pipeline_table_create(p, &table_params,
				&table_id[i+2])) {
				rte_panic("Unable to configure table %u\n", i);
				goto fail;
			}
		}
	}

	for (i = 0; i < N_PORTS; i++) {
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id[i])) {
			rte_panic("Unable to connect input port %u to "
				"table %u\n",
				port_in_id[i],  table_id[i]);
			goto fail;
		}
	}

	/* Add bulk entries to tables */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_table_acl_rule_add_params keys[5];
		struct rte_pipeline_table_entry entries[5];
		struct rte_table_acl_rule_add_params *key_array[5];
		struct rte_pipeline_table_entry *table_entries[5];
		int key_found[5];
		struct rte_pipeline_table_entry *table_entries_ptr[5];
		struct rte_pipeline_table_entry entries_ptr[5];

		parser = parse_cb_ipv4_rule;
		for (n = 0; n < 5; n++) {
			memset(&keys[n], 0, sizeof(struct rte_table_acl_rule_add_params));
			key_array[n] = &keys[n];

			strlcpy(line, lines[n], sizeof(line));
			printf("PARSING [%s]\n", line);

			ret = parser(line, &keys[n]);
			if (ret != 0) {
				RTE_LOG(ERR, PIPELINE,
					"line %u: parse_cb_ipv4vlan_rule"
					" failed, error code: %d (%s)\n",
					n, ret, strerror(-ret));
				return ret;
			}

			keys[n].priority = RTE_ACL_MAX_PRIORITY - n - 1;

			entries[n].action = RTE_PIPELINE_ACTION_PORT;
			entries[n].port_id = port_out_id[i^1];
			table_entries[n] = &entries[n];
			table_entries_ptr[n] = &entries_ptr[n];
		}

		ret = rte_pipeline_table_entry_add_bulk(p, table_id[i],
				(void **)key_array, table_entries, 5, key_found, table_entries_ptr);
		if (ret < 0) {
			rte_panic("Add entry bulk to table %u failed (%d)\n",
				table_id[i], ret);
			goto fail;
		}
	}

	/* Delete bulk entries from tables */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_table_acl_rule_delete_params keys[5];
		struct rte_table_acl_rule_delete_params *key_array[5];
		struct rte_pipeline_table_entry *table_entries[5];
		int key_found[5];

		memset(table_entries, 0, sizeof(table_entries));

		for (n = 0; n < 5; n++) {
			memset(&keys[n], 0, sizeof(struct rte_table_acl_rule_delete_params));
			key_array[n] = &keys[n];

			strlcpy(line, lines[n], sizeof(line));
			printf("PARSING [%s]\n", line);

			ret = parse_cb_ipv4_rule_del(line, &keys[n]);
			if (ret != 0) {
				RTE_LOG(ERR, PIPELINE,
					"line %u: parse_cb_ipv4vlan_rule"
					" failed, error code: %d (%s)\n",
					n, ret, strerror(-ret));
				return ret;
			}
		}

		ret = rte_pipeline_table_entry_delete_bulk(p, table_id[i],
			(void **)key_array, 5, key_found, table_entries);
		if (ret < 0) {
			rte_panic("Delete bulk entries from table %u failed (%d)\n",
				table_id[i], ret);
			goto fail;
		} else
			printf("Bulk deleted rules.\n");
	}

	/* Add entries to tables */
	for (i = 0; i < N_PORTS; i++) {
		struct rte_pipeline_table_entry table_entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i^1]},
		};
		int key_found;
		struct rte_pipeline_table_entry *entry_ptr;

		memset(&rule_params, 0, sizeof(rule_params));
		parser = parse_cb_ipv4_rule;

		for (n = 1; n <= 5; n++) {
			strlcpy(line, lines[n - 1], sizeof(line));
			printf("PARSING [%s]\n", line);

			ret = parser(line, &rule_params);
			if (ret != 0) {
				RTE_LOG(ERR, PIPELINE,
					"line %u: parse_cb_ipv4vlan_rule"
					" failed, error code: %d (%s)\n",
					n, ret, strerror(-ret));
				return ret;
			}

			rule_params.priority = RTE_ACL_MAX_PRIORITY - n;

			ret = rte_pipeline_table_entry_add(p, table_id[i],
				&rule_params,
				&table_entry, &key_found, &entry_ptr);
			if (ret < 0) {
				rte_panic("Add entry to table %u failed (%d)\n",
					table_id[i], ret);
				goto fail;
			}
		}

		/* delete a few rules */
		for (n = 2; n <= 3; n++) {
			strlcpy(line, lines[n - 1], sizeof(line));
			printf("PARSING [%s]\n", line);

			ret = parser(line, &rule_params);
			if (ret != 0) {
				RTE_LOG(ERR, PIPELINE, "line %u: parse rule "
					" failed, error code: %d (%s)\n",
					n, ret, strerror(-ret));
				return ret;
			}

			delete_params = (struct
				rte_pipeline_table_acl_rule_delete_params *)
				&(rule_params.field_value[0]);
			ret = rte_pipeline_table_entry_delete(p, table_id[i],
				delete_params, &key_found, NULL);
			if (ret < 0) {
				rte_panic("Add entry to table %u failed (%d)\n",
					table_id[i], ret);
				goto fail;
			} else
				printf("Deleted Rule.\n");
		}


		/* Try to add duplicates */
		for (n = 1; n <= 5; n++) {
			strlcpy(line, lines[n - 1], sizeof(line));
			printf("PARSING [%s]\n", line);

			ret = parser(line, &rule_params);
			if (ret != 0) {
				RTE_LOG(ERR, PIPELINE, "line %u: parse rule"
					" failed, error code: %d (%s)\n",
					n, ret, strerror(-ret));
				return ret;
			}

			rule_params.priority = RTE_ACL_MAX_PRIORITY - n;

			ret = rte_pipeline_table_entry_add(p, table_id[i],
				&rule_params,
				&table_entry, &key_found, &entry_ptr);
			if (ret < 0) {
				rte_panic("Add entry to table %u failed (%d)\n",
					table_id[i], ret);
				goto fail;
			}
		}
	}

	/* Enable input ports */
	for (i = 0; i < N_PORTS ; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0) {
		rte_panic("Pipeline consistency check failed\n");
		goto fail;
	}

	return  0;
fail:

	return -1;
}

static int
test_pipeline_single_filter(int expected_count)
{
	int i, j, ret, tx_count;
	struct ipv4_5tuple five_tuple;

	/* Allocate a few mbufs and manually insert into the rings. */
	for (i = 0; i < N_PORTS; i++) {
		for (j = 0; j < 8; j++) {
			struct rte_mbuf *mbuf;

			mbuf = rte_pktmbuf_alloc(pool);
			if (mbuf == NULL)
				/* this will cause test failure after cleanup
				 * of already enqueued mbufs, as the mbuf
				 * counts won't match */
				break;
			memset(rte_pktmbuf_mtod(mbuf, char *), 0x00,
				sizeof(struct ipv4_5tuple));

			five_tuple.proto = j;
			five_tuple.ip_src = rte_bswap32(IPv4(192, 168, j, 1));
			five_tuple.ip_dst = rte_bswap32(IPv4(10, 4, j, 1));
			five_tuple.port_src = rte_bswap16(100 + j);
			five_tuple.port_dst = rte_bswap16(200 + j);

			memcpy(rte_pktmbuf_mtod(mbuf, char *), &five_tuple,
				sizeof(struct ipv4_5tuple));
			RTE_LOG(INFO, PIPELINE, "%s: Enqueue onto ring %d\n",
				__func__, i);
			rte_ring_enqueue(rings_rx[i], mbuf);
		}
	}

	/* Run pipeline once */
	for (i = 0; i< N_PORTS; i++)
		rte_pipeline_run(p);

	rte_pipeline_flush(p);

	tx_count = 0;

	for (i = 0; i < N_PORTS; i++) {
		void *objs[RING_TX_SIZE];
		struct rte_mbuf *mbuf;

		ret = rte_ring_sc_dequeue_burst(rings_tx[i], objs, 10, NULL);
		if (ret <= 0) {
			printf("Got no objects from ring %d - error code %d\n",
				i, ret);
		} else {
			printf("Got %d object(s) from ring %d!\n", ret, i);
			for (j = 0; j < ret; j++) {
				mbuf = objs[j];
				rte_hexdump(stdout, "mbuf",
					rte_pktmbuf_mtod(mbuf, char *), 64);
				rte_pktmbuf_free(mbuf);
			}
			tx_count += ret;
		}
	}

	if (tx_count != expected_count) {
		RTE_LOG(INFO, PIPELINE,
			"%s: Unexpected packets for ACL test, "
			"expected %d, got %d\n",
			__func__, expected_count, tx_count);
		goto fail;
	}

	rte_pipeline_free(p);

	return  0;
fail:
	return -1;

}

int
test_table_acl(void)
{


	override_hit_mask = 0xFF; /* All packets are a hit */

	setup_acl_pipeline();
	if (test_pipeline_single_filter(10) < 0)
		return -1;

	return 0;
}
