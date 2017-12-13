/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <getopt.h>
#include <unistd.h>

#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "cperf_options.h"

#define AES_BLOCK_SIZE 16
#define DES_BLOCK_SIZE 8

struct name_id_map {
	const char *name;
	uint32_t id;
};

static void
usage(char *progname)
{
	printf("%s [EAL options] --\n"
		" --silent: disable options dump\n"
		" --ptest throughput / latency / verify / pmd-cycleount :"
		" set test type\n"
		" --pool_sz N: set the number of crypto ops/mbufs allocated\n"
		" --total-ops N: set the number of total operations performed\n"
		" --burst-sz N: set the number of packets per burst\n"
		" --buffer-sz N: set the size of a single packet\n"
		" --imix N: set the distribution of packet sizes\n"
		" --segment-sz N: set the size of the segment to use\n"
		" --desc-nb N: set number of descriptors for each crypto device\n"
		" --devtype TYPE: set crypto device type to use\n"
		" --optype cipher-only / auth-only / cipher-then-auth /\n"
		"           auth-then-cipher / aead : set operation type\n"
		" --sessionless: enable session-less crypto operations\n"
		" --out-of-place: enable out-of-place crypto operations\n"
		" --test-file NAME: set the test vector file path\n"
		" --test-name NAME: set specific test name section in test file\n"
		" --cipher-algo ALGO: set cipher algorithm\n"
		" --cipher-op encrypt / decrypt: set the cipher operation\n"
		" --cipher-key-sz N: set the cipher key size\n"
		" --cipher-iv-sz N: set the cipher IV size\n"
		" --auth-algo ALGO: set auth algorithm\n"
		" --auth-op generate / verify: set the auth operation\n"
		" --auth-key-sz N: set the auth key size\n"
		" --auth-iv-sz N: set the auth IV size\n"
		" --aead-algo ALGO: set AEAD algorithm\n"
		" --aead-op encrypt / decrypt: set the AEAD operation\n"
		" --aead-key-sz N: set the AEAD key size\n"
		" --aead-iv-sz N: set the AEAD IV size\n"
		" --aead-aad-sz N: set the AEAD AAD size\n"
		" --digest-sz N: set the digest size\n"
		" --pmd-cyclecount-delay-ms N: set delay between enqueue\n"
		"           and dequeue in pmd-cyclecount benchmarking mode\n"
		" --csv-friendly: enable test result output CSV friendly\n"
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
parse_cperf_test_type(struct cperf_options *opts, const char *arg)
{
	struct name_id_map cperftest_namemap[] = {
		{
			cperf_test_type_strs[CPERF_TEST_TYPE_THROUGHPUT],
			CPERF_TEST_TYPE_THROUGHPUT
		},
		{
			cperf_test_type_strs[CPERF_TEST_TYPE_VERIFY],
			CPERF_TEST_TYPE_VERIFY
		},
		{
			cperf_test_type_strs[CPERF_TEST_TYPE_LATENCY],
			CPERF_TEST_TYPE_LATENCY
		},
		{
			cperf_test_type_strs[CPERF_TEST_TYPE_PMDCC],
			CPERF_TEST_TYPE_PMDCC
		}
	};

	int id = get_str_key_id_mapping(
			(struct name_id_map *)cperftest_namemap,
			RTE_DIM(cperftest_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "failed to parse test type");
		return -1;
	}

	opts->test = (enum cperf_perf_test_type)id;

	return 0;
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
parse_range(const char *arg, uint32_t *min, uint32_t *max, uint32_t *inc)
{
	char *token;
	uint32_t number;

	char *copy_arg = strdup(arg);

	if (copy_arg == NULL)
		return -1;

	errno = 0;
	token = strtok(copy_arg, ":");

	/* Parse minimum value */
	if (token != NULL) {
		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE ||
				number == 0)
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
				number == 0 ||
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
parse_list(const char *arg, uint32_t *list, uint32_t *min, uint32_t *max)
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

		if (errno == EINVAL || errno == ERANGE ||
				number == 0)
			goto err_list;

		list[count++] = number;
		temp_min = number;
		temp_max = number;
	} else
		goto err_list;

	token = strtok(NULL, ",");

	while (token != NULL) {
		if (count == MAX_LIST) {
			RTE_LOG(WARNING, USER1, "Using only the first %u sizes\n",
					MAX_LIST);
			break;
		}

		number = strtoul(token, NULL, 10);

		if (errno == EINVAL || errno == ERANGE ||
				number == 0)
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
parse_total_ops(struct cperf_options *opts, const char *arg)
{
	int ret = parse_uint32_t(&opts->total_ops, arg);

	if (ret)
		RTE_LOG(ERR, USER1, "failed to parse total operations count\n");

	if (opts->total_ops == 0) {
		RTE_LOG(ERR, USER1,
				"invalid total operations count number specified\n");
		return -1;
	}

	return ret;
}

static int
parse_pool_sz(struct cperf_options *opts, const char *arg)
{
	int ret =  parse_uint32_t(&opts->pool_sz, arg);

	if (ret)
		RTE_LOG(ERR, USER1, "failed to parse pool size");
	return ret;
}

static int
parse_burst_sz(struct cperf_options *opts, const char *arg)
{
	int ret;

	/* Try parsing the argument as a range, if it fails, parse it as a list */
	if (parse_range(arg, &opts->min_burst_size, &opts->max_burst_size,
			&opts->inc_burst_size) < 0) {
		ret = parse_list(arg, opts->burst_size_list,
					&opts->min_burst_size,
					&opts->max_burst_size);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "failed to parse burst size/s\n");
			return -1;
		}
		opts->burst_size_count = ret;
	}

	return 0;
}

static int
parse_buffer_sz(struct cperf_options *opts, const char *arg)
{
	int ret;

	/* Try parsing the argument as a range, if it fails, parse it as a list */
	if (parse_range(arg, &opts->min_buffer_size, &opts->max_buffer_size,
			&opts->inc_buffer_size) < 0) {
		ret = parse_list(arg, opts->buffer_size_list,
					&opts->min_buffer_size,
					&opts->max_buffer_size);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "failed to parse buffer size/s\n");
			return -1;
		}
		opts->buffer_size_count = ret;
	}

	return 0;
}

static int
parse_segment_sz(struct cperf_options *opts, const char *arg)
{
	int ret = parse_uint32_t(&opts->segment_sz, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "failed to parse segment size\n");
		return -1;
	}

	if (opts->segment_sz == 0) {
		RTE_LOG(ERR, USER1, "Segment size has to be bigger than 0\n");
		return -1;
	}

	return 0;
}

static int
parse_imix(struct cperf_options *opts, const char *arg)
{
	int ret;

	ret = parse_list(arg, opts->imix_distribution_list,
				NULL, NULL);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "failed to parse imix distribution\n");
		return -1;
	}

	opts->imix_distribution_count = ret;

	if (opts->imix_distribution_count <= 1) {
		RTE_LOG(ERR, USER1, "imix distribution should have "
				"at least two entries\n");
		return -1;
	}

	return 0;
}

static int
parse_desc_nb(struct cperf_options *opts, const char *arg)
{
	int ret = parse_uint32_t(&opts->nb_descriptors, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "failed to parse descriptors number\n");
		return -1;
	}

	if (opts->nb_descriptors == 0) {
		RTE_LOG(ERR, USER1, "invalid descriptors number specified\n");
		return -1;
	}

	return 0;
}

static int
parse_device_type(struct cperf_options *opts, const char *arg)
{
	if (strlen(arg) > (sizeof(opts->device_type) - 1))
		return -1;

	strncpy(opts->device_type, arg, sizeof(opts->device_type) - 1);
	*(opts->device_type + sizeof(opts->device_type) - 1) = '\0';

	return 0;
}

static int
parse_op_type(struct cperf_options *opts, const char *arg)
{
	struct name_id_map optype_namemap[] = {
		{
			cperf_op_type_strs[CPERF_CIPHER_ONLY],
			CPERF_CIPHER_ONLY
		},
		{
			cperf_op_type_strs[CPERF_AUTH_ONLY],
			CPERF_AUTH_ONLY
		},
		{
			cperf_op_type_strs[CPERF_CIPHER_THEN_AUTH],
			CPERF_CIPHER_THEN_AUTH
		},
		{
			cperf_op_type_strs[CPERF_AUTH_THEN_CIPHER],
			CPERF_AUTH_THEN_CIPHER
		},
		{
			cperf_op_type_strs[CPERF_AEAD],
			CPERF_AEAD
		}
	};

	int id = get_str_key_id_mapping(optype_namemap,
			RTE_DIM(optype_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "invalid opt type specified\n");
		return -1;
	}

	opts->op_type = (enum cperf_op_type)id;

	return 0;
}

static int
parse_sessionless(struct cperf_options *opts,
		const char *arg __rte_unused)
{
	opts->sessionless = 1;
	return 0;
}

static int
parse_out_of_place(struct cperf_options *opts,
		const char *arg __rte_unused)
{
	opts->out_of_place = 1;
	return 0;
}

static int
parse_test_file(struct cperf_options *opts,
		const char *arg)
{
	opts->test_file = strdup(arg);
	if (access(opts->test_file, F_OK) != -1)
		return 0;
	RTE_LOG(ERR, USER1, "Test vector file doesn't exist\n");

	return -1;
}

static int
parse_test_name(struct cperf_options *opts,
		const char *arg)
{
	char *test_name = (char *) rte_zmalloc(NULL,
		sizeof(char) * (strlen(arg) + 3), 0);
	snprintf(test_name, strlen(arg) + 3, "[%s]", arg);
	opts->test_name = test_name;

	return 0;
}

static int
parse_silent(struct cperf_options *opts,
		const char *arg __rte_unused)
{
	opts->silent = 1;

	return 0;
}

static int
parse_cipher_algo(struct cperf_options *opts, const char *arg)
{

	enum rte_crypto_cipher_algorithm cipher_algo;

	if (rte_cryptodev_get_cipher_algo_enum(&cipher_algo, arg) < 0) {
		RTE_LOG(ERR, USER1, "Invalid cipher algorithm specified\n");
		return -1;
	}

	opts->cipher_algo = cipher_algo;

	return 0;
}

static int
parse_cipher_op(struct cperf_options *opts, const char *arg)
{
	struct name_id_map cipher_op_namemap[] = {
		{
			rte_crypto_cipher_operation_strings
			[RTE_CRYPTO_CIPHER_OP_ENCRYPT],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT },
		{
			rte_crypto_cipher_operation_strings
			[RTE_CRYPTO_CIPHER_OP_DECRYPT],
			RTE_CRYPTO_CIPHER_OP_DECRYPT
		}
	};

	int id = get_str_key_id_mapping(cipher_op_namemap,
			RTE_DIM(cipher_op_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "Invalid cipher operation specified\n");
		return -1;
	}

	opts->cipher_op = (enum rte_crypto_cipher_operation)id;

	return 0;
}

static int
parse_cipher_key_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->cipher_key_sz, arg);
}

static int
parse_cipher_iv_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->cipher_iv_sz, arg);
}

static int
parse_auth_algo(struct cperf_options *opts, const char *arg)
{
	enum rte_crypto_auth_algorithm auth_algo;

	if (rte_cryptodev_get_auth_algo_enum(&auth_algo, arg) < 0) {
		RTE_LOG(ERR, USER1, "Invalid authentication algorithm specified\n");
		return -1;
	}

	opts->auth_algo = auth_algo;

	return 0;
}

static int
parse_auth_op(struct cperf_options *opts, const char *arg)
{
	struct name_id_map auth_op_namemap[] = {
		{
			rte_crypto_auth_operation_strings
			[RTE_CRYPTO_AUTH_OP_GENERATE],
			RTE_CRYPTO_AUTH_OP_GENERATE },
		{
			rte_crypto_auth_operation_strings
			[RTE_CRYPTO_AUTH_OP_VERIFY],
			RTE_CRYPTO_AUTH_OP_VERIFY
		}
	};

	int id = get_str_key_id_mapping(auth_op_namemap,
			RTE_DIM(auth_op_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "invalid authentication operation specified"
				"\n");
		return -1;
	}

	opts->auth_op = (enum rte_crypto_auth_operation)id;

	return 0;
}

static int
parse_auth_key_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->auth_key_sz, arg);
}

static int
parse_digest_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->digest_sz, arg);
}

static int
parse_auth_iv_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->auth_iv_sz, arg);
}

static int
parse_aead_algo(struct cperf_options *opts, const char *arg)
{
	enum rte_crypto_aead_algorithm aead_algo;

	if (rte_cryptodev_get_aead_algo_enum(&aead_algo, arg) < 0) {
		RTE_LOG(ERR, USER1, "Invalid AEAD algorithm specified\n");
		return -1;
	}

	opts->aead_algo = aead_algo;

	return 0;
}

static int
parse_aead_op(struct cperf_options *opts, const char *arg)
{
	struct name_id_map aead_op_namemap[] = {
		{
			rte_crypto_aead_operation_strings
			[RTE_CRYPTO_AEAD_OP_ENCRYPT],
			RTE_CRYPTO_AEAD_OP_ENCRYPT },
		{
			rte_crypto_aead_operation_strings
			[RTE_CRYPTO_AEAD_OP_DECRYPT],
			RTE_CRYPTO_AEAD_OP_DECRYPT
		}
	};

	int id = get_str_key_id_mapping(aead_op_namemap,
			RTE_DIM(aead_op_namemap), arg);
	if (id < 0) {
		RTE_LOG(ERR, USER1, "invalid AEAD operation specified"
				"\n");
		return -1;
	}

	opts->aead_op = (enum rte_crypto_aead_operation)id;

	return 0;
}

static int
parse_aead_key_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->aead_key_sz, arg);
}

static int
parse_aead_iv_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->aead_iv_sz, arg);
}

static int
parse_aead_aad_sz(struct cperf_options *opts, const char *arg)
{
	return parse_uint16_t(&opts->aead_aad_sz, arg);
}

static int
parse_csv_friendly(struct cperf_options *opts, const char *arg __rte_unused)
{
	opts->csv = 1;
	opts->silent = 1;
	return 0;
}

static int
parse_pmd_cyclecount_delay_ms(struct cperf_options *opts,
			const char *arg)
{
	int ret = parse_uint32_t(&opts->pmdcc_delay, arg);

	if (ret) {
		RTE_LOG(ERR, USER1, "failed to parse pmd-cyclecount delay\n");
		return -1;
	}

	return 0;
}

typedef int (*option_parser_t)(struct cperf_options *opts,
		const char *arg);

struct long_opt_parser {
	const char *lgopt_name;
	option_parser_t parser_fn;

};

static struct option lgopts[] = {

	{ CPERF_PTEST_TYPE, required_argument, 0, 0 },

	{ CPERF_POOL_SIZE, required_argument, 0, 0 },
	{ CPERF_TOTAL_OPS, required_argument, 0, 0 },
	{ CPERF_BURST_SIZE, required_argument, 0, 0 },
	{ CPERF_BUFFER_SIZE, required_argument, 0, 0 },
	{ CPERF_SEGMENT_SIZE, required_argument, 0, 0 },
	{ CPERF_DESC_NB, required_argument, 0, 0 },

	{ CPERF_IMIX, required_argument, 0, 0 },
	{ CPERF_DEVTYPE, required_argument, 0, 0 },
	{ CPERF_OPTYPE, required_argument, 0, 0 },

	{ CPERF_SILENT, no_argument, 0, 0 },
	{ CPERF_SESSIONLESS, no_argument, 0, 0 },
	{ CPERF_OUT_OF_PLACE, no_argument, 0, 0 },
	{ CPERF_TEST_FILE, required_argument, 0, 0 },
	{ CPERF_TEST_NAME, required_argument, 0, 0 },

	{ CPERF_CIPHER_ALGO, required_argument, 0, 0 },
	{ CPERF_CIPHER_OP, required_argument, 0, 0 },

	{ CPERF_CIPHER_KEY_SZ, required_argument, 0, 0 },
	{ CPERF_CIPHER_IV_SZ, required_argument, 0, 0 },

	{ CPERF_AUTH_ALGO, required_argument, 0, 0 },
	{ CPERF_AUTH_OP, required_argument, 0, 0 },

	{ CPERF_AUTH_KEY_SZ, required_argument, 0, 0 },
	{ CPERF_AUTH_IV_SZ, required_argument, 0, 0 },

	{ CPERF_AEAD_ALGO, required_argument, 0, 0 },
	{ CPERF_AEAD_OP, required_argument, 0, 0 },

	{ CPERF_AEAD_KEY_SZ, required_argument, 0, 0 },
	{ CPERF_AEAD_AAD_SZ, required_argument, 0, 0 },
	{ CPERF_AEAD_IV_SZ, required_argument, 0, 0 },

	{ CPERF_DIGEST_SZ, required_argument, 0, 0 },

	{ CPERF_CSV, no_argument, 0, 0},

	{ CPERF_PMDCC_DELAY_MS, required_argument, 0, 0 },

	{ NULL, 0, 0, 0 }
};

void
cperf_options_default(struct cperf_options *opts)
{
	opts->test = CPERF_TEST_TYPE_THROUGHPUT;

	opts->pool_sz = 8192;
	opts->total_ops = 10000000;
	opts->nb_descriptors = 2048;

	opts->buffer_size_list[0] = 64;
	opts->buffer_size_count = 1;
	opts->max_buffer_size = 64;
	opts->min_buffer_size = 64;
	opts->inc_buffer_size = 0;

	opts->burst_size_list[0] = 32;
	opts->burst_size_count = 1;
	opts->max_burst_size = 32;
	opts->min_burst_size = 32;
	opts->inc_burst_size = 0;

	/*
	 * Will be parsed from command line or set to
	 * maximum buffer size + digest, later
	 */
	opts->segment_sz = 0;

	opts->imix_distribution_count = 0;
	strncpy(opts->device_type, "crypto_aesni_mb",
			sizeof(opts->device_type));
	opts->nb_qps = 1;

	opts->op_type = CPERF_CIPHER_THEN_AUTH;

	opts->silent = 0;
	opts->test_file = NULL;
	opts->test_name = NULL;
	opts->sessionless = 0;
	opts->out_of_place = 0;
	opts->csv = 0;

	opts->cipher_algo = RTE_CRYPTO_CIPHER_AES_CBC;
	opts->cipher_op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	opts->cipher_key_sz = 16;
	opts->cipher_iv_sz = 16;

	opts->auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	opts->auth_op = RTE_CRYPTO_AUTH_OP_GENERATE;

	opts->auth_key_sz = 64;
	opts->auth_iv_sz = 0;

	opts->aead_key_sz = 0;
	opts->aead_iv_sz = 0;
	opts->aead_aad_sz = 0;

	opts->digest_sz = 12;

	opts->pmdcc_delay = 0;
}

static int
cperf_opts_parse_long(int opt_idx, struct cperf_options *opts)
{
	struct long_opt_parser parsermap[] = {
		{ CPERF_PTEST_TYPE,	parse_cperf_test_type },
		{ CPERF_SILENT,		parse_silent },
		{ CPERF_POOL_SIZE,	parse_pool_sz },
		{ CPERF_TOTAL_OPS,	parse_total_ops },
		{ CPERF_BURST_SIZE,	parse_burst_sz },
		{ CPERF_BUFFER_SIZE,	parse_buffer_sz },
		{ CPERF_SEGMENT_SIZE,	parse_segment_sz },
		{ CPERF_DESC_NB,	parse_desc_nb },
		{ CPERF_DEVTYPE,	parse_device_type },
		{ CPERF_OPTYPE,		parse_op_type },
		{ CPERF_SESSIONLESS,	parse_sessionless },
		{ CPERF_OUT_OF_PLACE,	parse_out_of_place },
		{ CPERF_IMIX,		parse_imix },
		{ CPERF_TEST_FILE,	parse_test_file },
		{ CPERF_TEST_NAME,	parse_test_name },
		{ CPERF_CIPHER_ALGO,	parse_cipher_algo },
		{ CPERF_CIPHER_OP,	parse_cipher_op },
		{ CPERF_CIPHER_KEY_SZ,	parse_cipher_key_sz },
		{ CPERF_CIPHER_IV_SZ,	parse_cipher_iv_sz },
		{ CPERF_AUTH_ALGO,	parse_auth_algo },
		{ CPERF_AUTH_OP,	parse_auth_op },
		{ CPERF_AUTH_KEY_SZ,	parse_auth_key_sz },
		{ CPERF_AUTH_IV_SZ,	parse_auth_iv_sz },
		{ CPERF_AEAD_ALGO,	parse_aead_algo },
		{ CPERF_AEAD_OP,	parse_aead_op },
		{ CPERF_AEAD_KEY_SZ,	parse_aead_key_sz },
		{ CPERF_AEAD_IV_SZ,	parse_aead_iv_sz },
		{ CPERF_AEAD_AAD_SZ,	parse_aead_aad_sz },
		{ CPERF_DIGEST_SZ,	parse_digest_sz },
		{ CPERF_CSV,		parse_csv_friendly},
		{ CPERF_PMDCC_DELAY_MS,	parse_pmd_cyclecount_delay_ms},
	};
	unsigned int i;

	for (i = 0; i < RTE_DIM(parsermap); i++) {
		if (strncmp(lgopts[opt_idx].name, parsermap[i].lgopt_name,
				strlen(lgopts[opt_idx].name)) == 0)
			return parsermap[i].parser_fn(opts, optarg);
	}

	return -EINVAL;
}

int
cperf_options_parse(struct cperf_options *options, int argc, char **argv)
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
			retval = cperf_opts_parse_long(opt_idx, options);
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

static int
check_cipher_buffer_length(struct cperf_options *options)
{
	uint32_t buffer_size, buffer_size_idx = 0;

	if (options->cipher_algo == RTE_CRYPTO_CIPHER_AES_CBC ||
			options->cipher_algo == RTE_CRYPTO_CIPHER_AES_ECB) {
		if (options->inc_buffer_size != 0)
			buffer_size = options->min_buffer_size;
		else
			buffer_size = options->buffer_size_list[0];

		while (buffer_size <= options->max_buffer_size) {
			if ((buffer_size % AES_BLOCK_SIZE) != 0) {
				RTE_LOG(ERR, USER1, "Some of the buffer sizes are "
					"not suitable for the algorithm selected\n");
				return -EINVAL;
			}

			if (options->inc_buffer_size != 0)
				buffer_size += options->inc_buffer_size;
			else {
				if (++buffer_size_idx == options->buffer_size_count)
					break;
				buffer_size = options->buffer_size_list[buffer_size_idx];
			}

		}
	}

	if (options->cipher_algo == RTE_CRYPTO_CIPHER_DES_CBC ||
			options->cipher_algo == RTE_CRYPTO_CIPHER_3DES_CBC ||
			options->cipher_algo == RTE_CRYPTO_CIPHER_3DES_ECB) {
		if (options->inc_buffer_size != 0)
			buffer_size = options->min_buffer_size;
		else
			buffer_size = options->buffer_size_list[0];

		while (buffer_size <= options->max_buffer_size) {
			if ((buffer_size % DES_BLOCK_SIZE) != 0) {
				RTE_LOG(ERR, USER1, "Some of the buffer sizes are "
					"not suitable for the algorithm selected\n");
				return -EINVAL;
			}

			if (options->inc_buffer_size != 0)
				buffer_size += options->inc_buffer_size;
			else {
				if (++buffer_size_idx == options->buffer_size_count)
					break;
				buffer_size = options->buffer_size_list[buffer_size_idx];
			}

		}
	}

	return 0;
}

int
cperf_options_check(struct cperf_options *options)
{
	if (options->op_type == CPERF_CIPHER_ONLY)
		options->digest_sz = 0;

	/*
	 * If segment size is not set, assume only one segment,
	 * big enough to contain the largest buffer and the digest
	 */
	if (options->segment_sz == 0)
		options->segment_sz = options->max_buffer_size +
				options->digest_sz;

	if (options->segment_sz < options->digest_sz) {
		RTE_LOG(ERR, USER1,
				"Segment size should be at least "
				"the size of the digest\n");
		return -EINVAL;
	}

	if ((options->imix_distribution_count != 0) &&
			(options->imix_distribution_count !=
				options->buffer_size_count)) {
		RTE_LOG(ERR, USER1, "IMIX distribution must have the same "
				"number of buffer sizes\n");
		return -EINVAL;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY &&
			options->test_file == NULL) {
		RTE_LOG(ERR, USER1, "Define path to the file with test"
				" vectors.\n");
		return -EINVAL;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY &&
			options->op_type != CPERF_CIPHER_ONLY &&
			options->test_name == NULL) {
		RTE_LOG(ERR, USER1, "Define test name to get the correct digest"
				" from the test vectors.\n");
		return -EINVAL;
	}

	if (options->test_name != NULL && options->test_file == NULL) {
		RTE_LOG(ERR, USER1, "Define path to the file with test"
				" vectors.\n");
		return -EINVAL;
	}

	if (options->auth_op == RTE_CRYPTO_AUTH_OP_VERIFY &&
			options->test_file == NULL) {
		RTE_LOG(ERR, USER1, "Define path to the file with test"
				" vectors.\n");
		return -EINVAL;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY &&
			(options->inc_buffer_size != 0 ||
			options->buffer_size_count > 1)) {
		RTE_LOG(ERR, USER1, "Only one buffer size is allowed when "
				"using the verify test.\n");
		return -EINVAL;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY &&
			(options->inc_burst_size != 0 ||
			options->burst_size_count > 1)) {
		RTE_LOG(ERR, USER1, "Only one burst size is allowed when "
				"using the verify test.\n");
		return -EINVAL;
	}

	if (options->test == CPERF_TEST_TYPE_PMDCC &&
			options->pool_sz < options->nb_descriptors) {
		RTE_LOG(ERR, USER1, "For pmd cyclecount benchmarks, pool size "
				"must be equal or greater than the number of "
				"cryptodev descriptors.\n");
		return -EINVAL;
	}

	if (options->test == CPERF_TEST_TYPE_VERIFY &&
			options->imix_distribution_count > 0) {
		RTE_LOG(ERR, USER1, "IMIX is not allowed when "
				"using the verify test.\n");
		return -EINVAL;
	}

	if (options->op_type == CPERF_CIPHER_THEN_AUTH) {
		if (options->cipher_op != RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
				options->auth_op !=
				RTE_CRYPTO_AUTH_OP_GENERATE) {
			RTE_LOG(ERR, USER1, "Option cipher then auth must use"
					" options: encrypt and generate.\n");
			return -EINVAL;
		}
	} else if (options->op_type == CPERF_AUTH_THEN_CIPHER) {
		if (options->cipher_op != RTE_CRYPTO_CIPHER_OP_DECRYPT &&
				options->auth_op !=
				RTE_CRYPTO_AUTH_OP_VERIFY) {
			RTE_LOG(ERR, USER1, "Option auth then cipher must use"
					" options: decrypt and verify.\n");
			return -EINVAL;
		}
	}

	if (options->op_type == CPERF_CIPHER_ONLY ||
			options->op_type == CPERF_CIPHER_THEN_AUTH ||
			options->op_type == CPERF_AUTH_THEN_CIPHER) {
		if (check_cipher_buffer_length(options) < 0)
			return -EINVAL;
	}

	return 0;
}

void
cperf_options_dump(struct cperf_options *opts)
{
	uint8_t size_idx;

	printf("# Crypto Performance Application Options:\n");
	printf("#\n");
	printf("# cperf test: %s\n", cperf_test_type_strs[opts->test]);
	printf("#\n");
	printf("# size of crypto op / mbuf pool: %u\n", opts->pool_sz);
	printf("# total number of ops: %u\n", opts->total_ops);
	if (opts->inc_buffer_size != 0) {
		printf("# buffer size:\n");
		printf("#\t min: %u\n", opts->min_buffer_size);
		printf("#\t max: %u\n", opts->max_buffer_size);
		printf("#\t inc: %u\n", opts->inc_buffer_size);
	} else {
		printf("# buffer sizes: ");
		for (size_idx = 0; size_idx < opts->buffer_size_count; size_idx++)
			printf("%u ", opts->buffer_size_list[size_idx]);
		printf("\n");
	}
	if (opts->inc_burst_size != 0) {
		printf("# burst size:\n");
		printf("#\t min: %u\n", opts->min_burst_size);
		printf("#\t max: %u\n", opts->max_burst_size);
		printf("#\t inc: %u\n", opts->inc_burst_size);
	} else {
		printf("# burst sizes: ");
		for (size_idx = 0; size_idx < opts->burst_size_count; size_idx++)
			printf("%u ", opts->burst_size_list[size_idx]);
		printf("\n");
	}
	printf("\n# segment size: %u\n", opts->segment_sz);
	printf("#\n");
	printf("# cryptodev type: %s\n", opts->device_type);
	printf("#\n");
	printf("# number of queue pairs per device: %u\n", opts->nb_qps);
	printf("# crypto operation: %s\n", cperf_op_type_strs[opts->op_type]);
	printf("# sessionless: %s\n", opts->sessionless ? "yes" : "no");
	printf("# out of place: %s\n", opts->out_of_place ? "yes" : "no");
	if (opts->test == CPERF_TEST_TYPE_PMDCC)
		printf("# inter-burst delay: %u ms\n", opts->pmdcc_delay);

	printf("#\n");

	if (opts->op_type == CPERF_AUTH_ONLY ||
			opts->op_type == CPERF_CIPHER_THEN_AUTH ||
			opts->op_type == CPERF_AUTH_THEN_CIPHER) {
		printf("# auth algorithm: %s\n",
			rte_crypto_auth_algorithm_strings[opts->auth_algo]);
		printf("# auth operation: %s\n",
			rte_crypto_auth_operation_strings[opts->auth_op]);
		printf("# auth key size: %u\n", opts->auth_key_sz);
		printf("# auth iv size: %u\n", opts->auth_iv_sz);
		printf("# auth digest size: %u\n", opts->digest_sz);
		printf("#\n");
	}

	if (opts->op_type == CPERF_CIPHER_ONLY ||
			opts->op_type == CPERF_CIPHER_THEN_AUTH ||
			opts->op_type == CPERF_AUTH_THEN_CIPHER) {
		printf("# cipher algorithm: %s\n",
			rte_crypto_cipher_algorithm_strings[opts->cipher_algo]);
		printf("# cipher operation: %s\n",
			rte_crypto_cipher_operation_strings[opts->cipher_op]);
		printf("# cipher key size: %u\n", opts->cipher_key_sz);
		printf("# cipher iv size: %u\n", opts->cipher_iv_sz);
		printf("#\n");
	}

	if (opts->op_type == CPERF_AEAD) {
		printf("# aead algorithm: %s\n",
			rte_crypto_aead_algorithm_strings[opts->aead_algo]);
		printf("# aead operation: %s\n",
			rte_crypto_aead_operation_strings[opts->aead_op]);
		printf("# aead key size: %u\n", opts->aead_key_sz);
		printf("# aead iv size: %u\n", opts->aead_iv_sz);
		printf("# aead digest size: %u\n", opts->digest_sz);
		printf("# aead aad size: %u\n", opts->aead_aad_sz);
		printf("#\n");
	}
}
