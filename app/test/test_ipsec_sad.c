/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_ipsec_sad(void)
{
	printf("ipsec_sad not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_ipsec_sad.h>
#include <rte_memory.h>

#include "test_xmmt_ops.h"

typedef int32_t (*rte_ipsec_sad_test)(void);

static int32_t test_create_invalid(void);
static int32_t test_find_existing(void);
static int32_t test_multiple_create(void);
static int32_t test_add_invalid(void);
static int32_t test_delete_invalid(void);
static int32_t test_lookup_invalid(void);
static int32_t test_lookup_basic(void);
static int32_t test_lookup_adv(void);
static int32_t test_lookup_order(void);

#define MAX_SA	100000
#define PASS 0
#define SPI	0xdead	/* spi to install */
#define DIP	0xbeef	/* dip to install */
#define SIP	0xf00d	/* sip to install */
#define BAD	0xbad	/* some random value not installed into the table */

/*
 * Check that rte_ipsec_sad_create fails gracefully for incorrect user input
 * arguments
 */
int32_t
test_create_invalid(void)
{
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;

	/* name == NULL */
	sad = rte_ipsec_sad_create(NULL, &config);
	RTE_TEST_ASSERT(sad == NULL,
		"Call succeeded with invalid parameters\n");

	/* max_sa for every type = 0 */
	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = 0;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = 0;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = 0;
	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT(sad == NULL,
		"Call succeeded with invalid parameters\n");
	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;

	/* socket_id < -1 is invalid */
	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.socket_id = -2;
	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT(sad == NULL,
		"Call succeeded with invalid parameters\n");
	config.socket_id = SOCKET_ID_ANY;

	return TEST_SUCCESS;
}

/*
 * Test rte_ipsec_sad_find_existing()
 * Create SAD and try to find it by it's name
 */
int32_t
test_find_existing(void)
{
	const char *name1 = "sad_one";
	const char *name2 = "sad_two";
	struct rte_ipsec_sad *one, *two, *tmp;
	struct rte_ipsec_sad_conf config;

	config.socket_id = SOCKET_ID_ANY;
	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = 0;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = 0;
	one = rte_ipsec_sad_create(name1, &config);
	RTE_TEST_ASSERT_NOT_NULL(one, "Failed to create SAD\n");
	two = rte_ipsec_sad_create(name2, &config);
	RTE_TEST_ASSERT_NOT_NULL(two, "Failed to create SAD\n");

	/* Find non existing */
	tmp = rte_ipsec_sad_find_existing("sad_three");
	RTE_TEST_ASSERT(tmp == NULL,
		"rte_ipsec_sad_find_existing returns invalid SAD\n");

	tmp = rte_ipsec_sad_find_existing(name1);
	RTE_TEST_ASSERT(tmp == one,
		"rte_ipsec_sad_find_existing returns invalid SAD\n");

	tmp = rte_ipsec_sad_find_existing(name2);
	RTE_TEST_ASSERT(tmp == two,
		"rte_ipsec_sad_find_existing returns invalid SAD\n");

	rte_ipsec_sad_destroy(one);
	rte_ipsec_sad_destroy(two);
	return TEST_SUCCESS;
}

/*
 * Create ipsec sad then delete it 10 times
 * Use a slightly different max_sa each time
 */
int32_t
test_multiple_create(void)
{
	int i;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;

	config.socket_id = SOCKET_ID_ANY;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;

	for (i = 0; i < 10; i++) {
		config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA - i;
		sad = rte_ipsec_sad_create(__func__, &config);
		RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");
		rte_ipsec_sad_destroy(sad);
	}
	return TEST_SUCCESS;
}

static int32_t
__test_add_invalid(int ipv6, union rte_ipsec_sad_key *tuple)
{
	int status;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;
	uint64_t tmp;
	void *sa = &tmp;

	/* sad == NULL*/
	status = rte_ipsec_sad_add(NULL, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, sa);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;
	if (ipv6)
		config.flags = RTE_IPSEC_SAD_FLAG_IPV6;

	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");

	/* key == NULL*/
	status = rte_ipsec_sad_add(sad, NULL, RTE_IPSEC_SAD_SPI_DIP_SIP, sa);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	/* len is incorrect*/
	status = rte_ipsec_sad_add(sad, tuple,
		RTE_IPSEC_SAD_SPI_DIP_SIP + 1, sa);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	/* sa == NULL*/
	status = rte_ipsec_sad_add(sad, tuple,
		RTE_IPSEC_SAD_SPI_DIP_SIP, NULL);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	/* sa is not aligned*/
	status = rte_ipsec_sad_add(sad, tuple,
	RTE_IPSEC_SAD_SPI_DIP_SIP, (void *)((uint8_t *)sa + 1));
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	rte_ipsec_sad_destroy(sad);

	return TEST_SUCCESS;
}

/*
 * Check that rte_ipsec_sad_add fails gracefully
 * for incorrect user input arguments
 */
int32_t
test_add_invalid(void)
{
	int status;
	struct rte_ipsec_sadv4_key tuple_v4 = {10, 20, 30};
	struct rte_ipsec_sadv6_key tuple_v6 = {10, {20, }, {30, } };

	status = __test_add_invalid(0, (union rte_ipsec_sad_key *)&tuple_v4);
	if (status != TEST_SUCCESS)
		return status;

	status = __test_add_invalid(1, (union rte_ipsec_sad_key *)&tuple_v6);

	return status;

}

static int32_t
__test_delete_invalid(int ipv6, union rte_ipsec_sad_key *tuple)
{
	int status;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;

	/* sad == NULL*/
	status = rte_ipsec_sad_del(NULL, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;
	if (ipv6)
		config.flags = RTE_IPSEC_SAD_FLAG_IPV6;

	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");

	/* key == NULL*/
	status = rte_ipsec_sad_del(sad, NULL, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	/* len is incorrect */
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP + 1);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	rte_ipsec_sad_destroy(sad);

	return TEST_SUCCESS;
}

/*
 * Check that rte_ipsec_sad_delete fails gracefully for incorrect user input
 * arguments
 */
int32_t
test_delete_invalid(void)
{
	int status;
	struct rte_ipsec_sadv4_key tuple_v4 = {SPI, DIP, SIP};
	struct rte_ipsec_sadv6_key tuple_v6 = {SPI, {0xbe, 0xef, },
			{0xf0, 0x0d, } };

	status = __test_delete_invalid(0, (union rte_ipsec_sad_key *)&tuple_v4);
	if (status != TEST_SUCCESS)
		return status;

	status = __test_delete_invalid(1, (union rte_ipsec_sad_key *)&tuple_v6);

	return status;
}

static int32_t
__test_lookup_invalid(int ipv6, union rte_ipsec_sad_key *tuple)
{
	int status;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;
	const union rte_ipsec_sad_key *key_arr[] = {tuple};
	void *sa[1];

	status = rte_ipsec_sad_lookup(NULL, key_arr, sa, 1);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;
	if (ipv6)
		config.flags = RTE_IPSEC_SAD_FLAG_IPV6;

	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");

	status = rte_ipsec_sad_lookup(sad, NULL, sa, 1);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, NULL, 1);
	RTE_TEST_ASSERT(status < 0,
		"Call succeeded with invalid parameters\n");

	rte_ipsec_sad_destroy(sad);

	return TEST_SUCCESS;
}

/*
 * Check that rte_ipsec_sad_lookup fails gracefully for incorrect user input
 * arguments
 */
int32_t
test_lookup_invalid(void)
{
	int status;
	struct rte_ipsec_sadv4_key tuple_v4 = {10, 20, 30};
	struct rte_ipsec_sadv6_key tuple_v6 = {10, {20, }, {30, } };

	status = __test_lookup_invalid(0,
			(union rte_ipsec_sad_key *)&tuple_v4);
	if (status != TEST_SUCCESS)
		return status;

	status = __test_lookup_invalid(1,
			(union rte_ipsec_sad_key *)&tuple_v6);

	return status;
}

static int32_t
__test_lookup_basic(int ipv6, union rte_ipsec_sad_key *tuple,
	union rte_ipsec_sad_key *tuple_1)
{
	int status;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;
	const union rte_ipsec_sad_key *key_arr[] = {tuple};

	uint64_t tmp;
	void *sa[1];

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;
	if (ipv6)
		config.flags = RTE_IPSEC_SAD_FLAG_IPV6;

	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 1);
	RTE_TEST_ASSERT((status == 0) && (sa[0] == NULL),
		"Lookup returns an unexpected result\n");

	sa[0] = &tmp;
	status = rte_ipsec_sad_add(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY, sa[0]);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 1);
	RTE_TEST_ASSERT((status == 1) && (sa[0] == &tmp),
		"Lookup returns an unexpected result\n");

	key_arr[0] = tuple_1;
	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 1);
	RTE_TEST_ASSERT((status == 1) && (sa[0] == &tmp),
		"Lookup returns an unexpected result\n");
	key_arr[0] = tuple;

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 1);
	RTE_TEST_ASSERT((status == 0) && (sa[0] == NULL),
		"Lookup returns an unexpected result\n");

	rte_ipsec_sad_destroy(sad);

	return TEST_SUCCESS;
}

/*
 * Lookup missing key, then add it as RTE_IPSEC_SAD_SPI_ONLY, lookup it again,
 * lookup different key with the same SPI, then delete it and repeat lookup
 */
int32_t
test_lookup_basic(void)
{
	int status;
	struct rte_ipsec_sadv4_key tuple_v4 = {SPI, DIP, SIP};
	struct rte_ipsec_sadv4_key tuple_v4_1 = {SPI, BAD, BAD};
	struct rte_ipsec_sadv6_key tuple_v6 = {SPI, {0xbe, 0xef, },
			{0xf0, 0x0d, } };
	struct rte_ipsec_sadv6_key tuple_v6_1 = {SPI, {0x0b, 0xad, },
			{0x0b, 0xad, } };

	status = __test_lookup_basic(0, (union rte_ipsec_sad_key *)&tuple_v4,
			(union rte_ipsec_sad_key *)&tuple_v4_1);
	if (status != TEST_SUCCESS)
		return status;

	status = __test_lookup_basic(1, (union rte_ipsec_sad_key *)&tuple_v6,
			(union rte_ipsec_sad_key *)&tuple_v6_1);

	return status;
}

static int32_t
__test_lookup_adv(int ipv6, union rte_ipsec_sad_key *tuple,
	const union rte_ipsec_sad_key **key_arr)
{
	int status;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;
	uint64_t tmp1, tmp2, tmp3;
	void *install_sa;
	void *sa[4];

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;
	if (ipv6)
		config.flags = RTE_IPSEC_SAD_FLAG_IPV6;
	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");

	/* lookup with empty table */
	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 0, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	/* lookup with one RTE_IPSEC_SAD_SPI_ONLY rule */
	install_sa = &tmp1;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_ONLY, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY);
	RTE_TEST_ASSERT(status == 0, "Failde to delete a rule\n");

	/* lookup with one RTE_IPSEC_SAD_SPI_DIP rule */
	install_sa = &tmp2;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 2, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	/* lookup with one RTE_IPSEC_SAD_SPI_DIP_SIP rule */
	install_sa = &tmp3;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 1, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	/* lookup with two RTE_IPSEC_SAD_ONLY and RTE_IPSEC_SAD_DIP rules */
	install_sa = &tmp1;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_ONLY, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");
	install_sa = &tmp2;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	/* lookup with two RTE_IPSEC_SAD_ONLY and RTE_IPSEC_SAD_DIP_SIP rules */
	install_sa = &tmp1;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_ONLY, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");
	install_sa = &tmp3;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	/* lookup with two RTE_IPSEC_SAD_DIP and RTE_IPSEC_SAD_DIP_SIP rules */
	install_sa = &tmp2;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");
	install_sa = &tmp3;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 2, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	/*
	 * lookup with three RTE_IPSEC_SAD_DIP, RTE_IPSEC_SAD_DIP and
	 * RTE_IPSEC_SAD_DIP_SIP rules
	 */
	install_sa = &tmp1;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_ONLY, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");
	install_sa = &tmp2;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");
	install_sa = &tmp3;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 4);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[3] == NULL,
		"Lookup returns an unexpected result\n");

	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	rte_ipsec_sad_destroy(sad);

	return TEST_SUCCESS;
}

/*
 * Lookup different keys in a table with:
 *  - RTE_IPSEC_SAD_SPI_ONLY
 *  - RTE_IPSEC_SAD_SPI_DIP
 *  - RTE_IPSEC_SAD_SPI_SIP
 *  - RTE_IPSEC_SAD_SPI_ONLY/RTE_IPSEC_SAD_SPI_DIP
 *  - RTE_IPSEC_SAD_SPI_ONLY/RTE_IPSEC_SAD_SPI_DIP_SIP
 *  - RTE_IPSEC_SAD_SPI_DIP/RTE_IPSEC_SAD_SPI_DIP_SIP
 *  - RTE_IPSEC_SAD_SPI_ONLY/RTE_IPSEC_SAD_SPI_DIP/RTE_IPSEC_SAD_SPI_DIP_SIP
 * length of rule installed.
 */
int32_t
test_lookup_adv(void)
{
	int status;
	/* key to install*/
	struct rte_ipsec_sadv4_key tuple_v4 = {SPI, DIP, SIP};
	struct rte_ipsec_sadv4_key tuple_v4_1 = {SPI, DIP, BAD};
	struct rte_ipsec_sadv4_key tuple_v4_2 = {SPI, BAD, SIP};
	struct rte_ipsec_sadv4_key tuple_v4_3 = {BAD, DIP, SIP};

	/* key to install*/
	struct rte_ipsec_sadv6_key tuple_v6 = {SPI, {0xbe, 0xef, },
			{0xf0, 0x0d, } };
	struct rte_ipsec_sadv6_key tuple_v6_1 = {SPI, {0xbe, 0xef, },
			{0x0b, 0xad, } };
	struct rte_ipsec_sadv6_key tuple_v6_2 = {SPI, {0x0b, 0xad, },
			{0xf0, 0x0d, } };
	struct rte_ipsec_sadv6_key tuple_v6_3 = {BAD, {0xbe, 0xef, },
			{0xf0, 0x0d, } };

	const union rte_ipsec_sad_key *key_arr[] = {
				(union rte_ipsec_sad_key *)&tuple_v4,
				(union rte_ipsec_sad_key *)&tuple_v4_1,
				(union rte_ipsec_sad_key *)&tuple_v4_2,
				(union rte_ipsec_sad_key *)&tuple_v4_3
					};

	status = __test_lookup_adv(0, (union rte_ipsec_sad_key *)&tuple_v4,
			key_arr);
	if (status != TEST_SUCCESS)
		return status;
	key_arr[0] = (union rte_ipsec_sad_key *)&tuple_v6;
	key_arr[1] = (union rte_ipsec_sad_key *)&tuple_v6_1;
	key_arr[2] = (union rte_ipsec_sad_key *)&tuple_v6_2;
	key_arr[3] = (union rte_ipsec_sad_key *)&tuple_v6_3;
	status = __test_lookup_adv(1, (union rte_ipsec_sad_key *)&tuple_v6,
			key_arr);

	return status;
}


static int32_t
__test_lookup_order(int ipv6, union rte_ipsec_sad_key *tuple,
	union rte_ipsec_sad_key *tuple_1, union rte_ipsec_sad_key *tuple_2)
{
	int status;
	struct rte_ipsec_sad *sad = NULL;
	struct rte_ipsec_sad_conf config;
	const union rte_ipsec_sad_key *key_arr[] = {tuple, tuple_1, tuple_2,};
	uint64_t tmp1, tmp2, tmp3;
	void *install_sa;
	void *sa[3];

	config.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP] = MAX_SA;
	config.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = MAX_SA;
	config.socket_id = SOCKET_ID_ANY;
	config.flags = 0;
	if (ipv6)
		config.flags = RTE_IPSEC_SAD_FLAG_IPV6;
	sad = rte_ipsec_sad_create(__func__, &config);
	RTE_TEST_ASSERT_NOT_NULL(sad, "Failed to create SAD\n");

	/* install RTE_IPSEC_SAD_SPI_ONLY */
	install_sa = &tmp1;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_ONLY, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp1,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");

	/* add RTE_IPSEC_SAD_SPI_DIP */
	install_sa = &tmp2;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");

	/* add RTE_IPSEC_SAD_SPI_DIP_SIP */
	install_sa = &tmp3;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");

	/* delete RTE_IPSEC_SAD_SPI_ONLY */
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_ONLY);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 2, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");

	/* delete RTE_IPSEC_SAD_SPI_DIP */
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 1, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");

	/* delete RTE_IPSEC_SAD_SPI_DIP_SIP */
	status = rte_ipsec_sad_del(sad, tuple, RTE_IPSEC_SAD_SPI_DIP_SIP);
	RTE_TEST_ASSERT(status == 0, "Failed to delete a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 0, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");

	/* add RTE_IPSEC_SAD_SPI_DIP_SIP */
	install_sa = &tmp3;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP_SIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 1, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == NULL,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");

	/* add RTE_IPSEC_SAD_SPI_DIP */
	install_sa = &tmp2;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_DIP, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 2, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == NULL,
		"Lookup returns an unexpected result\n");

	/* add RTE_IPSEC_SAD_SPI_ONLY */
	install_sa = &tmp1;
	status = rte_ipsec_sad_add(sad, tuple,
			RTE_IPSEC_SAD_SPI_ONLY, install_sa);
	RTE_TEST_ASSERT(status == 0, "Failed to add a rule\n");

	status = rte_ipsec_sad_lookup(sad, key_arr, sa, 3);
	RTE_TEST_ASSERT(status == 3, "Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[0] == &tmp3,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[1] == &tmp2,
		"Lookup returns an unexpected result\n");
	RTE_TEST_ASSERT(sa[2] == &tmp1,
		"Lookup returns an unexpected result\n");

	rte_ipsec_sad_destroy(sad);
	return TEST_SUCCESS;
}

/*
 * Check an order of add and delete
 */
int32_t
test_lookup_order(void)
{
	int status;
	/* key to install*/
	struct rte_ipsec_sadv4_key tuple_v4 = {SPI, DIP, SIP};
	struct rte_ipsec_sadv4_key tuple_v4_1 = {SPI, DIP, BAD};
	struct rte_ipsec_sadv4_key tuple_v4_2 = {SPI, BAD, SIP};
	/* key to install*/
	struct rte_ipsec_sadv6_key tuple_v6 = {SPI, {0xbe, 0xef, },
			{0xf0, 0x0d, } };
	struct rte_ipsec_sadv6_key tuple_v6_1 = {SPI, {0xbe, 0xef, },
			{0x0b, 0xad, } };
	struct rte_ipsec_sadv6_key tuple_v6_2 = {SPI, {0x0b, 0xad, },
			{0xf0, 0x0d, } };

	status = __test_lookup_order(0, (union rte_ipsec_sad_key *)&tuple_v4,
			(union rte_ipsec_sad_key *)&tuple_v4_1,
			(union rte_ipsec_sad_key *)&tuple_v4_2);
	if (status != TEST_SUCCESS)
		return status;

	status = __test_lookup_order(1, (union rte_ipsec_sad_key *)&tuple_v6,
			(union rte_ipsec_sad_key *)&tuple_v6_1,
			(union rte_ipsec_sad_key *)&tuple_v6_2);
	return status;
}

static struct unit_test_suite ipsec_sad_tests = {
	.suite_name = "ipsec sad autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_create_invalid),
		TEST_CASE(test_find_existing),
		TEST_CASE(test_multiple_create),
		TEST_CASE(test_add_invalid),
		TEST_CASE(test_delete_invalid),
		TEST_CASE(test_lookup_invalid),
		TEST_CASE(test_lookup_basic),
		TEST_CASE(test_lookup_adv),
		TEST_CASE(test_lookup_order),
		TEST_CASES_END()
	}
};

static int
test_ipsec_sad(void)
{
	return unit_test_suite_runner(&ipsec_sad_tests);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(ipsec_sad_autotest, test_ipsec_sad);
