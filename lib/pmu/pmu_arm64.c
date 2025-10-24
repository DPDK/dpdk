/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_bitops.h>
#include <rte_common.h>

#include "pmu_private.h"

#define PERF_USER_ACCESS_PATH "/proc/sys/kernel/perf_user_access"

static int
read_attr_int(const char *path, int *val)
{
	char buf[BUFSIZ];
	int ret, fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	ret = read(fd, buf, sizeof(buf));
	if (ret == -1) {
		close(fd);

		return -errno;
	}

	*val = strtol(buf, NULL, 10);
	close(fd);

	return 0;
}

static int
pmu_arm64_init(void)
{
	int uaccess, ret;

	ret = read_attr_int(PERF_USER_ACCESS_PATH, &uaccess);
	if (ret)
		return ret;

	if (uaccess != 1)
		PMU_LOG(WARNING, "access to perf counters disabled, "
			"run 'echo 1 > %s' to enable",
			PERF_USER_ACCESS_PATH);

	return ret;
}

static void
pmu_arm64_fini(void)
{
}

static void
pmu_arm64_fixup_config(uint64_t config[3])
{
	/* select 64 bit counters */
	config[1] |= RTE_BIT64(0);
	/* enable userspace access */
	config[1] |= RTE_BIT64(1);
}

static const struct pmu_arch_ops arm64_ops = {
	.init = pmu_arm64_init,
	.fini = pmu_arm64_fini,
	.fixup_config = pmu_arm64_fixup_config,
};
PMU_SET_ARCH_OPS(arm64_ops)
