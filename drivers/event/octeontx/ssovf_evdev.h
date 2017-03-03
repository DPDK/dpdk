/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2017.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#ifndef __SSOVF_EVDEV_H__
#define __SSOVF_EVDEV_H__

#include <rte_config.h>

#define EVENTDEV_NAME_OCTEONTX_PMD event_octeontx

#ifdef RTE_LIBRTE_PMD_OCTEONTX_SSOVF_DEBUG
#define ssovf_log_info(fmt, args...) \
	RTE_LOG(INFO, EVENTDEV, "[%s] %s() " fmt "\n", \
		RTE_STR(EVENTDEV_NAME_OCTEONTX_PMD), __func__, ## args)
#define ssovf_log_dbg(fmt, args...) \
	RTE_LOG(DEBUG, EVENTDEV, "[%s] %s() " fmt "\n", \
		RTE_STR(EVENTDEV_NAME_OCTEONTX_PMD), __func__, ## args)
#else
#define ssovf_log_info(fmt, args...)
#define ssovf_log_dbg(fmt, args...)
#endif

#define ssovf_func_trace ssovf_log_dbg
#define ssovf_log_err(fmt, args...) \
	RTE_LOG(ERR, EVENTDEV, "[%s] %s() " fmt "\n", \
		RTE_STR(EVENTDEV_NAME_OCTEONTX_PMD), __func__, ## args)

#endif /* __SSOVF_EVDEV_H__ */
