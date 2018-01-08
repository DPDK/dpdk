/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "test.h"

#include <rte_cycles.h>
#include <rte_meter.h>

#define mlog(format, ...) do{\
		printf("Line %d:",__LINE__);\
		printf(format, ##__VA_ARGS__);\
		printf("\n");\
	}while(0);

#define melog(format, ...) do{\
		printf("Line %d:",__LINE__);\
		printf(format, ##__VA_ARGS__);\
		printf(" failed!\n");\
		return -1;\
	}while(0);

#define TM_TEST_SRTCM_CIR_DF 46000000
#define TM_TEST_SRTCM_CBS_DF 2048
#define TM_TEST_SRTCM_EBS_DF 4096

#define TM_TEST_TRTCM_CIR_DF 46000000
#define TM_TEST_TRTCM_PIR_DF 69000000
#define TM_TEST_TRTCM_CBS_DF 2048
#define TM_TEST_TRTCM_PBS_DF 4096

static struct rte_meter_srtcm_params sparams =
				{.cir = TM_TEST_SRTCM_CIR_DF,
				 .cbs = TM_TEST_SRTCM_CBS_DF,
				 .ebs = TM_TEST_SRTCM_EBS_DF,};

static struct	rte_meter_trtcm_params tparams=
				{.cir = TM_TEST_TRTCM_CIR_DF,
				 .pir = TM_TEST_TRTCM_PIR_DF,
				 .cbs = TM_TEST_TRTCM_CBS_DF,
				 .pbs = TM_TEST_TRTCM_PBS_DF,};

/**
 * functional test for rte_meter_srtcm_config
 */
static inline int
tm_test_srtcm_config(void)
{
#define SRTCM_CFG_MSG "srtcm_config"
	struct rte_meter_srtcm_profile sp;
	struct  rte_meter_srtcm_params sparams1;

	/* invalid parameter test */
	if (rte_meter_srtcm_profile_config(NULL, NULL) == 0)
		melog(SRTCM_CFG_MSG);
	if (rte_meter_srtcm_profile_config(&sp, NULL) == 0)
		melog(SRTCM_CFG_MSG);
	if (rte_meter_srtcm_profile_config(NULL, &sparams) == 0)
		melog(SRTCM_CFG_MSG);

	/* cbs and ebs can't both be zero */
	sparams1 = sparams;
	sparams1.cbs = 0;
	sparams1.ebs = 0;
	if (rte_meter_srtcm_profile_config(&sp, &sparams1) == 0)
		melog(SRTCM_CFG_MSG);

	/* cir should never be 0 */
	sparams1 = sparams;
	sparams1.cir = 0;
	if (rte_meter_srtcm_profile_config(&sp, &sparams1) == 0)
		melog(SRTCM_CFG_MSG);

	/* one of ebs and cbs can be zero, should be successful */
	sparams1 = sparams;
	sparams1.ebs = 0;
	if (rte_meter_srtcm_profile_config(&sp, &sparams1) != 0)
		melog(SRTCM_CFG_MSG);

	sparams1 = sparams;
	sparams1.cbs = 0;
	if (rte_meter_srtcm_profile_config(&sp, &sparams1) != 0)
		melog(SRTCM_CFG_MSG);

	/* usual parameter, should be successful */
	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_CFG_MSG);

	return 0;

}

/**
 * functional test for rte_meter_trtcm_config
 */
static inline int
tm_test_trtcm_config(void)
{
	struct rte_meter_trtcm_profile tp;
	struct  rte_meter_trtcm_params tparams1;
#define TRTCM_CFG_MSG "trtcm_config"

	/* invalid parameter test */
	if (rte_meter_trtcm_profile_config(NULL, NULL) == 0)
		melog(TRTCM_CFG_MSG);
	if (rte_meter_trtcm_profile_config(&tp, NULL) == 0)
		melog(TRTCM_CFG_MSG);
	if (rte_meter_trtcm_profile_config(NULL, &tparams) == 0)
		melog(TRTCM_CFG_MSG);

	/* cir, cbs, pir and pbs never be zero */
	tparams1 = tparams;
	tparams1.cir = 0;
	if (rte_meter_trtcm_profile_config(&tp, &tparams1) == 0)
		melog(TRTCM_CFG_MSG);

	tparams1 = tparams;
	tparams1.cbs = 0;
	if (rte_meter_trtcm_profile_config(&tp, &tparams1) == 0)
		melog(TRTCM_CFG_MSG);

	tparams1 = tparams;
	tparams1.pbs = 0;
	if (rte_meter_trtcm_profile_config(&tp, &tparams1) == 0)
		melog(TRTCM_CFG_MSG);

	tparams1 = tparams;
	tparams1.pir = 0;
	if (rte_meter_trtcm_profile_config(&tp, &tparams1) == 0)
		melog(TRTCM_CFG_MSG);

	/* pir should be greater or equal to cir */
	tparams1 = tparams;
	tparams1.pir = tparams1.cir - 1;
	if (rte_meter_trtcm_profile_config(&tp, &tparams1) == 0)
		melog(TRTCM_CFG_MSG" pir < cir test");

	/* usual parameter, should be successful */
	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_CFG_MSG);

	return 0;
}

/**
 * functional test for rte_meter_srtcm_color_blind_check
 */
static inline int
tm_test_srtcm_color_blind_check(void)
{
#define SRTCM_BLIND_CHECK_MSG "srtcm_blind_check"
	struct rte_meter_srtcm_profile sp;
	struct rte_meter_srtcm sm;
	uint64_t time;
	uint64_t hz = rte_get_tsc_hz();

	/* Test green */
	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_blind_check(
		&sm, &sp, time, TM_TEST_SRTCM_CBS_DF - 1)
		!= e_RTE_METER_GREEN)
		melog(SRTCM_BLIND_CHECK_MSG" GREEN");

	/* Test yellow */
	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_blind_check(
		&sm, &sp, time, TM_TEST_SRTCM_CBS_DF + 1)
		!= e_RTE_METER_YELLOW)
		melog(SRTCM_BLIND_CHECK_MSG" YELLOW");

	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_blind_check(
		&sm, &sp, time, (uint32_t)sp.ebs - 1) != e_RTE_METER_YELLOW)
		melog(SRTCM_BLIND_CHECK_MSG" YELLOW");

	/* Test red */
	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_blind_check(
		&sm, &sp, time, TM_TEST_SRTCM_EBS_DF + 1)
		!= e_RTE_METER_RED)
		melog(SRTCM_BLIND_CHECK_MSG" RED");

	return 0;

}

/**
 * functional test for rte_meter_trtcm_color_blind_check
 */
static inline int
tm_test_trtcm_color_blind_check(void)
{
#define TRTCM_BLIND_CHECK_MSG "trtcm_blind_check"

	uint64_t time;
	struct rte_meter_trtcm_profile tp;
	struct rte_meter_trtcm tm;
	uint64_t hz = rte_get_tsc_hz();

	/* Test green */
	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_blind_check(
		&tm, &tp, time, TM_TEST_TRTCM_CBS_DF - 1)
		!= e_RTE_METER_GREEN)
		melog(TRTCM_BLIND_CHECK_MSG" GREEN");

	/* Test yellow */
	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_blind_check(
		&tm, &tp, time, TM_TEST_TRTCM_CBS_DF + 1)
		!= e_RTE_METER_YELLOW)
		melog(TRTCM_BLIND_CHECK_MSG" YELLOW");

	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_blind_check(
		&tm, &tp, time, TM_TEST_TRTCM_PBS_DF - 1)
		!= e_RTE_METER_YELLOW)
		melog(TRTCM_BLIND_CHECK_MSG" YELLOW");

	/* Test red */
	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_BLIND_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_blind_check(
		&tm, &tp, time, TM_TEST_TRTCM_PBS_DF + 1)
		!= e_RTE_METER_RED)
		melog(TRTCM_BLIND_CHECK_MSG" RED");

	return 0;
}


/**
 * @in[4] : the flags packets carries.
 * @in[4] : the flags function expect to return.
 * It will do blind check at the time of 1 second from beginning.
 * At the time, it will use packets length of cbs -1, cbs + 1,
 * ebs -1 and ebs +1 with flag in[0], in[1], in[2] and in[3] to do
 * aware check, expect flag out[0], out[1], out[2] and out[3]
 */

static inline int
tm_test_srtcm_aware_check
(enum rte_meter_color in[4], enum rte_meter_color out[4])
{
#define SRTCM_AWARE_CHECK_MSG "srtcm_aware_check"
	struct rte_meter_srtcm_profile sp;
	struct rte_meter_srtcm sm;
	uint64_t time;
	uint64_t hz = rte_get_tsc_hz();

	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_aware_check(
		&sm, &sp, time, TM_TEST_SRTCM_CBS_DF - 1, in[0]) != out[0])
		melog(SRTCM_AWARE_CHECK_MSG" %u:%u", in[0], out[0]);

	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_aware_check(
		&sm, &sp, time, TM_TEST_SRTCM_CBS_DF + 1, in[1]) != out[1])
		melog(SRTCM_AWARE_CHECK_MSG" %u:%u", in[1], out[1]);

	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_aware_check(
		&sm, &sp, time, TM_TEST_SRTCM_EBS_DF - 1, in[2]) != out[2])
		melog(SRTCM_AWARE_CHECK_MSG" %u:%u", in[2], out[2]);

	if (rte_meter_srtcm_profile_config(&sp, &sparams) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	if (rte_meter_srtcm_config(&sm, &sp) != 0)
		melog(SRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_srtcm_color_aware_check(
		&sm, &sp, time, TM_TEST_SRTCM_EBS_DF + 1, in[3]) != out[3])
		melog(SRTCM_AWARE_CHECK_MSG" %u:%u", in[3], out[3]);

	return 0;
}


/**
 * functional test for rte_meter_srtcm_color_aware_check
 */
static inline int
tm_test_srtcm_color_aware_check(void)
{
	enum rte_meter_color in[4], out[4];

	/**
	  * test 4 points that will produce green, yellow, yellow, red flag
	  * if using blind check
	  */

	/* previouly have a green, test points should keep unchanged */
	in[0] = in[1] = in[2] = in[3] = e_RTE_METER_GREEN;
	out[0] = e_RTE_METER_GREEN;
	out[1] = e_RTE_METER_YELLOW;
	out[2] = e_RTE_METER_YELLOW;
	out[3] = e_RTE_METER_RED;
	if (tm_test_srtcm_aware_check(in, out) != 0)
		return -1;

	/**
	  * previously have a yellow, green & yellow = yellow
	  * yellow & red = red
	  */
	in[0] = in[1] = in[2] = in[3] = e_RTE_METER_YELLOW;
	out[0] = e_RTE_METER_YELLOW;
	out[1] = e_RTE_METER_YELLOW;
	out[2] = e_RTE_METER_YELLOW;
	out[3] = e_RTE_METER_RED;
	if (tm_test_srtcm_aware_check(in, out) != 0)
		return -1;

	/**
	  * previously have a red, red & green = red
	  * red & yellow = red
	  */
	in[0] = in[1] = in[2] = in[3] = e_RTE_METER_RED;
	out[0] = e_RTE_METER_RED;
	out[1] = e_RTE_METER_RED;
	out[2] = e_RTE_METER_RED;
	out[3] = e_RTE_METER_RED;
	if (tm_test_srtcm_aware_check(in, out) != 0)
		return -1;

	return 0;
}

/**
 * @in[4] : the flags packets carries.
 * @in[4] : the flags function expect to return.
 * It will do blind check at the time of 1 second from beginning.
 * At the time, it will use packets length of cbs -1, cbs + 1,
 * ebs -1 and ebs +1 with flag in[0], in[1], in[2] and in[3] to do
 * aware check, expect flag out[0], out[1], out[2] and out[3]
 */
static inline int
tm_test_trtcm_aware_check
(enum rte_meter_color in[4], enum rte_meter_color out[4])
{
#define TRTCM_AWARE_CHECK_MSG "trtcm_aware_check"
	struct rte_meter_trtcm_profile tp;
	struct rte_meter_trtcm tm;
	uint64_t time;
	uint64_t hz = rte_get_tsc_hz();

	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_aware_check(
		&tm, &tp, time, TM_TEST_TRTCM_CBS_DF - 1, in[0]) != out[0])
		melog(TRTCM_AWARE_CHECK_MSG" %u:%u", in[0], out[0]);

	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_aware_check(
		&tm, &tp, time, TM_TEST_TRTCM_CBS_DF + 1, in[1]) != out[1])
		melog(TRTCM_AWARE_CHECK_MSG" %u:%u", in[1], out[1]);

	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_aware_check(
		&tm, &tp, time, TM_TEST_TRTCM_PBS_DF - 1, in[2]) != out[2])
		melog(TRTCM_AWARE_CHECK_MSG" %u:%u", in[2], out[2]);

	if (rte_meter_trtcm_profile_config(&tp, &tparams) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	if (rte_meter_trtcm_config(&tm, &tp) != 0)
		melog(TRTCM_AWARE_CHECK_MSG);
	time = rte_get_tsc_cycles() + hz;
	if (rte_meter_trtcm_color_aware_check(
		&tm, &tp, time, TM_TEST_TRTCM_PBS_DF + 1, in[3]) != out[3])
		melog(TRTCM_AWARE_CHECK_MSG" %u:%u", in[3], out[3]);

	return 0;
}


/**
 * functional test for rte_meter_trtcm_color_aware_check
 */

static inline int
tm_test_trtcm_color_aware_check(void)
{
	enum rte_meter_color in[4], out[4];
	/**
	  * test 4 points that will produce green, yellow, yellow, red flag
	  * if using blind check
	  */

	/* previouly have a green, test points should keep unchanged */
	in[0] = in[1] = in[2] = in[3] = e_RTE_METER_GREEN;
	out[0] = e_RTE_METER_GREEN;
	out[1] = e_RTE_METER_YELLOW;
	out[2] = e_RTE_METER_YELLOW;
	out[3] = e_RTE_METER_RED;
	if (tm_test_trtcm_aware_check(in, out) != 0)
		return -1;

	in[0] = in[1] = in[2] = in[3] = e_RTE_METER_YELLOW;
	out[0] = e_RTE_METER_YELLOW;
	out[1] = e_RTE_METER_YELLOW;
	out[2] = e_RTE_METER_YELLOW;
	out[3] = e_RTE_METER_RED;
	if (tm_test_trtcm_aware_check(in, out) != 0)
		return -1;

	in[0] = in[1] = in[2] = in[3] = e_RTE_METER_RED;
	out[0] = e_RTE_METER_RED;
	out[1] = e_RTE_METER_RED;
	out[2] = e_RTE_METER_RED;
	out[3] = e_RTE_METER_RED;
	if (tm_test_trtcm_aware_check(in, out) != 0)
		return -1;

	return 0;
}

/**
 * test main entrance for library meter
 */
static int
test_meter(void)
{
	if (tm_test_srtcm_config() != 0)
		return -1;

	if (tm_test_trtcm_config() != 0)
		return -1;

	if (tm_test_srtcm_color_blind_check() != 0)
		return -1;

	if (tm_test_trtcm_color_blind_check() != 0)
		return -1;

	if (tm_test_srtcm_color_aware_check() != 0)
		return -1;

	if (tm_test_trtcm_color_aware_check() != 0)
		return -1;

	return 0;

}

REGISTER_TEST_COMMAND(meter_autotest, test_meter);
