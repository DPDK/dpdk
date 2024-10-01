/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN20K_RXTX_H__
#define __CN20K_RXTX_H__

#include <rte_security.h>

/* ROC Constants */
#include "roc_constants.h"

/* Platform definition */
#include "roc_platform.h"

/* IO */
#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

/* HW structure definition */
#include "hw/cpt.h"
#include "hw/nix.h"
#include "hw/npa.h"
#include "hw/npc.h"
#include "hw/ssow.h"

#include "roc_ie_ot.h"

/* NPA */
#include "roc_npa_dp.h"

/* SSO */
#include "roc_sso_dp.h"

/* CPT */
#include "roc_cpt.h"

/* NIX Inline dev */
#include "roc_nix_inl_dp.h"

#include "cnxk_ethdev_dp.h"

struct cn20k_eth_txq {
	uint64_t send_hdr_w0;
	int64_t fc_cache_pkts;
	uint64_t *fc_mem;
	uintptr_t lmt_base;
	rte_iova_t io_addr;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
	uint8_t flag;
	rte_iova_t cpt_io_addr;
	uint64_t sa_base;
	uint64_t *cpt_fc;
	uint16_t cpt_desc;
	int32_t *cpt_fc_sw;
	uint64_t lso_tun_fmt;
	uint64_t ts_mem;
	uint64_t mark_flag : 8;
	uint64_t mark_fmt : 48;
	struct cnxk_eth_txq_comp tx_compl;
} __plt_cache_aligned;

struct cn20k_eth_rxq {
	uint64_t mbuf_initializer;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t data_off;
	uint64_t sa_base;
	uint64_t lmt_base;
	uint64_t meta_aura;
	uintptr_t meta_pool;
	uint16_t rq;
	struct cnxk_timesync_info *tstamp;
} __plt_cache_aligned;

#define LMT_OFF(lmt_addr, lmt_num, offset)                                                         \
	(void *)((uintptr_t)(lmt_addr) + ((uint64_t)(lmt_num) << ROC_LMT_LINE_SIZE_LOG2) + (offset))

#endif /* __CN20K_RXTX_H__ */
