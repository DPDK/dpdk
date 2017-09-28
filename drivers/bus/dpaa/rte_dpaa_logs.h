/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
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
 *     * Neither the name of NXP nor the names of its
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

#ifndef _DPAA_LOGS_H_
#define _DPAA_LOGS_H_

#include <rte_log.h>

extern int dpaa_logtype_bus;
extern int dpaa_logtype_mempool;
extern int dpaa_logtype_pmd;

#define DPAA_BUS_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_bus, "%s(): " fmt "\n", \
		__func__, ##args)

#define BUS_INIT_FUNC_TRACE() DPAA_BUS_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_DPAA_DEBUG_BUS
#define DPAA_BUS_HWWARN(cond, fmt, args...) \
	do {\
		if (cond) \
			DPAA_BUS_LOG(DEBUG, "WARN: " fmt, ##args); \
	} while (0)
#else
#define DPAA_BUS_HWWARN(cond, fmt, args...) do { } while (0)
#endif

#define DPAA_BUS_DEBUG(fmt, args...) \
	DPAA_BUS_LOG(DEBUG, fmt, ## args)
#define DPAA_BUS_INFO(fmt, args...) \
	DPAA_BUS_LOG(INFO, fmt, ## args)
#define DPAA_BUS_ERR(fmt, args...) \
	DPAA_BUS_LOG(ERR, fmt, ## args)
#define DPAA_BUS_WARN(fmt, args...) \
	DPAA_BUS_LOG(WARNING, fmt, ## args)

/* Mempool related logs */

#define DPAA_MEMPOOL_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_mempool, "%s(): " fmt "\n", \
		__func__, ##args)

#define MEMPOOL_INIT_FUNC_TRACE() DPAA_MEMPOOL_LOG(DEBUG, " >>")

#define DPAA_MEMPOOL_DPDEBUG(fmt, args...) \
	RTE_LOG_DP(DEBUG, PMD, fmt, ## args)
#define DPAA_MEMPOOL_DEBUG(fmt, args...) \
	DPAA_MEMPOOL_LOG(DEBUG, fmt, ## args)
#define DPAA_MEMPOOL_ERR(fmt, args...) \
	DPAA_MEMPOOL_LOG(ERR, fmt, ## args)
#define DPAA_MEMPOOL_INFO(fmt, args...) \
	DPAA_MEMPOOL_LOG(INFO, fmt, ## args)
#define DPAA_MEMPOOL_WARN(fmt, args...) \
	DPAA_MEMPOOL_LOG(WARNING, fmt, ## args)

/* PMD related logs */

#define DPAA_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_pmd, "%s(): " fmt "\n", \
		__func__, ##args)

#define PMD_INIT_FUNC_TRACE() DPAA_PMD_LOG(DEBUG, " >>")

#define DPAA_PMD_DEBUG(fmt, args...) \
	DPAA_PMD_LOG(DEBUG, fmt, ## args)
#define DPAA_PMD_ERR(fmt, args...) \
	DPAA_PMD_LOG(ERR, fmt, ## args)
#define DPAA_PMD_INFO(fmt, args...) \
	DPAA_PMD_LOG(INFO, fmt, ## args)
#define DPAA_PMD_WARN(fmt, args...) \
	DPAA_PMD_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#endif /* _DPAA_LOGS_H_ */
