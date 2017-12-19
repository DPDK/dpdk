/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rte_eal.h>

#include <rte_log.h>
#include <rte_memzone.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>


#include "qwctl.h"
#include "commands.h"
#include "../include/conf.h"


int *quota;
unsigned int *low_watermark;
unsigned int *high_watermark;


static void
setup_shared_variables(void)
{
	const struct rte_memzone *qw_memzone;

	qw_memzone = rte_memzone_lookup(QUOTA_WATERMARK_MEMZONE_NAME);
	if (qw_memzone == NULL)
		rte_exit(EXIT_FAILURE, "Couldn't find memzone\n");

	quota = qw_memzone->addr;
	low_watermark = (unsigned int *) qw_memzone->addr + 1;
	high_watermark = (unsigned int *) qw_memzone->addr + 2;
}

int main(int argc, char **argv)
{
	int ret;
	struct cmdline *cl;

	rte_log_set_global_level(RTE_LOG_INFO);

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");

	setup_shared_variables();

	cl = cmdline_stdin_new(qwctl_ctx, "qwctl> ");
	if (cl == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create cmdline instance\n");

	cmdline_interact(cl);
	cmdline_stdin_exit(cl);

	return 0;
}
