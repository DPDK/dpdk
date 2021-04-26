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
#include <rte_interrupts.h>

#include "main.h"
#include "test_bbdev_vector.h"

#define GET_SOCKET(socket_id) (((socket_id) == SOCKET_ID_ANY) ? 0 : (socket_id))

#define MAX_QUEUES RTE_MAX_LCORE
#define TEST_REPETITIONS 100
#define WAIT_OFFLOAD_US 1000

#ifdef RTE_BASEBAND_FPGA_LTE_FEC
#include <fpga_lte_fec.h>
#define FPGA_LTE_PF_DRIVER_NAME ("intel_fpga_lte_fec_pf")
#define FPGA_LTE_VF_DRIVER_NAME ("intel_fpga_lte_fec_vf")
#define VF_UL_4G_QUEUE_VALUE 4
#define VF_DL_4G_QUEUE_VALUE 4
#define UL_4G_BANDWIDTH 3
#define DL_4G_BANDWIDTH 3
#define UL_4G_LOAD_BALANCE 128
#define DL_4G_LOAD_BALANCE 128
#define FLR_4G_TIMEOUT 610
#endif

#ifdef RTE_BASEBAND_FPGA_5GNR_FEC
#include <rte_pmd_fpga_5gnr_fec.h>
#define FPGA_5GNR_PF_DRIVER_NAME ("intel_fpga_5gnr_fec_pf")
#define FPGA_5GNR_VF_DRIVER_NAME ("intel_fpga_5gnr_fec_vf")
#define VF_UL_5G_QUEUE_VALUE 4
#define VF_DL_5G_QUEUE_VALUE 4
#define UL_5G_BANDWIDTH 3
#define DL_5G_BANDWIDTH 3
#define UL_5G_LOAD_BALANCE 128
#define DL_5G_LOAD_BALANCE 128
#define FLR_5G_TIMEOUT 610
#endif

#ifdef RTE_BASEBAND_ACC100
#include <rte_acc100_cfg.h>
#define ACC100PF_DRIVER_NAME   ("intel_acc100_pf")
#define ACC100VF_DRIVER_NAME   ("intel_acc100_vf")
#define ACC100_QMGR_NUM_AQS 16
#define ACC100_QMGR_NUM_QGS 2
#define ACC100_QMGR_AQ_DEPTH 5
#define ACC100_QMGR_INVALID_IDX -1
#define ACC100_QMGR_RR 1
#define ACC100_QOS_GBR 0
#endif

#define OPS_CACHE_SIZE 256U
#define OPS_POOL_SIZE_MIN 511U /* 0.5K per queue */

#define SYNC_WAIT 0
#define SYNC_START 1
#define INVALID_OPAQUE -1

#define INVALID_QUEUE_ID -1
/* Increment for next code block in external HARQ memory */
#define HARQ_INCR 32768
/* Headroom for filler LLRs insertion in HARQ buffer */
#define FILLER_HEADROOM 1024
/* Constants from K0 computation from 3GPP 38.212 Table 5.4.2.1-2 */
#define N_ZC_1 66 /* N = 66 Zc for BG 1 */
#define N_ZC_2 50 /* N = 50 Zc for BG 2 */
#define K0_1_1 17 /* K0 fraction numerator for rv 1 and BG 1 */
#define K0_1_2 13 /* K0 fraction numerator for rv 1 and BG 2 */
#define K0_2_1 33 /* K0 fraction numerator for rv 2 and BG 1 */
#define K0_2_2 25 /* K0 fraction numerator for rv 2 and BG 2 */
#define K0_3_1 56 /* K0 fraction numerator for rv 3 and BG 1 */
#define K0_3_2 43 /* K0 fraction numerator for rv 3 and BG 2 */

static struct test_bbdev_vector test_vector;

/* Switch between PMD and Interrupt for throughput TC */
static bool intr_enabled;

/* LLR arithmetic representation for numerical conversion */
static int ldpc_llr_decimals;
static int ldpc_llr_size;
/* Keep track of the LDPC decoder device capability flag */
static uint32_t ldpc_cap_flags;

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
	struct rte_mempool *harq_in_mbuf_pool;
	struct rte_mempool *harq_out_mbuf_pool;
} active_devs[RTE_BBDEV_MAX_DEVS];

static uint8_t nb_active_devs;

/* Data buffers used by BBDEV ops */
struct test_buffers {
	struct rte_bbdev_op_data *inputs;
	struct rte_bbdev_op_data *hard_outputs;
	struct rte_bbdev_op_data *soft_outputs;
	struct rte_bbdev_op_data *harq_inputs;
	struct rte_bbdev_op_data *harq_outputs;
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
	uint32_t lcore_id;
	uint64_t start_time;
	double ops_per_sec;
	double mbps;
	uint8_t iter_count;
	double iter_average;
	double bler;
	rte_atomic16_t nb_dequeued;
	rte_atomic16_t processing_status;
	rte_atomic16_t burst_sz;
	struct test_op_params *op_params;
	struct rte_bbdev_dec_op *dec_ops[MAX_BURST];
	struct rte_bbdev_enc_op *enc_ops[MAX_BURST];
};

#ifdef RTE_BBDEV_OFFLOAD_COST
/* Stores time statistics */
struct test_time_stats {
	/* Stores software enqueue total working time */
	uint64_t enq_sw_total_time;
	/* Stores minimum value of software enqueue working time */
	uint64_t enq_sw_min_time;
	/* Stores maximum value of software enqueue working time */
	uint64_t enq_sw_max_time;
	/* Stores turbo enqueue total working time */
	uint64_t enq_acc_total_time;
	/* Stores minimum value of accelerator enqueue working time */
	uint64_t enq_acc_min_time;
	/* Stores maximum value of accelerator enqueue working time */
	uint64_t enq_acc_max_time;
	/* Stores dequeue total working time */
	uint64_t deq_total_time;
	/* Stores minimum value of dequeue working time */
	uint64_t deq_min_time;
	/* Stores maximum value of dequeue working time */
	uint64_t deq_max_time;
};
#endif

typedef int (test_case_function)(struct active_device *ad,
		struct test_op_params *op_params);

static inline void
mbuf_reset(struct rte_mbuf *m)
{
	m->pkt_len = 0;

	do {
		m->data_len = 0;
		m = m->next;
	} while (m != NULL);
}

/* Read flag value 0/1 from bitmap */
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
	return bitmap & bitmask;
}

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
	unsigned int nb_inputs, nb_soft_outputs, nb_hard_outputs,
		nb_harq_inputs, nb_harq_outputs;
	const struct rte_bbdev_op_cap *op_cap = dev_info->drv.capabilities;

	nb_inputs = test_vector.entries[DATA_INPUT].nb_segments;
	nb_soft_outputs = test_vector.entries[DATA_SOFT_OUTPUT].nb_segments;
	nb_hard_outputs = test_vector.entries[DATA_HARD_OUTPUT].nb_segments;
	nb_harq_inputs  = test_vector.entries[DATA_HARQ_INPUT].nb_segments;
	nb_harq_outputs = test_vector.entries[DATA_HARQ_OUTPUT].nb_segments;

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
					"INFO: Device \"%s\" does not support soft output - soft output flags will be ignored.\n",
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
					nb_hard_outputs, cap->num_buffers_dst);
				return TEST_FAILED;
			}
			if (intr_enabled && !(cap->capability_flags &
					RTE_BBDEV_TURBO_ENC_INTERRUPTS)) {
				printf(
					"Dequeue interrupts are not supported!\n");
				return TEST_FAILED;
			}

			return TEST_SUCCESS;
		} else if (op_cap->type == RTE_BBDEV_OP_LDPC_ENC) {
			const struct rte_bbdev_op_cap_ldpc_enc *cap =
					&op_cap->cap.ldpc_enc;

			if (!flags_match(test_vector.ldpc_enc.op_flags,
					cap->capability_flags)){
				printf("Flag Mismatch\n");
				return TEST_FAILED;
			}
			if (nb_inputs > cap->num_buffers_src) {
				printf("Too many inputs defined: %u, max: %u\n",
					nb_inputs, cap->num_buffers_src);
				return TEST_FAILED;
			}
			if (nb_hard_outputs > cap->num_buffers_dst) {
				printf(
					"Too many hard outputs defined: %u, max: %u\n",
					nb_hard_outputs, cap->num_buffers_dst);
				return TEST_FAILED;
			}
			if (intr_enabled && !(cap->capability_flags &
					RTE_BBDEV_LDPC_ENC_INTERRUPTS)) {
				printf(
					"Dequeue interrupts are not supported!\n");
				return TEST_FAILED;
			}

			return TEST_SUCCESS;
		} else if (op_cap->type == RTE_BBDEV_OP_LDPC_DEC) {
			const struct rte_bbdev_op_cap_ldpc_dec *cap =
					&op_cap->cap.ldpc_dec;

			if (!flags_match(test_vector.ldpc_dec.op_flags,
					cap->capability_flags)){
				printf("Flag Mismatch\n");
				return TEST_FAILED;
			}
			if (nb_inputs > cap->num_buffers_src) {
				printf("Too many inputs defined: %u, max: %u\n",
					nb_inputs, cap->num_buffers_src);
				return TEST_FAILED;
			}
			if (nb_hard_outputs > cap->num_buffers_hard_out) {
				printf(
					"Too many hard outputs defined: %u, max: %u\n",
					nb_hard_outputs,
					cap->num_buffers_hard_out);
				return TEST_FAILED;
			}
			if (nb_harq_inputs > cap->num_buffers_hard_out) {
				printf(
					"Too many HARQ inputs defined: %u, max: %u\n",
					nb_harq_inputs,
					cap->num_buffers_hard_out);
				return TEST_FAILED;
			}
			if (nb_harq_outputs > cap->num_buffers_hard_out) {
				printf(
					"Too many HARQ outputs defined: %u, max: %u\n",
					nb_harq_outputs,
					cap->num_buffers_hard_out);
				return TEST_FAILED;
			}
			if (intr_enabled && !(cap->capability_flags &
					RTE_BBDEV_LDPC_DEC_INTERRUPTS)) {
				printf(
					"Dequeue interrupts are not supported!\n");
				return TEST_FAILED;
			}
			if (intr_enabled && (test_vector.ldpc_dec.op_flags &
				(RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE |
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK
					))) {
				printf("Skip loop-back with interrupt\n");
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
			RTE_MAX(max_seg_sz + RTE_PKTMBUF_HEADROOM
					+ FILLER_HEADROOM,
			(unsigned int)RTE_MBUF_DEFAULT_BUF_SIZE), socket_id);
}

static int
create_mempools(struct active_device *ad, int socket_id,
		enum rte_bbdev_op_type org_op_type, uint16_t num_ops)
{
	struct rte_mempool *mp;
	unsigned int ops_pool_size, mbuf_pool_size = 0;
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	const char *op_type_str;
	enum rte_bbdev_op_type op_type = org_op_type;

	struct op_data_entries *in = &test_vector.entries[DATA_INPUT];
	struct op_data_entries *hard_out =
			&test_vector.entries[DATA_HARD_OUTPUT];
	struct op_data_entries *soft_out =
			&test_vector.entries[DATA_SOFT_OUTPUT];
	struct op_data_entries *harq_in =
			&test_vector.entries[DATA_HARQ_INPUT];
	struct op_data_entries *harq_out =
			&test_vector.entries[DATA_HARQ_OUTPUT];

	/* allocate ops mempool */
	ops_pool_size = optimal_mempool_size(RTE_MAX(
			/* Ops used plus 1 reference op */
			RTE_MAX((unsigned int)(ad->nb_queues * num_ops + 1),
			/* Minimal cache size plus 1 reference op */
			(unsigned int)(1.5 * rte_lcore_count() *
					OPS_CACHE_SIZE + 1)),
			OPS_POOL_SIZE_MIN));

	if (org_op_type == RTE_BBDEV_OP_NONE)
		op_type = RTE_BBDEV_OP_TURBO_ENC;

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

	/* Do not create inputs and outputs mbufs for BaseBand Null Device */
	if (org_op_type == RTE_BBDEV_OP_NONE)
		return TEST_SUCCESS;

	/* Inputs */
	if (in->nb_segments > 0) {
		mbuf_pool_size = optimal_mempool_size(ops_pool_size *
				in->nb_segments);
		mp = create_mbuf_pool(in, ad->dev_id, socket_id,
				mbuf_pool_size, "in");
		TEST_ASSERT_NOT_NULL(mp,
				"ERROR Failed to create %u items input pktmbuf pool for dev %u on socket %u.",
				mbuf_pool_size,
				ad->dev_id,
				socket_id);
		ad->in_mbuf_pool = mp;
	}

	/* Hard outputs */
	if (hard_out->nb_segments > 0) {
		mbuf_pool_size = optimal_mempool_size(ops_pool_size *
				hard_out->nb_segments);
		mp = create_mbuf_pool(hard_out, ad->dev_id, socket_id,
				mbuf_pool_size,
				"hard_out");
		TEST_ASSERT_NOT_NULL(mp,
				"ERROR Failed to create %u items hard output pktmbuf pool for dev %u on socket %u.",
				mbuf_pool_size,
				ad->dev_id,
				socket_id);
		ad->hard_out_mbuf_pool = mp;
	}

	/* Soft outputs */
	if (soft_out->nb_segments > 0) {
		mbuf_pool_size = optimal_mempool_size(ops_pool_size *
				soft_out->nb_segments);
		mp = create_mbuf_pool(soft_out, ad->dev_id, socket_id,
				mbuf_pool_size,
				"soft_out");
		TEST_ASSERT_NOT_NULL(mp,
				"ERROR Failed to create %uB soft output pktmbuf pool for dev %u on socket %u.",
				mbuf_pool_size,
				ad->dev_id,
				socket_id);
		ad->soft_out_mbuf_pool = mp;
	}

	/* HARQ inputs */
	if (harq_in->nb_segments > 0) {
		mbuf_pool_size = optimal_mempool_size(ops_pool_size *
				harq_in->nb_segments);
		mp = create_mbuf_pool(harq_in, ad->dev_id, socket_id,
				mbuf_pool_size,
				"harq_in");
		TEST_ASSERT_NOT_NULL(mp,
				"ERROR Failed to create %uB harq input pktmbuf pool for dev %u on socket %u.",
				mbuf_pool_size,
				ad->dev_id,
				socket_id);
		ad->harq_in_mbuf_pool = mp;
	}

	/* HARQ outputs */
	if (harq_out->nb_segments > 0) {
		mbuf_pool_size = optimal_mempool_size(ops_pool_size *
				harq_out->nb_segments);
		mp = create_mbuf_pool(harq_out, ad->dev_id, socket_id,
				mbuf_pool_size,
				"harq_out");
		TEST_ASSERT_NOT_NULL(mp,
				"ERROR Failed to create %uB harq output pktmbuf pool for dev %u on socket %u.",
				mbuf_pool_size,
				ad->dev_id,
				socket_id);
		ad->harq_out_mbuf_pool = mp;
	}

	return TEST_SUCCESS;
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

/* Configure fpga lte fec with PF & VF values
 * if '-i' flag is set and using fpga device
 */
#ifdef RTE_BASEBAND_FPGA_LTE_FEC
	if ((get_init_device() == true) &&
		(!strcmp(info->drv.driver_name, FPGA_LTE_PF_DRIVER_NAME))) {
		struct rte_fpga_lte_fec_conf conf;
		unsigned int i;

		printf("Configure FPGA LTE FEC Driver %s with default values\n",
				info->drv.driver_name);

		/* clear default configuration before initialization */
		memset(&conf, 0, sizeof(struct rte_fpga_lte_fec_conf));

		/* Set PF mode :
		 * true if PF is used for data plane
		 * false for VFs
		 */
		conf.pf_mode_en = true;

		for (i = 0; i < FPGA_LTE_FEC_NUM_VFS; ++i) {
			/* Number of UL queues per VF (fpga supports 8 VFs) */
			conf.vf_ul_queues_number[i] = VF_UL_4G_QUEUE_VALUE;
			/* Number of DL queues per VF (fpga supports 8 VFs) */
			conf.vf_dl_queues_number[i] = VF_DL_4G_QUEUE_VALUE;
		}

		/* UL bandwidth. Needed for schedule algorithm */
		conf.ul_bandwidth = UL_4G_BANDWIDTH;
		/* DL bandwidth */
		conf.dl_bandwidth = DL_4G_BANDWIDTH;

		/* UL & DL load Balance Factor to 64 */
		conf.ul_load_balance = UL_4G_LOAD_BALANCE;
		conf.dl_load_balance = DL_4G_LOAD_BALANCE;

		/**< FLR timeout value */
		conf.flr_time_out = FLR_4G_TIMEOUT;

		/* setup FPGA PF with configuration information */
		ret = rte_fpga_lte_fec_configure(info->dev_name, &conf);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to configure 4G FPGA PF for bbdev %s",
				info->dev_name);
	}
#endif
#ifdef RTE_BASEBAND_FPGA_5GNR_FEC
	if ((get_init_device() == true) &&
		(!strcmp(info->drv.driver_name, FPGA_5GNR_PF_DRIVER_NAME))) {
		struct rte_fpga_5gnr_fec_conf conf;
		unsigned int i;

		printf("Configure FPGA 5GNR FEC Driver %s with default values\n",
				info->drv.driver_name);

		/* clear default configuration before initialization */
		memset(&conf, 0, sizeof(struct rte_fpga_5gnr_fec_conf));

		/* Set PF mode :
		 * true if PF is used for data plane
		 * false for VFs
		 */
		conf.pf_mode_en = true;

		for (i = 0; i < FPGA_5GNR_FEC_NUM_VFS; ++i) {
			/* Number of UL queues per VF (fpga supports 8 VFs) */
			conf.vf_ul_queues_number[i] = VF_UL_5G_QUEUE_VALUE;
			/* Number of DL queues per VF (fpga supports 8 VFs) */
			conf.vf_dl_queues_number[i] = VF_DL_5G_QUEUE_VALUE;
		}

		/* UL bandwidth. Needed for schedule algorithm */
		conf.ul_bandwidth = UL_5G_BANDWIDTH;
		/* DL bandwidth */
		conf.dl_bandwidth = DL_5G_BANDWIDTH;

		/* UL & DL load Balance Factor to 64 */
		conf.ul_load_balance = UL_5G_LOAD_BALANCE;
		conf.dl_load_balance = DL_5G_LOAD_BALANCE;

		/**< FLR timeout value */
		conf.flr_time_out = FLR_5G_TIMEOUT;

		/* setup FPGA PF with configuration information */
		ret = rte_fpga_5gnr_fec_configure(info->dev_name, &conf);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to configure 5G FPGA PF for bbdev %s",
				info->dev_name);
	}
#endif
#ifdef RTE_BASEBAND_ACC100
	if ((get_init_device() == true) &&
		(!strcmp(info->drv.driver_name, ACC100PF_DRIVER_NAME))) {
		struct rte_acc100_conf conf;
		unsigned int i;

		printf("Configure ACC100 FEC Driver %s with default values\n",
				info->drv.driver_name);

		/* clear default configuration before initialization */
		memset(&conf, 0, sizeof(struct rte_acc100_conf));

		/* Always set in PF mode for built-in configuration */
		conf.pf_mode_en = true;
		for (i = 0; i < RTE_ACC100_NUM_VFS; ++i) {
			conf.arb_dl_4g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_dl_4g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_dl_4g[i].round_robin_weight = ACC100_QMGR_RR;
			conf.arb_ul_4g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_ul_4g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_ul_4g[i].round_robin_weight = ACC100_QMGR_RR;
			conf.arb_dl_5g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_dl_5g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_dl_5g[i].round_robin_weight = ACC100_QMGR_RR;
			conf.arb_ul_5g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_ul_5g[i].gbr_threshold1 = ACC100_QOS_GBR;
			conf.arb_ul_5g[i].round_robin_weight = ACC100_QMGR_RR;
		}

		conf.input_pos_llr_1_bit = true;
		conf.output_pos_llr_1_bit = true;
		conf.num_vf_bundles = 1; /**< Number of VF bundles to setup */

		conf.q_ul_4g.num_qgroups = ACC100_QMGR_NUM_QGS;
		conf.q_ul_4g.first_qgroup_index = ACC100_QMGR_INVALID_IDX;
		conf.q_ul_4g.num_aqs_per_groups = ACC100_QMGR_NUM_AQS;
		conf.q_ul_4g.aq_depth_log2 = ACC100_QMGR_AQ_DEPTH;
		conf.q_dl_4g.num_qgroups = ACC100_QMGR_NUM_QGS;
		conf.q_dl_4g.first_qgroup_index = ACC100_QMGR_INVALID_IDX;
		conf.q_dl_4g.num_aqs_per_groups = ACC100_QMGR_NUM_AQS;
		conf.q_dl_4g.aq_depth_log2 = ACC100_QMGR_AQ_DEPTH;
		conf.q_ul_5g.num_qgroups = ACC100_QMGR_NUM_QGS;
		conf.q_ul_5g.first_qgroup_index = ACC100_QMGR_INVALID_IDX;
		conf.q_ul_5g.num_aqs_per_groups = ACC100_QMGR_NUM_AQS;
		conf.q_ul_5g.aq_depth_log2 = ACC100_QMGR_AQ_DEPTH;
		conf.q_dl_5g.num_qgroups = ACC100_QMGR_NUM_QGS;
		conf.q_dl_5g.first_qgroup_index = ACC100_QMGR_INVALID_IDX;
		conf.q_dl_5g.num_aqs_per_groups = ACC100_QMGR_NUM_AQS;
		conf.q_dl_5g.aq_depth_log2 = ACC100_QMGR_AQ_DEPTH;

		/* setup PF with configuration information */
		ret = rte_acc100_configure(info->dev_name, &conf);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to configure ACC100 PF for bbdev %s",
				info->dev_name);
	}
#endif
	/* Let's refresh this now this is configured */
	rte_bbdev_info_get(dev_id, info);
	nb_queues = RTE_MIN(rte_lcore_count(), info->drv.max_num_queues);
	nb_queues = RTE_MIN(nb_queues, (unsigned int) MAX_QUEUES);

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

	/* Disable interrupts */
	intr_enabled = false;
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
	bool large_input = false;

	for (i = 0; i < n; ++i) {
		char *data;
		struct op_data_buf *seg = &ref_entries->segments[0];
		struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
		TEST_ASSERT_NOT_NULL(m_head,
				"Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
				op_type, n * ref_entries->nb_segments,
				mbuf_pool->size);

		if (seg->length > RTE_BBDEV_LDPC_E_MAX_MBUF) {
			/*
			 * Special case when DPDK mbuf cannot handle
			 * the required input size
			 */
			printf("Warning: Larger input size than DPDK mbuf %d\n",
					seg->length);
			large_input = true;
		}
		bufs[i].data = m_head;
		bufs[i].offset = 0;
		bufs[i].length = 0;

		if ((op_type == DATA_INPUT) || (op_type == DATA_HARQ_INPUT)) {
			if ((op_type == DATA_INPUT) && large_input) {
				/* Allocate a fake overused mbuf */
				data = rte_malloc(NULL, seg->length, 0);
				TEST_ASSERT_NOT_NULL(data,
					"rte malloc failed with %u bytes",
					seg->length);
				memcpy(data, seg->addr, seg->length);
				m_head->buf_addr = data;
				m_head->buf_iova = rte_malloc_virt2iova(data);
				m_head->data_off = 0;
				m_head->data_len = seg->length;
			} else {
				data = rte_pktmbuf_append(m_head, seg->length);
				TEST_ASSERT_NOT_NULL(data,
					"Couldn't append %u bytes to mbuf from %d data type mbuf pool",
					seg->length, op_type);

				TEST_ASSERT(data == RTE_PTR_ALIGN(
						data, min_alignment),
					"Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
					data, min_alignment);
				rte_memcpy(data, seg->addr, seg->length);
			}

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
		} else {

			/* allocate chained-mbuf for output buffer */
			for (j = 1; j < ref_entries->nb_segments; ++j) {
				struct rte_mbuf *m_tail =
						rte_pktmbuf_alloc(mbuf_pool);
				TEST_ASSERT_NOT_NULL(m_tail,
						"Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
						op_type,
						n * ref_entries->nb_segments,
						mbuf_pool->size);

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
		const uint16_t n, const int8_t max_llr_modulus)
{
	uint16_t i, byte_idx;

	for (i = 0; i < n; ++i) {
		struct rte_mbuf *m = input_ops[i].data;
		while (m != NULL) {
			int8_t *llr = rte_pktmbuf_mtod_offset(m, int8_t *,
					input_ops[i].offset);
			for (byte_idx = 0; byte_idx < rte_pktmbuf_data_len(m);
					++byte_idx)
				llr[byte_idx] = round((double)max_llr_modulus *
						llr[byte_idx] / INT8_MAX);

			m = m->next;
		}
	}
}

/*
 * We may have to insert filler bits
 * when they are required by the HARQ assumption
 */
static void
ldpc_add_filler(struct rte_bbdev_op_data *input_ops,
		const uint16_t n, struct test_op_params *op_params)
{
	struct rte_bbdev_op_ldpc_dec dec = op_params->ref_dec_op->ldpc_dec;

	if (input_ops == NULL)
		return;
	/* No need to add filler if not required by device */
	if (!(ldpc_cap_flags &
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS))
		return;
	/* No need to add filler for loopback operation */
	if (dec.op_flags & RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)
		return;

	uint16_t i, j, parity_offset;
	for (i = 0; i < n; ++i) {
		struct rte_mbuf *m = input_ops[i].data;
		int8_t *llr = rte_pktmbuf_mtod_offset(m, int8_t *,
				input_ops[i].offset);
		parity_offset = (dec.basegraph == 1 ? 20 : 8)
				* dec.z_c - dec.n_filler;
		uint16_t new_hin_size = input_ops[i].length + dec.n_filler;
		m->data_len = new_hin_size;
		input_ops[i].length = new_hin_size;
		for (j = new_hin_size - 1; j >= parity_offset + dec.n_filler;
				j--)
			llr[j] = llr[j - dec.n_filler];
		uint16_t llr_max_pre_scaling = (1 << (ldpc_llr_size - 1)) - 1;
		for (j = 0; j < dec.n_filler; j++)
			llr[parity_offset + j] = llr_max_pre_scaling;
	}
}

static void
ldpc_input_llr_scaling(struct rte_bbdev_op_data *input_ops,
		const uint16_t n, const int8_t llr_size,
		const int8_t llr_decimals)
{
	if (input_ops == NULL)
		return;

	uint16_t i, byte_idx;

	int16_t llr_max, llr_min, llr_tmp;
	llr_max = (1 << (llr_size - 1)) - 1;
	llr_min = -llr_max;
	for (i = 0; i < n; ++i) {
		struct rte_mbuf *m = input_ops[i].data;
		while (m != NULL) {
			int8_t *llr = rte_pktmbuf_mtod_offset(m, int8_t *,
					input_ops[i].offset);
			for (byte_idx = 0; byte_idx < rte_pktmbuf_data_len(m);
					++byte_idx) {

				llr_tmp = llr[byte_idx];
				if (llr_decimals == 4)
					llr_tmp *= 8;
				else if (llr_decimals == 2)
					llr_tmp *= 2;
				else if (llr_decimals == 0)
					llr_tmp /= 2;
				llr_tmp = RTE_MIN(llr_max,
						RTE_MAX(llr_min, llr_tmp));
				llr[byte_idx] = (int8_t) llr_tmp;
			}

			m = m->next;
		}
	}
}



static int
fill_queue_buffers(struct test_op_params *op_params,
		struct rte_mempool *in_mp, struct rte_mempool *hard_out_mp,
		struct rte_mempool *soft_out_mp,
		struct rte_mempool *harq_in_mp, struct rte_mempool *harq_out_mp,
		uint16_t queue_id,
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
		harq_in_mp,
		harq_out_mp,
	};

	struct rte_bbdev_op_data **queue_ops[DATA_NUM_TYPES] = {
		&op_params->q_bufs[socket_id][queue_id].inputs,
		&op_params->q_bufs[socket_id][queue_id].soft_outputs,
		&op_params->q_bufs[socket_id][queue_id].hard_outputs,
		&op_params->q_bufs[socket_id][queue_id].harq_inputs,
		&op_params->q_bufs[socket_id][queue_id].harq_outputs,
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

	if (test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC) {
		bool loopback = op_params->ref_dec_op->ldpc_dec.op_flags &
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK;
		bool llr_comp = op_params->ref_dec_op->ldpc_dec.op_flags &
				RTE_BBDEV_LDPC_LLR_COMPRESSION;
		bool harq_comp = op_params->ref_dec_op->ldpc_dec.op_flags &
				RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;
		ldpc_llr_decimals = capabilities->cap.ldpc_dec.llr_decimals;
		ldpc_llr_size = capabilities->cap.ldpc_dec.llr_size;
		ldpc_cap_flags = capabilities->cap.ldpc_dec.capability_flags;
		if (!loopback && !llr_comp)
			ldpc_input_llr_scaling(*queue_ops[DATA_INPUT], n,
					ldpc_llr_size, ldpc_llr_decimals);
		if (!loopback && !harq_comp)
			ldpc_input_llr_scaling(*queue_ops[DATA_HARQ_INPUT], n,
					ldpc_llr_size, ldpc_llr_decimals);
		if (!loopback)
			ldpc_add_filler(*queue_ops[DATA_HARQ_INPUT], n,
					op_params);
	}

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
	rte_mempool_free(ad->harq_in_mbuf_pool);
	rte_mempool_free(ad->harq_out_mbuf_pool);

	for (i = 0; i < rte_lcore_count(); ++i) {
		for (j = 0; j < RTE_MAX_NUMA_NODES; ++j) {
			rte_free(op_params->q_bufs[j][i].inputs);
			rte_free(op_params->q_bufs[j][i].hard_outputs);
			rte_free(op_params->q_bufs[j][i].soft_outputs);
			rte_free(op_params->q_bufs[j][i].harq_inputs);
			rte_free(op_params->q_bufs[j][i].harq_outputs);
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
			ops[i]->turbo_dec.tb_params.r =
					turbo_dec->tb_params.r;
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


/* Returns a random number drawn from a normal distribution
 * with mean of 0 and variance of 1
 * Marsaglia algorithm
 */
static double
randn(int n)
{
	double S, Z, U1, U2, u, v, fac;

	do {
		U1 = (double)rand() / RAND_MAX;
		U2 = (double)rand() / RAND_MAX;
		u = 2. * U1 - 1.;
		v = 2. * U2 - 1.;
		S = u * u + v * v;
	} while (S >= 1 || S == 0);
	fac = sqrt(-2. * log(S) / S);
	Z = (n % 2) ? u * fac : v * fac;
	return Z;
}

static inline double
maxstar(double A, double B)
{
	if (fabs(A - B) > 5)
		return RTE_MAX(A, B);
	else
		return RTE_MAX(A, B) + log1p(exp(-fabs(A - B)));
}

/*
 * Generate Qm LLRS for Qm==8
 * Modulation, AWGN and LLR estimation from max log development
 */
static void
gen_qm8_llr(int8_t *llrs, uint32_t i, double N0, double llr_max)
{
	int qm = 8;
	int qam = 256;
	int m, k;
	double I, Q, p0, p1, llr_, b[qm], log_syml_prob[qam];
	/* 5.1.4 of TS38.211 */
	const double symbols_I[256] = {
			5, 5, 7, 7, 5, 5, 7, 7, 3, 3, 1, 1, 3, 3, 1, 1, 5,
			5, 7, 7, 5, 5, 7, 7, 3, 3, 1, 1, 3, 3, 1, 1, 11,
			11, 9, 9, 11, 11, 9, 9, 13, 13, 15, 15, 13, 13,
			15, 15, 11, 11, 9, 9, 11, 11, 9, 9, 13, 13, 15,
			15, 13, 13, 15, 15, 5, 5, 7, 7, 5, 5, 7, 7, 3, 3,
			1, 1, 3, 3, 1, 1, 5, 5, 7, 7, 5, 5, 7, 7, 3, 3, 1,
			1, 3, 3, 1, 1, 11, 11, 9, 9, 11, 11, 9, 9, 13, 13,
			15, 15, 13, 13, 15, 15, 11, 11, 9, 9, 11, 11, 9, 9,
			13, 13, 15, 15, 13, 13, 15, 15, -5, -5, -7, -7, -5,
			-5, -7, -7, -3, -3, -1, -1, -3, -3, -1, -1, -5, -5,
			-7, -7, -5, -5, -7, -7, -3, -3, -1, -1, -3, -3,
			-1, -1, -11, -11, -9, -9, -11, -11, -9, -9, -13,
			-13, -15, -15, -13, -13, -15, -15, -11, -11, -9,
			-9, -11, -11, -9, -9, -13, -13, -15, -15, -13,
			-13, -15, -15, -5, -5, -7, -7, -5, -5, -7, -7, -3,
			-3, -1, -1, -3, -3, -1, -1, -5, -5, -7, -7, -5, -5,
			-7, -7, -3, -3, -1, -1, -3, -3, -1, -1, -11, -11,
			-9, -9, -11, -11, -9, -9, -13, -13, -15, -15, -13,
			-13, -15, -15, -11, -11, -9, -9, -11, -11, -9, -9,
			-13, -13, -15, -15, -13, -13, -15, -15};
	const double symbols_Q[256] = {
			5, 7, 5, 7, 3, 1, 3, 1, 5, 7, 5, 7, 3, 1, 3, 1, 11,
			9, 11, 9, 13, 15, 13, 15, 11, 9, 11, 9, 13, 15, 13,
			15, 5, 7, 5, 7, 3, 1, 3, 1, 5, 7, 5, 7, 3, 1, 3, 1,
			11, 9, 11, 9, 13, 15, 13, 15, 11, 9, 11, 9, 13,
			15, 13, 15, -5, -7, -5, -7, -3, -1, -3, -1, -5,
			-7, -5, -7, -3, -1, -3, -1, -11, -9, -11, -9, -13,
			-15, -13, -15, -11, -9, -11, -9, -13, -15, -13,
			-15, -5, -7, -5, -7, -3, -1, -3, -1, -5, -7, -5,
			-7, -3, -1, -3, -1, -11, -9, -11, -9, -13, -15,
			-13, -15, -11, -9, -11, -9, -13, -15, -13, -15, 5,
			7, 5, 7, 3, 1, 3, 1, 5, 7, 5, 7, 3, 1, 3, 1, 11,
			9, 11, 9, 13, 15, 13, 15, 11, 9, 11, 9, 13, 15,
			13, 15, 5, 7, 5, 7, 3, 1, 3, 1, 5, 7, 5, 7, 3, 1,
			3, 1, 11, 9, 11, 9, 13, 15, 13, 15, 11, 9, 11, 9,
			13, 15, 13, 15, -5, -7, -5, -7, -3, -1, -3, -1,
			-5, -7, -5, -7, -3, -1, -3, -1, -11, -9, -11, -9,
			-13, -15, -13, -15, -11, -9, -11, -9, -13, -15,
			-13, -15, -5, -7, -5, -7, -3, -1, -3, -1, -5, -7,
			-5, -7, -3, -1, -3, -1, -11, -9, -11, -9, -13, -15,
			-13, -15, -11, -9, -11, -9, -13, -15, -13, -15};
	/* Average constellation point energy */
	N0 *= 170.0;
	for (k = 0; k < qm; k++)
		b[k] = llrs[qm * i + k] < 0 ? 1.0 : 0.0;
	/* 5.1.4 of TS38.211 */
	I = (1 - 2 * b[0]) * (8 - (1 - 2 * b[2]) *
			(4 - (1 - 2 * b[4]) * (2 - (1 - 2 * b[6]))));
	Q = (1 - 2 * b[1]) * (8 - (1 - 2 * b[3]) *
			(4 - (1 - 2 * b[5]) * (2 - (1 - 2 * b[7]))));
	/* AWGN channel */
	I += sqrt(N0 / 2) * randn(0);
	Q += sqrt(N0 / 2) * randn(1);
	/*
	 * Calculate the log of the probability that each of
	 * the constellation points was transmitted
	 */
	for (m = 0; m < qam; m++)
		log_syml_prob[m] = -(pow(I - symbols_I[m], 2.0)
				+ pow(Q - symbols_Q[m], 2.0)) / N0;
	/* Calculate an LLR for each of the k_64QAM bits in the set */
	for (k = 0; k < qm; k++) {
		p0 = -999999;
		p1 = -999999;
		/* For each constellation point */
		for (m = 0; m < qam; m++) {
			if ((m >> (qm - k - 1)) & 1)
				p1 = maxstar(p1, log_syml_prob[m]);
			else
				p0 = maxstar(p0, log_syml_prob[m]);
		}
		/* Calculate the LLR */
		llr_ = p0 - p1;
		llr_ *= (1 << ldpc_llr_decimals);
		llr_ = round(llr_);
		if (llr_ > llr_max)
			llr_ = llr_max;
		if (llr_ < -llr_max)
			llr_ = -llr_max;
		llrs[qm * i + k] = (int8_t) llr_;
	}
}


/*
 * Generate Qm LLRS for Qm==6
 * Modulation, AWGN and LLR estimation from max log development
 */
static void
gen_qm6_llr(int8_t *llrs, uint32_t i, double N0, double llr_max)
{
	int qm = 6;
	int qam = 64;
	int m, k;
	double I, Q, p0, p1, llr_, b[qm], log_syml_prob[qam];
	/* 5.1.4 of TS38.211 */
	const double symbols_I[64] = {
			3, 3, 1, 1, 3, 3, 1, 1, 5, 5, 7, 7, 5, 5, 7, 7,
			3, 3, 1, 1, 3, 3, 1, 1, 5, 5, 7, 7, 5, 5, 7, 7,
			-3, -3, -1, -1, -3, -3, -1, -1, -5, -5, -7, -7,
			-5, -5, -7, -7, -3, -3, -1, -1, -3, -3, -1, -1,
			-5, -5, -7, -7, -5, -5, -7, -7};
	const double symbols_Q[64] = {
			3, 1, 3, 1, 5, 7, 5, 7, 3, 1, 3, 1, 5, 7, 5, 7,
			-3, -1, -3, -1, -5, -7, -5, -7, -3, -1, -3, -1,
			-5, -7, -5, -7, 3, 1, 3, 1, 5, 7, 5, 7, 3, 1, 3, 1,
			5, 7, 5, 7, -3, -1, -3, -1, -5, -7, -5, -7,
			-3, -1, -3, -1, -5, -7, -5, -7};
	/* Average constellation point energy */
	N0 *= 42.0;
	for (k = 0; k < qm; k++)
		b[k] = llrs[qm * i + k] < 0 ? 1.0 : 0.0;
	/* 5.1.4 of TS38.211 */
	I = (1 - 2 * b[0])*(4 - (1 - 2 * b[2]) * (2 - (1 - 2 * b[4])));
	Q = (1 - 2 * b[1])*(4 - (1 - 2 * b[3]) * (2 - (1 - 2 * b[5])));
	/* AWGN channel */
	I += sqrt(N0 / 2) * randn(0);
	Q += sqrt(N0 / 2) * randn(1);
	/*
	 * Calculate the log of the probability that each of
	 * the constellation points was transmitted
	 */
	for (m = 0; m < qam; m++)
		log_syml_prob[m] = -(pow(I - symbols_I[m], 2.0)
				+ pow(Q - symbols_Q[m], 2.0)) / N0;
	/* Calculate an LLR for each of the k_64QAM bits in the set */
	for (k = 0; k < qm; k++) {
		p0 = -999999;
		p1 = -999999;
		/* For each constellation point */
		for (m = 0; m < qam; m++) {
			if ((m >> (qm - k - 1)) & 1)
				p1 = maxstar(p1, log_syml_prob[m]);
			else
				p0 = maxstar(p0, log_syml_prob[m]);
		}
		/* Calculate the LLR */
		llr_ = p0 - p1;
		llr_ *= (1 << ldpc_llr_decimals);
		llr_ = round(llr_);
		if (llr_ > llr_max)
			llr_ = llr_max;
		if (llr_ < -llr_max)
			llr_ = -llr_max;
		llrs[qm * i + k] = (int8_t) llr_;
	}
}

/*
 * Generate Qm LLRS for Qm==4
 * Modulation, AWGN and LLR estimation from max log development
 */
static void
gen_qm4_llr(int8_t *llrs, uint32_t i, double N0, double llr_max)
{
	int qm = 4;
	int qam = 16;
	int m, k;
	double I, Q, p0, p1, llr_, b[qm], log_syml_prob[qam];
	/* 5.1.4 of TS38.211 */
	const double symbols_I[16] = {1, 1, 3, 3, 1, 1, 3, 3,
			-1, -1, -3, -3, -1, -1, -3, -3};
	const double symbols_Q[16] = {1, 3, 1, 3, -1, -3, -1, -3,
			1, 3, 1, 3, -1, -3, -1, -3};
	/* Average constellation point energy */
	N0 *= 10.0;
	for (k = 0; k < qm; k++)
		b[k] = llrs[qm * i + k] < 0 ? 1.0 : 0.0;
	/* 5.1.4 of TS38.211 */
	I = (1 - 2 * b[0]) * (2 - (1 - 2 * b[2]));
	Q = (1 - 2 * b[1]) * (2 - (1 - 2 * b[3]));
	/* AWGN channel */
	I += sqrt(N0 / 2) * randn(0);
	Q += sqrt(N0 / 2) * randn(1);
	/*
	 * Calculate the log of the probability that each of
	 * the constellation points was transmitted
	 */
	for (m = 0; m < qam; m++)
		log_syml_prob[m] = -(pow(I - symbols_I[m], 2.0)
				+ pow(Q - symbols_Q[m], 2.0)) / N0;
	/* Calculate an LLR for each of the k_64QAM bits in the set */
	for (k = 0; k < qm; k++) {
		p0 = -999999;
		p1 = -999999;
		/* For each constellation point */
		for (m = 0; m < qam; m++) {
			if ((m >> (qm - k - 1)) & 1)
				p1 = maxstar(p1, log_syml_prob[m]);
			else
				p0 = maxstar(p0, log_syml_prob[m]);
		}
		/* Calculate the LLR */
		llr_ = p0 - p1;
		llr_ *= (1 << ldpc_llr_decimals);
		llr_ = round(llr_);
		if (llr_ > llr_max)
			llr_ = llr_max;
		if (llr_ < -llr_max)
			llr_ = -llr_max;
		llrs[qm * i + k] = (int8_t) llr_;
	}
}

static void
gen_qm2_llr(int8_t *llrs, uint32_t j, double N0, double llr_max)
{
	double b, b1, n;
	double coeff = 2.0 * sqrt(N0);

	/* Ignore in vectors rare quasi null LLRs not to be saturated */
	if (llrs[j] < 8 && llrs[j] > -8)
		return;

	/* Note don't change sign here */
	n = randn(j % 2);
	b1 = ((llrs[j] > 0 ? 2.0 : -2.0)
			+ coeff * n) / N0;
	b = b1 * (1 << ldpc_llr_decimals);
	b = round(b);
	if (b > llr_max)
		b = llr_max;
	if (b < -llr_max)
		b = -llr_max;
	llrs[j] = (int8_t) b;
}

/* Generate LLR for a given SNR */
static void
generate_llr_input(uint16_t n, struct rte_bbdev_op_data *inputs,
		struct rte_bbdev_dec_op *ref_op)
{
	struct rte_mbuf *m;
	uint16_t qm;
	uint32_t i, j, e, range;
	double N0, llr_max;

	e = ref_op->ldpc_dec.cb_params.e;
	qm = ref_op->ldpc_dec.q_m;
	llr_max = (1 << (ldpc_llr_size - 1)) - 1;
	range = e / qm;
	N0 = 1.0 / pow(10.0, get_snr() / 10.0);

	for (i = 0; i < n; ++i) {
		m = inputs[i].data;
		int8_t *llrs = rte_pktmbuf_mtod_offset(m, int8_t *, 0);
		if (qm == 8) {
			for (j = 0; j < range; ++j)
				gen_qm8_llr(llrs, j, N0, llr_max);
		} else if (qm == 6) {
			for (j = 0; j < range; ++j)
				gen_qm6_llr(llrs, j, N0, llr_max);
		} else if (qm == 4) {
			for (j = 0; j < range; ++j)
				gen_qm4_llr(llrs, j, N0, llr_max);
		} else {
			for (j = 0; j < e; ++j)
				gen_qm2_llr(llrs, j, N0, llr_max);
		}
	}
}

static void
copy_reference_ldpc_dec_op(struct rte_bbdev_dec_op **ops, unsigned int n,
		unsigned int start_idx,
		struct rte_bbdev_op_data *inputs,
		struct rte_bbdev_op_data *hard_outputs,
		struct rte_bbdev_op_data *soft_outputs,
		struct rte_bbdev_op_data *harq_inputs,
		struct rte_bbdev_op_data *harq_outputs,
		struct rte_bbdev_dec_op *ref_op)
{
	unsigned int i;
	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &ref_op->ldpc_dec;

	for (i = 0; i < n; ++i) {
		if (ldpc_dec->code_block_mode == 0) {
			ops[i]->ldpc_dec.tb_params.ea =
					ldpc_dec->tb_params.ea;
			ops[i]->ldpc_dec.tb_params.eb =
					ldpc_dec->tb_params.eb;
			ops[i]->ldpc_dec.tb_params.c =
					ldpc_dec->tb_params.c;
			ops[i]->ldpc_dec.tb_params.cab =
					ldpc_dec->tb_params.cab;
			ops[i]->ldpc_dec.tb_params.r =
					ldpc_dec->tb_params.r;
		} else {
			ops[i]->ldpc_dec.cb_params.e = ldpc_dec->cb_params.e;
		}

		ops[i]->ldpc_dec.basegraph = ldpc_dec->basegraph;
		ops[i]->ldpc_dec.z_c = ldpc_dec->z_c;
		ops[i]->ldpc_dec.q_m = ldpc_dec->q_m;
		ops[i]->ldpc_dec.n_filler = ldpc_dec->n_filler;
		ops[i]->ldpc_dec.n_cb = ldpc_dec->n_cb;
		ops[i]->ldpc_dec.iter_max = ldpc_dec->iter_max;
		ops[i]->ldpc_dec.rv_index = ldpc_dec->rv_index;
		ops[i]->ldpc_dec.op_flags = ldpc_dec->op_flags;
		ops[i]->ldpc_dec.code_block_mode = ldpc_dec->code_block_mode;

		if (hard_outputs != NULL)
			ops[i]->ldpc_dec.hard_output =
					hard_outputs[start_idx + i];
		if (inputs != NULL)
			ops[i]->ldpc_dec.input =
					inputs[start_idx + i];
		if (soft_outputs != NULL)
			ops[i]->ldpc_dec.soft_output =
					soft_outputs[start_idx + i];
		if (harq_inputs != NULL)
			ops[i]->ldpc_dec.harq_combined_input =
					harq_inputs[start_idx + i];
		if (harq_outputs != NULL)
			ops[i]->ldpc_dec.harq_combined_output =
					harq_outputs[start_idx + i];
	}
}


static void
copy_reference_ldpc_enc_op(struct rte_bbdev_enc_op **ops, unsigned int n,
		unsigned int start_idx,
		struct rte_bbdev_op_data *inputs,
		struct rte_bbdev_op_data *outputs,
		struct rte_bbdev_enc_op *ref_op)
{
	unsigned int i;
	struct rte_bbdev_op_ldpc_enc *ldpc_enc = &ref_op->ldpc_enc;
	for (i = 0; i < n; ++i) {
		if (ldpc_enc->code_block_mode == 0) {
			ops[i]->ldpc_enc.tb_params.ea = ldpc_enc->tb_params.ea;
			ops[i]->ldpc_enc.tb_params.eb = ldpc_enc->tb_params.eb;
			ops[i]->ldpc_enc.tb_params.cab =
					ldpc_enc->tb_params.cab;
			ops[i]->ldpc_enc.tb_params.c = ldpc_enc->tb_params.c;
			ops[i]->ldpc_enc.tb_params.r = ldpc_enc->tb_params.r;
		} else {
			ops[i]->ldpc_enc.cb_params.e = ldpc_enc->cb_params.e;
		}
		ops[i]->ldpc_enc.basegraph = ldpc_enc->basegraph;
		ops[i]->ldpc_enc.z_c = ldpc_enc->z_c;
		ops[i]->ldpc_enc.q_m = ldpc_enc->q_m;
		ops[i]->ldpc_enc.n_filler = ldpc_enc->n_filler;
		ops[i]->ldpc_enc.n_cb = ldpc_enc->n_cb;
		ops[i]->ldpc_enc.rv_index = ldpc_enc->rv_index;
		ops[i]->ldpc_enc.op_flags = ldpc_enc->op_flags;
		ops[i]->ldpc_enc.code_block_mode = ldpc_enc->code_block_mode;
		ops[i]->ldpc_enc.output = outputs[start_idx + i];
		ops[i]->ldpc_enc.input = inputs[start_idx + i];
	}
}

static int
check_dec_status_and_ordering(struct rte_bbdev_dec_op *op,
		unsigned int order_idx, const int expected_status)
{
	int status = op->status;
	/* ignore parity mismatch false alarms for long iterations */
	if (get_iter_max() >= 10) {
		if (!(expected_status & (1 << RTE_BBDEV_SYNDROME_ERROR)) &&
				(status & (1 << RTE_BBDEV_SYNDROME_ERROR))) {
			printf("WARNING: Ignore Syndrome Check mismatch\n");
			status -= (1 << RTE_BBDEV_SYNDROME_ERROR);
		}
		if ((expected_status & (1 << RTE_BBDEV_SYNDROME_ERROR)) &&
				!(status & (1 << RTE_BBDEV_SYNDROME_ERROR))) {
			printf("WARNING: Ignore Syndrome Check mismatch\n");
			status += (1 << RTE_BBDEV_SYNDROME_ERROR);
		}
	}

	TEST_ASSERT(status == expected_status,
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

	if (op->opaque_data != (void *)(uintptr_t)INVALID_OPAQUE)
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
	uint32_t total_data_size = 0;

	TEST_ASSERT(nb_dst_segments == m->nb_segs,
			"Number of segments differ in original (%u) and filled (%u) op",
			nb_dst_segments, m->nb_segs);

	/* Validate each mbuf segment length */
	for (i = 0; i < nb_dst_segments; ++i) {
		/* Apply offset to the first mbuf segment */
		uint16_t offset = (i == 0) ? op->offset : 0;
		uint16_t data_len = rte_pktmbuf_data_len(m) - offset;
		total_data_size += orig_op->segments[i].length;

		TEST_ASSERT(orig_op->segments[i].length == data_len,
				"Length of segment differ in original (%u) and filled (%u) op",
				orig_op->segments[i].length, data_len);
		TEST_ASSERT_BUFFERS_ARE_EQUAL(orig_op->segments[i].addr,
				rte_pktmbuf_mtod_offset(m, uint32_t *, offset),
				data_len,
				"Output buffers (CB=%u) are not equal", i);
		m = m->next;
	}

	/* Validate total mbuf pkt length */
	uint32_t pkt_len = rte_pktmbuf_pkt_len(op->data) - op->offset;
	TEST_ASSERT(total_data_size == pkt_len,
			"Length of data differ in original (%u) and filled (%u) op",
			total_data_size, pkt_len);

	return TEST_SUCCESS;
}

/*
 * Compute K0 for a given configuration for HARQ output length computation
 * As per definition in 3GPP 38.212 Table 5.4.2.1-2
 */
static inline uint16_t
get_k0(uint16_t n_cb, uint16_t z_c, uint8_t bg, uint8_t rv_index)
{
	if (rv_index == 0)
		return 0;
	uint16_t n = (bg == 1 ? N_ZC_1 : N_ZC_2) * z_c;
	if (n_cb == n) {
		if (rv_index == 1)
			return (bg == 1 ? K0_1_1 : K0_1_2) * z_c;
		else if (rv_index == 2)
			return (bg == 1 ? K0_2_1 : K0_2_2) * z_c;
		else
			return (bg == 1 ? K0_3_1 : K0_3_2) * z_c;
	}
	/* LBRM case - includes a division by N */
	if (rv_index == 1)
		return (((bg == 1 ? K0_1_1 : K0_1_2) * n_cb)
				/ n) * z_c;
	else if (rv_index == 2)
		return (((bg == 1 ? K0_2_1 : K0_2_2) * n_cb)
				/ n) * z_c;
	else
		return (((bg == 1 ? K0_3_1 : K0_3_2) * n_cb)
				/ n) * z_c;
}

/* HARQ output length including the Filler bits */
static inline uint16_t
compute_harq_len(struct rte_bbdev_op_ldpc_dec *ops_ld)
{
	uint16_t k0 = 0;
	uint8_t max_rv = (ops_ld->rv_index == 1) ? 3 : ops_ld->rv_index;
	k0 = get_k0(ops_ld->n_cb, ops_ld->z_c, ops_ld->basegraph, max_rv);
	/* Compute RM out size and number of rows */
	uint16_t parity_offset = (ops_ld->basegraph == 1 ? 20 : 8)
			* ops_ld->z_c - ops_ld->n_filler;
	uint16_t deRmOutSize = RTE_MIN(
			k0 + ops_ld->cb_params.e +
			((k0 > parity_offset) ?
					0 : ops_ld->n_filler),
					ops_ld->n_cb);
	uint16_t numRows = ((deRmOutSize + ops_ld->z_c - 1)
			/ ops_ld->z_c);
	uint16_t harq_output_len = numRows * ops_ld->z_c;
	return harq_output_len;
}

static inline int
validate_op_harq_chain(struct rte_bbdev_op_data *op,
		struct op_data_entries *orig_op,
		struct rte_bbdev_op_ldpc_dec *ops_ld)
{
	uint8_t i;
	uint32_t j, jj, k;
	struct rte_mbuf *m = op->data;
	uint8_t nb_dst_segments = orig_op->nb_segments;
	uint32_t total_data_size = 0;
	int8_t *harq_orig, *harq_out, abs_harq_origin;
	uint32_t byte_error = 0, cum_error = 0, error;
	int16_t llr_max = (1 << (ldpc_llr_size - ldpc_llr_decimals)) - 1;
	int16_t llr_max_pre_scaling = (1 << (ldpc_llr_size - 1)) - 1;
	uint16_t parity_offset;

	TEST_ASSERT(nb_dst_segments == m->nb_segs,
			"Number of segments differ in original (%u) and filled (%u) op",
			nb_dst_segments, m->nb_segs);

	/* Validate each mbuf segment length */
	for (i = 0; i < nb_dst_segments; ++i) {
		/* Apply offset to the first mbuf segment */
		uint16_t offset = (i == 0) ? op->offset : 0;
		uint16_t data_len = rte_pktmbuf_data_len(m) - offset;
		total_data_size += orig_op->segments[i].length;

		TEST_ASSERT(orig_op->segments[i].length <
				(uint32_t)(data_len + 64),
				"Length of segment differ in original (%u) and filled (%u) op",
				orig_op->segments[i].length, data_len);
		harq_orig = (int8_t *) orig_op->segments[i].addr;
		harq_out = rte_pktmbuf_mtod_offset(m, int8_t *, offset);

		if (!(ldpc_cap_flags &
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS
				) || (ops_ld->op_flags &
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
			data_len -= ops_ld->z_c;
			parity_offset = data_len;
		} else {
			/* Compute RM out size and number of rows */
			parity_offset = (ops_ld->basegraph == 1 ? 20 : 8)
					* ops_ld->z_c - ops_ld->n_filler;
			uint16_t deRmOutSize = compute_harq_len(ops_ld) -
					ops_ld->n_filler;
			if (data_len > deRmOutSize)
				data_len = deRmOutSize;
			if (data_len > orig_op->segments[i].length)
				data_len = orig_op->segments[i].length;
		}
		/*
		 * HARQ output can have minor differences
		 * due to integer representation and related scaling
		 */
		for (j = 0, jj = 0; j < data_len; j++, jj++) {
			if (j == parity_offset) {
				/* Special Handling of the filler bits */
				for (k = 0; k < ops_ld->n_filler; k++) {
					if (harq_out[jj] !=
							llr_max_pre_scaling) {
						printf("HARQ Filler issue %d: %d %d\n",
							jj, harq_out[jj],
							llr_max);
						byte_error++;
					}
					jj++;
				}
			}
			if (!(ops_ld->op_flags &
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
				if (ldpc_llr_decimals > 1)
					harq_out[jj] = (harq_out[jj] + 1)
						>> (ldpc_llr_decimals - 1);
				/* Saturated to S7 */
				if (harq_orig[j] > llr_max)
					harq_orig[j] = llr_max;
				if (harq_orig[j] < -llr_max)
					harq_orig[j] = -llr_max;
			}
			if (harq_orig[j] != harq_out[jj]) {
				error = (harq_orig[j] > harq_out[jj]) ?
						harq_orig[j] - harq_out[jj] :
						harq_out[jj] - harq_orig[j];
				abs_harq_origin = harq_orig[j] > 0 ?
							harq_orig[j] :
							-harq_orig[j];
				/* Residual quantization error */
				if ((error > 8 && (abs_harq_origin <
						(llr_max - 16))) ||
						(error > 16)) {
					printf("HARQ mismatch %d: exp %d act %d => %d\n",
							j, harq_orig[j],
							harq_out[jj], error);
					byte_error++;
					cum_error += error;
				}
			}
		}
		m = m->next;
	}

	if (byte_error)
		TEST_ASSERT(byte_error <= 1,
				"HARQ output mismatch (%d) %d",
				byte_error, cum_error);

	/* Validate total mbuf pkt length */
	uint32_t pkt_len = rte_pktmbuf_pkt_len(op->data) - op->offset;
	TEST_ASSERT(total_data_size < pkt_len + 64,
			"Length of data differ in original (%u) and filled (%u) op",
			total_data_size, pkt_len);

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

/* Check Number of code blocks errors */
static int
validate_ldpc_bler(struct rte_bbdev_dec_op **ops, const uint16_t n)
{
	unsigned int i;
	struct op_data_entries *hard_data_orig =
			&test_vector.entries[DATA_HARD_OUTPUT];
	struct rte_bbdev_op_ldpc_dec *ops_td;
	struct rte_bbdev_op_data *hard_output;
	int errors = 0;
	struct rte_mbuf *m;

	for (i = 0; i < n; ++i) {
		ops_td = &ops[i]->ldpc_dec;
		hard_output = &ops_td->hard_output;
		m = hard_output->data;
		if (memcmp(rte_pktmbuf_mtod_offset(m, uint32_t *, 0),
				hard_data_orig->segments[0].addr,
				hard_data_orig->segments[0].length))
			errors++;
	}
	return errors;
}

static int
validate_ldpc_dec_op(struct rte_bbdev_dec_op **ops, const uint16_t n,
		struct rte_bbdev_dec_op *ref_op, const int vector_mask)
{
	unsigned int i;
	int ret;
	struct op_data_entries *hard_data_orig =
			&test_vector.entries[DATA_HARD_OUTPUT];
	struct op_data_entries *soft_data_orig =
			&test_vector.entries[DATA_SOFT_OUTPUT];
	struct op_data_entries *harq_data_orig =
				&test_vector.entries[DATA_HARQ_OUTPUT];
	struct rte_bbdev_op_ldpc_dec *ops_td;
	struct rte_bbdev_op_data *hard_output;
	struct rte_bbdev_op_data *harq_output;
	struct rte_bbdev_op_data *soft_output;
	struct rte_bbdev_op_ldpc_dec *ref_td = &ref_op->ldpc_dec;

	for (i = 0; i < n; ++i) {
		ops_td = &ops[i]->ldpc_dec;
		hard_output = &ops_td->hard_output;
		harq_output = &ops_td->harq_combined_output;
		soft_output = &ops_td->soft_output;

		ret = check_dec_status_and_ordering(ops[i], i, ref_op->status);
		TEST_ASSERT_SUCCESS(ret,
				"Checking status and ordering for decoder failed");
		if (vector_mask & TEST_BBDEV_VF_EXPECTED_ITER_COUNT)
			TEST_ASSERT(ops_td->iter_count <= ref_td->iter_count,
					"Returned iter_count (%d) > expected iter_count (%d)",
					ops_td->iter_count, ref_td->iter_count);
		/*
		 * We can ignore output data when the decoding failed to
		 * converge or for loop-back cases
		 */
		if (!check_bit(ops[i]->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK
				) && (
				ops[i]->status & (1 << RTE_BBDEV_SYNDROME_ERROR
						)) == 0)
			TEST_ASSERT_SUCCESS(validate_op_chain(hard_output,
					hard_data_orig),
					"Hard output buffers (CB=%u) are not equal",
					i);

		if (ref_op->ldpc_dec.op_flags & RTE_BBDEV_LDPC_SOFT_OUT_ENABLE)
			TEST_ASSERT_SUCCESS(validate_op_chain(soft_output,
					soft_data_orig),
					"Soft output buffers (CB=%u) are not equal",
					i);
		if (ref_op->ldpc_dec.op_flags &
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE) {
			TEST_ASSERT_SUCCESS(validate_op_harq_chain(harq_output,
					harq_data_orig, ops_td),
					"HARQ output buffers (CB=%u) are not equal",
					i);
		}
		if (ref_op->ldpc_dec.op_flags &
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)
			TEST_ASSERT_SUCCESS(validate_op_harq_chain(harq_output,
					harq_data_orig, ops_td),
					"HARQ output buffers (CB=%u) are not equal",
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

static int
validate_ldpc_enc_op(struct rte_bbdev_enc_op **ops, const uint16_t n,
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
				&ops[i]->ldpc_enc.output,
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
create_reference_ldpc_dec_op(struct rte_bbdev_dec_op *op)
{
	unsigned int i;
	struct op_data_entries *entry;

	op->ldpc_dec = test_vector.ldpc_dec;
	entry = &test_vector.entries[DATA_INPUT];
	for (i = 0; i < entry->nb_segments; ++i)
		op->ldpc_dec.input.length +=
				entry->segments[i].length;
	if (test_vector.ldpc_dec.op_flags &
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE) {
		entry = &test_vector.entries[DATA_HARQ_INPUT];
		for (i = 0; i < entry->nb_segments; ++i)
			op->ldpc_dec.harq_combined_input.length +=
				entry->segments[i].length;
	}
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

static void
create_reference_ldpc_enc_op(struct rte_bbdev_enc_op *op)
{
	unsigned int i;
	struct op_data_entries *entry;

	op->ldpc_enc = test_vector.ldpc_enc;
	entry = &test_vector.entries[DATA_INPUT];
	for (i = 0; i < entry->nb_segments; ++i)
		op->ldpc_enc.input.length +=
				entry->segments[i].length;
}

static uint32_t
calc_dec_TB_size(struct rte_bbdev_dec_op *op)
{
	uint8_t i;
	uint32_t c, r, tb_size = 0;

	if (op->turbo_dec.code_block_mode) {
		tb_size = op->turbo_dec.tb_params.k_neg;
	} else {
		c = op->turbo_dec.tb_params.c;
		r = op->turbo_dec.tb_params.r;
		for (i = 0; i < c-r; i++)
			tb_size += (r < op->turbo_dec.tb_params.c_neg) ?
				op->turbo_dec.tb_params.k_neg :
				op->turbo_dec.tb_params.k_pos;
	}
	return tb_size;
}

static uint32_t
calc_ldpc_dec_TB_size(struct rte_bbdev_dec_op *op)
{
	uint8_t i;
	uint32_t c, r, tb_size = 0;
	uint16_t sys_cols = (op->ldpc_dec.basegraph == 1) ? 22 : 10;

	if (op->ldpc_dec.code_block_mode) {
		tb_size = sys_cols * op->ldpc_dec.z_c - op->ldpc_dec.n_filler;
	} else {
		c = op->ldpc_dec.tb_params.c;
		r = op->ldpc_dec.tb_params.r;
		for (i = 0; i < c-r; i++)
			tb_size += sys_cols * op->ldpc_dec.z_c
					- op->ldpc_dec.n_filler;
	}
	return tb_size;
}

static uint32_t
calc_enc_TB_size(struct rte_bbdev_enc_op *op)
{
	uint8_t i;
	uint32_t c, r, tb_size = 0;

	if (op->turbo_enc.code_block_mode) {
		tb_size = op->turbo_enc.tb_params.k_neg;
	} else {
		c = op->turbo_enc.tb_params.c;
		r = op->turbo_enc.tb_params.r;
		for (i = 0; i < c-r; i++)
			tb_size += (r < op->turbo_enc.tb_params.c_neg) ?
				op->turbo_enc.tb_params.k_neg :
				op->turbo_enc.tb_params.k_pos;
	}
	return tb_size;
}

static uint32_t
calc_ldpc_enc_TB_size(struct rte_bbdev_enc_op *op)
{
	uint8_t i;
	uint32_t c, r, tb_size = 0;
	uint16_t sys_cols = (op->ldpc_enc.basegraph == 1) ? 22 : 10;

	if (op->turbo_enc.code_block_mode) {
		tb_size = sys_cols * op->ldpc_enc.z_c - op->ldpc_enc.n_filler;
	} else {
		c = op->turbo_enc.tb_params.c;
		r = op->turbo_enc.tb_params.r;
		for (i = 0; i < c-r; i++)
			tb_size += sys_cols * op->ldpc_enc.z_c
					- op->ldpc_enc.n_filler;
	}
	return tb_size;
}


static int
init_test_op_params(struct test_op_params *op_params,
		enum rte_bbdev_op_type op_type, const int expected_status,
		const int vector_mask, struct rte_mempool *ops_mp,
		uint16_t burst_sz, uint16_t num_to_process, uint16_t num_lcores)
{
	int ret = 0;
	if (op_type == RTE_BBDEV_OP_TURBO_DEC ||
			op_type == RTE_BBDEV_OP_LDPC_DEC)
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
	if (op_type == RTE_BBDEV_OP_TURBO_DEC ||
			op_type == RTE_BBDEV_OP_LDPC_DEC)
		op_params->ref_dec_op->status = expected_status;
	else if (op_type == RTE_BBDEV_OP_TURBO_ENC
			|| op_type == RTE_BBDEV_OP_LDPC_ENC)
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

	f_ret = create_mempools(ad, socket_id, op_type,
			get_num_ops());
	if (f_ret != TEST_SUCCESS) {
		printf("Couldn't create mempools");
		goto fail;
	}
	if (op_type == RTE_BBDEV_OP_NONE)
		op_type = RTE_BBDEV_OP_TURBO_ENC;

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


	/* Find capabilities */
	const struct rte_bbdev_op_cap *cap = info.drv.capabilities;
	for (i = 0; i < RTE_BBDEV_OP_TYPE_COUNT; i++) {
		if (cap->type == test_vector.op_type) {
			capabilities = cap;
			break;
		}
		cap++;
	}
	TEST_ASSERT_NOT_NULL(capabilities,
			"Couldn't find capabilities");

	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC) {
		create_reference_dec_op(op_params->ref_dec_op);
	} else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC)
		create_reference_enc_op(op_params->ref_enc_op);
	else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_ENC)
		create_reference_ldpc_enc_op(op_params->ref_enc_op);
	else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC)
		create_reference_ldpc_dec_op(op_params->ref_dec_op);

	for (i = 0; i < ad->nb_queues; ++i) {
		f_ret = fill_queue_buffers(op_params,
				ad->in_mbuf_pool,
				ad->hard_out_mbuf_pool,
				ad->soft_out_mbuf_pool,
				ad->harq_in_mbuf_pool,
				ad->harq_out_mbuf_pool,
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


/* Push back the HARQ output from DDR to host */
static void
retrieve_harq_ddr(uint16_t dev_id, uint16_t queue_id,
		struct rte_bbdev_dec_op **ops,
		const uint16_t n)
{
	uint16_t j;
	int save_status, ret;
	uint32_t harq_offset = (uint32_t) queue_id * HARQ_INCR * MAX_OPS;
	struct rte_bbdev_dec_op *ops_deq[MAX_BURST];
	uint32_t flags = ops[0]->ldpc_dec.op_flags;
	bool loopback = flags & RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK;
	bool mem_out = flags & RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;
	bool hc_out = flags & RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE;
	bool h_comp = flags & RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;
	for (j = 0; j < n; ++j) {
		if ((loopback && mem_out) || hc_out) {
			save_status = ops[j]->status;
			ops[j]->ldpc_dec.op_flags =
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK +
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE;
			if (h_comp)
				ops[j]->ldpc_dec.op_flags +=
					RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;
			ops[j]->ldpc_dec.harq_combined_input.offset =
					harq_offset;
			ops[j]->ldpc_dec.harq_combined_output.offset = 0;
			harq_offset += HARQ_INCR;
			if (!loopback)
				ops[j]->ldpc_dec.harq_combined_input.length =
				ops[j]->ldpc_dec.harq_combined_output.length;
			rte_bbdev_enqueue_ldpc_dec_ops(dev_id, queue_id,
					&ops[j], 1);
			ret = 0;
			while (ret == 0)
				ret = rte_bbdev_dequeue_ldpc_dec_ops(
						dev_id, queue_id,
						&ops_deq[j], 1);
			ops[j]->ldpc_dec.op_flags = flags;
			ops[j]->status = save_status;
		}
	}
}

/*
 * Push back the HARQ output from HW DDR to Host
 * Preload HARQ memory input and adjust HARQ offset
 */
static void
preload_harq_ddr(uint16_t dev_id, uint16_t queue_id,
		struct rte_bbdev_dec_op **ops, const uint16_t n,
		bool preload)
{
	uint16_t j;
	int deq;
	uint32_t harq_offset = (uint32_t) queue_id * HARQ_INCR * MAX_OPS;
	struct rte_bbdev_op_data save_hc_in[MAX_OPS], save_hc_out[MAX_OPS];
	struct rte_bbdev_dec_op *ops_deq[MAX_OPS];
	uint32_t flags = ops[0]->ldpc_dec.op_flags;
	bool mem_in = flags & RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE;
	bool hc_in = flags & RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
	bool mem_out = flags & RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;
	bool hc_out = flags & RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE;
	bool h_comp = flags & RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;
	if ((mem_in || hc_in) && preload) {
		for (j = 0; j < n; ++j) {
			save_hc_in[j] = ops[j]->ldpc_dec.harq_combined_input;
			save_hc_out[j] = ops[j]->ldpc_dec.harq_combined_output;
			ops[j]->ldpc_dec.op_flags =
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK +
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;
			if (h_comp)
				ops[j]->ldpc_dec.op_flags +=
					RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;
			ops[j]->ldpc_dec.harq_combined_output.offset =
					harq_offset;
			ops[j]->ldpc_dec.harq_combined_input.offset = 0;
			harq_offset += HARQ_INCR;
		}
		rte_bbdev_enqueue_ldpc_dec_ops(dev_id, queue_id, &ops[0], n);
		deq = 0;
		while (deq != n)
			deq += rte_bbdev_dequeue_ldpc_dec_ops(
					dev_id, queue_id, &ops_deq[deq],
					n - deq);
		/* Restore the operations */
		for (j = 0; j < n; ++j) {
			ops[j]->ldpc_dec.op_flags = flags;
			ops[j]->ldpc_dec.harq_combined_input = save_hc_in[j];
			ops[j]->ldpc_dec.harq_combined_output = save_hc_out[j];
		}
	}
	harq_offset = (uint32_t) queue_id * HARQ_INCR * MAX_OPS;
	for (j = 0; j < n; ++j) {
		/* Adjust HARQ offset when we reach external DDR */
		if (mem_in || hc_in)
			ops[j]->ldpc_dec.harq_combined_input.offset
				= harq_offset;
		if (mem_out || hc_out)
			ops[j]->ldpc_dec.harq_combined_output.offset
				= harq_offset;
		harq_offset += HARQ_INCR;
	}
}

static void
dequeue_event_callback(uint16_t dev_id,
		enum rte_bbdev_event_type event, void *cb_arg,
		void *ret_param)
{
	int ret;
	uint16_t i;
	uint64_t total_time;
	uint16_t deq, burst_sz, num_ops;
	uint16_t queue_id = *(uint16_t *) ret_param;
	struct rte_bbdev_info info;
	double tb_len_bits;
	struct thread_params *tp = cb_arg;

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

	burst_sz = rte_atomic16_read(&tp->burst_sz);
	num_ops = tp->op_params->num_to_process;

	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
		deq = rte_bbdev_dequeue_dec_ops(dev_id, queue_id,
				&tp->dec_ops[
					rte_atomic16_read(&tp->nb_dequeued)],
				burst_sz);
	else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC)
		deq = rte_bbdev_dequeue_ldpc_dec_ops(dev_id, queue_id,
				&tp->dec_ops[
					rte_atomic16_read(&tp->nb_dequeued)],
				burst_sz);
	else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_ENC)
		deq = rte_bbdev_dequeue_ldpc_enc_ops(dev_id, queue_id,
				&tp->enc_ops[
					rte_atomic16_read(&tp->nb_dequeued)],
				burst_sz);
	else /*RTE_BBDEV_OP_TURBO_ENC*/
		deq = rte_bbdev_dequeue_enc_ops(dev_id, queue_id,
				&tp->enc_ops[
					rte_atomic16_read(&tp->nb_dequeued)],
				burst_sz);

	if (deq < burst_sz) {
		printf(
			"After receiving the interrupt all operations should be dequeued. Expected: %u, got: %u\n",
			burst_sz, deq);
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
		return;
	}

	if (rte_atomic16_read(&tp->nb_dequeued) + deq < num_ops) {
		rte_atomic16_add(&tp->nb_dequeued, deq);
		return;
	}

	total_time = rte_rdtsc_precise() - tp->start_time;

	rte_bbdev_info_get(dev_id, &info);

	ret = TEST_SUCCESS;

	if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC) {
		struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
		ret = validate_dec_op(tp->dec_ops, num_ops, ref_op,
				tp->op_params->vector_mask);
		/* get the max of iter_count for all dequeued ops */
		for (i = 0; i < num_ops; ++i)
			tp->iter_count = RTE_MAX(
					tp->dec_ops[i]->turbo_dec.iter_count,
					tp->iter_count);
		rte_bbdev_dec_op_free_bulk(tp->dec_ops, deq);
	} else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC) {
		struct rte_bbdev_enc_op *ref_op = tp->op_params->ref_enc_op;
		ret = validate_enc_op(tp->enc_ops, num_ops, ref_op);
		rte_bbdev_enc_op_free_bulk(tp->enc_ops, deq);
	} else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_ENC) {
		struct rte_bbdev_enc_op *ref_op = tp->op_params->ref_enc_op;
		ret = validate_ldpc_enc_op(tp->enc_ops, num_ops, ref_op);
		rte_bbdev_enc_op_free_bulk(tp->enc_ops, deq);
	} else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC) {
		struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
		ret = validate_ldpc_dec_op(tp->dec_ops, num_ops, ref_op,
				tp->op_params->vector_mask);
		rte_bbdev_dec_op_free_bulk(tp->dec_ops, deq);
	}

	if (ret) {
		printf("Buffers validation failed\n");
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
	}

	switch (test_vector.op_type) {
	case RTE_BBDEV_OP_TURBO_DEC:
		tb_len_bits = calc_dec_TB_size(tp->op_params->ref_dec_op);
		break;
	case RTE_BBDEV_OP_TURBO_ENC:
		tb_len_bits = calc_enc_TB_size(tp->op_params->ref_enc_op);
		break;
	case RTE_BBDEV_OP_LDPC_DEC:
		tb_len_bits = calc_ldpc_dec_TB_size(tp->op_params->ref_dec_op);
		break;
	case RTE_BBDEV_OP_LDPC_ENC:
		tb_len_bits = calc_ldpc_enc_TB_size(tp->op_params->ref_enc_op);
		break;
	case RTE_BBDEV_OP_NONE:
		tb_len_bits = 0.0;
		break;
	default:
		printf("Unknown op type: %d\n", test_vector.op_type);
		rte_atomic16_set(&tp->processing_status, TEST_FAILED);
		return;
	}

	tp->ops_per_sec += ((double)num_ops) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps += (((double)(num_ops * tb_len_bits)) / 1000000.0) /
			((double)total_time / (double)rte_get_tsc_hz());

	rte_atomic16_add(&tp->nb_dequeued, deq);
}

static int
throughput_intr_lcore_ldpc_dec(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct rte_bbdev_dec_op *ops[num_to_process];
	struct test_buffers *bufs = NULL;
	struct rte_bbdev_info info;
	int ret, i, j;
	struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
	uint16_t num_to_enq, enq;

	bool loopback = check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK);
	bool hc_out = check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE);

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_intr_enable(tp->dev_id, queue_id),
			"Failed to enable interrupts for dev: %u, queue_id: %u",
			tp->dev_id, queue_id);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_to_process > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	rte_atomic16_clear(&tp->processing_status);
	rte_atomic16_clear(&tp->nb_dequeued);

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp, ops,
				num_to_process);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops",
			num_to_process);
	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_ldpc_dec_op(ops, num_to_process, 0, bufs->inputs,
				bufs->hard_outputs, bufs->soft_outputs,
				bufs->harq_inputs, bufs->harq_outputs, ref_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_to_process; ++j)
		ops[j]->opaque_data = (void *)(uintptr_t)j;

	for (j = 0; j < TEST_REPETITIONS; ++j) {
		for (i = 0; i < num_to_process; ++i) {
			if (!loopback)
				rte_pktmbuf_reset(
					ops[i]->ldpc_dec.hard_output.data);
			if (hc_out || loopback)
				mbuf_reset(
				ops[i]->ldpc_dec.harq_combined_output.data);
		}

		tp->start_time = rte_rdtsc_precise();
		for (enqueued = 0; enqueued < num_to_process;) {
			num_to_enq = burst_sz;

			if (unlikely(num_to_process - enqueued < num_to_enq))
				num_to_enq = num_to_process - enqueued;

			enq = 0;
			do {
				enq += rte_bbdev_enqueue_ldpc_dec_ops(
						tp->dev_id,
						queue_id, &ops[enqueued],
						num_to_enq);
			} while (unlikely(num_to_enq != enq));
			enqueued += enq;

			/* Write to thread burst_sz current number of enqueued
			 * descriptors. It ensures that proper number of
			 * descriptors will be dequeued in callback
			 * function - needed for last batch in case where
			 * the number of operations is not a multiple of
			 * burst size.
			 */
			rte_atomic16_set(&tp->burst_sz, num_to_enq);

			/* Wait until processing of previous batch is
			 * completed
			 */
			while (rte_atomic16_read(&tp->nb_dequeued) !=
					(int16_t) enqueued)
				rte_pause();
		}
		if (j != TEST_REPETITIONS - 1)
			rte_atomic16_clear(&tp->nb_dequeued);
	}

	return TEST_SUCCESS;
}

static int
throughput_intr_lcore_dec(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct rte_bbdev_dec_op *ops[num_to_process];
	struct test_buffers *bufs = NULL;
	struct rte_bbdev_info info;
	int ret, i, j;
	uint16_t num_to_enq, enq;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_intr_enable(tp->dev_id, queue_id),
			"Failed to enable interrupts for dev: %u, queue_id: %u",
			tp->dev_id, queue_id);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_to_process > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	rte_atomic16_clear(&tp->processing_status);
	rte_atomic16_clear(&tp->nb_dequeued);

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp, ops,
				num_to_process);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops",
			num_to_process);
	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_dec_op(ops, num_to_process, 0, bufs->inputs,
				bufs->hard_outputs, bufs->soft_outputs,
				tp->op_params->ref_dec_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_to_process; ++j)
		ops[j]->opaque_data = (void *)(uintptr_t)j;

	for (j = 0; j < TEST_REPETITIONS; ++j) {
		for (i = 0; i < num_to_process; ++i)
			rte_pktmbuf_reset(ops[i]->turbo_dec.hard_output.data);

		tp->start_time = rte_rdtsc_precise();
		for (enqueued = 0; enqueued < num_to_process;) {
			num_to_enq = burst_sz;

			if (unlikely(num_to_process - enqueued < num_to_enq))
				num_to_enq = num_to_process - enqueued;

			enq = 0;
			do {
				enq += rte_bbdev_enqueue_dec_ops(tp->dev_id,
						queue_id, &ops[enqueued],
						num_to_enq);
			} while (unlikely(num_to_enq != enq));
			enqueued += enq;

			/* Write to thread burst_sz current number of enqueued
			 * descriptors. It ensures that proper number of
			 * descriptors will be dequeued in callback
			 * function - needed for last batch in case where
			 * the number of operations is not a multiple of
			 * burst size.
			 */
			rte_atomic16_set(&tp->burst_sz, num_to_enq);

			/* Wait until processing of previous batch is
			 * completed
			 */
			while (rte_atomic16_read(&tp->nb_dequeued) !=
					(int16_t) enqueued)
				rte_pause();
		}
		if (j != TEST_REPETITIONS - 1)
			rte_atomic16_clear(&tp->nb_dequeued);
	}

	return TEST_SUCCESS;
}

static int
throughput_intr_lcore_enc(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct rte_bbdev_enc_op *ops[num_to_process];
	struct test_buffers *bufs = NULL;
	struct rte_bbdev_info info;
	int ret, i, j;
	uint16_t num_to_enq, enq;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_intr_enable(tp->dev_id, queue_id),
			"Failed to enable interrupts for dev: %u, queue_id: %u",
			tp->dev_id, queue_id);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_to_process > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	rte_atomic16_clear(&tp->processing_status);
	rte_atomic16_clear(&tp->nb_dequeued);

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp, ops,
			num_to_process);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops",
			num_to_process);
	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_enc_op(ops, num_to_process, 0, bufs->inputs,
				bufs->hard_outputs, tp->op_params->ref_enc_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_to_process; ++j)
		ops[j]->opaque_data = (void *)(uintptr_t)j;

	for (j = 0; j < TEST_REPETITIONS; ++j) {
		for (i = 0; i < num_to_process; ++i)
			rte_pktmbuf_reset(ops[i]->turbo_enc.output.data);

		tp->start_time = rte_rdtsc_precise();
		for (enqueued = 0; enqueued < num_to_process;) {
			num_to_enq = burst_sz;

			if (unlikely(num_to_process - enqueued < num_to_enq))
				num_to_enq = num_to_process - enqueued;

			enq = 0;
			do {
				enq += rte_bbdev_enqueue_enc_ops(tp->dev_id,
						queue_id, &ops[enqueued],
						num_to_enq);
			} while (unlikely(enq != num_to_enq));
			enqueued += enq;

			/* Write to thread burst_sz current number of enqueued
			 * descriptors. It ensures that proper number of
			 * descriptors will be dequeued in callback
			 * function - needed for last batch in case where
			 * the number of operations is not a multiple of
			 * burst size.
			 */
			rte_atomic16_set(&tp->burst_sz, num_to_enq);

			/* Wait until processing of previous batch is
			 * completed
			 */
			while (rte_atomic16_read(&tp->nb_dequeued) !=
					(int16_t) enqueued)
				rte_pause();
		}
		if (j != TEST_REPETITIONS - 1)
			rte_atomic16_clear(&tp->nb_dequeued);
	}

	return TEST_SUCCESS;
}


static int
throughput_intr_lcore_ldpc_enc(void *arg)
{
	struct thread_params *tp = arg;
	unsigned int enqueued;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_to_process = tp->op_params->num_to_process;
	struct rte_bbdev_enc_op *ops[num_to_process];
	struct test_buffers *bufs = NULL;
	struct rte_bbdev_info info;
	int ret, i, j;
	uint16_t num_to_enq, enq;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	TEST_ASSERT_SUCCESS(rte_bbdev_queue_intr_enable(tp->dev_id, queue_id),
			"Failed to enable interrupts for dev: %u, queue_id: %u",
			tp->dev_id, queue_id);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_to_process > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	rte_atomic16_clear(&tp->processing_status);
	rte_atomic16_clear(&tp->nb_dequeued);

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp, ops,
			num_to_process);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops",
			num_to_process);
	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_ldpc_enc_op(ops, num_to_process, 0,
				bufs->inputs, bufs->hard_outputs,
				tp->op_params->ref_enc_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_to_process; ++j)
		ops[j]->opaque_data = (void *)(uintptr_t)j;

	for (j = 0; j < TEST_REPETITIONS; ++j) {
		for (i = 0; i < num_to_process; ++i)
			rte_pktmbuf_reset(ops[i]->turbo_enc.output.data);

		tp->start_time = rte_rdtsc_precise();
		for (enqueued = 0; enqueued < num_to_process;) {
			num_to_enq = burst_sz;

			if (unlikely(num_to_process - enqueued < num_to_enq))
				num_to_enq = num_to_process - enqueued;

			enq = 0;
			do {
				enq += rte_bbdev_enqueue_ldpc_enc_ops(
						tp->dev_id,
						queue_id, &ops[enqueued],
						num_to_enq);
			} while (unlikely(enq != num_to_enq));
			enqueued += enq;

			/* Write to thread burst_sz current number of enqueued
			 * descriptors. It ensures that proper number of
			 * descriptors will be dequeued in callback
			 * function - needed for last batch in case where
			 * the number of operations is not a multiple of
			 * burst size.
			 */
			rte_atomic16_set(&tp->burst_sz, num_to_enq);

			/* Wait until processing of previous batch is
			 * completed
			 */
			while (rte_atomic16_read(&tp->nb_dequeued) !=
					(int16_t) enqueued)
				rte_pause();
		}
		if (j != TEST_REPETITIONS - 1)
			rte_atomic16_clear(&tp->nb_dequeued);
	}

	return TEST_SUCCESS;
}

static int
throughput_pmd_lcore_dec(void *arg)
{
	struct thread_params *tp = arg;
	uint16_t enq, deq;
	uint64_t total_time = 0, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_ops = tp->op_params->num_to_process;
	struct rte_bbdev_dec_op *ops_enq[num_ops];
	struct rte_bbdev_dec_op *ops_deq[num_ops];
	struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
	struct test_buffers *bufs = NULL;
	int i, j, ret;
	struct rte_bbdev_info info;
	uint16_t num_to_enq;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_ops > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp, ops_enq, num_ops);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops", num_ops);

	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_dec_op(ops_enq, num_ops, 0, bufs->inputs,
				bufs->hard_outputs, bufs->soft_outputs, ref_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_ops; ++j)
		ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

	for (i = 0; i < TEST_REPETITIONS; ++i) {

		for (j = 0; j < num_ops; ++j)
			mbuf_reset(ops_enq[j]->turbo_dec.hard_output.data);

		start_time = rte_rdtsc_precise();

		for (enq = 0, deq = 0; enq < num_ops;) {
			num_to_enq = burst_sz;

			if (unlikely(num_ops - enq < num_to_enq))
				num_to_enq = num_ops - enq;

			enq += rte_bbdev_enqueue_dec_ops(tp->dev_id,
					queue_id, &ops_enq[enq], num_to_enq);

			deq += rte_bbdev_dequeue_dec_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		/* dequeue the remaining */
		while (deq < enq) {
			deq += rte_bbdev_dequeue_dec_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		total_time += rte_rdtsc_precise() - start_time;
	}

	tp->iter_count = 0;
	/* get the max of iter_count for all dequeued ops */
	for (i = 0; i < num_ops; ++i) {
		tp->iter_count = RTE_MAX(ops_enq[i]->turbo_dec.iter_count,
				tp->iter_count);
	}

	if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
		ret = validate_dec_op(ops_deq, num_ops, ref_op,
				tp->op_params->vector_mask);
		TEST_ASSERT_SUCCESS(ret, "Validation failed!");
	}

	rte_bbdev_dec_op_free_bulk(ops_enq, num_ops);

	double tb_len_bits = calc_dec_TB_size(ref_op);

	tp->ops_per_sec = ((double)num_ops * TEST_REPETITIONS) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = (((double)(num_ops * TEST_REPETITIONS * tb_len_bits)) /
			1000000.0) / ((double)total_time /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static int
bler_pmd_lcore_ldpc_dec(void *arg)
{
	struct thread_params *tp = arg;
	uint16_t enq, deq;
	uint64_t total_time = 0, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_ops = tp->op_params->num_to_process;
	struct rte_bbdev_dec_op *ops_enq[num_ops];
	struct rte_bbdev_dec_op *ops_deq[num_ops];
	struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
	struct test_buffers *bufs = NULL;
	int i, j, ret;
	float parity_bler = 0;
	struct rte_bbdev_info info;
	uint16_t num_to_enq;
	bool extDdr = check_bit(ldpc_cap_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE);
	bool loopback = check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK);
	bool hc_out = check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE);

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_ops > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp, ops_enq, num_ops);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops", num_ops);

	/* For BLER tests we need to enable early termination */
	if (!check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE))
		ref_op->ldpc_dec.op_flags +=
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
	ref_op->ldpc_dec.iter_max = get_iter_max();
	ref_op->ldpc_dec.iter_count = ref_op->ldpc_dec.iter_max;

	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_ldpc_dec_op(ops_enq, num_ops, 0, bufs->inputs,
				bufs->hard_outputs, bufs->soft_outputs,
				bufs->harq_inputs, bufs->harq_outputs, ref_op);
	generate_llr_input(num_ops, bufs->inputs, ref_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_ops; ++j)
		ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

	for (i = 0; i < 1; ++i) { /* Could add more iterations */
		for (j = 0; j < num_ops; ++j) {
			if (!loopback)
				mbuf_reset(
				ops_enq[j]->ldpc_dec.hard_output.data);
			if (hc_out || loopback)
				mbuf_reset(
				ops_enq[j]->ldpc_dec.harq_combined_output.data);
		}
		if (extDdr)
			preload_harq_ddr(tp->dev_id, queue_id, ops_enq,
					num_ops, true);
		start_time = rte_rdtsc_precise();

		for (enq = 0, deq = 0; enq < num_ops;) {
			num_to_enq = burst_sz;

			if (unlikely(num_ops - enq < num_to_enq))
				num_to_enq = num_ops - enq;

			enq += rte_bbdev_enqueue_ldpc_dec_ops(tp->dev_id,
					queue_id, &ops_enq[enq], num_to_enq);

			deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		/* dequeue the remaining */
		while (deq < enq) {
			deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		total_time += rte_rdtsc_precise() - start_time;
	}

	tp->iter_count = 0;
	tp->iter_average = 0;
	/* get the max of iter_count for all dequeued ops */
	for (i = 0; i < num_ops; ++i) {
		tp->iter_count = RTE_MAX(ops_enq[i]->ldpc_dec.iter_count,
				tp->iter_count);
		tp->iter_average += (double) ops_enq[i]->ldpc_dec.iter_count;
		if (ops_enq[i]->status & (1 << RTE_BBDEV_SYNDROME_ERROR))
			parity_bler += 1.0;
	}

	parity_bler /= num_ops; /* This one is based on SYND */
	tp->iter_average /= num_ops;
	tp->bler = (double) validate_ldpc_bler(ops_deq, num_ops) / num_ops;

	if (test_vector.op_type != RTE_BBDEV_OP_NONE
			&& tp->bler == 0
			&& parity_bler == 0
			&& !hc_out) {
		ret = validate_ldpc_dec_op(ops_deq, num_ops, ref_op,
				tp->op_params->vector_mask);
		TEST_ASSERT_SUCCESS(ret, "Validation failed!");
	}

	rte_bbdev_dec_op_free_bulk(ops_enq, num_ops);

	double tb_len_bits = calc_ldpc_dec_TB_size(ref_op);
	tp->ops_per_sec = ((double)num_ops * 1) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = (((double)(num_ops * 1 * tb_len_bits)) /
			1000000.0) / ((double)total_time /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static int
throughput_pmd_lcore_ldpc_dec(void *arg)
{
	struct thread_params *tp = arg;
	uint16_t enq, deq;
	uint64_t total_time = 0, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_ops = tp->op_params->num_to_process;
	struct rte_bbdev_dec_op *ops_enq[num_ops];
	struct rte_bbdev_dec_op *ops_deq[num_ops];
	struct rte_bbdev_dec_op *ref_op = tp->op_params->ref_dec_op;
	struct test_buffers *bufs = NULL;
	int i, j, ret;
	struct rte_bbdev_info info;
	uint16_t num_to_enq;
	bool extDdr = check_bit(ldpc_cap_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE);
	bool loopback = check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK);
	bool hc_out = check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE);

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_ops > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_dec_op_alloc_bulk(tp->op_params->mp, ops_enq, num_ops);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops", num_ops);

	/* For throughput tests we need to disable early termination */
	if (check_bit(ref_op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE))
		ref_op->ldpc_dec.op_flags -=
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
	ref_op->ldpc_dec.iter_max = get_iter_max();
	ref_op->ldpc_dec.iter_count = ref_op->ldpc_dec.iter_max;

	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_ldpc_dec_op(ops_enq, num_ops, 0, bufs->inputs,
				bufs->hard_outputs, bufs->soft_outputs,
				bufs->harq_inputs, bufs->harq_outputs, ref_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_ops; ++j)
		ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

	for (i = 0; i < TEST_REPETITIONS; ++i) {
		for (j = 0; j < num_ops; ++j) {
			if (!loopback)
				mbuf_reset(
				ops_enq[j]->ldpc_dec.hard_output.data);
			if (hc_out || loopback)
				mbuf_reset(
				ops_enq[j]->ldpc_dec.harq_combined_output.data);
		}
		if (extDdr)
			preload_harq_ddr(tp->dev_id, queue_id, ops_enq,
					num_ops, true);
		start_time = rte_rdtsc_precise();

		for (enq = 0, deq = 0; enq < num_ops;) {
			num_to_enq = burst_sz;

			if (unlikely(num_ops - enq < num_to_enq))
				num_to_enq = num_ops - enq;

			enq += rte_bbdev_enqueue_ldpc_dec_ops(tp->dev_id,
					queue_id, &ops_enq[enq], num_to_enq);

			deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		/* dequeue the remaining */
		while (deq < enq) {
			deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		total_time += rte_rdtsc_precise() - start_time;
	}

	tp->iter_count = 0;
	/* get the max of iter_count for all dequeued ops */
	for (i = 0; i < num_ops; ++i) {
		tp->iter_count = RTE_MAX(ops_enq[i]->ldpc_dec.iter_count,
				tp->iter_count);
	}
	if (extDdr) {
		/* Read loopback is not thread safe */
		retrieve_harq_ddr(tp->dev_id, queue_id, ops_enq, num_ops);
	}

	if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
		ret = validate_ldpc_dec_op(ops_deq, num_ops, ref_op,
				tp->op_params->vector_mask);
		TEST_ASSERT_SUCCESS(ret, "Validation failed!");
	}

	rte_bbdev_dec_op_free_bulk(ops_enq, num_ops);

	double tb_len_bits = calc_ldpc_dec_TB_size(ref_op);

	tp->ops_per_sec = ((double)num_ops * TEST_REPETITIONS) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = (((double)(num_ops * TEST_REPETITIONS * tb_len_bits)) /
			1000000.0) / ((double)total_time /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static int
throughput_pmd_lcore_enc(void *arg)
{
	struct thread_params *tp = arg;
	uint16_t enq, deq;
	uint64_t total_time = 0, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_ops = tp->op_params->num_to_process;
	struct rte_bbdev_enc_op *ops_enq[num_ops];
	struct rte_bbdev_enc_op *ops_deq[num_ops];
	struct rte_bbdev_enc_op *ref_op = tp->op_params->ref_enc_op;
	struct test_buffers *bufs = NULL;
	int i, j, ret;
	struct rte_bbdev_info info;
	uint16_t num_to_enq;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_ops > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp, ops_enq,
			num_ops);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops",
			num_ops);
	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_enc_op(ops_enq, num_ops, 0, bufs->inputs,
				bufs->hard_outputs, ref_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_ops; ++j)
		ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

	for (i = 0; i < TEST_REPETITIONS; ++i) {

		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			for (j = 0; j < num_ops; ++j)
				mbuf_reset(ops_enq[j]->turbo_enc.output.data);

		start_time = rte_rdtsc_precise();

		for (enq = 0, deq = 0; enq < num_ops;) {
			num_to_enq = burst_sz;

			if (unlikely(num_ops - enq < num_to_enq))
				num_to_enq = num_ops - enq;

			enq += rte_bbdev_enqueue_enc_ops(tp->dev_id,
					queue_id, &ops_enq[enq], num_to_enq);

			deq += rte_bbdev_dequeue_enc_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		/* dequeue the remaining */
		while (deq < enq) {
			deq += rte_bbdev_dequeue_enc_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		total_time += rte_rdtsc_precise() - start_time;
	}

	if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
		ret = validate_enc_op(ops_deq, num_ops, ref_op);
		TEST_ASSERT_SUCCESS(ret, "Validation failed!");
	}

	rte_bbdev_enc_op_free_bulk(ops_enq, num_ops);

	double tb_len_bits = calc_enc_TB_size(ref_op);

	tp->ops_per_sec = ((double)num_ops * TEST_REPETITIONS) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = (((double)(num_ops * TEST_REPETITIONS * tb_len_bits))
			/ 1000000.0) / ((double)total_time /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static int
throughput_pmd_lcore_ldpc_enc(void *arg)
{
	struct thread_params *tp = arg;
	uint16_t enq, deq;
	uint64_t total_time = 0, start_time;
	const uint16_t queue_id = tp->queue_id;
	const uint16_t burst_sz = tp->op_params->burst_sz;
	const uint16_t num_ops = tp->op_params->num_to_process;
	struct rte_bbdev_enc_op *ops_enq[num_ops];
	struct rte_bbdev_enc_op *ops_deq[num_ops];
	struct rte_bbdev_enc_op *ref_op = tp->op_params->ref_enc_op;
	struct test_buffers *bufs = NULL;
	int i, j, ret;
	struct rte_bbdev_info info;
	uint16_t num_to_enq;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(tp->dev_id, &info);

	TEST_ASSERT_SUCCESS((num_ops > info.drv.queue_size_lim),
			"NUM_OPS cannot exceed %u for this device",
			info.drv.queue_size_lim);

	bufs = &tp->op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
		rte_pause();

	ret = rte_bbdev_enc_op_alloc_bulk(tp->op_params->mp, ops_enq,
			num_ops);
	TEST_ASSERT_SUCCESS(ret, "Allocation failed for %d ops",
			num_ops);
	if (test_vector.op_type != RTE_BBDEV_OP_NONE)
		copy_reference_ldpc_enc_op(ops_enq, num_ops, 0, bufs->inputs,
				bufs->hard_outputs, ref_op);

	/* Set counter to validate the ordering */
	for (j = 0; j < num_ops; ++j)
		ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

	for (i = 0; i < TEST_REPETITIONS; ++i) {

		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			for (j = 0; j < num_ops; ++j)
				mbuf_reset(ops_enq[j]->turbo_enc.output.data);

		start_time = rte_rdtsc_precise();

		for (enq = 0, deq = 0; enq < num_ops;) {
			num_to_enq = burst_sz;

			if (unlikely(num_ops - enq < num_to_enq))
				num_to_enq = num_ops - enq;

			enq += rte_bbdev_enqueue_ldpc_enc_ops(tp->dev_id,
					queue_id, &ops_enq[enq], num_to_enq);

			deq += rte_bbdev_dequeue_ldpc_enc_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		/* dequeue the remaining */
		while (deq < enq) {
			deq += rte_bbdev_dequeue_ldpc_enc_ops(tp->dev_id,
					queue_id, &ops_deq[deq], enq - deq);
		}

		total_time += rte_rdtsc_precise() - start_time;
	}

	if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
		ret = validate_ldpc_enc_op(ops_deq, num_ops, ref_op);
		TEST_ASSERT_SUCCESS(ret, "Validation failed!");
	}

	rte_bbdev_enc_op_free_bulk(ops_enq, num_ops);

	double tb_len_bits = calc_ldpc_enc_TB_size(ref_op);

	tp->ops_per_sec = ((double)num_ops * TEST_REPETITIONS) /
			((double)total_time / (double)rte_get_tsc_hz());
	tp->mbps = (((double)(num_ops * TEST_REPETITIONS * tb_len_bits))
			/ 1000000.0) / ((double)total_time /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static void
print_enc_throughput(struct thread_params *t_params, unsigned int used_cores)
{
	unsigned int iter = 0;
	double total_mops = 0, total_mbps = 0;

	for (iter = 0; iter < used_cores; iter++) {
		printf(
			"Throughput for core (%u): %.8lg Ops/s, %.8lg Mbps\n",
			t_params[iter].lcore_id, t_params[iter].ops_per_sec,
			t_params[iter].mbps);
		total_mops += t_params[iter].ops_per_sec;
		total_mbps += t_params[iter].mbps;
	}
	printf(
		"\nTotal throughput for %u cores: %.8lg MOPS, %.8lg Mbps\n",
		used_cores, total_mops, total_mbps);
}

/* Aggregate the performance results over the number of cores used */
static void
print_dec_throughput(struct thread_params *t_params, unsigned int used_cores)
{
	unsigned int core_idx = 0;
	double total_mops = 0, total_mbps = 0;
	uint8_t iter_count = 0;

	for (core_idx = 0; core_idx < used_cores; core_idx++) {
		printf(
			"Throughput for core (%u): %.8lg Ops/s, %.8lg Mbps @ max %u iterations\n",
			t_params[core_idx].lcore_id,
			t_params[core_idx].ops_per_sec,
			t_params[core_idx].mbps,
			t_params[core_idx].iter_count);
		total_mops += t_params[core_idx].ops_per_sec;
		total_mbps += t_params[core_idx].mbps;
		iter_count = RTE_MAX(iter_count,
				t_params[core_idx].iter_count);
	}
	printf(
		"\nTotal throughput for %u cores: %.8lg MOPS, %.8lg Mbps @ max %u iterations\n",
		used_cores, total_mops, total_mbps, iter_count);
}

/* Aggregate the performance results over the number of cores used */
static void
print_dec_bler(struct thread_params *t_params, unsigned int used_cores)
{
	unsigned int core_idx = 0;
	double total_mbps = 0, total_bler = 0, total_iter = 0;
	double snr = get_snr();

	for (core_idx = 0; core_idx < used_cores; core_idx++) {
		printf("Core%u BLER %.1f %% - Iters %.1f - Tp %.1f Mbps %s\n",
				t_params[core_idx].lcore_id,
				t_params[core_idx].bler * 100,
				t_params[core_idx].iter_average,
				t_params[core_idx].mbps,
				get_vector_filename());
		total_mbps += t_params[core_idx].mbps;
		total_bler += t_params[core_idx].bler;
		total_iter += t_params[core_idx].iter_average;
	}
	total_bler /= used_cores;
	total_iter /= used_cores;

	printf("SNR %.2f BLER %.1f %% - Iterations %.1f %d - Tp %.1f Mbps %s\n",
			snr, total_bler * 100, total_iter, get_iter_max(),
			total_mbps, get_vector_filename());
}

/*
 * Test function that determines BLER wireless performance
 */
static int
bler_test(struct active_device *ad,
		struct test_op_params *op_params)
{
	int ret;
	unsigned int lcore_id, used_cores = 0;
	struct thread_params *t_params;
	struct rte_bbdev_info info;
	lcore_function_t *bler_function;
	uint16_t num_lcores;
	const char *op_type_str;

	rte_bbdev_info_get(ad->dev_id, &info);

	op_type_str = rte_bbdev_op_type_str(test_vector.op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u",
			test_vector.op_type);

	printf("+ ------------------------------------------------------- +\n");
	printf("== test: bler\ndev: %s, nb_queues: %u, burst size: %u, num ops: %u, num_lcores: %u, op type: %s, itr mode: %s, GHz: %lg\n",
			info.dev_name, ad->nb_queues, op_params->burst_sz,
			op_params->num_to_process, op_params->num_lcores,
			op_type_str,
			intr_enabled ? "Interrupt mode" : "PMD mode",
			(double)rte_get_tsc_hz() / 1000000000.0);

	/* Set number of lcores */
	num_lcores = (ad->nb_queues < (op_params->num_lcores))
			? ad->nb_queues
			: op_params->num_lcores;

	/* Allocate memory for thread parameters structure */
	t_params = rte_zmalloc(NULL, num_lcores * sizeof(struct thread_params),
			RTE_CACHE_LINE_SIZE);
	TEST_ASSERT_NOT_NULL(t_params, "Failed to alloc %zuB for t_params",
			RTE_ALIGN(sizeof(struct thread_params) * num_lcores,
				RTE_CACHE_LINE_SIZE));

	if ((test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC) &&
			!check_bit(test_vector.ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)
			&& !check_bit(test_vector.ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_LLR_COMPRESSION))
		bler_function = bler_pmd_lcore_ldpc_dec;
	else
		return TEST_SKIPPED;

	rte_atomic16_set(&op_params->sync, SYNC_WAIT);

	/* Main core is set at first entry */
	t_params[0].dev_id = ad->dev_id;
	t_params[0].lcore_id = rte_lcore_id();
	t_params[0].op_params = op_params;
	t_params[0].queue_id = ad->queue_ids[used_cores++];
	t_params[0].iter_count = 0;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (used_cores >= num_lcores)
			break;

		t_params[used_cores].dev_id = ad->dev_id;
		t_params[used_cores].lcore_id = lcore_id;
		t_params[used_cores].op_params = op_params;
		t_params[used_cores].queue_id = ad->queue_ids[used_cores];
		t_params[used_cores].iter_count = 0;

		rte_eal_remote_launch(bler_function,
				&t_params[used_cores++], lcore_id);
	}

	rte_atomic16_set(&op_params->sync, SYNC_START);
	ret = bler_function(&t_params[0]);

	/* Main core is always used */
	for (used_cores = 1; used_cores < num_lcores; used_cores++)
		ret |= rte_eal_wait_lcore(t_params[used_cores].lcore_id);

	print_dec_bler(t_params, num_lcores);

	/* Return if test failed */
	if (ret) {
		rte_free(t_params);
		return ret;
	}

	/* Function to print something  here*/
	rte_free(t_params);
	return ret;
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
	struct thread_params *t_params, *tp;
	struct rte_bbdev_info info;
	lcore_function_t *throughput_function;
	uint16_t num_lcores;
	const char *op_type_str;

	rte_bbdev_info_get(ad->dev_id, &info);

	op_type_str = rte_bbdev_op_type_str(test_vector.op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u",
			test_vector.op_type);

	printf("+ ------------------------------------------------------- +\n");
	printf("== test: throughput\ndev: %s, nb_queues: %u, burst size: %u, num ops: %u, num_lcores: %u, op type: %s, itr mode: %s, GHz: %lg\n",
			info.dev_name, ad->nb_queues, op_params->burst_sz,
			op_params->num_to_process, op_params->num_lcores,
			op_type_str,
			intr_enabled ? "Interrupt mode" : "PMD mode",
			(double)rte_get_tsc_hz() / 1000000000.0);

	/* Set number of lcores */
	num_lcores = (ad->nb_queues < (op_params->num_lcores))
			? ad->nb_queues
			: op_params->num_lcores;

	/* Allocate memory for thread parameters structure */
	t_params = rte_zmalloc(NULL, num_lcores * sizeof(struct thread_params),
			RTE_CACHE_LINE_SIZE);
	TEST_ASSERT_NOT_NULL(t_params, "Failed to alloc %zuB for t_params",
			RTE_ALIGN(sizeof(struct thread_params) * num_lcores,
				RTE_CACHE_LINE_SIZE));

	if (intr_enabled) {
		if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
			throughput_function = throughput_intr_lcore_dec;
		else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC)
			throughput_function = throughput_intr_lcore_ldpc_dec;
		else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC)
			throughput_function = throughput_intr_lcore_enc;
		else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_ENC)
			throughput_function = throughput_intr_lcore_ldpc_enc;
		else
			throughput_function = throughput_intr_lcore_enc;

		/* Dequeue interrupt callback registration */
		ret = rte_bbdev_callback_register(ad->dev_id,
				RTE_BBDEV_EVENT_DEQUEUE, dequeue_event_callback,
				t_params);
		if (ret < 0) {
			rte_free(t_params);
			return ret;
		}
	} else {
		if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC)
			throughput_function = throughput_pmd_lcore_dec;
		else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC)
			throughput_function = throughput_pmd_lcore_ldpc_dec;
		else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC)
			throughput_function = throughput_pmd_lcore_enc;
		else if (test_vector.op_type == RTE_BBDEV_OP_LDPC_ENC)
			throughput_function = throughput_pmd_lcore_ldpc_enc;
		else
			throughput_function = throughput_pmd_lcore_enc;
	}

	rte_atomic16_set(&op_params->sync, SYNC_WAIT);

	/* Main core is set at first entry */
	t_params[0].dev_id = ad->dev_id;
	t_params[0].lcore_id = rte_lcore_id();
	t_params[0].op_params = op_params;
	t_params[0].queue_id = ad->queue_ids[used_cores++];
	t_params[0].iter_count = 0;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (used_cores >= num_lcores)
			break;

		t_params[used_cores].dev_id = ad->dev_id;
		t_params[used_cores].lcore_id = lcore_id;
		t_params[used_cores].op_params = op_params;
		t_params[used_cores].queue_id = ad->queue_ids[used_cores];
		t_params[used_cores].iter_count = 0;

		rte_eal_remote_launch(throughput_function,
				&t_params[used_cores++], lcore_id);
	}

	rte_atomic16_set(&op_params->sync, SYNC_START);
	ret = throughput_function(&t_params[0]);

	/* Main core is always used */
	for (used_cores = 1; used_cores < num_lcores; used_cores++)
		ret |= rte_eal_wait_lcore(t_params[used_cores].lcore_id);

	/* Return if test failed */
	if (ret) {
		rte_free(t_params);
		return ret;
	}

	/* Print throughput if interrupts are disabled and test passed */
	if (!intr_enabled) {
		if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC ||
				test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC)
			print_dec_throughput(t_params, num_lcores);
		else
			print_enc_throughput(t_params, num_lcores);
		rte_free(t_params);
		return ret;
	}

	/* In interrupt TC we need to wait for the interrupt callback to deqeue
	 * all pending operations. Skip waiting for queues which reported an
	 * error using processing_status variable.
	 * Wait for main lcore operations.
	 */
	tp = &t_params[0];
	while ((rte_atomic16_read(&tp->nb_dequeued) <
			op_params->num_to_process) &&
			(rte_atomic16_read(&tp->processing_status) !=
			TEST_FAILED))
		rte_pause();

	tp->ops_per_sec /= TEST_REPETITIONS;
	tp->mbps /= TEST_REPETITIONS;
	ret |= (int)rte_atomic16_read(&tp->processing_status);

	/* Wait for worker lcores operations */
	for (used_cores = 1; used_cores < num_lcores; used_cores++) {
		tp = &t_params[used_cores];

		while ((rte_atomic16_read(&tp->nb_dequeued) <
				op_params->num_to_process) &&
				(rte_atomic16_read(&tp->processing_status) !=
				TEST_FAILED))
			rte_pause();

		tp->ops_per_sec /= TEST_REPETITIONS;
		tp->mbps /= TEST_REPETITIONS;
		ret |= (int)rte_atomic16_read(&tp->processing_status);
	}

	/* Print throughput if test passed */
	if (!ret) {
		if (test_vector.op_type == RTE_BBDEV_OP_TURBO_DEC ||
				test_vector.op_type == RTE_BBDEV_OP_LDPC_DEC)
			print_dec_throughput(t_params, num_lcores);
		else if (test_vector.op_type == RTE_BBDEV_OP_TURBO_ENC ||
				test_vector.op_type == RTE_BBDEV_OP_LDPC_ENC)
			print_enc_throughput(t_params, num_lcores);
	}

	rte_free(t_params);
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

/* Test case for latency/validation for LDPC Decoder */
static int
latency_test_ldpc_dec(struct rte_mempool *mempool,
		struct test_buffers *bufs, struct rte_bbdev_dec_op *ref_op,
		int vector_mask, uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *total_time, uint64_t *min_time, uint64_t *max_time,
		bool disable_et)
{
	int ret = TEST_SUCCESS;
	uint16_t i, j, dequeued;
	struct rte_bbdev_dec_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t start_time = 0, last_time = 0;
	bool extDdr = ldpc_cap_flags &
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;

	for (i = 0, dequeued = 0; dequeued < num_to_process; ++i) {
		uint16_t enq = 0, deq = 0;
		bool first_time = true;
		last_time = 0;

		if (unlikely(num_to_process - dequeued < burst_sz))
			burst_sz = num_to_process - dequeued;

		ret = rte_bbdev_dec_op_alloc_bulk(mempool, ops_enq, burst_sz);
		TEST_ASSERT_SUCCESS(ret,
				"rte_bbdev_dec_op_alloc_bulk() failed");

		/* For latency tests we need to disable early termination */
		if (disable_et && check_bit(ref_op->ldpc_dec.op_flags,
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE))
			ref_op->ldpc_dec.op_flags -=
					RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
		ref_op->ldpc_dec.iter_max = get_iter_max();
		ref_op->ldpc_dec.iter_count = ref_op->ldpc_dec.iter_max;

		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_ldpc_dec_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					bufs->soft_outputs,
					bufs->harq_inputs,
					bufs->harq_outputs,
					ref_op);

		if (extDdr)
			preload_harq_ddr(dev_id, queue_id, ops_enq,
					burst_sz, true);

		/* Set counter to validate the ordering */
		for (j = 0; j < burst_sz; ++j)
			ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

		start_time = rte_rdtsc_precise();

		enq = rte_bbdev_enqueue_ldpc_dec_ops(dev_id, queue_id,
				&ops_enq[enq], burst_sz);
		TEST_ASSERT(enq == burst_sz,
				"Error enqueueing burst, expected %u, got %u",
				burst_sz, enq);

		/* Dequeue */
		do {
			deq += rte_bbdev_dequeue_ldpc_dec_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);
			if (likely(first_time && (deq > 0))) {
				last_time = rte_rdtsc_precise() - start_time;
				first_time = false;
			}
		} while (unlikely(burst_sz != deq));

		*max_time = RTE_MAX(*max_time, last_time);
		*min_time = RTE_MIN(*min_time, last_time);
		*total_time += last_time;

		if (extDdr)
			retrieve_harq_ddr(dev_id, queue_id, ops_enq, burst_sz);

		if (test_vector.op_type != RTE_BBDEV_OP_NONE) {
			ret = validate_ldpc_dec_op(ops_deq, burst_sz, ref_op,
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
latency_test_ldpc_enc(struct rte_mempool *mempool,
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
			copy_reference_ldpc_enc_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					ref_op);

		/* Set counter to validate the ordering */
		for (j = 0; j < burst_sz; ++j)
			ops_enq[j]->opaque_data = (void *)(uintptr_t)j;

		start_time = rte_rdtsc_precise();

		enq = rte_bbdev_enqueue_ldpc_enc_ops(dev_id, queue_id,
				&ops_enq[enq], burst_sz);
		TEST_ASSERT(enq == burst_sz,
				"Error enqueueing burst, expected %u, got %u",
				burst_sz, enq);

		/* Dequeue */
		do {
			deq += rte_bbdev_dequeue_ldpc_enc_ops(dev_id, queue_id,
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

/* Common function for running validation and latency test cases */
static int
validation_latency_test(struct active_device *ad,
		struct test_op_params *op_params, bool latency_flag)
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

	printf("+ ------------------------------------------------------- +\n");
	if (latency_flag)
		printf("== test: latency\ndev:");
	else
		printf("== test: validation\ndev:");
	printf("%s, burst size: %u, num ops: %u, op type: %s\n",
			info.dev_name, burst_sz, num_to_process, op_type_str);

	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		iter = latency_test_dec(op_params->mp, bufs,
				op_params->ref_dec_op, op_params->vector_mask,
				ad->dev_id, queue_id, num_to_process,
				burst_sz, &total_time, &min_time, &max_time);
	else if (op_type == RTE_BBDEV_OP_LDPC_ENC)
		iter = latency_test_ldpc_enc(op_params->mp, bufs,
				op_params->ref_enc_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &total_time,
				&min_time, &max_time);
	else if (op_type == RTE_BBDEV_OP_LDPC_DEC)
		iter = latency_test_ldpc_dec(op_params->mp, bufs,
				op_params->ref_dec_op, op_params->vector_mask,
				ad->dev_id, queue_id, num_to_process,
				burst_sz, &total_time, &min_time, &max_time,
				latency_flag);
	else /* RTE_BBDEV_OP_TURBO_ENC */
		iter = latency_test_enc(op_params->mp, bufs,
				op_params->ref_enc_op,
				ad->dev_id, queue_id,
				num_to_process, burst_sz, &total_time,
				&min_time, &max_time);

	if (iter <= 0)
		return TEST_FAILED;

	printf("Operation latency:\n"
			"\tavg: %lg cycles, %lg us\n"
			"\tmin: %lg cycles, %lg us\n"
			"\tmax: %lg cycles, %lg us\n",
			(double)total_time / (double)iter,
			(double)(total_time * 1000000) / (double)iter /
			(double)rte_get_tsc_hz(), (double)min_time,
			(double)(min_time * 1000000) / (double)rte_get_tsc_hz(),
			(double)max_time, (double)(max_time * 1000000) /
			(double)rte_get_tsc_hz());

	return TEST_SUCCESS;
}

static int
latency_test(struct active_device *ad, struct test_op_params *op_params)
{
	return validation_latency_test(ad, op_params, true);
}

static int
validation_test(struct active_device *ad, struct test_op_params *op_params)
{
	return validation_latency_test(ad, op_params, false);
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
	stats->acc_offload_cycles = q_stats->acc_offload_cycles;

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
				stats.acc_offload_cycles;
		time_st->enq_sw_max_time = RTE_MAX(time_st->enq_sw_max_time,
				enq_sw_last_time);
		time_st->enq_sw_min_time = RTE_MIN(time_st->enq_sw_min_time,
				enq_sw_last_time);
		time_st->enq_sw_total_time += enq_sw_last_time;

		time_st->enq_acc_max_time = RTE_MAX(time_st->enq_acc_max_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_min_time = RTE_MIN(time_st->enq_acc_min_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_total_time += stats.acc_offload_cycles;

		/* give time for device to process ops */
		rte_delay_us(WAIT_OFFLOAD_US);

		/* Start time meas for dequeue function offload latency */
		deq_start_time = rte_rdtsc_precise();
		/* Dequeue one operation */
		do {
			deq += rte_bbdev_dequeue_dec_ops(dev_id, queue_id,
					&ops_deq[deq], enq);
		} while (unlikely(deq == 0));

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		time_st->deq_max_time = RTE_MAX(time_st->deq_max_time,
				deq_last_time);
		time_st->deq_min_time = RTE_MIN(time_st->deq_min_time,
				deq_last_time);
		time_st->deq_total_time += deq_last_time;

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
offload_latency_test_ldpc_dec(struct rte_mempool *mempool,
		struct test_buffers *bufs,
		struct rte_bbdev_dec_op *ref_op, uint16_t dev_id,
		uint16_t queue_id, const uint16_t num_to_process,
		uint16_t burst_sz, struct test_time_stats *time_st)
{
	int i, dequeued, ret;
	struct rte_bbdev_dec_op *ops_enq[MAX_BURST], *ops_deq[MAX_BURST];
	uint64_t enq_start_time, deq_start_time;
	uint64_t enq_sw_last_time, deq_last_time;
	struct rte_bbdev_stats stats;
	bool extDdr = ldpc_cap_flags &
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;

	for (i = 0, dequeued = 0; dequeued < num_to_process; ++i) {
		uint16_t enq = 0, deq = 0;

		if (unlikely(num_to_process - dequeued < burst_sz))
			burst_sz = num_to_process - dequeued;

		rte_bbdev_dec_op_alloc_bulk(mempool, ops_enq, burst_sz);
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_ldpc_dec_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					bufs->soft_outputs,
					bufs->harq_inputs,
					bufs->harq_outputs,
					ref_op);

		if (extDdr)
			preload_harq_ddr(dev_id, queue_id, ops_enq,
					burst_sz, true);

		/* Start time meas for enqueue function offload latency */
		enq_start_time = rte_rdtsc_precise();
		do {
			enq += rte_bbdev_enqueue_ldpc_dec_ops(dev_id, queue_id,
					&ops_enq[enq], burst_sz - enq);
		} while (unlikely(burst_sz != enq));

		enq_sw_last_time = rte_rdtsc_precise() - enq_start_time;
		ret = get_bbdev_queue_stats(dev_id, queue_id, &stats);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to get stats for queue (%u) of device (%u)",
				queue_id, dev_id);

		enq_sw_last_time -= stats.acc_offload_cycles;
		time_st->enq_sw_max_time = RTE_MAX(time_st->enq_sw_max_time,
				enq_sw_last_time);
		time_st->enq_sw_min_time = RTE_MIN(time_st->enq_sw_min_time,
				enq_sw_last_time);
		time_st->enq_sw_total_time += enq_sw_last_time;

		time_st->enq_acc_max_time = RTE_MAX(time_st->enq_acc_max_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_min_time = RTE_MIN(time_st->enq_acc_min_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_total_time += stats.acc_offload_cycles;

		/* give time for device to process ops */
		rte_delay_us(WAIT_OFFLOAD_US);

		/* Start time meas for dequeue function offload latency */
		deq_start_time = rte_rdtsc_precise();
		/* Dequeue one operation */
		do {
			deq += rte_bbdev_dequeue_ldpc_dec_ops(dev_id, queue_id,
					&ops_deq[deq], enq);
		} while (unlikely(deq == 0));

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		time_st->deq_max_time = RTE_MAX(time_st->deq_max_time,
				deq_last_time);
		time_st->deq_min_time = RTE_MIN(time_st->deq_min_time,
				deq_last_time);
		time_st->deq_total_time += deq_last_time;

		/* Dequeue remaining operations if needed*/
		while (burst_sz != deq)
			deq += rte_bbdev_dequeue_ldpc_dec_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);

		if (extDdr) {
			/* Read loopback is not thread safe */
			retrieve_harq_ddr(dev_id, queue_id, ops_enq, burst_sz);
		}

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

		ret = rte_bbdev_enc_op_alloc_bulk(mempool, ops_enq, burst_sz);
		TEST_ASSERT_SUCCESS(ret,
				"rte_bbdev_enc_op_alloc_bulk() failed");
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

		enq_sw_last_time = rte_rdtsc_precise() - enq_start_time;

		ret = get_bbdev_queue_stats(dev_id, queue_id, &stats);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to get stats for queue (%u) of device (%u)",
				queue_id, dev_id);
		enq_sw_last_time -= stats.acc_offload_cycles;
		time_st->enq_sw_max_time = RTE_MAX(time_st->enq_sw_max_time,
				enq_sw_last_time);
		time_st->enq_sw_min_time = RTE_MIN(time_st->enq_sw_min_time,
				enq_sw_last_time);
		time_st->enq_sw_total_time += enq_sw_last_time;

		time_st->enq_acc_max_time = RTE_MAX(time_st->enq_acc_max_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_min_time = RTE_MIN(time_st->enq_acc_min_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_total_time += stats.acc_offload_cycles;

		/* give time for device to process ops */
		rte_delay_us(WAIT_OFFLOAD_US);

		/* Start time meas for dequeue function offload latency */
		deq_start_time = rte_rdtsc_precise();
		/* Dequeue one operation */
		do {
			deq += rte_bbdev_dequeue_enc_ops(dev_id, queue_id,
					&ops_deq[deq], enq);
		} while (unlikely(deq == 0));

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		time_st->deq_max_time = RTE_MAX(time_st->deq_max_time,
				deq_last_time);
		time_st->deq_min_time = RTE_MIN(time_st->deq_min_time,
				deq_last_time);
		time_st->deq_total_time += deq_last_time;

		while (burst_sz != deq)
			deq += rte_bbdev_dequeue_enc_ops(dev_id, queue_id,
					&ops_deq[deq], burst_sz - deq);

		rte_bbdev_enc_op_free_bulk(ops_enq, deq);
		dequeued += deq;
	}

	return i;
}

static int
offload_latency_test_ldpc_enc(struct rte_mempool *mempool,
		struct test_buffers *bufs,
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

		ret = rte_bbdev_enc_op_alloc_bulk(mempool, ops_enq, burst_sz);
		TEST_ASSERT_SUCCESS(ret,
				"rte_bbdev_enc_op_alloc_bulk() failed");
		if (test_vector.op_type != RTE_BBDEV_OP_NONE)
			copy_reference_ldpc_enc_op(ops_enq, burst_sz, dequeued,
					bufs->inputs,
					bufs->hard_outputs,
					ref_op);

		/* Start time meas for enqueue function offload latency */
		enq_start_time = rte_rdtsc_precise();
		do {
			enq += rte_bbdev_enqueue_ldpc_enc_ops(dev_id, queue_id,
					&ops_enq[enq], burst_sz - enq);
		} while (unlikely(burst_sz != enq));

		enq_sw_last_time = rte_rdtsc_precise() - enq_start_time;
		ret = get_bbdev_queue_stats(dev_id, queue_id, &stats);
		TEST_ASSERT_SUCCESS(ret,
				"Failed to get stats for queue (%u) of device (%u)",
				queue_id, dev_id);

		enq_sw_last_time -= stats.acc_offload_cycles;
		time_st->enq_sw_max_time = RTE_MAX(time_st->enq_sw_max_time,
				enq_sw_last_time);
		time_st->enq_sw_min_time = RTE_MIN(time_st->enq_sw_min_time,
				enq_sw_last_time);
		time_st->enq_sw_total_time += enq_sw_last_time;

		time_st->enq_acc_max_time = RTE_MAX(time_st->enq_acc_max_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_min_time = RTE_MIN(time_st->enq_acc_min_time,
				stats.acc_offload_cycles);
		time_st->enq_acc_total_time += stats.acc_offload_cycles;

		/* give time for device to process ops */
		rte_delay_us(WAIT_OFFLOAD_US);

		/* Start time meas for dequeue function offload latency */
		deq_start_time = rte_rdtsc_precise();
		/* Dequeue one operation */
		do {
			deq += rte_bbdev_dequeue_ldpc_enc_ops(dev_id, queue_id,
					&ops_deq[deq], enq);
		} while (unlikely(deq == 0));

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		time_st->deq_max_time = RTE_MAX(time_st->deq_max_time,
				deq_last_time);
		time_st->deq_min_time = RTE_MIN(time_st->deq_min_time,
				deq_last_time);
		time_st->deq_total_time += deq_last_time;

		while (burst_sz != deq)
			deq += rte_bbdev_dequeue_ldpc_enc_ops(dev_id, queue_id,
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
	time_st.enq_acc_min_time = UINT64_MAX;
	time_st.deq_min_time = UINT64_MAX;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(ad->dev_id, &info);
	bufs = &op_params->q_bufs[GET_SOCKET(info.socket_id)][queue_id];

	op_type_str = rte_bbdev_op_type_str(op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u", op_type);

	printf("+ ------------------------------------------------------- +\n");
	printf("== test: offload latency test\ndev: %s, burst size: %u, num ops: %u, op type: %s\n",
			info.dev_name, burst_sz, num_to_process, op_type_str);

	if (op_type == RTE_BBDEV_OP_TURBO_DEC)
		iter = offload_latency_test_dec(op_params->mp, bufs,
				op_params->ref_dec_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &time_st);
	else if (op_type == RTE_BBDEV_OP_TURBO_ENC)
		iter = offload_latency_test_enc(op_params->mp, bufs,
				op_params->ref_enc_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &time_st);
	else if (op_type == RTE_BBDEV_OP_LDPC_ENC)
		iter = offload_latency_test_ldpc_enc(op_params->mp, bufs,
				op_params->ref_enc_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &time_st);
	else if (op_type == RTE_BBDEV_OP_LDPC_DEC)
		iter = offload_latency_test_ldpc_dec(op_params->mp, bufs,
			op_params->ref_dec_op, ad->dev_id, queue_id,
			num_to_process, burst_sz, &time_st);
	else
		iter = offload_latency_test_enc(op_params->mp, bufs,
				op_params->ref_enc_op, ad->dev_id, queue_id,
				num_to_process, burst_sz, &time_st);

	if (iter <= 0)
		return TEST_FAILED;

	printf("Enqueue driver offload cost latency:\n"
			"\tavg: %lg cycles, %lg us\n"
			"\tmin: %lg cycles, %lg us\n"
			"\tmax: %lg cycles, %lg us\n"
			"Enqueue accelerator offload cost latency:\n"
			"\tavg: %lg cycles, %lg us\n"
			"\tmin: %lg cycles, %lg us\n"
			"\tmax: %lg cycles, %lg us\n",
			(double)time_st.enq_sw_total_time / (double)iter,
			(double)(time_st.enq_sw_total_time * 1000000) /
			(double)iter / (double)rte_get_tsc_hz(),
			(double)time_st.enq_sw_min_time,
			(double)(time_st.enq_sw_min_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.enq_sw_max_time,
			(double)(time_st.enq_sw_max_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.enq_acc_total_time /
			(double)iter,
			(double)(time_st.enq_acc_total_time * 1000000) /
			(double)iter / (double)rte_get_tsc_hz(),
			(double)time_st.enq_acc_min_time,
			(double)(time_st.enq_acc_min_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.enq_acc_max_time,
			(double)(time_st.enq_acc_max_time * 1000000) /
			rte_get_tsc_hz());

	printf("Dequeue offload cost latency - one op:\n"
			"\tavg: %lg cycles, %lg us\n"
			"\tmin: %lg cycles, %lg us\n"
			"\tmax: %lg cycles, %lg us\n",
			(double)time_st.deq_total_time / (double)iter,
			(double)(time_st.deq_total_time * 1000000) /
			(double)iter / (double)rte_get_tsc_hz(),
			(double)time_st.deq_min_time,
			(double)(time_st.deq_min_time * 1000000) /
			rte_get_tsc_hz(), (double)time_st.deq_max_time,
			(double)(time_st.deq_max_time * 1000000) /
			rte_get_tsc_hz());

	struct rte_bbdev_stats stats = {0};
	get_bbdev_queue_stats(ad->dev_id, queue_id, &stats);
	if (op_type != RTE_BBDEV_OP_LDPC_DEC) {
		TEST_ASSERT_SUCCESS(stats.enqueued_count != num_to_process,
				"Mismatch in enqueue count %10"PRIu64" %d",
				stats.enqueued_count, num_to_process);
		TEST_ASSERT_SUCCESS(stats.dequeued_count != num_to_process,
				"Mismatch in dequeue count %10"PRIu64" %d",
				stats.dequeued_count, num_to_process);
	}
	TEST_ASSERT_SUCCESS(stats.enqueue_err_count != 0,
			"Enqueue count Error %10"PRIu64"",
			stats.enqueue_err_count);
	TEST_ASSERT_SUCCESS(stats.dequeue_err_count != 0,
			"Dequeue count Error (%10"PRIu64"",
			stats.dequeue_err_count);

	return TEST_SUCCESS;
#endif
}

#ifdef RTE_BBDEV_OFFLOAD_COST
static int
offload_latency_empty_q_test_dec(uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *deq_total_time, uint64_t *deq_min_time,
		uint64_t *deq_max_time, const enum rte_bbdev_op_type op_type)
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
		if (op_type == RTE_BBDEV_OP_LDPC_DEC)
			rte_bbdev_dequeue_ldpc_dec_ops(dev_id, queue_id, ops,
					burst_sz);
		else
			rte_bbdev_dequeue_dec_ops(dev_id, queue_id, ops,
					burst_sz);

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		*deq_max_time = RTE_MAX(*deq_max_time, deq_last_time);
		*deq_min_time = RTE_MIN(*deq_min_time, deq_last_time);
		*deq_total_time += deq_last_time;
	}

	return i;
}

static int
offload_latency_empty_q_test_enc(uint16_t dev_id, uint16_t queue_id,
		const uint16_t num_to_process, uint16_t burst_sz,
		uint64_t *deq_total_time, uint64_t *deq_min_time,
		uint64_t *deq_max_time, const enum rte_bbdev_op_type op_type)
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
		if (op_type == RTE_BBDEV_OP_LDPC_ENC)
			rte_bbdev_dequeue_ldpc_enc_ops(dev_id, queue_id, ops,
					burst_sz);
		else
			rte_bbdev_dequeue_enc_ops(dev_id, queue_id, ops,
					burst_sz);

		deq_last_time = rte_rdtsc_precise() - deq_start_time;
		*deq_max_time = RTE_MAX(*deq_max_time, deq_last_time);
		*deq_min_time = RTE_MIN(*deq_min_time, deq_last_time);
		*deq_total_time += deq_last_time;
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
	uint64_t deq_total_time, deq_min_time, deq_max_time;
	uint16_t burst_sz = op_params->burst_sz;
	const uint16_t num_to_process = op_params->num_to_process;
	const enum rte_bbdev_op_type op_type = test_vector.op_type;
	const uint16_t queue_id = ad->queue_ids[0];
	struct rte_bbdev_info info;
	const char *op_type_str;

	deq_total_time = deq_max_time = 0;
	deq_min_time = UINT64_MAX;

	TEST_ASSERT_SUCCESS((burst_sz > MAX_BURST),
			"BURST_SIZE should be <= %u", MAX_BURST);

	rte_bbdev_info_get(ad->dev_id, &info);

	op_type_str = rte_bbdev_op_type_str(op_type);
	TEST_ASSERT_NOT_NULL(op_type_str, "Invalid op type: %u", op_type);

	printf("+ ------------------------------------------------------- +\n");
	printf("== test: offload latency empty dequeue\ndev: %s, burst size: %u, num ops: %u, op type: %s\n",
			info.dev_name, burst_sz, num_to_process, op_type_str);

	if (op_type == RTE_BBDEV_OP_TURBO_DEC ||
			op_type == RTE_BBDEV_OP_LDPC_DEC)
		iter = offload_latency_empty_q_test_dec(ad->dev_id, queue_id,
				num_to_process, burst_sz, &deq_total_time,
				&deq_min_time, &deq_max_time, op_type);
	else
		iter = offload_latency_empty_q_test_enc(ad->dev_id, queue_id,
				num_to_process, burst_sz, &deq_total_time,
				&deq_min_time, &deq_max_time, op_type);

	if (iter <= 0)
		return TEST_FAILED;

	printf("Empty dequeue offload:\n"
			"\tavg: %lg cycles, %lg us\n"
			"\tmin: %lg cycles, %lg us\n"
			"\tmax: %lg cycles, %lg us\n",
			(double)deq_total_time / (double)iter,
			(double)(deq_total_time * 1000000) / (double)iter /
			(double)rte_get_tsc_hz(), (double)deq_min_time,
			(double)(deq_min_time * 1000000) / rte_get_tsc_hz(),
			(double)deq_max_time, (double)(deq_max_time * 1000000) /
			rte_get_tsc_hz());

	return TEST_SUCCESS;
#endif
}

static int
bler_tc(void)
{
	return run_test_case(bler_test);
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
validation_tc(void)
{
	return run_test_case(validation_test);
}

static int
interrupt_tc(void)
{
	return run_test_case(throughput_test);
}

static struct unit_test_suite bbdev_bler_testsuite = {
	.suite_name = "BBdev BLER Tests",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, bler_tc),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

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
		TEST_CASE_ST(ut_setup, ut_teardown, validation_tc),
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

REGISTER_TEST_COMMAND(bler, bbdev_bler_testsuite);
REGISTER_TEST_COMMAND(throughput, bbdev_throughput_testsuite);
REGISTER_TEST_COMMAND(validation, bbdev_validation_testsuite);
REGISTER_TEST_COMMAND(latency, bbdev_latency_testsuite);
REGISTER_TEST_COMMAND(offload, bbdev_offload_cost_testsuite);
REGISTER_TEST_COMMAND(interrupt, bbdev_interrupt_testsuite);
