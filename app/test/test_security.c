/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 */

#include <rte_errno.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_security.h>
#include <rte_security_driver.h>

/* Before including rte_test.h file you can define
 * RTE_TEST_TRACE_FAILURE(_file, _line, _func) macro to better trace/debug test
 * failures. Mostly useful in development phase.
 */
#ifndef RTE_TEST_TRACE_FAILURE
#define RTE_TEST_TRACE_FAILURE(_file, _line, _func) \
	RTE_LOG(DEBUG, EAL, "in %s:%d %s\n", _file, _line, _func)
#endif

#include <rte_test.h>
#include "test.h"

/**
 * Security
 * =======
 *
 * Basic unit tests of the librte_security API.
 *
 * Structure of the file:
 * - macros for making tests more readable;
 * - mockup structures and functions for rte_security_ops;
 * - test suite and test cases setup and teardown functions;
 * - tests functions;
 * - declaration of testcases.
 */


/**
 * Macros
 *
 * Set of macros for making tests easier to read.
 */

/**
 * Verify condition inside mocked up function.
 * Mockup function cannot return a test error, so the failure
 * of assertion increases counter and print logs.
 * The counter can be verified later to check if test case should fail.
 *
 * @param   fail_counter	fail counter
 * @param   cond	condition expected to be true
 * @param   msg	printf style formatting string for custom message
 */
#define MOCK_TEST_ASSERT(fail_counter, cond, msg, ...) do {		\
	if (!(cond)) {							\
		fail_counter++;						\
		RTE_LOG(DEBUG, EAL, "Test assert %s line %d failed: "	\
				msg "\n", __func__, __LINE__,		\
				 ##__VA_ARGS__);			\
		RTE_TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);	\
	}								\
} while (0)

/**
 * Verify equality condition inside mocked up function.
 * Mockup function cannot return a test error, so the failure
 * of assertion increases counter and print logs.
 * The counter can be verified later to check if test case should fail.
 *
 * @param   fail_counter	fail counter
 * @param   a	first value of comparison
 * @param   b	second value of comparison
 * @param   msg	printf style formatting string for custom message
 */
#define MOCK_TEST_ASSERT_EQUAL(fail_counter, a, b, msg, ...)	\
	MOCK_TEST_ASSERT(fail_counter, (a) == (b), msg, ##__VA_ARGS__)

/**
 * Verify not null condition inside mocked up function.
 * Mockup function cannot return a test error, so the failure
 * of assertion increases counter and print logs.
 * The counter can be verified later to check if test case should fail.
 *
 * @param   fail_counter	fail counter
 * @param   val	value expected not to be NULL
 * @param   msg	printf style formatting string for custom message
 */
#define MOCK_TEST_ASSERT_NOT_NULL(fail_counter, val, msg, ...)	\
	MOCK_TEST_ASSERT(fail_counter, (val) != NULL, msg, ##__VA_ARGS__)


/**
 * Verify if parameter of the mocked up function matches expected value.
 * The expected value is stored in data structure in the field matching
 * parameter name.
 *
 * @param   data	structure with expected values
 * @param   parameter	name of the parameter (both field and parameter name)
 * @param   spec	printf style spec for parameter
 */
#define MOCK_TEST_ASSERT_PARAMETER(data, parameter, spec)		\
	MOCK_TEST_ASSERT_EQUAL(data.failed, data.parameter, parameter,	\
			"Expecting parameter %s to be " spec		\
			" but it's " spec, RTE_STR(parameter),		\
			data.parameter, parameter)

/**
 * Wrap for MOCK_TEST_ASSERT_PARAMETER macro for pointer type parameters.
 *
 * @param   data	structure with expected values
 * @param   parameter	name of the parameter (both field and parameter name)
 */
#define MOCK_TEST_ASSERT_POINTER_PARAMETER(data, parameter)	\
	MOCK_TEST_ASSERT_PARAMETER(data, parameter, "%p")

/**
 * Wrap for MOCK_TEST_ASSERT_PARAMETER macro for uint64_t type parameters.
 *
 * @param   data	structure with expected values
 * @param   parameter	name of the parameter (both field and parameter name)
 */
#define MOCK_TEST_ASSERT_U64_PARAMETER(data, parameter)	\
	MOCK_TEST_ASSERT_PARAMETER(data, parameter, "%" PRIu64)

/**
 * Verify number of calls of the mocked up function
 * and check if there were any fails during execution.
 * The fails statistics inside mocked up functions are collected
 * as "failed" field in mockup structures.
 *
 * @param   mock_data	structure with statistics (called, failed)
 * @param   exp_calls	expected number of mockup function calls
 */
#define TEST_ASSERT_MOCK_CALLS(mock_data, exp_calls) do {		\
	TEST_ASSERT_EQUAL(exp_calls, mock_data.called,			\
			"Expecting sub op to be called %d times, "	\
			"but it's called %d times",			\
			exp_calls, mock_data.called);			\
	TEST_ASSERT_EQUAL(0, mock_data.failed,				\
			"Expecting sub op asserts not to fail, "	\
			"but they're failed %d times",			\
			mock_data.failed);				\
} while (0)

/**
 * Assert tested function result match expected value
 *
 * @param   f_name	name of tested function
 * @param   f_ret	value returned by the function
 * @param   exp_ret	expected returned value
 * @param   fmt		printf style format for returned value
 */
#define TEST_ASSERT_MOCK_FUNCTION_CALL_RET(f_name, f_ret, exp_ret, fmt)	\
	TEST_ASSERT_EQUAL(exp_ret, f_ret, "Expecting " RTE_STR(f_name)	\
			" to return " fmt ", but it returned " fmt	\
			"\n", exp_ret, f_ret)

/**
 * Assert tested function result is not NULL
 *
 * @param   f_name	name of tested function
 * @param   f_ret	value returned by the function
 */
#define TEST_ASSERT_MOCK_FUNCTION_CALL_NOT_NULL(f_name, f_ret)		\
	TEST_ASSERT_NOT_NULL(f_ret, "Expecting " RTE_STR(f_name)	\
			" to return not NULL\n")

/**
 * Verify that sess_cnt counter value matches expected
 *
 * @param   expected_sessions_count	expected counter value
 */
#define TEST_ASSERT_SESSION_COUNT(expected_sessions_count) do {		\
	struct security_unittest_params *ut_params = &unittest_params;	\
	TEST_ASSERT_EQUAL(expected_sessions_count,			\
			ut_params->ctx.sess_cnt,			\
			"Expecting session counter to be %u,"		\
			" but it's %u",	expected_sessions_count,	\
			ut_params->ctx.sess_cnt);			\
} while (0)

/**
 * Verify usage of mempool by checking if number of allocated objects matches
 * expectations. The mempool is used to manage objects for sessions data.
 * A single object is acquired from mempool during session_create
 * and put back in session_destroy.
 *
 * @param   expected_mempool_usage	expected number of used mempool objects
 */
#define TEST_ASSERT_MEMPOOL_USAGE(expected_mempool_usage) do {		\
	struct security_testsuite_params *ts_params = &testsuite_params;\
	unsigned int mempool_usage;					\
	mempool_usage = rte_mempool_in_use_count(			\
			ts_params->session_mpool);			\
	TEST_ASSERT_EQUAL(expected_mempool_usage, mempool_usage,	\
			"Expecting %u mempool allocations, "		\
			"but there are %u allocated objects",		\
			expected_mempool_usage, mempool_usage);		\
} while (0)

/**
 * Verify usage of mempool by checking if number of allocated objects matches
 * expectations. The mempool is used to manage objects for sessions priv data.
 * A single object is acquired from mempool during session_create
 * and put back in session_destroy.
 *
 * @param   expected_priv_mp_usage	expected number of used priv mp objects
 */
#define TEST_ASSERT_PRIV_MP_USAGE(expected_priv_mp_usage) do {		\
	struct security_testsuite_params *ts_params = &testsuite_params;\
	unsigned int priv_mp_usage;					\
	priv_mp_usage = rte_mempool_in_use_count(			\
			ts_params->session_priv_mpool);			\
	TEST_ASSERT_EQUAL(expected_priv_mp_usage, priv_mp_usage,	\
			"Expecting %u priv mempool allocations, "	\
			"but there are %u allocated objects",		\
			expected_priv_mp_usage, priv_mp_usage);		\
} while (0)

/**
 * Mockup structures and functions for rte_security_ops;
 *
 * Set of structures for controlling mockup functions calls.
 * Every mockup function X has its corresponding X_data structure
 * and an instance of that structure X_exp.
 * Structure contains parameters that a mockup function is expected
 * to be called with, a value to return (.ret) and 2 statistics:
 * .called (number of times the mockup function was called)
 * and .failed (number of assertion fails during mockup function call).
 *
 * Mockup functions verify that the parameters they are called with match
 * expected values. The expected values should be stored in corresponding
 * structures prior to mockup functions call. Every failure of such
 * verification increases .failed counter. Every call of mockup function
 * increases .called counter. Function returns value stored in .ret field
 * of the structure.
 * In case of some parameters in some functions the expected value is unknown
 * and cannot be detrmined prior to call. Such parameters are stored
 * in structure and can be compared or analyzed later in test case code.
 *
 * Below structures and functions follow the rules just described.
 * Additional remarks and exceptions are added in comments.
 */

/**
 * session_create mockup
 *
 * Verified parameters: device, conf, mp.
 * Saved, not verified parameters: sess.
 */
static struct mock_session_create_data {
	void *device;
	struct rte_security_session_conf *conf;
	struct rte_security_session *sess;
	struct rte_mempool *mp;
	struct rte_mempool *priv_mp;

	int ret;

	int called;
	int failed;
} mock_session_create_exp = {NULL, NULL, NULL, NULL, NULL, 0, 0, 0};

static int
mock_session_create(void *device,
		struct rte_security_session_conf *conf,
		struct rte_security_session *sess,
		struct rte_mempool *priv_mp)
{
	void *sess_priv;
	int ret;

	mock_session_create_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_create_exp, device);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_create_exp, conf);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_create_exp, priv_mp);

	if (mock_session_create_exp.ret == 0) {
		ret = rte_mempool_get(priv_mp, &sess_priv);
		TEST_ASSERT_EQUAL(0, ret,
			"priv mempool does not have enough objects");

		set_sec_session_private_data(sess, sess_priv);
		mock_session_create_exp.sess = sess;
	}

	return mock_session_create_exp.ret;
}

/**
 * session_update mockup
 *
 * Verified parameters: device, sess, conf.
 */
static struct mock_session_update_data {
	void *device;
	struct rte_security_session *sess;
	struct rte_security_session_conf *conf;

	int ret;

	int called;
	int failed;
} mock_session_update_exp = {NULL, NULL, NULL, 0, 0, 0};

static int
mock_session_update(void *device,
		struct rte_security_session *sess,
		struct rte_security_session_conf *conf)
{
	mock_session_update_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_update_exp, device);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_update_exp, sess);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_update_exp, conf);

	return mock_session_update_exp.ret;
}

/**
 * session_get_size mockup
 *
 * Verified parameters: device.
 */
static struct mock_session_get_size_data {
	void *device;

	unsigned int ret;

	int called;
	int failed;
} mock_session_get_size_exp = {NULL, 0U, 0, 0};

static unsigned int
mock_session_get_size(void *device)
{
	mock_session_get_size_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_get_size_exp, device);

	return mock_session_get_size_exp.ret;
}

/**
 * session_stats_get mockup
 *
 * Verified parameters: device, sess, stats.
 */
static struct mock_session_stats_get_data {
	void *device;
	struct rte_security_session *sess;
	struct rte_security_stats *stats;

	int ret;

	int called;
	int failed;
} mock_session_stats_get_exp = {NULL, NULL, NULL, 0, 0, 0};

static int
mock_session_stats_get(void *device,
		struct rte_security_session *sess,
		struct rte_security_stats *stats)
{
	mock_session_stats_get_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_stats_get_exp, device);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_stats_get_exp, sess);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_stats_get_exp, stats);

	return mock_session_stats_get_exp.ret;
}

/**
 * session_destroy mockup
 *
 * Verified parameters: device, sess.
 */
static struct mock_session_destroy_data {
	void *device;
	struct rte_security_session *sess;

	int ret;

	int called;
	int failed;
} mock_session_destroy_exp = {NULL, NULL, 0, 0, 0};

static int
mock_session_destroy(void *device, struct rte_security_session *sess)
{
	void *sess_priv = get_sec_session_private_data(sess);

	mock_session_destroy_exp.called++;
	if ((mock_session_destroy_exp.ret == 0) && (sess_priv != NULL)) {
		rte_mempool_put(rte_mempool_from_obj(sess_priv), sess_priv);
		set_sec_session_private_data(sess, NULL);
	}
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_destroy_exp, device);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_session_destroy_exp, sess);

	return mock_session_destroy_exp.ret;
}

/**
 * set_pkt_metadata mockup
 *
 * Verified parameters: device, sess, m, params.
 */
static struct mock_set_pkt_metadata_data {
	void *device;
	struct rte_security_session *sess;
	struct rte_mbuf *m;
	void *params;

	int ret;

	int called;
	int failed;
} mock_set_pkt_metadata_exp = {NULL, NULL, NULL, NULL, 0, 0, 0};

static int
mock_set_pkt_metadata(void *device,
		struct rte_security_session *sess,
		struct rte_mbuf *m,
		void *params)
{
	mock_set_pkt_metadata_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_set_pkt_metadata_exp, device);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_set_pkt_metadata_exp, sess);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_set_pkt_metadata_exp, m);
	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_set_pkt_metadata_exp, params);

	return mock_set_pkt_metadata_exp.ret;
}

/**
 * get_userdata mockup
 *
 * Verified parameters: device, md.
 * The userdata parameter works as an output parameter, so a passed address
 * is verified not to be NULL and filled with userdata stored in structure.
 */
static struct mock_get_userdata_data {
	void *device;
	uint64_t md;
	void *userdata;

	int ret;

	int called;
	int failed;
} mock_get_userdata_exp = {NULL, 0UL, NULL, 0, 0, 0};

static int
mock_get_userdata(void *device,
		uint64_t md,
		void **userdata)
{
	mock_get_userdata_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_get_userdata_exp, device);
	MOCK_TEST_ASSERT_U64_PARAMETER(mock_get_userdata_exp, md);

	MOCK_TEST_ASSERT_NOT_NULL(mock_get_userdata_exp.failed,
			userdata,
			"Expecting parameter userdata not to be NULL but it's %p",
			userdata);
	*userdata = mock_get_userdata_exp.userdata;

	return mock_get_userdata_exp.ret;
}

/**
 * capabilities_get mockup
 *
 * Verified parameters: device.
 */
static struct mock_capabilities_get_data {
	void *device;

	struct rte_security_capability *ret;

	int called;
	int failed;
} mock_capabilities_get_exp = {NULL, NULL, 0, 0};

static const struct rte_security_capability *
mock_capabilities_get(void *device)
{
	mock_capabilities_get_exp.called++;

	MOCK_TEST_ASSERT_POINTER_PARAMETER(mock_capabilities_get_exp, device);

	return mock_capabilities_get_exp.ret;
}

/**
 * empty_ops
 *
 * is an empty security operations set (all function pointers set to NULL)
 */
struct rte_security_ops empty_ops = { NULL };

/**
 * mock_ops
 *
 * is a security operations set using mockup functions
 */
struct rte_security_ops mock_ops = {
	.session_create = mock_session_create,
	.session_update = mock_session_update,
	.session_get_size = mock_session_get_size,
	.session_stats_get = mock_session_stats_get,
	.session_destroy = mock_session_destroy,
	.set_pkt_metadata = mock_set_pkt_metadata,
	.get_userdata = mock_get_userdata,
	.capabilities_get = mock_capabilities_get,
};


/**
 * Test suite and test cases setup and teardown functions.
 */

/**
 * struct security_testsuite_params defines parameters initialized once
 * for whole tests suite.
 * Currently the only stored parameter is session_mpool a mempool created
 * once in testsuite_setup and released in testsuite_teardown.
 * The instance of this structure is stored in testsuite_params variable.
 */
static struct security_testsuite_params {
	struct rte_mempool *session_mpool;
	struct rte_mempool *session_priv_mpool;
} testsuite_params = { NULL };

/**
 * struct security_unittest_params defines parameters initialized
 * for every test case. The parameters are initialized in ut_setup
 * or ut_setup_with_session (depending on the testcase)
 * and released in ut_teardown.
 * The instance of this structure is stored in unittest_params variable.
 */
static struct security_unittest_params {
	struct rte_security_ctx ctx;
	struct rte_security_session_conf conf;
	struct rte_security_session *sess;
} unittest_params = {
	.ctx = {
		.device = NULL,
		.ops = &mock_ops,
		.sess_cnt = 0,
	},
	.sess = NULL,
};

#define SECURITY_TEST_MEMPOOL_NAME "SecurityTestMp"
#define SECURITY_TEST_PRIV_MEMPOOL_NAME "SecurityTestPrivMp"
#define SECURITY_TEST_MEMPOOL_SIZE 15
#define SECURITY_TEST_SESSION_OBJ_SZ sizeof(struct rte_security_session)
#define SECURITY_TEST_SESSION_PRIV_OBJ_SZ 64

/**
 * testsuite_setup initializes whole test suite parameters.
 * It creates a new mempool used in all test cases
 * and verifies if it properly created.
 */
static int
testsuite_setup(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	ts_params->session_mpool = rte_mempool_create(
			SECURITY_TEST_MEMPOOL_NAME,
			SECURITY_TEST_MEMPOOL_SIZE,
			SECURITY_TEST_SESSION_OBJ_SZ,
			0, 0, NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
	TEST_ASSERT_NOT_NULL(ts_params->session_mpool,
			"Cannot create mempool %s\n", rte_strerror(rte_errno));

	ts_params->session_priv_mpool = rte_mempool_create(
			SECURITY_TEST_PRIV_MEMPOOL_NAME,
			SECURITY_TEST_MEMPOOL_SIZE,
			SECURITY_TEST_SESSION_PRIV_OBJ_SZ,
			0, 0, NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
	if (ts_params->session_priv_mpool == NULL) {
		RTE_LOG(ERR, USER1, "TestCase %s() line %d failed (null): "
				"Cannot create priv mempool %s\n",
				__func__, __LINE__, rte_strerror(rte_errno));
		rte_mempool_free(ts_params->session_mpool);
		ts_params->session_mpool = NULL;
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

/**
 * testsuite_teardown releases test suite wide parameters.
 */
static void
testsuite_teardown(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	if (ts_params->session_mpool) {
		rte_mempool_free(ts_params->session_mpool);
		ts_params->session_mpool = NULL;
	}
	if (ts_params->session_priv_mpool) {
		rte_mempool_free(ts_params->session_priv_mpool);
		ts_params->session_priv_mpool = NULL;
	}
}

/**
 * ut_setup initializes test case parameters to default values.
 * It resets also any .called and .failed statistics of mockup functions
 * usage.
 */
static int
ut_setup(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.device = NULL;
	ut_params->ctx.ops = &mock_ops;
	ut_params->ctx.sess_cnt = 0;
	ut_params->sess = NULL;

	mock_session_create_exp.called = 0;
	mock_session_update_exp.called = 0;
	mock_session_get_size_exp.called = 0;
	mock_session_stats_get_exp.called = 0;
	mock_session_destroy_exp.called = 0;
	mock_set_pkt_metadata_exp.called = 0;
	mock_get_userdata_exp.called = 0;
	mock_capabilities_get_exp.called = 0;

	mock_session_create_exp.failed = 0;
	mock_session_update_exp.failed = 0;
	mock_session_get_size_exp.failed = 0;
	mock_session_stats_get_exp.failed = 0;
	mock_session_destroy_exp.failed = 0;
	mock_set_pkt_metadata_exp.failed = 0;
	mock_get_userdata_exp.failed = 0;
	mock_capabilities_get_exp.failed = 0;

	return TEST_SUCCESS;
}

/**
 * destroy_session_with_check is a helper function releasing session
 * created with rte_security_session_create and stored in test case parameters.
 * It's used both to release sessions created in test cases' bodies
 * which are assigned to ut_params->sess
 * as well as sessions created in ut_setup_with_session.
 */
static int
destroy_session_with_check(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	if (ut_params->sess != NULL) {
		/* Assure that mockup function for destroy operation is set. */
		ut_params->ctx.ops = &mock_ops;

		mock_session_destroy_exp.device = NULL;
		mock_session_destroy_exp.sess = ut_params->sess;
		mock_session_destroy_exp.ret = 0;
		mock_session_destroy_exp.called = 0;
		mock_session_destroy_exp.failed = 0;

		int ret = rte_security_session_destroy(&ut_params->ctx,
				ut_params->sess);
		TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
				ret, 0, "%d");
		TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 1);

		ut_params->sess = NULL;
	}
	return TEST_SUCCESS;
}

/**
 * ut_teardown releases test case parameters.
 */
static void
ut_teardown(void)
{
	destroy_session_with_check();
}

/**
 * ut_setup_with_session initializes test case parameters by
 * - calling standard ut_setup,
 * - creating a session that can be used in test case.
 */
static int
ut_setup_with_session(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct rte_security_session *sess;

	int ret = ut_setup();
	if (ret != TEST_SUCCESS)
		return ret;

	mock_session_create_exp.device = NULL;
	mock_session_create_exp.conf = &ut_params->conf;
	mock_session_create_exp.mp = ts_params->session_mpool;
	mock_session_create_exp.priv_mp = ts_params->session_priv_mpool;
	mock_session_create_exp.ret = 0;

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_NOT_NULL(rte_security_session_create,
			sess);
	TEST_ASSERT_EQUAL(sess, mock_session_create_exp.sess,
			"Expecting session_create to be called with %p sess"
			" parameter, but it's called %p sess parameter",
			sess, mock_session_create_exp.sess);
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 1);

	/*
	 * Store created session in test case parameters, so it can be released
	 * after test case in ut_teardown by destroy_session_with_check.
	 */
	ut_params->sess = sess;

	return TEST_SUCCESS;
}


/**
 * Test functions
 *
 * Each test function is related to a single test case.
 * They are arranged by tested rte_security API function
 * and by rte_security execution paths sequence in code.
 */

/**
 * rte_security_session_create tests
 */

/**
 * Test execution of rte_security_session_create with NULL instance
 */
static int
test_session_create_inv_context(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *sess;

	sess = rte_security_session_create(NULL, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create with invalid
 * security operations structure (NULL)
 */
static int
test_session_create_inv_context_ops(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *sess;

	ut_params->ctx.ops = NULL;

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create with empty
 * security operations
 */
static int
test_session_create_inv_context_ops_fun(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *sess;

	ut_params->ctx.ops = &empty_ops;

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create with NULL conf parameter
 */
static int
test_session_create_inv_configuration(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *sess;

	sess = rte_security_session_create(&ut_params->ctx, NULL,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create with NULL session
 * mempool
 */
static int
test_session_create_inv_mempool(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct rte_security_session *sess;

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			NULL, ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create with NULL session
 * priv mempool
 */
static int
test_session_create_inv_sess_priv_mempool(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct rte_security_session *sess;

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool, NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create in case when mempool
 * is fully used and no object can be got from it
 */
static int
test_session_create_mempool_empty(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *tmp[SECURITY_TEST_MEMPOOL_SIZE];
	void *tmp1[SECURITY_TEST_MEMPOOL_SIZE];
	struct rte_security_session *sess;

	/* Get all available objects from mempool. */
	int i, ret;
	for (i = 0; i < SECURITY_TEST_MEMPOOL_SIZE; ++i) {
		ret = rte_mempool_get(ts_params->session_mpool,
				(void **)(&tmp[i]));
		TEST_ASSERT_EQUAL(0, ret,
				"Expect getting %d object from mempool"
				" to succeed", i);
		ret = rte_mempool_get(ts_params->session_priv_mpool,
				(void **)(&tmp1[i]));
		TEST_ASSERT_EQUAL(0, ret,
				"Expect getting %d object from priv mempool"
				" to succeed", i);
	}
	TEST_ASSERT_MEMPOOL_USAGE(SECURITY_TEST_MEMPOOL_SIZE);
	TEST_ASSERT_PRIV_MP_USAGE(SECURITY_TEST_MEMPOOL_SIZE);

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(SECURITY_TEST_MEMPOOL_SIZE);
	TEST_ASSERT_PRIV_MP_USAGE(SECURITY_TEST_MEMPOOL_SIZE);
	TEST_ASSERT_SESSION_COUNT(0);

	/* Put objects back to the pool. */
	for (i = 0; i < SECURITY_TEST_MEMPOOL_SIZE; ++i) {
		rte_mempool_put(ts_params->session_mpool,
				(void *)(tmp[i]));
		rte_mempool_put(ts_params->session_priv_mpool,
				(tmp1[i]));
	}
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create when session_create
 * security operation fails
 */
static int
test_session_create_ops_failure(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *sess;

	mock_session_create_exp.device = NULL;
	mock_session_create_exp.conf = &ut_params->conf;
	mock_session_create_exp.mp = ts_params->session_mpool;
	mock_session_create_exp.priv_mp = ts_params->session_priv_mpool;
	mock_session_create_exp.ret = -1;	/* Return failure status. */

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_create,
			sess, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 1);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_create in successful execution path
 */
static int
test_session_create_success(void)
{
	struct security_testsuite_params *ts_params = &testsuite_params;
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_session *sess;

	mock_session_create_exp.device = NULL;
	mock_session_create_exp.conf = &ut_params->conf;
	mock_session_create_exp.mp = ts_params->session_mpool;
	mock_session_create_exp.priv_mp = ts_params->session_priv_mpool;
	mock_session_create_exp.ret = 0;	/* Return success status. */

	sess = rte_security_session_create(&ut_params->ctx, &ut_params->conf,
			ts_params->session_mpool,
			ts_params->session_priv_mpool);
	TEST_ASSERT_MOCK_FUNCTION_CALL_NOT_NULL(rte_security_session_create,
			sess);
	TEST_ASSERT_EQUAL(sess, mock_session_create_exp.sess,
			"Expecting session_create to be called with %p sess"
			" parameter, but it's called %p sess parameter",
			sess, mock_session_create_exp.sess);
	TEST_ASSERT_MOCK_CALLS(mock_session_create_exp, 1);
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	/*
	 * Store created session in test case parameters, so it can be released
	 * after test case in ut_teardown by destroy_session_with_check.
	 */
	ut_params->sess = sess;

	return TEST_SUCCESS;
}


/**
 * rte_security_session_update tests
 */

/**
 * Test execution of rte_security_session_update with NULL instance
 */
static int
test_session_update_inv_context(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	int ret = rte_security_session_update(NULL, ut_params->sess,
			&ut_params->conf);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_update with invalid
 * security operations structure (NULL)
 */
static int
test_session_update_inv_context_ops(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = NULL;

	int ret = rte_security_session_update(&ut_params->ctx, ut_params->sess,
			&ut_params->conf);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_update with empty
 * security operations
 */
static int
test_session_update_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = &empty_ops;

	int ret = rte_security_session_update(&ut_params->ctx, ut_params->sess,
			&ut_params->conf);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, -ENOTSUP, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_update with NULL conf parameter
 */
static int
test_session_update_inv_configuration(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	int ret = rte_security_session_update(&ut_params->ctx, ut_params->sess,
			NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_update with NULL sess parameter
 */
static int
test_session_update_inv_session(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	int ret = rte_security_session_update(&ut_params->ctx, NULL,
			&ut_params->conf);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_update when session_update
 * security operation fails
 */
static int
test_session_update_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_session_update_exp.device = NULL;
	mock_session_update_exp.sess = ut_params->sess;
	mock_session_update_exp.conf = &ut_params->conf;
	mock_session_update_exp.ret = -1;	/* Return failure status. */

	int ret = rte_security_session_update(&ut_params->ctx, ut_params->sess,
			&ut_params->conf);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, -1, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_update in successful execution path
 */
static int
test_session_update_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_session_update_exp.device = NULL;
	mock_session_update_exp.sess = ut_params->sess;
	mock_session_update_exp.conf = &ut_params->conf;
	mock_session_update_exp.ret = 0;	/* Return success status. */

	int ret = rte_security_session_update(&ut_params->ctx, ut_params->sess,
			&ut_params->conf);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_update,
			ret, 0, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_update_exp, 1);

	return TEST_SUCCESS;
}


/**
 * rte_security_session_get_size tests
 */

/**
 * Test execution of rte_security_session_get_size with NULL instance
 */
static int
test_session_get_size_inv_context(void)
{
	unsigned int ret = rte_security_session_get_size(NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_get_size,
			ret, 0, "%u");
	TEST_ASSERT_MOCK_CALLS(mock_session_get_size_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_get_size with invalid
 * security operations structure (NULL)
 */
static int
test_session_get_size_inv_context_ops(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = NULL;

	unsigned int ret = rte_security_session_get_size(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_get_size,
			ret, 0, "%u");
	TEST_ASSERT_MOCK_CALLS(mock_session_get_size_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_get_size with empty
 * security operations
 */
static int
test_session_get_size_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = &empty_ops;

	unsigned int ret = rte_security_session_get_size(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_get_size,
			ret, 0, "%u");
	TEST_ASSERT_MOCK_CALLS(mock_session_get_size_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_get_size when session_get_size
 * security operation fails
 */
static int
test_session_get_size_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_session_get_size_exp.device = NULL;
	mock_session_get_size_exp.ret = 0;

	unsigned int ret = rte_security_session_get_size(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_get_size,
			ret, 0, "%u");
	TEST_ASSERT_MOCK_CALLS(mock_session_get_size_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_get_size in successful execution path
 */
static int
test_session_get_size_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_session_get_size_exp.device = NULL;
	mock_session_get_size_exp.ret = 1024;

	unsigned int ret = rte_security_session_get_size(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_get_size,
			ret, 1024U, "%u");
	TEST_ASSERT_MOCK_CALLS(mock_session_get_size_exp, 1);

	return TEST_SUCCESS;
}


/**
 * rte_security_session_stats_get tests
 */

/**
 * Test execution of rte_security_session_stats_get with NULL instance
 */
static int
test_session_stats_get_inv_context(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_stats stats;

	int ret = rte_security_session_stats_get(NULL, ut_params->sess, &stats);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_stats_get,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_stats_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_stats_get with invalid
 * security operations structure (NULL)
 */
static int
test_session_stats_get_inv_context_ops(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_stats stats;
	ut_params->ctx.ops = NULL;

	int ret = rte_security_session_stats_get(&ut_params->ctx,
			ut_params->sess, &stats);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_stats_get,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_stats_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_stats_get with empty
 * security operations
 */
static int
test_session_stats_get_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_stats stats;
	ut_params->ctx.ops = &empty_ops;

	int ret = rte_security_session_stats_get(&ut_params->ctx,
			ut_params->sess, &stats);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_stats_get,
			ret, -ENOTSUP, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_stats_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_stats_get with NULL stats parameter
 */
static int
test_session_stats_get_inv_stats(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	int ret = rte_security_session_stats_get(&ut_params->ctx,
			ut_params->sess, NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_stats_get,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_stats_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_stats_get when session_stats_get
 * security operation fails
 */
static int
test_session_stats_get_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_stats stats;

	mock_session_stats_get_exp.device = NULL;
	mock_session_stats_get_exp.sess = ut_params->sess;
	mock_session_stats_get_exp.stats = &stats;
	mock_session_stats_get_exp.ret = -1;

	int ret = rte_security_session_stats_get(&ut_params->ctx,
			ut_params->sess, &stats);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_stats_get,
			ret, -1, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_stats_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_stats_get in successful execution
 * path
 */
static int
test_session_stats_get_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_stats stats;

	mock_session_stats_get_exp.device = NULL;
	mock_session_stats_get_exp.sess = ut_params->sess;
	mock_session_stats_get_exp.stats = &stats;
	mock_session_stats_get_exp.ret = 0;

	int ret = rte_security_session_stats_get(&ut_params->ctx,
			ut_params->sess, &stats);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_stats_get,
			ret, 0, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_stats_get_exp, 1);

	return TEST_SUCCESS;
}


/**
 * rte_security_session_destroy tests
 */

/**
 * Test execution of rte_security_session_destroy with NULL instance
 */
static int
test_session_destroy_inv_context(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	int ret = rte_security_session_destroy(NULL, ut_params->sess);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_destroy with invalid
 * security operations structure (NULL)
 */
static int
test_session_destroy_inv_context_ops(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = NULL;

	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	int ret = rte_security_session_destroy(&ut_params->ctx,
			ut_params->sess);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_destroy with empty
 * security operations
 */
static int
test_session_destroy_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = &empty_ops;

	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	int ret = rte_security_session_destroy(&ut_params->ctx,
			ut_params->sess);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
			ret, -ENOTSUP, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_destroy with NULL sess parameter
 */
static int
test_session_destroy_inv_session(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	int ret = rte_security_session_destroy(&ut_params->ctx, NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 0);
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_destroy when session_destroy
 * security operation fails
 */
static int
test_session_destroy_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_session_destroy_exp.device = NULL;
	mock_session_destroy_exp.sess = ut_params->sess;
	mock_session_destroy_exp.ret = -1;

	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	int ret = rte_security_session_destroy(&ut_params->ctx,
			ut_params->sess);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
			ret, -1, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 1);
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_session_destroy in successful execution path
 */
static int
test_session_destroy_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_session_destroy_exp.device = NULL;
	mock_session_destroy_exp.sess = ut_params->sess;
	mock_session_destroy_exp.ret = 0;
	TEST_ASSERT_MEMPOOL_USAGE(1);
	TEST_ASSERT_PRIV_MP_USAGE(1);
	TEST_ASSERT_SESSION_COUNT(1);

	int ret = rte_security_session_destroy(&ut_params->ctx,
			ut_params->sess);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_session_destroy,
			ret, 0, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_session_destroy_exp, 1);
	TEST_ASSERT_MEMPOOL_USAGE(0);
	TEST_ASSERT_PRIV_MP_USAGE(0);
	TEST_ASSERT_SESSION_COUNT(0);

	/*
	 * Remove session from test case parameters, so it won't be destroyed
	 * during test case teardown.
	 */
	ut_params->sess = NULL;

	return TEST_SUCCESS;
}


/**
 * rte_security_set_pkt_metadata tests
 */

/**
 * Test execution of rte_security_set_pkt_metadata with NULL instance
 */
static int
test_set_pkt_metadata_inv_context(void)
{
#ifdef RTE_DEBUG
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf m;
	int params;

	int ret = rte_security_set_pkt_metadata(NULL, ut_params->sess, &m,
			&params);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_set_pkt_metadata,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_set_pkt_metadata_exp, 0);

	return TEST_SUCCESS;
#else
	return TEST_SKIPPED;
#endif
}

/**
 * Test execution of rte_security_set_pkt_metadata with invalid
 * security operations structure (NULL)
 */
static int
test_set_pkt_metadata_inv_context_ops(void)
{
#ifdef RTE_DEBUG
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf m;
	int params;
	ut_params->ctx.ops = NULL;

	int ret = rte_security_set_pkt_metadata(&ut_params->ctx,
			ut_params->sess, &m, &params);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_set_pkt_metadata,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_set_pkt_metadata_exp, 0);

	return TEST_SUCCESS;
#else
	return TEST_SKIPPED;
#endif
}

/**
 * Test execution of rte_security_set_pkt_metadata with empty
 * security operations
 */
static int
test_set_pkt_metadata_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf m;
	int params;
	ut_params->ctx.ops = &empty_ops;

	int ret = rte_security_set_pkt_metadata(&ut_params->ctx,
			ut_params->sess, &m, &params);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_set_pkt_metadata,
			ret, -ENOTSUP, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_set_pkt_metadata_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_set_pkt_metadata with NULL sess parameter
 */
static int
test_set_pkt_metadata_inv_session(void)
{
#ifdef RTE_DEBUG
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf m;
	int params;

	int ret = rte_security_set_pkt_metadata(&ut_params->ctx, NULL,
			&m, &params);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_set_pkt_metadata,
			ret, -EINVAL, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_set_pkt_metadata_exp, 0);

	return TEST_SUCCESS;
#else
	return TEST_SKIPPED;
#endif
}

/**
 * Test execution of rte_security_set_pkt_metadata when set_pkt_metadata
 * security operation fails
 */
static int
test_set_pkt_metadata_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf m;
	int params;

	mock_set_pkt_metadata_exp.device = NULL;
	mock_set_pkt_metadata_exp.sess = ut_params->sess;
	mock_set_pkt_metadata_exp.m = &m;
	mock_set_pkt_metadata_exp.params = &params;
	mock_set_pkt_metadata_exp.ret = -1;

	int ret = rte_security_set_pkt_metadata(&ut_params->ctx,
			ut_params->sess, &m, &params);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_set_pkt_metadata,
			ret, -1, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_set_pkt_metadata_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_set_pkt_metadata in successful execution path
 */
static int
test_set_pkt_metadata_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf m;
	int params;

	mock_set_pkt_metadata_exp.device = NULL;
	mock_set_pkt_metadata_exp.sess = ut_params->sess;
	mock_set_pkt_metadata_exp.m = &m;
	mock_set_pkt_metadata_exp.params = &params;
	mock_set_pkt_metadata_exp.ret = 0;

	int ret = rte_security_set_pkt_metadata(&ut_params->ctx,
			ut_params->sess, &m, &params);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_set_pkt_metadata,
			ret, 0, "%d");
	TEST_ASSERT_MOCK_CALLS(mock_set_pkt_metadata_exp, 1);

	return TEST_SUCCESS;
}


/**
 * rte_security_get_userdata tests
 */

/**
 * Test execution of rte_security_get_userdata with NULL instance
 */
static int
test_get_userdata_inv_context(void)
{
#ifdef RTE_DEBUG
	uint64_t md = 0xDEADBEEF;

	void *ret = rte_security_get_userdata(NULL, md);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_get_userdata,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_get_userdata_exp, 0);

	return TEST_SUCCESS;
#else
	return TEST_SKIPPED;
#endif
}

/**
 * Test execution of rte_security_get_userdata with invalid
 * security operations structure (NULL)
 */
static int
test_get_userdata_inv_context_ops(void)
{
#ifdef RTE_DEBUG
	struct security_unittest_params *ut_params = &unittest_params;
	uint64_t md = 0xDEADBEEF;
	ut_params->ctx.ops = NULL;

	void *ret = rte_security_get_userdata(&ut_params->ctx, md);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_get_userdata,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_get_userdata_exp, 0);

	return TEST_SUCCESS;
#else
	return TEST_SKIPPED;
#endif
}

/**
 * Test execution of rte_security_get_userdata with empty
 * security operations
 */
static int
test_get_userdata_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	uint64_t md = 0xDEADBEEF;
	ut_params->ctx.ops = &empty_ops;

	void *ret = rte_security_get_userdata(&ut_params->ctx, md);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_get_userdata,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_get_userdata_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_get_userdata when get_userdata
 * security operation fails
 */
static int
test_get_userdata_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	uint64_t md = 0xDEADBEEF;
	void *userdata = (void *)0x7E577E57;

	mock_get_userdata_exp.device = NULL;
	mock_get_userdata_exp.md = md;
	mock_get_userdata_exp.userdata = userdata;
	mock_get_userdata_exp.ret = -1;

	void *ret = rte_security_get_userdata(&ut_params->ctx, md);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_get_userdata,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_get_userdata_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_get_userdata in successful execution path
 */
static int
test_get_userdata_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	uint64_t md = 0xDEADBEEF;
	void *userdata = (void *)0x7E577E57;

	mock_get_userdata_exp.device = NULL;
	mock_get_userdata_exp.md = md;
	mock_get_userdata_exp.userdata = userdata;
	mock_get_userdata_exp.ret = 0;

	void *ret = rte_security_get_userdata(&ut_params->ctx, md);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_get_userdata,
			ret, userdata, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_get_userdata_exp, 1);

	return TEST_SUCCESS;
}


/**
 * rte_security_capabilities_get tests
 */

/**
 * Test execution of rte_security_capabilities_get with NULL instance
 */
static int
test_capabilities_get_inv_context(void)
{
	const struct rte_security_capability *ret;
	ret = rte_security_capabilities_get(NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capabilities_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capabilities_get with invalid
 * security operations structure (NULL)
 */
static int
test_capabilities_get_inv_context_ops(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = NULL;

	const struct rte_security_capability *ret;
	ret = rte_security_capabilities_get(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capabilities_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capabilities_get with empty
 * security operations
 */
static int
test_capabilities_get_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	ut_params->ctx.ops = &empty_ops;

	const struct rte_security_capability *ret;
	ret = rte_security_capabilities_get(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capabilities_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capabilities_get when capabilities_get
 * security operation fails
 */
static int
test_capabilities_get_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = NULL;

	const struct rte_security_capability *ret;
	ret = rte_security_capabilities_get(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capabilities_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capabilities_get in successful execution path
 */
static int
test_capabilities_get_success(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability capabilities;

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = &capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capabilities_get(&ut_params->ctx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capabilities_get,
			ret, &capabilities, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}


/**
 * rte_security_capability_get tests
 */

/**
 * Test execution of rte_security_capability_get with NULL instance
 */
static int
test_capability_get_inv_context(void)
{
	struct rte_security_capability_idx idx;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(NULL, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get with invalid
 * security operations structure (NULL)
 */
static int
test_capability_get_inv_context_ops(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx;
	ut_params->ctx.ops = NULL;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get with empty
 * security operations
 */
static int
test_capability_get_inv_context_ops_fun(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx;
	ut_params->ctx.ops = &empty_ops;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get with NULL idx parameter
 */
static int
test_capability_get_inv_idx(void)
{
	struct security_unittest_params *ut_params = &unittest_params;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, NULL);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 0);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities_get
 * security operation fails
 */
static int
test_capability_get_ops_failure(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx;

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = NULL;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * is empty (contains only RTE_SECURITY_ACTION_TYPE_NONE ending entry)
 */
static int
test_capability_get_empty_table(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx;
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching action
 */
static int
test_capability_get_no_matching_action(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching protocol
 */
static int
test_capability_get_no_matching_protocol(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_MACSEC,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when macsec protocol
 * is searched and capabilities table contain proper entry.
 * However macsec records search is not supported in rte_security.
 */
static int
test_capability_get_no_support_for_macsec(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_MACSEC,
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_MACSEC,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching ipsec proto field
 */
static int
test_capability_get_ipsec_mismatch_proto(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_AH,
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching ipsec mode field
 */
static int
test_capability_get_ipsec_mismatch_mode(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching ipsec direction field
 */
static int
test_capability_get_ipsec_mismatch_dir(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * contains matching ipsec entry
 */
static int
test_capability_get_ipsec_match(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
			.ipsec = {
				.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
				.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
				.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, &capabilities[1], "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching pdcp domain field
 */
static int
test_capability_get_pdcp_mismatch_domain(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		.pdcp = {
			.domain = RTE_SECURITY_PDCP_MODE_CONTROL,
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_PDCP,
			.pdcp = {
				.domain = RTE_SECURITY_PDCP_MODE_DATA,
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * contains matching pdcp entry
 */
static int
test_capability_get_pdcp_match(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		.pdcp = {
			.domain = RTE_SECURITY_PDCP_MODE_CONTROL,
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_PDCP,
			.pdcp = {
				.domain = RTE_SECURITY_PDCP_MODE_CONTROL,
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, &capabilities[1], "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * does not contain entry with matching DOCSIS direction field
 */
static int
test_capability_get_docsis_mismatch_direction(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
		.docsis = {
			.direction = RTE_SECURITY_DOCSIS_DOWNLINK
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
			.docsis = {
				.direction = RTE_SECURITY_DOCSIS_UPLINK
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, NULL, "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Test execution of rte_security_capability_get when capabilities table
 * contains matching DOCSIS entry
 */
static int
test_capability_get_docsis_match(void)
{
	struct security_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx idx = {
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
		.docsis = {
			.direction = RTE_SECURITY_DOCSIS_UPLINK
		},
	};
	struct rte_security_capability capabilities[] = {
		{
			.action = RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
			.docsis = {
				.direction = RTE_SECURITY_DOCSIS_UPLINK
			},
		},
		{
			.action = RTE_SECURITY_ACTION_TYPE_NONE,
		},
	};

	mock_capabilities_get_exp.device = NULL;
	mock_capabilities_get_exp.ret = capabilities;

	const struct rte_security_capability *ret;
	ret = rte_security_capability_get(&ut_params->ctx, &idx);
	TEST_ASSERT_MOCK_FUNCTION_CALL_RET(rte_security_capability_get,
			ret, &capabilities[1], "%p");
	TEST_ASSERT_MOCK_CALLS(mock_capabilities_get_exp, 1);

	return TEST_SUCCESS;
}

/**
 * Declaration of testcases
 */
static struct unit_test_suite security_testsuite  = {
	.suite_name = "generic security",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_inv_context),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_inv_context_ops),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_inv_configuration),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_inv_mempool),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_inv_sess_priv_mempool),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_mempool_empty),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_ops_failure),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_session_create_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_inv_configuration),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_inv_session),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_update_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_get_size_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_get_size_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_get_size_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_get_size_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_get_size_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_stats_get_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_stats_get_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_stats_get_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_stats_get_inv_stats),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_stats_get_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_stats_get_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_destroy_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_destroy_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_destroy_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_destroy_inv_session),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_destroy_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_session_destroy_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_set_pkt_metadata_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_set_pkt_metadata_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_set_pkt_metadata_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_set_pkt_metadata_inv_session),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_set_pkt_metadata_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_set_pkt_metadata_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_get_userdata_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_get_userdata_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_get_userdata_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_get_userdata_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_get_userdata_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capabilities_get_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capabilities_get_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capabilities_get_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capabilities_get_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capabilities_get_success),

		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_inv_context),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_inv_context_ops),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_inv_context_ops_fun),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_inv_idx),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_ops_failure),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_empty_table),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_no_matching_action),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_no_matching_protocol),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_no_support_for_macsec),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_ipsec_mismatch_proto),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_ipsec_mismatch_mode),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_ipsec_mismatch_dir),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_ipsec_match),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_pdcp_mismatch_domain),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_pdcp_match),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_docsis_mismatch_direction),
		TEST_CASE_ST(ut_setup_with_session, ut_teardown,
				test_capability_get_docsis_match),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_security(void)
{
	rte_log_set_global_level(RTE_LOG_DEBUG);
	rte_log_set_level(RTE_LOGTYPE_EAL, RTE_LOG_DEBUG);

	return unit_test_suite_runner(&security_testsuite);
}

REGISTER_TEST_COMMAND(security_autotest, test_security);
