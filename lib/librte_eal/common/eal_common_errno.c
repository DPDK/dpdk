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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

RTE_DEFINE_PER_LCORE(int, _rte_errno);

const char *
rte_strerror(int errnum)
{
#define RETVAL_SZ 256
	static RTE_DEFINE_PER_LCORE(char[RETVAL_SZ], retval);

	/* since some implementations of strerror_r throw an error
	 * themselves if errnum is too big, we handle that case here */
	if (errnum > RTE_MAX_ERRNO)
		snprintf(RTE_PER_LCORE(retval), RETVAL_SZ,
#ifdef RTE_EXEC_ENV_BSDAPP
				"Unknown error: %d", errnum);
#else
				"Unknown error %d", errnum);
#endif
	else
		switch (errnum){
		case E_RTE_SECONDARY:
			return "Invalid call in secondary process";
		case E_RTE_NO_CONFIG:
			return "Missing rte_config structure";
		case E_RTE_NO_TAILQ:
			return "No TAILQ initialised";
		default:
			strerror_r(errnum, RTE_PER_LCORE(retval), RETVAL_SZ);
		}

	return RTE_PER_LCORE(retval);
}
