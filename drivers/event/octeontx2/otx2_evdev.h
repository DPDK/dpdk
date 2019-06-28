/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_EVDEV_H__
#define __OTX2_EVDEV_H__

#include <rte_eventdev.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_mempool.h"

#define EVENTDEV_NAME_OCTEONTX2_PMD otx2_eventdev

#define sso_func_trace otx2_sso_dbg

#define OTX2_SSO_MAX_VHGRP                  RTE_EVENT_MAX_QUEUES_PER_DEV
#define OTX2_SSO_MAX_VHWS                   (UINT8_MAX)
#define OTX2_SSO_FC_NAME                    "otx2_evdev_xaq_fc"
#define OTX2_SSO_XAQ_SLACK                  (8)
#define OTX2_SSO_XAQ_CACHE_CNT              (0x7)

/* SSO LF register offsets (BAR2) */
#define SSO_LF_GGRP_OP_ADD_WORK0            (0x0ull)
#define SSO_LF_GGRP_OP_ADD_WORK1            (0x8ull)

#define SSO_LF_GGRP_QCTL                    (0x20ull)
#define SSO_LF_GGRP_EXE_DIS                 (0x80ull)
#define SSO_LF_GGRP_INT                     (0x100ull)
#define SSO_LF_GGRP_INT_W1S                 (0x108ull)
#define SSO_LF_GGRP_INT_ENA_W1S             (0x110ull)
#define SSO_LF_GGRP_INT_ENA_W1C             (0x118ull)
#define SSO_LF_GGRP_INT_THR                 (0x140ull)
#define SSO_LF_GGRP_INT_CNT                 (0x180ull)
#define SSO_LF_GGRP_XAQ_CNT                 (0x1b0ull)
#define SSO_LF_GGRP_AQ_CNT                  (0x1c0ull)
#define SSO_LF_GGRP_AQ_THR                  (0x1e0ull)
#define SSO_LF_GGRP_MISC_CNT                (0x200ull)

#define USEC2NSEC(__us)                 ((__us) * 1E3)

enum otx2_sso_lf_type {
	SSO_LF_GGRP,
	SSO_LF_GWS
};

struct otx2_sso_evdev {
	OTX2_DEV; /* Base class */
	uint8_t max_event_queues;
	uint8_t max_event_ports;
	uint8_t is_timeout_deq;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint8_t configured;
	uint32_t deq_tmo_ns;
	uint32_t min_dequeue_timeout_ns;
	uint32_t max_dequeue_timeout_ns;
	int32_t max_num_events;
	uint64_t *fc_mem;
	uint64_t xaq_lmt;
	uint64_t nb_xaq_cfg;
	rte_iova_t fc_iova;
	struct rte_mempool *xaq_pool;
	/* Dev args */
	uint32_t xae_cnt;
	/* HW const */
	uint32_t xae_waes;
	uint32_t xaq_buf_size;
	uint32_t iue;
} __rte_cache_aligned;

static inline struct otx2_sso_evdev *
sso_pmd_priv(const struct rte_eventdev *event_dev)
{
	return event_dev->data->dev_private;
}

static inline int
parse_kvargs_value(const char *key, const char *value, void *opaque)
{
	RTE_SET_USED(key);

	*(uint32_t *)opaque = (uint32_t)atoi(value);
	return 0;
}

/* Init and Fini API's */
int otx2_sso_init(struct rte_eventdev *event_dev);
int otx2_sso_fini(struct rte_eventdev *event_dev);

#endif /* __OTX2_EVDEV_H__ */
