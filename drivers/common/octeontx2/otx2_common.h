/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_COMMON_H_
#define _OTX2_COMMON_H_

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_io.h>

#include "hw/otx2_rvu.h"
#include "hw/otx2_nix.h"
#include "hw/otx2_npc.h"
#include "hw/otx2_npa.h"
#include "hw/otx2_sdp.h"
#include "hw/otx2_sso.h"
#include "hw/otx2_ssow.h"
#include "hw/otx2_tim.h"
#include "hw/otx2_ree.h"

/* Alignment */
#define OTX2_ALIGN  128

/* Bits manipulation */
#ifndef BIT_ULL
#define BIT_ULL(nr) (1ULL << (nr))
#endif
#ifndef BIT
#define BIT(nr)     (1UL << (nr))
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#endif
#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG (__SIZEOF_LONG_LONG__ * 8)
#endif

#ifndef GENMASK
#define GENMASK(h, l) \
		(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif
#ifndef GENMASK_ULL
#define GENMASK_ULL(h, l) \
	(((~0ULL) - (1ULL << (l)) + 1) & \
	 (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))
#endif

#define OTX2_NPA_LOCK_MASK "npa_lock_mask"

/* Intra device related functions */
struct otx2_npa_lf;
struct otx2_idev_cfg {
	uint16_t sso_pf_func;
	uint16_t npa_pf_func;
	struct otx2_npa_lf *npa_lf;
	RTE_STD_C11
	union {
		rte_atomic16_t npa_refcnt;
		uint16_t npa_refcnt_u16;
	};
	uint64_t npa_lock_mask;
};

__rte_internal
struct otx2_idev_cfg *otx2_intra_dev_get_cfg(void);
__rte_internal
void otx2_sso_pf_func_set(uint16_t sso_pf_func);
__rte_internal
uint16_t otx2_sso_pf_func_get(void);
__rte_internal
uint16_t otx2_npa_pf_func_get(void);
__rte_internal
struct otx2_npa_lf *otx2_npa_lf_obj_get(void);
__rte_internal
void otx2_npa_set_defaults(struct otx2_idev_cfg *idev);
__rte_internal
int otx2_npa_lf_active(void *dev);
__rte_internal
int otx2_npa_lf_obj_ref(void);
__rte_internal
void otx2_parse_common_devargs(struct rte_kvargs *kvlist);

/* Log */
extern int otx2_logtype_base;
extern int otx2_logtype_mbox;
extern int otx2_logtype_npa;
extern int otx2_logtype_nix;
extern int otx2_logtype_sso;
extern int otx2_logtype_npc;
extern int otx2_logtype_tm;
extern int otx2_logtype_tim;
extern int otx2_logtype_dpi;
extern int otx2_logtype_ep;
extern int otx2_logtype_ree;

#define otx2_err(fmt, args...)			\
	RTE_LOG(ERR, PMD, "%s():%u " fmt "\n",	\
		__func__, __LINE__, ## args)

#define otx2_info(fmt, args...)						\
	RTE_LOG(INFO, PMD, fmt"\n", ## args)

#define otx2_dbg(subsystem, fmt, args...)				\
	rte_log(RTE_LOG_DEBUG, otx2_logtype_ ## subsystem,		\
		"[%s] %s():%u " fmt "\n",				\
		 #subsystem, __func__, __LINE__, ##args)

#define otx2_base_dbg(fmt, ...) otx2_dbg(base, fmt, ##__VA_ARGS__)
#define otx2_mbox_dbg(fmt, ...) otx2_dbg(mbox, fmt, ##__VA_ARGS__)
#define otx2_npa_dbg(fmt, ...) otx2_dbg(npa, fmt, ##__VA_ARGS__)
#define otx2_nix_dbg(fmt, ...) otx2_dbg(nix, fmt, ##__VA_ARGS__)
#define otx2_sso_dbg(fmt, ...) otx2_dbg(sso, fmt, ##__VA_ARGS__)
#define otx2_npc_dbg(fmt, ...) otx2_dbg(npc, fmt, ##__VA_ARGS__)
#define otx2_tm_dbg(fmt, ...) otx2_dbg(tm, fmt, ##__VA_ARGS__)
#define otx2_tim_dbg(fmt, ...) otx2_dbg(tim, fmt, ##__VA_ARGS__)
#define otx2_dpi_dbg(fmt, ...) otx2_dbg(dpi, fmt, ##__VA_ARGS__)
#define otx2_sdp_dbg(fmt, ...) otx2_dbg(ep, fmt, ##__VA_ARGS__)
#define otx2_ree_dbg(fmt, ...) otx2_dbg(ree, fmt, ##__VA_ARGS__)

/* PCI IDs */
#define PCI_VENDOR_ID_CAVIUM			0x177D
#define PCI_DEVID_OCTEONTX2_RVU_PF              0xA063
#define PCI_DEVID_OCTEONTX2_RVU_VF		0xA064
#define PCI_DEVID_OCTEONTX2_RVU_AF		0xA065
#define PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_PF	0xA0F9
#define PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_VF	0xA0FA
#define PCI_DEVID_OCTEONTX2_RVU_NPA_PF		0xA0FB
#define PCI_DEVID_OCTEONTX2_RVU_NPA_VF		0xA0FC
#define PCI_DEVID_OCTEONTX2_RVU_CPT_PF		0xA0FD
#define PCI_DEVID_OCTEONTX2_RVU_CPT_VF		0xA0FE
#define PCI_DEVID_OCTEONTX2_RVU_AF_VF		0xA0f8
#define PCI_DEVID_OCTEONTX2_DPI_VF		0xA081
#define PCI_DEVID_OCTEONTX2_EP_VF		0xB203 /* OCTEON TX2 EP mode */
#define PCI_DEVID_OCTEONTX2_RVU_SDP_PF		0xA0f6
#define PCI_DEVID_OCTEONTX2_RVU_SDP_VF		0xA0f7
#define PCI_DEVID_OCTEONTX2_RVU_REE_PF		0xA0f4
#define PCI_DEVID_OCTEONTX2_RVU_REE_VF		0xA0f5

/*
 * REVID for RVU PCIe devices.
 * Bits 0..1: minor pass
 * Bits 3..2: major pass
 * Bits 7..4: midr id, 0:96, 1:95, 2:loki, f:unknown
 */

#define RVU_PCI_REV_MIDR_ID(rev_id)		(rev_id >> 4)
#define RVU_PCI_REV_MAJOR(rev_id)		((rev_id >> 2) & 0x3)
#define RVU_PCI_REV_MINOR(rev_id)		(rev_id & 0x3)

#define RVU_PCI_CN96XX_MIDR_ID			0x0
#define RVU_PCI_CNF95XX_MIDR_ID			0x1

/* PCI Config offsets */
#define RVU_PCI_REVISION_ID			0x08

/* IO Access */
#define otx2_read64(addr) rte_read64_relaxed((void *)(addr))
#define otx2_write64(val, addr) rte_write64_relaxed((val), (void *)(addr))

#if defined(RTE_ARCH_ARM64)
#include "otx2_io_arm64.h"
#else
#include "otx2_io_generic.h"
#endif

/* Fastpath lookup */
#define OTX2_NIX_FASTPATH_LOOKUP_MEM	"otx2_nix_fastpath_lookup_mem"
#define OTX2_NIX_SA_TBL_START		(4096*4 + 69632*2)

#endif /* _OTX2_COMMON_H_ */
