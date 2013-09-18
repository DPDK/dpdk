/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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

#include <cmdline_parse.h>

#include <rte_string_fns.h>
#include <rte_mbuf.h>

#include "test.h"

#ifdef RTE_LIBRTE_PMAC

#include <rte_pm.h>

#include "test_pmac_pm.h"

struct pm_store_buf {
	void * buf;
	size_t len;
};

struct rte_pm_param good_param = {
	.name = "param",
	.socket_id = SOCKET_ID_ANY,
	.max_pattern_num = 0x20000,
	.max_pattern_len = 0x20000,
};

/* pattern set.
 * this switches between clean (ASCII-only), mixed (those needing conversion
 * from ASCII to binary) and P1 (a list of only 1 pattern, for use with P1
 * algorithm) pattern sets. each pattern set is attempted to be tested with all
 * algorithms, thus maximizing coverage.
 */
enum pattern_set {
	PAT_CLEAN,
	PAT_MIXED,
	PAT_P1,
	PAT_NUM
};


#define LEN 16
#define BUFSIZE 0x1000000

#define OPT_CASE_SENSE 0x1
#define OPT_OUT_OF_ORDER 0x2
#define NUM_OPTS 2

/* keep track of which algorithms were tested */
uint8_t tested_algorithms[RTE_PM_SEARCH_NUM];

/* pointer to an array with one of the test buffers from test_pmac_pm.h */
static struct pm_test_buffer * pm_test_buf;
/* length of the list (since we can't use sizeof on a pointer) */
int pm_test_buf_len = 0;


/* store pattern-match buffer */
static int
pm_store(void *arg, const void *buf,
		uint64_t offset, uint64_t size)
{
	struct pm_store_buf * dst = (struct pm_store_buf *) arg;
	if (size + offset > dst->len) {
		printf("Line %i: Not enough space in PM store buffer!\n", __LINE__);
		return -ENOMEM;
	}
	memcpy((char*) dst->buf + offset, buf, size);

	return (0);
}

/* load pattern-match buffer */
static int
pm_load(void *arg, void *buf,
		uint64_t offset, uint64_t size)
{
	struct pm_store_buf * src = (struct pm_store_buf *) arg;
	if (size + offset > src->len) {
		printf("Line %i: Not enough space in PM load buffer!\n", __LINE__);
		return -ENOMEM;
	}
	memcpy(buf, (char*) src->buf + offset, size);

	return (0);
}

/**
 * perform bulk search
 *
 * Due to the way bulk works, we can only look for <=1 results per buffer
 */
static int
bulk_search(struct rte_pm_ctx * pmx, struct rte_pm_build_opt * bopt)
{
	struct rte_pm_match res[pm_test_buf_len];
	struct rte_pm_inbuf in_buf[pm_test_buf_len];

	int i, len, tmp;
	int num_matches, total_matches;

	if (pm_test_buf_len <= 0) {
		printf("Line %i: Error at %s invalid value for "
			"pm_test_buf_len: %d\n",
				__LINE__, __func__, pm_test_buf_len);
		return (-1);
	}

	memset(res, 0, sizeof(res));
	memset(in_buf, 0, sizeof(in_buf));

	/* prepare buffers */
	for (i = 0; i < pm_test_buf_len; i++) {

		/* prepare PM buffer */
		len = strnlen(pm_test_buf[i].string, sizeof(pm_test_buf[i].string));

		in_buf[i].buf = (const uint8_t*) pm_test_buf[i].string;
		in_buf[i].len = len;
	}

	num_matches = 0;

	/* get number of total matches we're supposed to get */
	/* we can only get up to 1 results because of bulk search */
	if (bopt->case_sense) {
		for (i = 0; i < pm_test_buf_len; i++)
			num_matches += pm_test_buf[i].n_matches_with_case_sense > 0;
	}
	else {
		for (i = 0; i < pm_test_buf_len; i++)
			num_matches += pm_test_buf[i].n_matches > 0;
	}

	/* run bulk search */
	total_matches = rte_pm_search_bulk(pmx, in_buf, res, pm_test_buf_len);

	/* check if we have a different number of total matches */
	if (total_matches != num_matches) {
		rte_pm_dump(pmx);
		printf("Line %i: Error bulk matching (ret=%i num_matches=%i)!\n",
				__LINE__, total_matches, num_matches);
		return -1;
	}
	/* cycle through each result and check first match, if any */
	else {
		for (i = 0; i < pm_test_buf_len; i++) {

			/* get supposed number of matches */
			if (bopt->case_sense)
				tmp = pm_test_buf[i].n_matches_with_case_sense > 0;
			else
				tmp = pm_test_buf[i].n_matches > 0;

			/* check if we have a match when we shouldn't (and vice versa) */
			if (((const char *)(uintptr_t)res[i].userdata !=
					NULL) == (tmp == 0)) {
				printf("Line %i: Should have %i matches!\n", __LINE__, tmp);
				return -1;
			}

			/* skip null results */
			if (tmp == 0)
				continue;

			/* compare result string */
			if ((const char*)(uintptr_t)res[i].userdata !=
					pm_test_buf[i].matched_str[0]) {
				rte_pm_dump(pmx);
				printf("Line %i: Wrong match at bulk search %i!\n",
						__LINE__, i);
				printf("Matched: %s\n",
					(const char *)(uintptr_t)
					res[i].userdata);
				printf("Should have matched: %s\n",
						pm_test_buf[i].matched_str[0]);
				return -1;
			}
		}
	}
	return 0;
}

/**
 * perform multiple searches on a split single buffer
 */
static int
split_buffer_search(struct rte_pm_ctx * pmx, struct rte_pm_build_opt * bopt)
{
/* works with any reasonable segment count */
#define NUM_SEG 2
	struct rte_pm_match res_seg[NUM_SEG][MAX_MATCH_COUNT];
	struct rte_pm_inbuf in_seg[NUM_SEG];
	struct rte_pm_match * res;
	struct rte_pm_state state;

	int len, seg_len, total_len;
	int i, j, n_seg;
	int cur_match, num_matches, total_matches = 0;

	/* chain matching */
	for (i = 0; i < pm_test_buf_len; i++) {

		memset(res_seg, 0, sizeof(res_seg));
		memset(&state, 0, sizeof(struct rte_pm_state));

		/* prepare PM buffer */
		len = strnlen(pm_test_buf[i].string, sizeof(pm_test_buf[i].string));

		total_len = 0;

		/* create segments out of one string */
		for (n_seg = 0; n_seg < NUM_SEG; n_seg++) {
			/* if last segment */
			if (n_seg == NUM_SEG - 1)
				seg_len = len - total_len;
			else
				seg_len = len / NUM_SEG;
			in_seg[n_seg].len = seg_len;
			in_seg[n_seg].buf =
					(const uint8_t*) (pm_test_buf[i].string + total_len);
			total_len += seg_len;
		}


		/* number of matches we are supposed to find */
		if (bopt->case_sense)
			num_matches = pm_test_buf[i].n_matches_with_case_sense;
		else
			num_matches = pm_test_buf[i].n_matches;

		/* search in segments */
		for (n_seg = 0; n_seg < NUM_SEG; n_seg++) {
			/* if first segment */
			if (n_seg == 0)
				total_matches = rte_pm_search_chain_start(pmx, &in_seg[n_seg],
						res_seg[n_seg], MAX_MATCH_COUNT, &state);
			else
				total_matches += rte_pm_search_chain_next(pmx, &in_seg[n_seg],
						res_seg[n_seg], MAX_MATCH_COUNT - total_matches,
						&state);
		}

		if (total_matches != num_matches) {
			rte_pm_dump(pmx);
			printf("Line %i: Error matching %s%s (ret=%i num_matches=%i)!\n",
					__LINE__, in_seg[0].buf, in_seg[1].buf, total_matches,
					num_matches);
			return -1;
		}
		/* check if match was correct */
		else {
			cur_match = 0;
			for (j = 0; j < MAX_MATCH_COUNT * NUM_SEG; j++) {

				/* check if we have reached our maximum */
				if (cur_match == num_matches || cur_match == MAX_MATCH_COUNT)
					break;

				n_seg = j / MAX_MATCH_COUNT;

				/* get current result pointer */
				res = &res_seg[n_seg][j % MAX_MATCH_COUNT];

				/* skip uninitialized results */
				if (res->fin == 0)
					continue;

				/* compare result string */
				if ((const char*)(uintptr_t)res->userdata !=
						pm_test_buf[i].matched_str[cur_match]) {
					rte_pm_dump(pmx);
					printf("Line %i: Wrong match at split buffer search %i!\n",
							__LINE__, i);
					printf("Matched: %s\n",
						(const char *)(uintptr_t)
						res->userdata);
					printf("Should have matched: %s\n",
							pm_test_buf[i].matched_str[cur_match]);
					return -1;
				}
				/* we got ourselves a match! */
				else
					cur_match++;
			}
		}
	}
	return 0;
}

/**
 * perform multiple searches on a single buffer
 */
static int
single_buffer_search(struct rte_pm_ctx * pmx, struct rte_pm_build_opt * bopt)
{
	struct rte_pm_match res[MAX_MATCH_COUNT];
	struct rte_pm_state state;
	struct rte_pm_inbuf in_buf;

	int i, j, len;
	int match, num_matches, total_matches = 0;

	/* look at same segment three times */
	for (i = 0; i < pm_test_buf_len; i++) {

		memset(&res, 0, sizeof(res));
		memset(&state, 0, sizeof(struct rte_pm_state));

		/* prepare PM buffer */
		len = strnlen(pm_test_buf[i].string, sizeof(pm_test_buf[i].string));

		in_buf.buf = (const uint8_t*) pm_test_buf[i].string;
		in_buf.len = len;

		/* number of matches we are supposed to find */
		if (bopt->case_sense)
			num_matches = pm_test_buf[i].n_matches_with_case_sense;
		else
			num_matches = pm_test_buf[i].n_matches;

		/* run through a buffer multiple times, looking for 1 match */
		for (j = 0; j < MAX_MATCH_COUNT; j++) {
			/* start search chain */
			if (j == 0)
				total_matches = rte_pm_search_chain_start(pmx, &in_buf,
						&res[j], 1, &state);
			/* continue search */
			else
				total_matches += rte_pm_search_chain(pmx, &res[j], 1, &state);
		}

		if (total_matches != num_matches) {
			rte_pm_dump(pmx);
			printf("Line %i: Error matching %s (ret=%i num_matches=%i)!\n",
					__LINE__, in_buf.buf, total_matches, num_matches);
			return -1;
		}
		/* check if match was correct */
		else {
			for (match = 0; match < num_matches; match++) {
				if ((const char*)(uintptr_t)
						res[match].userdata !=
						pm_test_buf[i].matched_str[match]) {
					rte_pm_dump(pmx);
					printf("Line %i: Wrong match at single buffer search %i!\n",
							__LINE__, i);
					printf("Matched: %s\n",
						(const char *)(uintptr_t)
						res[match].userdata);
					printf("Should have matched: %s\n",
							pm_test_buf[i].matched_str[match]);
					return -1;
				}
			}
		}
	}

	return 0;
}

/*
 * perform basic searches
 */
static int
simple_search(struct rte_pm_ctx * pmx, struct rte_pm_build_opt * bopt)
{
	struct rte_pm_match res[MAX_MATCH_COUNT];
	struct rte_pm_state state;
	struct rte_pm_inbuf in_buf;

	int i, len, ret;
	int match, num_matches;

	/* simple matching */
	for (i = 0; i < pm_test_buf_len; i++) {

		memset(&res, 0, sizeof(res));
		memset(&state, 0, sizeof(struct rte_pm_state));

		/* prepare PM buffer */
		len = strnlen(pm_test_buf[i].string, sizeof(pm_test_buf[i].string));

		in_buf.buf = (const uint8_t*) pm_test_buf[i].string;
		in_buf.len = len;

		/* number of matches we are supposed to find */
		if (bopt->case_sense)
			num_matches = pm_test_buf[i].n_matches_with_case_sense;
		else
			num_matches = pm_test_buf[i].n_matches;

		ret = rte_pm_search_chain_start(pmx, &in_buf, res,
				MAX_MATCH_COUNT, &state);

		if (ret != num_matches) {
			rte_pm_dump(pmx);
			printf("Line %i: Error matching %s (ret=%i num_matches=%i)!\n",
					__LINE__, in_buf.buf, ret, num_matches);
			return -1;
		}
		/* check if match was correct */
		else {
			for (match = 0; match < num_matches; match++) {
				if ((const char *)(uintptr_t)
						res[match].userdata !=
						pm_test_buf[i].matched_str[match]) {
					rte_pm_dump(pmx);
					printf("Line %i: Wrong match at simple search %i!\n",
							__LINE__, i);
					printf("Matched: %s\n",
						(const char *)(uintptr_t)
						res[match].userdata);
					printf("Should have matched: %s\n",
							pm_test_buf[i].matched_str[match]);
					return -1;
				}
			}
		}
	}

	return 0;
}

/*
 * build PM context and call search function
 */
static int
build_and_search(struct rte_pm_ctx * pmx,
				 struct rte_pm_search_avail * avail,
				 struct rte_pm_build_opt * bopt, enum pattern_set p_set)
{
	struct rte_pm_param param;
	struct rte_pm_ctx * pmx2;
	struct pm_store_buf buffer;
	enum rte_pm_search search_type;
	int ret;

	/* allocate load/store buffer */
	if ((buffer.buf = malloc(BUFSIZE)) == NULL) {
		printf("%s at line %i: failed to allocate load/store buffer!\n",
			__func__, __LINE__);
		return (-1);
	}
	buffer.len = BUFSIZE;

	/* prepare data for second context */
	memcpy(&param, &good_param, sizeof(param));
	param.name = "pmx2";

	/* cycle through all search algorithms */
	for (search_type = RTE_PM_SEARCH_UNDEF; search_type < RTE_PM_SEARCH_NUM;
			search_type++) {

		/* skip unavailable search types, but include RTE_PM_SEARCH_UNDEF
		 * as it should work */
		if (search_type == RTE_PM_SEARCH_UNDEF ||
				RTE_PM_GET_BIT(avail->avail, search_type) > 0) {

			/* make a note that we tested this algorithm */
			tested_algorithms[search_type] = 1;

			/* build pm */
			bopt->search_type = search_type;

			ret = rte_pm_build(pmx, bopt);
			if (ret == -ENOTSUP) {
				printf("Line %i: Algorightm %s not supported.\n",
						__LINE__, rte_pm_search_names[search_type]);
				continue;
			}
			else if (ret != 0) {
				printf("Line %i: PM build for algorithm %s failed! "
						"Return code: %i\n",
						__LINE__, rte_pm_search_names[search_type], ret);
				goto err;
			}

			/* select which buffer list to process */
			switch (p_set)
			{
			case PAT_CLEAN:
				pm_test_buf = clean_buffers;
				pm_test_buf_len = DIM(clean_buffers);
				break;
			case PAT_MIXED:
				pm_test_buf = mixed_buffers;
				pm_test_buf_len = DIM(mixed_buffers);
				break;
			case PAT_P1:
				pm_test_buf = P1_buffers;
				pm_test_buf_len = DIM(P1_buffers);
				break;
			default:
				goto err;
			}

			/* do searches */
			if (simple_search(pmx, bopt) < 0)
				goto err;
			if (single_buffer_search(pmx, bopt) < 0)
				goto err;
			if (split_buffer_search(pmx, bopt) < 0)
				goto err;
			if (bulk_search(pmx, bopt) < 0)
				goto err;

			/* create second context and load it with data from pmx */
			pmx2 = rte_pm_create(&param);
			if (pmx2 == NULL) {
				printf("Line %i: Creating second context failed!\n", __LINE__);
				goto err;
			}

			/* clear load/store buffer, store pmx data and load into pmx2 */
			memset(buffer.buf, 0, BUFSIZE);

			ret = rte_pm_store(pmx, pm_store, &buffer);
			if (ret != 0) {
				printf("Line %i: PM store failed!\n", __LINE__);
				goto err_pmx2;
			}

			ret = rte_pm_load(pmx2, pm_load, &buffer);
			if (ret != 0) {
				printf("Line %i: PM load failed!\n", __LINE__);
				goto err_pmx2;
			}

			/* do searches for pmx2 */
			if (simple_search(pmx2, bopt) < 0)
				goto err_pmx2;
			if (single_buffer_search(pmx2, bopt) < 0)
				goto err_pmx2;
			if (split_buffer_search(pmx2, bopt) < 0)
				goto err_pmx2;
			if (bulk_search(pmx2, bopt) < 0)
				goto err_pmx2;

			/* free second context */
			rte_pm_free(pmx2);
		}
	}

	/* free load/store buffer */
	free(buffer.buf);

	return 0;
err_pmx2:
	rte_pm_free(pmx2);
err:
	free(buffer.buf);
	return -1;
}

/* add patterns to PM context */
static int
add_patterns(struct rte_pm_ctx * pmx, enum pattern_set p_set)
{
	int i, ret;
	struct rte_pm_pattern * pat = NULL;

	/* only needed when converting strings */
	uint8_t tmp_str[DIM(mixed_patterns)][MAX_PATTERN_LEN];

	switch (p_set)
	{
		case PAT_CLEAN:
		{
			/* allocate space for patterns */
			pat = malloc(sizeof(struct rte_pm_pattern) * DIM(clean_patterns));

			if (!pat) {
				printf("Line %i: Allocating space for patterns failed!\n",
						__LINE__);
				return -1;
			}

			for (i = 0; i < (int) DIM(clean_patterns); i++) {
				pat[i].pattern = (const uint8_t *) clean_patterns[i];
				pat[i].userdata = (uintptr_t) clean_patterns[i];
				pat[i].len = strnlen(clean_patterns[i],
						sizeof(clean_patterns[i]));
			}

			ret = rte_pm_add_patterns(pmx, pat, DIM(clean_patterns));

			if (ret != 0) {
				printf("Line %i: PM pattern add failed! Return code: %i\n",
						__LINE__, ret);
				free(pat);
				return -1;
			}
			free(pat);
			break;
		}
		case PAT_MIXED:
		{
			pat = NULL;

			pat = malloc(sizeof(struct rte_pm_pattern) * DIM(mixed_patterns));
			memset(tmp_str, 0, sizeof(tmp_str));

			if (!pat) {
				printf("Line %i: Allocating space for patterns failed!\n",
						__LINE__);
				if (pat)
					free(pat);
				return -1;
			}

			for (i = 0; i < (int) DIM(mixed_patterns); i++) {

				ret = rte_pm_convert_pattern(mixed_patterns[i],
						tmp_str[i], MAX_PATTERN_LEN);

				if (!ret) {
					printf("Line %i: Converting pattern failed!\n", __LINE__);
					free(pat);
					return -1;
				}
				pat[i].pattern = tmp_str[i];
				/* we assign original string here so that later comparison
				 * doesn't fail.
				 */
				pat[i].userdata = (uintptr_t) mixed_patterns[i];
				pat[i].len = strnlen((const char*) tmp_str[i], MAX_PATTERN_LEN);
			}

			ret = rte_pm_add_patterns(pmx, pat, DIM(mixed_patterns));

			if (ret != 0) {
				printf("Line %i: PM pattern add failed! Return code: %i\n",
						__LINE__, ret);
				free(pat);
				return -1;
			}
			free(pat);
			break;
		}
		case PAT_P1:
		{
			pat = malloc(sizeof(struct rte_pm_pattern) * DIM(P1_patterns));

			if (!pat) {
				printf("Line %i: Allocating space for patterns failed!\n",
						__LINE__);
				return -1;
			}

			for (i = 0; i < (int) DIM(P1_patterns); i++) {
				pat[i].pattern = (const uint8_t *) P1_patterns[i];
				pat[i].userdata = (uintptr_t) P1_patterns[i];
				pat[i].len = strnlen(P1_patterns[i], sizeof(P1_patterns[i]));
			}

			ret = rte_pm_add_patterns(pmx, pat, 1);

			if (ret != 0) {
				printf("Line %i: PM pattern add failed! Return code: %i\n",
						__LINE__, ret);
				free(pat);
				return -1;
			}
			free(pat);
			break;
		}
	default:
		printf("Line %i: Unknown pattern type\n", __LINE__);
		return -1;
	}
	return 0;
}

/*
 * this function is in no way a replacement for
 * proper in-depth unit tests (those are available
 * as a separate application). this is just a quick
 * sanity test to check basic functionality.
 */
static int
test_search_patterns(void)
{
	struct rte_pm_ctx * pmx;
	struct rte_pm_search_avail avail;
	struct rte_pm_build_opt bopt;
	int i, ret;

	/* bitmask to configure build options */
	uint8_t options_bm = 0;
	/* pattern set to use for tests */
	enum pattern_set p_set;

	/* reset the tested algorithms array */
	memset(tested_algorithms, 0, sizeof(tested_algorithms));

	/* two possible options: case sense and OOO */
	for (options_bm = 0; options_bm < (1 << NUM_OPTS); options_bm++) {

		bopt.search_type = RTE_PM_SEARCH_UNDEF;

		/* configure options according to bitmask */
		bopt.case_sense = !!(options_bm & OPT_CASE_SENSE);
		bopt.out_of_order = !!(options_bm & OPT_OUT_OF_ORDER);

		for (p_set = PAT_CLEAN; p_set < PAT_NUM; p_set++) {

			/* create new PM context */
			pmx = rte_pm_create(&good_param);
			if (pmx == NULL) {
				printf("Line %i: Failed to create PM context!\n", __LINE__);
				return -1;
			}

			/* add patterns to context */
			ret = add_patterns(pmx, p_set);
			if (ret < 0)
				goto err;

			ret = rte_pm_analyze(pmx, &bopt, &avail);
			if (ret != 0) {
				printf("Line %i: PM analyze failed! Return code: %i\n",
						__LINE__, ret);
				goto err;
			}

			ret = build_and_search(pmx, &avail, &bopt, p_set);
			if (ret < 0)
				goto err;

			rte_pm_free(pmx);
		}
	}

	ret = 0;

	/*
	 * check if all algorithms were attempted
	 */

	/* skip nil algorithm */
	for (i = 1; i < RTE_PM_SEARCH_NUM; i++) {
		if (tested_algorithms[i] == 0) {
			printf("Line %i: Algorithm %s was not tested!\n",
					__LINE__, rte_pm_search_names[i]);
			ret = -1;
		}
	}

	return ret;
err:
	rte_pm_free(pmx);
	return -1;
}


/*
 * Test creating and finding PM contexts, and adding patterns
 */
static int
test_create_find_add(void)
{
	struct rte_pm_param param;
	struct rte_pm_ctx * pmx, *pmx2, *tmp;
	struct rte_pm_pattern pat[LEN];
	const char * pmx_name = "pmx";
	const char * pmx2_name = "pmx2";
	int i, ret;

	/* create two contexts */
	memcpy(&param, &good_param, sizeof(param));
	param.max_pattern_len = 8;
	param.max_pattern_num = 2;

	param.name = pmx_name;
	pmx = rte_pm_create(&param);
	if (pmx == NULL) {
		printf("Line %i: Error creating %s!\n", __LINE__, pmx_name);
		return -1;
	}

	param.name = pmx2_name;
	pmx2 = rte_pm_create(&param);
	if (pmx2 == NULL || pmx2 == pmx) {
		printf("Line %i: Error creating %s!\n", __LINE__, pmx2_name);
		rte_pm_free(pmx);
		return -1;
	}

	/* try to create third one, with an existing name */
	param.name = pmx_name;
	tmp = rte_pm_create(&param);
	if (tmp != pmx) {
		printf("Line %i: Creating context with existing name test failed!\n",
				__LINE__);
		if (tmp)
			rte_pm_free(tmp);
		goto err;
	}

	param.name = pmx2_name;
	tmp = rte_pm_create(&param);
	if (tmp != pmx2) {
		printf("Line %i: Creating context with existing name test 2 failed!\n",
				__LINE__);
		if (tmp)
			rte_pm_free(tmp);
		goto err;
	}

	/* try to find existing PM contexts */
	tmp = rte_pm_find_existing(pmx_name);
	if (tmp != pmx) {
		printf("Line %i: Finding %s failed!\n", __LINE__, pmx_name);
		if (tmp)
			rte_pm_free(tmp);
		goto err;
	}

	tmp = rte_pm_find_existing(pmx2_name);
	if (tmp != pmx2) {
		printf("Line %i: Finding %s failed!\n", __LINE__, pmx2_name);
		if (tmp)
			rte_pm_free(tmp);
		goto err;
	}

	/* try to find non-existing context */
	tmp = rte_pm_find_existing("invalid");
	if (tmp != NULL) {
		printf("Line %i: Non-existent PM context found!\n", __LINE__);
		goto err;
	}

	rte_pm_free(pmx);


	/* create valid (but severely limited) pmx */
	memcpy(&param, &good_param, sizeof(param));
	param.max_pattern_num = LEN;

	pmx = rte_pm_create(&param);
	if (pmx == NULL) {
		printf("Line %i: Error creating %s!\n", __LINE__, param.name);
		goto err;
	}

	/* create dummy patterns */
	for (i = 0; i < LEN; i++) {
		pat[i].len = 4;
		pat[i].userdata = 0;
		pat[i].pattern = (const uint8_t*)"1234";
	}

	/* try filling up the context */
	ret = rte_pm_add_patterns(pmx, pat, LEN);
	if (ret != 0) {
		printf("Line %i: Adding %i patterns to PM context failed!\n",
				__LINE__, LEN);
		goto err;
	}

	/* try adding to a (supposedly) full context */
	ret = rte_pm_add_patterns(pmx, pat, 1);
	if (ret == 0) {
		printf("Line %i: Adding patterns to full PM context "
				"should have failed!\n", __LINE__);
		goto err;
	}

	rte_pm_free(pmx);

	/* create another valid (but severely limited) pmx */
	memcpy(&param, &good_param, sizeof(param));
	param.max_pattern_len = LEN * 4;

	pmx = rte_pm_create(&param);
	if (pmx == NULL) {
		printf("Line %i: Error creating %s!\n", __LINE__, param.name);
		goto err;
	}

	/* create dummy patterns */
	for (i = 0; i < LEN; i++) {
		pat[i].len = 4;
		pat[i].userdata = 0;
		pat[i].pattern = (const uint8_t*)"1234";
	}

	/* try filling up the context */
	ret = rte_pm_add_patterns(pmx, pat, LEN);
	if (ret != 0) {
		printf("Line %i: Adding %i patterns to PM context failed!\n",
				__LINE__, LEN);
		goto err;
	}

	/* try adding to a (supposedly) full context */
	ret = rte_pm_add_patterns(pmx, pat, 1);
	if (ret == 0) {
		printf("Line %i: Adding patterns to full PM context "
				"should have failed!\n", __LINE__);
		goto err;
	}

	rte_pm_free(pmx);
	rte_pm_free(pmx2);

	return 0;
err:
	rte_pm_free(pmx);
	rte_pm_free(pmx2);
	return -1;
}

/*
 * test serialization functions.
 * tests include:
 * - passing invalid parameters to function
 * - trying to load invalid pm store
 * - save buffer, load buffer, save another, and compare them
 * - try to load/save with a too small buffer (pm_store/pm_load should fail)
 */
static int
test_serialize(void)
{
	struct rte_pm_ctx * pmx, * pmx2;
	struct rte_pm_build_opt build_opt;
	struct rte_pm_pattern pat[LEN];
	int i, res;
	struct pm_store_buf buffer, buffer2;

	memset(&buffer, 0, sizeof (buffer));
	memset(&buffer2, 0, sizeof (buffer2));

	/* allocate two load/store buffers */
	if ((buffer.buf = malloc(BUFSIZE)) == NULL ||
		(buffer2.buf = malloc(BUFSIZE)) == NULL) {
		printf("Line %i: Creating load/store buffers failed!\n",
			__LINE__);
		free(buffer2.buf);
		free(buffer.buf);
		return (-1);
	}

	buffer.len = BUFSIZE;
	memset(buffer.buf, 0, BUFSIZE);

	buffer2.len = BUFSIZE;
	memset(buffer2.buf, 0, BUFSIZE);

	/* create a context */
	pmx = rte_pm_create(&good_param);
	if (!pmx) {
		printf("Line %i: Creating pmx failed!\n", __LINE__);
		goto err;
	}

	/* create dummy patterns */
	for (i = 0; i < LEN; i++) {
			pat[i].len = 4;
			pat[i].userdata = 0;
			pat[i].pattern = (const uint8_t*)"1234";
	}

	/* fill context with patterns */
	res = rte_pm_add_patterns(pmx, pat, LEN);
	if (res != 0) {
		printf("Line %i: Adding patterns to PM context failed!\n", __LINE__);
		goto err;
	}

	build_opt.search_type = RTE_PM_SEARCH_UNDEF;

	/* build the patterns */
	res = rte_pm_build(pmx, &build_opt);
	if (res != 0) {
		printf("Line %i: Building PM context failed!\n", __LINE__);
		goto err;
	}

	/**
	 * test serialize functions
	 */
	res = rte_pm_store(NULL, pm_store, &buffer);
	if (res != -EINVAL) {
		printf("Line %i: PM store should have failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_store(pmx, NULL, &buffer);
	if (res != -EINVAL) {
		printf("Line %i: PM store should have failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_store(pmx, pm_store, NULL);
	if (res != -EINVAL) {
		printf("Line %i: PM store should have failed!\n", __LINE__);
		goto err;
	}


	res = rte_pm_load(NULL, pm_load, &buffer);
	if (res != -EINVAL) {
		printf("Line %i: PM load should have failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_load(pmx, NULL, &buffer);
	if (res != -EINVAL) {
		printf("Line %i: PM load should have failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_load(pmx, pm_load, NULL);
	if (res != -EINVAL) {
		printf("Line %i: PM load should have failed!\n", __LINE__);
		goto err;
	}

	/* since buffer is currently zeroed out, load should complain about
	 * unsupported format*/
	res = rte_pm_load(pmx, pm_load, &buffer);
	if (res != -EINVAL) {
		printf("Line %i: PM load should have failed!\n", __LINE__);
		goto err;
	}

	/* rte_pm_load zeroed out the context, so re-add all patterns, rebuild,
	 * save the context to buffer and free context
	 */
	rte_pm_free(pmx);

	/* create a context */
	pmx = rte_pm_create(&good_param);
	if (!pmx) {
		printf("Line %i: Creating pmx failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_add_patterns(pmx, pat, LEN);
	if (res != 0) {
		printf("Line %i: Adding patterns to PM context failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_build(pmx, &build_opt);
	if (res != 0) {
		printf("Line %i: Building PM context failed!\n", __LINE__);
		goto err;
	}

	res = rte_pm_store(pmx, pm_store, &buffer);
	if (res != 0) {
		printf("Line %i: PM store failed!\n", __LINE__);
		goto err;
	}

	rte_pm_free(pmx);
	pmx = NULL;



	/* create pmx2 */
	pmx2 = rte_pm_create(&good_param);
	if (!pmx2) {
		printf("Line %i: Creating pmx2 failed!\n", __LINE__);
		goto err;
	}

	/* load buffer into pmx2 */
	res = rte_pm_load(pmx2, pm_load, &buffer);
	if (res != 0) {
		printf("Line %i: PM load failed!\n", __LINE__);
		goto err_pmx2;
	}

	/* save pmx2 into another buffer */
	res = rte_pm_store(pmx2, pm_store, &buffer2);
	if (res != 0) {
		printf("Line %i: PM store failed!\n", __LINE__);
		goto err_pmx2;
	}

	/* compare two buffers */
	if (memcmp(buffer.buf, buffer2.buf, BUFSIZE) != 0) {
		printf("Line %i: Buffers are different!\n", __LINE__);
		goto err_pmx2;
	}

	/* try and save pmx2 into a too small buffer */
	buffer2.len = 4;
	res = rte_pm_store(pmx2, pm_store, &buffer2);
	if (res != -ENOMEM) {
		printf("Line %i: PM store should have failed!\n", __LINE__);
		goto err_pmx2;
	}

	/* try and load from a too small buffer */
	res = rte_pm_load(pmx2, pm_load, &buffer2);
	if (res != -ENOMEM) {
		printf("Line %i: PM load should have failed!\n", __LINE__);
		goto err_pmx2;
	}

	/* free everything */
	rte_pm_free(pmx2);

	free(buffer2.buf);
	free(buffer.buf);

	return 0;
err_pmx2:
	rte_pm_free(pmx2);
err:
	rte_pm_free(pmx);
	free(buffer2.buf);
	free(buffer.buf);
	return -1;
}

/*
 * test functions by passing invalid or
 * non-workable parameters.
 *
 * we do NOT test pattern search functions here
 * because those are performance-critical and
 * thus don't do any parameter checking.
 */
static int
test_invalid_parameters(void)
{
	struct rte_pm_param param;
	struct rte_pm_ctx * pmx;
	enum rte_pm_search search_result;
	int res = 0;
	/* needed for rte_pm_convert_pattern */
	char in_buf[LEN];
	uint8_t out_buf[LEN];
	/* needed for rte_pm_add_patterns */
	struct rte_pm_pattern pat;
	/* needed for rte_pm_analyze */
	struct rte_pm_search_avail build_res[LEN];
	/* needed for rte_pm_build */
	struct rte_pm_build_opt build_opt;


	/**
	 * rte_pm_create()
	 */

	/* NULL param */
	pmx = rte_pm_create(NULL);
	if (pmx != NULL) {
		printf("Line %i: PM context creation with NULL param "
				"should have failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/* zero pattern len */
	memcpy(&param, &good_param, sizeof(param));
	param.max_pattern_len = 0;

	pmx = rte_pm_create(&param);
	if (pmx == NULL) {
		printf("Line %i: PM context creation with zero pattern len failed!\n",
				__LINE__);
		return -1;
	}
	else
		rte_pm_free(pmx);

	/* zero pattern num */
	memcpy(&param, &good_param, sizeof(param));
	param.max_pattern_num = 0;

	pmx = rte_pm_create(&param);
	if (pmx == NULL) {
		printf("Line %i: PM context creation with zero pattern num failed!\n",
				__LINE__);
		return -1;
	}
	else
		rte_pm_free(pmx);

	/* invalid NUMA node */
	memcpy(&param, &good_param, sizeof(param));
	param.socket_id = RTE_MAX_NUMA_NODES + 1;

	pmx = rte_pm_create(&param);
	if (pmx != NULL) {
		printf("Line %i: PM context creation with invalid NUMA "
				"should have failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/* NULL name */
	memcpy(&param, &good_param, sizeof(param));
	param.name = NULL;

	pmx = rte_pm_create(&param);
	if (pmx != NULL) {
		printf("Line %i: PM context creation with NULL name "
				"should have failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/**
	 * rte_pm_search_type_by_name()
	 */

	/* invalid algorithm names */
	search_result = rte_pm_search_type_by_name("invalid");
	if (search_result != RTE_PM_SEARCH_UNDEF) {
		printf("Line %i: Found invalid PM algorithm!\n", __LINE__);
	}

	search_result = rte_pm_search_type_by_name(NULL);
	if (search_result != RTE_PM_SEARCH_UNDEF) {
		printf("Line %i: Found NULL PM algorithm!\n", __LINE__);
	}

	/**
	 * rte_pm_convert_pattern()
	 */

	/* null in buffer */
	res = rte_pm_convert_pattern(NULL, out_buf, sizeof(out_buf));
	if (res != (-EINVAL)) {
		printf("Line %i: Converting a NULL input pattern "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/* null out buffer */
	res = rte_pm_convert_pattern(in_buf, NULL, sizeof(out_buf));
	if (res != (-EINVAL)) {
		printf("Line %i: Converting to NULL output buffer "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/* zero length (should throw -ENOMEM) */
	res = rte_pm_convert_pattern(in_buf, out_buf, 0);
	if (res != -(ENOMEM)) {
		printf("Line %i: Converting to a 0-length output buffer "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/* wrong binary value */
	res = rte_pm_convert_pattern("|1", out_buf, sizeof(out_buf));
	if (res != (-EINVAL)) {
		printf("Line %i: Converting malformed binary "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/* wrong binary value */
	res = rte_pm_convert_pattern("|P1|", out_buf, sizeof(out_buf));
	if (res != (-EINVAL)) {
		printf("Line %i: Converting malformed binary "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/* wrong binary value */
	res = rte_pm_convert_pattern("|FFF|", out_buf, sizeof(out_buf));
	if (res != (-EINVAL)) {
		printf("Line %i: Converting malformed binary "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/**
	 * rte_pm_add_patterns()
	 */
	/* create valid pmx since we'll need it for tests */
	memcpy(&param, &good_param, sizeof(param));

	param.max_pattern_len = 2;

	pmx = rte_pm_create(&param);
	if (!pmx) {
		printf("Line %i: Creating pmx failed!\n", __LINE__);
		return -1;
	}

	/* NULL pmx */
	res = rte_pm_add_patterns(NULL, &pat, LEN);
	if (res != -EINVAL) {
		printf("Line %i: Adding patterns to NULL PM context "
				"should have failed!\n", __LINE__);
		return -1;
	}

	/* NULL pat */
	res = rte_pm_add_patterns(pmx, NULL, LEN);
	if (res != -EINVAL) {
		printf("Line %i: Adding patterns to NULL pattern "
				"should have failed!\n", __LINE__);
		return -1;
	}

	pat.len = 4;

	/* zero len (should succeed) */
	res = rte_pm_add_patterns(pmx, &pat, 0);
	if (res != 0) {
		printf("Line %i: Adding 0 patterns to PM context failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/**
	 * rte_pm_analyze()
	 */

	/* NULL context */
	res = rte_pm_analyze(NULL, &build_opt, build_res);
	if (res != -EINVAL) {
		printf("Line %i: PM analyze on NULL pmx "
				"should have failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/* NULL opt */
	res = rte_pm_analyze(pmx, NULL, build_res);
	if (res != -EINVAL) {
		printf("Line %i: PM analyze on NULL opt "
				"should have failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/* NULL res */
	res = rte_pm_analyze(pmx, &build_opt, NULL);
	if (res != -EINVAL) {
		printf("Line %i: PM analyze on NULL res should have failed!\n",
				__LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/**
	 * rte_pm_build()
	 */

	/* NULL context */
	res = rte_pm_build(NULL, &build_opt);
	if (res != -EINVAL) {
		printf("Line %i: PM build on NULL pmx should have failed!\n",
				__LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/* NULL opt */
	res = rte_pm_build(pmx, NULL);
	if (res != -EINVAL) {
		printf("Line %i: PM build on NULL opt should have failed!\n", __LINE__);
		rte_pm_free(pmx);
		return -1;
	}

	/* build with unsuitable algorithm */
	build_opt.case_sense = 0;
	build_opt.out_of_order = 0;
	/* MB expects out_of_order */
	build_opt.search_type = RTE_PM_SEARCH_AC2_L1x4_MB;

	res = rte_pm_build(pmx, &build_opt);
	if (res != -EINVAL) {
		printf("Line %i: PM build on NULL opt should have failed! %i\n",
				__LINE__, res);
		rte_pm_free(pmx);
		return -1;
	}

	/* free context */
	rte_pm_free(pmx);

	/**
	 * make sure void functions don't crash with NULL parameters
	 */

	rte_pm_free(NULL);

	rte_pm_dump(NULL);

	return 0;
}


/**
 * Various tests that don't test much but improve coverage
 */
static int
test_misc(void)
{
	struct rte_pm_build_opt build_opt;
	struct rte_pm_pattern pat;
	struct rte_pm_ctx * pmx;
	enum rte_pm_search search_result;
	char buf[MAX_PATTERN_LEN];
	int ret;

	pmx = NULL;

	/* search for existing PM algorithm */
	search_result = rte_pm_search_type_by_name("AC2_L1x4");
	if (search_result != RTE_PM_SEARCH_AC2_L1x4) {
		printf("Line %i: Wrong PM algorithm found!\n", __LINE__);
	}

	pmx = rte_pm_create(&good_param);
	if (!pmx) {
		printf("Line %i: Failed to create PM context!\n", __LINE__);
		return -1;
	}

	/* convert a pattern and add it to context */
	ret = rte_pm_convert_pattern("|01 02 03 04| readable", (uint8_t*) buf,
			sizeof(buf));
	if (ret <= 0) {
		rte_pm_free(pmx);
		printf("Line %i: Converting binary failed!\n", __LINE__);
		return -1;
	}

	/* add pattern to context */
	pat.len = (uint32_t) ret;
	pat.pattern = (const uint8_t *) buf;
	pat.userdata = 0;

	ret = rte_pm_add_patterns(pmx, &pat, 1);
	if (ret != 0) {
		rte_pm_free(pmx);
		printf("Line %i: Adding pattern failed!\n", __LINE__);
		return -1;
	}

	/* convert another pattern and add it to context */
	ret = rte_pm_convert_pattern("pattern", (uint8_t*) buf, 4);

	if (ret <= 0) {
		rte_pm_free(pmx);
		printf("Line %i: Converting pattern failed!\n", __LINE__);
		return -1;
	}

	/* add pattern to context */
	pat.len = (uint32_t) ret;
	pat.pattern = (const uint8_t *) buf;
	pat.userdata = 0;

	ret = rte_pm_add_patterns(pmx, &pat, 1);
	if (ret != 0) {
		rte_pm_free(pmx);
		printf("Line %i: Adding pattern failed!\n", __LINE__);
		return -1;
	}

	build_opt.case_sense = 0;
	build_opt.out_of_order = 0;
	build_opt.search_type = RTE_PM_SEARCH_AC2_L1x4;

	ret = rte_pm_build(pmx, &build_opt);
	if (ret != 0) {
		rte_pm_free(pmx);
		printf("Line %i: Building PM failed!\n", __LINE__);
		return -1;
	}

	/* dump context with patterns - useful for coverage */
	rte_pm_list_dump();

	rte_pm_dump(pmx);

	rte_pm_free(pmx);

	return 0;
}

int
test_pmac_pm(void)
{
	if (test_invalid_parameters() < 0)
		return -1;
	if (test_serialize() < 0)
		return -1;
	if (test_create_find_add() < 0)
		return -1;
	if (test_search_patterns() < 0)
		return -1;
	if (test_misc() < 0)
		return -1;
	return 0;
}

#else /* RTE_LIBRTE_PMAC=n */

int
test_pmac_pm(void)
{
	printf("This binary was not compiled with PMAC support!\n");
	return 0;
}

#endif /* RTE_LIBRTE_PMAC */
