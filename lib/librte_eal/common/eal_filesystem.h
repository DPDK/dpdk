/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Stores functions and path defines for files and directories
 * on the filesystem for Linux, that are used by the Linux EAL.
 */

#ifndef _EAL_LINUXAPP_FILESYSTEM_H
#define _EAL_LINUXAPP_FILESYSTEM_H

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

#ifdef RTE_EAL_SINGLE_FILE_SEGMENTS
static inline const char *
eal_get_hugefile_temp_path(char *buffer, size_t buflen, const char *hugedir, int f_id)
{
	snprintf(buffer, buflen, TEMP_HUGEFILE_FMT, hugedir,
			internal_config.hugefile_prefix, f_id);
	buffer[buflen - 1] = '\0';
	return buffer;
}
#endif

/** define the default filename prefix for the %s values above */
#define HUGEFILE_PREFIX_DEFAULT "rte"

/** Function to read a single numeric value from a file on the filesystem.
 * Used to read information from files on /sys */
int eal_parse_sysfs_value(const char *filename, unsigned long *val);

#endif /* _EAL_LINUXAPP_FILESYSTEM_H */
