/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <sys/queue.h>

#include <rte_common.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "cmdline_test.h"

int
main(int __rte_unused argc, char __rte_unused ** argv)
{
	struct cmdline *cl;

	cl = cmdline_stdin_new(main_ctx, "CMDLINE_TEST>>");
	if (cl == NULL) {
		return -1;
	}
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);

	return 0;
}
