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
 * Definitions of DPDK version numbers
 */

#ifndef _RTE_VERSION_H_
#define _RTE_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <rte_common.h>

/**
 * String that appears before the version number
 */
#define RTE_VER_PREFIX "DPDK"

/**
 * Major version/year number i.e. the yy in yy.mm.z
 */
#define RTE_VER_YEAR 17

/**
 * Minor version/month number i.e. the mm in yy.mm.z
 */
#define RTE_VER_MONTH 5

/**
 * Patch level number i.e. the z in yy.mm.z
 */
#define RTE_VER_MINOR 0

/**
 * Extra string to be appended to version number
 */
#define RTE_VER_SUFFIX "-rc"

/**
 * Patch release number
 *   0-15 = release candidates
 *   16   = release
 */
#define RTE_VER_RELEASE 1

/**
 * Macro to compute a version number usable for comparisons
 */
#define RTE_VERSION_NUM(a,b,c,d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

/**
 * All version numbers in one to compare with RTE_VERSION_NUM()
 */
#define RTE_VERSION RTE_VERSION_NUM( \
			RTE_VER_YEAR, \
			RTE_VER_MONTH, \
			RTE_VER_MINOR, \
			RTE_VER_RELEASE)

/**
 * Function returning version string
 * @return
 *     string
 */
static inline const char *
rte_version(void)
{
	static char version[32];
	if (version[0] != 0)
		return version;
	if (strlen(RTE_VER_SUFFIX) == 0)
		snprintf(version, sizeof(version), "%s %d.%02d.%d",
			RTE_VER_PREFIX,
			RTE_VER_YEAR,
			RTE_VER_MONTH,
			RTE_VER_MINOR);
	else
		snprintf(version, sizeof(version), "%s %d.%02d.%d%s%d",
			RTE_VER_PREFIX,
			RTE_VER_YEAR,
			RTE_VER_MONTH,
			RTE_VER_MINOR,
			RTE_VER_SUFFIX,
			RTE_VER_RELEASE < 16 ?
				RTE_VER_RELEASE :
				RTE_VER_RELEASE - 16);
	return version;
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_VERSION_H */
