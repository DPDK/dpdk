/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include "qman_priv.h"

/***************************/
/* Portal register assists */
/***************************/
#define QM_REG_EQCR_PI_CINH	0x3000
#define QM_REG_EQCR_CI_CINH	0x3040
#define QM_REG_EQCR_ITR		0x3080
#define QM_REG_DQRR_PI_CINH	0x3100
#define QM_REG_DQRR_CI_CINH	0x3140
#define QM_REG_DQRR_ITR		0x3180
#define QM_REG_DQRR_DCAP	0x31C0
#define QM_REG_DQRR_SDQCR	0x3200
#define QM_REG_DQRR_VDQCR	0x3240
#define QM_REG_DQRR_PDQCR	0x3280
#define QM_REG_MR_PI_CINH	0x3300
#define QM_REG_MR_CI_CINH	0x3340
#define QM_REG_MR_ITR		0x3380
#define QM_REG_CFG		0x3500
#define QM_REG_ISR		0x3600
#define QM_REG_IIR              0x36C0
#define QM_REG_ITPR		0x3740

/* Cache-enabled register offsets */
#define QM_CL_EQCR		0x0000
#define QM_CL_DQRR		0x1000
#define QM_CL_MR		0x2000
#define QM_CL_EQCR_PI_CENA	0x3000
#define QM_CL_EQCR_CI_CENA	0x3040
#define QM_CL_DQRR_PI_CENA	0x3100
#define QM_CL_DQRR_CI_CENA	0x3140
#define QM_CL_MR_PI_CENA	0x3300
#define QM_CL_MR_CI_CENA	0x3340
#define QM_CL_CR		0x3800
#define QM_CL_RR0		0x3900
#define QM_CL_RR1		0x3940

/* BTW, the drivers (and h/w programming model) already obtain the required
 * synchronisation for portal accesses via lwsync(), hwsync(), and
 * data-dependencies. Use of barrier()s or other order-preserving primitives
 * simply degrade performance. Hence the use of the __raw_*() interfaces, which
 * simply ensure that the compiler treats the portal registers as volatile (ie.
 * non-coherent).
 */

/* Cache-inhibited register access. */
#define __qm_in(qm, o)		be32_to_cpu(__raw_readl((qm)->ci  + (o)))
#define __qm_out(qm, o, val)	__raw_writel((cpu_to_be32(val)), \
					     (qm)->ci + (o))
#define qm_in(reg)		__qm_in(&portal->addr, QM_REG_##reg)
#define qm_out(reg, val)	__qm_out(&portal->addr, QM_REG_##reg, val)

/* Cache-enabled (index) register access */
#define __qm_cl_touch_ro(qm, o) dcbt_ro((qm)->ce + (o))
#define __qm_cl_touch_rw(qm, o) dcbt_rw((qm)->ce + (o))
#define __qm_cl_in(qm, o)	be32_to_cpu(__raw_readl((qm)->ce + (o)))
#define __qm_cl_out(qm, o, val) \
	do { \
		u32 *__tmpclout = (qm)->ce + (o); \
		__raw_writel(cpu_to_be32(val), __tmpclout); \
		dcbf(__tmpclout); \
	} while (0)
#define __qm_cl_invalidate(qm, o) dccivac((qm)->ce + (o))
#define qm_cl_touch_ro(reg) __qm_cl_touch_ro(&portal->addr, QM_CL_##reg##_CENA)
#define qm_cl_touch_rw(reg) __qm_cl_touch_rw(&portal->addr, QM_CL_##reg##_CENA)
#define qm_cl_in(reg)	    __qm_cl_in(&portal->addr, QM_CL_##reg##_CENA)
#define qm_cl_out(reg, val) __qm_cl_out(&portal->addr, QM_CL_##reg##_CENA, val)
#define qm_cl_invalidate(reg)\
	__qm_cl_invalidate(&portal->addr, QM_CL_##reg##_CENA)

/* Cache-enabled ring access */
#define qm_cl(base, idx)	((void *)base + ((idx) << 6))

/* Cyclic helper for rings. FIXME: once we are able to do fine-grain perf
 * analysis, look at using the "extra" bit in the ring index registers to avoid
 * cyclic issues.
 */
static inline u8 qm_cyc_diff(u8 ringsize, u8 first, u8 last)
{
	/* 'first' is included, 'last' is excluded */
	if (first <= last)
		return last - first;
	return ringsize + last - first;
}

/* Portal modes.
 *   Enum types;
 *     pmode == production mode
 *     cmode == consumption mode,
 *     dmode == h/w dequeue mode.
 *   Enum values use 3 letter codes. First letter matches the portal mode,
 *   remaining two letters indicate;
 *     ci == cache-inhibited portal register
 *     ce == cache-enabled portal register
 *     vb == in-band valid-bit (cache-enabled)
 *     dc == DCA (Discrete Consumption Acknowledgment), DQRR-only
 *   As for "enum qm_dqrr_dmode", it should be self-explanatory.
 */
enum qm_eqcr_pmode {		/* matches QCSP_CFG::EPM */
	qm_eqcr_pci = 0,	/* PI index, cache-inhibited */
	qm_eqcr_pce = 1,	/* PI index, cache-enabled */
	qm_eqcr_pvb = 2		/* valid-bit */
};

enum qm_dqrr_dmode {		/* matches QCSP_CFG::DP */
	qm_dqrr_dpush = 0,	/* SDQCR  + VDQCR */
	qm_dqrr_dpull = 1	/* PDQCR */
};

enum qm_dqrr_pmode {		/* s/w-only */
	qm_dqrr_pci,		/* reads DQRR_PI_CINH */
	qm_dqrr_pce,		/* reads DQRR_PI_CENA */
	qm_dqrr_pvb		/* reads valid-bit */
};

enum qm_dqrr_cmode {		/* matches QCSP_CFG::DCM */
	qm_dqrr_cci = 0,	/* CI index, cache-inhibited */
	qm_dqrr_cce = 1,	/* CI index, cache-enabled */
	qm_dqrr_cdc = 2		/* Discrete Consumption Acknowledgment */
};

enum qm_mr_pmode {		/* s/w-only */
	qm_mr_pci,		/* reads MR_PI_CINH */
	qm_mr_pce,		/* reads MR_PI_CENA */
	qm_mr_pvb		/* reads valid-bit */
};

enum qm_mr_cmode {		/* matches QCSP_CFG::MM */
	qm_mr_cci = 0,		/* CI index, cache-inhibited */
	qm_mr_cce = 1		/* CI index, cache-enabled */
};

/* ------------------------- */
/* --- Portal structures --- */

#define QM_EQCR_SIZE		8
#define QM_DQRR_SIZE		16
#define QM_MR_SIZE		8

struct qm_eqcr {
	struct qm_eqcr_entry *ring, *cursor;
	u8 ci, available, ithresh, vbit;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	u32 busy;
	enum qm_eqcr_pmode pmode;
#endif
};

struct qm_dqrr {
	struct qm_dqrr_entry *ring, *cursor;
	u8 pi, ci, fill, ithresh, vbit;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	enum qm_dqrr_dmode dmode;
	enum qm_dqrr_pmode pmode;
	enum qm_dqrr_cmode cmode;
#endif
};

struct qm_mr {
	const struct qm_mr_entry *ring, *cursor;
	u8 pi, ci, fill, ithresh, vbit;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	enum qm_mr_pmode pmode;
	enum qm_mr_cmode cmode;
#endif
};

struct qm_mc {
	struct qm_mc_command *cr;
	struct qm_mc_result *rr;
	u8 rridx, vbit;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	enum {
		/* Can be _mc_start()ed */
		qman_mc_idle,
		/* Can be _mc_commit()ed or _mc_abort()ed */
		qman_mc_user,
		/* Can only be _mc_retry()ed */
		qman_mc_hw
	} state;
#endif
};

#define QM_PORTAL_ALIGNMENT ____cacheline_aligned

struct qm_addr {
	void __iomem *ce;	/* cache-enabled */
	void __iomem *ci;	/* cache-inhibited */
};

struct qm_portal {
	struct qm_addr addr;
	struct qm_eqcr eqcr;
	struct qm_dqrr dqrr;
	struct qm_mr mr;
	struct qm_mc mc;
} QM_PORTAL_ALIGNMENT;

/* Bit-wise logic to wrap a ring pointer by clearing the "carry bit" */
#define EQCR_CARRYCLEAR(p) \
	(void *)((unsigned long)(p) & (~(unsigned long)(QM_EQCR_SIZE << 6)))

extern dma_addr_t rte_mem_virt2iova(const void *addr);

/* Bit-wise logic to convert a ring pointer to a ring index */
static inline u8 EQCR_PTR2IDX(struct qm_eqcr_entry *e)
{
	return ((uintptr_t)e >> 6) & (QM_EQCR_SIZE - 1);
}

/* Increment the 'cursor' ring pointer, taking 'vbit' into account */
static inline void EQCR_INC(struct qm_eqcr *eqcr)
{
	/* NB: this is odd-looking, but experiments show that it generates fast
	 * code with essentially no branching overheads. We increment to the
	 * next EQCR pointer and handle overflow and 'vbit'.
	 */
	struct qm_eqcr_entry *partial = eqcr->cursor + 1;

	eqcr->cursor = EQCR_CARRYCLEAR(partial);
	if (partial != eqcr->cursor)
		eqcr->vbit ^= QM_EQCR_VERB_VBIT;
}

static inline struct qm_eqcr_entry *qm_eqcr_start_no_stash(struct qm_portal
								 *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(!eqcr->busy);
#endif
	if (!eqcr->available)
		return NULL;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	eqcr->busy = 1;
#endif

	return eqcr->cursor;
}

static inline struct qm_eqcr_entry *qm_eqcr_start_stash(struct qm_portal
								*portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;
	u8 diff, old_ci;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(!eqcr->busy);
#endif
	if (!eqcr->available) {
		old_ci = eqcr->ci;
		eqcr->ci = qm_cl_in(EQCR_CI) & (QM_EQCR_SIZE - 1);
		diff = qm_cyc_diff(QM_EQCR_SIZE, old_ci, eqcr->ci);
		eqcr->available += diff;
		if (!diff)
			return NULL;
	}
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	eqcr->busy = 1;
#endif
	return eqcr->cursor;
}

static inline void qm_eqcr_abort(struct qm_portal *portal)
{
	__maybe_unused register struct qm_eqcr *eqcr = &portal->eqcr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(eqcr->busy);
	eqcr->busy = 0;
#endif
}

static inline struct qm_eqcr_entry *qm_eqcr_pend_and_next(
					struct qm_portal *portal, u8 myverb)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(eqcr->busy);
	DPAA_ASSERT(eqcr->pmode != qm_eqcr_pvb);
#endif
	if (eqcr->available == 1)
		return NULL;
	eqcr->cursor->__dont_write_directly__verb = myverb | eqcr->vbit;
	dcbf(eqcr->cursor);
	EQCR_INC(eqcr);
	eqcr->available--;
	return eqcr->cursor;
}

#define EQCR_COMMIT_CHECKS(eqcr) \
do { \
	DPAA_ASSERT(eqcr->busy); \
	DPAA_ASSERT(eqcr->cursor->orp == (eqcr->cursor->orp & 0x00ffffff)); \
	DPAA_ASSERT(eqcr->cursor->fqid == (eqcr->cursor->fqid & 0x00ffffff)); \
} while (0)

static inline void qm_eqcr_pci_commit(struct qm_portal *portal, u8 myverb)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	EQCR_COMMIT_CHECKS(eqcr);
	DPAA_ASSERT(eqcr->pmode == qm_eqcr_pci);
#endif
	eqcr->cursor->__dont_write_directly__verb = myverb | eqcr->vbit;
	EQCR_INC(eqcr);
	eqcr->available--;
	dcbf(eqcr->cursor);
	hwsync();
	qm_out(EQCR_PI_CINH, EQCR_PTR2IDX(eqcr->cursor));
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	eqcr->busy = 0;
#endif
}

static inline void qm_eqcr_pce_prefetch(struct qm_portal *portal)
{
	__maybe_unused register struct qm_eqcr *eqcr = &portal->eqcr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(eqcr->pmode == qm_eqcr_pce);
#endif
	qm_cl_invalidate(EQCR_PI);
	qm_cl_touch_rw(EQCR_PI);
}

static inline void qm_eqcr_pce_commit(struct qm_portal *portal, u8 myverb)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	EQCR_COMMIT_CHECKS(eqcr);
	DPAA_ASSERT(eqcr->pmode == qm_eqcr_pce);
#endif
	eqcr->cursor->__dont_write_directly__verb = myverb | eqcr->vbit;
	EQCR_INC(eqcr);
	eqcr->available--;
	dcbf(eqcr->cursor);
	lwsync();
	qm_cl_out(EQCR_PI, EQCR_PTR2IDX(eqcr->cursor));
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	eqcr->busy = 0;
#endif
}

static inline void qm_eqcr_pvb_commit(struct qm_portal *portal, u8 myverb)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;
	struct qm_eqcr_entry *eqcursor;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	EQCR_COMMIT_CHECKS(eqcr);
	DPAA_ASSERT(eqcr->pmode == qm_eqcr_pvb);
#endif
	lwsync();
	eqcursor = eqcr->cursor;
	eqcursor->__dont_write_directly__verb = myverb | eqcr->vbit;
	dcbf(eqcursor);
	EQCR_INC(eqcr);
	eqcr->available--;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	eqcr->busy = 0;
#endif
}

static inline u8 qm_eqcr_cci_update(struct qm_portal *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;
	u8 diff, old_ci = eqcr->ci;

	eqcr->ci = qm_in(EQCR_CI_CINH) & (QM_EQCR_SIZE - 1);
	diff = qm_cyc_diff(QM_EQCR_SIZE, old_ci, eqcr->ci);
	eqcr->available += diff;
	return diff;
}

static inline void qm_eqcr_cce_prefetch(struct qm_portal *portal)
{
	__maybe_unused register struct qm_eqcr *eqcr = &portal->eqcr;

	qm_cl_touch_ro(EQCR_CI);
}

static inline u8 qm_eqcr_cce_update(struct qm_portal *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;
	u8 diff, old_ci = eqcr->ci;

	eqcr->ci = qm_cl_in(EQCR_CI) & (QM_EQCR_SIZE - 1);
	qm_cl_invalidate(EQCR_CI);
	diff = qm_cyc_diff(QM_EQCR_SIZE, old_ci, eqcr->ci);
	eqcr->available += diff;
	return diff;
}

static inline u8 qm_eqcr_get_ithresh(struct qm_portal *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

	return eqcr->ithresh;
}

static inline void qm_eqcr_set_ithresh(struct qm_portal *portal, u8 ithresh)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

	eqcr->ithresh = ithresh;
	qm_out(EQCR_ITR, ithresh);
}

static inline u8 qm_eqcr_get_avail(struct qm_portal *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

	return eqcr->available;
}

static inline u8 qm_eqcr_get_fill(struct qm_portal *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;

	return QM_EQCR_SIZE - 1 - eqcr->available;
}

#define DQRR_CARRYCLEAR(p) \
	(void *)((unsigned long)(p) & (~(unsigned long)(QM_DQRR_SIZE << 6)))

static inline u8 DQRR_PTR2IDX(const struct qm_dqrr_entry *e)
{
	return ((uintptr_t)e >> 6) & (QM_DQRR_SIZE - 1);
}

static inline struct qm_dqrr_entry *DQRR_INC(
						const struct qm_dqrr_entry *e)
{
	return DQRR_CARRYCLEAR(e + 1);
}

static inline void qm_dqrr_set_maxfill(struct qm_portal *portal, u8 mf)
{
	qm_out(CFG, (qm_in(CFG) & 0xff0fffff) |
		((mf & (QM_DQRR_SIZE - 1)) << 20));
}

static inline const struct qm_dqrr_entry *qm_dqrr_current(
						struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

	if (!dqrr->fill)
		return NULL;
	return dqrr->cursor;
}

static inline u8 qm_dqrr_cursor(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

	return DQRR_PTR2IDX(dqrr->cursor);
}

static inline u8 qm_dqrr_next(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

	DPAA_ASSERT(dqrr->fill);
	dqrr->cursor = DQRR_INC(dqrr->cursor);
	return --dqrr->fill;
}

static inline u8 qm_dqrr_pci_update(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;
	u8 diff, old_pi = dqrr->pi;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->pmode == qm_dqrr_pci);
#endif
	dqrr->pi = qm_in(DQRR_PI_CINH) & (QM_DQRR_SIZE - 1);
	diff = qm_cyc_diff(QM_DQRR_SIZE, old_pi, dqrr->pi);
	dqrr->fill += diff;
	return diff;
}

static inline void qm_dqrr_pce_prefetch(struct qm_portal *portal)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->pmode == qm_dqrr_pce);
#endif
	qm_cl_invalidate(DQRR_PI);
	qm_cl_touch_ro(DQRR_PI);
}

static inline u8 qm_dqrr_pce_update(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;
	u8 diff, old_pi = dqrr->pi;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->pmode == qm_dqrr_pce);
#endif
	dqrr->pi = qm_cl_in(DQRR_PI) & (QM_DQRR_SIZE - 1);
	diff = qm_cyc_diff(QM_DQRR_SIZE, old_pi, dqrr->pi);
	dqrr->fill += diff;
	return diff;
}

static inline void qm_dqrr_pvb_update(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;
	const struct qm_dqrr_entry *res = qm_cl(dqrr->ring, dqrr->pi);

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->pmode == qm_dqrr_pvb);
#endif
	/* when accessing 'verb', use __raw_readb() to ensure that compiler
	 * inlining doesn't try to optimise out "excess reads".
	 */
	if ((__raw_readb(&res->verb) & QM_DQRR_VERB_VBIT) == dqrr->vbit) {
		dqrr->pi = (dqrr->pi + 1) & (QM_DQRR_SIZE - 1);
		if (!dqrr->pi)
			dqrr->vbit ^= QM_DQRR_VERB_VBIT;
		dqrr->fill++;
	}
}

static inline void qm_dqrr_cci_consume(struct qm_portal *portal, u8 num)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cci);
#endif
	dqrr->ci = (dqrr->ci + num) & (QM_DQRR_SIZE - 1);
	qm_out(DQRR_CI_CINH, dqrr->ci);
}

static inline void qm_dqrr_cci_consume_to_current(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cci);
#endif
	dqrr->ci = DQRR_PTR2IDX(dqrr->cursor);
	qm_out(DQRR_CI_CINH, dqrr->ci);
}

static inline void qm_dqrr_cce_prefetch(struct qm_portal *portal)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cce);
#endif
	qm_cl_invalidate(DQRR_CI);
	qm_cl_touch_rw(DQRR_CI);
}

static inline void qm_dqrr_cce_consume(struct qm_portal *portal, u8 num)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cce);
#endif
	dqrr->ci = (dqrr->ci + num) & (QM_DQRR_SIZE - 1);
	qm_cl_out(DQRR_CI, dqrr->ci);
}

static inline void qm_dqrr_cce_consume_to_current(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cce);
#endif
	dqrr->ci = DQRR_PTR2IDX(dqrr->cursor);
	qm_cl_out(DQRR_CI, dqrr->ci);
}

static inline void qm_dqrr_cdc_consume_1(struct qm_portal *portal, u8 idx,
					 int park)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cdc);
#endif
	DPAA_ASSERT(idx < QM_DQRR_SIZE);
	qm_out(DQRR_DCAP, (0 << 8) |	/* S */
		((park ? 1 : 0) << 6) |	/* PK */
		idx);			/* DCAP_CI */
}

static inline void qm_dqrr_cdc_consume_1ptr(struct qm_portal *portal,
					    const struct qm_dqrr_entry *dq,
					int park)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;
	u8 idx = DQRR_PTR2IDX(dq);

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cdc);
#endif
	DPAA_ASSERT(idx < QM_DQRR_SIZE);
	qm_out(DQRR_DCAP, (0 << 8) |		/* DQRR_DCAP::S */
		((park ? 1 : 0) << 6) |		/* DQRR_DCAP::PK */
		idx);				/* DQRR_DCAP::DCAP_CI */
}

static inline void qm_dqrr_cdc_consume_n(struct qm_portal *portal, u16 bitmask)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cdc);
#endif
	qm_out(DQRR_DCAP, (1 << 8) |		/* DQRR_DCAP::S */
		((u32)bitmask << 16));		/* DQRR_DCAP::DCAP_CI */
	dqrr->ci = qm_in(DQRR_CI_CINH) & (QM_DQRR_SIZE - 1);
	dqrr->fill = qm_cyc_diff(QM_DQRR_SIZE, dqrr->ci, dqrr->pi);
}

static inline u8 qm_dqrr_cdc_cci(struct qm_portal *portal)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cdc);
#endif
	return qm_in(DQRR_CI_CINH) & (QM_DQRR_SIZE - 1);
}

static inline void qm_dqrr_cdc_cce_prefetch(struct qm_portal *portal)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cdc);
#endif
	qm_cl_invalidate(DQRR_CI);
	qm_cl_touch_ro(DQRR_CI);
}

static inline u8 qm_dqrr_cdc_cce(struct qm_portal *portal)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode == qm_dqrr_cdc);
#endif
	return qm_cl_in(DQRR_CI) & (QM_DQRR_SIZE - 1);
}

static inline u8 qm_dqrr_get_ci(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode != qm_dqrr_cdc);
#endif
	return dqrr->ci;
}

static inline void qm_dqrr_park(struct qm_portal *portal, u8 idx)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode != qm_dqrr_cdc);
#endif
	qm_out(DQRR_DCAP, (0 << 8) |		/* S */
		(1 << 6) |			/* PK */
		(idx & (QM_DQRR_SIZE - 1)));	/* DCAP_CI */
}

static inline void qm_dqrr_park_current(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(dqrr->cmode != qm_dqrr_cdc);
#endif
	qm_out(DQRR_DCAP, (0 << 8) |		/* S */
		(1 << 6) |			/* PK */
		DQRR_PTR2IDX(dqrr->cursor));	/* DCAP_CI */
}

static inline void qm_dqrr_sdqcr_set(struct qm_portal *portal, u32 sdqcr)
{
	qm_out(DQRR_SDQCR, sdqcr);
}

static inline u32 qm_dqrr_sdqcr_get(struct qm_portal *portal)
{
	return qm_in(DQRR_SDQCR);
}

static inline void qm_dqrr_vdqcr_set(struct qm_portal *portal, u32 vdqcr)
{
	qm_out(DQRR_VDQCR, vdqcr);
}

static inline u32 qm_dqrr_vdqcr_get(struct qm_portal *portal)
{
	return qm_in(DQRR_VDQCR);
}

static inline u8 qm_dqrr_get_ithresh(struct qm_portal *portal)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;

	return dqrr->ithresh;
}

static inline void qm_dqrr_set_ithresh(struct qm_portal *portal, u8 ithresh)
{
	qm_out(DQRR_ITR, ithresh);
}

static inline u8 qm_dqrr_get_maxfill(struct qm_portal *portal)
{
	return (qm_in(CFG) & 0x00f00000) >> 20;
}

/* -------------- */
/* --- MR API --- */

#define MR_CARRYCLEAR(p) \
	(void *)((unsigned long)(p) & (~(unsigned long)(QM_MR_SIZE << 6)))

static inline u8 MR_PTR2IDX(const struct qm_mr_entry *e)
{
	return ((uintptr_t)e >> 6) & (QM_MR_SIZE - 1);
}

static inline const struct qm_mr_entry *MR_INC(const struct qm_mr_entry *e)
{
	return MR_CARRYCLEAR(e + 1);
}

static inline void qm_mr_finish(struct qm_portal *portal)
{
	register struct qm_mr *mr = &portal->mr;

	if (mr->ci != MR_PTR2IDX(mr->cursor))
		pr_crit("Ignoring completed MR entries\n");
}

static inline const struct qm_mr_entry *qm_mr_current(struct qm_portal *portal)
{
	register struct qm_mr *mr = &portal->mr;

	if (!mr->fill)
		return NULL;
	return mr->cursor;
}

static inline u8 qm_mr_next(struct qm_portal *portal)
{
	register struct qm_mr *mr = &portal->mr;

	DPAA_ASSERT(mr->fill);
	mr->cursor = MR_INC(mr->cursor);
	return --mr->fill;
}

static inline void qm_mr_cci_consume(struct qm_portal *portal, u8 num)
{
	register struct qm_mr *mr = &portal->mr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mr->cmode == qm_mr_cci);
#endif
	mr->ci = (mr->ci + num) & (QM_MR_SIZE - 1);
	qm_out(MR_CI_CINH, mr->ci);
}

static inline void qm_mr_cci_consume_to_current(struct qm_portal *portal)
{
	register struct qm_mr *mr = &portal->mr;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mr->cmode == qm_mr_cci);
#endif
	mr->ci = MR_PTR2IDX(mr->cursor);
	qm_out(MR_CI_CINH, mr->ci);
}

static inline void qm_mr_set_ithresh(struct qm_portal *portal, u8 ithresh)
{
	qm_out(MR_ITR, ithresh);
}

/* ------------------------------ */
/* --- Management command API --- */
static inline int qm_mc_init(struct qm_portal *portal)
{
	register struct qm_mc *mc = &portal->mc;

	mc->cr = portal->addr.ce + QM_CL_CR;
	mc->rr = portal->addr.ce + QM_CL_RR0;
	mc->rridx = (__raw_readb(&mc->cr->__dont_write_directly__verb) &
			QM_MCC_VERB_VBIT) ?  0 : 1;
	mc->vbit = mc->rridx ? QM_MCC_VERB_VBIT : 0;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	mc->state = qman_mc_idle;
#endif
	return 0;
}

static inline void qm_mc_finish(struct qm_portal *portal)
{
	__maybe_unused register struct qm_mc *mc = &portal->mc;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mc->state == qman_mc_idle);
	if (mc->state != qman_mc_idle)
		pr_crit("Losing incomplete MC command\n");
#endif
}

static inline struct qm_mc_command *qm_mc_start(struct qm_portal *portal)
{
	register struct qm_mc *mc = &portal->mc;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mc->state == qman_mc_idle);
	mc->state = qman_mc_user;
#endif
	dcbz_64(mc->cr);
	return mc->cr;
}

static inline void qm_mc_commit(struct qm_portal *portal, u8 myverb)
{
	register struct qm_mc *mc = &portal->mc;
	struct qm_mc_result *rr = mc->rr + mc->rridx;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mc->state == qman_mc_user);
#endif
	lwsync();
	mc->cr->__dont_write_directly__verb = myverb | mc->vbit;
	dcbf(mc->cr);
	dcbit_ro(rr);
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	mc->state = qman_mc_hw;
#endif
}

static inline struct qm_mc_result *qm_mc_result(struct qm_portal *portal)
{
	register struct qm_mc *mc = &portal->mc;
	struct qm_mc_result *rr = mc->rr + mc->rridx;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mc->state == qman_mc_hw);
#endif
	/* The inactive response register's verb byte always returns zero until
	 * its command is submitted and completed. This includes the valid-bit,
	 * in case you were wondering.
	 */
	if (!__raw_readb(&rr->verb)) {
		dcbit_ro(rr);
		return NULL;
	}
	mc->rridx ^= 1;
	mc->vbit ^= QM_MCC_VERB_VBIT;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	mc->state = qman_mc_idle;
#endif
	return rr;
}

/* Portal interrupt register API */
static inline void qm_isr_set_iperiod(struct qm_portal *portal, u16 iperiod)
{
	qm_out(ITPR, iperiod);
}

static inline u32 __qm_isr_read(struct qm_portal *portal, enum qm_isr_reg n)
{
#if defined(RTE_ARCH_ARM64)
	return __qm_in(&portal->addr, QM_REG_ISR + (n << 6));
#else
	return __qm_in(&portal->addr, QM_REG_ISR + (n << 2));
#endif
}

static inline void __qm_isr_write(struct qm_portal *portal, enum qm_isr_reg n,
				  u32 val)
{
#if defined(RTE_ARCH_ARM64)
	__qm_out(&portal->addr, QM_REG_ISR + (n << 6), val);
#else
	__qm_out(&portal->addr, QM_REG_ISR + (n << 2), val);
#endif
}
