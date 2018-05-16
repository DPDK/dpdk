/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_launch.h>
#include <rte_bbdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_hexdump.h>

#include "main.h"
#include "test_bbdev_vector.h"

#define GET_SOCKET(socket_id) (((socket_id) == SOCKET_ID_ANY) ? 0 : (socket_id))

#define MAX_QUEUES RTE_MAX_LCORE

#define OPS_CACHE_SIZE 256U
#define OPS_POOL_SIZE_MIN 511U /* 0.5K per queue */

#define SYNC_WAIT 0
#define SYNC_START 1

#define INVALID_QUEUE_ID -1

static struct test_bbdev_vector test_vector;

/* Switch between PMD and Interrupt for throughput TC */
static bool intr_enabled;

/* Represents tested active devices */
static struct active_device {
	const char *driver_name;
	uint8_t dev_id;
	uint16_t supported_ops;
	uint16_t queue_ids[MAX_QUEUES];
	uint16_t nb_queues;
	struct rte_mempool *ops_mempool;
	struct rte_mempool *in_mbuf_pool;
	struct rte_mempool *hard_out_mbuf_pool;
	struct rte_mempool *soft_out_mbuf_pool;
} active_devs[RTE_BBDEV_MAX_DEVS];

static uint8_t nb_active_devs;

/* Data buffers used by BBDEV ops */
struct test_buffers {
	struct rte_bbdev_op_data *inputs;
	struct rte_bbdev_op_data *hard_outputs;
	struct rte_bbdev_op_data *soft_outputs;
};

/* Operation parameters specific for given test case */
struct test_op_params {
	struct rte_mempool *mp;
	struct rte_bbdev_dec_op *ref_dec_op;
	struct rte_bbdev_enc_op *ref_enc_op;
	uint16_t burst_sz;
	uint16_t num_to_process;
	uint16_t num_lcores;
	int vector_mask;
	rte_atomic16_t sync;
	struct test_buffers q_bufs[RTE_MAX_NUMA_NODES][MAX_QUEUES];
};

/* Contains per lcore params */
struct thread_params {
	uint8_t dev_id;
	uint16_t queue_id;
	uint64_t start_time;
	double mops;
	double mbps;
	rte_atomic16_t nb_dequeued;
	rte_atomic16_t processing_status;
	struct test_op_params *op_params;
};

#ifdef RTE_BBDEV_OFFLOAD_COST
/* Stores time statistics */
struct test_time_stats {
	/* Stores software enqueue total working time */
	uint64_t enq_sw_tot_time;
	/* Stores minimum value of software enqueue working time */
	uint64_t enq_sw_min_time;
	/* Stores maximum value of software enqueue working time */
	uint64_t enq_sw_max_time;
	/* Stores turbo enqueue total working time */
	uint64_t enq_tur_tot_time;
	/* Stores minimum value of turbo enqueue working time */
	uint64_t enq_tur_min_time;
	/* Stores maximum value of turbo enqueue working time */
	uint64_t enq_tur_max_time;
	/* Stores dequeue total working time */
	uint64_t deq_tot_time;
	/* Stores minimum value of dequeue working time */
	uint64_t deq_min_time;
	/* Stores maximum value of dequeue working time */
	uint64_t deq_max_time;
};
#endif

typedef int (test_case_function)(struct active_device *ad,
		struct test_op_params *op_params);

static inline void
set_avail_op(struct active_device *ad, enum rte_bbdev_op_type op_type)
{
	ad->supported_ops |= (1 << op_type);
}

static inline bool
is_avail_op(struct active_device *ad, enum rte_bbdev_op_type op_type)
{
	return ad->supported_ops & (1 << op_type);
}

static inline bool
flags_match(uint32_t flags_req, uint32_t flags_present)
{
	return (flags_req & flags_present) == flags_req;
}

static void
clear_soft_out_cap(uint32_t *op_flags)
{
	*op_flags &= ~RTE_BBDEV_TURBO_SOFT_OUTPUT;
	*op_flags &= ~RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT;
	*op_flags &= ~RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT;
}

static int
check_dev_cap(const struct rte_bbdev_info *dev_info)
{
	unsigned int i;
	unsigned int nb_inputs, nb_soft_outputs, nb_hard_outputs;
	const struct rte_bbdev_op_cap *op_cap = dev_info->drv.capabilities;

	nb_inputs = test_vector.entries[DATA_INPUT].nb_segments;
	nb_soft_outputs = test_vector.entries[DATA_SOFT_OUTPUT].nb_segments;
	nb_hard_outputs = test_vector.entries[DATA_HARD_OUTPUT].nb_segments;

	for (i = 0; op_cap->type != RTE_BBDEV_OP_NONE; ++i, ++op_cap) {
		if (op_cap->type != test_vector.op_type)
			continue;

		if (op_cap->type == RTE_BBDEV_OP_TURBO_DEC) {
			const struct rte_bbdev_op_cap_turbo_dec *cap =
					&op_cap->cap.turbo_dec;
			/* Ignore lack of soft output capability, just skip
			 * checking if soft output is valid.
			 */
			if ((test_vector.turbo_dec.op_flags &
					RTE_BBDEV_TURBO_SOFT_OUTPUT) &&
					!(cap->capability_flags &
					RTE_BBDEV_TURBO_SOFT_OUTPUT)) {
				printf(
					"WARNING: Device \"%s\" does not support soft output - soft output flags will be ignored.\n",
					dev_info->dev_name);
				clear_soft_out_cap(
					&test_vector.turbo_dec.op_flags);
			}

			if (!flags_match(test_vector.turbo_dec.op_flags,
					cap->capability_flags))
				return TEST_FAILED;
			if (nb_inputs > cap->num_buffers_src) {
				printf("Too many inputs defined: %u, max: %u\n",
					nb_inputs, cap->num_buffers_src);
				return TEST_FAILED;
			}
			if (nb_soft_outputs > cap->num_buffers_soft_out &&
					(test_vector.turbo_dec.op_flags &
					RTE_BBDEV_TURBO_SOFT_OUTPUT)) {
				printf(
					"Too many soft outputs defined: %u, max: %u\n",
						nb_soft_outputs,
						cap->num_buffers_soft_out);
				return TEST_FAILED;
			}
			if (nb_hard_outputs > cap->num_buffers_hard_out) {
				printf(
					"Too many hard outputs defined: %u, max: %u\n",
						nb_hard_outputs,
						cap->num_buffers_hard_out);
				return TEST_FAILED;
			}
			if (intr_enabled && !(cap->capability_flags &
					RTE_BBDEV_TURBO_DEC_INTERRUPTS)) {
				printf(
					"Dequeue interrupts are not supported!\n");
				return TEST_FAILED;
			}

			return TEST_SUCCESS;
		} else if (op_cap->type == RTE_BBDEV_OP_TURBO_ENC) {
			const struct rte_bbdev_op_cap_turbo_enc *cap =
					&op_cap->cap.turbo_enc;

			if (!flags_match(test_vector.turbo_enc.op_flags,
					cap->capability_flags))
				return TEST_FAILED;
			if (nb_inputs > cap->num_buffers_src) {
				printf("Too many inputs defined: %u, max: %u\n",
					nb_inputs, cap->num_buffers_src);
				return TEST_FAILED;
			}
			if (nb_hard_outputs > cap->num_buffers_dst) {
				printf(
					"Too many hard outputs defined: %u, max: %u\n",
					nb_hard_outputs, cap->num_buffers_src);
				return TEST_FAILED;
			}
			if (intr_enabled && !(cap->capability_flags &
					RTE_BBDEV_TURBO_ENC_INTERRUPTS)) {
				printf(
					"Dequeue interrupts are not supported!\n");
				return TEST_FAILED;
			}

			return TEST_SUCCESS;
		}
	}

	if ((i == 0) && (test_vector.op_type == RTE_BBDEV_OP_NONE))
		return TEST_SUCCESS; /* Special case for NULL device */

	return TEST_FAILED;
}

/* calculates optimal mempool size not smaller than the val */
static unsigned int
optimal_mempool_size(unsigned int val)
{
	return rte_align32pow2(val + 1) - 1;
}

/* allocates mbuf mempool for inputs and outputs */
static struct rte_mempool *
create_mbuf_pool(struct op_data_entries *entries, uint8_t dev_id,
		int socket_id, unsigned int mbuf_pool_size,
		const char *op_type_str)
{
	unsigned int i;
	uint32_t max_seg_sz = 0;
	char pool_name[RTE_MEMPOOL_NAMESIZE];

	/* find max input segment size */
	for (i = 0; i < entries->nb_segments; ++i)
		if (entries->segments[i].length > max_seg_sz)
			max_seg_sz = entries->segments[i].length;

	snprintf(pool_name, sizeof(pool_name), "%s_pool_%u", op_type_str,
			dev_id);
	return rte_pktmbuf_pool_create(pool_name, mbuf_pool_size, 0, 0,
			RTE_MAX(max_seg_sz + RTE_PKTMBUF_HEADROOM,
			(unsigned int)RTE_MBUF_DEFAULT_BUF_SIZE), socket_id);
}

static int
create_mempools(struct active_device *ad, int socket_id,
		enum rte_bbdev_op_type op_type, uint16_t num_ops)
{
	struct rte_mempool *mp;
	unsigned int ops_pool_size, mbuf_pool_size = 0;
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	const char *op_type_str;

	struct op_data_entries *in = &test_vector.entries[DATA_INPUT];
	struct op_data_entries *hard_out =
			&test_vector.entries[DATA_HARD_OUTPUT];
	struct op_data_entries *soft_out =
			&test_vector.entries[DATA_SOFT_OUTPUT];

	/* allocate ops mempool */
	ops_pool_size = optimal_mempool_size(RTE_MAX(
			/* Ops used plus 1 reference op */
			RTE_MAX((unsigned int)(ad->nb_queues * num_ops + 1),
			/* Minimal cache size plus 1 reference op */
			(unsigned int)(1.5 * rte_lcore_count() *
					OPS_CACHE_SIZE + 1)),
			OPS_POOL_SIZE_MIN));

	op_type_str = rte_bbdev_op_type_str(op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u", op_type);

	snprintf(pool_name, sizeof(pool_name), "%s_pool_%u", op_type_str,
			ad->dev_id);
	mp = rte_bbdev_op_pool_create(pool_name, op_type,
			ops_pool_size, OPS_CACHE_SIZE, socket_id);
	TEST_ASSERT_NOT_NULL(mp,
			"ERROR Failed to create %u items ops pool for dev %u on socket %u.",
			ops_pool_size,
			ad->dev_id,
			socket_id);
	ad->ops_mempool = mp;

	/* Inputs */
	mbuf_pool_size = optimal_mempool_size(ops_pool_size * in->nb_segments);
	mp = create_mbuf_pool(in, ad->dev_id, socket_id, mbuf_pool_size, "in");
	TEST_ASSERT_NOT_NULL(mp,
			"ERROR Failed to create %u items input pktmbuf pool for dev %u on socket %u.",
			mbuf_pool_size,
			ad->dev_id,
			socket_id);
	ad->in_mbuf_pool = mp;

	/* Hard outputs */
	mbuf_pool_size = optimal_mempool_size(ops_pool_size *
			hard_out->nb_segments);
	mp = create_mbuf_pool(hard_out, ad->dev_id, socket_id, mbuf_pool_size,
			"hard_out");
	TEST_ASSERT_NOT_NULL(mp,
			"ERROR Failed to create %u items hard output pktmbuf pool for dev %u on socket %u.",
			mbuf_pool_size,
			ad->dev_id,
			socket_id);
	ad->hard_out_mbuf_pool = mp;

	if (soft_out->nb_segments == 0)
		return TEST_SUCCESS;

	/* Soft outputs */
	mbuf_pool_size = optimal_mempool_size(ops_pool_size *
			soft_out->nb_segments);
	mp = create_mbuf_pool(soft_out, ad->dev_id, socket_id, mbuf_pool_size,
			"soft_out");
	TEST_ASSERT_NOT_NULL(mp,
			"ERROR Failed to create %uB soft output pktmbuf pool for dev %u on socket %u.",
			mbuf_pool_size,
			ad->dev_id,
			socket_id);
	ad->soft_out_mbuf_pool = mp;

	return 0;
}

static int
add_bbdev_dev(uint8_t dev_id, struct rte_bbdev_info *info,
		struct test_bbdev_vector *vector)
{
	int ret;
	unsigned int queue_id;
	struct rte_bbdev_queue_conf qconf;
	struct active_device *ad = &active_devs[nb_active_devs];
	unsigned int nb_queues;
	enum rte_bbdev_op_type op_type = vector->op_type;

	nb_queues = RTE_MIN(rte_lcore_count(), info->drv.max_num_queues);
	/* setup device */
	ret = rte_bbdev_setup_queues(dev_id, nb_queues, info->socket_id);
	if (ret < 0) {
		printf("rte_bbdev_setup_queues(%u, %u, %d) ret %i\n",
				dev_id, nb_queues, info->socket_id, ret);
		return TEST_FAILED;
	}

	/* configure interrupts if needed */
	if (intr_enabled) {
		ret = rte_bbdev_intr_enable(dev_id);
		if (ret < 0) {
			printf("rte_bbdev_intr_enable(%u) ret %i\n", dev_id,
					ret);
			return TEST_FAILED;
		}
	}

	/* setup device queues */
	qconf.socket = info->socket_id;
	qconf.queue_size = info->drv.default_queue_conf.queue_size;
	qconf.priority = 0;
	qconf.deferred_start = 0;
	qconf.op_type = op_type;

	for (queue_id = 0; queue_id < nb_queues; ++queue_id) {
		ret = rte_bbdev_queue_configure(dev_id, queue_id, &qconf);
		if (ret != 0) {
			printf(
					"Allocated all queues (id=%u) at prio%u on dev%u\n",
					queue_id, qconf.priority, dev_id);
			qconf.priority++;
			ret = rte_bbdev_queue_configure(ad->dev_id, queue_id,
					&qconf);
		}
		if (ret != 0) {
			printf("All queues on dev %u allocated: %u\n",
					dev_id, queue_id);
			break;
		}
		ad->queue_ids[queue_id] = queue_id;
	}
	TEST_ASSERT(queue_id != 0,
			"ERROR Failed to configure any queues on dev %u",
			dev_id);
	ad->nb_queues = queue_id;

	set_avail_op(ad, op_type);

	return TEST_SUCCESS;
}

static int
add_active_device(uint8_t dev_id, struct rte_bbdev_info *info,
		struct test_bbdev_vector *vector)
{
	int ret;

	active_devs[nb_active_devs].driver_name = info->drv.driver_name;
	active_devs[nb_active_devs].dev_id = dev_id;

	ret = add_bbdev_dev(dev_id, info, vector);
	if (ret == TEST_SUCCESS)
		++nb_active_devs;
	return ret;
}

static uint8_t
populate_active_devices(void)
{
	int ret;
	uint8_t dev_id;
	uint8_t nb_devs_added = 0;
	struct rte_bbdev_info info;

	RTE_BBDEV_FOREACH(dev_id) {
		rte_bbdev_info_get(dev_id, &info);

		if (check_dev_cap(&info)) {
			printf(
				"Device %d (%s) does not support specified capabilities\n",
					dev_id, info.dev_name);
			continue;
		}

		ret = add_active_device(dev_id, &info, &test_vector);
		if (ret != 0) {
			printf("Adding active bbdev %s skipped\n",
					info.dev_name);
			continue;
		}
		nb_devs_added++;
	}

	return nb_devs_added;
}

static int
read_test_vector(void)
{
	int ret;

	memset(&test_vector, 0, sizeof(test_vector));
	printf("Test vector file = %s\n", get_vector_filename());
	ret = test_bbdev_vector_read(get_vector_filename(), &test_vector);
	TEST_ASSERT_SUCCESS(ret, "Failed to parse file %s\n",
			get_vector_filename());

	return TEST_SUCCESS;
}

static int
testsuite_setup(void)
{
	TEST_ASSERT_SUCCESS(read_test_vector(), "Test suite setup failed\n");

	if (populate_active_devices() == 0) {
		printf("No suitable devices found!\n");
		return TEST_SKIPPED;
	}

	return TEST_SUCCESS;
}

static int
interrupt_testsuite_setup(void)
{
	TEST_ASSERT_SUCCESS(read_test_vector(), "Test suite setup failed\n");

	/* Enable interrupts */
	intr_enabled = true;

	/* Special case for NULL device (RTE_BBDEV_OP_NONE) */
	if (populate_active_devices() == 0 ||
			test_vector.op_type == RTE_BBDEV_OP_NONE) {
		intr_enabled = false;
		printf("No suitable devices found!\n");
		return TEST_SKIPPED;
	}

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	uint8_t dev_id;

	/* Unconfigure devices */
	RTE_BBDEV_FOREACH(dev_id)
		rte_bbdev_close(dev_id);

	/* Clear active devices structs. */
	memset(active_devs, 0, sizeof(active_devs));
	nb_active_devs = 0;
}

static int
ut_setup(void)
{
	uint8_t i, dev_id;

	for (i = 0; i < nb_active_devs; i++) {
		dev_id = active_devs[i].dev_id;
		/* reset bbdev stats */
		TEST_ASSERT_SUCCESS(rte_bbdev_stats_reset(dev_id),
				"Failed to reset stats of bbdev %u", dev_id);
		/* start the device */
		TEST_ASSERT_SUCCESS(rte_bbdev_start(dev_id),
				"Failed to start bbdev %u", dev_id);
	}

	return TEST_SUCCESS;
}

static void
ut_teardown(void)
{
	uint8_t i, dev_id;
	struct rte_bbdev_stats stats;

	for (i = 0; i < nb_active_devs; i++) {
		dev_id = active_devs[i].dev_id;
		/* read stats and print */
		rte_bbdev_stats_get(dev_id, &stats);
		/* Stop the device */
		rte_bbdev_stop(dev_id);
	}
}

static int
init_op_data_objs(struct rte_bbdev_op_data *bufs,
		struct op_data_entries *ref_entries,
		struct rte_mempool *mbuf_pool, const uint16_t n,
		enum op_data_type op_type, uint16_t min_alignment)
{
	int ret;
	unsigned int i, j;

	for (i = 0; i < n; ++i) {
		char *data;
		struct op_data_buf *seg = &ref_entries->segments[0];
		struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
		TEST_ASSERT_NOT_NULL(m_head,
				"Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
				op_type, n * ref_entries->nb_segments,
				mbuf_pool->size);

		bufs[i].data = m_head;
		bufs[i].offset = 0;
		bufs[i].length = 0;

		if (op_type == DATA_INPUT) {
			data = rte_pktmbuf_append(m_head, seg->length);
			TEST_ASSERT_NOT_NULL(data,
					"Couldn't append %u bytes to mbuf from %d data type mbuf pool",
					seg->length, op_type);

			TEST_ASSERT(data == RTE_PTR_ALIGN(data, min_alignment),
					"Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
					data, min_alignment);
			rte_memcpy(data, seg->addr, seg->length);
			bufs[i].length += seg->length;


			for (j = 1; j < ref_entries->nb_segments; ++j) {
				struct rte_mbuf *m_tail =
						rte_pktmbuf_alloc(mbuf_pool);
				TEST_ASSERT_NOT_NULL(m_tail,
						"Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
						op_type,
						n * ref_entries->nb_segments,
						mbuf_pool->size);
				seg += 1;

				data = rte_pktmbuf_append(m_tail, seg->length);
				TEST_ASSERT_NOT_NULL(data,
						"Couldn't append %u bytes to mbuf from %d data type mbuf pool",
						seg->length, op_type);

				TEST_ASSERT(data == RTE_PTR_ALIGN(data,
						min_alignment),
						"Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
						data, min_alignment);
				rte_memcpy(data, seg->addr, seg->length);
				bufs[i].length += seg->length;

				ret = rte_pktmbuf_chain(m_head, m_tail);
				TEST_ASSERT_SUCCESS(ret,
						"Couldn't chain mbufs from %d data type mbuf pool",
						op_type);
			}
		}
	}

	return 0;
}

static int
allocate_buffers_on_socket(struct rte_bbdev_op_data **buffers, const int len,
		const int socket)
{
	int i;

	*buffers = rte_zmalloc_socket(NULL, len, 0, socket);
	if (*buffers == NULL) {
		printf("WARNING: Failed to allocate op_data on socket %d\n",
				socket);
		/* try to allocate memory on other detected sockets */
		for (i = 0; i < socket; i++) {
			*buffers = rte_zmalloc_socket(NULL, len, 0, i);
			if (*buffers != NULL)
				break;
		}
	}

	return (*buffers == NULL) ? TEST_FAILED : TEST_SUCCESS;
}

static void
limit_input_llr_val_range(struct rte_bbdev_op_data *input_ops,
		uint16_t n, int8_t max_llr_modulus)
{
	uint16_t i, byte_idx;

	for (i = 0; i < n; ++i) {
		struct rte_mbuf *m = input_ops[i].data;
		while (m != NULL) {
			int8_t *llr = rte_pktmbuf_mtod_offset(m, int8_t *,
					input_ops[i].offset);
			for (byte_idx = 0; byte_idx < input_ops[i].length;
					++byte_idx)
				llr[byte_idx] = round((double)max_llr_modulus *
						llr[byte_idx] / INT8_MAX);

			m = m->next;
		}
	}
}

static int
fill_queue_buffers(struct test_op_params *op_params,
		struct rte_mempool *in_mp, struct rte_mempool *hard_out_mp,
		struct rte_mempool *soft_out_mp, uint16_t queue_id,
		const struct rte_bbdev_op_cap *capabilities,
		uint16_t min_alignment, const int socket_id)
{
	int ret;
	enum op_data_type type;
	const uint16_t n = op_params->num_to_process;

	struct rte_mempool *mbuf_pools[DATA_NUM_TYPES] = {
		in_mp,
		soft_out_mp,
		hard_out_mp,
	};

	struct rte_bbdev_op_data **queue_ops[DATA_NUM_TYPES] = {
		&op_params->q_bufs[socket_id][queue_id].inputs,
		&op_params->q_bufs[socket_id][queue_id].soft_outputs,
		&op_params->q_bufs[socket_id][queue_id].hard_outputs,
	};

	for (type = DATA_INPUT; type < DATA_NUM_TYPES; ++type) {
		struct op_data_entries *ref_entries =
				&test_vector.entries[type];
		if (ref_entries->nb_segments == 0)
			continue;

		ret = allocate_buffers_on_socket(queue_ops[type],
				n * sizeof(struct rte_bbdev_op_data),
				socket_id);
		TEST_ASSERT_SUCCESS(ret,
				"Couldn't allocate memory for rte_bbdev_op_data structs");

		ret = init_op_data_objs(*queue_ops[type], ref_entries,
				mbuf_pools[type], n, type, min_alignment);
		TEST_ASSERT_SUCCESS(ret,
				"Couldn't init rte_bbdev_op_data structs");
	}

	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
		limit_input_llr_val_range(*queue_ops[DATA_INPUT], n,
			capabilities->cap.turbo_dec.max_llr_modulus);

	return 0;
}

static void
free_buffers(struct active_device *ad, struct test_op_params *op_params)
{
	unsigned int i, j;

	rte_mempool_free(ad->ops_mempool);
	rte_mempool_free(ad->in_mbuf_pool);
	rte_mempool_free(ad->hard_out_mbuf_pool);
	rte_mempool_free(ad->soft_out_mbuf_pool);

	for (i = 0; i < rte_lcore_count(); ++i) {
		for (j = 0; j < RTE_MAX_NUMA_NODES; ++j) {
			rte_free(op_params->q_bufs[j][i].inputs);
			rte_free(op_params->q_bufs[j][i].hard_outputs);
			rte_free(op_params->q_bufs[j][i].soft_outputs);
		}
	}
}

static void
copy_reference_dec_op(struct rte_bbdev_dec_op **ops, unsigned int n,
		unsigned int start_idx,
		struct rte_bbdev_op_data *inputs,
		struct rte_bbdev_op_data *hard_outputs,
		struct rte_bbdev_op_data *soft_outputs,
		struct rte_bbdev_dec_op *ref_op)
{
	unsigned int i;
	struct rte_bbdev_op_turbo_dec *turbo_dec = &ref_op->turbo_dec;

	for (i = 0; i < n; ++i) {
		if (turbo_dec->code_block_mode == 0) {
			ops[i]->turbo_dec.tb_params.ea =
					turbo_dec->tb_params.ea;
			ops[i]->turbo_dec.tb_params.eb =
					turbo_dec->tb_params.eb;
			ops[i]->turbo_dec.tb_params.k_pos =
					turbo_dec->tb_params.k_pos;
			ops[i]->turbo_dec.tb_params.k_neg =
					turbo_dec->tb_params.k_neg;
			ops[i]->turbo_dec.tb_params.c =
					turbo_dec->tb_params.c;
			ops[i]->turbo_dec.tb_params.c_neg =
					turbo_dec->tb_params.c_neg;
			ops[i]->turbo_dec.tb_params.cab =
					turbo_dec->tb_params.cab;
		} else {
			ops[i]->turbo_dec.cb_params.e = turbo_dec->cb_params.e;
			ops[i]->turbo_dec.cb_params.k = turbo_dec->cb_params.k;
		}

		ops[i]->turbo_dec.ext_scale = turbo_dec->ext_scale;
		ops[i]->turbo_dec.iter_max = turbo_dec->iter_max;
		ops[i]->turbo_dec.iter_min = turbo_dec->iter_min;
		ops[i]->turbo_dec.op_flags = turbo_dec->op_flags;
		ops[i]->turbo_dec.rv_index = turbo_dec->rv_index;
		ops[i]->turbo_dec.num_maps = turbo_dec->num_maps;
		ops[i]->turbo_dec.code_block_mode = turbo_dec->code_block_mode;

		ops[i]->turbo_dec.hard_output = hard_outputs[start_idx + i];
		ops[i]->turbo_dec.input = inputs[start_idx + i];
		if (soft_outputs != NULL)
			ops[i]->turbo_dec.soft_output =
				soft_outputs[start_idx + i];
	}
}

static void
copy_reference_enc_op(struct rte_bbdev_enc_op **ops, unsigned int n,
		unsigned int start_idx,
		struct rte_bbdev_op_data *inputs,
		struct rte_bbdev_op_data *outputs,
		struct rte_bbdev_enc_op *ref_op)
{
	unsigned int i;
	struct rte_bbdev_op_turbo_enc *turbo_enc = &ref_op->turbo_enc;
	for (i = 0; i < n; ++i) {
		if (turbo_enc->code_block_mode == 0) {
			ops[i]->turbo_enc.tb_params.ea =
					turbo_enc->tb_params.ea;
			ops[i]->turbo_enc.tb_params.eb =
					turbo_enc->tb_params.eb;
			ops[i]->turbo_enc.tb_params.k_pos =
					turbo_enc->tb_params.k_pos;
			ops[i]->turbo_enc.tb_params.k_neg =
					turbo_enc->tb_params.k_neg;
			ops[i]->turbo_enc.tb_params.c =
					turbo_enc->tb_params.c;
			ops[i]->turbo_enc.tb_params.c_neg =
					turbo_enc->tb_params.c_neg;
			ops[i]->turbo_enc.tb_params.cab =
					turbo_enc->tb_params.cab;
			ops[i]->turbo_enc.tb_params.ncb_pos =
					turbo_enc->tb_params.ncb_pos;
			ops[i]->turbo_enc.tb_params.ncb_neg =
					turbo_enc->tb_params.ncb_neg;
			ops[i]->turbo_enc.tb_params.r = turbo_enc->tb_params.r;
		} else {
			ops[i]->turbo_enc.cb_params.e = turbo_enc->cb_params.e;
			ops[i]->turbo_enc.cb_params.k = turbo_enc->cb_params.k;
			ops[i]->turbo_enc.cb_params.ncb =
					turbo_enc->cb_params.ncb;
		}
		ops[i]->turbo_enc.rv_index = turbo_enc->rv_index;
		ops[i]->turbo_enc.op_flags = turbo_enc->op_flags;
		ops[i]->turbo_enc.code_block_mode = turbo_enc->code_block_mode;

		ops[i]->turbo_enc.output = outputs[start_idx + i];
		ops[i]->turbo_enc.input = inputs[start_idx + i];
	}
}

static int
check_dec_status_and_ordering(struct rte_bbdev_dec_op *op,
		unsigned int order_idx, const int expected_status)
{
	TEST_ASSERT(op->status == expected_status,
			"op_status (%d) != expected_status (%d)",
			op->status, expected_status);

	TEST_ASSERT((void *)(uintptr_t)order_idx == op->opaque_data,
			"Ordering error, expected %p, got %p",
			(void *)(uintptr_t)order_idx, op->opaque_data);

	return TEST_SUCCESS;
}

static int
check_enc_status_and_ordering(struct rte_bbdev_enc_op *op,
		unsigned int order_idx, const int expected_status)
{
	TEST_ASSERT(op->status == expected_status,
			"op_status (%d) != expected_status (%d)",
			op->status, expected_status);

	TEST_ASSERT((void *)(uintptr_t)order_idx == op->opaque_data,
			"Ordering error, expected %p, got %p",
			(void *)(uintptr_t)order_idx, op->opaque_data);

	return TEST_SUCCESS;
}

static inline int
validate_op_chain(struct rte_bbdev_op_data *op,
		struct op_data_entries *orig_op)
{
	uint8_t i;
	struct rte_mbuf *m = op->data;
	uint8_t nb_dst_segments = orig_op->nb_segments;

	TEST_ASSERT(nb_dst_segments == m->nb_segs,
			"Number of segments differ in original (%u) and filled (%u) op",
			nb_dst_segments, m->nb_segs);

	for (i = 0; i < nb_dst_segments; ++i) {
		/* Apply offset to the first mbuf segment */
		uint16_t offset = (i == 0) ? op->offset : 0;
		uint16_t data_len = m->data_len - offset;

		TEST_ASSERT(orig_op->segments[i].length == data_len,
				"Length of segment differ in original (%u) and filled (%u) op",
				orig_op->segments[i].length, data_len);
		TEST_ASSERT_BUFFERS_ARE_EQUAL(orig_op->segments[i].addr,
				rte_pktmbuf_mtod_offset(m, uint32_t *, offset),
				data_len,
				"Output buffers (CB=%u) are not equal", i);
		m = m->next;
	}

	return TEST_SUCCESS;
}

static int
validate_dec_buffers(struct rte_bbdev_dec_op *ref_op, struct test_buffers *bufs,
		const uint16_t num_to_process)
{
	int i;

	struct op_data_entries *hard_data_orig =
			&test_vector.entries[DATA_HARD_OUTPUT];
	struct op_data_entries *soft_data_orig =
			&test_vector.entries[DATA_SOFT_OUTPUT];

	for (i = 0; i < num_to_process; i++) {
		TEST_ASSERT_SUCCESS(validate_op_chain(&bufs->hard_outputs[i],
				hard_data_orig),
				"Hard output buffers are not equal");
		if (ref_op->turbo_dec.op_flags &
				RTE_BBDEV_TURBO_SOFT_OUTPUT)
			TEST_ASSERT_SUCCESS(validate_op_chain(
					&bufs->soft_outputs[i],
					soft_data_orig),
					"Soft output buffers are not equal");
	}

	return TEST_SUCCESS;
}

static int
validate_enc_buffers(struct test_buffers *bufs, const uint16_t num_to_process)
{
	int i;

	struct op_data_entries *hard_data_orig =
			&test_vector.entries[DATA_HARD_OUTPUT];

	for (i = 0; i < num_to_process; i++)
		TEST_ASSERT_SUCCESS(validate_op_chain(&bufs->hard_outputs[i],
				hard_data_orig), "");

	return TEST_SUCCESS;
}

static int
validate_dec_op(struct rte_bbdev_dec_op **ops, const uint16_t n,
		struct rte_bbdev_dec_op *ref_op, const int vector_mask)
{
	unsigned int i;
	int ret;
	struct op_data_entries *hard_data_orig =
			&test_vector.entries[DATA_HARD_OUTPUT];
	struct op_data_entries *soft_data_orig =
			&test_vector.entries[DATA_SOFT_OUTPUT];
	struct rte_bbdev_op_turbo_dec *ops_td;
	struct rte_bbdev_op_data *hard_output;
	struct rte_bbdev_op_data *soft_output;
	struct rte_bbdev_op_turbo_dec *ref_td = &ref_op->turbo_dec;

	for (i = 0; i < n; ++i) {
		ops_td = &ops[i]->turbo_dec;
		hard_output = &ops_td->hard_output;
		soft_output = &ops_td->soft_output;

		if (vector_mask & TEST_BBDEV_VF_EXPECTED_ITER_COUNT)
			TEST_ASSERT(ops_td->iter_count <= ref_td->iter_count,
					"Returned iter_count (%d) > expected iter_count (%d)",
					ops_td->iter_count, ref_td->iter_count);
		ret = check_dec_status_and_ordering(ops[i], i, ref_op->status);
		TEST_ASSERT_SUCCESS(ret,
				"Checking status and ordering for decoder failed");

		TEST_ASSERT_SUCCESS(validate_op_chain(hard_output,
				hard_data_orig),
				"Hard output buffers (CB=%u) are not equal",
				i);

		if (ref_op->turbo_dec.op_flags & RTE_BBDEV_TURBO_SOFT_OUTPUT)
			TEST_ASSERT_SUCCESS(validate_op_chain(soft_output,
					soft_data_orig),
					"Soft output buffers (CB=%u) are not equal",
					i);
	}

	return TEST_SUCCESS;
}

static int
validate_enc_op(struct rte_bbdev_enc_op **ops, const uint16_t n,
		struct rte_bbdev_enc_op *ref_op)
{
	unsigned int i;
	int ret;
	struct op_data_entries *hard_data_orig =
			&test_vector.entries[DATA_HARD_OUTPUT];

	for (i = 0; i < n; ++i) {
		ret = check_enc_status_and_ordering(ops[i], i, ref_op->status);
		TEST_ASSERT_SUCCESS(ret,
				"Checking status and ordering for encoder failed");
		TEST_ASSERT_SUCCESS(validate_op_chain(
				&ops[i]->turbo_enc.output,
				hard_data_orig),
				"Output buffers (CB=%u) are not equal",
				i);
	}

	return TEST_SUCCESS;
}

static void
create_reference_dec_op(struct rte_bbdev_dec_op *op)
{
	unsigned int i;
	struct op_data_entries *entry;

	op->turbo_dec = test_vector.turbo_dec;
	entry = &test_vector.entries[DATA_INPUT];
	for (i = 0; i < entry->nb_segments; ++i)
		op->turbo_dec.input.length +=
				entry->segments[i].length;
}

static void
create_reference_enc_op(struct rte_bbdev_enc_op *op)
{
	unsigned int i;
	struct op_data_entries *entry;

	op->turbo_enc = test_vector.turbo_enc;
	entry = &test_vector.entries[DATA_INPUT];
	for (i = 0; i < entry->nb_segments; ++i)
		op->turbo_enc.input.length +=
				entry->segments[i].length;
}

static int
init_test_op_params(struct test_op_params *op_params,
		enum rte_bbdev_op_type op_type, const int expected_status,
		const int vector_mask, struct rte_mempool *ops_mp,
		uint16_t burst_sz, uint16_t num_to_process, uint16_t num_lcores)
{
	int ret = 0;
	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		ret = rte_bbdev_dec_op_alloc_bulk(ops_mp,
				&op_params->ref_dec_op, 1);
	else
		ret = rte_bbdev_enc_op_alloc_bulk(ops_mp,
				&op_params->ref_enc_op, 1);

	TEST_ASSERT_SUCCESS(ret, "rte_bbdev_op_alloc_bulk() failed");

	op_params->mp = ops_mp;
	op_params->burst_sz = burst_sz;
	op_params->num_to_process = num_to_process;
	op_params->num_lcores = num_lcores;
	op_params->vector_mask = vector_mask;
	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		op_params->ref_dec_op->status = expected_status;
	else if (op_type == RTE_BBDEV_OP_TURBO_ENC)
		op_params->ref_enc_op->status = expected_status;

	return 0;
}

static int
run_test_case_on_device(test_case_function *test_case_func, uint8_t dev_id,
		struct test_op_params *op_params)
{
	int t_ret, f_ret, socket_id = SOCKET_ID_ANY;
	unsigned int i;
	struct active_device *ad;
	unsigned int burst_sz = get_burst_sz();
	enum rte_bbdev_op_type op_type = test_vector.op_type;
	const struct rte_bbdev_op_cap *capabilities = NULL;

	ad = &active_devs[dev_id];

	/* Check if device supports op_type */
	if (!is_avail_op(ad, test_vector.op_type))
		return TEST_SUCCESS;

	struct rte_bbdev_info info;
	rte_bbdev_info_get(ad->dev_id, &info);
	socket_id = GET_SOCKET(info.socket_id);

	if (op_type == RTE_BBDEV_OP_NONE)
		op_type = RTE_BBDEV_OP_TURBO_ENC;
	f_ret = create_mempools(ad, socket_id, op_type,
			get_num_ops());
	if (f_ret != TEST_SUCCESS) {
		printf("Couldn't create mempools");
		goto fail;
	}

	f_ret = init_test_op_params(op_params, test_vector.op_type,
			test_vector.expected_status,
			test_vector.mask,
			ad->ops_mempool,
			burst_sz,
			get_num_ops(),
			get_num_lcores());
	if (f_ret != TEST_SUCCESS) {
		printf("Couldn't init test op params");
		goto fail;
	}

	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC) {
		/* Find Decoder capabilities */
		const struct rte_bbdev_op_cap *cap = info.drv.capabilities;
		while (cap->type != RTE_BBDEV_OP_NONE) {
			if (cap->type == RTE_BBDEV_OP_TURBO_DEC) {
				capabilities = cap;
				break;
			}
		}
		TEST_ASSERT_NOT_NULL(capabilities,
				"Couldn't find Decoder capabilities");

		create_reference_dec_op(op_params->ref_dec_op);
	} else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC)
		create_reference_enc_op(op_params->ref_enc_op);

	for (i = 0; i < ad->nb_queues; ++i) {
		f_ret = fill_queue_buffers(op_params,
				ad->in_mbuf_pool,
				ad->hard_out_mbuf_pool,
				ad->soft_out_mbuf_pool,
				ad->queue_ids[i],
				capabilities,
				info.drv.min_alignment,
				socket_id);
		if (f_ret != TEST_SUCCESS) {
			printf("Couldn't init queue buffers");
			goto fail;
		}
	}

	/* Run test case function */
	t_ret = test_case_func(ad, op_params);

	/* Free active device resources and return */
	free_buffers(ad, op_params);
	return t_ret;

fail:
	free_buffers(ad, op_params);
	return TEST_FAILED;
}

/* Run given test function per active device per supported op type
 * per burst size.
 */
static int
run_test_case(test_case_function *test_case_func)
{
	int ret = 0;
	uint8_t dev;

	/* Alloc op_params */
	struct test_op_params *op_params = rte_zmalloc(NULL,
			sizeof(struct test_op_params), RTE_CACHE_LINE_SIZE);
	TEST_ASSERT_NOT_NULL(op_params, "Failed to alloc %zuB for op_params",
			RTE_ALIGN(sizeof(struct test_op_params),
				RTE_CACHE_LINE_SIZE));

	/* For each device run test case function */
	for (dev = 0; dev < nb_active_devs; ++dev)
		ret |= run_test_case_on_device(test_case_func, dev, op_params);

	rte_free(op_params);

	return ret;
}

static void
dequeue_event_callback(uint16_t dev_id,
		enum rte_bbdev_event_type event, void *cb_arg,
		void *ret_param)
{
	int ret;
	uint16_t i;
	uint64_t total_time;
	uint16_t deq, burst_sz, num_to_process;
	uint16_t queue_id = INVALID_QUEUE_ID;
	struct rte_bbdev_dec_op *dec_ops[MAX_BURST];
	struct rte_bbdev_enc_op *enc_ops[MAX_BURST];
	struct test_buffers *bufs;
	struct rte_bbdev_info info;

	/* Input length in bytes, million operations per second,
	 * million bits per second.
	 */
	double in_len;

	struct thread_params *tp = cb_arg;
	RTE_SET_USED(ret_param);
	queue_id = tp->queue_id;

	/* Find matching thread params using queue_id */
	for (i = 0; i < MAX_QUEUES; ++i, ++tp)
		if (tp->queue_id == queue_id)
			break;

	if (i == MAX_QUEUES) {
		printf("%s: Queue_id from interrupt details was not found!\n",
				__func__);
		return;
	}

	if (unlikely(event != RTE_BBDEV_EVENT_DEQUEUE)) {
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
		printf(
			"Dequeue interrupt handler called for incorrect event!\n");
		return;
	}

	burst_sz = tp->op_params->burst_sz;
	num_to_process = tp->op_params->num_to_process;

	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
		deq = rte_bbdev_dequeue_dec_ops(dev_id, queue_id, dec_ops,
				burst_sz);
	else
		deq = rte_bbdev_dequeue_enc_ops(dev_id, queue_id, enc_ops,
				burst_sz);

	if (deq < burst_sz) {
		printf(
			"After receiving the interrupt all operations should be dequeued. Expected: %u, got: %u\n",
			burst_sz, deq);
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
		return;
	}

	if (rte_atomic16_read(&tp->nb_dequeued) + deq < num_to_process) {
		rte_atomic16_add(&tp->nb_dequeued, deq);
		return;
	}

	total_time = rte_rdtsc_precise() - tp->start_time;

	rte_bbdev_info_get(dev_id, &info);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	ret = TEST_SUCCESS;
	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
		ret = validate_dec_buffers(tp->op_params->ref_dec_op, bufs,
				num_to_process);
	else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC)
		ret = validate_enc_buffers(bufs, num_to_process);

	if (ret) {
		printf("Buffers validation failed\n");
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
	}

	switch (test_vector.op_type) {
	case RTE_BBDEV_OP_TURBO_DEC:
		in_len = tp->op_params->ref_dec_op->turbo_dec.input.length;
		break;
	case RTE_BBDEV_OP_TURBO_ENC:
		in_len = tp->op_params->ref_enc_op->turbo_enc.input.length;
		break;
	case RTE_BBDEV_OP_NONE:
		in_len = 0.0;
		break;
	default:
		printf("Unknown op type: %d\n", test_vector.op_type);
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
		return;
	}

	tp->mops = ((double)num_to_process / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = ((double)num_to_process * in_len * 8 / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());

	rte_atomic16_add(&tp->nb_dequeued, deq);
}

static int
throughput_intr_lcore_dec(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued;
	struct rte_bbdev_dec_op *ops[MAX_BURST];
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct test_buffers *bufs = NULL;
	unsigned int allocs_failed = 0;
	struct rte_bbdev_info info;
	int ret;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_intr_enable(tp->dev_id, queue_id),
			"Failed to enable interrupts for dev: %u, queue_id: %u",
			tp->dev_id, queue_id);

	rte_bbdev_info_get(tp->dev_id, &info);
	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	rte_atomic16_clear(&tp->processing_status);
	rte_atomic16_clear(&tp->nb_dequeued);

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	tp->start_time = rte_rdtsc_precise();
	for (enqueued = 0; enqueued < num_to_process;) {

		uint16_t num_to_enq = burst_sz;

		if (unlikely(num_to_process - enqueued < num_to_enq))
			num_to_enq = num_to_process - enqueued;

		ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp, ops,
				num_to_enq);
		if (ret != 0) {
			allocs_failed++;
			continue;
		}

		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_dec_op(ops, num_to_enq, enqueued,
					bufs->inputs,
					bufs->hard_outputs,
					bufs->soft_outputs,
					tp->op_params->ref_dec_op);

		enqueued += rte_bbdev_enqueue_dec_ops(tp->dev_id, queue_id, ops,
				num_to_enq);

		rte_bbdev_dec_op_free_bulk(ops, num_to_enq);
	}

	if (allocs_failed > 0)
		printf("WARNING: op allocations failed: %u times\n",
				allocs_failed);

	return TEST_SUCCESS;
}

static int
throughput_intr_lcore_enc(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued;
	struct rte_bbdev_enc_op *ops[MAX_BURST];
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct test_buffers *bufs = NULL;
	unsigned int allocs_failed = 0;
	struct rte_bbdev_info info;
	int ret;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_intr_enable(tp->dev_id, queue_id),
			"Failed to enable interrupts for dev: %u, queue_id: %u",
			tp->dev_id, queue_id);

	rte_bbdev_info_get(tp->dev_id, &info);
	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	rte_atomic16_clear(&tp->processing_status);
	rte_atomic16_clear(&tp->nb_dequeued);

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	tp->start_time = rte_rdtsc_precise();
	for (enqueued = 0; enqueued < num_to_process;) {

		uint16_t num_to_enq = burst_sz;

		if (unlikely(num_to_process - enqueued < num_to_enq))
			num_to_enq = num_to_process - enqueued;

		ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp, ops,
				num_to_enq);
		if (ret != 0) {
			allocs_failed++;
			continue;
		}

		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_enc_op(ops, num_to_enq, enqueued,
					bufs->inputs,
					bufs->hard_outputs,
					tp->op_params->ref_enc_op);

		enqueued += rte_bbdev_enqueue_enc_ops(tp->dev_id, queue_id, ops,
				num_to_enq);

		rte_bbdev_enc_op_free_bulk(ops, num_to_enq);
	}

	if (allocs_failed > 0)
		printf("WARNING: op allocations failed: %u times\n",
				allocs_failed);

	return TEST_SUCCESS;
}

static int
throughput_pmd_lcore_dec(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued, dequeued;
	struct rte_bbdev_dec_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t total_time, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
	struct test_buffers *bufs = NULL;
	unsigned int allocs_failed = 0;
	int ret;
	struct rte_bbdev_info info;

	/* Input length in bytes, million operations per second, million bits
	 * per second.
	 */
	double in_len;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);
	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	start_time = rte_rdtsc_precise();
	for (enqueued = 0, dequeued = 0; dequeued < num_to_process;) {
		uint16_t deq;

		if (likely(enqueued < num_to_process)) {

			uint16_t num_to_enq = burst_sz;

			if (unlikely(num_to_process - enqueued < num_to_enq))
				num_to_enq = num_to_process - enqueued;

			ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp,
					ops_enq, num_to_enq);
			if (ret != 0) {
				allocs_failed++;
				goto do_dequeue;
			}

			if (test_vector.op_type != RTE_BBDEV_OP_NONE)
				copy_reference_dec_op(ops_enq, num_to_enq,
						enqueued,
						bufs->inputs,
						bufs->hard_outputs,
						bufs->soft_outputs,
						ref_op);

			enqueued += rte_bbdev_enqueue_dec_ops(tp->dev_id,
					queue_id, ops_enq, num_to_enq);
		}
do_dequeue:
		deq = rte_bbdev_dequeue_dec_ops(tp->dev_id, queue_id, ops_deq,
				burst_sz);
		dequeued += deq;
		rte_bbdev_dec_op_free_bulk(ops_enq, deq);
	}
	total_time = rte_rdtsc_precise() - start_time;

	if (allocs_failed > 0)
		printf("WARNING: op allocations failed: %u times\n",
				allocs_failed);

	TEST_ASSERT(enqueued == dequeued, "enqueued (%u) != dequeued (%u)",
			enqueued, dequeued);

	if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
		ret = validate_dec_buffers(ref_op, bufs, num_to_process);
		TEST_ASSERT_SUCCESS(ret, "Buffers validation failed");
	}

	in_len = ref_op->turbo_dec.input.length;
	tp->mops = ((double)num_to_process / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = ((double)num_to_process * in_len * 8 / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static int
throughput_pmd_lcore_enc(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued, dequeued;
	struct rte_bbdev_enc_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t total_time, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct rte_bbdev_enc_op *ref_op = tp->op_params->ref_enc_op;
	struct test_buffers *bufs = NULL;
	unsigned int allocs_failed = 0;
	int ret;
	struct rte_bbdev_info info;

	/* Input length in bytes, million operations per second, million bits
	 * per second.
	 */
	double in_len;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);
	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	start_time = rte_rdtsc_precise();
	for (enqueued = 0, dequeued = 0; dequeued < num_to_process;) {
		uint16_t deq;

		if (likely(enqueued < num_to_process)) {

			uint16_t num_to_enq = burst_sz;

			if (unlikely(num_to_process - enqueued < num_to_enq))
				num_to_enq = num_to_process - enqueued;

			ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp,
					ops_enq, num_to_enq);
			if (ret != 0) {
				allocs_failed++;
				goto do_dequeue;
			}

			if (test_vector.op_type != RTE_BBDEV_OP_NONE)
				copy_reference_enc_op(ops_enq, num_to_enq,
						enqueued,
						bufs->inputs,
						bufs->hard_outputs,
						ref_op);

			enqueued += rte_bbdev_enqueue_enc_ops(tp->dev_id,
					queue_id, ops_enq, num_to_enq);
		}
do_dequeue:
		deq = rte_bbdev_dequeue_enc_ops(tp->dev_id, queue_id, ops_deq,
				burst_sz);
		dequeued += deq;
		rte_bbdev_enc_op_free_bulk(ops_enq, deq);
	}
	total_time = rte_rdtsc_precise() - start_time;

	if (allocs_failed > 0)
		printf("WARNING: op allocations failed: %u times\n",
				allocs_failed);

	TEST_ASSERT(enqueued == dequeued, "enqueued (%u) != dequeued (%u)",
			enqueued, dequeued);

	if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
		ret = validate_enc_buffers(bufs, num_to_process);
		TEST_ASSERT_SUCCESS(ret, "Buffers validation failed");
	}

	in_len = ref_op->turbo_enc.input.length;

	tp->mops = ((double)num_to_process / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = ((double)num_to_process * in_len * 8 / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}
static void
print_throughput(struct thread_params *t_params, unsigned int used_cores)
{
	unsigned int lcore_id, iter = 0;
	double total_mops = 0, total_mbps = 0;

	RTE_LCORE_FOREACH(lcore_id) {
		if (iter++ >= used_cores)
			break;
		printf("\tlcore_id: %u, throughput: %.8lg MOPS, %.8lg Mbps\n",
		lcore_id, t_params[lcore_id].mops, t_params[lcore_id].mbps);
		total_mops += t_params[lcore_id].mops;
		total_mbps += t_params[lcore_id].mbps;
	}
	printf(
		"\n\tTotal stats for %u cores: throughput: %.8lg MOPS, %.8lg Mbps\n",
		used_cores, total_mops, total_mbps);
}

/*
 * Test function that determines how long an enqueue + dequeue of a burst
 * takes on available lcores.
 */
static int
throughput_test(struct active_device *ad,
		struct test_op_params *op_params)
{
	int ret;
	unsigned int lcore_id, used_cores = 0;
	struct thread_params t_params[MAX_QUEUES];
	struct rte_bbdev_info info;
	lcore_function_t *throughput_function;
	struct thread_params *tp;
	uint16_t num_lcores;
	const char *op_type_str;

	rte_bbdev_info_get(ad->dev_id, &info);

	op_type_str = rte_bbdev_op_type_str(test_vector.op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u",
			test_vector.op_type);

	printf(
		"Throughput test: dev: %s, nb_queues: %u, burst size: %u, num ops: %u, num_lcores: %u, op type: %s, int mode: %s, GHz: %lg\n",
			info.dev_name, ad->nb_queues, op_params->burst_sz,
			op_params->num_to_process, op_params->num_lcores,
			op_type_str,
			intr_enabled ? "Interrupt mode" : "PMD mode",
			(double)rte_get_tsc_hz() / 1000000000.0);

	/* Set number of lcores */
	num_lcores = (ad->nb_queues < (op_params->num_lcores))
			? ad->nb_queues
			: op_params->num_lcores;

	if (intr_enabled) {
		if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
			throughput_function = throughput_intr_lcore_dec;
		else
			throughput_function = throughput_intr_lcore_enc;

		/* Dequeue interrupt callback registration */
		ret = rte_bbdev_callback_register(ad->dev_id,
				RTE_BBDEV_EVENT_DEQUEUE, dequeue_event_callback,
				&t_params);
		if (ret < 0)
			return ret;
	} else {
		if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
			throughput_function = throughput_pmd_lcore_dec;
		else
			throughput_function = throughput_pmd_lcore_enc;
	}

	rte_atomic16_set(&op_params->sync, SYNC_WAIT);

	t_params[rte_lcore_id()].dev_id = ad->dev_id;
	t_params[rte_lcore_id()].op_params = op_params;
	t_params[rte_lcore_id()].queue_id =
			ad->queue_ids[used_cores++];

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (used_cores >= num_lcores)
			break;

		t_params[lcore_id].dev_id = ad->dev_id;
		t_params[lcore_id].op_params = op_params;
		t_params[lcore_id].queue_id = ad->queue_ids[used_cores++];

		rte_eal_remote_launch(throughput_function, &t_params[lcore_id],
				lcore_id);
	}

	rte_atomic16_set(&op_params->sync, SYNC_START);
	ret = throughput_function(&t_params[rte_lcore_id()]);

	/* Master core is always used */
	used_cores = 1;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (used_cores++ >= num_lcores)
			break;

		ret |= rte_eal_wait_lcore(lcore_id);
	}

	/* Return if test failed */
	if (ret)
		return ret;

	/* Print throughput if interrupts are disabled and test passed */
	if (!intr_enabled) {
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			print_throughput(t_params, num_lcores);
		return ret;
	}

	/* In interrupt TC we need to wait for the interrupt callback to deqeue
	 * all pending operations. Skip waiting for queues which reported an
	 * error using processing_status variable.
	 * Wait for master lcore operations.
	 */
	tp = &t_params[rte_lcore_id()];
	while ((rte_atomic16_read(&tp->nb_dequeued) <
			op_params->num_to_process) &&
			(rte_atomic16_read(&tp->processing_status) !=
			TEST_FAILED))
		rte_pause();

	ret |= rte_atomic16_read(&tp->processing_status);

	/* Wait for slave lcores operations */
	used_cores = 1;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		tp = &t_params[lcore_id];
		if (used_cores++ >= num_lcores)
			break;

		while ((rte_atomic16_read(&tp->nb_dequeued) <
				op_params->num_to_process) &&
				(rte_atomic16_read(&tp->processing_status) !=
				TEST_FAILED))
			rte_pause();

		ret |= rte_atomic16_read(&tp->processing_status);
	}

	/* Print throughput if test passed */
	if (!ret && test_vector.op_type != RTE_BBDEV_OP_NONE)
		print_throughput(t_params, num_lcores);

	return ret;
}

static int
latency_test_dec(struct rte_mempool *mempool,
		struct test_buffers *bufs, struct rte_bbdev_dec_op *ref_op,
		int vector_mask, uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *total_time, uint64_t *min_time, uint64_t *max_time)
{
	int ret = TEST_SUCCESS;
	uint16_t i, j, dequeued;
	struct rte_bbdev_dec_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t start_time = 0, last_time = 0;

	for (i = 0, dequeued = 0; dequeued < num_to_process; ++i) {
		uint16_t enq = 0, deq = 0;
		bool first_time = true;
		last_time = 0;

		if (unlikely(num_to_process - dequeued < burst_sz))
			burst_sz = num_to_process - dequeued;

		ret = rte_bbdev_dec_op_alloc_bulk(mempool, ops_enq, burst_sz);
		TEST_ASSERT_SUCCESS(ret,
				"rte_bbdev_dec_op_alloc_bulk() failed");
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_dec_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					bufs->soft_outputs,
					ref_op);

		/* Set counter to validate the ordering */
		for (j = 0; j < burst_sz; ++j)
			ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

		start_time = rte_rdtsc_precise();

		enq = rte_bbdev_enqueue_dec_ops(dev_id, queue_id, &ops_enq[enq],
				burst_sz);
		TEST_ASSERT(enq == burst_sz,
				"Error enqueueing burst, expected %u, got %u",
				burst_sz, enq);

		/* Dequeue */
		do {
			deq += rte_bbdev_dequeue_dec_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);
			if (likely(first_time && (deq > 0))) {
				last_time = rte_rdtsc_precise() - start_time;
				first_time = false;
			}
		} while (unlikely(burst_sz != deq));

		*max_time = RTE_MAX(*max_time, last_time);
		*min_time = RTE_MIN(*min_time, last_time);
		*total_time += last_time;

		if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
			ret = validate_dec_op(ops_deq, burst_sz, ref_op,
					vector_mask);
			TEST_ASSERT_SUCCESS(ret, "Validation failed!");
		}

		rte_bbdev_dec_op_free_bulk(ops_enq, deq);
		dequeued += deq;
	}

	return i;
}

static int
latency_test_enc(struct rte_mempool *mempool,
		struct test_buffers *bufs, struct rte_bbdev_enc_op *ref_op,
		uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *total_time, uint64_t *min_time, uint64_t *max_time)
{
	int ret = TEST_SUCCESS;
	uint16_t i, j, dequeued;
	struct rte_bbdev_enc_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t start_time = 0, last_time = 0;

	for (i = 0, dequeued = 0; dequeued < num_to_process; ++i) {
		uint16_t enq = 0, deq = 0;
		bool first_time = true;
		last_time = 0;

		if (unlikely(num_to_process - dequeued < burst_sz))
			burst_sz = num_to_process - dequeued;

		ret = rte_bbdev_enc_op_alloc_bulk(mempool, ops_enq, burst_sz);
		TEST_ASSERT_SUCCESS(ret,
				"rte_bbdev_enc_op_alloc_bulk() failed");
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_enc_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					ref_op);

		/* Set counter to validate the ordering */
		for (j = 0; j < burst_sz; ++j)
			ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

		start_time = rte_rdtsc_precise();

		enq = rte_bbdev_enqueue_enc_ops(dev_id, queue_id, &ops_enq[enq],
				burst_sz);
		TEST_ASSERT(enq == burst_sz,
				"Error enqueueing burst, expected %u, got %u",
				burst_sz, enq);

		/* Dequeue */
		do {
			deq += rte_bbdev_dequeue_enc_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);
			if (likely(first_time && (deq > 0))) {
				last_time += rte_rdtsc_precise() - start_time;
				first_time = false;
			}
		} while (unlikely(burst_sz != deq));

		*max_time = RTE_MAX(*max_time, last_time);
		*min_time = RTE_MIN(*min_time, last_time);
		*total_time += last_time;

		if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
			ret = validate_enc_op(ops_deq, burst_sz, ref_op);
			TEST_ASSERT_SUCCESS(ret, "Validation failed!");
		}

		rte_bbdev_enc_op_free_bulk(ops_enq, deq);
		dequeued += deq;
	}

	return i;
}

static int
latency_test(struct active_device *ad,
		struct test_op_params *op_params)
{
	int iter;
	uint16_t burst_sz = op_params->burst_sz;
	const uint16_t num_to_process = op_params->num_to_process;
	const enum rte_bbdev_op_type op_type = test_vector.op_type;
	const uint16_t queue_id = ad->queue_ids[0];
	struct test_buffers *bufs = NULL;
	struct rte_bbdev_info info;
	uint64_t total_time, min_time, max_time;
	const char *op_type_str;

	total_time = max_time = 0;
	min_time = UINT64_MAX;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(ad->dev_id, &info);
	bufs = &op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	op_type_str = rte_bbdev_op_type_str(op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u", op_type);

	printf(
		"Validation/Latency test: dev: %s, burst size: %u, num ops: %u, op type: %s\n",
			info.dev_name, burst_sz, num_to_process, op_type_str);

	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		iter = latency_test_dec(op_params->mp, bufs,
				op_params->ref_dec_op, op_params->vector_mask,
				ad->dev_id, queue_id, num_to_process,
				burst_sz, &total_time, &min_time, &max_time);
	else
		iter = latency_test_enc(op_params->mp, bufs,
				op_params->ref_enc_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &total_time,
				&min_time, &max_time);

	if (iter <= 0)
		return TEST_FAILED;

	printf("\toperation latency:\n"
			"\t\tavg latency: %lg cycles, %lg us\n"
			"\t\tmin latency: %lg cycles, %lg us\n"
			"\t\tmax latency: %lg cycles, %lg us\n",
			(double)total_time / (double)iter,
			(double)(total_time * 1000000) / (double)iter /
			(double)rte_get_tsc_hz(), (double)min_time,
			(double)(min_time * 1000000) / (double)rte_get_tsc_hz(),
			(double)max_time, (double)(max_time * 1000000) /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

#ifdef RTE_BBDEV_OFFLOAD_COST
static int
get_bbdev_queue_stats(uint16_t dev_id, uint16_t queue_id,
		struct rte_bbdev_stats *stats)
{
	struct rte_bbdev *dev = &rte_bbdev_devices[dev_id];
	struct rte_bbdev_stats *q_stats;

	if (queue_id >= dev->data->num_queues)
		return -1;

	q_stats = &dev->data->queues[queue_id].queue_stats;

	stats->enqueued_count = q_stats->enqueued_count;
	stats->dequeued_count = q_stats->dequeued_count;
	stats->enqueue_err_count = q_stats->enqueue_err_count;
	stats->dequeue_err_count = q_stats->dequeue_err_count;
	stats->offload_time = q_stats->offload_time;

	return 0;
}

static int
offload_latency_test_dec(struct rte_mempool *mempool, struct test_buffers *bufs,
		struct rte_bbdev_dec_op *ref_op, uint16_t dev_id,
		uint16_t queue_id, const uint16_t num_to_process,
		uint16_t burst_sz, struct test_time_stats *time_st)
{
	int i, dequeued, ret;
	struct rte_bbdev_dec_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t enq_start_time, deq_start_time;
	uint64_t enq_sw_last_time, deq_last_time;
	struct rte_bbdev_stats stats;

	for (i = 0, dequeued = 0; dequeued < num_to_process; ++i) {
		uint16_t enq = 0, deq = 0;

		if (unlikely(num_to_process - dequeued < burst_sz))
			burst_sz = num_to_process - dequeued;

		rte_bbdev_dec_op_alloc_bulk(mempool, ops_enq, burst_sz);
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_dec_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					bufs->soft_outputs,
					ref_op);

		/* Start time meas for enqueue function offload latency */
		enq_start_time = rte_rdtsc_precise();
		do {
			enq += rte_bbdev_enqueue_dec_ops(dev_id, queue_id,
					&ops_enq[enq], burst_sz - enq);
		} while (unlikely(burst_sz != enq));

		ret = get_bbdev_queue_stats(dev_id, queue_id, &stats);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to get stats for queue (%u) of device (%u)",
				queue_id, dev_id);

		enq_sw_last_time = rte_rdtsc_precise() - enq_start_time -
				stats.offload_time;
		time_st->enq_sw_max_time = RTE_MAX(time_st->enq_sw_max_time,
				enq_sw_last_time);
		time_st->enq_sw_min_time = RTE_MIN(time_st->enq_sw_min_time,
				enq_sw_last_time);
		time_st->enq_sw_tot_time += enq_sw_last_time;

		time_st->enq_tur_max_time = RTE_MAX(time_st->enq_tur_max_time,
				stats.offload_time);
		time_st->enq_tur_min_time = RTE_MIN(time_st->enq_tur_min_time,
				stats.offload_time);
		time_st->enq_tur_tot_time += stats.offload_time;

		/* ensure enqueue has been completed */
		rte_delay_ms(10);

		/* Start time meas for dequeue function offload latency */
		deq_start_time = rte_rdtsc_precise();
		/* Dequeue one operation */
		do {
			deq += rte_bbdev_dequeue_dec_ops(dev_id, queue_id,
					&ops_deq[deq], 1);
		} while (unlikely(deq != 1));

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		time_st->deq_max_time = RTE_MAX(time_st->deq_max_time,
				deq_last_time);
		time_st->deq_min_time = RTE_MIN(time_st->deq_min_time,
				deq_last_time);
		time_st->deq_tot_time += deq_last_time;

		/* Dequeue remaining operations if needed*/
		while (burst_sz != deq)
			deq += rte_bbdev_dequeue_dec_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);

		rte_bbdev_dec_op_free_bulk(ops_enq, deq);
		dequeued += deq;
	}

	return i;
}

static int
offload_latency_test_enc(struct rte_mempool *mempool, struct test_buffers *bufs,
		struct rte_bbdev_enc_op *ref_op, uint16_t dev_id,
		uint16_t queue_id, const uint16_t num_to_process,
		uint16_t burst_sz, struct test_time_stats *time_st)
{
	int i, dequeued, ret;
	struct rte_bbdev_enc_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t enq_start_time, deq_start_time;
	uint64_t enq_sw_last_time, deq_last_time;
	struct rte_bbdev_stats stats;

	for (i = 0, dequeued = 0; dequeued < num_to_process; ++i) {
		uint16_t enq = 0, deq = 0;

		if (unlikely(num_to_process - dequeued < burst_sz))
			burst_sz = num_to_process - dequeued;

		rte_bbdev_enc_op_alloc_bulk(mempool, ops_enq, burst_sz);
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_enc_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					ref_op);

		/* Start time meas for enqueue function offload latency */
		enq_start_time = rte_rdtsc_precise();
		do {
			enq += rte_bbdev_enqueue_enc_ops(dev_id, queue_id,
					&ops_enq[enq], burst_sz - enq);
		} while (unlikely(burst_sz != enq));

		ret = get_bbdev_queue_stats(dev_id, queue_id, &stats);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to get stats for queue (%u) of device (%u)",
				queue_id, dev_id);

		enq_sw_last_time = rte_rdtsc_precise() - enq_start_time -
				stats.offload_time;
		time_st->enq_sw_max_time = RTE_MAX(time_st->enq_sw_max_time,
				enq_sw_last_time);
		time_st->enq_sw_min_time = RTE_MIN(time_st->enq_sw_min_time,
				enq_sw_last_time);
		time_st->enq_sw_tot_time += enq_sw_last_time;

		time_st->enq_tur_max_time = RTE_MAX(time_st->enq_tur_max_time,
				stats.offload_time);
		time_st->enq_tur_min_time = RTE_MIN(time_st->enq_tur_min_time,
				stats.offload_time);
		time_st->enq_tur_tot_time += stats.offload_time;

		/* ensure enqueue has been completed */
		rte_delay_ms(10);

		/* Start time meas for dequeue function offload latency */
		deq_start_time = rte_rdtsc_precise();
		/* Dequeue one operation */
		do {
			deq += rte_bbdev_dequeue_enc_ops(dev_id, queue_id,
					&ops_deq[deq], 1);
		} while (unlikely(deq != 1));

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		time_st->deq_max_time = RTE_MAX(time_st->deq_max_time,
				deq_last_time);
		time_st->deq_min_time = RTE_MIN(time_st->deq_min_time,
				deq_last_time);
		time_st->deq_tot_time += deq_last_time;

		while (burst_sz != deq)
			deq += rte_bbdev_dequeue_enc_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);

		rte_bbdev_enc_op_free_bulk(ops_enq, deq);
		dequeued += deq;
	}

	return i;
}
#endif

static int
offload_cost_test(struct active_device *ad,
		struct test_op_params *op_params)
{
#ifndef RTE_BBDEV_OFFLOAD_COST
	RTE_SET_USED(ad);
	RTE_SET_USED(op_params);
	printf("Offload latency test is disabled.\n");
	printf("Set RTE_BBDEV_OFFLOAD_COST to 'y' to turn the test on.\n");
	return TEST_SKIPPED;
#else
	int iter;
	uint16_t burst_sz = op_params->burst_sz;
	const uint16_t num_to_process = op_params->num_to_process;
	const enum rte_bbdev_op_type op_type = test_vector.op_type;
	const uint16_t queue_id = ad->queue_ids[0];
	struct test_buffers *bufs = NULL;
	struct rte_bbdev_info info;
	const char *op_type_str;
	struct test_time_stats time_st;

	memset(&time_st, 0, sizeof(struct test_time_stats));
	time_st.enq_sw_min_time = UINT64_MAX;
	time_st.enq_tur_min_time = UINT64_MAX;
	time_st.deq_min_time = UINT64_MAX;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(ad->dev_id, &info);
	bufs = &op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	op_type_str = rte_bbdev_op_type_str(op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u", op_type);

	printf(
		"Offload latency test: dev: %s, burst size: %u, num ops: %u, op type: %s\n",
			info.dev_name, burst_sz, num_to_process, op_type_str);

	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		iter = offload_latency_test_dec(op_params->mp, bufs,
				op_params->ref_dec_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &time_st);
	else
		iter = offload_latency_test_enc(op_params->mp, bufs,
				op_params->ref_enc_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &time_st);

	if (iter <= 0)
		return TEST_FAILED;

	printf("\tenq offload cost latency:\n"
			"\t\tsoftware avg %lg cycles, %lg us\n"
			"\t\tsoftware min %lg cycles, %lg us\n"
			"\t\tsoftware max %lg cycles, %lg us\n"
			"\t\tturbo avg %lg cycles, %lg us\n"
			"\t\tturbo min %lg cycles, %lg us\n"
			"\t\tturbo max %lg cycles, %lg us\n",
			(double)time_st.enq_sw_tot_time / (double)iter,
			(double)(time_st.enq_sw_tot_time * 1000000) /
			(double)iter / (double)rte_get_tsc_hz(),
			(double)time_st.enq_sw_min_time,
			(double)(time_st.enq_sw_min_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.enq_sw_max_time,
			(double)(time_st.enq_sw_max_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.enq_tur_tot_time /
			(double)iter,
			(double)(time_st.enq_tur_tot_time * 1000000) /
			(double)iter / (double)rte_get_tsc_hz(),
			(double)time_st.enq_tur_min_time,
			(double)(time_st.enq_tur_min_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.enq_tur_max_time,
			(double)(time_st.enq_tur_max_time * 1000000) /
			rte_get_tsc_hz());

	printf("\tdeq offload cost latency - one op:\n"
			"\t\tavg %lg cycles, %lg us\n"
			"\t\tmin %lg cycles, %lg us\n"
			"\t\tmax %lg cycles, %lg us\n",
			(double)time_st.deq_tot_time / (double)iter,
			(double)(time_st.deq_tot_time * 1000000) /
			(double)iter / (double)rte_get_tsc_hz(),
			(double)time_st.deq_min_time,
			(double)(time_st.deq_min_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.deq_max_time,
			(double)(time_st.deq_max_time * 1000000) /
			rte_get_tsc_hz());

	return TEST_SUCCESS;
#endif
}

#ifdef RTE_BBDEV_OFFLOAD_COST
static int
offload_latency_empty_q_test_dec(uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *deq_tot_time, uint64_t *deq_min_time,
		uint64_t *deq_max_time)
{
	int i, deq_total;
	struct rte_bbdev_dec_op *ops[MAX_BURST];
	uint64_t deq_start_time, deq_last_time;

	/* Test deq offload latency from an empty queue */

	for (i = 0, deq_total = 0; deq_total < num_to_process;
			++i, deq_total += burst_sz) {
		deq_start_time = rte_rdtsc_precise();

		if (unlikely(num_to_process - deq_total < burst_sz))
			burst_sz = num_to_process - deq_total;
		rte_bbdev_dequeue_dec_ops(dev_id, queue_id, ops, burst_sz);

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		*deq_max_time = RTE_MAX(*deq_max_time, deq_last_time);
		*deq_min_time = RTE_MIN(*deq_min_time, deq_last_time);
		*deq_tot_time += deq_last_time;
	}

	return i;
}

static int
offload_latency_empty_q_test_enc(uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *deq_tot_time, uint64_t *deq_min_time,
		uint64_t *deq_max_time)
{
	int i, deq_total;
	struct rte_bbdev_enc_op *ops[MAX_BURST];
	uint64_t deq_start_time, deq_last_time;

	/* Test deq offload latency from an empty queue */
	for (i = 0, deq_total = 0; deq_total < num_to_process;
			++i, deq_total += burst_sz) {
		deq_start_time = rte_rdtsc_precise();

		if (unlikely(num_to_process - deq_total < burst_sz))
			burst_sz = num_to_process - deq_total;
		rte_bbdev_dequeue_enc_ops(dev_id, queue_id, ops, burst_sz);

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		*deq_max_time = RTE_MAX(*deq_max_time, deq_last_time);
		*deq_min_time = RTE_MIN(*deq_min_time, deq_last_time);
		*deq_tot_time += deq_last_time;
	}

	return i;
}
#endif

static int
offload_latency_empty_q_test(struct active_device *ad,
		struct test_op_params *op_params)
{
#ifndef RTE_BBDEV_OFFLOAD_COST
	RTE_SET_USED(ad);
	RTE_SET_USED(op_params);
	printf("Offload latency empty dequeue test is disabled.\n");
	printf("Set RTE_BBDEV_OFFLOAD_COST to 'y' to turn the test on.\n");
	return TEST_SKIPPED;
#else
	int iter;
	uint64_t deq_tot_time, deq_min_time, deq_max_time;
	uint16_t burst_sz = op_params->burst_sz;
	const uint16_t num_to_process = op_params->num_to_process;
	const enum rte_bbdev_op_type op_type = test_vector.op_type;
	const uint16_t queue_id = ad->queue_ids[0];
	struct rte_bbdev_info info;
	const char *op_type_str;

	deq_tot_time = deq_max_time = 0;
	deq_min_time = UINT64_MAX;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(ad->dev_id, &info);

	op_type_str = rte_bbdev_op_type_str(op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u", op_type);

	printf(
		"Offload latency empty dequeue test: dev: %s, burst size: %u, num ops: %u, op type: %s\n",
			info.dev_name, burst_sz, num_to_process, op_type_str);

	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		iter = offload_latency_empty_q_test_dec(ad->dev_id, queue_id,
				num_to_process, burst_sz, &deq_tot_time,
				&deq_min_time, &deq_max_time);
	else
		iter = offload_latency_empty_q_test_enc(ad->dev_id, queue_id,
				num_to_process, burst_sz, &deq_tot_time,
				&deq_min_time, &deq_max_time);

	if (iter <= 0)
		return TEST_FAILED;

	printf("\tempty deq offload\n"
			"\t\tavg. latency: %lg cycles, %lg us\n"
			"\t\tmin. latency: %lg cycles, %lg us\n"
			"\t\tmax. latency: %lg cycles, %lg us\n",
			(double)deq_tot_time / (double)iter,
			(double)(deq_tot_time * 1000000) / (double)iter /
			(double)rte_get_tsc_hz(), (double)deq_min_time,
			(double)(deq_min_time * 1000000) / rte_get_tsc_hz(),
			(double)deq_max_time, (double)(deq_max_time * 1000000) /
			rte_get_tsc_hz());

	return TEST_SUCCESS;
#endif
}

static int
throughput_tc(void)
{
	return run_test_case(throughput_test);
}

static int
offload_cost_tc(void)
{
	return run_test_case(offload_cost_test);
}

static int
offload_latency_empty_q_tc(void)
{
	return run_test_case(offload_latency_empty_q_test);
}

static int
latency_tc(void)
{
	return run_test_case(latency_test);
}

static int
interrupt_tc(void)
{
	return run_test_case(throughput_test);
}

static struct unit_test_suite bbdev_throughput_testsuite = {
	.suite_name = "BBdev Throughput Tests",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, throughput_tc),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite bbdev_validation_testsuite = {
	.suite_name = "BBdev Validation Tests",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, latency_tc),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite bbdev_latency_testsuite = {
	.suite_name = "BBdev Latency Tests",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, latency_tc),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite bbdev_offload_cost_testsuite = {
	.suite_name = "BBdev Offload Cost Tests",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, offload_cost_tc),
		TEST_CASE_ST(ut_setup, ut_teardown, offload_latency_empty_q_tc),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite bbdev_interrupt_testsuite = {
	.suite_name = "BBdev Interrupt Tests",
	.setup = interrupt_testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, interrupt_tc),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

REGISTER_TEST_COMMAND(throughput, bbdev_throughput_testsuite);
REGISTER_TEST_COMMAND(validation, bbdev_validation_testsuite);
REGISTER_TEST_COMMAND(latency, bbdev_latency_testsuite);
REGISTER_TEST_COMMAND(offload, bbdev_offload_cost_testsuite);
REGISTER_TEST_COMMAND(interrupt, bbdev_interrupt_testsuite);
