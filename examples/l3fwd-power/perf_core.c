/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_power.h>
#include <rte_string_fns.h>

#include "perf_core.h"
#include "main.h"


static uint16_t hp_lcores[RTE_MAX_LCORE];
static uint16_t nb_hp_lcores;

struct perf_lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t high_perf;
	uint8_t lcore_idx;
} __rte_cache_aligned;

static struct perf_lcore_params prf_lc_prms[MAX_LCORE_PARAMS];
static uint16_t nb_prf_lc_prms;

static int
is_hp_core(unsigned int lcore)
{
	struct rte_power_core_capabilities caps;
	int ret;

	/* do we have power management enabled? */
	if (rte_power_get_env() == PM_ENV_NOT_SET) {
		/* there's no power management, so just mark it as high perf */
		return 1;
	}
	ret = rte_power_get_capabilities(lcore, &caps);
	return ret == 0 && caps.turbo;
}

int
update_lcore_params(void)
{
	uint8_t non_perf_lcores[RTE_MAX_LCORE];
	uint16_t nb_non_perf_lcores = 0;
	int i, j;

	/* if perf-config option was not used do nothing */
	if (nb_prf_lc_prms == 0)
		return 0;

	/* if high-perf-cores option was not used query every available core */
	if (nb_hp_lcores == 0) {
		for (i = 0; i < RTE_MAX_LCORE; i++) {
			if (rte_lcore_is_enabled(i) && is_hp_core(i)) {
				hp_lcores[nb_hp_lcores] = i;
				nb_hp_lcores++;
			}
		}
	}

	/* create a list on non high performance cores*/
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (rte_lcore_is_enabled(i)) {
			int hp = 0;
			for (j = 0; j < nb_hp_lcores; j++) {
				if (hp_lcores[j] == i) {
					hp = 1;
					break;
				}
			}
			if (!hp)
				non_perf_lcores[nb_non_perf_lcores++] = i;
		}
	}

	/* update the lcore config */
	for (i = 0; i < nb_prf_lc_prms; i++) {
		int lcore = -1;
		if (prf_lc_prms[i].high_perf) {
			if (prf_lc_prms[i].lcore_idx < nb_hp_lcores)
				lcore = hp_lcores[prf_lc_prms[i].lcore_idx];
		} else {
			if (prf_lc_prms[i].lcore_idx < nb_non_perf_lcores)
				lcore =
				non_perf_lcores[prf_lc_prms[i].lcore_idx];
		}

		if (lcore < 0) {
			printf("Performance cores configuration error\n");
			return -1;
		}

		lcore_params_array[i].lcore_id = lcore;
		lcore_params_array[i].queue_id = prf_lc_prms[i].queue_id;
		lcore_params_array[i].port_id = prf_lc_prms[i].port_id;
	}

	lcore_params = lcore_params_array;
	nb_lcore_params = nb_prf_lc_prms;

	printf("Updated performance core configuration\n");
	for (i = 0; i < nb_prf_lc_prms; i++)
		printf("\t(%d,%d,%d)\n", lcore_params[i].port_id,
				lcore_params[i].queue_id,
				lcore_params[i].lcore_id);

	return 0;
}

int
parse_perf_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE_HP,
		FLD_LCORE_IDX,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned int size;

	nb_prf_lc_prms = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') !=
								_NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_prf_lc_prms >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
					nb_prf_lc_prms);
			return -1;
		}
		prf_lc_prms[nb_prf_lc_prms].port_id =
				(uint8_t)int_fld[FLD_PORT];
		prf_lc_prms[nb_prf_lc_prms].queue_id =
				(uint8_t)int_fld[FLD_QUEUE];
		prf_lc_prms[nb_prf_lc_prms].high_perf =
				!!(uint8_t)int_fld[FLD_LCORE_HP];
		prf_lc_prms[nb_prf_lc_prms].lcore_idx =
				(uint8_t)int_fld[FLD_LCORE_IDX];
		++nb_prf_lc_prms;
	}

	return 0;
}

int
parse_perf_core_list(const char *corelist)
{
	int i, idx = 0;
	unsigned int count = 0;
	char *end = NULL;
	int min, max;

	if (corelist == NULL) {
		printf("invalid core list\n");
		return -1;
	}


	/* Remove all blank characters ahead and after */
	while (isblank(*corelist))
		corelist++;
	i = strlen(corelist);
	while ((i > 0) && isblank(corelist[i - 1]))
		i--;

	/* Get list of cores */
	min = RTE_MAX_LCORE;
	do {
		while (isblank(*corelist))
			corelist++;
		if (*corelist == '\0')
			return -1;
		errno = 0;
		idx = strtoul(corelist, &end, 10);
		if (errno || end == NULL)
			return -1;
		while (isblank(*end))
			end++;
		if (*end == '-') {
			min = idx;
		} else if ((*end == ',') || (*end == '\0')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = min; idx <= max; idx++) {
				hp_lcores[count] = idx;
				count++;
			}
			min = RTE_MAX_LCORE;
		} else {
			printf("invalid core list\n");
			return -1;
		}
		corelist = end + 1;
	} while (*end != '\0');

	if (count == 0) {
		printf("invalid core list\n");
		return -1;
	}

	nb_hp_lcores = count;

	printf("Configured %d high performance cores\n", nb_hp_lcores);
	for (i = 0; i < nb_hp_lcores; i++)
		printf("\tHigh performance core %d %d\n",
				i, hp_lcores[i]);

	return 0;
}
