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
 *
 * API for error cause tracking
 */

#ifndef _RTE_ERRNO_H_
#define _RTE_ERRNO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_per_lcore.h>

RTE_DECLARE_PER_LCORE(int, _rte_errno); /**< Per core error number. */

/**
 * Error number value, stored per-thread, which can be queried after
 * calls to certain functions to determine why those functions failed.
 *
 * Uses standard values from errno.h wherever possible, with a small number
 * of additional possible values for RTE-specific conditions.
 */
#define rte_errno RTE_PER_LCORE(_rte_errno)

/**
 * Function which returns a printable string describing a particular
 * error code. For non-RTE-specific error codes, this function returns
 * the value from the libc strerror function.
 *
 * @param errnum
 *   The error number to be looked up - generally the value of rte_errno
 * @return
 *   A pointer to a thread-local string containing the text describing
 *   the error.
 */
const char *rte_strerror(int errnum);

#ifndef __ELASTERROR
/**
 * Check if we have a defined value for the max system-defined errno values.
 * if no max defined, start from 1000 to prevent overlap with standard values
 */
#define __ELASTERROR 1000
#endif

/** Error types */
enum {
	RTE_MIN_ERRNO = __ELASTERROR, /**< Start numbering above std errno vals */

	E_RTE_SECONDARY, /**< Operation not allowed in secondary processes */
	E_RTE_NO_CONFIG, /**< Missing rte_config */
	E_RTE_NO_TAILQ,  /**< Uninitialised TAILQ */

	RTE_MAX_ERRNO    /**< Max RTE error number */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ERRNO_H_ */
