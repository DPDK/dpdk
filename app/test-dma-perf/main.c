/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <libgen.h>

#include <rte_argparse.h>
#include <rte_eal.h>
#include <rte_cfgfile.h>
#include <rte_string_fns.h>
#include <rte_lcore.h>
#include <rte_dmadev.h>
#include <rte_kvargs.h>

#include "main.h"

#define CSV_HDR_FMT "Case %u : %s,lcore,DMA,DMA ring size,kick batch size,buffer size(B),number of buffers,memory(MB),average cycle,bandwidth(Gbps),MOps\n"

#define DMA_MEM_COPY "DMA_MEM_COPY"
#define CPU_MEM_COPY "CPU_MEM_COPY"

#define MAX_PARAMS_PER_ENTRY 4

#define MAX_TEST_CASES 16
static struct test_configure test_cases[MAX_TEST_CASES];

#define GLOBAL_SECTION_NAME	"GLOBAL"
struct global_configure global_cfg;

static char *config_path;
static char *result_path;
static bool  list_dma;

__rte_format_printf(1, 2)
void
output_csv(const char *fmt, ...)
{
#define MAX_OUTPUT_STR_LEN 512
	char str[MAX_OUTPUT_STR_LEN] = {0};
	va_list ap;
	FILE *fd;

	fd = fopen(result_path, "a");
	if (fd == NULL) {
		printf("Open output CSV file error.\n");
		return;
	}

	va_start(ap, fmt);
	vsnprintf(str, MAX_OUTPUT_STR_LEN, fmt, ap);
	va_end(ap);

	fprintf(fd, "%s", str);

	fflush(fd);
	fclose(fd);
}

static void
output_blanklines(int lines)
{
	int i;
	for (i = 0; i < lines; i++)
		output_csv("%s\n", ",,,,,,,,");
}

static void
output_env_info(void)
{
	output_blanklines(2);
	output_csv("Test Environment:\n"
		   "CPU frequency,%.3lf Ghz\n", rte_get_timer_hz() / 1000000000.0);
	output_blanklines(1);
}

static void
output_header(uint32_t case_id, struct test_configure *case_cfg)
{
	static const char * const type_str[] = { "NONE", DMA_MEM_COPY, CPU_MEM_COPY };
	output_csv(CSV_HDR_FMT, case_id, type_str[case_cfg->test_type]);
}

static int
run_test_case(struct test_configure *case_cfg)
{
	int ret = 0;

	switch (case_cfg->test_type) {
	case TEST_TYPE_DMA_MEM_COPY:
	case TEST_TYPE_CPU_MEM_COPY:
		ret = mem_copy_benchmark(case_cfg);
		break;
	default:
		printf("Unknown test type\n");
		break;
	}

	return ret;
}

static void
run_test(uint32_t case_id, struct test_configure *case_cfg)
{
	uint32_t nb_lcores = rte_lcore_count();
	struct test_configure_entry *mem_size = &case_cfg->mem_size;
	struct test_configure_entry *buf_size = &case_cfg->buf_size;
	struct test_configure_entry *ring_size = &case_cfg->ring_size;
	struct test_configure_entry *kick_batch = &case_cfg->kick_batch;
	struct test_configure_entry dummy = { 0 };
	struct test_configure_entry *var_entry = &dummy;

	if (nb_lcores <= case_cfg->num_worker) {
		printf("Case %u: Not enough lcores.\n", case_id);
		return;
	}

	printf("Number of used lcores: %u.\n", nb_lcores);

	if (mem_size->incr != 0)
		var_entry = mem_size;

	if (buf_size->incr != 0)
		var_entry = buf_size;

	if (ring_size->incr != 0)
		var_entry = ring_size;

	if (kick_batch->incr != 0)
		var_entry = kick_batch;

	case_cfg->scenario_id = 0;

	output_header(case_id, case_cfg);

	for (var_entry->cur = var_entry->first; var_entry->cur <= var_entry->last;) {
		case_cfg->scenario_id++;
		printf("\nRunning scenario %d\n", case_cfg->scenario_id);

		if (run_test_case(case_cfg) < 0)
			printf("\nTest fails! skipping this scenario.\n");

		if (var_entry->op == OP_ADD)
			var_entry->cur += var_entry->incr;
		else if (var_entry->op == OP_MUL)
			var_entry->cur *= var_entry->incr;
		else {
			printf("No proper operation for variable entry.\n");
			break;
		}
	}
}

static int
parse_lcore(struct test_configure *test_case, const char *value)
{
	uint16_t len;
	char *input;
	struct lcore_dma_map_t *lcore_dma_map;

	if (test_case == NULL || value == NULL)
		return -1;

	len = strlen(value);
	input = (char *)malloc((len + 1) * sizeof(char));
	strlcpy(input, value, len + 1);

	char *token = strtok(input, ", ");
	while (token != NULL) {
		lcore_dma_map = &(test_case->dma_config[test_case->num_worker++].lcore_dma_map);
		memset(lcore_dma_map, 0, sizeof(struct lcore_dma_map_t));
		if (test_case->num_worker >= MAX_WORKER_NB) {
			free(input);
			return -1;
		}

		uint16_t lcore_id = atoi(token);
		lcore_dma_map->lcore = lcore_id;

		token = strtok(NULL, ", ");
	}

	free(input);
	return 0;
}

static void
parse_cpu_config(struct test_configure *test_case, int case_id,
		     struct rte_cfgfile *cfgfile, char *section_name)
{
	const char *lcore;

	lcore = rte_cfgfile_get_entry(cfgfile, section_name, "lcore");
	int lcore_ret = parse_lcore(test_case, lcore);
	if (lcore_ret < 0) {
		printf("parse lcore error in case %d.\n", case_id);
		test_case->is_valid = false;
	}
}

static int
parse_entry(const char *value, struct test_configure_entry *entry)
{
	char input[255] = {0};
	char *args[MAX_PARAMS_PER_ENTRY];
	int args_nr = -1;
	int ret;

	if (value == NULL || entry == NULL)
		goto out;

	strncpy(input, value, 254);
	if (*input == '\0')
		goto out;

	ret = rte_strsplit(input, strlen(input), args, MAX_PARAMS_PER_ENTRY, ',');
	if (ret != 1 && ret != 4)
		goto out;

	entry->cur = entry->first = (uint32_t)atoi(args[0]);

	if (ret == 4) {
		args_nr = 4;
		entry->last = (uint32_t)atoi(args[1]);
		entry->incr = (uint32_t)atoi(args[2]);
		if (!strcmp(args[3], "MUL"))
			entry->op = OP_MUL;
		else if (!strcmp(args[3], "ADD"))
			entry->op = OP_ADD;
		else {
			args_nr = -1;
			printf("Invalid op %s.\n", args[3]);
		}

	} else {
		args_nr = 1;
		entry->op = OP_NONE;
		entry->last = 0;
		entry->incr = 0;
	}
out:
	return args_nr;
}

static int populate_dma_dev_config(const char *key, const char *value, void *test)
{
	struct lcore_dma_config *dma_config = (struct lcore_dma_config *)test;
	struct vchan_dev_config *vchan_config = &dma_config->vchan_dev;
	struct lcore_dma_map_t *lcore_map = &dma_config->lcore_dma_map;
	char *endptr;
	int ret = 0;

	if (strcmp(key, "lcore") == 0)
		lcore_map->lcore = (uint16_t)atoi(value);
	else if (strcmp(key, "dev") == 0)
		strlcpy(lcore_map->dma_names, value, RTE_DEV_NAME_MAX_LEN);
	else if (strcmp(key, "dir") == 0 && strcmp(value, "mem2mem") == 0)
		vchan_config->tdir = RTE_DMA_DIR_MEM_TO_MEM;
	else if (strcmp(key, "dir") == 0 && strcmp(value, "mem2dev") == 0)
		vchan_config->tdir = RTE_DMA_DIR_MEM_TO_DEV;
	else if (strcmp(key, "dir") == 0 && strcmp(value, "dev2mem") == 0)
		vchan_config->tdir = RTE_DMA_DIR_DEV_TO_MEM;
	else if (strcmp(key, "raddr") == 0)
		vchan_config->raddr = strtoull(value, &endptr, 16);
	else if (strcmp(key, "coreid") == 0)
		vchan_config->port.pcie.coreid = (uint8_t)atoi(value);
	else if (strcmp(key, "vfid") == 0)
		vchan_config->port.pcie.vfid = (uint16_t)atoi(value);
	else if (strcmp(key, "pfid") == 0)
		vchan_config->port.pcie.pfid = (uint16_t)atoi(value);
	else {
		printf("Invalid config param: %s\n", key);
		ret = -1;
	}

	return ret;
}

static void
parse_dma_config(struct test_configure *test_case, int case_id,
		     struct rte_cfgfile *cfgfile, char *section_name, int *nb_vp)
{
	const char *ring_size_str, *kick_batch_str, *src_sges_str, *dst_sges_str, *use_dma_ops;
	char lc_dma[RTE_DEV_NAME_MAX_LEN];
	struct rte_kvargs *kvlist;
	const char *lcore_dma;
	int args_nr;
	int i;

	use_dma_ops = rte_cfgfile_get_entry(cfgfile, section_name, "use_enq_deq_ops");
	test_case->use_ops = (use_dma_ops != NULL && (atoi(use_dma_ops) == 1));

	ring_size_str = rte_cfgfile_get_entry(cfgfile, section_name, "dma_ring_size");
	args_nr = parse_entry(ring_size_str, &test_case->ring_size);
	if (args_nr < 0) {
		printf("parse error in case %d.\n", case_id);
		test_case->is_valid = false;
		return;
	} else if (args_nr == 4)
		*nb_vp += 1;

	src_sges_str = rte_cfgfile_get_entry(cfgfile, section_name, "dma_src_sge");
	if (src_sges_str != NULL)
		test_case->nb_src_sges = (int)atoi(rte_cfgfile_get_entry(cfgfile,
						section_name, "dma_src_sge"));

	dst_sges_str = rte_cfgfile_get_entry(cfgfile, section_name, "dma_dst_sge");
	if (dst_sges_str != NULL)
		test_case->nb_dst_sges = (int)atoi(rte_cfgfile_get_entry(cfgfile,
						section_name, "dma_dst_sge"));

	if ((src_sges_str != NULL && dst_sges_str == NULL) ||
	    (src_sges_str == NULL && dst_sges_str != NULL)) {
		printf("parse dma_src_sge, dma_dst_sge error in case %d.\n", case_id);
		test_case->is_valid = false;
		return;
	} else if (src_sges_str != NULL && dst_sges_str != NULL) {
		if (test_case->nb_src_sges == 0 || test_case->nb_dst_sges == 0) {
			printf("dma_src_sge and dma_dst_sge can not be 0 in case %d.\n", case_id);
			test_case->is_valid = false;
			return;
		}
		test_case->is_sg = true;
	}

	kick_batch_str = rte_cfgfile_get_entry(cfgfile, section_name, "kick_batch");
	args_nr = parse_entry(kick_batch_str, &test_case->kick_batch);
	if (args_nr < 0) {
		printf("parse error in case %d.\n", case_id);
		test_case->is_valid = false;
		return;
	} else if (args_nr == 4)
		*nb_vp += 1;

	i = 0;
	while (1) {
		snprintf(lc_dma, RTE_DEV_NAME_MAX_LEN, "lcore_dma%d", i);
		lcore_dma = rte_cfgfile_get_entry(cfgfile, section_name, lc_dma);
		if (lcore_dma == NULL)
			break;

		kvlist = rte_kvargs_parse(lcore_dma, NULL);
		if (kvlist == NULL) {
			printf("rte_kvargs_parse() error");
			test_case->is_valid = false;
			break;
		}

		if (rte_kvargs_process(kvlist, NULL, populate_dma_dev_config,
				       (void *)&test_case->dma_config[i]) < 0) {
			printf("rte_kvargs_process() error\n");
			rte_kvargs_free(kvlist);
			test_case->is_valid = false;
			break;
		}
		i++;
		test_case->num_worker++;
		rte_kvargs_free(kvlist);
	}

	if (test_case->num_worker == 0) {
		printf("Error: Parsing %s Failed\n", lc_dma);
		test_case->is_valid = false;
	}
}

static int
parse_global_config(struct rte_cfgfile *cfgfile)
{
	static char prog_name[] = "test-dma-perf";
	char *tokens[MAX_EAL_ARGV_NB];
	const char *entry;
	int token_nb;
	char *args;
	int ret;
	int i;

	ret = rte_cfgfile_num_sections(cfgfile, GLOBAL_SECTION_NAME, strlen(GLOBAL_SECTION_NAME));
	if (ret != 1) {
		printf("Error: GLOBAL section not exist or has multiple!\n");
		return -1;
	}

	entry = rte_cfgfile_get_entry(cfgfile, GLOBAL_SECTION_NAME, "eal_args");
	if (entry == NULL) {
		printf("Error: GLOBAL section must have 'eal_args' entry!\n");
		return -1;
	}
	args = strdup(entry);
	if (args == NULL) {
		printf("Error: dup GLOBAL 'eal_args' failed!\n");
		return -1;
	}
	token_nb = rte_strsplit(args, strlen(args), tokens, MAX_EAL_ARGV_NB, ' ');
	global_cfg.eal_argv[0] = prog_name;
	for (i = 0; i < token_nb; i++)
		global_cfg.eal_argv[i + 1] = tokens[i];
	global_cfg.eal_argc = i + 1;

	entry = rte_cfgfile_get_entry(cfgfile, GLOBAL_SECTION_NAME, "cache_flush");
	if (entry == NULL) {
		printf("Error: GLOBAL section don't has 'cache_flush' entry!\n");
		return -1;
	}
	global_cfg.cache_flush = (uint8_t)atoi(entry);

	entry = rte_cfgfile_get_entry(cfgfile, GLOBAL_SECTION_NAME, "test_seconds");
	if (entry == NULL) {
		printf("Error: GLOBAL section don't has 'test_seconds' entry!\n");
		return -1;
	}
	global_cfg.test_secs = (uint16_t)atoi(entry);

	return 0;
}

static uint16_t
load_configs(const char *path)
{
	const char *mem_size_str, *buf_size_str;
	struct test_configure *test_case;
	char section_name[CFG_NAME_LEN];
	struct rte_cfgfile *cfgfile;
	const char *case_type;
	int nb_sections, i;
	int args_nr, nb_vp;
	const char *skip;

	printf("config file parsing...\n");
	cfgfile = rte_cfgfile_load(path, 0);
	if (!cfgfile) {
		printf("Open configure file error.\n");
		exit(1);
	}

	if (parse_global_config(cfgfile) != 0)
		exit(1);

	nb_sections = rte_cfgfile_num_sections(cfgfile, NULL, 0) - 1;
	if (nb_sections > MAX_TEST_CASES) {
		printf("Error: The maximum number of cases is %d.\n", MAX_TEST_CASES);
		exit(1);
	}

	for (i = 0; i < nb_sections; i++) {
		snprintf(section_name, CFG_NAME_LEN, "case%d", i + 1);
		test_case = &test_cases[i];

		skip = rte_cfgfile_get_entry(cfgfile, section_name, "skip");
		if (skip && (atoi(skip) == 1)) {
			test_case->is_skip = true;
			continue;
		}

		test_case->is_valid = true;
		case_type = rte_cfgfile_get_entry(cfgfile, section_name, "type");
		if (case_type == NULL) {
			printf("Error: No case type in case %d, the test will be finished here.\n",
				i + 1);
			test_case->is_valid = false;
			continue;
		}

		if (strcmp(case_type, DMA_MEM_COPY) == 0) {
			test_case->test_type = TEST_TYPE_DMA_MEM_COPY;
		} else if (strcmp(case_type, CPU_MEM_COPY) == 0) {
			test_case->test_type = TEST_TYPE_CPU_MEM_COPY;
		} else {
			printf("Error: Wrong test case type %s in case%d.\n", case_type, i + 1);
			test_case->is_valid = false;
			continue;
		}

		test_case->src_numa_node = (int)atoi(rte_cfgfile_get_entry(cfgfile,
								section_name, "src_numa_node"));
		test_case->dst_numa_node = (int)atoi(rte_cfgfile_get_entry(cfgfile,
								section_name, "dst_numa_node"));
		nb_vp = 0;
		mem_size_str = rte_cfgfile_get_entry(cfgfile, section_name, "mem_size");
		args_nr = parse_entry(mem_size_str, &test_case->mem_size);
		if (args_nr < 0) {
			printf("parse error in case %d.\n", i + 1);
			test_case->is_valid = false;
			continue;
		} else if (args_nr == 4)
			nb_vp++;

		buf_size_str = rte_cfgfile_get_entry(cfgfile, section_name, "buf_size");
		args_nr = parse_entry(buf_size_str, &test_case->buf_size);
		if (args_nr < 0) {
			printf("parse error in case %d.\n", i + 1);
			test_case->is_valid = false;
			continue;
		} else if (args_nr == 4)
			nb_vp++;

		if (test_case->test_type == TEST_TYPE_DMA_MEM_COPY)
			parse_dma_config(test_case, i + 1, cfgfile, section_name, &nb_vp);
		else
			parse_cpu_config(test_case, i + 1, cfgfile, section_name);

		if (test_case->is_valid && nb_vp > 1) {
			printf("Case %d error, each section can only have a single variable parameter.\n",
					i + 1);
			test_case->is_valid = false;
			continue;
		}
	}

	rte_cfgfile_close(cfgfile);
	printf("config file parsing complete.\n\n");
	return i;
}

static int
parse_args(int argc, char **argv)
{
	static struct rte_argparse obj = {
		.prog_name = "test-dma-perf",
		.usage = "[optional parameters]",
		.descriptor = NULL,
		.epilog = NULL,
		.exit_on_error = true,
		.args = {
			{ "--config", NULL, "Specify a configuration file",
			  (void *)&config_path, NULL,
			  RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_STR,
			},
			{ "--result", NULL, "Optional, specify a result file name",
			  (void *)&result_path, NULL,
			  RTE_ARGPARSE_VALUE_REQUIRED, RTE_ARGPARSE_VALUE_TYPE_STR,
			},
			{ "--list-dma", NULL, "Optional, list dma devices and exit",
			  (void *)&list_dma, (void *)true,
			  RTE_ARGPARSE_VALUE_NONE, RTE_ARGPARSE_VALUE_TYPE_BOOL,
			},
			ARGPARSE_ARG_END(),
		},
	};
	char rst_path[PATH_MAX + 16] = {0};
	FILE *fd;
	int ret;

	ret = rte_argparse_parse(&obj, argc, argv);
	if (ret < 0)
		exit(1);

	if (config_path == NULL) {
		printf("Config file not assigned.\n");
		exit(1);
	}

	if (result_path == NULL) {
		rte_basename(config_path, rst_path, sizeof(rst_path));
		char *token = strtok(rst_path, ".");
		if (token == NULL) {
			printf("Config file error.\n");
			return -1;
		}
		strlcat(token, "_result.csv", sizeof(rst_path));
		result_path = strdup(rst_path);
		if (result_path == NULL) {
			printf("Generate result file path error.\n");
			exit(1);
		}
	}
	fd = fopen(result_path, "w");
	if (fd == NULL) {
		printf("Open output CSV file error.\n");
		exit(1);
	}
	fclose(fd);

	return 0;
}

static void
list_dma_dev(void)
{
	static const struct {
		uint64_t capability;
		const char *name;
	} capa_names[] = {
		{ RTE_DMA_CAPA_MEM_TO_MEM,  "mem2mem" },
		{ RTE_DMA_CAPA_MEM_TO_DEV,  "mem2dev" },
		{ RTE_DMA_CAPA_DEV_TO_MEM,  "dev2mem" },
		{ RTE_DMA_CAPA_DEV_TO_DEV,  "dev2dev" },
	};
	struct rte_dma_info info;
	uint32_t count = 0;
	int idx = 0;
	uint32_t i;
	int ret;

	ret = rte_eal_init(global_cfg.eal_argc, global_cfg.eal_argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

	RTE_DMA_FOREACH_DEV(idx) {
		ret = rte_dma_info_get(idx, &info);
		if (ret != 0)
			continue;

		printf("\nDMA device name: %s\n", info.dev_name);
		printf("    transfer-capa:");
		for (i = 0; i < RTE_DIM(capa_names); i++) {
			if (capa_names[i].capability & info.dev_capa)
				printf(" %s", capa_names[i].name);
		}
		printf("\n");
		printf("    max-segs: %u numa-node: %d min-desc: %u max-desc: %u\n",
			info.max_sges, info.numa_node, info.min_desc, info.max_desc);
		count++;
	}

	printf("\n");
	if (count == 0)
		printf("There are no dmadev devices!\n\n");

	rte_eal_cleanup();
}

int
main(int argc, char *argv[])
{
	uint32_t i, nb_lcores;
	pid_t cpid, wpid;
	uint16_t case_nb;
	int wstatus;
	int ret;

	parse_args(argc, argv);

	case_nb = load_configs(config_path);

	if (list_dma) {
		list_dma_dev();
		return 0;
	}

	printf("Running cases...\n");
	for (i = 0; i < case_nb; i++) {
		if (test_cases[i].is_skip) {
			printf("Test case %d configured to be skipped.\n\n", i + 1);
			output_blanklines(2);
			output_csv("Skip the test-case %d\n", i + 1);
			continue;
		}

		if (!test_cases[i].is_valid) {
			printf("Invalid test case %d.\n\n", i + 1);
			output_blanklines(2);
			output_csv("Invalid case %d\n", i + 1);
			continue;
		}

		cpid = fork();
		if (cpid < 0) {
			printf("Fork case %d failed.\n", i + 1);
			exit(EXIT_FAILURE);
		} else if (cpid == 0) {
			printf("\nRunning case %u\n\n", i + 1);

			ret = rte_eal_init(global_cfg.eal_argc, global_cfg.eal_argv);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

			/* Check lcores. */
			nb_lcores = rte_lcore_count();
			if (nb_lcores < 2)
				rte_exit(EXIT_FAILURE,
					"There should be at least 2 worker lcores.\n");

			output_env_info();

			run_test(i + 1, &test_cases[i]);

			/* clean up the EAL */
			rte_eal_cleanup();

			printf("\nCase %u completed.\n\n", i + 1);

			exit(EXIT_SUCCESS);
		} else {
			wpid = waitpid(cpid, &wstatus, 0);
			if (wpid == -1) {
				printf("waitpid error.\n");
				exit(EXIT_FAILURE);
			}

			if (WIFEXITED(wstatus))
				printf("Case process exited. status %d\n\n",
					WEXITSTATUS(wstatus));
			else if (WIFSIGNALED(wstatus))
				printf("Case process killed by signal %d\n\n",
					WTERMSIG(wstatus));
			else if (WIFSTOPPED(wstatus))
				printf("Case process stopped by signal %d\n\n",
					WSTOPSIG(wstatus));
			else if (WIFCONTINUED(wstatus))
				printf("Case process continued.\n\n");
			else
				printf("Case process unknown terminated.\n\n");
		}
	}

	printf("Bye...\n");
	return 0;
}
