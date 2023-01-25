/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef __CNXK_EVENTDEV_DP_H__
#define __CNXK_EVENTDEV_DP_H__

#include "hw/sso.h"

#define CNXK_SSO_MAX_HWGRP     (RTE_EVENT_MAX_QUEUES_PER_DEV + 1)
#define CNXK_SSO_FC_NAME       "cnxk_evdev_xaq_fc"
#define CNXK_SSO_MZ_NAME       "cnxk_evdev_mz"
#define CNXK_SSO_XAQ_CACHE_CNT (0x7)
#define CNXK_SSO_XAQ_SLACK     (8)
#define CNXK_SSO_WQE_SG_PTR    (9)
#define CNXK_SSO_WQE_LAYR_PTR  (5)
#define CNXK_SSO_PRIORITY_CNT  (0x8)
#define CNXK_SSO_WEIGHT_MAX    (0x3f)
#define CNXK_SSO_WEIGHT_MIN    (0x3)
#define CNXK_SSO_WEIGHT_CNT    (CNXK_SSO_WEIGHT_MAX - CNXK_SSO_WEIGHT_MIN + 1)
#define CNXK_SSO_AFFINITY_CNT  (0x10)


#define CNXK_TT_FROM_TAG(x)	    (((x) >> 32) & SSO_TT_EMPTY)
#define CNXK_TT_FROM_EVENT(x)	    (((x) >> 38) & SSO_TT_EMPTY)
#define CNXK_EVENT_TYPE_FROM_TAG(x) (((x) >> 28) & 0xf)
#define CNXK_SUB_EVENT_FROM_TAG(x)  (((x) >> 20) & 0xff)
#define CNXK_CLR_SUB_EVENT(x)	    (~(0xffull << 20) & x)
#define CNXK_GRP_FROM_TAG(x)	    (((x) >> 36) & 0x3ff)
#define CNXK_SWTAG_PEND(x)	    (BIT_ULL(62) & x)
#define CNXK_TAG_IS_HEAD(x)	    (BIT_ULL(35) & x)

#endif /* __CNXK_EVENTDEV_DP_H__ */
