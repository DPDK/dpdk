/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

/**
 * @file
 * Stores functions and path defines for files and directories
 * on the filesystem for Linux, that are used by the Linux EAL.
 */

#ifndef EAL_FILESYSTEM_H
#define EAL_FILESYSTEM_H

/** Path of rte config file. */
#define RUNTIME_CONFIG_FMT "%s/.%s_config"

#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>

#include <rte_string_fns.h>
#include "eal_internal_cfg.h"

static const char *default_config_dir = "/var/run";

static inline const char *
eal_runtime_config_path(void)
{
	static char buffer[PATH_MAX]; /* static so auto-zeroed */
	const char *directory = default_config_dir;
	const char *home_dir = getenv("HOME");

	if (getuid() != 0 && home_dir != NULL)
		directory = home_dir;
	snprintf(buffer, sizeof(buffer) - 1, RUNTIME_CONFIG_FMT, directory,
			internal_config.hugefile_prefix);
	return buffer;
}

/** Path of primary/secondary communication unix socket file. */
#define MP_SOCKET_PATH_FMT "%s/.%s_unix"
static inline const char *
eal_mp_socket_path(void)
{
	static char buffer[PATH_MAX]; /* static so auto-zeroed */
	const char *directory = default_config_dir;
	const char *home_dir = getenv("HOME");

	if (getuid() != 0 && home_dir != NULL)
		directory = home_dir;
	snprintf(buffer, sizeof(buffer) - 1, MP_SOCKET_PATH_FMT,
		 directory, internal_config.hugefile_prefix);

	return buffer;
}

/** Path of hugepage info file. */
#define HUGEPAGE_INFO_FMT "%s/.%s_hugepage_info"

static inline const char *
eal_hugepage_info_path(void)
{
	static char buffer[PATH_MAX]; /* static so auto-zeroed */
	const char *directory = default_config_dir;
	const char *home_dir = getenv("HOME");

	if (getuid() != 0 && home_dir != NULL)
		directory = home_dir;
	snprintf(buffer, sizeof(buffer) - 1, HUGEPAGE_INFO_FMT, directory,
			internal_config.hugefile_prefix);
	return buffer;
}

/** String format for hugepage map files. */
#define HUGEFILE_FMT "%s/%smap_%d"
#define TEMP_HUGEFILE_FMT "%s/%smap_temp_%d"

static inline const char *
eal_get_hugefile_path(char *buffer, size_t buflen, const char *hugedir, int f_id)
{
	snprintf(buffer, buflen, HUGEFILE_FMT, hugedir,
			internal_config.hugefile_prefix, f_id);
	buffer[buflen - 1] = '\0';
	return buffer;
}

/** define the default filename prefix for the %s values above */
#define HUGEFILE_PREFIX_DEFAULT "rte"

/** Function to read a single numeric value from a file on the filesystem.
 * Used to read information from files on /sys */
int eal_parse_sysfs_value(const char *filename, unsigned long *val);

#endif /* EAL_FILESYSTEM_H */
