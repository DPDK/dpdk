/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2017.
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
 *     * Neither the name of Cavium, Inc nor the names of its
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
#include <rte_eventdev_pmd_vdev.h>
#include <rte_io.h>

#include "rte_pmd_octeontx_ssovf.h"

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

#define PCI_VENDOR_ID_CAVIUM              0x177D
#define PCI_DEVICE_ID_OCTEONTX_SSOGRP_VF  0xA04B
#define PCI_DEVICE_ID_OCTEONTX_SSOWS_VF   0xA04D

#define SSO_MAX_VHGRP                     (64)
#define SSO_MAX_VHWS                      (32)

/* SSO VF register offsets */
#define SSO_VHGRP_QCTL                    (0x010ULL)
#define SSO_VHGRP_INT                     (0x100ULL)
#define SSO_VHGRP_INT_W1S                 (0x108ULL)
#define SSO_VHGRP_INT_ENA_W1S             (0x110ULL)
#define SSO_VHGRP_INT_ENA_W1C             (0x118ULL)
#define SSO_VHGRP_INT_THR                 (0x140ULL)
#define SSO_VHGRP_INT_CNT                 (0x180ULL)
#define SSO_VHGRP_XAQ_CNT                 (0x1B0ULL)
#define SSO_VHGRP_AQ_CNT                  (0x1C0ULL)
#define SSO_VHGRP_AQ_THR                  (0x1E0ULL)
#define SSO_VHGRP_PF_MBOX(x)              (0x200ULL | ((x) << 3))

/* BAR2 */
#define SSO_VHGRP_OP_ADD_WORK0            (0x00ULL)
#define SSO_VHGRP_OP_ADD_WORK1            (0x08ULL)

/* SSOW VF register offsets (BAR0) */
#define SSOW_VHWS_GRPMSK_CHGX(x)          (0x080ULL | ((x) << 3))
#define SSOW_VHWS_TAG                     (0x300ULL)
#define SSOW_VHWS_WQP                     (0x308ULL)
#define SSOW_VHWS_LINKS                   (0x310ULL)
#define SSOW_VHWS_PENDTAG                 (0x340ULL)
#define SSOW_VHWS_PENDWQP                 (0x348ULL)
#define SSOW_VHWS_SWTP                    (0x400ULL)
#define SSOW_VHWS_OP_ALLOC_WE             (0x410ULL)
#define SSOW_VHWS_OP_UPD_WQP_GRP0         (0x440ULL)
#define SSOW_VHWS_OP_UPD_WQP_GRP1         (0x448ULL)
#define SSOW_VHWS_OP_SWTAG_UNTAG          (0x490ULL)
#define SSOW_VHWS_OP_SWTAG_CLR            (0x820ULL)
#define SSOW_VHWS_OP_DESCHED              (0x860ULL)
#define SSOW_VHWS_OP_DESCHED_NOSCH        (0x870ULL)
#define SSOW_VHWS_OP_SWTAG_DESCHED        (0x8C0ULL)
#define SSOW_VHWS_OP_SWTAG_NOSCHED        (0x8D0ULL)
#define SSOW_VHWS_OP_SWTP_SET             (0xC20ULL)
#define SSOW_VHWS_OP_SWTAG_NORM           (0xC80ULL)
#define SSOW_VHWS_OP_SWTAG_FULL0          (0xCA0UL)
#define SSOW_VHWS_OP_SWTAG_FULL1          (0xCA8ULL)
#define SSOW_VHWS_OP_CLR_NSCHED           (0x10000ULL)
#define SSOW_VHWS_OP_GET_WORK0            (0x80000ULL)
#define SSOW_VHWS_OP_GET_WORK1            (0x80008ULL)

#define SSOW_BAR4_LEN                     (64 * 1024)

/* Mailbox message constants */
#define SSO_COPROC                        0x2

#define SSO_GETDOMAINCFG                  0x1
#define SSO_IDENTIFY                      0x2
#define SSO_GET_DEV_INFO                  0x3
#define SSO_GET_GETWORK_WAIT              0x4
#define SSO_SET_GETWORK_WAIT              0x5
#define SSO_CONVERT_NS_GETWORK_ITER       0x6
#define SSO_GRP_GET_PRIORITY              0x7
#define SSO_GRP_SET_PRIORITY              0x8

/*
 * In Cavium OcteonTX SoC, all accesses to the device registers are
 * implictly strongly ordered. So, The relaxed version of IO operation is
 * safe to use with out any IO memory barriers.
 */
#define ssovf_read64 rte_read64_relaxed
#define ssovf_write64 rte_write64_relaxed

/* ARM64 specific functions */
#if defined(RTE_ARCH_ARM64)
#define ssovf_load_pair(val0, val1, addr) ({		\
			asm volatile(			\
			"ldp %x[x0], %x[x1], [%x[p1]]"	\
			:[x0]"=r"(val0), [x1]"=r"(val1) \
			:[p1]"r"(addr)			\
			); })

#define ssovf_store_pair(val0, val1, addr) ({		\
			asm volatile(			\
			"stp %x[x0], %x[x1], [%x[p1]]"	\
			::[x0]"r"(val0), [x1]"r"(val1), [p1]"r"(addr) \
			); })
#else /* Un optimized functions for building on non arm64 arch */

#define ssovf_load_pair(val0, val1, addr)		\
do {							\
	val0 = rte_read64(addr);			\
	val1 = rte_read64(((uint8_t *)addr) + 8);	\
} while (0)

#define ssovf_store_pair(val0, val1, addr)		\
do {							\
	rte_write64(val0, addr);			\
	rte_write64(val1, (((uint8_t *)addr) + 8));	\
} while (0)
#endif


struct ssovf_evdev {
	uint8_t max_event_queues;
	uint8_t max_event_ports;
	uint8_t is_timeout_deq;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint32_t min_deq_timeout_ns;
	uint32_t max_deq_timeout_ns;
	int32_t max_num_events;
} __rte_cache_aligned;

/* Event port aka HWS */
struct ssows {
	uint8_t cur_tt;
	uint8_t cur_grp;
	uint8_t swtag_req;
	uint8_t *base;
	uint8_t *getwork;
	uint8_t *grps[SSO_MAX_VHGRP];
	uint8_t port;
} __rte_cache_aligned;

static inline struct ssovf_evdev *
ssovf_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

uint16_t ssows_enq(void *port, const struct rte_event *ev);
uint16_t ssows_enq_burst(void *port,
		const struct rte_event ev[], uint16_t nb_events);
uint16_t ssows_enq_new_burst(void *port,
		const struct rte_event ev[], uint16_t nb_events);
uint16_t ssows_enq_fwd_burst(void *port,
		const struct rte_event ev[], uint16_t nb_events);
uint16_t ssows_deq(void *port, struct rte_event *ev, uint64_t timeout_ticks);
uint16_t ssows_deq_burst(void *port, struct rte_event ev[],
		uint16_t nb_events, uint64_t timeout_ticks);
uint16_t ssows_deq_timeout(void *port, struct rte_event *ev,
		uint64_t timeout_ticks);
uint16_t ssows_deq_timeout_burst(void *port, struct rte_event ev[],
		uint16_t nb_events, uint64_t timeout_ticks);
void ssows_flush_events(struct ssows *ws, uint8_t queue_id);
void ssows_reset(struct ssows *ws);

#endif /* __SSOVF_EVDEV_H__ */
