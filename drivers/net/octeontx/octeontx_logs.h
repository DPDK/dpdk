/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
#ifndef __OCTEONTX_LOGS_H__
#define __OCTEONTX_LOGS_H__

#define PMD_INIT_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)

#ifdef RTE_LIBRTE_OCTEONTX_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, ">>")
#else
#define PMD_INIT_FUNC_TRACE() do { } while (0)
#endif

#ifdef RTE_LIBRTE_OCTEONTX_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_OCTEONTX_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_OCTEONTX_DEBUG_DRIVER
#define PMD_DRV_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_DRV_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_OCTEONTX_DEBUG_MBOX
#define PMD_MBOX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_MBOX_LOG(level, fmt, args...) do { } while (0)
#endif

#define octeontx_log_err(s, ...) PMD_INIT_LOG(ERR, s, ##__VA_ARGS__)
#define octeontx_log_dbg(s, ...) PMD_DRV_LOG(DEBUG, s, ##__VA_ARGS__)
#define octeontx_mbox_log(s, ...) PMD_MBOX_LOG(DEBUG, s, ##__VA_ARGS__)

#endif /* __OCTEONTX_LOGS_H__*/
