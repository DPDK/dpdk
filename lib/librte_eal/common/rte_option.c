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

static struct rte_option *option;

int
rte_option_parse(const char *opt)
{
	/* Check if the option is registered */
	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (strcmp(opt, option->opt_str) == 0) {
			option->enabled = 1;
			return 0;
		}
	}

	return -1;
}

void __rte_experimental
rte_option_register(struct rte_option *opt)
{
	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (strcmp(opt->opt_str, option->opt_str) == 0)
			RTE_LOG(INFO, EAL, "Option %s has already been registered.",
					opt->opt_str);
			return;
	}

	TAILQ_INSERT_HEAD(&rte_option_list, opt, next);
}

void
rte_option_init(void)
{
	TAILQ_FOREACH(option, &rte_option_list, next) {
		if (option->enabled)
			option->cb();
	}
}
