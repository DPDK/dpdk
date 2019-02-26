/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_version.h>

#include "test.h"


static int
test_version(void)
{
	const char *version = rte_version();
	if (version == NULL)
		return -1;
	printf("Version string: '%s'\n", version);
	if (*version == '\0' ||
			strncmp(version, RTE_VER_PREFIX, sizeof(RTE_VER_PREFIX)-1) != 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(version_autotest, test_version);
