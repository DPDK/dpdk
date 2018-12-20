/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#include <unistd.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_option.h>

#include "eal_private.h"

TAILQ_HEAD(rte_option_list, rte_option);

struct rte_option_list rte_option_list =
	TAILQ_HEAD_INITIALIZER(rte_option_list);

int
rte_option_parse(const char *opt)
{
	struct rte_option *option;

	if (strlen(opt) <= 2 ||
	    strncmp(opt, "--", 2))
		return -1;

	/* Check if the option is registered */
	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (strcmp(&opt[2], option->opt_str) == 0) {
			option->enabled = 1;
			return 0;
		}
	}

	return -1;
}

void __rte_experimental
rte_option_register(struct rte_option *opt)
{
	struct rte_option *option;

	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (strcmp(opt->opt_str, option->opt_str) == 0) {
			RTE_LOG(INFO, EAL, "Option %s has already been registered.\n",
					opt->opt_str);
			return;
		}
	}

	TAILQ_INSERT_HEAD(&rte_option_list, opt, next);
}

void
rte_option_init(void)
{
	struct rte_option *option;

	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (option->enabled)
			option->cb();
	}
}

void
rte_option_usage(void)
{
	struct rte_option *option;
	int opt_count = 0;

	TAILQ_FOREACH(option, &rte_option_list, next)
		opt_count += 1;
	if (opt_count == 0)
		return;

	printf("EAL dynamic options:\n");
	TAILQ_FOREACH(option, &rte_option_list, next)
		printf("  --%-*s %s\n", 17, option->opt_str, option->usage);
	printf("\n");
}
