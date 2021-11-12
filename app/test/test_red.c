/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "test.h"

#include <rte_red.h>

#ifdef __INTEL_COMPILER
#pragma warning(disable:2259)       /* conversion may lose significant bits */
#pragma warning(disable:181)        /* Arg incompatible with format string */
#endif

#define TEST_HZ_PER_KHZ 1000
#define TEST_NSEC_MARGIN 500        /**< nanosecond margin when calculating clk freq */

#define MAX_QEMPTY_TIME_MSEC   50000
#define MSEC_PER_SEC           1000      /**< Milli-seconds per second */
#define USEC_PER_MSEC          1000      /**< Micro-seconds per milli-second */
#define USEC_PER_SEC           1000000   /**< Micro-seconds per second */
#define NSEC_PER_SEC           (USEC_PER_SEC * 1000) /**< Nano-seconds per second */

/**< structures for testing rte_red performance and function */
struct test_rte_red_config {        /**< Test structure for RTE_RED config */
	struct rte_red_config *rconfig; /**< RTE_RED configuration parameters */
	uint8_t num_cfg;                /**< Number of RTE_RED configs to test */
	uint8_t *wq_log2;               /**< Test wq_log2 value to use */
	uint32_t min_th;                /**< Queue minimum threshold */
	uint32_t max_th;                /**< Queue maximum threshold */
	uint8_t *maxp_inv;              /**< Inverse mark probability */
};

struct test_queue {                 /**< Test structure for RTE_RED Queues */
	struct rte_red *rdata;          /**< RTE_RED runtime data */
	uint32_t num_queues;            /**< Number of RTE_RED queues to test */
	uint32_t *qconfig;              /**< Configuration of RTE_RED queues for test */
	uint32_t *q;                    /**< Queue size */
	uint32_t q_ramp_up;             /**< Num of enqueues to ramp up the queue */
	uint32_t avg_ramp_up;           /**< Average num of enqueues to ramp up the queue */
	uint32_t avg_tolerance;         /**< Tolerance in queue average */
	double drop_tolerance;          /**< Drop tolerance of packets not enqueued */
};

struct test_var {                   /**< Test variables used for testing RTE_RED */
	uint32_t wait_usec;             /**< Micro second wait interval */
	uint32_t num_iterations;        /**< Number of test iterations */
	uint32_t num_ops;               /**< Number of test operations */
	uint64_t clk_freq;              /**< CPU clock frequency */
	uint32_t sleep_sec;             /**< Seconds to sleep */
	uint32_t *dropped;              /**< Test operations dropped */
	uint32_t *enqueued;             /**< Test operations enqueued */
};

struct test_config {                /**< Master test structure for RTE_RED */
	const char *ifname;             /**< Interface name */
	const char *msg;                /**< Test message for display */
	const char *htxt;               /**< Header txt display for result output */
	struct test_rte_red_config *tconfig; /**< Test structure for RTE_RED config */
	struct test_queue *tqueue;      /**< Test structure for RTE_RED Queues */
	struct test_var *tvar;          /**< Test variables used for testing RTE_RED */
	uint32_t *tlevel;               /**< Queue levels */
};

enum test_result {
	FAIL = 0,
	PASS
};

/**< Test structure to define tests to run */
struct tests {
	struct test_config *testcfg;
	enum test_result (*testfn)(struct test_config *);
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
static double inv_cycles_per_byte = 0;
static double pkt_time_usec = 0;

static void init_port_ts(uint64_t cpu_clock)
{
	double cycles_per_byte = (double)(cpu_clock) / (double)(port_speed_bytes);
	inv_cycles_per_byte = 1.0 / cycles_per_byte;
	pkt_time_usec = 1000000.0 / ((double)port_speed_bytes / (double)RTE_RED_S);
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
	if (p->clk_avgc>0) {
		printf("RDTSC stats for %s: n=%" PRIu64 ", min=%" PRIu64 ", max=%" PRIu64 ", avg=%.1f\n",
			p->name,
			p->clk_avgc,
			p->clk_min,
			p->clk_max,
			(p->clk_avg / ((double) p->clk_avgc)));
	}
}

static uint32_t rte_red_get_avg_int(const struct rte_red_config *red_cfg,
				    struct rte_red *red)
{
	/**
	 * scale by 1/n and convert from fixed-point to integer
	 */
	return red->avg >> (RTE_RED_SCALING + red_cfg->wq_log2);
}

static double rte_red_get_avg_float(const struct rte_red_config *red_cfg,
				    struct rte_red *red)
{
	/**
	 * scale by 1/n and convert from fixed-point to floating-point
	 */
	return ldexp((double)red->avg,  -(RTE_RED_SCALING + red_cfg->wq_log2));
}

static void rte_red_set_avg_int(const struct rte_red_config *red_cfg,
				struct rte_red *red,
				uint32_t avg)
{
	/**
	 * scale by n and convert from integer to fixed-point
	 */
	red->avg = avg << (RTE_RED_SCALING + red_cfg->wq_log2);
}

static double calc_exp_avg_on_empty(double avg, uint32_t n, uint32_t time_diff)
{
	return avg * pow((1.0 - 1.0 / (double)n), (double)time_diff / pkt_time_usec);
}

static double calc_drop_rate(uint32_t enqueued, uint32_t dropped)
{
	return (double)dropped / ((double)enqueued + (double)dropped);
}

/**
 * calculate the drop probability
 */
static double calc_drop_prob(uint32_t min_th, uint32_t max_th,
			     uint32_t maxp_inv, uint32_t avg)
{
	double drop_prob = 0.0;

	if (avg < min_th) {
		drop_prob = 0.0;
	} else if (avg < max_th) {
		drop_prob = (1.0 / (double)maxp_inv)
			* ((double)(avg - min_th)
			   / (double)(max_th - min_th));
	} else {
		drop_prob = 1.0;
	}
	return drop_prob;
}

/**
 *  check if drop rate matches drop probability within tolerance
 */
static int check_drop_rate(double *diff, double drop_rate, double drop_prob, double tolerance)
{
	double abs_diff = 0.0;
	int ret = 1;

	abs_diff = fabs(drop_rate - drop_prob);
	if ((int)abs_diff == 0) {
	        *diff = 0.0;
	} else {
	        *diff = (abs_diff / drop_prob) * 100.0;
	        if (*diff > tolerance) {
	                ret = 0;
	        }
        }
	return ret;
}

/**
 *  check if average queue size is within tolerance
 */
static int check_avg(double *diff, double avg, double exp_avg, double tolerance)
{
	double abs_diff = 0.0;
	int ret = 1;

	abs_diff = fabs(avg - exp_avg);
	if ((int)abs_diff == 0) {
	        *diff = 0.0;
	} else {
	        *diff = (abs_diff / exp_avg) * 100.0;
	        if (*diff > tolerance) {
	                ret = 0;
                }
	}
	return ret;
}

/**
 * initialize the test rte_red config
 */
static enum test_result
test_rte_red_init(struct test_config *tcfg)
{
	unsigned i = 0;

	tcfg->tvar->clk_freq = rte_get_timer_hz();
	init_port_ts( tcfg->tvar->clk_freq );

	for (i = 0; i < tcfg->tconfig->num_cfg; i++) {
		if (rte_red_config_init(&tcfg->tconfig->rconfig[i],
					(uint16_t)tcfg->tconfig->wq_log2[i],
					(uint16_t)tcfg->tconfig->min_th,
					(uint16_t)tcfg->tconfig->max_th,
					(uint16_t)tcfg->tconfig->maxp_inv[i]) != 0) {
			return FAIL;
		}
	}

	*tcfg->tqueue->q = 0;
	*tcfg->tvar->dropped = 0;
	*tcfg->tvar->enqueued = 0;
	return PASS;
}

/**
 * enqueue until actual queue size reaches target level
 */
static int
increase_actual_qsize(struct rte_red_config *red_cfg,
                      struct rte_red *red,
                      uint32_t *q,
                      uint32_t level,
                      uint32_t attempts)
{
        uint32_t i = 0;

        for (i = 0; i < attempts; i++) {
                int ret = 0;

                /**
                 * enqueue
                 */
                ret = rte_red_enqueue(red_cfg, red, *q, get_port_ts() );
                if (ret == 0) {
                        if (++(*q) >= level)
                                break;
                }
        }
        /**
        * check if target actual queue size has been reached
        */
        if (*q != level)
                return -1;
        /**
         * success
         */
        return 0;
}

/**
 * enqueue until average queue size reaches target level
 */
static int
increase_average_qsize(struct rte_red_config *red_cfg,
                       struct rte_red *red,
                       uint32_t *q,
                       uint32_t level,
                       uint32_t num_ops)
{
        uint32_t avg = 0;
        uint32_t i = 0;

        for (i = 0; i < num_ops; i++) {
                /**
                 * enqueue
                 */
                rte_red_enqueue(red_cfg, red, *q, get_port_ts());
        }
        /**
         * check if target average queue size has been reached
         */
        avg = rte_red_get_avg_int(red_cfg, red);
        if (avg != level)
                return -1;
        /**
         * success
         */
        return 0;
}

/**
 * setup default values for the functional test structures
 */
static struct rte_red_config ft_wrconfig[1];
static struct rte_red ft_rtdata[1];
static uint8_t ft_wq_log2[] = {9};
static uint8_t ft_maxp_inv[] = {10};
static uint32_t  ft_qconfig[] = {0, 0, 1, 1};
static uint32_t  ft_q[] ={0};
static uint32_t  ft_dropped[] ={0};
static uint32_t  ft_enqueued[] ={0};

static struct test_rte_red_config ft_tconfig =  {
	.rconfig = ft_wrconfig,
	.num_cfg = RTE_DIM(ft_wrconfig),
	.wq_log2 = ft_wq_log2,
	.min_th = 32,
	.max_th = 128,
	.maxp_inv = ft_maxp_inv,
};

static struct test_queue ft_tqueue = {
	.rdata = ft_rtdata,
	.num_queues = RTE_DIM(ft_rtdata),
	.qconfig = ft_qconfig,
	.q = ft_q,
	.q_ramp_up = 1000000,
	.avg_ramp_up = 1000000,
	.avg_tolerance = 5,  /* 5 percent */
	.drop_tolerance = 50,  /* 50 percent */
};

static struct test_var ft_tvar = {
	.wait_usec = 10000,
	.num_iterations = 5,
	.num_ops = 10000,
	.clk_freq = 0,
	.dropped = ft_dropped,
	.enqueued = ft_enqueued,
	.sleep_sec = (MAX_QEMPTY_TIME_MSEC / MSEC_PER_SEC) + 2,
};

/**
 * functional test enqueue/dequeue packets
 */
static void enqueue_dequeue_func(struct rte_red_config *red_cfg,
                                 struct rte_red *red,
                                 uint32_t *q,
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
                ret = rte_red_enqueue(red_cfg, red, *q, get_port_ts());
                if (ret == 0)
                        (*enqueued)++;
                else
                        (*dropped)++;
        }
}

/**
 * Test F1: functional test 1
 */
static uint32_t ft1_tlevels[] =  {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90, 96, 102, 108, 114, 120, 126, 132, 138, 144};

static struct test_config func_test1_config = {
	.ifname = "functional test 1 interface",
	.msg = "functional test 1 : use one rte_red configuration,\n"
	"		    increase average queue size to various levels,\n"
	"		    compare drop rate to drop probability\n\n",
	.htxt = "                "
	"avg queue size "
	"enqueued       "
	"dropped        "
	"drop prob %    "
	"drop rate %    "
	"diff %         "
	"tolerance %    "
	"\n",
	.tconfig = &ft_tconfig,
	.tqueue = &ft_tqueue,
	.tvar = &ft_tvar,
	.tlevel = ft1_tlevels,
};

static enum test_result func_test1(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	for (i = 0; i < RTE_DIM(ft1_tlevels); i++) {
		const char *label = NULL;
		uint32_t avg = 0;
		double drop_rate = 0.0;
		double drop_prob = 0.0;
		double diff = 0.0;

		/**
		 * reset rte_red run-time data
		 */
		rte_red_rt_data_init(tcfg->tqueue->rdata);
		*tcfg->tvar->enqueued = 0;
		*tcfg->tvar->dropped = 0;

		if (increase_actual_qsize(tcfg->tconfig->rconfig,
					  tcfg->tqueue->rdata,
					  tcfg->tqueue->q,
					  tcfg->tlevel[i],
					  tcfg->tqueue->q_ramp_up) != 0) {
			result = FAIL;
			goto out;
		}

		if (increase_average_qsize(tcfg->tconfig->rconfig,
					   tcfg->tqueue->rdata,
					   tcfg->tqueue->q,
					   tcfg->tlevel[i],
					   tcfg->tqueue->avg_ramp_up) != 0)  {
			result = FAIL;
			goto out;
		}

		enqueue_dequeue_func(tcfg->tconfig->rconfig,
				     tcfg->tqueue->rdata,
				     tcfg->tqueue->q,
				     tcfg->tvar->num_ops,
				     tcfg->tvar->enqueued,
				     tcfg->tvar->dropped);

		avg = rte_red_get_avg_int(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);
		if (avg != tcfg->tlevel[i]) {
                        fprintf(stderr, "Fail: avg != level\n");
			result = FAIL;
                }

		drop_rate = calc_drop_rate(*tcfg->tvar->enqueued, *tcfg->tvar->dropped);
		drop_prob = calc_drop_prob(tcfg->tconfig->min_th, tcfg->tconfig->max_th,
					   *tcfg->tconfig->maxp_inv, tcfg->tlevel[i]);
		if (!check_drop_rate(&diff, drop_rate, drop_prob, (double)tcfg->tqueue->drop_tolerance))
		        result = FAIL;

		if (tcfg->tlevel[i] == tcfg->tconfig->min_th)
			label = "min thresh:     ";
		else if (tcfg->tlevel[i] == tcfg->tconfig->max_th)
			label = "max thresh:     ";
		else
			label = "                ";
		printf("%s%-15u%-15u%-15u%-15.4lf%-15.4lf%-15.4lf%-15.4lf\n",
		       label, avg, *tcfg->tvar->enqueued, *tcfg->tvar->dropped,
		       drop_prob * 100.0, drop_rate * 100.0, diff,
	               (double)tcfg->tqueue->drop_tolerance);
	}
out:
	return result;
}

/**
 * Test F2: functional test 2
 */
static uint32_t ft2_tlevel[] = {127};
static uint8_t ft2_wq_log2[] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
static uint8_t ft2_maxp_inv[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static struct rte_red_config ft2_rconfig[10];

static struct test_rte_red_config ft2_tconfig =  {
	.rconfig = ft2_rconfig,
	.num_cfg = RTE_DIM(ft2_rconfig),
	.wq_log2 = ft2_wq_log2,
	.min_th = 32,
	.max_th = 128,
	.maxp_inv = ft2_maxp_inv,
};

static struct test_config func_test2_config = {
	.ifname = "functional test 2 interface",
	.msg = "functional test 2 : use several RED configurations,\n"
	"		    increase average queue size to just below maximum threshold,\n"
	"		    compare drop rate to drop probability\n\n",
	.htxt = "RED config     "
	"avg queue size "
	"min threshold  "
	"max threshold  "
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
        double prev_drop_rate = 1.0;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}
	rte_red_rt_data_init(tcfg->tqueue->rdata);

	if (increase_actual_qsize(tcfg->tconfig->rconfig,
				  tcfg->tqueue->rdata,
				  tcfg->tqueue->q,
				  *tcfg->tlevel,
				  tcfg->tqueue->q_ramp_up) != 0) {
		result = FAIL;
		goto out;
	}

	if (increase_average_qsize(tcfg->tconfig->rconfig,
				   tcfg->tqueue->rdata,
				   tcfg->tqueue->q,
				   *tcfg->tlevel,
				   tcfg->tqueue->avg_ramp_up) != 0) {
		result = FAIL;
		goto out;
	}
	printf("%s", tcfg->htxt);

	for (i = 0; i < tcfg->tconfig->num_cfg; i++) {
		uint32_t avg = 0;
		double drop_rate = 0.0;
		double drop_prob = 0.0;
		double diff = 0.0;

		*tcfg->tvar->dropped = 0;
		*tcfg->tvar->enqueued = 0;

		enqueue_dequeue_func(&tcfg->tconfig->rconfig[i],
				     tcfg->tqueue->rdata,
				     tcfg->tqueue->q,
				     tcfg->tvar->num_ops,
				     tcfg->tvar->enqueued,
				     tcfg->tvar->dropped);

		avg = rte_red_get_avg_int(&tcfg->tconfig->rconfig[i], tcfg->tqueue->rdata);
		if (avg != *tcfg->tlevel)
			result = FAIL;

		drop_rate = calc_drop_rate(*tcfg->tvar->enqueued, *tcfg->tvar->dropped);
		drop_prob = calc_drop_prob(tcfg->tconfig->min_th, tcfg->tconfig->max_th,
					   tcfg->tconfig->maxp_inv[i], *tcfg->tlevel);
		if (!check_drop_rate(&diff, drop_rate, drop_prob, (double)tcfg->tqueue->drop_tolerance))
		        result = FAIL;
	        /**
	         * drop rate should decrease as maxp_inv increases
	         */
	        if (drop_rate > prev_drop_rate)
	                result = FAIL;
	        prev_drop_rate = drop_rate;

		printf("%-15u%-15u%-15u%-15u%-15.4lf%-15.4lf%-15.4lf%-15.4lf\n",
		       i, avg, tcfg->tconfig->min_th, tcfg->tconfig->max_th,
		       drop_prob * 100.0, drop_rate * 100.0, diff,
	               (double)tcfg->tqueue->drop_tolerance);
	}
out:
	return result;
}

/**
 * Test F3: functional test 3
 */
static uint32_t ft3_tlevel[] = {1022};

static struct test_rte_red_config ft3_tconfig =  {
	.rconfig = ft_wrconfig,
	.num_cfg = RTE_DIM(ft_wrconfig),
	.wq_log2 = ft_wq_log2,
	.min_th = 32,
	.max_th = 1023,
	.maxp_inv = ft_maxp_inv,
};

static struct test_config func_test3_config = {
	.ifname = "functional test 3 interface",
	.msg = "functional test 3 : use one RED configuration,\n"
	"		    increase average queue size to target level,\n"
	"		    dequeue all packets until queue is empty,\n"
	"		    confirm that average queue size is computed correctly while queue is empty\n\n",
	.htxt = "q avg before   "
	"q avg after    "
	"expected       "
	"difference %   "
	"tolerance %    "
	"result	 "
	"\n",
	.tconfig = &ft3_tconfig,
	.tqueue = &ft_tqueue,
	.tvar = &ft_tvar,
	.tlevel = ft3_tlevel,
};

static enum test_result func_test3(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	rte_red_rt_data_init(tcfg->tqueue->rdata);

	if (increase_actual_qsize(tcfg->tconfig->rconfig,
				  tcfg->tqueue->rdata,
				  tcfg->tqueue->q,
				  *tcfg->tlevel,
				  tcfg->tqueue->q_ramp_up) != 0) {
		result = FAIL;
		goto out;
	}

	if (increase_average_qsize(tcfg->tconfig->rconfig,
				   tcfg->tqueue->rdata,
				   tcfg->tqueue->q,
				   *tcfg->tlevel,
				   tcfg->tqueue->avg_ramp_up) != 0) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	for (i = 0; i < tcfg->tvar->num_iterations; i++) {
		double avg_before = 0;
		double avg_after = 0;
                double exp_avg = 0;
		double diff = 0.0;

		avg_before = rte_red_get_avg_float(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);

		/**
		* empty the queue
		*/
		*tcfg->tqueue->q = 0;
		rte_red_mark_queue_empty(tcfg->tqueue->rdata, get_port_ts());

		rte_delay_us(tcfg->tvar->wait_usec);

		/**
		 * enqueue one packet to recalculate average queue size
		 */
		if (rte_red_enqueue(tcfg->tconfig->rconfig,
				    tcfg->tqueue->rdata,
				    *tcfg->tqueue->q,
				    get_port_ts()) == 0) {
			(*tcfg->tqueue->q)++;
		} else {
			printf("%s:%d: packet enqueued on empty queue was dropped\n", __func__, __LINE__);
			result = FAIL;
		}

		exp_avg = calc_exp_avg_on_empty(avg_before,
					      (1 << *tcfg->tconfig->wq_log2),
					      tcfg->tvar->wait_usec);
		avg_after = rte_red_get_avg_float(tcfg->tconfig->rconfig,
						  tcfg->tqueue->rdata);
		if (!check_avg(&diff, avg_after, exp_avg, (double)tcfg->tqueue->avg_tolerance))
		        result = FAIL;

		printf("%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15s\n",
		       avg_before, avg_after, exp_avg, diff,
		       (double)tcfg->tqueue->avg_tolerance,
		       diff <= (double)tcfg->tqueue->avg_tolerance ? "pass" : "fail");
	}
out:
	return result;
}

/**
 * Test F4: functional test 4
 */
static uint32_t ft4_tlevel[] = {1022};
static uint8_t ft4_wq_log2[] = {11};

static struct test_rte_red_config ft4_tconfig =  {
	.rconfig = ft_wrconfig,
	.num_cfg = RTE_DIM(ft_wrconfig),
	.min_th = 32,
	.max_th = 1023,
	.wq_log2 = ft4_wq_log2,
	.maxp_inv = ft_maxp_inv,
};

static struct test_queue ft4_tqueue = {
	.rdata = ft_rtdata,
	.num_queues = RTE_DIM(ft_rtdata),
	.qconfig = ft_qconfig,
	.q = ft_q,
	.q_ramp_up = 1000000,
	.avg_ramp_up = 1000000,
	.avg_tolerance = 0,  /* 0 percent */
	.drop_tolerance = 50,  /* 50 percent */
};

static struct test_config func_test4_config = {
	.ifname = "functional test 4 interface",
	.msg = "functional test 4 : use one RED configuration,\n"
	"		    increase average queue size to target level,\n"
	"		    dequeue all packets until queue is empty,\n"
	"		    confirm that average queue size is computed correctly while\n"
	"		    queue is empty for more than 50 sec,\n"
	"		    (this test takes 52 sec to run)\n\n",
	.htxt = "q avg before   "
	"q avg after    "
	"expected       "
	"difference %   "
	"tolerance %    "
	"result	 "
	"\n",
	.tconfig = &ft4_tconfig,
	.tqueue = &ft4_tqueue,
	.tvar = &ft_tvar,
	.tlevel = ft4_tlevel,
};

static enum test_result func_test4(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint64_t time_diff = 0;
	uint64_t start = 0;
	double avg_before = 0.0;
	double avg_after = 0.0;
        double exp_avg = 0.0;
        double diff = 0.0;

	printf("%s", tcfg->msg);

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	rte_red_rt_data_init(tcfg->tqueue->rdata);

	if (increase_actual_qsize(tcfg->tconfig->rconfig,
				  tcfg->tqueue->rdata,
				  tcfg->tqueue->q,
				  *tcfg->tlevel,
				  tcfg->tqueue->q_ramp_up) != 0) {
		result = FAIL;
		goto out;
	}

	if (increase_average_qsize(tcfg->tconfig->rconfig,
				   tcfg->tqueue->rdata,
				   tcfg->tqueue->q,
				   *tcfg->tlevel,
				   tcfg->tqueue->avg_ramp_up) != 0) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	avg_before = rte_red_get_avg_float(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);

	/**
	 * empty the queue
	 */
	*tcfg->tqueue->q = 0;
	rte_red_mark_queue_empty(tcfg->tqueue->rdata, get_port_ts());

	/**
	 * record empty time locally
	 */
	start = rte_rdtsc();

	sleep(tcfg->tvar->sleep_sec);

	/**
	 * enqueue one packet to recalculate average queue size
	 */
	if (rte_red_enqueue(tcfg->tconfig->rconfig,
			    tcfg->tqueue->rdata,
			    *tcfg->tqueue->q,
			    get_port_ts()) != 0) {
		result = FAIL;
		goto out;
	}
	(*tcfg->tqueue->q)++;

	/**
	 * calculate how long queue has been empty
	 */
	time_diff = ((rte_rdtsc() - start) / tcfg->tvar->clk_freq)
		  * MSEC_PER_SEC;
	if (time_diff < MAX_QEMPTY_TIME_MSEC) {
		/**
		 * this could happen if sleep was interrupted for some reason
		 */
		result = FAIL;
		goto out;
	}

	/**
	 * confirm that average queue size is now at expected level
	 */
        exp_avg = 0.0;
	avg_after = rte_red_get_avg_float(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);
	if (!check_avg(&diff, avg_after, exp_avg, (double)tcfg->tqueue->avg_tolerance))
	        result = FAIL;

	printf("%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15s\n",
	       avg_before, avg_after, exp_avg,
	       diff, (double)tcfg->tqueue->avg_tolerance,
	       diff <= (double)tcfg->tqueue->avg_tolerance ? "pass" : "fail");
out:
	return result;
}

/**
 * Test F5: functional test 5
 */
static uint32_t ft5_tlevel[] = {127};
static uint8_t ft5_wq_log2[] = {9, 8};
static uint8_t ft5_maxp_inv[] = {10, 20};
static struct rte_red_config ft5_config[2];
static struct rte_red ft5_data[4];
static uint32_t ft5_q[4];
static uint32_t ft5_dropped[] = {0, 0, 0, 0};
static uint32_t ft5_enqueued[] = {0, 0, 0, 0};

static struct test_rte_red_config ft5_tconfig =  {
	.rconfig = ft5_config,
	.num_cfg = RTE_DIM(ft5_config),
	.min_th = 32,
	.max_th = 128,
	.wq_log2 = ft5_wq_log2,
	.maxp_inv = ft5_maxp_inv,
};

static struct test_queue ft5_tqueue = {
	.rdata = ft5_data,
	.num_queues = RTE_DIM(ft5_data),
	.qconfig = ft_qconfig,
	.q = ft5_q,
	.q_ramp_up = 1000000,
	.avg_ramp_up = 1000000,
	.avg_tolerance = 5,  /* 10 percent */
	.drop_tolerance = 50,  /* 50 percent */
};

struct test_var ft5_tvar = {
	.wait_usec = 0,
	.num_iterations = 15,
	.num_ops = 10000,
	.clk_freq = 0,
	.dropped = ft5_dropped,
	.enqueued = ft5_enqueued,
	.sleep_sec = 0,
};

static struct test_config func_test5_config = {
	.ifname = "functional test 5 interface",
	.msg = "functional test 5 : use several queues (each with its own run-time data),\n"
	"		    use several RED configurations (such that each configuration is shared by multiple queues),\n"
	"		    increase average queue size to just below maximum threshold,\n"
	"		    compare drop rate to drop probability,\n"
	"		    (this is a larger scale version of functional test 2)\n\n",
	.htxt = "queue          "
	"config         "
	"avg queue size "
	"min threshold  "
	"max threshold  "
	"drop prob %    "
	"drop rate %    "
	"diff %         "
	"tolerance %    "
	"\n",
	.tconfig = &ft5_tconfig,
	.tqueue = &ft5_tqueue,
	.tvar = &ft5_tvar,
	.tlevel = ft5_tlevel,
};

static enum test_result func_test5(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t j = 0;

	printf("%s", tcfg->msg);

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	for (j = 0; j < tcfg->tqueue->num_queues; j++) {
		rte_red_rt_data_init(&tcfg->tqueue->rdata[j]);
		tcfg->tqueue->q[j] = 0;

		if (increase_actual_qsize(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
					  &tcfg->tqueue->rdata[j],
					  &tcfg->tqueue->q[j],
					  *tcfg->tlevel,
					  tcfg->tqueue->q_ramp_up) != 0) {
			result = FAIL;
			goto out;
		}

		if (increase_average_qsize(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
					   &tcfg->tqueue->rdata[j],
					   &tcfg->tqueue->q[j],
					   *tcfg->tlevel,
					   tcfg->tqueue->avg_ramp_up) != 0) {
			result = FAIL;
			goto out;
		}
	}

	for (j = 0; j < tcfg->tqueue->num_queues; j++) {
		uint32_t avg = 0;
		double drop_rate = 0.0;
		double drop_prob = 0.0;
		double diff = 0.0;

		tcfg->tvar->dropped[j] = 0;
		tcfg->tvar->enqueued[j] = 0;

		enqueue_dequeue_func(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
				     &tcfg->tqueue->rdata[j],
				     &tcfg->tqueue->q[j],
				     tcfg->tvar->num_ops,
				     &tcfg->tvar->enqueued[j],
				     &tcfg->tvar->dropped[j]);

		avg = rte_red_get_avg_int(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
					  &tcfg->tqueue->rdata[j]);
		if (avg != *tcfg->tlevel)
			result = FAIL;

		drop_rate = calc_drop_rate(tcfg->tvar->enqueued[j],tcfg->tvar->dropped[j]);
		drop_prob = calc_drop_prob(tcfg->tconfig->min_th, tcfg->tconfig->max_th,
					   tcfg->tconfig->maxp_inv[tcfg->tqueue->qconfig[j]],
					   *tcfg->tlevel);
		if (!check_drop_rate(&diff, drop_rate, drop_prob, (double)tcfg->tqueue->drop_tolerance))
		        result = FAIL;

		printf("%-15u%-15u%-15u%-15u%-15u%-15.4lf%-15.4lf%-15.4lf%-15.4lf\n",
		       j, tcfg->tqueue->qconfig[j], avg,
		       tcfg->tconfig->min_th, tcfg->tconfig->max_th,
		       drop_prob * 100.0, drop_rate * 100.0,
		       diff, (double)tcfg->tqueue->drop_tolerance);
	}
out:
	return result;
}

/**
 * Test F6: functional test 6
 */
static uint32_t ft6_tlevel[] = {1022};
static uint8_t ft6_wq_log2[] = {9, 8};
static uint8_t ft6_maxp_inv[] = {10, 20};
static struct rte_red_config ft6_config[2];
static struct rte_red ft6_data[4];
static uint32_t ft6_q[4];

static struct test_rte_red_config ft6_tconfig =  {
	.rconfig = ft6_config,
	.num_cfg = RTE_DIM(ft6_config),
	.min_th = 32,
	.max_th = 1023,
	.wq_log2 = ft6_wq_log2,
	.maxp_inv = ft6_maxp_inv,
};

static struct test_queue ft6_tqueue = {
	.rdata = ft6_data,
	.num_queues = RTE_DIM(ft6_data),
	.qconfig = ft_qconfig,
	.q = ft6_q,
	.q_ramp_up = 1000000,
	.avg_ramp_up = 1000000,
	.avg_tolerance = 5,  /* 10 percent */
	.drop_tolerance = 50,  /* 50 percent */
};

static struct test_config func_test6_config = {
	.ifname = "functional test 6 interface",
	.msg = "functional test 6 : use several queues (each with its own run-time data),\n"
	"		    use several RED configurations (such that each configuration is shared by multiple queues),\n"
	"		    increase average queue size to target level,\n"
	"		    dequeue all packets until queue is empty,\n"
	"		    confirm that average queue size is computed correctly while queue is empty\n"
	"		    (this is a larger scale version of functional test 3)\n\n",
	.htxt = "queue          "
	"config         "
	"q avg before   "
	"q avg after    "
	"expected       "
	"difference %   "
	"tolerance %    "
	"result	 ""\n",
	.tconfig = &ft6_tconfig,
	.tqueue = &ft6_tqueue,
	.tvar = &ft_tvar,
	.tlevel = ft6_tlevel,
};

static enum test_result func_test6(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t j = 0;

	printf("%s", tcfg->msg);
	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}
	printf("%s", tcfg->htxt);

	for (j = 0; j < tcfg->tqueue->num_queues; j++) {
		rte_red_rt_data_init(&tcfg->tqueue->rdata[j]);
		tcfg->tqueue->q[j] = 0;

		if (increase_actual_qsize(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
					  &tcfg->tqueue->rdata[j],
					  &tcfg->tqueue->q[j],
					  *tcfg->tlevel,
					  tcfg->tqueue->q_ramp_up) != 0) {
			result = FAIL;
			goto out;
		}
		if (increase_average_qsize(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
					   &tcfg->tqueue->rdata[j],
					   &tcfg->tqueue->q[j],
					   *tcfg->tlevel,
					   tcfg->tqueue->avg_ramp_up) != 0) {
			result = FAIL;
			goto out;
		}
	}
	for (j = 0; j < tcfg->tqueue->num_queues; j++) {
		double avg_before = 0;
		double avg_after = 0;
		double exp_avg = 0;
		double diff = 0.0;

		avg_before = rte_red_get_avg_float(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
						   &tcfg->tqueue->rdata[j]);

		/**
		 * empty the queue
		 */
		tcfg->tqueue->q[j] = 0;
		rte_red_mark_queue_empty(&tcfg->tqueue->rdata[j], get_port_ts());
		rte_delay_us(tcfg->tvar->wait_usec);

		/**
		 * enqueue one packet to recalculate average queue size
		 */
		if (rte_red_enqueue(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
				    &tcfg->tqueue->rdata[j],
				    tcfg->tqueue->q[j],
				    get_port_ts()) == 0) {
			tcfg->tqueue->q[j]++;
		} else {
			printf("%s:%d: packet enqueued on empty queue was dropped\n", __func__, __LINE__);
			result = FAIL;
		}

		exp_avg = calc_exp_avg_on_empty(avg_before,
				(1 << tcfg->tconfig->wq_log2[tcfg->tqueue->qconfig[j]]),
				tcfg->tvar->wait_usec);
		avg_after = rte_red_get_avg_float(&tcfg->tconfig->rconfig[tcfg->tqueue->qconfig[j]],
						&tcfg->tqueue->rdata[j]);
		if (!check_avg(&diff, avg_after, exp_avg, (double)tcfg->tqueue->avg_tolerance))
		        result = FAIL;

		printf("%-15u%-15u%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15s\n",
		       j, tcfg->tqueue->qconfig[j], avg_before, avg_after,
		       exp_avg, diff, (double)tcfg->tqueue->avg_tolerance,
		       diff <= tcfg->tqueue->avg_tolerance ? "pass" : "fail");
	}
out:
	return result;
}

/**
 * setup default values for the performance test structures
 */
static struct rte_red_config pt_wrconfig[1];
static struct rte_red pt_rtdata[1];
static uint8_t pt_wq_log2[] = {9};
static uint8_t pt_maxp_inv[] = {10};
static uint32_t pt_qconfig[] = {0};
static uint32_t pt_q[] = {0};
static uint32_t pt_dropped[] = {0};
static uint32_t pt_enqueued[] = {0};

static struct test_rte_red_config pt_tconfig =  {
	.rconfig = pt_wrconfig,
	.num_cfg = RTE_DIM(pt_wrconfig),
	.wq_log2 = pt_wq_log2,
	.min_th = 32,
	.max_th = 128,
	.maxp_inv = pt_maxp_inv,
};

static struct test_queue pt_tqueue = {
	.rdata = pt_rtdata,
	.num_queues = RTE_DIM(pt_rtdata),
	.qconfig = pt_qconfig,
	.q = pt_q,
	.q_ramp_up = 1000000,
	.avg_ramp_up = 1000000,
	.avg_tolerance = 5,  /* 10 percent */
	.drop_tolerance = 50,  /* 50 percent */
};

/**
 * enqueue/dequeue packets
 */
static void enqueue_dequeue_perf(struct rte_red_config *red_cfg,
				 struct rte_red *red,
				 uint32_t *q,
				 uint32_t num_ops,
				 uint32_t *enqueued,
				 uint32_t *dropped,
				 struct rdtsc_prof *prof)
{
	uint32_t i = 0;

	for (i = 0; i < num_ops; i++) {
		uint64_t ts = 0;
		int ret = 0;
		/**
		 * enqueue
		 */
		ts = get_port_ts();
		rdtsc_prof_start(prof);
		ret = rte_red_enqueue(red_cfg, red, *q, ts );
		rdtsc_prof_end(prof);
		if (ret == 0)
			(*enqueued)++;
		else
			(*dropped)++;
	}
}

/**
 * Setup test structures for tests P1, P2, P3
 * performance tests 1, 2 and 3
 */
static uint32_t pt1_tlevel[] = {16};
static uint32_t pt2_tlevel[] = {80};
static uint32_t pt3_tlevel[] = {144};

static struct test_var perf1_tvar = {
	.wait_usec = 0,
	.num_iterations = 15,
	.num_ops = 50000000,
	.clk_freq = 0,
	.dropped = pt_dropped,
	.enqueued = pt_enqueued,
	.sleep_sec = 0
};

static struct test_config perf1_test1_config = {
	.ifname = "performance test 1 interface",
	.msg = "performance test 1 : use one RED configuration,\n"
	"		     set actual and average queue sizes to level below min threshold,\n"
	"		     measure enqueue performance\n\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf1_tvar,
	.tlevel = pt1_tlevel,
};

static struct test_config perf1_test2_config = {
	.ifname = "performance test 2 interface",
	.msg = "performance test 2 : use one RED configuration,\n"
	"		     set actual and average queue sizes to level in between min and max thresholds,\n"
	"		     measure enqueue performance\n\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf1_tvar,
	.tlevel = pt2_tlevel,
};

static struct test_config perf1_test3_config = {
	.ifname = "performance test 3 interface",
	.msg = "performance test 3 : use one RED configuration,\n"
	"		     set actual and average queue sizes to level above max threshold,\n"
	"		     measure enqueue performance\n\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf1_tvar,
	.tlevel = pt3_tlevel,
};

/**
 * Performance test function to measure enqueue performance.
 * This runs performance tests 1, 2 and 3
 */
static enum test_result perf1_test(struct test_config *tcfg)
{
	enum test_result result = PASS;
	struct rdtsc_prof prof = {0, 0, 0, 0, 0.0, NULL};
	uint32_t total = 0;

	printf("%s", tcfg->msg);

	rdtsc_prof_init(&prof, "enqueue");

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	/**
	 * set average queue size to target level
	 */
	*tcfg->tqueue->q = *tcfg->tlevel;

	/**
	 * initialize the rte_red run time data structure
	 */
	rte_red_rt_data_init(tcfg->tqueue->rdata);

	/**
	 *  set the queue average
	 */
	rte_red_set_avg_int(tcfg->tconfig->rconfig, tcfg->tqueue->rdata, *tcfg->tlevel);
	if (rte_red_get_avg_int(tcfg->tconfig->rconfig, tcfg->tqueue->rdata)
	    != *tcfg->tlevel) {
		result = FAIL;
		goto out;
	}

	enqueue_dequeue_perf(tcfg->tconfig->rconfig,
			     tcfg->tqueue->rdata,
			     tcfg->tqueue->q,
			     tcfg->tvar->num_ops,
			     tcfg->tvar->enqueued,
			     tcfg->tvar->dropped,
			     &prof);

	total = *tcfg->tvar->enqueued + *tcfg->tvar->dropped;

	printf("\ntotal: %u, enqueued: %u (%.2lf%%), dropped: %u (%.2lf%%)\n", total,
	       *tcfg->tvar->enqueued, ((double)(*tcfg->tvar->enqueued) / (double)total) * 100.0,
	       *tcfg->tvar->dropped, ((double)(*tcfg->tvar->dropped) / (double)total) * 100.0);

	rdtsc_prof_print(&prof);
out:
	return result;
}

/**
 * Setup test structures for tests P4, P5, P6
 * performance tests 4, 5 and 6
 */
static uint32_t pt4_tlevel[] = {16};
static uint32_t pt5_tlevel[] = {80};
static uint32_t pt6_tlevel[] = {144};

static struct test_var perf2_tvar = {
	.wait_usec = 500,
	.num_iterations = 10000,
	.num_ops = 10000,
	.dropped = pt_dropped,
	.enqueued = pt_enqueued,
	.sleep_sec = 0
};

static struct test_config perf2_test4_config = {
	.ifname = "performance test 4 interface",
	.msg = "performance test 4 : use one RED configuration,\n"
	"		     set actual and average queue sizes to level below min threshold,\n"
	"		     dequeue all packets until queue is empty,\n"
	"		     measure enqueue performance when queue is empty\n\n",
	.htxt = "iteration      "
	"q avg before   "
	"q avg after    "
	"expected       "
	"difference %   "
	"tolerance %    "
	"result	 ""\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf2_tvar,
	.tlevel = pt4_tlevel,
};

static struct test_config perf2_test5_config = {
	.ifname = "performance test 5 interface",
	.msg = "performance test 5 : use one RED configuration,\n"
	"		     set actual and average queue sizes to level in between min and max thresholds,\n"
	"		     dequeue all packets until queue is empty,\n"
	"		     measure enqueue performance when queue is empty\n\n",
	.htxt = "iteration      "
	"q avg before   "
	"q avg after    "
	"expected       "
	"difference     "
	"tolerance      "
	"result	 ""\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf2_tvar,
	.tlevel = pt5_tlevel,
};

static struct test_config perf2_test6_config = {
	.ifname = "performance test 6 interface",
	.msg = "performance test 6 : use one RED configuration,\n"
	"		     set actual and average queue sizes to level above max threshold,\n"
	"		     dequeue all packets until queue is empty,\n"
	"		     measure enqueue performance when queue is empty\n\n",
	.htxt = "iteration      "
	"q avg before   "
	"q avg after    "
	"expected       "
	"difference %   "
	"tolerance %    "
	"result	 ""\n",
	.tconfig = &pt_tconfig,
	.tqueue = &pt_tqueue,
	.tvar = &perf2_tvar,
	.tlevel = pt6_tlevel,
};

/**
 * Performance test function to measure enqueue performance when the
 * queue is empty. This runs performance tests 4, 5 and 6
 */
static enum test_result perf2_test(struct test_config *tcfg)
{
	enum test_result result = PASS;
	struct rdtsc_prof prof = {0, 0, 0, 0, 0.0, NULL};
	uint32_t total = 0;
	uint32_t i = 0;

	printf("%s", tcfg->msg);

	rdtsc_prof_init(&prof, "enqueue");

	if (test_rte_red_init(tcfg) != PASS) {
		result = FAIL;
		goto out;
	}

	printf("%s", tcfg->htxt);

	for (i = 0; i < tcfg->tvar->num_iterations; i++) {
		uint32_t count = 0;
		uint64_t ts = 0;
		double avg_before = 0;
		int ret = 0;

		/**
		 * set average queue size to target level
		 */
		*tcfg->tqueue->q = *tcfg->tlevel;
		count = (*tcfg->tqueue->rdata).count;

		/**
		 * initialize the rte_red run time data structure
		 */
		rte_red_rt_data_init(tcfg->tqueue->rdata);
		(*tcfg->tqueue->rdata).count = count;

		/**
		 * set the queue average
		 */
		rte_red_set_avg_int(tcfg->tconfig->rconfig, tcfg->tqueue->rdata, *tcfg->tlevel);
		avg_before = rte_red_get_avg_float(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);
		if ((avg_before < *tcfg->tlevel) || (avg_before > *tcfg->tlevel)) {
			result = FAIL;
			goto out;
		}

		/**
		 * empty the queue
		 */
		*tcfg->tqueue->q = 0;
		rte_red_mark_queue_empty(tcfg->tqueue->rdata, get_port_ts());

		/**
		 * wait for specified period of time
		 */
		rte_delay_us(tcfg->tvar->wait_usec);

		/**
		 * measure performance of enqueue operation while queue is empty
		 */
		ts = get_port_ts();
		rdtsc_prof_start(&prof);
		ret = rte_red_enqueue(tcfg->tconfig->rconfig, tcfg->tqueue->rdata,
				      *tcfg->tqueue->q, ts );
		rdtsc_prof_end(&prof);

		/**
		 * gather enqueued/dropped statistics
		 */
		if (ret == 0)
			(*tcfg->tvar->enqueued)++;
		else
			(*tcfg->tvar->dropped)++;

		/**
		 * on first and last iteration, confirm that
		 * average queue size was computed correctly
		 */
		if ((i == 0) || (i == tcfg->tvar->num_iterations - 1)) {
			double avg_after = 0;
			double exp_avg = 0;
			double diff = 0.0;
			int ok = 0;

			avg_after = rte_red_get_avg_float(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);
			exp_avg = calc_exp_avg_on_empty(avg_before,
						  (1 << *tcfg->tconfig->wq_log2),
						  tcfg->tvar->wait_usec);
			if (check_avg(&diff, avg_after, exp_avg, (double)tcfg->tqueue->avg_tolerance))
		        	ok = 1;
			printf("%-15u%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15.4lf%-15s\n",
				i, avg_before, avg_after, exp_avg, diff,
				(double)tcfg->tqueue->avg_tolerance, ok ? "pass" : "fail");
			if (!ok) {
				result = FAIL;
				goto out;
			}
		}
	}
	total =  *tcfg->tvar->enqueued +  *tcfg->tvar->dropped;
	printf("\ntotal: %u, enqueued: %u (%.2lf%%), dropped: %u (%.2lf%%)\n", total,
	       *tcfg->tvar->enqueued, ((double)(*tcfg->tvar->enqueued) / (double)total) * 100.0,
	       *tcfg->tvar->dropped, ((double)(*tcfg->tvar->dropped) / (double)total) * 100.0);

	rdtsc_prof_print(&prof);
out:
	return result;
}

/**
 * setup default values for overflow test structures
 */
static uint32_t avg_max = 0;
static uint32_t avg_max_bits = 0;

static struct rte_red_config ovfl_wrconfig[1];
static struct rte_red ovfl_rtdata[1];
static uint8_t ovfl_maxp_inv[] = {10};
static uint32_t ovfl_qconfig[] = {0, 0, 1, 1};
static uint32_t ovfl_q[] ={0};
static uint32_t ovfl_dropped[] ={0};
static uint32_t ovfl_enqueued[] ={0};
static uint32_t ovfl_tlevel[] = {1023};
static uint8_t ovfl_wq_log2[] = {12};

static struct test_rte_red_config ovfl_tconfig =  {
	.rconfig = ovfl_wrconfig,
	.num_cfg = RTE_DIM(ovfl_wrconfig),
	.wq_log2 = ovfl_wq_log2,
	.min_th = 32,
	.max_th = 1023,
	.maxp_inv = ovfl_maxp_inv,
};

static struct test_queue ovfl_tqueue = {
	.rdata = ovfl_rtdata,
	.num_queues = RTE_DIM(ovfl_rtdata),
	.qconfig = ovfl_qconfig,
	.q = ovfl_q,
	.q_ramp_up = 1000000,
	.avg_ramp_up = 1000000,
	.avg_tolerance = 5,  /* 10 percent */
	.drop_tolerance = 50,  /* 50 percent */
};

static struct test_var ovfl_tvar = {
	.wait_usec = 10000,
	.num_iterations = 1,
	.num_ops = 10000,
	.clk_freq = 0,
	.dropped = ovfl_dropped,
	.enqueued = ovfl_enqueued,
	.sleep_sec = 0
};

static void ovfl_check_avg(uint32_t avg)
{
	if (avg > avg_max) {
		double avg_log = 0;
		uint32_t bits = 0;
		avg_max = avg;
		avg_log = log(((double)avg_max));
		avg_log = avg_log / log(2.0);
		bits = (uint32_t)ceil(avg_log);
		if (bits > avg_max_bits)
			avg_max_bits = bits;
	}
}

static struct test_config ovfl_test1_config = {
	.ifname = "queue avergage overflow test interface",
	.msg = "overflow test 1 : use one RED configuration,\n"
	"		  increase average queue size to target level,\n"
	"		  check maximum number of bits requirte_red to represent avg_s\n\n",
	.htxt = "avg queue size  "
	"wq_log2  "
	"fraction bits  "
	"max queue avg  "
	"num bits  "
	"enqueued  "
	"dropped   "
	"drop prob %  "
	"drop rate %  "
	"\n",
	.tconfig = &ovfl_tconfig,
	.tqueue = &ovfl_tqueue,
	.tvar = &ovfl_tvar,
	.tlevel = ovfl_tlevel,
};

static enum test_result ovfl_test1(struct test_config *tcfg)
{
	enum test_result result = PASS;
	uint32_t avg = 0;
	uint32_t i = 0;
	double drop_rate = 0.0;
	double drop_prob = 0.0;
	double diff = 0.0;
	int ret = 0;

	printf("%s", tcfg->msg);

	if (test_rte_red_init(tcfg) != PASS) {

		result = FAIL;
		goto out;
	}

	/**
	 * reset rte_red run-time data
	 */
	rte_red_rt_data_init(tcfg->tqueue->rdata);

	/**
	 * increase actual queue size
	 */
	for (i = 0; i < tcfg->tqueue->q_ramp_up; i++) {
		ret = rte_red_enqueue(tcfg->tconfig->rconfig, tcfg->tqueue->rdata,
				      *tcfg->tqueue->q, get_port_ts());

		if (ret == 0) {
			if (++(*tcfg->tqueue->q) >= *tcfg->tlevel)
				break;
		}
	}

	/**
	 * enqueue
	 */
	for (i = 0; i < tcfg->tqueue->avg_ramp_up; i++) {
		ret = rte_red_enqueue(tcfg->tconfig->rconfig, tcfg->tqueue->rdata,
				      *tcfg->tqueue->q, get_port_ts());
		ovfl_check_avg((*tcfg->tqueue->rdata).avg);
		avg = rte_red_get_avg_int(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);
		if (avg == *tcfg->tlevel) {
			if (ret == 0)
				(*tcfg->tvar->enqueued)++;
			else
				(*tcfg->tvar->dropped)++;
		}
	}

	/**
	 * check if target average queue size has been reached
	 */
	avg = rte_red_get_avg_int(tcfg->tconfig->rconfig, tcfg->tqueue->rdata);
	if (avg != *tcfg->tlevel) {
		result = FAIL;
		goto out;
	}

	/**
	 * check drop rate against drop probability
	 */
	drop_rate = calc_drop_rate(*tcfg->tvar->enqueued, *tcfg->tvar->dropped);
	drop_prob = calc_drop_prob(tcfg->tconfig->min_th,
				   tcfg->tconfig->max_th,
				   *tcfg->tconfig->maxp_inv,
				   *tcfg->tlevel);
	if (!check_drop_rate(&diff, drop_rate, drop_prob, (double)tcfg->tqueue->drop_tolerance))
	        result = FAIL;

	printf("%s", tcfg->htxt);

	printf("%-16u%-9u%-15u0x%08x     %-10u%-10u%-10u%-13.2lf%-13.2lf\n",
	       avg, *tcfg->tconfig->wq_log2, RTE_RED_SCALING,
	       avg_max, avg_max_bits,
	       *tcfg->tvar->enqueued, *tcfg->tvar->dropped,
	       drop_prob * 100.0, drop_rate * 100.0);
out:
	return result;
}

/**
 * define the functional and performance tests to be executed
 */
struct tests func_tests[] = {
	{ &func_test1_config, func_test1 },
	{ &func_test2_config, func_test2 },
	{ &func_test3_config, func_test3 },
	{ &func_test4_config, func_test4 },
	{ &func_test5_config, func_test5 },
	{ &func_test6_config, func_test6 },
	{ &ovfl_test1_config, ovfl_test1 },
};

struct tests func_tests_quick[] = {
	{ &func_test1_config, func_test1 },
	{ &func_test2_config, func_test2 },
	{ &func_test3_config, func_test3 },
	/* no test 4 as it takes a lot of time */
	{ &func_test5_config, func_test5 },
	{ &func_test6_config, func_test6 },
	{ &ovfl_test1_config, ovfl_test1 },
};

struct tests perf_tests[] = {
	{ &perf1_test1_config, perf1_test },
	{ &perf1_test2_config, perf1_test },
	{ &perf1_test3_config, perf1_test },
	{ &perf2_test4_config, perf2_test },
	{ &perf2_test5_config, perf2_test },
	{ &perf2_test6_config, perf2_test },
};

/**
 * function to execute the required_red tests
 */
static void run_tests(struct tests *test_type, uint32_t test_count, uint32_t *num_tests, uint32_t *num_pass)
{
	enum test_result result = PASS;
	uint32_t i = 0;

	for (i = 0; i < test_count; i++) {
		printf("\n--------------------------------------------------------------------------------\n");
		result = test_type[i].testfn(test_type[i].testcfg);
		(*num_tests)++;
		if (result == PASS) {
			(*num_pass)++;
				printf("-------------------------------------<pass>-------------------------------------\n");
		} else {
			printf("-------------------------------------<fail>-------------------------------------\n");
		}
	}
	return;
}

/**
 * check if functions accept invalid parameters
 *
 * First, all functions will be called without initialized RED
 * Then, all of them will be called with NULL/invalid parameters
 *
 * Some functions are not tested as they are performance-critical and thus
 * don't do any parameter checking.
 */
static int
test_invalid_parameters(void)
{
	struct rte_red_config config;

	if (rte_red_rt_data_init(NULL) == 0) {
		printf("rte_red_rt_data_init should have failed!\n");
		return -1;
	}

	if (rte_red_config_init(NULL, 0, 0, 0, 0) == 0) {
		printf("rte_red_config_init should have failed!\n");
		return -1;
	}

	if (rte_red_rt_data_init(NULL) == 0) {
		printf("rte_red_rt_data_init should have failed!\n");
		return -1;
	}

	/* NULL config */
	if (rte_red_config_init(NULL, 0, 0, 0, 0) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}
	/* min_treshold == max_treshold */
	if (rte_red_config_init(&config, 0, 1, 1, 0) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}
	/* min_treshold > max_treshold */
	if (rte_red_config_init(&config, 0, 2, 1, 0) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}
	/* wq_log2 > RTE_RED_WQ_LOG2_MAX */
	if (rte_red_config_init(&config,
			RTE_RED_WQ_LOG2_MAX + 1, 1, 2, 0) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}
	/* wq_log2 < RTE_RED_WQ_LOG2_MIN */
	if (rte_red_config_init(&config,
			RTE_RED_WQ_LOG2_MIN - 1, 1, 2, 0) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}
	/* maxp_inv > RTE_RED_MAXP_INV_MAX */
	if (rte_red_config_init(&config,
			RTE_RED_WQ_LOG2_MIN, 1, 2, RTE_RED_MAXP_INV_MAX + 1) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}
	/* maxp_inv < RTE_RED_MAXP_INV_MIN */
	if (rte_red_config_init(&config,
			RTE_RED_WQ_LOG2_MIN, 1, 2, RTE_RED_MAXP_INV_MIN - 1) == 0) {
		printf("%i: rte_red_config_init should have failed!\n", __LINE__);
		return -1;
	}

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
test_red(void)
{
	uint32_t num_tests = 0;
	uint32_t num_pass = 0;

	if (test_invalid_parameters() < 0)
		return -1;
	run_tests(func_tests_quick, RTE_DIM(func_tests_quick),
		  &num_tests, &num_pass);
	show_stats(num_tests, num_pass);
	return tell_the_result(num_tests, num_pass);
}

static int
test_red_perf(void)
{
	uint32_t num_tests = 0;
	uint32_t num_pass = 0;

	run_tests(perf_tests, RTE_DIM(perf_tests), &num_tests, &num_pass);
	show_stats(num_tests, num_pass);
	return tell_the_result(num_tests, num_pass);
}

static int
test_red_all(void)
{
	uint32_t num_tests = 0;
	uint32_t num_pass = 0;

	if (test_invalid_parameters() < 0)
		return -1;

	run_tests(func_tests, RTE_DIM(func_tests), &num_tests, &num_pass);
	run_tests(perf_tests, RTE_DIM(perf_tests), &num_tests, &num_pass);
	show_stats(num_tests, num_pass);
	return tell_the_result(num_tests, num_pass);
}

REGISTER_TEST_COMMAND(red_autotest, test_red);
REGISTER_TEST_COMMAND(red_perf, test_red_perf);
REGISTER_TEST_COMMAND(red_all, test_red_all);
