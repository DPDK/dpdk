/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_cycles.h>

#include <rte_bus_vdev.h>

#include <rte_bbdev.h>
#include <rte_bbdev_op.h>
#include <rte_bbdev_pmd.h>

#include "main.h"


#define BBDEV_NAME_NULL          ("bbdev_null")

struct bbdev_testsuite_params {
	struct rte_bbdev_queue_conf qconf;
};

static struct bbdev_testsuite_params testsuite_params;

static uint8_t null_dev_id;

static int
testsuite_setup(void)
{
	uint8_t nb_devs;
	int ret;
	char buf[RTE_BBDEV_NAME_MAX_LEN];

	/* Create test device */
	snprintf(buf, sizeof(buf), "%s_unittest", BBDEV_NAME_NULL);
	ret = rte_vdev_init(buf, NULL);
	TEST_ASSERT(ret == 0, "Failed to create instance of pmd: %s", buf);

	nb_devs = rte_bbdev_count();
	TEST_ASSERT(nb_devs != 0, "No devices found");

	/* Most recently created device is our device */
	null_dev_id = nb_devs - 1;

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	char buf[RTE_BBDEV_NAME_MAX_LEN];

	snprintf(buf, sizeof(buf), "%s_unittest", BBDEV_NAME_NULL);
	rte_vdev_uninit(buf);
}

static int
ut_setup(void)
{
	struct bbdev_testsuite_params *ts_params = &testsuite_params;
	uint8_t num_queues;

	/* Valid queue configuration */
	ts_params->qconf.priority = 0;
	ts_params->qconf.socket = SOCKET_ID_ANY;
	ts_params->qconf.deferred_start = 1;

	num_queues = 1;
	TEST_ASSERT_SUCCESS(rte_bbdev_setup_queues(null_dev_id, num_queues,
			SOCKET_ID_ANY), "Failed to setup queues for bbdev %u",
			0);

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_bbdev_start(null_dev_id),
			"Failed to start bbdev %u", 0);

	return TEST_SUCCESS;
}

static void
ut_teardown(void)
{
	rte_bbdev_close(null_dev_id);
}

static int
test_bbdev_configure_invalid_dev_id(void)
{
	uint8_t dev_id;
	uint8_t num_queues;

	num_queues = 1;
	for (dev_id = 0; dev_id < RTE_BBDEV_MAX_DEVS; dev_id++) {
		if (!rte_bbdev_is_valid(dev_id)) {
			TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id,
					num_queues, SOCKET_ID_ANY),
					"Failed test for rte_bbdev_setup_queues: "
					"invalid dev_num %u", dev_id);
			TEST_ASSERT(rte_bbdev_intr_enable(dev_id) == -ENODEV,
					"Failed test for rte_bbdev_intr_enable: "
					"invalid dev_num %u", dev_id);
			break;
		}
	}

	return TEST_SUCCESS;
}

static int
test_bbdev_configure_invalid_num_queues(void)
{
	struct rte_bbdev_info info;
	uint8_t dev_id, num_devs;
	uint8_t num_queues;
	int return_value;

	TEST_ASSERT((num_devs = rte_bbdev_count()) >= 1,
			"Need at least %d devices for test", 1);

	/* valid num_queues values */
	num_queues = 8;

	/* valid dev_id values */
	dev_id = null_dev_id;

	/* Stop the device in case it's started so it can be configured */
	rte_bbdev_stop(dev_id);

	TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id, 0, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"invalid num_queues %d", 0);

	TEST_ASSERT_SUCCESS(rte_bbdev_setup_queues(dev_id, num_queues,
			SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"invalid dev_num %u", dev_id);

	TEST_ASSERT_FAIL(return_value = rte_bbdev_info_get(dev_id, NULL),
			 "Failed test for rte_bbdev_info_get: "
			 "returned value:%i", return_value);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_info_get(dev_id, &info),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value:%i", return_value);

	TEST_ASSERT(info.num_queues == num_queues,
			"Failed test for rte_bbdev_info_get: "
			"invalid num_queues:%u", info.num_queues);

	num_queues = info.drv.max_num_queues;
	TEST_ASSERT_SUCCESS(rte_bbdev_setup_queues(dev_id, num_queues,
			SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"invalid num_queues: %u", num_queues);

	num_queues++;
	TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id, num_queues,
			SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"invalid num_queues: %u", num_queues);

	return TEST_SUCCESS;
}

static int
test_bbdev_configure_stop_device(void)
{
	struct rte_bbdev_info info;
	uint8_t dev_id;
	int return_value;

	/* valid dev_id values */
	dev_id = null_dev_id;

	/* Stop the device so it can be configured */
	rte_bbdev_stop(dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_info_get(dev_id, &info),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value from "
			"rte_bbdev_info_get function: %i", return_value);

	TEST_ASSERT_SUCCESS(info.started, "Failed test for rte_bbdev_info_get: "
			"started value: %u", info.started);

	TEST_ASSERT_SUCCESS(rte_bbdev_setup_queues(dev_id,
			info.drv.max_num_queues, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"device should be stopped, dev_id: %u", dev_id);

	return_value = rte_bbdev_intr_enable(dev_id);
	TEST_ASSERT(return_value != -EBUSY,
			"Failed test for rte_bbdev_intr_enable: device should be stopped, dev_id: %u",
			dev_id);

	/* Start the device so it cannot be configured */
	TEST_ASSERT_FAIL(rte_bbdev_start(RTE_BBDEV_MAX_DEVS),
			"Failed to start bbdev %u", dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_start(dev_id),
			"Failed to start bbdev %u", dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_info_get(dev_id, &info),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value from "
			"rte_bbdev_info_get function: %i", return_value);

	TEST_ASSERT_FAIL(info.started, "Failed test for rte_bbdev_info_get: "
			"started value: %u", info.started);

	TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id,
			info.drv.max_num_queues, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"device should be started, dev_id: %u", dev_id);

	return_value = rte_bbdev_intr_enable(dev_id);
	TEST_ASSERT(return_value == -EBUSY,
			"Failed test for rte_bbdev_intr_enable: device should be started, dev_id: %u",
			dev_id);

	/* Stop again the device so it can be once again configured */
	TEST_ASSERT_FAIL(rte_bbdev_stop(RTE_BBDEV_MAX_DEVS),
			"Failed to start bbdev %u", dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_stop(dev_id), "Failed to stop bbdev %u",
			dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_info_get(dev_id, &info),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value from "
			"rte_bbdev_info_get function: %i", return_value);

	TEST_ASSERT_SUCCESS(info.started, "Failed test for rte_bbdev_info_get: "
			"started value: %u", info.started);

	TEST_ASSERT_SUCCESS(rte_bbdev_setup_queues(dev_id,
			info.drv.max_num_queues, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"device should be stopped, dev_id: %u", dev_id);

	return_value = rte_bbdev_intr_enable(dev_id);
	TEST_ASSERT(return_value != -EBUSY,
			"Failed test for rte_bbdev_intr_enable: device should be stopped, dev_id: %u",
			dev_id);

	return TEST_SUCCESS;
}

static int
test_bbdev_configure_stop_queue(void)
{
	struct bbdev_testsuite_params *ts_params = &testsuite_params;
	struct rte_bbdev_info info;
	struct rte_bbdev_queue_info qinfo;
	uint8_t dev_id;
	uint16_t queue_id;
	int return_value;

	/* Valid dev_id values */
	dev_id = null_dev_id;

	/* Valid queue_id values */
	queue_id = 0;

	rte_bbdev_stop(dev_id);
	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_info_get(dev_id, &info),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value:%i", return_value);

	/* Valid queue configuration */
	ts_params->qconf.queue_size = info.drv.queue_size_lim;
	ts_params->qconf.priority = info.drv.max_ul_queue_priority;

	/* Device - started; queue - started */
	rte_bbdev_start(dev_id);

	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"queue:%u on device:%u should be stopped",
			 queue_id, dev_id);

	/* Device - stopped; queue - started */
	rte_bbdev_stop(dev_id);

	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"queue:%u on device:%u should be stopped",
			 queue_id, dev_id);

	TEST_ASSERT_FAIL(rte_bbdev_queue_stop(RTE_BBDEV_MAX_DEVS, queue_id),
			"Failed test for rte_bbdev_queue_stop "
			"invalid dev_id ");

	TEST_ASSERT_FAIL(rte_bbdev_queue_stop(dev_id, RTE_MAX_QUEUES_PER_PORT),
			"Failed test for rte_bbdev_queue_stop "
			"invalid queue_id ");

	/* Device - stopped; queue - stopped */
	rte_bbdev_queue_stop(dev_id, queue_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"queue:%u on device:%u should be stopped", queue_id,
			dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_queue_info_get(dev_id,
			queue_id, &qinfo),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value from "
			"rte_bbdev_queue_info_get function: %i", return_value);

	TEST_ASSERT(qinfo.conf.socket == ts_params->qconf.socket,
			"Failed test for rte_bbdev_queue_info_get: "
			"invalid queue_size:%u", qinfo.conf.socket);

	TEST_ASSERT(qinfo.conf.queue_size == ts_params->qconf.queue_size,
			"Failed test for rte_bbdev_queue_info_get: "
			"invalid queue_size:%u", qinfo.conf.queue_size);

	TEST_ASSERT(qinfo.conf.priority == ts_params->qconf.priority,
			"Failed test for rte_bbdev_queue_info_get: "
			"invalid queue_size:%u", qinfo.conf.priority);

	TEST_ASSERT(qinfo.conf.deferred_start ==
			ts_params->qconf.deferred_start,
			"Failed test for rte_bbdev_queue_info_get: "
			"invalid queue_size:%u", qinfo.conf.deferred_start);

	/* Device - started; queue - stopped */
	rte_bbdev_start(dev_id);
	rte_bbdev_queue_stop(dev_id, queue_id);

	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"queue:%u on device:%u should be stopped", queue_id,
			dev_id);

	rte_bbdev_stop(dev_id);

	/* After rte_bbdev_start(dev_id):
	 * - queue should be still stopped if deferred_start ==
	 */
	rte_bbdev_start(dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_queue_info_get(dev_id,
			queue_id, &qinfo),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value from "
			"rte_bbdev_queue_info_get function: %i", return_value);

	TEST_ASSERT(qinfo.started == 0,
			"Failed test for rte_bbdev_queue_info_get: "
			"invalid value for qinfo.started:%u", qinfo.started);

	rte_bbdev_stop(dev_id);

	/* After rte_bbdev_start(dev_id):
	 * - queue should be started if deferred_start ==
	 */
	ts_params->qconf.deferred_start = 0;
	rte_bbdev_queue_configure(dev_id, queue_id, &ts_params->qconf);
	rte_bbdev_start(dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_queue_info_get(dev_id,
			queue_id, &qinfo),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value from "
			"rte_bbdev_queue_info_get function: %i", return_value);

	TEST_ASSERT(qinfo.started == 1,
			"Failed test for rte_bbdev_queue_info_get: "
			"invalid value for qinfo.started:%u", qinfo.started);

	return TEST_SUCCESS;
}

static int
test_bbdev_configure_invalid_queue_configure(void)
{
	struct bbdev_testsuite_params *ts_params = &testsuite_params;
	int return_value;
	struct rte_bbdev_info info;
	uint8_t dev_id;
	uint16_t queue_id;

	/* Valid dev_id values */
	dev_id = null_dev_id;

	/* Valid queue_id values */
	queue_id = 0;

	rte_bbdev_stop(dev_id);

	TEST_ASSERT_SUCCESS(return_value = rte_bbdev_info_get(dev_id, &info),
			"Failed test for rte_bbdev_info_get: "
			"invalid return value:%i", return_value);

	rte_bbdev_queue_stop(dev_id, queue_id);

	ts_params->qconf.queue_size = info.drv.queue_size_lim + 1;
	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"invalid value qconf.queue_size: %u",
			ts_params->qconf.queue_size);

	ts_params->qconf.queue_size = info.drv.queue_size_lim;
	ts_params->qconf.priority = info.drv.max_ul_queue_priority;
	queue_id = info.num_queues;
	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"invalid value queue_id: %u", queue_id);

	queue_id = 0;
	TEST_ASSERT_SUCCESS(rte_bbdev_queue_configure(dev_id, queue_id, NULL),
			"Failed test for rte_bbdev_queue_configure: "
			"NULL qconf structure ");

	ts_params->qconf.socket = RTE_MAX_NUMA_NODES;
	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"invalid socket number ");

	ts_params->qconf.socket = SOCKET_ID_ANY;
	TEST_ASSERT_SUCCESS(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"invalid value qconf.queue_size: %u",
			ts_params->qconf.queue_size);

	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(RTE_BBDEV_MAX_DEVS, queue_id,
			&ts_params->qconf),
			"Failed test for rte_bbdev_queue_configure: "
			"invalid dev_id");

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_configure(dev_id, queue_id, NULL),
			"Failed test for rte_bbdev_queue_configure: "
			"invalid value qconf.queue_size: %u",
			ts_params->qconf.queue_size);

	return TEST_SUCCESS;
}

static int
test_bbdev_op_pool(void)
{
	struct rte_mempool *mp;

	unsigned int dec_size = sizeof(struct rte_bbdev_dec_op);
	unsigned int enc_size = sizeof(struct rte_bbdev_enc_op);

	const char *pool_dec = "Test_DEC";
	const char *pool_enc = "Test_ENC";

	/* Valid pool configuration */
	uint32_t size = 256;
	uint32_t cache_size = 128;

	TEST_ASSERT(rte_bbdev_op_pool_create(NULL,
			RTE_BBDEV_OP_TURBO_DEC, size, cache_size, 0) == NULL,
			"Failed test for rte_bbdev_op_pool_create: "
			"NULL name parameter");

	TEST_ASSERT((mp = rte_bbdev_op_pool_create(pool_dec,
			RTE_BBDEV_OP_TURBO_DEC, size, cache_size, 0)) != NULL,
			"Failed test for rte_bbdev_op_pool_create: "
			"returned value is empty");

	TEST_ASSERT(mp->size == size,
			"Failed test for rte_bbdev_op_pool_create: "
			"invalid size of the mempool, mp->size: %u", mp->size);

	TEST_ASSERT(mp->cache_size == cache_size,
			"Failed test for rte_bbdev_op_pool_create: "
			"invalid size of the mempool, mp->size: %u",
			mp->cache_size);

	TEST_ASSERT_SUCCESS(strcmp(mp->name, pool_dec),
			"Failed test for rte_bbdev_op_pool_create: "
			"invalid name of mempool, mp->name: %s", mp->name);

	TEST_ASSERT(mp->elt_size == dec_size,
			"Failed test for rte_bbdev_op_pool_create: "
			"invalid element size for RTE_BBDEV_OP_TURBO_DEC, "
			"mp->elt_size: %u", mp->elt_size);

	rte_mempool_free(mp);

	TEST_ASSERT((mp = rte_bbdev_op_pool_create(pool_enc,
			RTE_BBDEV_OP_TURBO_ENC, size, cache_size, 0)) != NULL,
			 "Failed test for rte_bbdev_op_pool_create: "
			"returned value is empty");

	TEST_ASSERT(mp->elt_size == enc_size,
			"Failed test for rte_bbdev_op_pool_create: "
			"invalid element size for RTE_BBDEV_OP_TURBO_ENC, "
			"mp->elt_size: %u", mp->elt_size);

	rte_mempool_free(mp);

	TEST_ASSERT((mp = rte_bbdev_op_pool_create("Test_NONE",
			RTE_BBDEV_OP_NONE, size, cache_size, 0)) != NULL,
			"Failed test for rte_bbdev_op_pool_create: "
			"returned value is empty for RTE_BBDEV_OP_NONE");

	TEST_ASSERT(mp->elt_size == (enc_size > dec_size ? enc_size : dec_size),
			"Failed test for rte_bbdev_op_pool_create: "
			"invalid  size for RTE_BBDEV_OP_NONE, mp->elt_size: %u",
			mp->elt_size);

	rte_mempool_free(mp);

	TEST_ASSERT((mp = rte_bbdev_op_pool_create("Test_INV",
			RTE_BBDEV_OP_TYPE_COUNT, size, cache_size, 0)) == NULL,
			"Failed test for rte_bbdev_op_pool_create: "
			"returned value is not NULL for invalid type");

	/* Invalid pool configuration */
	size = 128;
	cache_size = 256;

	TEST_ASSERT((mp = rte_bbdev_op_pool_create("Test_InvSize",
			RTE_BBDEV_OP_NONE, size, cache_size, 0)) == NULL,
			"Failed test for rte_bbdev_op_pool_create: "
			"returned value should be empty "
			"because size of per-lcore local cache "
			"is greater than size of the mempool.");

	return TEST_SUCCESS;
}

/**
 *  Create pool of OP types RTE_BBDEV_OP_NONE, RTE_BBDEV_OP_TURBO_DEC and
 *  RTE_BBDEV_OP_TURBO_ENC and check that only ops of that type can be
 *  allocated
 */
static int
test_bbdev_op_type(void)
{
	struct rte_mempool *mp_dec;

	const unsigned int OPS_COUNT = 32;
	struct rte_bbdev_dec_op *dec_ops_arr[OPS_COUNT];
	struct rte_bbdev_enc_op *enc_ops_arr[OPS_COUNT];

	const char *pool_dec = "Test_op_dec";

	/* Valid pool configuration */
	uint32_t num_elements = 256;
	uint32_t cache_size = 128;

	/* mempool type : RTE_BBDEV_OP_TURBO_DEC */
	mp_dec = rte_bbdev_op_pool_create(pool_dec,
			RTE_BBDEV_OP_TURBO_DEC, num_elements, cache_size, 0);
	TEST_ASSERT(mp_dec != NULL, "Failed to create %s mempool", pool_dec);

	TEST_ASSERT(rte_bbdev_dec_op_alloc_bulk(mp_dec, dec_ops_arr, 1) == 0,
			"Failed test for rte_bbdev_op_alloc_bulk TURBO_DEC: "
			"OPs type: RTE_BBDEV_OP_TURBO_DEC");

	TEST_ASSERT(rte_bbdev_enc_op_alloc_bulk(mp_dec, enc_ops_arr, 1) != 0,
			"Failed test for rte_bbdev_op_alloc_bulk TURBO_DEC: "
			"OPs type: RTE_BBDEV_OP_TURBO_ENC");

	rte_mempool_free(mp_dec);

	return TEST_SUCCESS;
}

static int
test_bbdev_op_pool_size(void)
{
	struct rte_mempool *mp_none;

	const unsigned int OPS_COUNT = 128;
	struct rte_bbdev_enc_op *ops_enc_arr[OPS_COUNT];
	struct rte_bbdev_enc_op *ops_ext_arr[OPS_COUNT];
	struct rte_bbdev_enc_op *ops_ext2_arr[OPS_COUNT];

	const char *pool_none = "Test_pool_size";

	/* Valid pool configuration */
	uint32_t num_elements = 256;
	uint32_t cache_size = 0;

	/* Create mempool type : RTE_BBDEV_OP_TURBO_ENC, size : 256 */
	mp_none = rte_bbdev_op_pool_create(pool_none, RTE_BBDEV_OP_TURBO_ENC,
			num_elements, cache_size, 0);
	TEST_ASSERT(mp_none != NULL, "Failed to create %s mempool", pool_none);

	/* Add 128 RTE_BBDEV_OP_TURBO_ENC ops */
	rte_bbdev_enc_op_alloc_bulk(mp_none, ops_enc_arr, OPS_COUNT);

	/* Add 128 RTE_BBDEV_OP_TURBO_ENC ops */
	TEST_ASSERT(rte_bbdev_enc_op_alloc_bulk(mp_none, ops_ext_arr,
			OPS_COUNT) == 0,
			"Failed test for allocating bbdev ops: "
			"Mempool size: 256, Free : 128, Attempted to add: 128");

	/* Try adding 128 more RTE_BBDEV_OP_TURBO_ENC ops, this should fail */
	TEST_ASSERT(rte_bbdev_enc_op_alloc_bulk(mp_none, ops_ext2_arr,
			OPS_COUNT) != 0,
			"Failed test for allocating bbdev ops: "
			"Mempool size: 256, Free : 0, Attempted to add: 128");

	/* Free-up 128 RTE_BBDEV_OP_TURBO_ENC ops */
	rte_bbdev_enc_op_free_bulk(ops_enc_arr, OPS_COUNT);

	/* Try adding 128 RTE_BBDEV_OP_TURBO_DEC ops, this should succeed */
	/* Cache size > 0 causes reallocation of ops size > 127 fail */
	TEST_ASSERT(rte_bbdev_enc_op_alloc_bulk(mp_none, ops_ext2_arr,
			OPS_COUNT) == 0,
			"Failed test for allocating ops after mempool freed:  "
			"Mempool size: 256, Free : 128, Attempted to add: 128");

	rte_mempool_free(mp_none);

	return TEST_SUCCESS;
}

static int
test_bbdev_count(void)
{
	uint8_t num_devs, num_valid_devs = 0;

	for (num_devs = 0; num_devs < RTE_BBDEV_MAX_DEVS; num_devs++) {
		if (rte_bbdev_is_valid(num_devs))
			num_valid_devs++;
	}

	num_devs = rte_bbdev_count();
	TEST_ASSERT(num_valid_devs == num_devs,
			"Failed test for rte_bbdev_is_valid: "
			"invalid num_devs %u ", num_devs);

	return TEST_SUCCESS;
}

static int
test_bbdev_stats(void)
{
	uint8_t dev_id = null_dev_id;
	uint16_t queue_id = 0;
	struct rte_bbdev_dec_op *dec_ops[4096] = { 0 };
	struct rte_bbdev_dec_op *dec_proc_ops[4096] = { 0 };
	struct rte_bbdev_enc_op *enc_ops[4096] = { 0 };
	struct rte_bbdev_enc_op *enc_proc_ops[4096] = { 0 };
	uint16_t num_ops = 236;
	struct rte_bbdev_stats stats;
	struct bbdev_testsuite_params *ts_params = &testsuite_params;

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_stop(dev_id, queue_id),
			"Failed to stop queue %u on device %u ", queue_id,
			dev_id);
	TEST_ASSERT_SUCCESS(rte_bbdev_stop(dev_id),
			"Failed to stop bbdev %u ", dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed to configure queue %u on device %u ",
			queue_id, dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_start(dev_id),
			"Failed to start bbdev %u ", dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_start(dev_id, queue_id),
			"Failed to start queue %u on device %u ", queue_id,
			dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_start(dev_id, queue_id),
			"Failed to start queue %u on device %u ", queue_id,
			dev_id);

	/* Tests after enqueue operation */
	rte_bbdev_enqueue_enc_ops(dev_id, queue_id, enc_ops, num_ops);
	rte_bbdev_enqueue_dec_ops(dev_id, queue_id, dec_ops, num_ops);

	TEST_ASSERT_FAIL(rte_bbdev_stats_get(RTE_BBDEV_MAX_DEVS, &stats),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);

	TEST_ASSERT_FAIL(rte_bbdev_stats_get(dev_id, NULL),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_stats_get(dev_id, &stats),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);

	TEST_ASSERT(stats.enqueued_count == 2 * num_ops,
			"Failed test for rte_bbdev_enqueue_ops: "
			"invalid enqueued_count %" PRIu64 " ",
			stats.enqueued_count);

	TEST_ASSERT(stats.dequeued_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid dequeued_count %" PRIu64 " ",
			stats.dequeued_count);

	/* Tests after dequeue operation */
	rte_bbdev_dequeue_enc_ops(dev_id, queue_id, enc_proc_ops, num_ops);
	rte_bbdev_dequeue_dec_ops(dev_id, queue_id, dec_proc_ops, num_ops);

	TEST_ASSERT_SUCCESS(rte_bbdev_stats_get(dev_id, &stats),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);

	TEST_ASSERT(stats.dequeued_count == 2 * num_ops,
			"Failed test for rte_bbdev_dequeue_ops: "
			"invalid enqueued_count %" PRIu64 " ",
			stats.dequeued_count);

	TEST_ASSERT(stats.enqueue_err_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid enqueue_err_count %" PRIu64 " ",
			stats.enqueue_err_count);

	TEST_ASSERT(stats.dequeue_err_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid dequeue_err_count %" PRIu64 " ",
			stats.dequeue_err_count);

	/* Tests after reset operation */
	TEST_ASSERT_FAIL(rte_bbdev_stats_reset(RTE_BBDEV_MAX_DEVS),
			"Failed to reset statistic for device %u ", dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_stats_reset(dev_id),
			"Failed to reset statistic for device %u ", dev_id);
	TEST_ASSERT_SUCCESS(rte_bbdev_stats_get(dev_id, &stats),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);

	TEST_ASSERT(stats.enqueued_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid enqueued_count %" PRIu64 " ",
			stats.enqueued_count);

	TEST_ASSERT(stats.dequeued_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid dequeued_count %" PRIu64 " ",
			stats.dequeued_count);

	TEST_ASSERT(stats.enqueue_err_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid enqueue_err_count %" PRIu64 " ",
			stats.enqueue_err_count);

	TEST_ASSERT(stats.dequeue_err_count == 0,
			"Failed test for rte_bbdev_stats_reset: "
			"invalid dequeue_err_count %" PRIu64 " ",
			stats.dequeue_err_count);

	return TEST_SUCCESS;
}

static int
test_bbdev_driver_init(void)
{
	struct rte_bbdev *dev1, *dev2;
	const char *name = "dev_name";
	char name_tmp[32];
	int num_devs, num_devs_tmp;

	dev1 = rte_bbdev_allocate(NULL);
	TEST_ASSERT(dev1 == NULL,
			"Failed initialize bbdev driver with NULL name");

	dev1 = rte_bbdev_allocate(name);
	TEST_ASSERT(dev1 != NULL, "Failed to initialize bbdev driver");

	dev2 = rte_bbdev_allocate(name);
	TEST_ASSERT(dev2 == NULL,
			"Failed to initialize bbdev driver: "
			"driver with the same name has been initialized before");

	num_devs = rte_bbdev_count() - 1;
	num_devs_tmp = num_devs;

	/* Initialize the maximum amount of devices */
	do {
		sprintf(name_tmp, "%s%i", "name_", num_devs);
		dev2 = rte_bbdev_allocate(name_tmp);
		TEST_ASSERT(dev2 != NULL,
				"Failed to initialize bbdev driver");
		++num_devs;
	} while (num_devs < (RTE_BBDEV_MAX_DEVS - 1));

	sprintf(name_tmp, "%s%i", "name_", num_devs);
	dev2 = rte_bbdev_allocate(name_tmp);
	TEST_ASSERT(dev2 == NULL, "Failed to initialize bbdev driver number %d "
			"more drivers than RTE_BBDEV_MAX_DEVS: %d ", num_devs,
			RTE_BBDEV_MAX_DEVS);

	num_devs--;

	while (num_devs >= num_devs_tmp) {
		sprintf(name_tmp, "%s%i", "name_", num_devs);
		dev2 = rte_bbdev_get_named_dev(name_tmp);
		TEST_ASSERT_SUCCESS(rte_bbdev_release(dev2),
				"Failed to uninitialize bbdev driver %s ",
				name_tmp);
		num_devs--;
	}

	TEST_ASSERT(dev1->data->dev_id < RTE_BBDEV_MAX_DEVS,
			"Failed test rte_bbdev_allocate: "
			"invalid dev_id %" PRIu8 ", max number of devices %d ",
			dev1->data->dev_id, RTE_BBDEV_MAX_DEVS);

	TEST_ASSERT(dev1->state == RTE_BBDEV_INITIALIZED,
			"Failed test rte_bbdev_allocate: "
			"invalid state %d (0 - RTE_BBDEV_UNUSED, 1 - RTE_BBDEV_INITIALIZED",
			dev1->state);

	TEST_ASSERT_FAIL(rte_bbdev_release(NULL),
			"Failed to uninitialize bbdev driver with NULL bbdev");

	sprintf(name_tmp, "%s", "invalid_name");
	dev2 = rte_bbdev_get_named_dev(name_tmp);
	TEST_ASSERT_FAIL(rte_bbdev_release(dev2),
			"Failed to uninitialize bbdev driver with invalid name");

	dev2 = rte_bbdev_get_named_dev(name);
	TEST_ASSERT_SUCCESS(rte_bbdev_release(dev2),
			"Failed to uninitialize bbdev driver: %s ", name);

	return TEST_SUCCESS;
}

static void
event_callback(uint16_t dev_id, enum rte_bbdev_event_type type, void *param,
		void *ret_param)
{
	RTE_SET_USED(dev_id);
	RTE_SET_USED(ret_param);

	if (param == NULL)
		return;

	if (type == RTE_BBDEV_EVENT_UNKNOWN ||
			type == RTE_BBDEV_EVENT_ERROR ||
			type == RTE_BBDEV_EVENT_MAX)
		*(int *)param = type;
}

static int
test_bbdev_callback(void)
{
	struct rte_bbdev *dev1, *dev2;
	const char *name = "dev_name1";
	const char *name2 = "dev_name2";
	int event_status;
	uint8_t invalid_dev_id = RTE_BBDEV_MAX_DEVS;
	enum rte_bbdev_event_type invalid_event_type = RTE_BBDEV_EVENT_MAX;
	uint8_t dev_id;

	dev1 = rte_bbdev_allocate(name);
	TEST_ASSERT(dev1 != NULL, "Failed to initialize bbdev driver");

	/*
	 * RTE_BBDEV_EVENT_UNKNOWN - unregistered
	 * RTE_BBDEV_EVENT_ERROR - unregistered
	 */
	event_status = -1;
	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == -1,
			"Failed test for rte_bbdev_pmd_callback_process: "
			"events were not registered ");

	TEST_ASSERT_FAIL(rte_bbdev_callback_register(dev1->data->dev_id,
			RTE_BBDEV_EVENT_MAX, event_callback, NULL),
			"Failed to callback register for RTE_BBDEV_EVENT_MAX ");

	TEST_ASSERT_FAIL(rte_bbdev_callback_unregister(dev1->data->dev_id,
			RTE_BBDEV_EVENT_MAX, event_callback, NULL),
			"Failed to unregister RTE_BBDEV_EVENT_MAX ");

	/*
	 * RTE_BBDEV_EVENT_UNKNOWN - registered
	 * RTE_BBDEV_EVENT_ERROR - unregistered
	 */
	TEST_ASSERT_SUCCESS(rte_bbdev_callback_register(dev1->data->dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, &event_status),
			"Failed to callback rgstr for RTE_BBDEV_EVENT_UNKNOWN");

	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process "
			"for RTE_BBDEV_EVENT_UNKNOWN ");

	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process: "
			"event RTE_BBDEV_EVENT_ERROR was not registered ");

	/*
	 * RTE_BBDEV_EVENT_UNKNOWN - registered
	 * RTE_BBDEV_EVENT_ERROR - registered
	 */
	TEST_ASSERT_SUCCESS(rte_bbdev_callback_register(dev1->data->dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed to callback rgstr for RTE_BBDEV_EVENT_ERROR ");

	TEST_ASSERT_SUCCESS(rte_bbdev_callback_register(dev1->data->dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed to callback register for RTE_BBDEV_EVENT_ERROR"
			"(re-registration) ");

	event_status = -1;
	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process "
			"for RTE_BBDEV_EVENT_UNKNOWN ");

	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_ERROR,
			"Failed test for rte_bbdev_pmd_callback_process "
			"for RTE_BBDEV_EVENT_ERROR ");

	/*
	 * RTE_BBDEV_EVENT_UNKNOWN - registered
	 * RTE_BBDEV_EVENT_ERROR - unregistered
	 */
	TEST_ASSERT_SUCCESS(rte_bbdev_callback_unregister(dev1->data->dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed to unregister RTE_BBDEV_EVENT_ERROR ");

	event_status = -1;
	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process "
			"for RTE_BBDEV_EVENT_UNKNOWN ");

	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process: "
			"event RTE_BBDEV_EVENT_ERROR was unregistered ");

	/* rte_bbdev_callback_register with invalid inputs */
	TEST_ASSERT_FAIL(rte_bbdev_callback_register(invalid_dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed test for rte_bbdev_callback_register "
			"for invalid_dev_id ");

	TEST_ASSERT_FAIL(rte_bbdev_callback_register(dev1->data->dev_id,
			invalid_event_type, event_callback, &event_status),
			"Failed to callback register for invalid event type ");

	TEST_ASSERT_FAIL(rte_bbdev_callback_register(dev1->data->dev_id,
			RTE_BBDEV_EVENT_ERROR, NULL, &event_status),
			"Failed to callback register - no callback function ");

	/* The impact of devices on each other */
	dev2 = rte_bbdev_allocate(name2);
	TEST_ASSERT(dev2 != NULL,
			"Failed to initialize bbdev driver");

	/*
	 * dev2:
	 * RTE_BBDEV_EVENT_UNKNOWN - unregistered
	 * RTE_BBDEV_EVENT_ERROR - unregistered
	 */
	event_status = -1;
	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == -1,
			"Failed test for rte_bbdev_pmd_callback_process: "
			"events were not registered ");

	/*
	 * dev1: RTE_BBDEV_EVENT_ERROR - unregistered
	 * dev2: RTE_BBDEV_EVENT_ERROR - registered
	 */
	TEST_ASSERT_SUCCESS(rte_bbdev_callback_register(dev2->data->dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed to callback rgstr for RTE_BBDEV_EVENT_ERROR");

	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == -1,
		"Failed test for rte_bbdev_pmd_callback_process in dev1 "
		"for RTE_BBDEV_EVENT_ERROR ");

	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_ERROR,
		"Failed test for rte_bbdev_pmd_callback_process in dev2 "
		"for RTE_BBDEV_EVENT_ERROR ");

	/*
	 * dev1: RTE_BBDEV_EVENT_UNKNOWN - registered
	 * dev2: RTE_BBDEV_EVENT_UNKNOWN - unregistered
	 */
	TEST_ASSERT_SUCCESS(rte_bbdev_callback_register(dev2->data->dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, &event_status),
			"Failed to callback register for RTE_BBDEV_EVENT_UNKNOWN "
			"in dev 2 ");

	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process in dev2"
			" for RTE_BBDEV_EVENT_UNKNOWN ");

	TEST_ASSERT_SUCCESS(rte_bbdev_callback_unregister(dev2->data->dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, &event_status),
			"Failed to unregister RTE_BBDEV_EVENT_UNKNOWN ");

	TEST_ASSERT_SUCCESS(rte_bbdev_callback_unregister(dev2->data->dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, &event_status),
			"Failed to unregister RTE_BBDEV_EVENT_UNKNOWN : "
			"unregister function called once again ");

	event_status = -1;
	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	TEST_ASSERT(event_status == -1,
			"Failed test for rte_bbdev_pmd_callback_process in dev2"
		" for RTE_BBDEV_EVENT_UNKNOWN ");

	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	TEST_ASSERT(event_status == (int) RTE_BBDEV_EVENT_UNKNOWN,
			"Failed test for rte_bbdev_pmd_callback_process in dev2 "
			"for RTE_BBDEV_EVENT_UNKNOWN ");

	/* rte_bbdev_pmd_callback_process with invalid inputs */
	rte_bbdev_pmd_callback_process(NULL, RTE_BBDEV_EVENT_UNKNOWN, NULL);

	event_status = -1;
	rte_bbdev_pmd_callback_process(dev1, invalid_event_type, NULL);
	TEST_ASSERT(event_status == -1,
			"Failed test for rte_bbdev_pmd_callback_process: "
			"for invalid event type ");

	/* rte_dev_callback_unregister with invalid inputs */
	TEST_ASSERT_FAIL(rte_bbdev_callback_unregister(invalid_dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, &event_status),
			"Failed test for rte_dev_callback_unregister "
			"for invalid_dev_id ");

	TEST_ASSERT_FAIL(rte_bbdev_callback_unregister(dev1->data->dev_id,
			invalid_event_type, event_callback, &event_status),
			"Failed rte_dev_callback_unregister "
			"for invalid event type ");

	TEST_ASSERT_FAIL(rte_bbdev_callback_unregister(dev1->data->dev_id,
			invalid_event_type, NULL, &event_status),
			"Failed rte_dev_callback_unregister "
			"when no callback function ");

	dev_id = dev1->data->dev_id;

	rte_bbdev_release(dev1);
	rte_bbdev_release(dev2);

	TEST_ASSERT_FAIL(rte_bbdev_callback_register(dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed test for rte_bbdev_callback_register: "
			"function called after rte_bbdev_driver_uninit .");

	TEST_ASSERT_FAIL(rte_bbdev_callback_unregister(dev_id,
			RTE_BBDEV_EVENT_ERROR, event_callback, &event_status),
			"Failed test for rte_dev_callback_unregister: "
			"function called after rte_bbdev_driver_uninit. ");

	event_status = -1;
	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	rte_bbdev_pmd_callback_process(dev1, RTE_BBDEV_EVENT_ERROR, NULL);
	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_UNKNOWN, NULL);
	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_ERROR, NULL);
	TEST_ASSERT(event_status == -1,
			"Failed test for rte_bbdev_pmd_callback_process: "
			"callback function was called after rte_bbdev_driver_uninit");

	return TEST_SUCCESS;
}

static int
test_bbdev_invalid_driver(void)
{
	struct rte_bbdev dev1, *dev2;
	uint8_t dev_id = null_dev_id;
	uint16_t queue_id = 0;
	struct rte_bbdev_stats stats;
	struct bbdev_testsuite_params *ts_params = &testsuite_params;
	struct rte_bbdev_queue_info qinfo;
	struct rte_bbdev_ops dev_ops_tmp;

	TEST_ASSERT_SUCCESS(rte_bbdev_stop(dev_id), "Failed to stop bbdev %u ",
			dev_id);

	dev1 = rte_bbdev_devices[dev_id];
	dev2 = &rte_bbdev_devices[dev_id];

	/* Tests for rte_bbdev_setup_queues */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id, 1, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"NULL dev_ops structure ");
	dev2->dev_ops = dev1.dev_ops;

	dev_ops_tmp = *dev2->dev_ops;
	dev_ops_tmp.info_get = NULL;
	dev2->dev_ops = &dev_ops_tmp;
	TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id, 1, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"NULL info_get ");
	dev2->dev_ops = dev1.dev_ops;

	dev_ops_tmp = *dev2->dev_ops;
	dev_ops_tmp.queue_release = NULL;
	dev2->dev_ops = &dev_ops_tmp;
	TEST_ASSERT_FAIL(rte_bbdev_setup_queues(dev_id, 1, SOCKET_ID_ANY),
			"Failed test for rte_bbdev_setup_queues: "
			"NULL queue_release ");
	dev2->dev_ops = dev1.dev_ops;

	dev2->data->socket_id = SOCKET_ID_ANY;
	TEST_ASSERT_SUCCESS(rte_bbdev_setup_queues(dev_id, 1,
			SOCKET_ID_ANY), "Failed to configure bbdev %u", dev_id);

	/* Test for rte_bbdev_queue_configure */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed to configure queue %u on device %u "
			"with NULL dev_ops structure ", queue_id, dev_id);
	dev2->dev_ops = dev1.dev_ops;

	dev_ops_tmp = *dev2->dev_ops;
	dev_ops_tmp.queue_setup = NULL;
	dev2->dev_ops = &dev_ops_tmp;
	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed to configure queue %u on device %u "
			"with NULL queue_setup ", queue_id, dev_id);
	dev2->dev_ops = dev1.dev_ops;

	dev_ops_tmp = *dev2->dev_ops;
	dev_ops_tmp.info_get = NULL;
	dev2->dev_ops = &dev_ops_tmp;
	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed to configure queue %u on device %u "
			"with NULL info_get ", queue_id, dev_id);
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_FAIL(rte_bbdev_queue_configure(RTE_BBDEV_MAX_DEVS,
			queue_id, &ts_params->qconf),
			"Failed to configure queue %u on device %u ",
			queue_id, dev_id);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_configure(dev_id, queue_id,
			&ts_params->qconf),
			"Failed to configure queue %u on device %u ",
			queue_id, dev_id);

	/* Test for rte_bbdev_queue_info_get */
	dev2->dev_ops = NULL;
	TEST_ASSERT_SUCCESS(rte_bbdev_queue_info_get(dev_id, queue_id, &qinfo),
			"Failed test for rte_bbdev_info_get: "
			"NULL dev_ops structure  ");
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_FAIL(rte_bbdev_queue_info_get(RTE_BBDEV_MAX_DEVS,
			queue_id, &qinfo),
			"Failed test for rte_bbdev_info_get: "
			"invalid dev_id ");

	TEST_ASSERT_FAIL(rte_bbdev_queue_info_get(dev_id,
			RTE_MAX_QUEUES_PER_PORT, &qinfo),
			"Failed test for rte_bbdev_info_get: "
			"invalid queue_id ");

	TEST_ASSERT_FAIL(rte_bbdev_queue_info_get(dev_id, queue_id, NULL),
			"Failed test for rte_bbdev_info_get: "
			"invalid dev_info ");

	/* Test for rte_bbdev_start */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_start(dev_id),
			"Failed to start bbdev %u "
			"with NULL dev_ops structure ", dev_id);
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_start(dev_id),
			"Failed to start bbdev %u ", dev_id);

	/* Test for rte_bbdev_queue_start */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_queue_start(dev_id, queue_id),
			"Failed to start queue %u on device %u: "
			"NULL dev_ops structure", queue_id, dev_id);
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_start(dev_id, queue_id),
			"Failed to start queue %u on device %u ", queue_id,
			dev_id);

	/* Tests for rte_bbdev_stats_get */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_stats_get(dev_id, &stats),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);
	dev2->dev_ops = dev1.dev_ops;

	dev_ops_tmp = *dev2->dev_ops;
	dev_ops_tmp.stats_reset = NULL;
	dev2->dev_ops = &dev_ops_tmp;
	TEST_ASSERT_SUCCESS(rte_bbdev_stats_get(dev_id, &stats),
			"Failed test for rte_bbdev_stats_get: "
			"NULL stats_get ");
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_stats_get(dev_id, &stats),
			"Failed test for rte_bbdev_stats_get on device %u ",
			dev_id);

	/*
	 * Tests for:
	 * rte_bbdev_callback_register,
	 * rte_bbdev_pmd_callback_process,
	 * rte_dev_callback_unregister
	 */
	dev2->dev_ops = NULL;
	TEST_ASSERT_SUCCESS(rte_bbdev_callback_register(dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, NULL),
			"Failed to callback rgstr for RTE_BBDEV_EVENT_UNKNOWN");
	rte_bbdev_pmd_callback_process(dev2, RTE_BBDEV_EVENT_UNKNOWN, NULL);

	TEST_ASSERT_SUCCESS(rte_bbdev_callback_unregister(dev_id,
			RTE_BBDEV_EVENT_UNKNOWN, event_callback, NULL),
			"Failed to unregister RTE_BBDEV_EVENT_ERROR ");
	dev2->dev_ops = dev1.dev_ops;

	/* Tests for rte_bbdev_stats_reset */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_stats_reset(dev_id),
			"Failed to reset statistic for device %u ", dev_id);
	dev2->dev_ops = dev1.dev_ops;

	dev_ops_tmp = *dev2->dev_ops;
	dev_ops_tmp.stats_reset = NULL;
	dev2->dev_ops = &dev_ops_tmp;
	TEST_ASSERT_SUCCESS(rte_bbdev_stats_reset(dev_id),
			"Failed test for rte_bbdev_stats_reset: "
			"NULL stats_reset ");
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_stats_reset(dev_id),
			"Failed to reset statistic for device %u ", dev_id);

	/* Tests for rte_bbdev_queue_stop */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_queue_stop(dev_id, queue_id),
			"Failed to stop queue %u on device %u: "
			"NULL dev_ops structure", queue_id, dev_id);
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_stop(dev_id, queue_id),
			"Failed to stop queue %u on device %u ", queue_id,
			dev_id);

	/* Tests for rte_bbdev_stop */
	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_stop(dev_id),
			"Failed to stop bbdev %u with NULL dev_ops structure ",
			dev_id);
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_stop(dev_id),
			"Failed to stop bbdev %u ", dev_id);

	/* Tests for rte_bbdev_close */
	TEST_ASSERT_FAIL(rte_bbdev_close(RTE_BBDEV_MAX_DEVS),
			"Failed to close bbdev with invalid dev_id");

	dev2->dev_ops = NULL;
	TEST_ASSERT_FAIL(rte_bbdev_close(dev_id),
			"Failed to close bbdev %u with NULL dev_ops structure ",
			dev_id);
	dev2->dev_ops = dev1.dev_ops;

	TEST_ASSERT_SUCCESS(rte_bbdev_close(dev_id),
			"Failed to close bbdev %u ", dev_id);

	return TEST_SUCCESS;
}

static int
test_bbdev_get_named_dev(void)
{
	struct rte_bbdev *dev, *dev_tmp;
	const char *name = "name";

	dev = rte_bbdev_allocate(name);
	TEST_ASSERT(dev != NULL, "Failed to initialize bbdev driver");

	dev_tmp = rte_bbdev_get_named_dev(NULL);
	TEST_ASSERT(dev_tmp == NULL, "Failed test for rte_bbdev_get_named_dev: "
			"function called with NULL parameter");

	dev_tmp = rte_bbdev_get_named_dev(name);

	TEST_ASSERT(dev == dev_tmp, "Failed test for rte_bbdev_get_named_dev: "
			"wrong device was returned ");

	TEST_ASSERT_SUCCESS(rte_bbdev_release(dev),
			"Failed to uninitialize bbdev driver %s ", name);

	return TEST_SUCCESS;
}

static struct unit_test_suite bbdev_null_testsuite = {
	.suite_name = "BBDEV NULL Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {

		TEST_CASE(test_bbdev_configure_invalid_dev_id),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_configure_invalid_num_queues),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_configure_stop_device),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_configure_stop_queue),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_configure_invalid_queue_configure),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_op_pool),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_op_type),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_op_pool_size),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_stats),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_driver_init),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_callback),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_invalid_driver),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_bbdev_get_named_dev),

		TEST_CASE(test_bbdev_count),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

REGISTER_TEST_COMMAND(unittest, bbdev_null_testsuite);
