/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_lcore.h>

#include "args.h"


unsigned int portmask = 0;


static void
usage(const char *prgname)
{
	fprintf(stderr, "Usage: %s [EAL args] -- -p <portmask>\n"
			"-p PORTMASK: hexadecimal bitmask of NIC ports to configure\n",
			prgname);
}

static unsigned long
parse_portmask(const char *portmask_str)
{
	return strtoul(portmask_str, NULL, 16);
}

static void
check_core_count(void)
{
	if (rte_lcore_count() < 3)
		rte_exit(EXIT_FAILURE,
				"At least 3 cores need to be passed in the coremask\n");
}

static void
check_portmask_value(unsigned int portmask)
{
	unsigned int port_nb = 0;

	port_nb = __builtin_popcount(portmask);

	if (port_nb == 0)
		rte_exit(EXIT_FAILURE,
				"At least 2 ports need to be passed in the portmask\n");

	if (port_nb % 2 != 0)
		rte_exit(EXIT_FAILURE,
				"An even number of ports is required in the portmask\n");
}

int
parse_qw_args(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "h:p:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			break;
		case 'p':
			portmask = parse_portmask(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	check_core_count();
	check_portmask_value(portmask);

	return 0;
}
