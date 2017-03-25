/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIO_LOGS_H_
#define _LIO_LOGS_H_

#define lio_dev_printf(lio_dev, level, fmt, args...)			\
	RTE_LOG(level, PMD, "%s" fmt, (lio_dev)->dev_string, ##args)

#define lio_dev_info(lio_dev, fmt, args...)				\
	lio_dev_printf(lio_dev, INFO, "INFO: " fmt, ##args)

#define lio_dev_err(lio_dev, fmt, args...)				\
	lio_dev_printf(lio_dev, ERR, "ERROR: %s() " fmt, __func__, ##args)

#define PMD_INIT_LOG(level, fmt, args...) RTE_LOG(level, PMD, fmt, ## args)

/* Enable these through config options */

#ifdef RTE_LIBRTE_LIO_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, "%s() >>\n", __func__)
#else /* !RTE_LIBRTE_LIO_DEBUG_INIT */
#define PMD_INIT_FUNC_TRACE() do { } while (0)
#endif /* RTE_LIBRTE_LIO_DEBUG_INIT */

#ifdef RTE_LIBRTE_LIO_DEBUG_DRIVER
#define lio_dev_dbg(lio_dev, fmt, args...)				\
	lio_dev_printf(lio_dev, DEBUG, "DEBUG: %s() " fmt, __func__, ##args)
#else /* !RTE_LIBRTE_LIO_DEBUG_DRIVER */
#define lio_dev_dbg(lio_dev, fmt, args...) do { } while (0)
#endif /* RTE_LIBRTE_LIO_DEBUG_DRIVER */

#ifdef RTE_LIBRTE_LIO_DEBUG_RX
#define PMD_RX_LOG(lio_dev, level, fmt, args...)			\
	lio_dev_printf(lio_dev, level, "RX: %s() " fmt, __func__, ##args)
#else /* !RTE_LIBRTE_LIO_DEBUG_RX */
#define PMD_RX_LOG(lio_dev, level, fmt, args...) do { } while (0)
#endif /* RTE_LIBRTE_LIO_DEBUG_RX */

#ifdef RTE_LIBRTE_LIO_DEBUG_TX
#define PMD_TX_LOG(lio_dev, level, fmt, args...)			\
	lio_dev_printf(lio_dev, level, "TX: %s() " fmt, __func__, ##args)
#else /* !RTE_LIBRTE_LIO_DEBUG_TX */
#define PMD_TX_LOG(lio_dev, level, fmt, args...) do { } while (0)
#endif /* RTE_LIBRTE_LIO_DEBUG_TX */

#ifdef RTE_LIBRTE_LIO_DEBUG_MBOX
#define PMD_MBOX_LOG(lio_dev, level, fmt, args...)			\
	lio_dev_printf(lio_dev, level, "MBOX: %s() " fmt, __func__, ##args)
#else /* !RTE_LIBRTE_LIO_DEBUG_MBOX */
#define PMD_MBOX_LOG(level, fmt, args...) do { } while (0)
#endif /* RTE_LIBRTE_LIO_DEBUG_MBOX */

#ifdef RTE_LIBRTE_LIO_DEBUG_REGS
#define PMD_REGS_LOG(lio_dev, fmt, args...)				\
	lio_dev_printf(lio_dev, DEBUG, "REGS: " fmt, ##args)
#else /* !RTE_LIBRTE_LIO_DEBUG_REGS */
#define PMD_REGS_LOG(level, fmt, args...) do { } while (0)
#endif /* RTE_LIBRTE_LIO_DEBUG_REGS */

#endif  /* _LIO_LOGS_H_ */
