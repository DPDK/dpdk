/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_option.h>

#include "eal_private.h"
#include "eal_internal_cfg.h" /* Necessary for eal_options.h */
#include "eal_options.h"

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
		if (strcmp(&opt[2], option->name) == 0) {
			option->enabled = 1;
			return 0;
		}
	}

	return -1;
}

__rte_experimental
int
rte_option_register(struct rte_option *opt)
{
	struct rte_option *option;
	const struct option *gopt;

	gopt = &eal_long_options[0];
	while (gopt->name != NULL) {
		if (strcmp(gopt->name, opt->name) == 0) {
			RTE_LOG(ERR, EAL, "Option %s is already a common EAL option.\n",
					opt->name);
			return -1;
		}
		gopt++;
	}

	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (strcmp(opt->name, option->name) == 0) {
			RTE_LOG(ERR, EAL, "Option %s has already been registered.\n",
					opt->name);
			return -1;
		}
	}

	TAILQ_INSERT_HEAD(&rte_option_list, opt, next);
	return 0;
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
		printf("  --%-*s %s\n", 17, option->name, option->usage);
	printf("\n");
}
