/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_debug.h>

#include "commands.h"

/* Initialization of the Environment Abstraction Layer (EAL). 8< */
int main(int argc, char **argv)
{
	int ret;
	struct cmdline *cl;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer (EAL). */

	/* Creating a new command line object. 8< */
	cl = cmdline_stdin_new(main_ctx, "example> ");
	if (cl == NULL)
		rte_panic("Cannot create cmdline instance\n");
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
	/* >8 End of creating a new command line object. */

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
