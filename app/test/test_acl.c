/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <string.h>
#include <errno.h>

#include "test.h"

#ifdef RTE_LIBRTE_ACL

#include <rte_string_fns.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_acl.h>
#include <rte_common.h>

#include "test_acl.h"

#define LEN RTE_ACL_MAX_CATEGORIES

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

/*
 * Test scalar and SSE ACL lookup.
 */
static int
test_classify_run(struct rte_acl_ctx *acx)
{
	int ret, i;
	uint32_t result, count;
	uint32_t results[RTE_DIM(acl_test_data) * RTE_ACL_MAX_CATEGORIES];
	const uint8_t *data[RTE_DIM(acl_test_data)];

	/* swap all bytes in the data to network order */
	bswap_test_data(acl_test_data, RTE_DIM(acl_test_data), 1);

	/* store pointers to test data */
	for (i = 0; i < (int) RTE_DIM(acl_test_data); i++)
		data[i] = (uint8_t *)&acl_test_data[i];

	/**
	 * these will run quite a few times, it's necessary to test code paths
	 * from num=0 to num>8
	 */
	for (count = 0; count < RTE_DIM(acl_test_data); count++) {
		ret = rte_acl_classify(acx, data, results,
				count, RTE_ACL_MAX_CATEGORIES);
		if (ret != 0) {
			printf("Line %i: SSE classify failed!\n", __LINE__);
			goto err;
		}

		/* check if we allow everything we should allow */
		for (i = 0; i < (int) count; i++) {
			result =
				results[i * RTE_ACL_MAX_CATEGORIES + ACL_ALLOW];
			if (result != acl_test_data[i].allow) {
				printf("Line %i: Error in allow results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, i, acl_test_data[i].allow,
					result);
				goto err;
			}
		}

		/* check if we deny everything we should deny */
		for (i = 0; i < (int) count; i++) {
			result = results[i * RTE_ACL_MAX_CATEGORIES + ACL_DENY];
			if (result != acl_test_data[i].deny) {
				printf("Line %i: Error in deny results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, i, acl_test_data[i].deny,
					result);
				goto err;
			}
		}
	}

	/* make a quick check for scalar */
	ret = rte_acl_classify_scalar(acx, data, results,
			RTE_DIM(acl_test_data), RTE_ACL_MAX_CATEGORIES);
	if (ret != 0) {
		printf("Line %i: SSE classify failed!\n", __LINE__);
		goto err;
	}

	/* check if we allow everything we should allow */
	for (i = 0; i < (int) RTE_DIM(acl_test_data); i++) {
		result = results[i * RTE_ACL_MAX_CATEGORIES + ACL_ALLOW];
		if (result != acl_test_data[i].allow) {
			printf("Line %i: Error in allow results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, i, acl_test_data[i].allow,
					result);
			goto err;
		}
	}

	/* check if we deny everything we should deny */
	for (i = 0; i < (int) RTE_DIM(acl_test_data); i++) {
		result = results[i * RTE_ACL_MAX_CATEGORIES + ACL_DENY];
		if (result != acl_test_data[i].deny) {
			printf("Line %i: Error in deny results at %i "
					"(expected %"PRIu32" got %"PRIu32")!\n",
					__LINE__, i, acl_test_data[i].deny,
					result);
			goto err;
		}
	}

	ret = 0;

err:
	/* swap data back to cpu order so that next time tests don't fail */
	bswap_test_data(acl_test_data, RTE_DIM(acl_test_data), 0);
	return ret;
}

static int
test_classify_buid(struct rte_acl_ctx *acx)
{
	int ret;
	const uint32_t layout[RTE_ACL_IPV4VLAN_NUM] = {
			offsetof(struct ipv4_7tuple, proto),
			offsetof(struct ipv4_7tuple, vlan),
			offsetof(struct ipv4_7tuple, ip_src),
			offsetof(struct ipv4_7tuple, ip_dst),
			offsetof(struct ipv4_7tuple, port_src),
	};

	/* add rules to the context */
	ret = rte_acl_ipv4vlan_add_rules(acx, acl_test_rules,
			RTE_DIM(acl_test_rules));
	if (ret != 0) {
		printf("Line %i: Adding rules to ACL context failed!\n",
			__LINE__);
		return ret;
	}

	/* try building the context */
	ret = rte_acl_ipv4vlan_build(acx, layout, RTE_ACL_MAX_CATEGORIES);
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

		ret = test_classify_buid(acx);
		if (ret != 0) {
			printf("Line %i, iter: %d: "
				"Adding rules to ACL context failed!\n",
				__LINE__, i);
			break;
		}

		ret = test_classify_run(acx);
		if (ret != 0) {
			printf("Line %i, iter: %d: %s failed!\n",
				__LINE__, i, __func__);
			break;
		}

		/* reset rules and make sure that classify still works ok. */
		rte_acl_reset_rules(acx);
		ret = test_classify_run(acx);
		if (ret != 0) {
			printf("Line %i, iter: %d: %s failed!\n",
				__LINE__, i, __func__);
			break;
		}
	}

	rte_acl_free(acx);
	return ret;
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
	ret = rte_acl_classify(acx, data, results,
			RTE_DIM(results), 1);
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
	ret = rte_acl_classify_scalar(acx, data, results,
			RTE_DIM(results), 1);
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

	rule.dst_mask_len = 0;
	rule.src_mask_len = 0;
	rule.data.userdata = 0;

	/* try adding this rule (it should fail because userdata is invalid) */
	ret = rte_acl_ipv4vlan_add_rules(acx, &rule, 1);
	if (ret == 0) {
		printf("Line %i: Adding a rule with invalid user data "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
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

	/* invalid NUMA node */
	memcpy(&param, &acl_param, sizeof(param));
	param.socket_id = RTE_MAX_NUMA_NODES + 1;

	acx = rte_acl_create(&param);
	if (acx != NULL) {
		printf("Line %i: ACL context creation with invalid NUMA "
				"should have failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
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

	/* set wrong rule_size so that adding any rules would fail */
	param.rule_size = RTE_ACL_IPV4VLAN_RULE_SZ + 4;
	acx = rte_acl_create(&param);
	if (acx == NULL) {
		printf("Line %i: ACL context creation failed!\n", __LINE__);
		return -1;
	}

	/* try adding a rule with size different from context rule_size */
	result = rte_acl_ipv4vlan_add_rules(acx, &rule, 1);
	if (result == 0) {
		printf("Line %i: Adding an invalid sized rule "
				"should have failed!\n", __LINE__);
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
	result = rte_acl_classify_scalar(acx, NULL, NULL, 0, 0);
	if (result != 0) {
		printf("Line %i: Scalar classify with zero categories "
				"failed!\n", __LINE__);
		rte_acl_free(acx);
		return -1;
	}

	/* cover invalid but positive categories in classify */
	result = rte_acl_classify_scalar(acx, NULL, NULL, 0, 3);
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

int
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

	return 0;
}
#else

int
test_acl(void)
{
	printf("This binary was not compiled with ACL support!\n");
	return 0;
}

#endif /* RTE_LIBRTE_ACL */
