/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>
#include <getopt.h>
#include <math.h>

#include <rte_memory.h>
#include <rte_mldev.h>
#include <rte_string_fns.h>

#include "ml_common.h"
#include "ml_test.h"
#include "parser.h"

typedef int (*option_parser_t)(struct ml_options *opt, const char *arg);

void
ml_options_default(struct ml_options *opt)
{
	memset(opt, 0, sizeof(*opt));
	strlcpy(opt->test_name, "device_ops", ML_TEST_NAME_MAX_LEN);
	opt->dev_id = 0;
	opt->socket_id = SOCKET_ID_ANY;
	opt->nb_filelist = 0;
	opt->repetitions = 1;
	opt->burst_size = 1;
	opt->queue_pairs = 1;
	opt->queue_size = 1;
	opt->batches = 0;
	opt->tolerance = 0.0;
	opt->stats = false;
	opt->debug = false;
}

struct long_opt_parser {
	const char *lgopt_name;
	option_parser_t parser_fn;
};

static int
ml_parse_test_name(struct ml_options *opt, const char *arg)
{
	strlcpy(opt->test_name, arg, ML_TEST_NAME_MAX_LEN);

	return 0;
}

static int
ml_parse_dev_id(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_int16(&opt->dev_id, arg);
	if (ret < 0)
		ml_err("Invalid option: dev_id = %s\n", arg);

	return ret;
}

static int
ml_parse_socket_id(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_int32(&opt->socket_id, arg);
	if (ret < 0)
		ml_err("Invalid option: socket_id = %s\n", arg);

	return ret;
}

static int
ml_parse_models(struct ml_options *opt, const char *arg)
{
	const char *delim = ",";
	char models[PATH_MAX];
	char *token;
	int ret = 0;

	strlcpy(models, arg, PATH_MAX);

	token = strtok(models, delim);
	while (token != NULL) {
		strlcpy(opt->filelist[opt->nb_filelist].model, token, PATH_MAX);
		opt->nb_filelist++;

		if (opt->nb_filelist >= ML_TEST_MAX_MODELS) {
			ml_err("Exceeded model count, max = %d\n", ML_TEST_MAX_MODELS);
			ret = -EINVAL;
			break;
		}
		token = strtok(NULL, delim);
	}

	if (opt->nb_filelist == 0) {
		ml_err("Models list is empty. Need at least one model for the test");
		ret = -EINVAL;
	}

	return ret;
}

static int
ml_parse_filelist(struct ml_options *opt, const char *arg)
{
	const char *delim = ",";
	char filelist[PATH_MAX];
	char *token;

	if (opt->nb_filelist >= ML_TEST_MAX_MODELS) {
		ml_err("Exceeded filelist count, max = %d\n", ML_TEST_MAX_MODELS);
		return -1;
	}

	strlcpy(filelist, arg, PATH_MAX);

	/* model */
	token = strtok(filelist, delim);
	if (token == NULL) {
		ml_err("Invalid filelist, model not specified = %s\n", arg);
		return -EINVAL;
	}
	strlcpy(opt->filelist[opt->nb_filelist].model, token, PATH_MAX);

	/* input */
	token = strtok(NULL, delim);
	if (token == NULL) {
		ml_err("Invalid filelist, input not specified = %s\n", arg);
		return -EINVAL;
	}
	strlcpy(opt->filelist[opt->nb_filelist].input, token, PATH_MAX);

	/* output */
	token = strtok(NULL, delim);
	if (token == NULL) {
		ml_err("Invalid filelist, output not specified = %s\n", arg);
		return -EINVAL;
	}
	strlcpy(opt->filelist[opt->nb_filelist].output, token, PATH_MAX);

	/* reference - optional */
	token = strtok(NULL, delim);
	if (token != NULL)
		strlcpy(opt->filelist[opt->nb_filelist].reference, token, PATH_MAX);
	else
		memset(opt->filelist[opt->nb_filelist].reference, 0, PATH_MAX);

	/* check for extra tokens */
	token = strtok(NULL, delim);
	if (token != NULL) {
		ml_err("Invalid filelist. Entries > 4\n.");
		return -EINVAL;
	}

	opt->nb_filelist++;

	if (opt->nb_filelist == 0) {
		ml_err("Empty filelist. Need at least one filelist entry for the test.\n");
		return -EINVAL;
	}

	return 0;
}

static int
ml_parse_repetitions(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&opt->repetitions, arg);
	if (ret != 0)
		ml_err("Invalid option, repetitions = %s\n", arg);

	return ret;
}

static int
ml_parse_burst_size(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&opt->burst_size, arg);
	if (ret != 0)
		ml_err("Invalid option, burst_size = %s\n", arg);

	return ret;
}

static int
ml_parse_queue_pairs(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&opt->queue_pairs, arg);
	if (ret != 0)
		ml_err("Invalid option, queue_pairs = %s\n", arg);

	return ret;
}

static int
ml_parse_queue_size(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&opt->queue_size, arg);
	if (ret != 0)
		ml_err("Invalid option, queue_size = %s\n", arg);

	return ret;
}

static int
ml_parse_batches(struct ml_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&opt->batches, arg);
	if (ret != 0)
		ml_err("Invalid option, batches = %s\n", arg);

	return ret;
}

static int
ml_parse_tolerance(struct ml_options *opt, const char *arg)
{
	opt->tolerance = fabs(atof(arg));

	return 0;
}

static void
ml_dump_test_options(const char *testname)
{
	if (strcmp(testname, "device_ops") == 0) {
		printf("\t\t--queue_pairs      : number of queue pairs to create\n"
		       "\t\t--queue_size       : size of queue-pair\n");
		printf("\n");
	}

	if (strcmp(testname, "model_ops") == 0) {
		printf("\t\t--models           : comma separated list of models\n"
		       "\t\t--stats            : enable reporting device statistics\n");
		printf("\n");
	}

	if ((strcmp(testname, "inference_ordered") == 0) ||
	    (strcmp(testname, "inference_interleave") == 0)) {
		printf("\t\t--filelist         : comma separated list of model, input, output and reference\n"
		       "\t\t--repetitions      : number of inference repetitions\n"
		       "\t\t--burst_size       : inference burst size\n"
		       "\t\t--queue_pairs      : number of queue pairs to create\n"
		       "\t\t--queue_size       : size of queue-pair\n"
		       "\t\t--batches          : number of batches of input\n"
		       "\t\t--tolerance        : maximum tolerance (%%) for output validation\n"
		       "\t\t--stats            : enable reporting device and model statistics\n");
		printf("\n");
	}
}

static void
print_usage(char *program)
{
	printf("\nusage : %s [EAL options] -- [application options]\n", program);
	printf("application options:\n");
	printf("\t--test             : name of the test application to run\n"
	       "\t--dev_id           : device id of the ML device\n"
	       "\t--socket_id        : socket_id of application resources\n"
	       "\t--debug            : enable debug mode\n"
	       "\t--help             : print help\n");
	printf("\n");
	printf("available tests and test specific application options:\n");
	ml_test_dump_names(ml_dump_test_options);
}

static struct option lgopts[] = {
	{ML_TEST, 1, 0, 0},
	{ML_DEVICE_ID, 1, 0, 0},
	{ML_SOCKET_ID, 1, 0, 0},
	{ML_MODELS, 1, 0, 0},
	{ML_FILELIST, 1, 0, 0},
	{ML_REPETITIONS, 1, 0, 0},
	{ML_BURST_SIZE, 1, 0, 0},
	{ML_QUEUE_PAIRS, 1, 0, 0},
	{ML_QUEUE_SIZE, 1, 0, 0},
	{ML_BATCHES, 1, 0, 0},
	{ML_TOLERANCE, 1, 0, 0},
	{ML_STATS, 0, 0, 0},
	{ML_DEBUG, 0, 0, 0},
	{ML_HELP, 0, 0, 0},
	{NULL, 0, 0, 0}};

static int
ml_opts_parse_long(int opt_idx, struct ml_options *opt)
{
	unsigned int i;

	struct long_opt_parser parsermap[] = {
		{ML_TEST, ml_parse_test_name},
		{ML_DEVICE_ID, ml_parse_dev_id},
		{ML_SOCKET_ID, ml_parse_socket_id},
		{ML_MODELS, ml_parse_models},
		{ML_FILELIST, ml_parse_filelist},
		{ML_REPETITIONS, ml_parse_repetitions},
		{ML_BURST_SIZE, ml_parse_burst_size},
		{ML_QUEUE_PAIRS, ml_parse_queue_pairs},
		{ML_QUEUE_SIZE, ml_parse_queue_size},
		{ML_BATCHES, ml_parse_batches},
		{ML_TOLERANCE, ml_parse_tolerance},
	};

	for (i = 0; i < RTE_DIM(parsermap); i++) {
		if (strncmp(lgopts[opt_idx].name, parsermap[i].lgopt_name,
			    strlen(lgopts[opt_idx].name)) == 0)
			return parsermap[i].parser_fn(opt, optarg);
	}

	return -EINVAL;
}

int
ml_options_parse(struct ml_options *opt, int argc, char **argv)
{
	int opt_idx;
	int retval;
	int opts;

	while ((opts = getopt_long(argc, argv, "", lgopts, &opt_idx)) != EOF) {
		switch (opts) {
		case 0: /* parse long options */
			if (!strcmp(lgopts[opt_idx].name, "stats")) {
				opt->stats = true;
				break;
			}

			if (!strcmp(lgopts[opt_idx].name, "debug")) {
				opt->debug = true;
				break;
			}

			if (!strcmp(lgopts[opt_idx].name, "help")) {
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			}

			retval = ml_opts_parse_long(opt_idx, opt);
			if (retval != 0)
				return retval;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

void
ml_options_dump(struct ml_options *opt)
{
	struct rte_ml_dev_info dev_info;

	rte_ml_dev_info_get(opt->dev_id, &dev_info);

	ml_dump("driver", "%s", dev_info.driver_name);
	ml_dump("test", "%s", opt->test_name);
	ml_dump("dev_id", "%d", opt->dev_id);

	if (opt->socket_id == SOCKET_ID_ANY)
		ml_dump("socket_id", "%d (SOCKET_ID_ANY)", opt->socket_id);
	else
		ml_dump("socket_id", "%d", opt->socket_id);

	ml_dump("debug", "%s", (opt->debug ? "true" : "false"));
}
