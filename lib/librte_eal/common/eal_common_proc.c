/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <rte_eal.h>

#include "eal_filesystem.h"
#include "eal_internal_cfg.h"

int
rte_eal_primary_proc_alive(const char *config_file_path)
{
	int config_fd;

	if (config_file_path)
		config_fd = open(config_file_path, O_RDONLY);
	else {
		const char *path;

		path = eal_runtime_config_path();
		config_fd = open(path, O_RDONLY);
	}
	if (config_fd < 0)
		return 0;

	int ret = lockf(config_fd, F_TEST, 0);
	close(config_fd);

	return !!ret;
}
