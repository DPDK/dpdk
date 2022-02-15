/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>

#include <rte_lcore.h>
#include <rte_power.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_string_fns.h>

#include "vm_power_cli_guest.h"
#include "parse.h"

static void
sig_handler(int signo)
{
	printf("Received signal %d, exiting...\n", signo);
	unsigned lcore_id;

	RTE_LCORE_FOREACH(lcore_id) {
		rte_power_exit(lcore_id);
	}

}

#define MAX_HOURS 24

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	const struct option lgopts[] = {
		{ "vm-name", required_argument, 0, 'n'},
		{ "busy-hours", required_argument, 0, 'b'},
		{ "quiet-hours", required_argument, 0, 'q'},
		{ "port-list", required_argument, 0, 'p'},
		{ "vcpu-list", required_argument, 0, 'l'},
		{ "policy", required_argument, 0, 'o'},
		{NULL, 0, 0, 0}
	};
	struct rte_power_channel_packet *policy;
	unsigned short int hours[MAX_HOURS];
	unsigned short int cores[RTE_POWER_MAX_VCPU_PER_VM];
	unsigned short int ports[RTE_POWER_MAX_VCPU_PER_VM];
	int i, cnt, idx;

	policy = get_policy();
	ret = set_policy_defaults(policy);
	if (ret != 0) {
		printf("Failed to set policy defaults\n");
		return -1;
	}

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "n:b:q:p:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'n':
			strlcpy(policy->vm_name, optarg,
					RTE_POWER_VM_MAX_NAME_SZ);
			printf("Setting VM Name to [%s]\n", policy->vm_name);
			break;
		case 'b':
		case 'q':
			//printf("***Processing set using [%s]\n", optarg);
			cnt = parse_set(optarg, hours, MAX_HOURS);
			if (cnt < 0) {
				printf("Invalid value passed to quiet/busy hours - [%s]\n",
						optarg);
				break;
			}
			idx = 0;
			for (i = 0; i < MAX_HOURS; i++) {
				if (hours[i]) {
					if (opt == 'b') {
						printf("***Busy Hour %d\n", i);
						policy->timer_policy.busy_hours
							[idx++] = i;
					} else {
						printf("***Quiet Hour %d\n", i);
						policy->timer_policy.quiet_hours
							[idx++] = i;
					}
				}
			}
			break;
		case 'l':
			cnt = parse_set(optarg, cores,
					RTE_POWER_MAX_VCPU_PER_VM);
			if (cnt < 0) {
				printf("Invalid value passed to vcpu-list - [%s]\n",
						optarg);
				break;
			}
			idx = 0;
			for (i = 0; i < RTE_POWER_MAX_VCPU_PER_VM; i++) {
				if (cores[i]) {
					printf("***Using core %d\n", i);
					policy->vcpu_to_control[idx++] = i;
				}
			}
			policy->num_vcpu = idx;
			printf("Total cores: %d\n", idx);
			break;
		case 'p':
			cnt = parse_set(optarg, ports,
					RTE_POWER_MAX_VCPU_PER_VM);
			if (cnt < 0) {
				printf("Invalid value passed to port-list - [%s]\n",
						optarg);
				break;
			}
			idx = 0;
			for (i = 0; i < RTE_POWER_MAX_VCPU_PER_VM; i++) {
				if (ports[i]) {
					printf("***Using port %d\n", i);
					if (set_policy_mac(i, idx++) != 0) {
						printf("Cannot set policy MAC");
						return -1;
					}
				}
			}
			policy->nb_mac_to_monitor = idx;
			printf("Total Ports: %d\n", idx);
			break;
		case 'o':
			if (!strcmp(optarg, "TRAFFIC"))
				policy->policy_to_use =
						RTE_POWER_POLICY_TRAFFIC;
			else if (!strcmp(optarg, "TIME"))
				policy->policy_to_use =
						RTE_POWER_POLICY_TIME;
			else if (!strcmp(optarg, "WORKLOAD"))
				policy->policy_to_use =
						RTE_POWER_POLICY_WORKLOAD;
			else if (!strcmp(optarg, "BRANCH_RATIO"))
				policy->policy_to_use =
						RTE_POWER_POLICY_BRANCH_RATIO;
			else {
				printf("Invalid policy specified: %s\n",
						optarg);
				return -1;
			}
			break;
		/* long options */

		case 0:
			break;

		default:
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");

	rte_power_set_env(PM_ENV_KVM_VM);
	RTE_LCORE_FOREACH(lcore_id) {
		rte_power_init(lcore_id);
	}
	run_cli(NULL);

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
