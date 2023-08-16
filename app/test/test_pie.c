/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"

#ifdef RTE_EXEC_ENV_WINDOWS

static int
test_pie(void)
{
	printf("pie not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

static int
test_pie_perf(void)
{
	printf("pie_perf not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

static int
test_pie_all(void)
{
	printf("pie_all not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include <rte_pie.h>

#ifdef __INTEL_COMPILER
#pragma warning(disable:2259)       /* conversion may lose significant bits */
#pragma warning(disable:181)        /* Arg incompatible with format string */
#endif

/**< structures for testing rte_pie performance and function */
struct test_rte_pie_config {        /**< Test structure for RTE_PIE config */
	struct rte_pie_config *pconfig; /**< RTE_PIE configuration parameters */
	uint8_t num_cfg;                /**< Number of RTE_PIE configs to test */
	uint16_t qdelay_ref;            /**< Latency Target (milliseconds) */
	uint16_t *dp_update_interval;   /**< Update interval for drop probability
					  * (milliseconds)
					  */
	uint16_t *max_burst;            /**< Max Burst Allowance (milliseconds) */
	uint16_t tailq_th;              /**< Tailq drop threshold (packet counts) */
};

struct test_queue {                 /**< Test structure for RTE_PIE Queues */
	struct rte_pie *pdata_in;       /**< RTE_PIE runtime data input */
	struct rte_pie *pdata_out;		/**< RTE_PIE runtime data output*/
	uint32_t num_queues;            /**< Number of RTE_PIE queues to test */
	uint32_t *qlen;                 /**< Queue size */
	uint32_t q_ramp_up;             /**< Num of enqueues to ramp up the queue */
	double drop_tolerance;          /**< Drop tolerance of packets not enqueued */
};

struct test_var {                   /**< Test variables used for testing RTE_PIE */
	uint32_t num_iterations;        /**< Number of test iterations */
	uint32_t num_ops;               /**< Number of test operations */
	uint64_t clk_freq;              /**< CPU clock frequency */
	uint32_t *dropped;              /**< Test operations dropped */
	uint32_t *enqueued;             /**< Test operations enqueued */
	uint32_t *dequeued;             /**< Test operations dequeued */
};

struct test_config {                /**< Primary test structure for RTE_PIE */
	const char *ifname;             /**< Interface name */
	const char *msg;                /**< Test message for display */
	const char *htxt;               /**< Header txt display for result output */
	struct test_rte_pie_config *tconfig; /**< Test structure for RTE_PIE config */
	struct test_queue *tqueue;      /**< Test structure for RTE_PIE Queues */
	struct test_var *tvar;          /**< Test variables used for testing RTE_PIE */
	uint32_t *tlevel;               /**< Queue levels */
};

enum test_result {
	FAIL = 0,
	PASS
};

/**< Test structure to define tests to run */
struct tests {
	struct test_config *testcfg;
	enum test_result (*testfn)(struct test_config *cfg);
};

struct rdtsc_prof {
	uint64_t clk_start;
	uint64_t clk_min;               /**< min clocks */
	uint64_t clk_max;               /**< max clocks */
	uint64_t clk_avgc;              /**< count to calc average */
	double clk_avg;                 /**< cumulative sum to calc average */
	const char *name;
};

static const uint64_t port_speed_bytes = (10ULL*1000ULL*1000ULL*1000ULL)/8ULL;
static double inv_cycles_per_byte;

static void init_port_ts(uint64_t cpu_clock)
{
	double cycles_per_byte = (double)(cpu_clock) / (double)(port_speed_bytes);
	inv_cycles_per_byte = 1.0 / cycles_per_byte;
}

static uint64_t get_port_ts(void)
{
	return (uint64_t)((double)rte_rdtsc() * inv_cycles_per_byte);
}

static void rdtsc_prof_init(struct rdtsc_prof *p, const char *name)
{
	p->clk_min = (uint64_t)(-1LL);
	p->clk_max = 0;
	p->clk_avg = 0;
	p->clk_avgc = 0;
	p->name = name;
}

static inline void rdtsc_prof_start(struct rdtsc_prof *p)
{
	p->clk_start = rte_rdtsc_precise();
}

static inline void rdtsc_prof_end(struct rdtsc_prof *p)
{
	uint64_t clk_start = rte_rdtsc() - p->clk_start;

	p->clk_avgc++;
	p->clk_avg += (double) clk_start;

	if (clk_start > p->clk_max)
		p->clk_max = clk_start;
	if (clk_start < p->clk_min)
		p->clk_min = clk_start;
}

static void rdtsc_prof_print(struct rdtsc_prof *p)
{
	if (p->clk_avgc > 0) {
		printf("RDTSC stats for %s: n=%" PRIu64 ", min=%" PRIu64
						",max=%" PRIu64 ", avg=%.1f\n",
			p->name,
			p->clk_avgc,
			p->clk_min,
			p->clk_max,
			(p->clk_avg / ((double) p->clk_avgc)));
	}
}

static uint16_t rte_pie_get_active(const struct rte_pie_config *pie_cfg,
				    struct rte_pie *pie)
{
    /**< Flag for activating/deactivating pie */
	RTE_SET_USED(pie_cfg);
	return pie->active;
}

static void rte_pie_set_active(const struct rte_pie_config *pie_cfg,
					struct rte_pie *pie,
					uint16_t active)
{
    /**< Flag for activating/deactivating pie */
	RTE_SET_USED(pie_cfg);
	pie->active = active;
}

/**
 * Read the drop probability
 */
static double rte_pie_get_drop_prob(const struct rte_pie_config *pie_cfg,
				    struct rte_pie *pie)
{
    /**< Current packet drop probability */
	RTE_SET_USED(pie_cfg);
	return pie->drop_prob;
}

static double rte_pie_get_avg_dq_time(const struct rte_pie_config *pie_cfg,
				    struct rte_pie *pie)
{
    /**< Current packet drop probability */
	RTE_SET_USED(pie_cfg);
	return pie->avg_dq_time;
}

static double calc_drop_rate(uint32_t enqueued, uint32_t dropped)
{
	return (double)dropped / ((double)enqueued + (double)dropped);
}

/**
 *  check if drop rate matches drop probability within tolerance
 */
static int check_drop_rate(double *diff, double drop_rate, double drop_prob,
							double tolerance)
{
	double abs_diff = 0.0;
	int ret = 1;

	abs_diff = fabs(drop_rate - drop_prob);
	if ((int)abs_diff == 0) {
		*diff = 0.0;
	} else {
		*diff = (abs_diff / drop_prob) * 100.0;
		if (*diff > tolerance)
			ret = 0;
	}
	return ret;
}

/**
 * initialize the test rte_pie config
 */
static enum test_result
test_rte_pie_init(struct test_config *tcfg)
{
	unsigned int i = 0;

	tcfg->tvar->clk_freq = rte_get_timer_hz();
	init_port_ts(tcfg->tvar->clk_freq);

	for (i = 0; i < tcfg->tconfig->num_cfg; i++) {
		if (rte_pie_config_init(&tcfg->tconfig->pconfig[i],
					(uint16_t)tcfg->tconfig->qdelay_ref,
					(uint16_t)tcfg->tconfig->dp_update_interval[i],
					(uint16_t)tcfg->tconfig->max_burst[i],
					(uint16_t)tcfg->tconfig->tailq_th) != 0) {
			return FAIL;
		}
	}

	*tcfg->tqueue->qlen = 0;
	*tcfg->tvar->dropped = 0;
	*tcfg->tvar->enqueued = 0;

	return PASS;
}

/**
 * enqueue until actual queue size reaches target level
 */
static int
increase_qsize(struct rte_pie_config *pie_cfg,
				struct rte_pie *pie,
				uint32_t *qlen,
				uint32_t pkt_len,
				uint32_t attempts)
{
	uint32_t i = 0;

		for (i = 0; i < attempts; i++) {
			int ret = 0;

			/**
			 * enqueue
			 */
			ret = rte_pie_enqueue(pie_cfg, pie, *qlen, pkt_len, get_port_ts());
			/**
			 * check if target actual queue size has been reached
			 */
			if (ret == 0)
				return 0;
		}
		/**
		 * no success
		 */
		return -1;
}

/**
 * functional test enqueue/dequeue packets
 */
static void
enqueue_dequeue_func(struct rte_pie_config *pie_cfg,
					struct rte_pie *pie,
					uint32_t *qlen,
					uint32_t num_ops,
					uint32_t *enqueued,
					uint32_t *dropped)
{
	uint32_t i = 0;

	for (i = 0; i < num_ops; i++) {
		int ret = 0;

		/**
		 * enqueue
		 */
		ret = rte_pie_enqueue(pie_cfg, pie, *qlen, sizeof(uint32_t),
							get_port_ts());
		if (ret == 0)
			(*enqueued)++;
		else
			(*dropped)++;
	}
}

/**
 * setup default values for the Functional test structures
 */
static struct rte_pie_config ft_wpconfig[1];
static struct rte_pie ft_rtdata[1];
static uint32_t  ft_q[] = {0};
static uint32_t  ft_dropped[] = {0};
static uint32_t  ft_enqueued[] = {0};
static uint16_t ft_max_burst[] = {64};
static uint16_t ft_dp_update_interval[] = {150};

static struct test_rte_pie_config ft_tconfig =  {
	.pconfig = ft_wpconfig,
	.num_cfg = RTE_DIM(ft_wpconfig),
	.qdelay_ref = 15,
	.dp_update_interval = ft_dp_update_interval,
	.max_burst = ft_max_burst,
	.tailq_th = 15,
};

static struct test_queue ft_tqueue = {
	.pdata_in = ft_rtdata,
	.num_queues = RTE_DIM(ft_rtdata),
	.qlen = ft_q,
	.q_ramp_up = 10,
	.drop_tolerance = 0,
};

static struct test_var ft_tvar = {
	.num_iterations = 0,
	.num_ops = 10000,
	.clk_freq = 0,
	.dropped = ft_dropped,
	.enqueued = ft_enqueued,
};

/**
 * Test F1: functional test 1
 */
static uint32_t ft_tlevels[] =  {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66,
				72, 78, 84, 90, 96, 102, 108, 114, 120, 126, 132, 138, 144};

static struct test_config func_test_config1 = {
	.ifname = "functional test interface",
	.msg = "functional test : use one pie configuration\n\n",
	.htxt = "                "
	"drop probability "
	"enqueued    "
	"dropped     "
	"drop prob % "
	"drop rate % "
	"diff %      "
	"tolerance % "
	"active  "
	"\n",
	.tconfig = &ft_tconfig,
	.tqueue = &ft_tqueue,
	.tvar = &ft_tvar,
	.tlevel = ft_tlevels,
};

static enum test_result func_test1(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	if (test_rte_pie_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	/**
	 * reset rte_pie run-time data
	 */
	rte_pie_rt_data_init(tcfg->tqueue->pdata_in);
	rte_pie_set_active(NULL, tcfg->tqueue->pdata_in, 1);
	*tcfg->tvar->enqueued = 0;
	*tcfg->tvar->dropped = 0;

	if (increase_qsize(&tcfg->tconfig->pconfig[i],
				tcfg->tqueue->pdata_in,
				tcfg->tqueue->qlen,
				tcfg->tlevel[i],
				tcfg->tqueue->q_ramp_up) != 0) {
		fprintf(stderr, "Fail: increase qsize\n");
		result = FAIL;
		goto out;
	}

	for (i = 0; i < RTE_DIM(ft_tlevels); i++) {
		const char *label = NULL;
		uint16_t prob = 0;
		uint16_t active = 0;
		double drop_rate = 1.0;
		double drop_prob = 0.0;
		double diff = 0.0;

		enqueue_dequeue_func(&tcfg->tconfig->pconfig[i],
				     tcfg->tqueue->pdata_in,
				     tcfg->tqueue->qlen,
				     tcfg->tvar->num_ops,
				     tcfg->tvar->enqueued,
				     tcfg->tvar->dropped);

		drop_rate = calc_drop_rate(*tcfg->tvar->enqueued,
							*tcfg->tvar->dropped);
		drop_prob = rte_pie_get_drop_prob(NULL, tcfg->tqueue->pdata_in);

		if (drop_prob != 0) {
			fprintf(stderr, "Fail: check drop prob\n");
			result = FAIL;
		}

		if (drop_rate != 0) {
			fprintf(stderr, "Fail: check drop rate\n");
			result = FAIL;
		}

		label = "Summary           ";
		active = rte_pie_get_active(NULL, tcfg->tqueue->pdata_in);
		printf("%s%-16u%-12u%-12u%-12.4lf%-12.4lf%-12.4lf%-12.4lf%-8i\n",
				label, prob, *tcfg->tvar->enqueued, *tcfg->tvar->dropped,
				drop_prob * 100.0, drop_rate * 100.0, diff,
				(double)tcfg->tqueue->drop_tolerance, active);
	}
out:
	return result;
}

/**
 * Test F2: functional test 2
 */
static uint32_t ft2_tlevel[] = {127};
static uint16_t ft2_max_burst[] = {1, 2, 8, 16, 32, 64, 128, 256, 512, 1024};
static uint16_t ft2_dp_update_interval[] = {
				10, 20, 50, 150, 300, 600, 900, 1200, 1500, 3000};
static struct rte_pie_config ft2_pconfig[10];

static struct test_rte_pie_config ft2_tconfig =  {
	.pconfig = ft2_pconfig,
	.num_cfg = RTE_DIM(ft2_pconfig),
	.qdelay_ref = 15,
	.dp_update_interval = ft2_dp_update_interval,
	.max_burst = ft2_max_burst,
	.tailq_th = 15,
};

static struct test_config func_test_config2 = {
	.ifname = "functional test 2 interface",
	.msg = "functional test 2 : use several PIE configurations,\n"
	"		    compare drop rate to drop probability\n\n",
	.htxt = "PIE config     "
	"avg queue size "
	"enqueued       "
	"dropped        "
	"drop prob %    "
	"drop rate %    "
	"diff %         "
	"tolerance %    "
	"\n",
	.tconfig = &ft2_tconfig,
	.tqueue = &ft_tqueue,
	.tvar = &ft_tvar,
	.tlevel = ft2_tlevel,
};

static enum test_result func_test2(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	printf("%s", tcfg->htxt);

	for (i = 0; i < tcfg->tconfig->num_cfg; i++) {
		uint32_t avg = 0;
		double drop_rate = 0.0;
		double drop_prob = 0.0;
		double diff = 0.0;

		if (test_rte_pie_init(tcfg) != PASS) {
			result = FAIL;
			goto out;
		}

		rte_pie_rt_data_init(tcfg->tqueue->pdata_in);
		rte_pie_set_active(NULL, tcfg->tqueue->pdata_in, 1);
		*tcfg->tvar->enqueued = 0;
		*tcfg->tvar->dropped = 0;

		if (increase_qsize(&tcfg->tconfig->pconfig[i],
					tcfg->tqueue->pdata_in,
					tcfg->tqueue->qlen,
					*tcfg->tlevel,
					tcfg->tqueue->q_ramp_up) != 0) {
			result = FAIL;
			goto out;
		}

		enqueue_dequeue_func(&tcfg->tconfig->pconfig[i],
				     tcfg->tqueue->pdata_in,
				     tcfg->tqueue->qlen,
				     tcfg->tvar->num_ops,
				     tcfg->tvar->enqueued,
				     tcfg->tvar->dropped);

		avg = rte_pie_get_avg_dq_time(NULL, tcfg->tqueue->pdata_in);

		drop_rate = calc_drop_rate(*tcfg->tvar->enqueued,
							*tcfg->tvar->dropped);
		drop_prob = rte_pie_get_drop_prob(NULL, tcfg->tqueue->pdata_in);

		if (!check_drop_rate(&diff, drop_rate, drop_prob,
				 (double)tcfg->tqueue->drop_tolerance)) {
			fprintf(stderr, "Fail: drop rate outside tolerance\n");
			result = FAIL;
		}

		printf("%-15u%-15u%-15u%-15u%-15.4lf%-15.4lf%-15.4lf%-15.4lf\n",
				i, avg, *tcfg->tvar->enqueued, *tcfg->tvar->dropped,
				drop_prob * 100.0, drop_rate * 100.0, diff,
				(double)tcfg->tqueue->drop_tolerance);
	}
out:
	return result;
}

static uint32_t ft3_qlen[] = {100};

static struct test_rte_pie_config ft3_tconfig =  {
	.pconfig = ft_wpconfig,
	.num_cfg = RTE_DIM(ft_wpconfig),
	.qdelay_ref = 15,
	.dp_update_interval = ft_dp_update_interval,
	.max_burst = ft_max_burst,
	.tailq_th = 15,
};

static struct test_queue ft3_tqueue = {
	.pdata_in = ft_rtdata,
	.num_queues = RTE_DIM(ft_rtdata),
	.qlen = ft3_qlen,
	.q_ramp_up = 10,
	.drop_tolerance = 0,
};

static struct test_var ft3_tvar = {
	.num_iterations = 0,
	.num_ops = 10000,
	.clk_freq = 0,
	.dropped = ft_dropped,
	.enqueued = ft_enqueued,
};

/**
 * Test F3: functional test 3
 */
static uint32_t ft3_tlevels[] =  {64, 127, 222};

static struct test_config func_test_config3 = {
	.ifname = "functional test interface",
	.msg = "functional test 2 : use one pie configuration\n"
			"using non zero qlen\n\n",
	.htxt = "                "
	"drop probability "
	"enqueued    "
	"dropped     "
	"drop prob % "
	"drop rate % "
	"diff %      "
	"tolerance % "
	"active  "
	"\n",
	.tconfig = &ft3_tconfig,
	.tqueue = &ft3_tqueue,
	.tvar = &ft3_tvar,
	.tlevel = ft3_tlevels,
};

static enum test_result func_test3(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	if (test_rte_pie_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	/**
	 * reset rte_pie run-time data
	 */
	rte_pie_rt_data_init(tcfg->tqueue->pdata_in);
	rte_pie_set_active(NULL, tcfg->tqueue->pdata_in, 1);
	*tcfg->tvar->enqueued = 0;
	*tcfg->tvar->dropped = 0;

	if (increase_qsize(&tcfg->tconfig->pconfig[i],
				tcfg->tqueue->pdata_in,
				tcfg->tqueue->qlen,
				tcfg->tlevel[i],
				tcfg->tqueue->q_ramp_up) != 0) {
		fprintf(stderr, "Fail: increase qsize\n");
		result = FAIL;
		goto out;
	}

	for (i = 0; i < RTE_DIM(ft_tlevels); i++) {
		const char *label = NULL;
		uint16_t prob = 0;
		uint16_t active = 0;
		double drop_rate = 1.0;
		double drop_prob = 0.0;
		double diff = 0.0;

		enqueue_dequeue_func(&tcfg->tconfig->pconfig[i],
				     tcfg->tqueue->pdata_in,
				     tcfg->tqueue->qlen,
				     tcfg->tvar->num_ops,
				     tcfg->tvar->enqueued,
				     tcfg->tvar->dropped);

		drop_rate = calc_drop_rate(*tcfg->tvar->enqueued,
						*tcfg->tvar->dropped);
		drop_prob = rte_pie_get_drop_prob(NULL, tcfg->tqueue->pdata_in);

		if (drop_prob != 0) {
			fprintf(stderr, "Fail: check drop prob\n");
			result = FAIL;
		}

		if (drop_rate != 0) {
			fprintf(stderr, "Fail: check drop rate\n");
			result = FAIL;
		}

		label = "Summary           ";
		active = rte_pie_get_active(NULL, tcfg->tqueue->pdata_in);
		printf("%s%-16u%-12u%-12u%-12.4lf%-12.4lf%-12.4lf%-12.4lf%-8i\n",
				label, prob, *tcfg->tvar->enqueued, *tcfg->tvar->dropped,
				drop_prob * 100.0, drop_rate * 100.0, diff,
				(double)tcfg->tqueue->drop_tolerance, active);
	}
out:
	return result;
}

/**
 * setup default values for the Performance test structures
 */
static struct rte_pie_config pt_wrconfig[1];
static struct rte_pie pt_rtdata[1];
static struct rte_pie pt_wtdata[1];
static uint32_t pt_q[] = {0};
static uint32_t pt_dropped[] = {0};
static uint32_t pt_enqueued[] = {0};
static uint32_t pt_dequeued[] = {0};
static uint16_t pt_max_burst[] = {64};
static uint16_t pt_dp_update_interval[] = {150};

static struct test_rte_pie_config pt_tconfig =  {
	.pconfig = pt_wrconfig,
	.num_cfg = RTE_DIM(pt_wrconfig),
	.qdelay_ref = 15,
	.dp_update_interval = pt_dp_update_interval,
	.max_burst = pt_max_burst,
	.tailq_th = 150,
};

static struct test_queue pt_tqueue = {
	.pdata_in = pt_rtdata,
	.num_queues = RTE_DIM(pt_rtdata),
	.qlen = pt_q,
	.q_ramp_up = 1000000,
	.drop_tolerance = 0,  /* 0 percent */
};

static struct test_rte_pie_config pt_tconfig2 =  {
	.pconfig = pt_wrconfig,
	.num_cfg = RTE_DIM(pt_wrconfig),
	.qdelay_ref = 15,
	.dp_update_interval = pt_dp_update_interval,
	.max_burst = pt_max_burst,
	.tailq_th = 150,
};

static struct test_queue pt_tqueue2 = {
	.pdata_in = pt_rtdata,
	.pdata_out = pt_wtdata,
	.num_queues = RTE_DIM(pt_rtdata),
	.qlen = pt_q,
	.q_ramp_up = 1000000,
	.drop_tolerance = 0,  /* 0 percent */
};

/**
 * enqueue/dequeue packets
 * aka
 *  rte_sched_port_enqueue(port, in_mbufs, 10);
 *	rte_sched_port_dequeue(port, out_mbufs, 10);
 */
static void enqueue_dequeue_perf(struct rte_pie_config *pie_cfg,
				 struct rte_pie *pie_in,
				 struct rte_pie *pie_out,
				 uint32_t *qlen,
				 uint32_t num_ops,
				 uint32_t *enqueued,
				 uint32_t *dropped,
				 uint32_t *dequeued,
				 struct rdtsc_prof *prof)
{
	uint32_t i = 0;

	if (pie_cfg == NULL) {
		printf("%s: Error: PIE configuration cannot be empty.\n", __func__);
		return;
	}

	if (pie_in == NULL) {
		printf("%s: Error: PIE enqueue data cannot be empty.\n", __func__);
		return;
	}

	for (i = 0; i < num_ops; i++) {
		uint64_t ts = 0;
		int ret = 0;

		/**
		 * enqueue
		 */
		ts = get_port_ts();
		rdtsc_prof_start(prof);
		ret = rte_pie_enqueue(pie_cfg, pie_in, *qlen,
								1000*sizeof(uint32_t), ts);
		rdtsc_prof_end(prof);

		if (ret == 0)
			(*enqueued)++;
		else
			(*dropped)++;

		if (pie_out != NULL) {
			ts = get_port_ts();
			rdtsc_prof_start(prof);
			rte_pie_dequeue(pie_out, 1000*sizeof(uint32_t), ts);
			rdtsc_prof_end(prof);

			(*dequeued)++;
		}
	}
}

/**
 * Setup test structures for tests P1
 * performance tests 1
 */
static uint32_t pt1_tlevel[] = {80};

static struct test_var perf1_tvar = {
	.num_iterations = 0,
	.num_ops = 30000,
	.clk_freq = 0,
	.dropped = pt_dropped,
	.enqueued = pt_enqueued
};

static struct test_config perf_test_config = {
	.ifname = "performance test 1 interface",
	.msg = "performance test 1 : use one PIE configuration,\n"
	"		     measure enqueue performance\n\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf1_tvar,
	.tlevel = pt1_tlevel,
};

/**
 * Performance test function to measure enqueue performance.
 *
 */
static enum test_result perf_test(struct test_config *tcfg)
{
	enum test_result result = PASS;
	struct rdtsc_prof prof = {0, 0, 0, 0, 0.0, NULL};
	uint32_t total = 0;

	printf("%s", tcfg->msg);

	rdtsc_prof_init(&prof, "enqueue");

	if (test_rte_pie_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	/**
	 * initialize the rte_pie run time data structure
	 */
	rte_pie_rt_data_init(tcfg->tqueue->pdata_in);
	rte_pie_set_active(NULL, tcfg->tqueue->pdata_in, 1);
	*tcfg->tvar->enqueued = 0;
	*tcfg->tvar->dropped = 0;

	enqueue_dequeue_perf(tcfg->tconfig->pconfig,
			     tcfg->tqueue->pdata_in,
				 NULL,
			     tcfg->tqueue->qlen,
			     tcfg->tvar->num_ops,
			     tcfg->tvar->enqueued,
			     tcfg->tvar->dropped,
				 tcfg->tvar->dequeued,
			     &prof);

	total = *tcfg->tvar->enqueued + *tcfg->tvar->dropped;

	printf("\ntotal: %u, enqueued: %u (%.2lf%%), dropped: %u (%.2lf%%)\n",
			total, *tcfg->tvar->enqueued,
			((double)(*tcfg->tvar->enqueued) / (double)total) * 100.0,
			*tcfg->tvar->dropped,
			((double)(*tcfg->tvar->dropped) / (double)total) * 100.0);

	rdtsc_prof_print(&prof);
out:
	return result;
}



/**
 * Setup test structures for tests P2
 * performance tests 2
 */
static uint32_t pt2_tlevel[] = {80};

static struct test_var perf2_tvar = {
	.num_iterations = 0,
	.num_ops = 30000,
	.clk_freq = 0,
	.dropped = pt_dropped,
	.enqueued = pt_enqueued,
	.dequeued = pt_dequeued
};

static struct test_config perf_test_config2 = {
	.ifname = "performance test 2 interface",
	.msg = "performance test 2 : use one PIE configuration,\n"
	"		     measure enqueue & dequeue performance\n\n",
	.tconfig = &pt_tconfig2,
	.tqueue = &pt_tqueue2,
	.tvar = &perf2_tvar,
	.tlevel = pt2_tlevel,
};

/**
 * Performance test function to measure enqueue & dequeue performance.
 *
 */
static enum test_result perf_test2(struct test_config *tcfg)
{
	enum test_result result = PASS;
	struct rdtsc_prof prof = {0, 0, 0, 0, 0.0, NULL};
	uint32_t total = 0;

	printf("%s", tcfg->msg);

	rdtsc_prof_init(&prof, "enqueue");

	if (test_rte_pie_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	/**
	 * initialize the rte_pie run time data structure
	 */
	rte_pie_rt_data_init(tcfg->tqueue->pdata_in);
	rte_pie_set_active(NULL, tcfg->tqueue->pdata_in, 1);
	*tcfg->tvar->enqueued = 0;
	*tcfg->tvar->dequeued = 0;
	*tcfg->tvar->dropped = 0;

	enqueue_dequeue_perf(tcfg->tconfig->pconfig,
				 tcfg->tqueue->pdata_in,
				 tcfg->tqueue->pdata_out,
				 tcfg->tqueue->qlen,
				 tcfg->tvar->num_ops,
				 tcfg->tvar->enqueued,
				 tcfg->tvar->dropped,
				 tcfg->tvar->dequeued,
				 &prof);

	total = *tcfg->tvar->enqueued + *tcfg->tvar->dropped;

	printf("\ntotal: %u, dequeued: %u (%.2lf%%), dropped: %u (%.2lf%%)\n",
			total, *tcfg->tvar->dequeued,
			((double)(*tcfg->tvar->dequeued) / (double)total) * 100.0,
			*tcfg->tvar->dropped,
			((double)(*tcfg->tvar->dropped) / (double)total) * 100.0);

	rdtsc_prof_print(&prof);
out:
	return result;
}

/**
 * define the functional tests to be executed fast
 */
struct tests func_pie_tests_quick[] = {
	{ &func_test_config1, func_test1 },
	{ &func_test_config2, func_test2 },
};

/**
 * define the functional and performance tests to be executed
 */
struct tests func_pie_tests[] = {
	{ &func_test_config1, func_test1 },
	{ &func_test_config2, func_test2 },
	{ &func_test_config3, func_test3 },
};

struct tests perf_pie_tests[] = {
	{ &perf_test_config, perf_test },
	{ &perf_test_config2, perf_test2 },
};

/**
 * function to execute the required pie tests
 */
static void run_tests(struct tests *test_type, uint32_t test_count,
						uint32_t *num_tests, uint32_t *num_pass)
{
	enum test_result result = PASS;
	uint32_t i = 0;
	static const char *bar_str = "-------------------------------------"
						"-------------------------------------------";
	static const char *bar_pass_str = "-------------------------------------"
						"<pass>-------------------------------------";
	static const char *bar_fail_str = "-------------------------------------"
						"<fail>-------------------------------------";

	for (i = 0; i < test_count; i++) {
		printf("\n%s\n", bar_str);
		result = test_type[i].testfn(test_type[i].testcfg);
		(*num_tests)++;
		if (result == PASS) {
			(*num_pass)++;
				printf("%s\n", bar_pass_str);
		} else {
			printf("%s\n", bar_fail_str);
		}
	}
}

/**
 * check if functions accept invalid parameters
 *
 * First, all functions will be called without initialized PIE
 * Then, all of them will be called with NULL/invalid parameters
 *
 * Some functions are not tested as they are performance-critical and thus
 * don't do any parameter checking.
 */
static int
test_invalid_parameters(void)
{
	struct rte_pie_config config;
	static const char *shf_str = "rte_pie_config_init should have failed!";
	static const char *shf_rt_str = "rte_pie_rt_data_init should have failed!";

	/* NULL config */
	if (rte_pie_rt_data_init(NULL) == 0) {
		printf("%i: %s\n", __LINE__, shf_rt_str);
		return -1;
	}

	/* NULL config */
	if (rte_pie_config_init(NULL, 0, 0, 0, 0) == 0) {
		printf("%i%s\n", __LINE__, shf_str);
		return -1;
	}

	/* qdelay_ref <= 0 */
	if (rte_pie_config_init(&config, 0, 1, 1, 1) == 0) {
		printf("%i%s\n", __LINE__, shf_str);
		return -1;
	}

	/* dp_update_interval <= 0 */
	if (rte_pie_config_init(&config, 1, 0, 1, 1) == 0) {
		printf("%i%s\n", __LINE__, shf_str);
		return -1;
	}

	/* max_burst <= 0 */
	if (rte_pie_config_init(&config, 1, 1, 0, 1) == 0) {
		printf("%i%s\n", __LINE__, shf_str);
		return -1;
	}

	/* tailq_th <= 0 */
	if (rte_pie_config_init(&config, 1, 1, 1, 0) == 0) {
		printf("%i%s\n", __LINE__, shf_str);
		return -1;
	}

	RTE_SET_USED(config);

	return 0;
}

static void
show_stats(const uint32_t num_tests, const uint32_t num_pass)
{
	if (num_pass == num_tests)
		printf("[total: %u, pass: %u]\n", num_tests, num_pass);
	else
		printf("[total: %u, pass: %u, fail: %u]\n", num_tests, num_pass,
		       num_tests - num_pass);
}

static int
tell_the_result(const uint32_t num_tests, const uint32_t num_pass)
{
	return (num_pass == num_tests) ? 0 : 1;
}

static int
test_pie(void)
{
	uint32_t num_tests = 0;
	uint32_t num_pass = 0;

	if (test_invalid_parameters() < 0)
		return -1;

	run_tests(func_pie_tests_quick, RTE_DIM(func_pie_tests_quick),
		  &num_tests, &num_pass);
	show_stats(num_tests, num_pass);
	return tell_the_result(num_tests, num_pass);
}

static int
test_pie_perf(void)
{
	uint32_t num_tests = 0;
	uint32_t num_pass = 0;

	run_tests(perf_pie_tests, RTE_DIM(perf_pie_tests), &num_tests, &num_pass);
	show_stats(num_tests, num_pass);
	return tell_the_result(num_tests, num_pass);
}

static int
test_pie_all(void)
{
	uint32_t num_tests = 0;
	uint32_t num_pass = 0;

	if (test_invalid_parameters() < 0)
		return -1;

	run_tests(func_pie_tests, RTE_DIM(func_pie_tests), &num_tests, &num_pass);
	run_tests(perf_pie_tests, RTE_DIM(perf_pie_tests), &num_tests, &num_pass);
	show_stats(num_tests, num_pass);
	return tell_the_result(num_tests, num_pass);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_FAST_TEST(pie_autotest, true, true, test_pie);
REGISTER_PERF_TEST(pie_perf, test_pie_perf);
REGISTER_PERF_TEST(pie_all, test_pie_all);
