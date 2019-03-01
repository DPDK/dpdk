/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>

#include <rte_string_fns.h>
#include <rte_comp.h>

#include "comp_perf_options.h"

#define CPERF_DRIVER_NAME	("driver-name")
#define CPERF_TEST_FILE		("input-file")
#define CPERF_SEG_SIZE		("seg-sz")
#define CPERF_BURST_SIZE	("burst-sz")
#define CPERF_EXTENDED_SIZE	("extended-input-sz")
#define CPERF_POOL_SIZE		("pool-sz")
#define CPERF_MAX_SGL_SEGS	("max-num-sgl-segs")
#define CPERF_NUM_ITER		("num-iter")
#define CPERF_OPTYPE		("operation")
#define CPERF_HUFFMAN_ENC	("huffman-enc")
#define CPERF_LEVEL		("compress-level")
#define CPERF_WINDOW_SIZE	("window-sz")

struct name_id_map {
	const char *name;
	uint32_t id;
};

static void
usage(char *progname)
{
	printf("%s [EAL options] --\n"
		" --driver-name NAME: compress driver to use\n"
		" --input-file NAME: file to compress and decompress\n"
		" --extended-input-sz N: extend file data up to this size (default: no extension)\n"
		" --seg-sz N: size of segment to store the data (default: 2048)\n"
		" --burst-sz N: compress operation burst size\n"
		" --pool-sz N: mempool size for compress operations/mbufs\n"
		"		(default: 8192)\n"
		" --max-num-sgl-segs N: maximum number of segments for each mbuf\n"
		"		(default: 16)\n"
		" --num-iter N: number of times the file will be\n"
		"		compressed/decompressed (default: 10000)\n"
		" --operation [comp/decomp/comp_and_decomp]: perform test on\n"
		"		compression, decompression or both operations\n"
		" --huffman-enc [fixed/dynamic/default]: Huffman encoding\n"
		"		(default: dynamic)\n"
		" --compress-level N: compression level, which could be a single value, list or range\n"
		"		(default: range between 1 and 9)\n"
		" --window-sz N: base two log value of compression window size\n"
		"		(e.g.: 15 => 32k, default: max supported by PMD)\n"
		" -h: prints this help\n",
		progname);
}

static int
get_str_key_id_mapping(struct name_id_map *map, unsigned int map_len,
		const char *str_key)
{
	unsigned int i;

	for (i = 0; i < map_len; i++) {

		if (strcmp(str_key, map[i].name) == 0)
			return map[i].id;
	}

	return -1;
}

static int
parse_uint32_t(uint32_t *value, const char *arg)
{
	char *end = NULL;
	unsigned long n = strtoul(arg, &end, 10);

	if ((optarg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (n > UINT32_MAX)
		return -ERANGE;

	*value = (uint32_t) n;

	return 0;
}

static int
parse_uint16_t(uint16_t *value, const char *arg)
{
	uint32_t val = 0;
	int ret = parse_uint32_t(&val, arg);

	if (ret < 0)
		return ret;

	if (val > UINT16_MAX)
		return -ERANGE;

	*value = (uint16_t) val;

	return 0;
}

static int
parse_range(const char *arg, uint8_t *min, uint8_t *max, uint8_t *inc)
{
	char *token;
	uint8_t number;

	char *copy_arg = strdup(arg);

	if (copy_arg == NULL)
		return -1;

	errno = 0;
	token = strtok(copy_arg, ":");

	/* Parse minimum value */
	if (token != NULL) {
		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE)
			goto err_range;

		*min = number;
	} else
		goto err_range;

	token = strtok(NULL, ":");

	/* Parse increment value */
	if (token != NULL) {
		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE ||
				number == 0)
			goto err_range;

		*inc = number;
	} else
		goto err_range;

	token = strtok(NULL, ":");

	/* Parse maximum value */
	if (token != NULL) {
		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE ||
				number < *min)
			goto err_range;

		*max = number;
	} else
		goto err_range;

	if (strtok(NULL, ":") != NULL)
		goto err_range;

	free(copy_arg);
	return 0;

err_range:
	free(copy_arg);
	return -1;
}

static int
parse_list(const char *arg, uint8_t *list, uint8_t *min, uint8_t *max)
{
	char *token;
	uint32_t number;
	uint8_t count = 0;
	uint32_t temp_min;
	uint32_t temp_max;

	char *copy_arg = strdup(arg);

	if (copy_arg == NULL)
		return -1;

	errno = 0;
	token = strtok(copy_arg, ",");

	/* Parse first value */
	if (token != NULL) {
		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE)
			goto err_list;

		list[count++] = number;
		temp_min = number;
		temp_max = number;
	} else
		goto err_list;

	token = strtok(NULL, ",");

	while (token != NULL) {
		if (count == MAX_LIST) {
			RTE_LOG(WARNING, USER1,
				"Using only the first %u sizes\n",
					MAX_LIST);
			break;
		}

		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE)
			goto err_list;

		list[count++] = number;

		if (number < temp_min)
			temp_min = number;
		if (number > temp_max)
			temp_max = number;

		token = strtok(NULL, ",");
	}

	if (min)
		*min = temp_min;
	if (max)
		*max = temp_max;

	free(copy_arg);
	return count;

err_list:
	free(copy_arg);
	return -1;
}

static int
parse_num_iter(struct comp_test_data *test_data, const char *arg)
{
	int ret = parse_uint32_t(&test_data->num_iter, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to parse total iteration count\n");
		return -1;
	}

	if (test_data->num_iter == 0) {
		RTE_LOG(ERR, USER1,
				"Total number of iterations must be higher than 0\n");
		return -1;
	}

	return ret;
}

static int
parse_pool_sz(struct comp_test_data *test_data, const char *arg)
{
	int ret = parse_uint32_t(&test_data->pool_sz, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to parse pool size");
		return -1;
	}

	if (test_data->pool_sz == 0) {
		RTE_LOG(ERR, USER1, "Pool size must be higher than 0\n");
		return -1;
	}

	return ret;
}

static int
parse_burst_sz(struct comp_test_data *test_data, const char *arg)
{
	int ret = parse_uint16_t(&test_data->burst_sz, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to parse burst size/s\n");
		return -1;
	}

	if (test_data->burst_sz == 0) {
		RTE_LOG(ERR, USER1, "Burst size must be higher than 0\n");
		return -1;
	}

	return 0;
}

static int
parse_extended_input_sz(struct comp_test_data *test_data, const char *arg)
{
	uint32_t tmp;
	int ret = parse_uint32_t(&tmp, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to parse extended input size\n");
		return -1;
	}
	test_data->input_data_sz = tmp;

	if (tmp == 0) {
		RTE_LOG(ERR, USER1,
			"Extended file size must be higher than 0\n");
		return -1;
	}
	return 0;
}

static int
parse_seg_sz(struct comp_test_data *test_data, const char *arg)
{
	int ret = parse_uint16_t(&test_data->seg_sz, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to parse segment size\n");
		return -1;
	}

	if (test_data->seg_sz < MIN_COMPRESSED_BUF_SIZE) {
		RTE_LOG(ERR, USER1, "Segment size must be higher than %d\n",
			MIN_COMPRESSED_BUF_SIZE - 1);
		return -1;
	}

	if (test_data->seg_sz > MAX_SEG_SIZE) {
		RTE_LOG(ERR, USER1, "Segment size must be lower than %d\n",
			MAX_SEG_SIZE + 1);
		return -1;
	}

	return 0;
}

static int
parse_max_num_sgl_segs(struct comp_test_data *test_data, const char *arg)
{
	int ret = parse_uint16_t(&test_data->max_sgl_segs, arg);

	if (ret) {
		RTE_LOG(ERR, USER1,
			"Failed to parse max number of segments per mbuf chain\n");
		return -1;
	}

	if (test_data->max_sgl_segs == 0) {
		RTE_LOG(ERR, USER1, "Max number of segments per mbuf chain "
			"must be higher than 0\n");
		return -1;
	}

	return 0;
}

static int
parse_window_sz(struct comp_test_data *test_data, const char *arg)
{
	int ret = parse_uint16_t((uint16_t *)&test_data->window_sz, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "Failed to parse window size\n");
		return -1;
	}

	return 0;
}

static int
parse_driver_name(struct comp_test_data *test_data, const char *arg)
{
	if (strlen(arg) > (sizeof(test_data->driver_name) - 1))
		return -1;

	strlcpy(test_data->driver_name, arg,
			sizeof(test_data->driver_name));

	return 0;
}

static int
parse_test_file(struct comp_test_data *test_data, const char *arg)
{
	if (strlen(arg) > (sizeof(test_data->input_file) - 1))
		return -1;

	strlcpy(test_data->input_file, arg, sizeof(test_data->input_file));

	return 0;
}

static int
parse_op_type(struct comp_test_data *test_data, const char *arg)
{
	struct name_id_map optype_namemap[] = {
		{
			"comp",
			COMPRESS_ONLY
		},
		{
			"decomp",
			DECOMPRESS_ONLY
		},
		{
			"comp_and_decomp",
			COMPRESS_DECOMPRESS
		}
	};

	int id = get_str_key_id_mapping(optype_namemap,
			RTE_DIM(optype_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "Invalid operation type specified\n");
		return -1;
	}

	test_data->test_op = (enum comp_operation)id;

	return 0;
}

static int
parse_huffman_enc(struct comp_test_data *test_data, const char *arg)
{
	struct name_id_map huffman_namemap[] = {
		{
			"default",
			RTE_COMP_HUFFMAN_DEFAULT
		},
		{
			"fixed",
			RTE_COMP_HUFFMAN_FIXED
		},
		{
			"dynamic",
			RTE_COMP_HUFFMAN_DYNAMIC
		}
	};

	int id = get_str_key_id_mapping(huffman_namemap,
			RTE_DIM(huffman_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "Invalid Huffmane encoding specified\n");
		return -1;
	}

	test_data->huffman_enc = (enum rte_comp_huffman)id;

	return 0;
}

static int
parse_level(struct comp_test_data *test_data, const char *arg)
{
	int ret;

	/*
	 * Try parsing the argument as a range, if it fails,
	 * arse it as a list
	 */
	if (parse_range(arg, &test_data->level.min, &test_data->level.max,
			&test_data->level.inc) < 0) {
		ret = parse_list(arg, test_data->level.list,
					&test_data->level.min,
					&test_data->level.max);
		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Failed to parse compression level/s\n");
			return -1;
		}
		test_data->level.count = ret;

		if (test_data->level.max > RTE_COMP_LEVEL_MAX) {
			RTE_LOG(ERR, USER1, "Level cannot be higher than %u\n",
					RTE_COMP_LEVEL_MAX);
			return -1;
		}
	}

	return 0;
}

typedef int (*option_parser_t)(struct comp_test_data *test_data,
		const char *arg);

struct long_opt_parser {
	const char *lgopt_name;
	option_parser_t parser_fn;

};

static struct option lgopts[] = {

	{ CPERF_DRIVER_NAME, required_argument, 0, 0 },
	{ CPERF_TEST_FILE, required_argument, 0, 0 },
	{ CPERF_SEG_SIZE, required_argument, 0, 0 },
	{ CPERF_BURST_SIZE, required_argument, 0, 0 },
	{ CPERF_EXTENDED_SIZE, required_argument, 0, 0 },
	{ CPERF_POOL_SIZE, required_argument, 0, 0 },
	{ CPERF_MAX_SGL_SEGS, required_argument, 0, 0},
	{ CPERF_NUM_ITER, required_argument, 0, 0 },
	{ CPERF_OPTYPE,	required_argument, 0, 0 },
	{ CPERF_HUFFMAN_ENC, required_argument, 0, 0 },
	{ CPERF_LEVEL, required_argument, 0, 0 },
	{ CPERF_WINDOW_SIZE, required_argument, 0, 0 },
	{ NULL, 0, 0, 0 }
};
static int
comp_perf_opts_parse_long(int opt_idx, struct comp_test_data *test_data)
{
	struct long_opt_parser parsermap[] = {
		{ CPERF_DRIVER_NAME,	parse_driver_name },
		{ CPERF_TEST_FILE,	parse_test_file },
		{ CPERF_SEG_SIZE,	parse_seg_sz },
		{ CPERF_BURST_SIZE,	parse_burst_sz },
		{ CPERF_EXTENDED_SIZE,	parse_extended_input_sz },
		{ CPERF_POOL_SIZE,	parse_pool_sz },
		{ CPERF_MAX_SGL_SEGS,	parse_max_num_sgl_segs },
		{ CPERF_NUM_ITER,	parse_num_iter },
		{ CPERF_OPTYPE,		parse_op_type },
		{ CPERF_HUFFMAN_ENC,	parse_huffman_enc },
		{ CPERF_LEVEL,		parse_level },
		{ CPERF_WINDOW_SIZE,	parse_window_sz },
	};
	unsigned int i;

	for (i = 0; i < RTE_DIM(parsermap); i++) {
		if (strncmp(lgopts[opt_idx].name, parsermap[i].lgopt_name,
				strlen(lgopts[opt_idx].name)) == 0)
			return parsermap[i].parser_fn(test_data, optarg);
	}

	return -EINVAL;
}

int
comp_perf_options_parse(struct comp_test_data *test_data, int argc, char **argv)
{
	int opt, retval, opt_idx;

	while ((opt = getopt_long(argc, argv, "h", lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			rte_exit(EXIT_SUCCESS, "Displayed help\n");
			break;
		/* long options */
		case 0:
			retval = comp_perf_opts_parse_long(opt_idx, test_data);
			if (retval != 0)
				return retval;

			break;

		default:
			usage(argv[0]);
			return -EINVAL;
		}
	}

	return 0;
}

void
comp_perf_options_default(struct comp_test_data *test_data)
{
	test_data->cdev_id = -1;
	test_data->seg_sz = 2048;
	test_data->burst_sz = 32;
	test_data->pool_sz = 8192;
	test_data->max_sgl_segs = 16;
	test_data->num_iter = 10000;
	test_data->huffman_enc = RTE_COMP_HUFFMAN_DYNAMIC;
	test_data->test_op = COMPRESS_DECOMPRESS;
	test_data->window_sz = -1;
	test_data->level.min = 1;
	test_data->level.max = 9;
	test_data->level.inc = 1;
}

int
comp_perf_options_check(struct comp_test_data *test_data)
{
	if (test_data->driver_name[0] == '\0') {
		RTE_LOG(ERR, USER1, "Driver name has to be set\n");
		return -1;
	}

	if (test_data->input_file[0] == '\0') {
		RTE_LOG(ERR, USER1, "Input file name has to be set\n");
		return -1;
	}

	return 0;
}
