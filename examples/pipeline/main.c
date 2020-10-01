/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <rte_launch.h>
#include <rte_eal.h>

#include "obj.h"
#include "thread.h"

int
main(int argc, char **argv)
{
	struct obj *obj;
	int status;

	/* EAL */
	status = rte_eal_init(argc, argv);
	if (status < 0) {
		printf("Error: EAL initialization failed (%d)\n", status);
		return status;
	};

	/* Obj */
	obj = obj_init();
	if (!obj) {
		printf("Error: Obj initialization failed (%d)\n", status);
		return status;
	}

	/* Thread */
	status = thread_init();
	if (status) {
		printf("Error: Thread initialization failed (%d)\n", status);
		return status;
	}

	rte_eal_mp_remote_launch(
		thread_main,
		NULL,
		SKIP_MASTER);

	return 0;
}
