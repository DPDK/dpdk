/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include "test_stats.h"
#include "test_inference_common.h"
#include "test_model_ops.h"

int
ml_stats_get(struct ml_test *test, struct ml_options *opt, enum rte_ml_dev_xstats_mode mode,
	     int32_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int32_t model_id;
	int ret;
	int i;

	if (!opt->stats)
		return 0;

	if (mode == RTE_ML_DEV_XSTATS_MODEL)
		model_id = ((struct test_inference *)t)->model[fid].id;
	else
		model_id = -1;

	/* get xstats size */
	t->xstats_size = rte_ml_dev_xstats_names_get(opt->dev_id, mode, model_id, NULL, 0);
	if (t->xstats_size > 0) {
		/* allocate for xstats_map and values */
		t->xstats_map = rte_malloc(
			"ml_xstats_map", t->xstats_size * sizeof(struct rte_ml_dev_xstats_map), 0);
		if (t->xstats_map == NULL) {
			ret = -ENOMEM;
			goto error;
		}

		t->xstats_values =
			rte_malloc("ml_xstats_values", t->xstats_size * sizeof(uint64_t), 0);
		if (t->xstats_values == NULL) {
			ret = -ENOMEM;
			goto error;
		}

		ret = rte_ml_dev_xstats_names_get(opt->dev_id, mode, model_id, t->xstats_map,
						  t->xstats_size);
		if (ret != t->xstats_size) {
			printf("Unable to get xstats names, ret = %d\n", ret);
			ret = -1;
			goto error;
		}

		for (i = 0; i < t->xstats_size; i++)
			rte_ml_dev_xstats_get(opt->dev_id, mode, model_id, &t->xstats_map[i].id,
					      &t->xstats_values[i], 1);
	}

	/* print xstats*/
	printf("\n");
	ml_print_line(80);
	if (mode == RTE_ML_DEV_XSTATS_MODEL)
		printf(" Model Statistics: %s\n",
		       ((struct test_inference *)t)->model[fid].info.name);
	else
		printf(" Device Statistics\n");
	ml_print_line(80);
	for (i = 0; i < t->xstats_size; i++)
		printf(" %-64s = %" PRIu64 "\n", t->xstats_map[i].name, t->xstats_values[i]);
	ml_print_line(80);

	rte_free(t->xstats_map);
	rte_free(t->xstats_values);

	return 0;

error:
	rte_free(t->xstats_map);
	rte_free(t->xstats_values);

	return ret;
}

int
ml_throughput_get(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t = ml_test_priv(test);
	uint64_t total_cycles = 0;
	uint32_t nb_filelist;
	uint64_t throughput;
	uint64_t avg_e2e;
	uint32_t qp_id;
	uint64_t freq;

	if (!opt->stats)
		return 0;

	/* print inference throughput */
	if (strcmp(opt->test_name, "inference_ordered") == 0)
		nb_filelist = 1;
	else
		nb_filelist = opt->nb_filelist;

	/* Print model end-to-end latency and throughput */
	freq = rte_get_tsc_hz();
	for (qp_id = 0; qp_id < RTE_MAX_LCORE; qp_id++)
		total_cycles += t->args[qp_id].end_cycles - t->args[qp_id].start_cycles;

	avg_e2e = total_cycles / (opt->repetitions * nb_filelist);
	if (freq == 0) {
		printf(" %-64s = %" PRIu64 "\n", "Average End-to-End Latency (cycles)", avg_e2e);
	} else {
		avg_e2e = (avg_e2e * NS_PER_S) / freq;
		printf(" %-64s = %" PRIu64 "\n", "Average End-to-End Latency (ns)", avg_e2e);
	}

	/* Print model throughput */
	if (freq == 0) {
		throughput = 1000000 / avg_e2e;
		printf(" %-64s = %" PRIu64 "\n", "Average Throughput (inferences / million cycles)",
		       throughput);
	} else {
		throughput = freq / avg_e2e;
		printf(" %-64s = %" PRIu64 "\n", "Average Throughput (inferences / second)",
		       throughput);
	}

	ml_print_line(80);

	return 0;
}
