/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <errno.h>

#include "test.h"

#include <rte_string_fns.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_acl.h>
#include <rte_common.h>

#include "test_acl.h"

#define	BIT_SIZEOF(x) (sizeof(x) * CHAR_BIT)

#define LEN RTE_ACL_MAX_CATEGORIES

RTE_ACL_RULE_DEF(acl_ipv4vlan_rule, RTE_ACL_IPV4VLAN_NUM_FIELDS);

struct rte_acl_param acl_param = {
	.name = "acl_ctx",
	.socket_id = SOCKET_ID_ANY,
	.rule_size = RTE_ACL_IPV4VLAN_RULE_SZ,
	.max_rule_num = 0x30000,
};

struct rte_acl_ipv4vlan_rule acl_rule = {
		.data = { .priority = 1, .category_mask = 0xff },
		.src_port_low = 0,
		.src_port_high = UINT16_MAX,
		.dst_port_low = 0,
		.dst_port_high = UINT16_MAX,
};

const uint32_t ipv4_7tuple_layout[RTE_ACL_IPV4VLAN_NUM] = {
	offsetof(struct ipv4_7tuple, proto),
	offsetof(struct ipv4_7tuple, vlan),
	offsetof(struct ipv4_7tuple, ip_src),
	offsetof(struct ipv4_7tuple, ip_dst),
	offsetof(struct ipv4_7tuple, port_src),
};


/* byteswap to cpu or network order */
static void
bswap_test_data(struct ipv4_7tuple *data, int len, int to_be)
{
	int i;

	for (i = 0; i < len; i++) {

		if (to_be) {
			/* swap all bytes so that they are in network order */
			data[i].ip_dst = rte_cpu_to_be_32(data[i].ip_dst);
			data[i].ip_src = rte_cpu_to_be_32(data[i].ip_src);
			data[i].port_dst = rte_cpu_to_be_16(data[i].port_dst);
			data[i].port_src = rte_cpu_to_be_16(data[i].port_src);
			data[i].vlan = rte_cpu_to_be_16(data[i].vlan);
			data[i].domain = rte_cpu_to_be_16(data[i].domain);
		} else {
			data[i].ip_dst = rte_be_to_cpu_32(data[i].ip_dst);
			data[i].ip_src = rte_be_to_cpu_32(data[i].ip_src);
			data[i].port_dst = rte_be_to_cpu_16(data[i].port_dst);
			data[i].port_src = rte_be_to_cpu_16(data[i].port_src);
			data[i].vlan = rte_be_to_cpu_16(data[i].vlan);
			data[i].domain = rte_be_to_cpu_16(data[i].domain);
		}
	}
}

static int
acl_ipv4vlan_check_rule(const struct rte_acl_ipv4vlan_rule *rule)
{
	if (rule->src_port_low > rule->src_port_high ||
			rule->dst_port_low > rule->dst_port_high ||
			rule->src_mask_len > BIT_SIZEOF(rule->src_addr) ||
			rule->dst_mask_len > BIT_SIZEOF(rule->dst_addr))
		return -EINVAL;
	return 0;
}

static void
acl_ipv4vlan_convert_rule(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	ro->data = ri->data;

	ro->field[RTE_ACL_IPV4VLAN_PROTO_FIELD].value.u8 = ri->proto;
	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD].value.u16 = ri->vlan;
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD].value.u16 = ri->domain;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].value.u32 = ri->src_addr;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].value.u32 = ri->dst_addr;
	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD].value.u16 = ri->src_port_low;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD].value.u16 = ri->dst_port_low;

	ro->field[RTE_ACL_IPV4VLAN_PROTO_FIELD].mask_range.u8 = ri->proto_mask;
	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD].mask_range.u16 = ri->vlan_mask;
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD].mask_range.u16 =
		ri->domain_mask;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32 =
		ri->src_mask_len;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32 = ri->dst_mask_len;
	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD].mask_range.u16 =
		ri->src_port_high;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD].mask_range.u16 =
		ri->dst_port_high;
}

/*
 * Add ipv4vlan rules to an existing ACL context.
 * This function is not multi-thread safe.
 *
 * @param ctx
 *   ACL context to add patterns to.
 * @param rules
 *   Array of rules to add to the ACL context.
 *   Note that all fields in rte_acl_ipv4vlan_rule structures are expected
 *   to be in host byte order.
 * @param num
 *   Number of elements in the input array of rules.
 * @return
 *   - -ENOMEM if there is no space in the ACL context for these rules.
 *   - -EINVAL if the parameters are invalid.
 *   - Zero if operation completed successfully.
 */
static int
rte_acl_ipv4vlan_add_rules(struct rte_acl_ctx *ctx,
	const struct rte_acl_ipv4vlan_rule *rules,
	uint32_t num)
{
	int32_t rc;
	uint32_t i;
	struct acl_ipv4vlan_rule rv;

	if (ctx == NULL || rules == NULL)
		return -EINVAL;

	/* check input rules. */
	for (i = 0; i != num; i++) {
		rc = acl_ipv4vlan_check_rule(rules + i);
		if (rc != 0) {
			RTE_LOG(ERR, ACL, "%s: rule #%u is invalid\n",
				__func__, i + 1);
			return rc;
		}
	}

	/* perform conversion to the internal format and add to the context. */
	for (i = 0, rc = 0; i != num && rc == 0; i++) {
		acl_ipv4vlan_convert_rule(rules + i, &rv);
		rc = rte_acl_add_rules(ctx, (struct rte_acl_rule *)&rv, 1);
	}

	return rc;
}

static void
acl_ipv4vlan_config(struct rte_acl_config *cfg,
	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM],
	uint32_t num_categories)
{
	static const struct rte_acl_field_def
		ipv4_defs[RTE_ACL_IPV4VLAN_NUM_FIELDS] = {
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint8_t),
			.field_index = RTE_ACL_IPV4VLAN_PROTO_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PROTO,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_VLAN1_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_VLAN,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_VLAN2_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_VLAN,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = RTE_ACL_IPV4VLAN_SRC_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_SRC,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = RTE_ACL_IPV4VLAN_DST_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_DST,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_SRCP_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PORTS,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_DSTP_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PORTS,
		},
	};

	memcpy(&cfg->defs, ipv4_defs, sizeof(ipv4_defs));
	cfg->num_fields = RTE_DIM(ipv4_defs);

	cfg->defs[RTE_ACL_IPV4VLAN_PROTO_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PROTO];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_VLAN];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_VLAN] +
		cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].size;
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_SRC];
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_DST];
	cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PORTS];
	cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PORTS] +
		cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].size;

	cfg->num_categories = num_categories;
}

/*
 * Analyze set of ipv4vlan rules and build required internal
 * run-time structures.
 * This function is not multi-thread safe.
 *
 * @param ctx
 *   ACL context to build.
 * @param layout
 *   Layout of input data to search through.
 * @param num_categories
 *   Maximum number of categories to use in that build.
 * @return
 *   - -ENOMEM if couldn't allocate enough memory.
 *   - -EINVAL if the parameters are invalid.
 *   - Negative error code if operation failed.
 *   - Zero if operation completed successfully.
 */
static int
rte_acl_ipv4vlan_build(struct rte_acl_ctx *ctx,
	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM],
	uint32_t num_categories)
{
	struct rte_acl_config cfg;

	if (ctx == NULL || layout == NULL)
		return -EINVAL;

	memset(&cfg, 0, sizeof(cfg));
	acl_ipv4vlan_config(&cfg, layout, num_categories);
	return rte_acl_build(ctx, &cfg);
}

/*
 * Test ACL lookup (selected alg).
 */
static int
test_classify_alg(struct rte_acl_ctx *acx, struct ipv4_7tuple test_data[],
	const uint8_t *data[], size_t dim, enum rte_acl_classify_alg alg)
{
	int32_t ret;
	uint32_t i, result, count;
	uint32_t results[dim * RTE_ACL_MAX_CATEGORIES];

	/* set given classify alg, skip test if alg is not supported */
	ret = rte_acl_set_ctx_classify(acx, alg);
	if (ret != 0)
		return (ret == -ENOTSUP) ? 0 : ret;

	/**
	 * these will run quite a few times, it's necessary to test code paths
	 * from num=0 to num>8
	 */
	for (count = 0; count <= dim; count++) {
		ret = rte_acl_classify(acx, data, results,
				count, RTE_ACL_MAX_CATEGORIES);
		if (ret != 0) {
			printf("Line %i: classify(alg=%d) failed!\n",
				__LINE__, alg);
			return ret;
		}

		/* check if we allow everything we should allow */
		for (i = 0; i < count; i++) {
			result =
				results[i * RTE_ACL_MAX_CATEGORIES + ACL_ALLOW];
			if (result != test_data[i].allow) {
				printf("Line %i: Error in allow results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, i, test_data[i].allow,
					result);
				return -EINVAL;
			}
		}

		/* check if we deny everything we should deny */
		for (i = 0; i < count; i++) {
			result = results[i * RTE_ACL_MAX_CATEGORIES + ACL_DENY];
			if (result != test_data[i].deny) {
				printf("Line %i: Error in deny results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, i, test_data[i].deny,
					result);
				return -EINVAL;
			}
		}
	}

	/* restore default classify alg */
	return rte_acl_set_ctx_classify(acx, RTE_ACL_CLASSIFY_DEFAULT);
}

/*
 * Test ACL lookup (all possible methods).
 */
static int
test_classify_run(struct rte_acl_ctx *acx, struct ipv4_7tuple test_data[],
	size_t dim)
{
	int32_t ret;
	uint32_t i;
	const uint8_t *data[dim];

	static const enum rte_acl_classify_alg alg[] = {
		RTE_ACL_CLASSIFY_SCALAR,
		RTE_ACL_CLASSIFY_SSE,
		RTE_ACL_CLASSIFY_AVX2,
		RTE_ACL_CLASSIFY_NEON,
		RTE_ACL_CLASSIFY_ALTIVEC,
		RTE_ACL_CLASSIFY_AVX512X16,
		RTE_ACL_CLASSIFY_AVX512X32,
	};

	/* swap all bytes in the data to network order */
	bswap_test_data(test_data, dim, 1);

	/* store pointers to test data */
	for (i = 0; i < dim; i++)
		data[i] = (uint8_t *)&test_data[i];

	ret = 0;
	for (i = 0; i != RTE_DIM(alg); i++) {
		ret = test_classify_alg(acx, test_data, data, dim, alg[i]);
		if (ret < 0) {
			printf("Line %i: %s() for alg=%d failed, errno=%d\n",
				__LINE__, __func__, alg[i], -ret);
			break;
		}
	}

	/* swap data back to cpu order so that next time tests don't fail */
	bswap_test_data(test_data, dim, 0);
	return ret;
}

static int
test_classify_buid(struct rte_acl_ctx *acx,
	const struct rte_acl_ipv4vlan_rule *rules, uint32_t num)
{
	int ret;

	/* add rules to the context */
	ret = rte_acl_ipv4vlan_add_rules(acx, rules, num);
	if (ret != 0) {
		printf("Line %i: Adding rules to ACL context failed!\n",
			__LINE__);
		return ret;
	}

	/* try building the context */
	ret = rte_acl_ipv4vlan_build(acx, ipv4_7tuple_layout,
		RTE_ACL_MAX_CATEGORIES);
	if (ret != 0) {
		printf("Line %i: Building ACL context failed!\n", __LINE__);
		return ret;
	}

	return 0;
}

#define	TEST_CLASSIFY_ITER	4

/*
 * Test scalar and SSE ACL lookup.
 */
static int
test_classify(void)
{
	struct rte_acl_ctx *acx;
	int i, ret;

	acx = rte_acl_create(&acl_param);
	if (acx == NULL) {
		printf("Line %i: Error creating ACL context!\n", __LINE__);
		return -1;
	}

	ret = 0;
	for (i = 0; i != TEST_CLASSIFY_ITER; i++) {

		if ((i & 1) == 0)
			rte_acl_reset(acx);
		else
			rte_acl_reset_rules(acx);

		ret = test_classify_buid(acx, acl_test_rules,
			RTE_DIM(acl_test_rules));
		if (ret != 0) {
			printf("Line %i, iter: %d: "
				"Adding rules to ACL context failed!\n",
				__LINE__, i);
			break;
		}

		ret = test_classify_run(acx, acl_test_data,
			RTE_DIM(acl_test_data));
		if (ret != 0) {
			printf("Line %i, iter: %d: %s failed!\n",
				__LINE__, i, __func__);
			break;
		}

		/* reset rules and make sure that classify still works ok. */
		rte_acl_reset_rules(acx);
		ret = test_classify_run(acx, acl_test_data,
			RTE_DIM(acl_test_data));
		if (ret != 0) {
			printf("Line %i, iter: %d: %s failed!\n",
				__LINE__, i, __func__);
			break;
		}
	}

	rte_acl_free(acx);
	return ret;
}

static int
test_build_ports_range(void)
{
	static const struct rte_acl_ipv4vlan_rule test_rules[] = {
		{
			/* match all packets. */
			.data = {
				.userdata = 1,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 101,
			},
			.src_port_low = 0,
			.src_port_high = UINT16_MAX,
			.dst_port_low = 0,
			.dst_port_high = UINT16_MAX,
		},
		{
			/* match all packets with dst ports [54-65280]. */
			.data = {
				.userdata = 2,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 102,
			},
			.src_port_low = 0,
			.src_port_high = UINT16_MAX,
			.dst_port_low = 54,
			.dst_port_high = 65280,
		},
		{
			/* match all packets with dst ports [0-52]. */
			.data = {
				.userdata = 3,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 103,
			},
			.src_port_low = 0,
			.src_port_high = UINT16_MAX,
			.dst_port_low = 0,
			.dst_port_high = 52,
		},
		{
			/* match all packets with dst ports [53]. */
			.data = {
				.userdata = 4,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 99,
			},
			.src_port_low = 0,
			.src_port_high = UINT16_MAX,
			.dst_port_low = 53,
			.dst_port_high = 53,
		},
		{
			/* match all packets with dst ports [65279-65535]. */
			.data = {
				.userdata = 5,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 98,
			},
			.src_port_low = 0,
			.src_port_high = UINT16_MAX,
			.dst_port_low = 65279,
			.dst_port_high = UINT16_MAX,
		},
	};

	static struct ipv4_7tuple test_data[] = {
		{
			.proto = 6,
			.ip_src = RTE_IPV4(10, 1, 1, 1),
			.ip_dst = RTE_IPV4(192, 168, 0, 33),
			.port_dst = 53,
			.allow = 1,
		},
		{
			.proto = 6,
			.ip_src = RTE_IPV4(127, 84, 33, 1),
			.ip_dst = RTE_IPV4(1, 2, 3, 4),
			.port_dst = 65281,
			.allow = 1,
		},
	};

	struct rte_acl_ctx *acx;
	int32_t ret, i, j;
	uint32_t results[RTE_DIM(test_data)];
	const uint8_t *data[RTE_DIM(test_data)];

	acx = rte_acl_create(&acl_param);
	if (acx == NULL) {
		printf("Line %i: Error creating ACL context!\n", __LINE__);
		return -1;
	}

	/* swap all bytes in the data to network order */
	bswap_test_data(test_data, RTE_DIM(test_data), 1);

	/* store pointers to test data */
	for (i = 0; i != RTE_DIM(test_data); i++)
		data[i] = (uint8_t *)&test_data[i];

	for (i = 0; i != RTE_DIM(test_rules); i++) {
		rte_acl_reset(acx);
		ret = test_classify_buid(acx, test_rules, i + 1);
		if (ret != 0) {
			printf("Line %i, iter: %d: "
				"Adding rules to ACL context failed!\n",
				__LINE__, i);
			break;
		}
		ret = rte_acl_classify(acx, data, results,
			RTE_DIM(data), 1);
		if (ret != 0) {
			printf("Line %i, iter: %d: classify failed!\n",
				__LINE__, i);
			break;
		}

		/* check results */
		for (j = 0; j != RTE_DIM(results); j++) {
			if (results[j] != test_data[j].allow) {
				printf("Line %i: Error in allow results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, j, test_data[j].allow,
					results[j]);
				ret = -EINVAL;
			}
		}
	}

	bswap_test_data(test_data, RTE_DIM(test_data), 0);

	rte_acl_free(acx);
	return ret;
}

static void
convert_rule(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	ro->data = ri->data;

	ro->field[RTE_ACL_IPV4VLAN_PROTO_FIELD].value.u8 = ri->proto;
	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD].value.u16 = ri->vlan;
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD].value.u16 = ri->domain;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].value.u32 = ri->src_addr;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].value.u32 = ri->dst_addr;
	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD].value.u16 = ri->src_port_low;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD].value.u16 = ri->dst_port_low;

	ro->field[RTE_ACL_IPV4VLAN_PROTO_FIELD].mask_range.u8 = ri->proto_mask;
	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD].mask_range.u16 = ri->vlan_mask;
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD].mask_range.u16 =
		ri->domain_mask;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32 =
		ri->src_mask_len;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32 = ri->dst_mask_len;
	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD].mask_range.u16 =
		ri->src_port_high;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD].mask_range.u16 =
		ri->dst_port_high;
}

/*
 * Convert IPV4 source and destination from RTE_ACL_FIELD_TYPE_MASK to
 * RTE_ACL_FIELD_TYPE_BITMASK.
 */
static void
convert_rule_1(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	uint32_t v;

	convert_rule(ri, ro);
	v = ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32 =
		RTE_ACL_MASKLEN_TO_BITMASK(v, sizeof(v));
	v = ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32 =
		RTE_ACL_MASKLEN_TO_BITMASK(v, sizeof(v));
}

/*
 * Convert IPV4 source and destination from RTE_ACL_FIELD_TYPE_MASK to
 * RTE_ACL_FIELD_TYPE_RANGE.
 */
static void
convert_rule_2(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	uint32_t hi, lo, mask;

	convert_rule(ri, ro);

	mask = ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32;
	mask = RTE_ACL_MASKLEN_TO_BITMASK(mask, sizeof(mask));
	lo = ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].value.u32 & mask;
	hi = lo + ~mask;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].value.u32 = lo;
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD].mask_range.u32 = hi;

	mask = ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32;
	mask = RTE_ACL_MASKLEN_TO_BITMASK(mask, sizeof(mask));
	lo = ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].value.u32 & mask;
	hi = lo + ~mask;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].value.u32 = lo;
	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD].mask_range.u32 = hi;
}

/*
 * Convert rte_acl_ipv4vlan_rule: swap VLAN and PORTS rule fields.
 */
static void
convert_rule_3(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	struct rte_acl_field t1, t2;

	convert_rule(ri, ro);

	t1 = ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD];
	t2 = ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD];

	ro->field[RTE_ACL_IPV4VLAN_VLAN1_FIELD] =
		ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD];
	ro->field[RTE_ACL_IPV4VLAN_VLAN2_FIELD] =
		ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD];

	ro->field[RTE_ACL_IPV4VLAN_SRCP_FIELD] = t1;
	ro->field[RTE_ACL_IPV4VLAN_DSTP_FIELD] = t2;
}

/*
 * Convert rte_acl_ipv4vlan_rule: swap SRC and DST IPv4 address rules.
 */
static void
convert_rule_4(const struct rte_acl_ipv4vlan_rule *ri,
	struct acl_ipv4vlan_rule *ro)
{
	struct rte_acl_field t;

	convert_rule(ri, ro);

	t = ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD];
	ro->field[RTE_ACL_IPV4VLAN_SRC_FIELD] =
		ro->field[RTE_ACL_IPV4VLAN_DST_FIELD];

	ro->field[RTE_ACL_IPV4VLAN_DST_FIELD] = t;
}

static void
ipv4vlan_config(struct rte_acl_config *cfg,
	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM],
	uint32_t num_categories)
{
	static const struct rte_acl_field_def
		ipv4_defs[RTE_ACL_IPV4VLAN_NUM_FIELDS] = {
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint8_t),
			.field_index = RTE_ACL_IPV4VLAN_PROTO_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PROTO,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_VLAN1_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_VLAN,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_VLAN2_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_VLAN,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = RTE_ACL_IPV4VLAN_SRC_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_SRC,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = RTE_ACL_IPV4VLAN_DST_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_DST,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_SRCP_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PORTS,
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = RTE_ACL_IPV4VLAN_DSTP_FIELD,
			.input_index = RTE_ACL_IPV4VLAN_PORTS,
		},
	};

	memcpy(&cfg->defs, ipv4_defs, sizeof(ipv4_defs));
	cfg->num_fields = RTE_DIM(ipv4_defs);

	cfg->defs[RTE_ACL_IPV4VLAN_PROTO_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PROTO];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_VLAN];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_VLAN] +
		cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].size;
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_SRC];
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_DST];
	cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PORTS];
	cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD].offset =
		layout[RTE_ACL_IPV4VLAN_PORTS] +
		cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].size;

	cfg->num_categories = num_categories;
}

static int
convert_rules(struct rte_acl_ctx *acx,
	void (*convert)(const struct rte_acl_ipv4vlan_rule *,
	struct acl_ipv4vlan_rule *),
	const struct rte_acl_ipv4vlan_rule *rules, uint32_t num)
{
	int32_t rc;
	uint32_t i;
	struct acl_ipv4vlan_rule r;

	for (i = 0; i != num; i++) {
		convert(rules + i, &r);
		rc = rte_acl_add_rules(acx, (struct rte_acl_rule *)&r, 1);
		if (rc != 0) {
			printf("Line %i: Adding rule %u to ACL context "
				"failed with error code: %d\n",
			__LINE__, i, rc);
			return rc;
		}
	}

	return 0;
}

static void
convert_config(struct rte_acl_config *cfg)
{
	ipv4vlan_config(cfg, ipv4_7tuple_layout, RTE_ACL_MAX_CATEGORIES);
}

/*
 * Convert rte_acl_ipv4vlan_rule to use RTE_ACL_FIELD_TYPE_BITMASK.
 */
static void
convert_config_1(struct rte_acl_config *cfg)
{
	ipv4vlan_config(cfg, ipv4_7tuple_layout, RTE_ACL_MAX_CATEGORIES);
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].type = RTE_ACL_FIELD_TYPE_BITMASK;
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].type = RTE_ACL_FIELD_TYPE_BITMASK;
}

/*
 * Convert rte_acl_ipv4vlan_rule to use RTE_ACL_FIELD_TYPE_RANGE.
 */
static void
convert_config_2(struct rte_acl_config *cfg)
{
	ipv4vlan_config(cfg, ipv4_7tuple_layout, RTE_ACL_MAX_CATEGORIES);
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].type = RTE_ACL_FIELD_TYPE_RANGE;
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].type = RTE_ACL_FIELD_TYPE_RANGE;
}

/*
 * Convert rte_acl_ipv4vlan_rule: swap VLAN and PORTS rule definitions.
 */
static void
convert_config_3(struct rte_acl_config *cfg)
{
	struct rte_acl_field_def t1, t2;

	ipv4vlan_config(cfg, ipv4_7tuple_layout, RTE_ACL_MAX_CATEGORIES);

	t1 = cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD];
	t2 = cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD];

	/* swap VLAN1 and SRCP rule definition. */
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD] =
		cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].field_index = t1.field_index;
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN1_FIELD].input_index = t1.input_index;

	/* swap VLAN2 and DSTP rule definition. */
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD] =
		cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD];
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD].field_index = t2.field_index;
	cfg->defs[RTE_ACL_IPV4VLAN_VLAN2_FIELD].input_index = t2.input_index;

	cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].type = t1.type;
	cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].size = t1.size;
	cfg->defs[RTE_ACL_IPV4VLAN_SRCP_FIELD].offset = t1.offset;

	cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD].type = t2.type;
	cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD].size = t2.size;
	cfg->defs[RTE_ACL_IPV4VLAN_DSTP_FIELD].offset = t2.offset;
}

/*
 * Convert rte_acl_ipv4vlan_rule: swap SRC and DST ip address rule definitions.
 */
static void
convert_config_4(struct rte_acl_config *cfg)
{
	struct rte_acl_field_def t;

	ipv4vlan_config(cfg, ipv4_7tuple_layout, RTE_ACL_MAX_CATEGORIES);

	t = cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD];

	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD] =
		cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD];
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].field_index = t.field_index;
	cfg->defs[RTE_ACL_IPV4VLAN_SRC_FIELD].input_index = t.input_index;

	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].type = t.type;
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].size = t.size;
	cfg->defs[RTE_ACL_IPV4VLAN_DST_FIELD].offset = t.offset;
}


static int
build_convert_rules(struct rte_acl_ctx *acx,
	void (*config)(struct rte_acl_config *),
	size_t max_size)
{
	struct rte_acl_config cfg;

	memset(&cfg, 0, sizeof(cfg));
	config(&cfg);
	cfg.max_size = max_size;
	return rte_acl_build(acx, &cfg);
}

static int
test_convert_rules(const char *desc,
	void (*config)(struct rte_acl_config *),
	void (*convert)(const struct rte_acl_ipv4vlan_rule *,
	struct acl_ipv4vlan_rule *))
{
	struct rte_acl_ctx *acx;
	int32_t rc;
	uint32_t i;
	static const size_t mem_sizes[] = {0, -1};

	printf("running %s(%s)\n", __func__, desc);

	acx = rte_acl_create(&acl_param);
	if (acx == NULL) {
		printf("Line %i: Error creating ACL context!\n", __LINE__);
		return -1;
	}

	rc = convert_rules(acx, convert, acl_test_rules,
		RTE_DIM(acl_test_rules));
	if (rc != 0)
		printf("Line %i: Error converting ACL rules!\n", __LINE__);

	for (i = 0; rc == 0 && i != RTE_DIM(mem_sizes); i++) {

		rc = build_convert_rules(acx, config, mem_sizes[i]);
		if (rc != 0) {
			printf("Line %i: Error @ build_convert_rules(%zu)!\n",
				__LINE__, mem_sizes[i]);
			break;
		}

		rc = test_classify_run(acx, acl_test_data,
			RTE_DIM(acl_test_data));
		if (rc != 0)
			printf("%s failed at line %i, max_size=%zu\n",
				__func__, __LINE__, mem_sizes[i]);
	}

	rte_acl_free(acx);
	return rc;
}

static int
test_convert(void)
{
	static const struct {
		const char *desc;
		void (*config)(struct rte_acl_config *);
		void (*convert)(const struct rte_acl_ipv4vlan_rule *,
			struct acl_ipv4vlan_rule *);
	} convert_param[] = {
		{
			"acl_ipv4vlan_tuple",
			convert_config,
			convert_rule,
		},
		{
			"acl_ipv4vlan_tuple, RTE_ACL_FIELD_TYPE_BITMASK type "
			"for IPv4",
			convert_config_1,
			convert_rule_1,
		},
		{
			"acl_ipv4vlan_tuple, RTE_ACL_FIELD_TYPE_RANGE type "
			"for IPv4",
			convert_config_2,
			convert_rule_2,
		},
		{
			"acl_ipv4vlan_tuple: swap VLAN and PORTs order",
			convert_config_3,
			convert_rule_3,
		},
		{
			"acl_ipv4vlan_tuple: swap SRC and DST IPv4 order",
			convert_config_4,
			convert_rule_4,
		},
	};

	uint32_t i;
	int32_t rc;

	for (i = 0; i != RTE_DIM(convert_param); i++) {
		rc = test_convert_rules(convert_param[i].desc,
			convert_param[i].config,
			convert_param[i].convert);
		if (rc != 0) {
			printf("%s for test-case: %s failed, error code: %d;\n",
				__func__, convert_param[i].desc, rc);
			return rc;
		}
	}

	return 0;
}

/*
 * Test wrong layout behavior
 * This test supplies the ACL context with invalid layout, which results in
 * ACL matching the wrong stuff. However, it should match the wrong stuff
 * the right way. We switch around source and destination addresses,
 * source and destination ports, and protocol will point to first byte of
 * destination port.
 */
static int
test_invalid_layout(void)
{
	struct rte_acl_ctx *acx;
	int ret, i;

	uint32_t results[RTE_DIM(invalid_layout_data)];
	const uint8_t *data[RTE_DIM(invalid_layout_data)];

	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM] = {
			/* proto points to destination port's first byte */
			offsetof(struct ipv4_7tuple, port_dst),

			0, /* VLAN not used */

			/* src and dst addresses are swapped */
			offsetof(struct ipv4_7tuple, ip_dst),
			offsetof(struct ipv4_7tuple, ip_src),

			/*
			 * we can't swap ports here, so we will swap
			 * them in the data
			 */
			offsetof(struct ipv4_7tuple, port_src),
	};

	acx = rte_acl_create(&acl_param);
	if (acx == NULL) {
		printf("Line %i: Error creating ACL context!\n", __LINE__);
		return -1;
	}

	/* putting a lot of rules into the context results in greater
	 * coverage numbers. it doesn't matter if they are identical */
	for (i = 0; i < 1000; i++) {
		/* add rules to the context */
		ret = rte_acl_ipv4vlan_add_rules(acx, invalid_layout_rules,
				RTE_DIM(invalid_layout_rules));
		if (ret != 0) {
			printf("Line %i: Adding rules to ACL context failed!\n",
				__LINE__);
			rte_acl_free(acx);
			return -1;
		}
	}

	/* try building the context */
	ret = rte_acl_ipv4vlan_build(acx, layout, 1);
	if (ret != 0) {
		printf("Line %i: Building ACL context failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* swap all bytes in the data to network order */
	bswap_test_data(invalid_layout_data, RTE_DIM(invalid_layout_data), 1);

	/* prepare data */
	for (i = 0; i < (int) RTE_DIM(invalid_layout_data); i++) {
		data[i] = (uint8_t *)&invalid_layout_data[i];
	}

	/* classify tuples */
	ret = rte_acl_classify_alg(acx, data, results,
			RTE_DIM(results), 1, RTE_ACL_CLASSIFY_SCALAR);
	if (ret != 0) {
		printf("Line %i: SSE classify failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	for (i = 0; i < (int) RTE_DIM(results); i++) {
		if (results[i] != invalid_layout_data[i].allow) {
			printf("Line %i: Wrong results at %i "
				"(result=%u, should be %u)!\n",
				__LINE__, i, results[i],
				invalid_layout_data[i].allow);
			goto err;
		}
	}

	/* classify tuples (scalar) */
	ret = rte_acl_classify_alg(acx, data, results, RTE_DIM(results), 1,
		RTE_ACL_CLASSIFY_SCALAR);

	if (ret != 0) {
		printf("Line %i: Scalar classify failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	for (i = 0; i < (int) RTE_DIM(results); i++) {
		if (results[i] != invalid_layout_data[i].allow) {
			printf("Line %i: Wrong results at %i "
				"(result=%u, should be %u)!\n",
				__LINE__, i, results[i],
				invalid_layout_data[i].allow);
			goto err;
		}
	}

	rte_acl_free(acx);

	/* swap data back to cpu order so that next time tests don't fail */
	bswap_test_data(invalid_layout_data, RTE_DIM(invalid_layout_data), 0);

	return 0;
err:

	/* swap data back to cpu order so that next time tests don't fail */
	bswap_test_data(invalid_layout_data, RTE_DIM(invalid_layout_data), 0);

	rte_acl_free(acx);

	return -1;
}

/*
 * Test creating and finding ACL contexts, and adding rules
 */
static int
test_create_find_add(void)
{
	struct rte_acl_param param;
	struct rte_acl_ctx *acx, *acx2, *tmp;
	struct rte_acl_ipv4vlan_rule rules[LEN];

	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM] = {0};

	const char *acx_name = "acx";
	const char *acx2_name = "acx2";
	int i, ret;

	/* create two contexts */
	memcpy(&param, &acl_param, sizeof(param));
	param.max_rule_num = 2;

	param.name = acx_name;
	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: Error creating %s!\n", __LINE__, acx_name);
		return -1;
	}

	param.name = acx2_name;
	acx2 = rte_acl_create(&param);
	if (acx2 == NULL || acx2 == acx) {
		printf("Line %i: Error creating %s!\n", __LINE__, acx2_name);
		rte_acl_free(acx);
		return -1;
	}

	/* try to create third one, with an existing name */
	param.name = acx_name;
	tmp = rte_acl_create(&param);
	if (tmp != acx) {
		printf("Line %i: Creating context with existing name "
			"test failed!\n",
			__LINE__);
		if (tmp)
			rte_acl_free(tmp);
		goto err;
	}

	param.name = acx2_name;
	tmp = rte_acl_create(&param);
	if (tmp != acx2) {
		printf("Line %i: Creating context with existing "
			"name test 2 failed!\n",
			__LINE__);
		if (tmp)
			rte_acl_free(tmp);
		goto err;
	}

	/* try to find existing ACL contexts */
	tmp = rte_acl_find_existing(acx_name);
	if (tmp != acx) {
		printf("Line %i: Finding %s failed!\n", __LINE__, acx_name);
		if (tmp)
			rte_acl_free(tmp);
		goto err;
	}

	tmp = rte_acl_find_existing(acx2_name);
	if (tmp != acx2) {
		printf("Line %i: Finding %s failed!\n", __LINE__, acx2_name);
		if (tmp)
			rte_acl_free(tmp);
		goto err;
	}

	/* try to find non-existing context */
	tmp = rte_acl_find_existing("invalid");
	if (tmp != NULL) {
		printf("Line %i: Non-existent ACL context found!\n", __LINE__);
		goto err;
	}

	/* free context */
	rte_acl_free(acx);


	/* create valid (but severely limited) acx */
	memcpy(&param, &acl_param, sizeof(param));
	param.max_rule_num = LEN;

	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: Error creating %s!\n", __LINE__, param.name);
		goto err;
	}

	/* create dummy acl */
	for (i = 0; i < LEN; i++) {
		memcpy(&rules[i], &acl_rule,
			sizeof(struct rte_acl_ipv4vlan_rule));
		/* skip zero */
		rules[i].data.userdata = i + 1;
		/* one rule per category */
		rules[i].data.category_mask = 1 << i;
	}

	/* try filling up the context */
	ret = rte_acl_ipv4vlan_add_rules(acx, rules, LEN);
	if (ret != 0) {
		printf("Line %i: Adding %i rules to ACL context failed!\n",
				__LINE__, LEN);
		goto err;
	}

	/* try adding to a (supposedly) full context */
	ret = rte_acl_ipv4vlan_add_rules(acx, rules, 1);
	if (ret == 0) {
		printf("Line %i: Adding rules to full ACL context should"
				"have failed!\n", __LINE__);
		goto err;
	}

	/* try building the context */
	ret = rte_acl_ipv4vlan_build(acx, layout, RTE_ACL_MAX_CATEGORIES);
	if (ret != 0) {
		printf("Line %i: Building ACL context failed!\n", __LINE__);
		goto err;
	}

	rte_acl_free(acx);
	rte_acl_free(acx2);

	return 0;
err:
	rte_acl_free(acx);
	rte_acl_free(acx2);
	return -1;
}

/*
 * test various invalid rules
 */
static int
test_invalid_rules(void)
{
	struct rte_acl_ctx *acx;
	int ret;

	struct rte_acl_ipv4vlan_rule rule;

	acx = rte_acl_create(&acl_param);
	if (acx == NULL) {
		printf("Line %i: Error creating ACL context!\n", __LINE__);
		return -1;
	}

	/* test inverted high/low source and destination ports.
	 * originally, there was a problem with memory consumption when using
	 * such rules.
	 */
	/* create dummy acl */
	memcpy(&rule, &acl_rule, sizeof(struct rte_acl_ipv4vlan_rule));
	rule.data.userdata = 1;
	rule.dst_port_low = 0xfff0;
	rule.dst_port_high = 0x0010;

	/* add rules to context and try to build it */
	ret = rte_acl_ipv4vlan_add_rules(acx, &rule, 1);
	if (ret == 0) {
		printf("Line %i: Adding rules to ACL context "
				"should have failed!\n", __LINE__);
		goto err;
	}

	rule.dst_port_low = 0x0;
	rule.dst_port_high = 0xffff;
	rule.src_port_low = 0xfff0;
	rule.src_port_high = 0x0010;

	/* add rules to context and try to build it */
	ret = rte_acl_ipv4vlan_add_rules(acx, &rule, 1);
	if (ret == 0) {
		printf("Line %i: Adding rules to ACL context "
				"should have failed!\n", __LINE__);
		goto err;
	}

	rule.dst_port_low = 0x0;
	rule.dst_port_high = 0xffff;
	rule.src_port_low = 0x0;
	rule.src_port_high = 0xffff;

	rule.dst_mask_len = 33;

	/* add rules to context and try to build it */
	ret = rte_acl_ipv4vlan_add_rules(acx, &rule, 1);
	if (ret == 0) {
		printf("Line %i: Adding rules to ACL context "
				"should have failed!\n", __LINE__);
		goto err;
	}

	rule.dst_mask_len = 0;
	rule.src_mask_len = 33;

	/* add rules to context and try to build it */
	ret = rte_acl_ipv4vlan_add_rules(acx, &rule, 1);
	if (ret == 0) {
		printf("Line %i: Adding rules to ACL context "
				"should have failed!\n", __LINE__);
		goto err;
	}

	rte_acl_free(acx);

	return 0;

err:
	rte_acl_free(acx);

	return -1;
}

/*
 * test functions by passing invalid or
 * non-workable parameters.
 *
 * we do very limited testing of classify functions here
 * because those are performance-critical and
 * thus don't do much parameter checking.
 */
static int
test_invalid_parameters(void)
{
	struct rte_acl_param param;
	struct rte_acl_ctx *acx;
	struct rte_acl_ipv4vlan_rule rule;
	int result;

	uint32_t layout[RTE_ACL_IPV4VLAN_NUM] = {0};


	/**
	 * rte_ac_create()
	 */

	/* NULL param */
	acx = rte_acl_create(NULL);
	if (acx != NULL) {
		printf("Line %i: ACL context creation with NULL param "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* zero rule size */
	memcpy(&param, &acl_param, sizeof(param));
	param.rule_size = 0;

	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: ACL context creation with zero rule len "
				"failed!\n", __LINE__);
		return -1;
	} else
		rte_acl_free(acx);

	/* zero max rule num */
	memcpy(&param, &acl_param, sizeof(param));
	param.max_rule_num = 0;

	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: ACL context creation with zero rule num "
				"failed!\n", __LINE__);
		return -1;
	} else
		rte_acl_free(acx);

	if (rte_eal_has_hugepages()) {
		/* invalid NUMA node */
		memcpy(&param, &acl_param, sizeof(param));
		param.socket_id = RTE_MAX_NUMA_NODES + 1;

		acx = rte_acl_create(&param);
		if (acx != NULL) {
			printf("Line %i: ACL context creation with invalid "
					"NUMA should have failed!\n", __LINE__);
			rte_acl_free(acx);
			return -1;
		}
	}

	/* NULL name */
	memcpy(&param, &acl_param, sizeof(param));
	param.name = NULL;

	acx = rte_acl_create(&param);
	if (acx != NULL) {
		printf("Line %i: ACL context creation with NULL name "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/**
	 * rte_acl_find_existing
	 */

	acx = rte_acl_find_existing(NULL);
	if (acx != NULL) {
		printf("Line %i: NULL ACL context found!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/**
	 * rte_acl_ipv4vlan_add_rules
	 */

	/* initialize everything */
	memcpy(&param, &acl_param, sizeof(param));
	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: ACL context creation failed!\n", __LINE__);
		return -1;
	}

	memcpy(&rule, &acl_rule, sizeof(rule));

	/* NULL context */
	result = rte_acl_ipv4vlan_add_rules(NULL, &rule, 1);
	if (result == 0) {
		printf("Line %i: Adding rules with NULL ACL context "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* NULL rule */
	result = rte_acl_ipv4vlan_add_rules(acx, NULL, 1);
	if (result == 0) {
		printf("Line %i: Adding NULL rule to ACL context "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* zero count (should succeed) */
	result = rte_acl_ipv4vlan_add_rules(acx, &rule, 0);
	if (result != 0) {
		printf("Line %i: Adding 0 rules to ACL context failed!\n",
			__LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* free ACL context */
	rte_acl_free(acx);


	/**
	 * rte_acl_ipv4vlan_build
	 */

	/* reinitialize context */
	memcpy(&param, &acl_param, sizeof(param));
	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: ACL context creation failed!\n", __LINE__);
		return -1;
	}

	/* NULL context */
	result = rte_acl_ipv4vlan_build(NULL, layout, 1);
	if (result == 0) {
		printf("Line %i: Building with NULL context "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* NULL layout */
	result = rte_acl_ipv4vlan_build(acx, NULL, 1);
	if (result == 0) {
		printf("Line %i: Building with NULL layout "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* zero categories (should not fail) */
	result = rte_acl_ipv4vlan_build(acx, layout, 0);
	if (result == 0) {
		printf("Line %i: Building with 0 categories should fail!\n",
			__LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* SSE classify test */

	/* cover zero categories in classify (should not fail) */
	result = rte_acl_classify(acx, NULL, NULL, 0, 0);
	if (result != 0) {
		printf("Line %i: SSE classify with zero categories "
				"failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* cover invalid but positive categories in classify */
	result = rte_acl_classify(acx, NULL, NULL, 0, 3);
	if (result == 0) {
		printf("Line %i: SSE classify with 3 categories "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* scalar classify test */

	/* cover zero categories in classify (should not fail) */
	result = rte_acl_classify_alg(acx, NULL, NULL, 0, 0,
		RTE_ACL_CLASSIFY_SCALAR);
	if (result != 0) {
		printf("Line %i: Scalar classify with zero categories "
				"failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* cover invalid but positive categories in classify */
	result = rte_acl_classify(acx, NULL, NULL, 0, 3);
	if (result == 0) {
		printf("Line %i: Scalar classify with 3 categories "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* free ACL context */
	rte_acl_free(acx);


	/**
	 * make sure void functions don't crash with NULL parameters
	 */

	rte_acl_free(NULL);

	rte_acl_dump(NULL);

	return 0;
}

/**
 * Various tests that don't test much but improve coverage
 */
static int
test_misc(void)
{
	struct rte_acl_param param;
	struct rte_acl_ctx *acx;

	/* create context */
	memcpy(&param, &acl_param, sizeof(param));

	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: Error creating ACL context!\n", __LINE__);
		return -1;
	}

	/* dump context with rules - useful for coverage */
	rte_acl_list_dump();

	rte_acl_dump(acx);

	rte_acl_free(acx);

	return 0;
}

static uint32_t
get_u32_range_max(void)
{
	uint32_t i, max;

	max = 0;
	for (i = 0; i != RTE_DIM(acl_u32_range_test_rules); i++)
		max = RTE_MAX(max, acl_u32_range_test_rules[i].src_mask_len);
	return max;
}

static uint32_t
get_u32_range_min(void)
{
	uint32_t i, min;

	min = UINT32_MAX;
	for (i = 0; i != RTE_DIM(acl_u32_range_test_rules); i++)
		min = RTE_MIN(min, acl_u32_range_test_rules[i].src_addr);
	return min;
}

static const struct rte_acl_ipv4vlan_rule *
find_u32_range_rule(uint32_t val)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(acl_u32_range_test_rules); i++) {
		if (val >= acl_u32_range_test_rules[i].src_addr &&
				val <= acl_u32_range_test_rules[i].src_mask_len)
			return acl_u32_range_test_rules + i;
	}
	return NULL;
}

static void
fill_u32_range_data(struct ipv4_7tuple tdata[], uint32_t start, uint32_t num)
{
	uint32_t i;
	const struct rte_acl_ipv4vlan_rule *r;

	for (i = 0; i != num; i++) {
		tdata[i].ip_src = start + i;
		r = find_u32_range_rule(start + i);
		if (r != NULL)
			tdata[i].allow = r->data.userdata;
	}
}

static int
test_u32_range(void)
{
	int32_t rc;
	uint32_t i, k, max, min;
	struct rte_acl_ctx *acx;
	struct acl_ipv4vlan_rule r;
	struct ipv4_7tuple test_data[64];

	acx = rte_acl_create(&acl_param);
	if (acx == NULL) {
		printf("%s#%i: Error creating ACL context!\n",
			__func__, __LINE__);
		return -1;
	}

	for (i = 0; i != RTE_DIM(acl_u32_range_test_rules); i++) {
		convert_rule(&acl_u32_range_test_rules[i], &r);
		rc = rte_acl_add_rules(acx, (struct rte_acl_rule *)&r, 1);
		if (rc != 0) {
			printf("%s#%i: Adding rule to ACL context "
				"failed with error code: %d\n",
				__func__, __LINE__, rc);
			rte_acl_free(acx);
			return rc;
		}
	}

	rc = build_convert_rules(acx, convert_config_2, 0);
	if (rc != 0) {
		printf("%s#%i Error @ build_convert_rules!\n",
			__func__, __LINE__);
		rte_acl_free(acx);
		return rc;
	}

	max = get_u32_range_max();
	min = get_u32_range_min();

	max = RTE_MAX(max, max + 1);
	min = RTE_MIN(min, min - 1);

	printf("%s#%d starting range test from %u to %u\n",
		__func__, __LINE__, min, max);

	for (i = min; i <= max; i += k) {

		k = RTE_MIN(max - i + 1, RTE_DIM(test_data));

		memset(test_data, 0, sizeof(test_data));
		fill_u32_range_data(test_data, i, k);

		rc = test_classify_run(acx, test_data, k);
		if (rc != 0) {
			printf("%s#%d failed at [%u, %u) interval\n",
				__func__, __LINE__, i, i + k);
			break;
		}
	}

	rte_acl_free(acx);
	return rc;
}

static int
test_acl(void)
{
	if (test_invalid_parameters() < 0)
		return -1;
	if (test_invalid_rules() < 0)
		return -1;
	if (test_create_find_add() < 0)
		return -1;
	if (test_invalid_layout() < 0)
		return -1;
	if (test_misc() < 0)
		return -1;
	if (test_classify() < 0)
		return -1;
	if (test_build_ports_range() < 0)
		return -1;
	if (test_convert() < 0)
		return -1;
	if (test_u32_range() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(acl_autotest, test_acl);
