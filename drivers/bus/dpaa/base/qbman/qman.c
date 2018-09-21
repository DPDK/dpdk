/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include "qman.h"
#include <rte_branch_prediction.h>
#include <rte_dpaa_bus.h>
#include <rte_eventdev.h>
#include <rte_byteorder.h>

/* Compilation constants */
#define DQRR_MAXFILL	15
#define EQCR_ITHRESH	4	/* if EQCR congests, interrupt threshold */
#define IRQNAME		"QMan portal %d"
#define MAX_IRQNAME	16	/* big enough for "QMan portal %d" */
/* maximum number of DQRR entries to process in qman_poll() */
#define FSL_QMAN_POLL_LIMIT 8

/* Lock/unlock frame queues, subject to the "LOCKED" flag. This is about
 * inter-processor locking only. Note, FQLOCK() is always called either under a
 * local_irq_save() or from interrupt context - hence there's no need for irq
 * protection (and indeed, attempting to nest irq-protection doesn't work, as
 * the "irq en/disable" machinery isn't recursive...).
 */
#define FQLOCK(fq) \
	do { \
		struct qman_fq *__fq478 = (fq); \
		if (fq_isset(__fq478, QMAN_FQ_FLAG_LOCKED)) \
			spin_lock(&__fq478->fqlock); \
	} while (0)
#define FQUNLOCK(fq) \
	do { \
		struct qman_fq *__fq478 = (fq); \
		if (fq_isset(__fq478, QMAN_FQ_FLAG_LOCKED)) \
			spin_unlock(&__fq478->fqlock); \
	} while (0)

static inline void fq_set(struct qman_fq *fq, u32 mask)
{
	dpaa_set_bits(mask, &fq->flags);
}

static inline void fq_clear(struct qman_fq *fq, u32 mask)
{
	dpaa_clear_bits(mask, &fq->flags);
}

static inline int fq_isset(struct qman_fq *fq, u32 mask)
{
	return fq->flags & mask;
}

static inline int fq_isclear(struct qman_fq *fq, u32 mask)
{
	return !(fq->flags & mask);
}

struct qman_portal {
	struct qm_portal p;
	/* PORTAL_BITS_*** - dynamic, strictly internal */
	unsigned long bits;
	/* interrupt sources processed by portal_isr(), configurable */
	unsigned long irq_sources;
	u32 use_eqcr_ci_stashing;
	u32 slowpoll;	/* only used when interrupts are off */
	/* only 1 volatile dequeue at a time */
	struct qman_fq *vdqcr_owned;
	u32 sdqcr;
	int dqrr_disable_ref;
	/* A portal-specific handler for DCP ERNs. If this is NULL, the global
	 * handler is called instead.
	 */
	qman_cb_dc_ern cb_dc_ern;
	/* When the cpu-affine portal is activated, this is non-NULL */
	const struct qm_portal_config *config;
	struct dpa_rbtree retire_table;
	char irqname[MAX_IRQNAME];
	/* 2-element array. cgrs[0] is mask, cgrs[1] is snapshot. */
	struct qman_cgrs *cgrs;
	/* linked-list of CSCN handlers. */
	struct list_head cgr_cbs;
	/* list lock */
	spinlock_t cgr_lock;
	/* track if memory was allocated by the driver */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	/* Keep a shadow copy of the DQRR on LE systems as the SW needs to
	 * do byte swaps of DQRR read only memory.  First entry must be aligned
	 * to 2 ** 10 to ensure DQRR index calculations based shadow copy
	 * address (6 bits for address shift + 4 bits for the DQRR size).
	 */
	struct qm_dqrr_entry shadow_dqrr[QM_DQRR_SIZE]
		    __attribute__((aligned(1024)));
#endif
};

/* Global handler for DCP ERNs. Used when the portal receiving the message does
 * not have a portal-specific handler.
 */
static qman_cb_dc_ern cb_dc_ern;

static cpumask_t affine_mask;
static DEFINE_SPINLOCK(affine_mask_lock);
static u16 affine_channels[NR_CPUS];
static RTE_DEFINE_PER_LCORE(struct qman_portal, qman_affine_portal);

static inline struct qman_portal *get_affine_portal(void)
{
	return &RTE_PER_LCORE(qman_affine_portal);
}

/* This gives a FQID->FQ lookup to cover the fact that we can't directly demux
 * retirement notifications (the fact they are sometimes h/w-consumed means that
 * contextB isn't always a s/w demux - and as we can't know which case it is
 * when looking at the notification, we have to use the slow lookup for all of
 * them). NB, it's possible to have multiple FQ objects refer to the same FQID
 * (though at most one of them should be the consumer), so this table isn't for
 * all FQs - FQs are added when retirement commands are issued, and removed when
 * they complete, which also massively reduces the size of this table.
 */
IMPLEMENT_DPAA_RBTREE(fqtree, struct qman_fq, node, fqid);
/*
 * This is what everything can wait on, even if it migrates to a different cpu
 * to the one whose affine portal it is waiting on.
 */
static DECLARE_WAIT_QUEUE_HEAD(affine_queue);

static inline int table_push_fq(struct qman_portal *p, struct qman_fq *fq)
{
	int ret = fqtree_push(&p->retire_table, fq);

	if (ret)
		pr_err("ERROR: double FQ-retirement %d\n", fq->fqid);
	return ret;
}

static inline void table_del_fq(struct qman_portal *p, struct qman_fq *fq)
{
	fqtree_del(&p->retire_table, fq);
}

static inline struct qman_fq *table_find_fq(struct qman_portal *p, u32 fqid)
{
	return fqtree_find(&p->retire_table, fqid);
}

#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
static void **qman_fq_lookup_table;
static size_t qman_fq_lookup_table_size;

int qman_setup_fq_lookup_table(size_t num_entries)
{
	num_entries++;
	/* Allocate 1 more entry since the first entry is not used */
	qman_fq_lookup_table = vmalloc((num_entries * sizeof(void *)));
	if (!qman_fq_lookup_table) {
		pr_err("QMan: Could not allocate fq lookup table\n");
		return -ENOMEM;
	}
	memset(qman_fq_lookup_table, 0, num_entries * sizeof(void *));
	qman_fq_lookup_table_size = num_entries;
	pr_debug("QMan: Allocated lookup table at %p, entry count %lu\n",
		qman_fq_lookup_table,
			(unsigned long)qman_fq_lookup_table_size);
	return 0;
}

/* global structure that maintains fq object mapping */
static DEFINE_SPINLOCK(fq_hash_table_lock);

static int find_empty_fq_table_entry(u32 *entry, struct qman_fq *fq)
{
	u32 i;

	spin_lock(&fq_hash_table_lock);
	/* Can't use index zero because this has special meaning
	 * in context_b field.
	 */
	for (i = 1; i < qman_fq_lookup_table_size; i++) {
		if (qman_fq_lookup_table[i] == NULL) {
			*entry = i;
			qman_fq_lookup_table[i] = fq;
			spin_unlock(&fq_hash_table_lock);
			return 0;
		}
	}
	spin_unlock(&fq_hash_table_lock);
	return -ENOMEM;
}

static void clear_fq_table_entry(u32 entry)
{
	spin_lock(&fq_hash_table_lock);
	DPAA_BUG_ON(entry >= qman_fq_lookup_table_size);
	qman_fq_lookup_table[entry] = NULL;
	spin_unlock(&fq_hash_table_lock);
}

static inline struct qman_fq *get_fq_table_entry(u32 entry)
{
	DPAA_BUG_ON(entry >= qman_fq_lookup_table_size);
	return qman_fq_lookup_table[entry];
}
#endif

static inline void cpu_to_hw_fqd(struct qm_fqd *fqd)
{
	/* Byteswap the FQD to HW format */
	fqd->fq_ctrl = cpu_to_be16(fqd->fq_ctrl);
	fqd->dest_wq = cpu_to_be16(fqd->dest_wq);
	fqd->ics_cred = cpu_to_be16(fqd->ics_cred);
	fqd->context_b = cpu_to_be32(fqd->context_b);
	fqd->context_a.opaque = cpu_to_be64(fqd->context_a.opaque);
	fqd->opaque_td = cpu_to_be16(fqd->opaque_td);
}

static inline void hw_fqd_to_cpu(struct qm_fqd *fqd)
{
	/* Byteswap the FQD to CPU format */
	fqd->fq_ctrl = be16_to_cpu(fqd->fq_ctrl);
	fqd->dest_wq = be16_to_cpu(fqd->dest_wq);
	fqd->ics_cred = be16_to_cpu(fqd->ics_cred);
	fqd->context_b = be32_to_cpu(fqd->context_b);
	fqd->context_a.opaque = be64_to_cpu(fqd->context_a.opaque);
}

static inline void cpu_to_hw_fd(struct qm_fd *fd)
{
	fd->addr = cpu_to_be40(fd->addr);
	fd->status = cpu_to_be32(fd->status);
	fd->opaque = cpu_to_be32(fd->opaque);
}

static inline void hw_fd_to_cpu(struct qm_fd *fd)
{
	fd->addr = be40_to_cpu(fd->addr);
	fd->status = be32_to_cpu(fd->status);
	fd->opaque = be32_to_cpu(fd->opaque);
}

/* In the case that slow- and fast-path handling are both done by qman_poll()
 * (ie. because there is no interrupt handling), we ought to balance how often
 * we do the fast-path poll versus the slow-path poll. We'll use two decrementer
 * sources, so we call the fast poll 'n' times before calling the slow poll
 * once. The idle decrementer constant is used when the last slow-poll detected
 * no work to do, and the busy decrementer constant when the last slow-poll had
 * work to do.
 */
#define SLOW_POLL_IDLE   1000
#define SLOW_POLL_BUSY   10
static u32 __poll_portal_slow(struct qman_portal *p, u32 is);
static inline unsigned int __poll_portal_fast(struct qman_portal *p,
					      unsigned int poll_limit);

/* Portal interrupt handler */
static irqreturn_t portal_isr(__always_unused int irq, void *ptr)
{
	struct qman_portal *p = ptr;
	/*
	 * The CSCI/CCSCI source is cleared inside __poll_portal_slow(), because
	 * it could race against a Query Congestion State command also given
	 * as part of the handling of this interrupt source. We mustn't
	 * clear it a second time in this top-level function.
	 */
	u32 clear = QM_DQAVAIL_MASK | (p->irq_sources &
		~(QM_PIRQ_CSCI | QM_PIRQ_CCSCI));
	u32 is = qm_isr_status_read(&p->p) & p->irq_sources;
	/* DQRR-handling if it's interrupt-driven */
	if (is & QM_PIRQ_DQRI)
		__poll_portal_fast(p, FSL_QMAN_POLL_LIMIT);
	/* Handling of anything else that's interrupt-driven */
	clear |= __poll_portal_slow(p, is);
	qm_isr_status_clear(&p->p, clear);
	return IRQ_HANDLED;
}

/* This inner version is used privately by qman_create_affine_portal(), as well
 * as by the exported qman_stop_dequeues().
 */
static inline void qman_stop_dequeues_ex(struct qman_portal *p)
{
	if (!(p->dqrr_disable_ref++))
		qm_dqrr_set_maxfill(&p->p, 0);
}

static int drain_mr_fqrni(struct qm_portal *p)
{
	const struct qm_mr_entry *msg;
loop:
	msg = qm_mr_current(p);
	if (!msg) {
		/*
		 * if MR was full and h/w had other FQRNI entries to produce, we
		 * need to allow it time to produce those entries once the
		 * existing entries are consumed. A worst-case situation
		 * (fully-loaded system) means h/w sequencers may have to do 3-4
		 * other things before servicing the portal's MR pump, each of
		 * which (if slow) may take ~50 qman cycles (which is ~200
		 * processor cycles). So rounding up and then multiplying this
		 * worst-case estimate by a factor of 10, just to be
		 * ultra-paranoid, goes as high as 10,000 cycles. NB, we consume
		 * one entry at a time, so h/w has an opportunity to produce new
		 * entries well before the ring has been fully consumed, so
		 * we're being *really* paranoid here.
		 */
		u64 now, then = mfatb();

		do {
			now = mfatb();
		} while ((then + 10000) > now);
		msg = qm_mr_current(p);
		if (!msg)
			return 0;
	}
	if ((msg->ern.verb & QM_MR_VERB_TYPE_MASK) != QM_MR_VERB_FQRNI) {
		/* We aren't draining anything but FQRNIs */
		pr_err("Found verb 0x%x in MR\n", msg->ern.verb);
		return -1;
	}
	qm_mr_next(p);
	qm_mr_cci_consume(p, 1);
	goto loop;
}

static inline int qm_eqcr_init(struct qm_portal *portal,
			       enum qm_eqcr_pmode pmode,
			       unsigned int eq_stash_thresh,
			       int eq_stash_prio)
{
	/* This use of 'register', as well as all other occurrences, is because
	 * it has been observed to generate much faster code with gcc than is
	 * otherwise the case.
	 */
	register struct qm_eqcr *eqcr = &portal->eqcr;
	u32 cfg;
	u8 pi;

	eqcr->ring = portal->addr.ce + QM_CL_EQCR;
	eqcr->ci = qm_in(EQCR_CI_CINH) & (QM_EQCR_SIZE - 1);
	qm_cl_invalidate(EQCR_CI);
	pi = qm_in(EQCR_PI_CINH) & (QM_EQCR_SIZE - 1);
	eqcr->cursor = eqcr->ring + pi;
	eqcr->vbit = (qm_in(EQCR_PI_CINH) & QM_EQCR_SIZE) ?
			QM_EQCR_VERB_VBIT : 0;
	eqcr->available = QM_EQCR_SIZE - 1 -
			qm_cyc_diff(QM_EQCR_SIZE, eqcr->ci, pi);
	eqcr->ithresh = qm_in(EQCR_ITR);
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	eqcr->busy = 0;
	eqcr->pmode = pmode;
#endif
	cfg = (qm_in(CFG) & 0x00ffffff) |
		(eq_stash_thresh << 28) | /* QCSP_CFG: EST */
		(eq_stash_prio << 26)	| /* QCSP_CFG: EP */
		((pmode & 0x3) << 24);	/* QCSP_CFG::EPM */
	qm_out(CFG, cfg);
	return 0;
}

static inline void qm_eqcr_finish(struct qm_portal *portal)
{
	register struct qm_eqcr *eqcr = &portal->eqcr;
	u8 pi, ci;
	u32 cfg;

	/*
	 * Disable EQCI stashing because the QMan only
	 * presents the value it previously stashed to
	 * maintain coherency.  Setting the stash threshold
	 * to 1 then 0 ensures that QMan has resyncronized
	 * its internal copy so that the portal is clean
	 * when it is reinitialized in the future
	 */
	cfg = (qm_in(CFG) & 0x0fffffff) |
		(1 << 28); /* QCSP_CFG: EST */
	qm_out(CFG, cfg);
	cfg &= 0x0fffffff; /* stash threshold = 0 */
	qm_out(CFG, cfg);

	pi = qm_in(EQCR_PI_CINH) & (QM_EQCR_SIZE - 1);
	ci = qm_in(EQCR_CI_CINH) & (QM_EQCR_SIZE - 1);

	/* Refresh EQCR CI cache value */
	qm_cl_invalidate(EQCR_CI);
	eqcr->ci = qm_cl_in(EQCR_CI) & (QM_EQCR_SIZE - 1);

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(!eqcr->busy);
#endif
	if (pi != EQCR_PTR2IDX(eqcr->cursor))
		pr_crit("losing uncommitted EQCR entries\n");
	if (ci != eqcr->ci)
		pr_crit("missing existing EQCR completions\n");
	if (eqcr->ci != EQCR_PTR2IDX(eqcr->cursor))
		pr_crit("EQCR destroyed unquiesced\n");
}

static inline int qm_dqrr_init(struct qm_portal *portal,
			__maybe_unused const struct qm_portal_config *config,
			enum qm_dqrr_dmode dmode,
			__maybe_unused enum qm_dqrr_pmode pmode,
			enum qm_dqrr_cmode cmode, u8 max_fill)
{
	register struct qm_dqrr *dqrr = &portal->dqrr;
	u32 cfg;

	/* Make sure the DQRR will be idle when we enable */
	qm_out(DQRR_SDQCR, 0);
	qm_out(DQRR_VDQCR, 0);
	qm_out(DQRR_PDQCR, 0);
	dqrr->ring = portal->addr.ce + QM_CL_DQRR;
	dqrr->pi = qm_in(DQRR_PI_CINH) & (QM_DQRR_SIZE - 1);
	dqrr->ci = qm_in(DQRR_CI_CINH) & (QM_DQRR_SIZE - 1);
	dqrr->cursor = dqrr->ring + dqrr->ci;
	dqrr->fill = qm_cyc_diff(QM_DQRR_SIZE, dqrr->ci, dqrr->pi);
	dqrr->vbit = (qm_in(DQRR_PI_CINH) & QM_DQRR_SIZE) ?
			QM_DQRR_VERB_VBIT : 0;
	dqrr->ithresh = qm_in(DQRR_ITR);
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	dqrr->dmode = dmode;
	dqrr->pmode = pmode;
	dqrr->cmode = cmode;
#endif
	/* Invalidate every ring entry before beginning */
	for (cfg = 0; cfg < QM_DQRR_SIZE; cfg++)
		dccivac(qm_cl(dqrr->ring, cfg));
	cfg = (qm_in(CFG) & 0xff000f00) |
		((max_fill & (QM_DQRR_SIZE - 1)) << 20) | /* DQRR_MF */
		((dmode & 1) << 18) |			/* DP */
		((cmode & 3) << 16) |			/* DCM */
		0xa0 |					/* RE+SE */
		(0 ? 0x40 : 0) |			/* Ignore RP */
		(0 ? 0x10 : 0);				/* Ignore SP */
	qm_out(CFG, cfg);
	qm_dqrr_set_maxfill(portal, max_fill);
	return 0;
}

static inline void qm_dqrr_finish(struct qm_portal *portal)
{
	__maybe_unused register struct qm_dqrr *dqrr = &portal->dqrr;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if ((dqrr->cmode != qm_dqrr_cdc) &&
	    (dqrr->ci != DQRR_PTR2IDX(dqrr->cursor)))
		pr_crit("Ignoring completed DQRR entries\n");
#endif
}

static inline int qm_mr_init(struct qm_portal *portal,
			     __maybe_unused enum qm_mr_pmode pmode,
			     enum qm_mr_cmode cmode)
{
	register struct qm_mr *mr = &portal->mr;
	u32 cfg;

	mr->ring = portal->addr.ce + QM_CL_MR;
	mr->pi = qm_in(MR_PI_CINH) & (QM_MR_SIZE - 1);
	mr->ci = qm_in(MR_CI_CINH) & (QM_MR_SIZE - 1);
	mr->cursor = mr->ring + mr->ci;
	mr->fill = qm_cyc_diff(QM_MR_SIZE, mr->ci, mr->pi);
	mr->vbit = (qm_in(MR_PI_CINH) & QM_MR_SIZE) ? QM_MR_VERB_VBIT : 0;
	mr->ithresh = qm_in(MR_ITR);
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	mr->pmode = pmode;
	mr->cmode = cmode;
#endif
	cfg = (qm_in(CFG) & 0xfffff0ff) |
		((cmode & 1) << 8);		/* QCSP_CFG:MM */
	qm_out(CFG, cfg);
	return 0;
}

static inline void qm_mr_pvb_update(struct qm_portal *portal)
{
	register struct qm_mr *mr = &portal->mr;
	const struct qm_mr_entry *res = qm_cl(mr->ring, mr->pi);

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	DPAA_ASSERT(mr->pmode == qm_mr_pvb);
#endif
	/* when accessing 'verb', use __raw_readb() to ensure that compiler
	 * inlining doesn't try to optimise out "excess reads".
	 */
	if ((__raw_readb(&res->ern.verb) & QM_MR_VERB_VBIT) == mr->vbit) {
		mr->pi = (mr->pi + 1) & (QM_MR_SIZE - 1);
		if (!mr->pi)
			mr->vbit ^= QM_MR_VERB_VBIT;
		mr->fill++;
		res = MR_INC(res);
	}
	dcbit_ro(res);
}

static inline
struct qman_portal *qman_create_portal(
			struct qman_portal *portal,
			      const struct qm_portal_config *c,
			      const struct qman_cgrs *cgrs)
{
	struct qm_portal *p;
	char buf[16];
	int ret;
	u32 isdr;

	p = &portal->p;

	if (dpaa_svr_family == SVR_LS1043A_FAMILY)
		portal->use_eqcr_ci_stashing = 3;
	else
		portal->use_eqcr_ci_stashing =
					((qman_ip_rev >= QMAN_REV30) ? 1 : 0);

	/*
	 * prep the low-level portal struct with the mapped addresses from the
	 * config, everything that follows depends on it and "config" is more
	 * for (de)reference
	 */
	p->addr.ce = c->addr_virt[DPAA_PORTAL_CE];
	p->addr.ci = c->addr_virt[DPAA_PORTAL_CI];
	/*
	 * If CI-stashing is used, the current defaults use a threshold of 3,
	 * and stash with high-than-DQRR priority.
	 */
	if (qm_eqcr_init(p, qm_eqcr_pvb,
			 portal->use_eqcr_ci_stashing, 1)) {
		pr_err("Qman EQCR initialisation failed\n");
		goto fail_eqcr;
	}
	if (qm_dqrr_init(p, c, qm_dqrr_dpush, qm_dqrr_pvb,
			 qm_dqrr_cdc, DQRR_MAXFILL)) {
		pr_err("Qman DQRR initialisation failed\n");
		goto fail_dqrr;
	}
	if (qm_mr_init(p, qm_mr_pvb, qm_mr_cci)) {
		pr_err("Qman MR initialisation failed\n");
		goto fail_mr;
	}
	if (qm_mc_init(p)) {
		pr_err("Qman MC initialisation failed\n");
		goto fail_mc;
	}

	/* static interrupt-gating controls */
	qm_dqrr_set_ithresh(p, 0);
	qm_mr_set_ithresh(p, 0);
	qm_isr_set_iperiod(p, 0);
	portal->cgrs = kmalloc(2 * sizeof(*cgrs), GFP_KERNEL);
	if (!portal->cgrs)
		goto fail_cgrs;
	/* initial snapshot is no-depletion */
	qman_cgrs_init(&portal->cgrs[1]);
	if (cgrs)
		portal->cgrs[0] = *cgrs;
	else
		/* if the given mask is NULL, assume all CGRs can be seen */
		qman_cgrs_fill(&portal->cgrs[0]);
	INIT_LIST_HEAD(&portal->cgr_cbs);
	spin_lock_init(&portal->cgr_lock);
	portal->bits = 0;
	portal->slowpoll = 0;
	portal->sdqcr = QM_SDQCR_SOURCE_CHANNELS | QM_SDQCR_COUNT_UPTO3 |
			QM_SDQCR_DEDICATED_PRECEDENCE | QM_SDQCR_TYPE_PRIO_QOS |
			QM_SDQCR_TOKEN_SET(0xab) | QM_SDQCR_CHANNELS_DEDICATED;
	portal->dqrr_disable_ref = 0;
	portal->cb_dc_ern = NULL;
	sprintf(buf, "qportal-%d", c->channel);
	dpa_rbtree_init(&portal->retire_table);
	isdr = 0xffffffff;
	qm_isr_disable_write(p, isdr);
	portal->irq_sources = 0;
	qm_isr_enable_write(p, portal->irq_sources);
	qm_isr_status_clear(p, 0xffffffff);
	snprintf(portal->irqname, MAX_IRQNAME, IRQNAME, c->cpu);
	if (request_irq(c->irq, portal_isr, 0, portal->irqname,
			portal)) {
		pr_err("request_irq() failed\n");
		goto fail_irq;
	}

	/* Need EQCR to be empty before continuing */
	isdr &= ~QM_PIRQ_EQCI;
	qm_isr_disable_write(p, isdr);
	ret = qm_eqcr_get_fill(p);
	if (ret) {
		pr_err("Qman EQCR unclean\n");
		goto fail_eqcr_empty;
	}
	isdr &= ~(QM_PIRQ_DQRI | QM_PIRQ_MRI);
	qm_isr_disable_write(p, isdr);
	if (qm_dqrr_current(p)) {
		pr_err("Qman DQRR unclean\n");
		qm_dqrr_cdc_consume_n(p, 0xffff);
	}
	if (qm_mr_current(p) && drain_mr_fqrni(p)) {
		/* special handling, drain just in case it's a few FQRNIs */
		if (drain_mr_fqrni(p))
			goto fail_dqrr_mr_empty;
	}
	/* Success */
	portal->config = c;
	qm_isr_disable_write(p, 0);
	qm_isr_uninhibit(p);
	/* Write a sane SDQCR */
	qm_dqrr_sdqcr_set(p, portal->sdqcr);
	return portal;
fail_dqrr_mr_empty:
fail_eqcr_empty:
	free_irq(c->irq, portal);
fail_irq:
	kfree(portal->cgrs);
	spin_lock_destroy(&portal->cgr_lock);
fail_cgrs:
	qm_mc_finish(p);
fail_mc:
	qm_mr_finish(p);
fail_mr:
	qm_dqrr_finish(p);
fail_dqrr:
	qm_eqcr_finish(p);
fail_eqcr:
	return NULL;
}

#define MAX_GLOBAL_PORTALS 8
static struct qman_portal global_portals[MAX_GLOBAL_PORTALS];
static rte_atomic16_t global_portals_used[MAX_GLOBAL_PORTALS];

static struct qman_portal *
qman_alloc_global_portal(void)
{
	unsigned int i;

	for (i = 0; i < MAX_GLOBAL_PORTALS; i++) {
		if (rte_atomic16_test_and_set(&global_portals_used[i]))
			return &global_portals[i];
	}
	pr_err("No portal available (%x)\n", MAX_GLOBAL_PORTALS);

	return NULL;
}

static int
qman_free_global_portal(struct qman_portal *portal)
{
	unsigned int i;

	for (i = 0; i < MAX_GLOBAL_PORTALS; i++) {
		if (&global_portals[i] == portal) {
			rte_atomic16_clear(&global_portals_used[i]);
			return 0;
		}
	}
	return -1;
}

struct qman_portal *qman_create_affine_portal(const struct qm_portal_config *c,
					      const struct qman_cgrs *cgrs,
					      int alloc)
{
	struct qman_portal *res;
	struct qman_portal *portal;

	if (alloc)
		portal = qman_alloc_global_portal();
	else
		portal = get_affine_portal();

	/* A criteria for calling this function (from qman_driver.c) is that
	 * we're already affine to the cpu and won't schedule onto another cpu.
	 */

	res = qman_create_portal(portal, c, cgrs);
	if (res) {
		spin_lock(&affine_mask_lock);
		CPU_SET(c->cpu, &affine_mask);
		affine_channels[c->cpu] =
			c->channel;
		spin_unlock(&affine_mask_lock);
	}
	return res;
}

static inline
void qman_destroy_portal(struct qman_portal *qm)
{
	const struct qm_portal_config *pcfg;

	/* Stop dequeues on the portal */
	qm_dqrr_sdqcr_set(&qm->p, 0);

	/*
	 * NB we do this to "quiesce" EQCR. If we add enqueue-completions or
	 * something related to QM_PIRQ_EQCI, this may need fixing.
	 * Also, due to the prefetching model used for CI updates in the enqueue
	 * path, this update will only invalidate the CI cacheline *after*
	 * working on it, so we need to call this twice to ensure a full update
	 * irrespective of where the enqueue processing was at when the teardown
	 * began.
	 */
	qm_eqcr_cce_update(&qm->p);
	qm_eqcr_cce_update(&qm->p);
	pcfg = qm->config;

	free_irq(pcfg->irq, qm);

	kfree(qm->cgrs);
	qm_mc_finish(&qm->p);
	qm_mr_finish(&qm->p);
	qm_dqrr_finish(&qm->p);
	qm_eqcr_finish(&qm->p);

	qm->config = NULL;

	spin_lock_destroy(&qm->cgr_lock);
}

const struct qm_portal_config *
qman_destroy_affine_portal(struct qman_portal *qp)
{
	/* We don't want to redirect if we're a slave, use "raw" */
	struct qman_portal *qm;
	const struct qm_portal_config *pcfg;
	int cpu;

	if (qp == NULL)
		qm = get_affine_portal();
	else
		qm = qp;
	pcfg = qm->config;
	cpu = pcfg->cpu;

	qman_destroy_portal(qm);

	spin_lock(&affine_mask_lock);
	CPU_CLR(cpu, &affine_mask);
	spin_unlock(&affine_mask_lock);

	qman_free_global_portal(qm);

	return pcfg;
}

int qman_get_portal_index(void)
{
	struct qman_portal *p = get_affine_portal();
	return p->config->index;
}

/* Inline helper to reduce nesting in __poll_portal_slow() */
static inline void fq_state_change(struct qman_portal *p, struct qman_fq *fq,
				   const struct qm_mr_entry *msg, u8 verb)
{
	FQLOCK(fq);
	switch (verb) {
	case QM_MR_VERB_FQRL:
		DPAA_ASSERT(fq_isset(fq, QMAN_FQ_STATE_ORL));
		fq_clear(fq, QMAN_FQ_STATE_ORL);
		table_del_fq(p, fq);
		break;
	case QM_MR_VERB_FQRN:
		DPAA_ASSERT((fq->state == qman_fq_state_parked) ||
			    (fq->state == qman_fq_state_sched));
		DPAA_ASSERT(fq_isset(fq, QMAN_FQ_STATE_CHANGING));
		fq_clear(fq, QMAN_FQ_STATE_CHANGING);
		if (msg->fq.fqs & QM_MR_FQS_NOTEMPTY)
			fq_set(fq, QMAN_FQ_STATE_NE);
		if (msg->fq.fqs & QM_MR_FQS_ORLPRESENT)
			fq_set(fq, QMAN_FQ_STATE_ORL);
		else
			table_del_fq(p, fq);
		fq->state = qman_fq_state_retired;
		break;
	case QM_MR_VERB_FQPN:
		DPAA_ASSERT(fq->state == qman_fq_state_sched);
		DPAA_ASSERT(fq_isclear(fq, QMAN_FQ_STATE_CHANGING));
		fq->state = qman_fq_state_parked;
	}
	FQUNLOCK(fq);
}

static u32 __poll_portal_slow(struct qman_portal *p, u32 is)
{
	const struct qm_mr_entry *msg;
	struct qm_mr_entry swapped_msg;

	if (is & QM_PIRQ_CSCI) {
		struct qman_cgrs rr, c;
		struct qm_mc_result *mcr;
		struct qman_cgr *cgr;

		spin_lock(&p->cgr_lock);
		/*
		 * The CSCI bit must be cleared _before_ issuing the
		 * Query Congestion State command, to ensure that a long
		 * CGR State Change callback cannot miss an intervening
		 * state change.
		 */
		qm_isr_status_clear(&p->p, QM_PIRQ_CSCI);
		qm_mc_start(&p->p);
		qm_mc_commit(&p->p, QM_MCC_VERB_QUERYCONGESTION);
		while (!(mcr = qm_mc_result(&p->p)))
			cpu_relax();
		/* mask out the ones I'm not interested in */
		qman_cgrs_and(&rr, (const struct qman_cgrs *)
			&mcr->querycongestion.state, &p->cgrs[0]);
		/* check previous snapshot for delta, enter/exit congestion */
		qman_cgrs_xor(&c, &rr, &p->cgrs[1]);
		/* update snapshot */
		qman_cgrs_cp(&p->cgrs[1], &rr);
		/* Invoke callback */
		list_for_each_entry(cgr, &p->cgr_cbs, node)
			if (cgr->cb && qman_cgrs_get(&c, cgr->cgrid))
				cgr->cb(p, cgr, qman_cgrs_get(&rr, cgr->cgrid));
		spin_unlock(&p->cgr_lock);
	}

	if (is & QM_PIRQ_EQRI) {
		qm_eqcr_cce_update(&p->p);
		qm_eqcr_set_ithresh(&p->p, 0);
		wake_up(&affine_queue);
	}

	if (is & QM_PIRQ_MRI) {
		struct qman_fq *fq;
		u8 verb, num = 0;
mr_loop:
		qm_mr_pvb_update(&p->p);
		msg = qm_mr_current(&p->p);
		if (!msg)
			goto mr_done;
		swapped_msg = *msg;
		hw_fd_to_cpu(&swapped_msg.ern.fd);
		verb = msg->ern.verb & QM_MR_VERB_TYPE_MASK;
		/* The message is a software ERN iff the 0x20 bit is set */
		if (verb & 0x20) {
			switch (verb) {
			case QM_MR_VERB_FQRNI:
				/* nada, we drop FQRNIs on the floor */
				break;
			case QM_MR_VERB_FQRN:
			case QM_MR_VERB_FQRL:
				/* Lookup in the retirement table */
				fq = table_find_fq(p,
						   be32_to_cpu(msg->fq.fqid));
				DPAA_BUG_ON(!fq);
				fq_state_change(p, fq, &swapped_msg, verb);
				if (fq->cb.fqs)
					fq->cb.fqs(p, fq, &swapped_msg);
				break;
			case QM_MR_VERB_FQPN:
				/* Parked */
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
				fq = get_fq_table_entry(msg->fq.contextB);
#else
				fq = (void *)(uintptr_t)msg->fq.contextB;
#endif
				fq_state_change(p, fq, msg, verb);
				if (fq->cb.fqs)
					fq->cb.fqs(p, fq, &swapped_msg);
				break;
			case QM_MR_VERB_DC_ERN:
				/* DCP ERN */
				if (p->cb_dc_ern)
					p->cb_dc_ern(p, msg);
				else if (cb_dc_ern)
					cb_dc_ern(p, msg);
				else {
					static int warn_once;

					if (!warn_once) {
						pr_crit("Leaking DCP ERNs!\n");
						warn_once = 1;
					}
				}
				break;
			default:
				pr_crit("Invalid MR verb 0x%02x\n", verb);
			}
		} else {
			/* Its a software ERN */
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
			fq = get_fq_table_entry(be32_to_cpu(msg->ern.tag));
#else
			fq = (void *)(uintptr_t)be32_to_cpu(msg->ern.tag);
#endif
			fq->cb.ern(p, fq, &swapped_msg);
		}
		num++;
		qm_mr_next(&p->p);
		goto mr_loop;
mr_done:
		qm_mr_cci_consume(&p->p, num);
	}
	/*
	 * QM_PIRQ_CSCI/CCSCI has already been cleared, as part of its specific
	 * processing. If that interrupt source has meanwhile been re-asserted,
	 * we mustn't clear it here (or in the top-level interrupt handler).
	 */
	return is & (QM_PIRQ_EQCI | QM_PIRQ_EQRI | QM_PIRQ_MRI);
}

/*
 * remove some slowish-path stuff from the "fast path" and make sure it isn't
 * inlined.
 */
static noinline void clear_vdqcr(struct qman_portal *p, struct qman_fq *fq)
{
	p->vdqcr_owned = NULL;
	FQLOCK(fq);
	fq_clear(fq, QMAN_FQ_STATE_VDQCR);
	FQUNLOCK(fq);
	wake_up(&affine_queue);
}

/*
 * The only states that would conflict with other things if they ran at the
 * same time on the same cpu are:
 *
 *   (i) setting/clearing vdqcr_owned, and
 *  (ii) clearing the NE (Not Empty) flag.
 *
 * Both are safe. Because;
 *
 *   (i) this clearing can only occur after qman_set_vdq() has set the
 *	 vdqcr_owned field (which it does before setting VDQCR), and
 *	 qman_volatile_dequeue() blocks interrupts and preemption while this is
 *	 done so that we can't interfere.
 *  (ii) the NE flag is only cleared after qman_retire_fq() has set it, and as
 *	 with (i) that API prevents us from interfering until it's safe.
 *
 * The good thing is that qman_set_vdq() and qman_retire_fq() run far
 * less frequently (ie. per-FQ) than __poll_portal_fast() does, so the nett
 * advantage comes from this function not having to "lock" anything at all.
 *
 * Note also that the callbacks are invoked at points which are safe against the
 * above potential conflicts, but that this function itself is not re-entrant
 * (this is because the function tracks one end of each FIFO in the portal and
 * we do *not* want to lock that). So the consequence is that it is safe for
 * user callbacks to call into any QMan API.
 */
static inline unsigned int __poll_portal_fast(struct qman_portal *p,
					      unsigned int poll_limit)
{
	const struct qm_dqrr_entry *dq;
	struct qman_fq *fq;
	enum qman_cb_dqrr_result res;
	unsigned int limit = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	struct qm_dqrr_entry *shadow;
#endif
	do {
		qm_dqrr_pvb_update(&p->p);
		dq = qm_dqrr_current(&p->p);
		if (unlikely(!dq))
			break;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	/* If running on an LE system the fields of the
	 * dequeue entry must be swapper.  Because the
	 * QMan HW will ignore writes the DQRR entry is
	 * copied and the index stored within the copy
	 */
		shadow = &p->shadow_dqrr[DQRR_PTR2IDX(dq)];
		*shadow = *dq;
		dq = shadow;
		shadow->fqid = be32_to_cpu(shadow->fqid);
		shadow->seqnum = be16_to_cpu(shadow->seqnum);
		hw_fd_to_cpu(&shadow->fd);
#endif

		if (dq->stat & QM_DQRR_STAT_UNSCHEDULED) {
			/*
			 * VDQCR: don't trust context_b as the FQ may have
			 * been configured for h/w consumption and we're
			 * draining it post-retirement.
			 */
			fq = p->vdqcr_owned;
			/*
			 * We only set QMAN_FQ_STATE_NE when retiring, so we
			 * only need to check for clearing it when doing
			 * volatile dequeues.  It's one less thing to check
			 * in the critical path (SDQCR).
			 */
			if (dq->stat & QM_DQRR_STAT_FQ_EMPTY)
				fq_clear(fq, QMAN_FQ_STATE_NE);
			/*
			 * This is duplicated from the SDQCR code, but we
			 * have stuff to do before *and* after this callback,
			 * and we don't want multiple if()s in the critical
			 * path (SDQCR).
			 */
			res = fq->cb.dqrr(p, fq, dq);
			if (res == qman_cb_dqrr_stop)
				break;
			/* Check for VDQCR completion */
			if (dq->stat & QM_DQRR_STAT_DQCR_EXPIRED)
				clear_vdqcr(p, fq);
		} else {
			/* SDQCR: context_b points to the FQ */
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
			fq = get_fq_table_entry(dq->contextB);
#else
			fq = (void *)(uintptr_t)dq->contextB;
#endif
			/* Now let the callback do its stuff */
			res = fq->cb.dqrr(p, fq, dq);
			/*
			 * The callback can request that we exit without
			 * consuming this entry nor advancing;
			 */
			if (res == qman_cb_dqrr_stop)
				break;
		}
		/* Interpret 'dq' from a driver perspective. */
		/*
		 * Parking isn't possible unless HELDACTIVE was set. NB,
		 * FORCEELIGIBLE implies HELDACTIVE, so we only need to
		 * check for HELDACTIVE to cover both.
		 */
		DPAA_ASSERT((dq->stat & QM_DQRR_STAT_FQ_HELDACTIVE) ||
			    (res != qman_cb_dqrr_park));
		/* just means "skip it, I'll consume it myself later on" */
		if (res != qman_cb_dqrr_defer)
			qm_dqrr_cdc_consume_1ptr(&p->p, dq,
						 res == qman_cb_dqrr_park);
		/* Move forward */
		qm_dqrr_next(&p->p);
		/*
		 * Entry processed and consumed, increment our counter.  The
		 * callback can request that we exit after consuming the
		 * entry, and we also exit if we reach our processing limit,
		 * so loop back only if neither of these conditions is met.
		 */
	} while (++limit < poll_limit && res != qman_cb_dqrr_consume_stop);

	return limit;
}

int qman_irqsource_add(u32 bits)
{
	struct qman_portal *p = get_affine_portal();

	bits = bits & QM_PIRQ_VISIBLE;

	/* Clear any previously remaining interrupt conditions in
	 * QCSP_ISR. This prevents raising a false interrupt when
	 * interrupt conditions are enabled in QCSP_IER.
	 */
	qm_isr_status_clear(&p->p, bits);
	dpaa_set_bits(bits, &p->irq_sources);
	qm_isr_enable_write(&p->p, p->irq_sources);


	return 0;
}

int qman_irqsource_remove(u32 bits)
{
	struct qman_portal *p = get_affine_portal();
	u32 ier;

	/* Our interrupt handler only processes+clears status register bits that
	 * are in p->irq_sources. As we're trimming that mask, if one of them
	 * were to assert in the status register just before we remove it from
	 * the enable register, there would be an interrupt-storm when we
	 * release the IRQ lock. So we wait for the enable register update to
	 * take effect in h/w (by reading it back) and then clear all other bits
	 * in the status register. Ie. we clear them from ISR once it's certain
	 * IER won't allow them to reassert.
	 */

	bits &= QM_PIRQ_VISIBLE;
	dpaa_clear_bits(bits, &p->irq_sources);
	qm_isr_enable_write(&p->p, p->irq_sources);
	ier = qm_isr_enable_read(&p->p);
	/* Using "~ier" (rather than "bits" or "~p->irq_sources") creates a
	 * data-dependency, ie. to protect against re-ordering.
	 */
	qm_isr_status_clear(&p->p, ~ier);
	return 0;
}

u16 qman_affine_channel(int cpu)
{
	if (cpu < 0) {
		struct qman_portal *portal = get_affine_portal();

		cpu = portal->config->cpu;
	}
	DPAA_BUG_ON(!CPU_ISSET(cpu, &affine_mask));
	return affine_channels[cpu];
}

unsigned int qman_portal_poll_rx(unsigned int poll_limit,
				 void **bufs,
				 struct qman_portal *p)
{
	struct qm_portal *portal = &p->p;
	register struct qm_dqrr *dqrr = &portal->dqrr;
	struct qm_dqrr_entry *dq[QM_DQRR_SIZE], *shadow[QM_DQRR_SIZE];
	struct qman_fq *fq;
	unsigned int limit = 0, rx_number = 0;
	uint32_t consume = 0;

	do {
		qm_dqrr_pvb_update(&p->p);
		if (!dqrr->fill)
			break;

		dq[rx_number] = dqrr->cursor;
		dqrr->cursor = DQRR_CARRYCLEAR(dqrr->cursor + 1);
		/* Prefetch the next DQRR entry */
		rte_prefetch0(dqrr->cursor);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		/* If running on an LE system the fields of the
		 * dequeue entry must be swapper.  Because the
		 * QMan HW will ignore writes the DQRR entry is
		 * copied and the index stored within the copy
		 */
		shadow[rx_number] =
			&p->shadow_dqrr[DQRR_PTR2IDX(dq[rx_number])];
		shadow[rx_number]->fd.opaque_addr =
			dq[rx_number]->fd.opaque_addr;
		shadow[rx_number]->fd.addr =
			be40_to_cpu(dq[rx_number]->fd.addr);
		shadow[rx_number]->fd.opaque =
			be32_to_cpu(dq[rx_number]->fd.opaque);
#else
		shadow[rx_number] = dq[rx_number];
#endif

		/* SDQCR: context_b points to the FQ */
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
		fq = qman_fq_lookup_table[dq[rx_number]->contextB];
#else
		fq = (void *)dq[rx_number]->contextB;
#endif
		if (fq->cb.dqrr_prepare)
			fq->cb.dqrr_prepare(shadow[rx_number],
					    &bufs[rx_number]);

		consume |= (1 << (31 - DQRR_PTR2IDX(shadow[rx_number])));
		rx_number++;
		--dqrr->fill;
	} while (++limit < poll_limit);

	if (rx_number)
		fq->cb.dqrr_dpdk_pull_cb(&fq, shadow, bufs, rx_number);

	/* Consume all the DQRR enries together */
	qm_out(DQRR_DCAP, (1 << 8) | consume);

	return rx_number;
}

void qman_clear_irq(void)
{
	struct qman_portal *p = get_affine_portal();
	u32 clear = QM_DQAVAIL_MASK | (p->irq_sources &
		~(QM_PIRQ_CSCI | QM_PIRQ_CCSCI));
	qm_isr_status_clear(&p->p, clear);
}

u32 qman_portal_dequeue(struct rte_event ev[], unsigned int poll_limit,
			void **bufs)
{
	const struct qm_dqrr_entry *dq;
	struct qman_fq *fq;
	enum qman_cb_dqrr_result res;
	unsigned int limit = 0;
	struct qman_portal *p = get_affine_portal();
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	struct qm_dqrr_entry *shadow;
#endif
	unsigned int rx_number = 0;

	do {
		qm_dqrr_pvb_update(&p->p);
		dq = qm_dqrr_current(&p->p);
		if (!dq)
			break;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		/*
		 * If running on an LE system the fields of the
		 * dequeue entry must be swapper.  Because the
		 * QMan HW will ignore writes the DQRR entry is
		 * copied and the index stored within the copy
		 */
		shadow = &p->shadow_dqrr[DQRR_PTR2IDX(dq)];
		*shadow = *dq;
		dq = shadow;
		shadow->fqid = be32_to_cpu(shadow->fqid);
		shadow->seqnum = be16_to_cpu(shadow->seqnum);
		hw_fd_to_cpu(&shadow->fd);
#endif

	       /* SDQCR: context_b points to the FQ */
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
		fq = get_fq_table_entry(dq->contextB);
#else
		fq = (void *)(uintptr_t)dq->contextB;
#endif
		/* Now let the callback do its stuff */
		res = fq->cb.dqrr_dpdk_cb(&ev[rx_number], p, fq,
					 dq, &bufs[rx_number]);
		rx_number++;
		/* Interpret 'dq' from a driver perspective. */
		/*
		 * Parking isn't possible unless HELDACTIVE was set. NB,
		 * FORCEELIGIBLE implies HELDACTIVE, so we only need to
		 * check for HELDACTIVE to cover both.
		 */
		DPAA_ASSERT((dq->stat & QM_DQRR_STAT_FQ_HELDACTIVE) ||
			    (res != qman_cb_dqrr_park));
		if (res != qman_cb_dqrr_defer)
			qm_dqrr_cdc_consume_1ptr(&p->p, dq,
						 res == qman_cb_dqrr_park);
		/* Move forward */
		qm_dqrr_next(&p->p);
		/*
		 * Entry processed and consumed, increment our counter.  The
		 * callback can request that we exit after consuming the
		 * entry, and we also exit if we reach our processing limit,
		 * so loop back only if neither of these conditions is met.
		 */
	} while (++limit < poll_limit);

	return limit;
}

struct qm_dqrr_entry *qman_dequeue(struct qman_fq *fq)
{
	struct qman_portal *p = get_affine_portal();
	const struct qm_dqrr_entry *dq;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	struct qm_dqrr_entry *shadow;
#endif

	qm_dqrr_pvb_update(&p->p);
	dq = qm_dqrr_current(&p->p);
	if (!dq)
		return NULL;

	if (!(dq->stat & QM_DQRR_STAT_FD_VALID)) {
		/* Invalid DQRR - put the portal and consume the DQRR.
		 * Return NULL to user as no packet is seen.
		 */
		qman_dqrr_consume(fq, (struct qm_dqrr_entry *)dq);
		return NULL;
	}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	shadow = &p->shadow_dqrr[DQRR_PTR2IDX(dq)];
	*shadow = *dq;
	dq = shadow;
	shadow->fqid = be32_to_cpu(shadow->fqid);
	shadow->seqnum = be16_to_cpu(shadow->seqnum);
	hw_fd_to_cpu(&shadow->fd);
#endif

	if (dq->stat & QM_DQRR_STAT_FQ_EMPTY)
		fq_clear(fq, QMAN_FQ_STATE_NE);

	return (struct qm_dqrr_entry *)dq;
}

void qman_dqrr_consume(struct qman_fq *fq,
		       struct qm_dqrr_entry *dq)
{
	struct qman_portal *p = get_affine_portal();

	if (dq->stat & QM_DQRR_STAT_DQCR_EXPIRED)
		clear_vdqcr(p, fq);

	qm_dqrr_cdc_consume_1ptr(&p->p, dq, 0);
	qm_dqrr_next(&p->p);
}

int qman_poll_dqrr(unsigned int limit)
{
	struct qman_portal *p = get_affine_portal();
	int ret;

	ret = __poll_portal_fast(p, limit);
	return ret;
}

void qman_poll(void)
{
	struct qman_portal *p = get_affine_portal();

	if ((~p->irq_sources) & QM_PIRQ_SLOW) {
		if (!(p->slowpoll--)) {
			u32 is = qm_isr_status_read(&p->p) & ~p->irq_sources;
			u32 active = __poll_portal_slow(p, is);

			if (active) {
				qm_isr_status_clear(&p->p, active);
				p->slowpoll = SLOW_POLL_BUSY;
			} else
				p->slowpoll = SLOW_POLL_IDLE;
		}
	}
	if ((~p->irq_sources) & QM_PIRQ_DQRI)
		__poll_portal_fast(p, FSL_QMAN_POLL_LIMIT);
}

void qman_stop_dequeues(void)
{
	struct qman_portal *p = get_affine_portal();

	qman_stop_dequeues_ex(p);
}

void qman_start_dequeues(void)
{
	struct qman_portal *p = get_affine_portal();

	DPAA_ASSERT(p->dqrr_disable_ref > 0);
	if (!(--p->dqrr_disable_ref))
		qm_dqrr_set_maxfill(&p->p, DQRR_MAXFILL);
}

void qman_static_dequeue_add(u32 pools, struct qman_portal *qp)
{
	struct qman_portal *p = qp ? qp : get_affine_portal();

	pools &= p->config->pools;
	p->sdqcr |= pools;
	qm_dqrr_sdqcr_set(&p->p, p->sdqcr);
}

void qman_static_dequeue_del(u32 pools, struct qman_portal *qp)
{
	struct qman_portal *p = qp ? qp : get_affine_portal();

	pools &= p->config->pools;
	p->sdqcr &= ~pools;
	qm_dqrr_sdqcr_set(&p->p, p->sdqcr);
}

u32 qman_static_dequeue_get(struct qman_portal *qp)
{
	struct qman_portal *p = qp ? qp : get_affine_portal();
	return p->sdqcr;
}

void qman_dca(const struct qm_dqrr_entry *dq, int park_request)
{
	struct qman_portal *p = get_affine_portal();

	qm_dqrr_cdc_consume_1ptr(&p->p, dq, park_request);
}

void qman_dca_index(u8 index, int park_request)
{
	struct qman_portal *p = get_affine_portal();

	qm_dqrr_cdc_consume_1(&p->p, index, park_request);
}

/* Frame queue API */
static const char *mcr_result_str(u8 result)
{
	switch (result) {
	case QM_MCR_RESULT_NULL:
		return "QM_MCR_RESULT_NULL";
	case QM_MCR_RESULT_OK:
		return "QM_MCR_RESULT_OK";
	case QM_MCR_RESULT_ERR_FQID:
		return "QM_MCR_RESULT_ERR_FQID";
	case QM_MCR_RESULT_ERR_FQSTATE:
		return "QM_MCR_RESULT_ERR_FQSTATE";
	case QM_MCR_RESULT_ERR_NOTEMPTY:
		return "QM_MCR_RESULT_ERR_NOTEMPTY";
	case QM_MCR_RESULT_PENDING:
		return "QM_MCR_RESULT_PENDING";
	case QM_MCR_RESULT_ERR_BADCOMMAND:
		return "QM_MCR_RESULT_ERR_BADCOMMAND";
	}
	return "<unknown MCR result>";
}

int qman_create_fq(u32 fqid, u32 flags, struct qman_fq *fq)
{
	struct qm_fqd fqd;
	struct qm_mcr_queryfq_np np;
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p;

	if (flags & QMAN_FQ_FLAG_DYNAMIC_FQID) {
		int ret = qman_alloc_fqid(&fqid);

		if (ret)
			return ret;
	}
	spin_lock_init(&fq->fqlock);
	fq->fqid = fqid;
	fq->fqid_le = cpu_to_be32(fqid);
	fq->flags = flags;
	fq->state = qman_fq_state_oos;
	fq->cgr_groupid = 0;
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
	if (unlikely(find_empty_fq_table_entry(&fq->key, fq))) {
		pr_info("Find empty table entry failed\n");
		return -ENOMEM;
	}
#endif
	if (!(flags & QMAN_FQ_FLAG_AS_IS) || (flags & QMAN_FQ_FLAG_NO_MODIFY))
		return 0;
	/* Everything else is AS_IS support */
	p = get_affine_portal();
	mcc = qm_mc_start(&p->p);
	mcc->queryfq.fqid = cpu_to_be32(fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYFQ);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCC_VERB_QUERYFQ);
	if (mcr->result != QM_MCR_RESULT_OK) {
		pr_err("QUERYFQ failed: %s\n", mcr_result_str(mcr->result));
		goto err;
	}
	fqd = mcr->queryfq.fqd;
	hw_fqd_to_cpu(&fqd);
	mcc = qm_mc_start(&p->p);
	mcc->queryfq_np.fqid = cpu_to_be32(fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYFQ_NP);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCC_VERB_QUERYFQ_NP);
	if (mcr->result != QM_MCR_RESULT_OK) {
		pr_err("QUERYFQ_NP failed: %s\n", mcr_result_str(mcr->result));
		goto err;
	}
	np = mcr->queryfq_np;
	/* Phew, have queryfq and queryfq_np results, stitch together
	 * the FQ object from those.
	 */
	fq->cgr_groupid = fqd.cgid;
	switch (np.state & QM_MCR_NP_STATE_MASK) {
	case QM_MCR_NP_STATE_OOS:
		break;
	case QM_MCR_NP_STATE_RETIRED:
		fq->state = qman_fq_state_retired;
		if (np.frm_cnt)
			fq_set(fq, QMAN_FQ_STATE_NE);
		break;
	case QM_MCR_NP_STATE_TEN_SCHED:
	case QM_MCR_NP_STATE_TRU_SCHED:
	case QM_MCR_NP_STATE_ACTIVE:
		fq->state = qman_fq_state_sched;
		if (np.state & QM_MCR_NP_STATE_R)
			fq_set(fq, QMAN_FQ_STATE_CHANGING);
		break;
	case QM_MCR_NP_STATE_PARKED:
		fq->state = qman_fq_state_parked;
		break;
	default:
		DPAA_ASSERT(NULL == "invalid FQ state");
	}
	if (fqd.fq_ctrl & QM_FQCTRL_CGE)
		fq->state |= QMAN_FQ_STATE_CGR_EN;
	return 0;
err:
	if (flags & QMAN_FQ_FLAG_DYNAMIC_FQID)
		qman_release_fqid(fqid);
	return -EIO;
}

void qman_destroy_fq(struct qman_fq *fq, u32 flags __maybe_unused)
{
	/*
	 * We don't need to lock the FQ as it is a pre-condition that the FQ be
	 * quiesced. Instead, run some checks.
	 */
	switch (fq->state) {
	case qman_fq_state_parked:
		DPAA_ASSERT(flags & QMAN_FQ_DESTROY_PARKED);
		/* Fallthrough */
	case qman_fq_state_oos:
		if (fq_isset(fq, QMAN_FQ_FLAG_DYNAMIC_FQID))
			qman_release_fqid(fq->fqid);
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
		clear_fq_table_entry(fq->key);
#endif
		return;
	default:
		break;
	}
	DPAA_ASSERT(NULL == "qman_free_fq() on unquiesced FQ!");
}

u32 qman_fq_fqid(struct qman_fq *fq)
{
	return fq->fqid;
}

void qman_fq_state(struct qman_fq *fq, enum qman_fq_state *state, u32 *flags)
{
	if (state)
		*state = fq->state;
	if (flags)
		*flags = fq->flags;
}

int qman_init_fq(struct qman_fq *fq, u32 flags, struct qm_mcc_initfq *opts)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p;

	u8 res, myverb = (flags & QMAN_INITFQ_FLAG_SCHED) ?
		QM_MCC_VERB_INITFQ_SCHED : QM_MCC_VERB_INITFQ_PARKED;

	if ((fq->state != qman_fq_state_oos) &&
	    (fq->state != qman_fq_state_parked))
		return -EINVAL;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (unlikely(fq_isset(fq, QMAN_FQ_FLAG_NO_MODIFY)))
		return -EINVAL;
#endif
	if (opts && (opts->we_mask & QM_INITFQ_WE_OAC)) {
		/* And can't be set at the same time as TDTHRESH */
		if (opts->we_mask & QM_INITFQ_WE_TDTHRESH)
			return -EINVAL;
	}
	/* Issue an INITFQ_[PARKED|SCHED] management command */
	p = get_affine_portal();
	FQLOCK(fq);
	if (unlikely((fq_isset(fq, QMAN_FQ_STATE_CHANGING)) ||
		     ((fq->state != qman_fq_state_oos) &&
				(fq->state != qman_fq_state_parked)))) {
		FQUNLOCK(fq);
		return -EBUSY;
	}
	mcc = qm_mc_start(&p->p);
	if (opts)
		mcc->initfq = *opts;
	mcc->initfq.fqid = cpu_to_be32(fq->fqid);
	mcc->initfq.count = 0;
	/*
	 * If the FQ does *not* have the TO_DCPORTAL flag, context_b is set as a
	 * demux pointer. Otherwise, the caller-provided value is allowed to
	 * stand, don't overwrite it.
	 */
	if (fq_isclear(fq, QMAN_FQ_FLAG_TO_DCPORTAL)) {
		dma_addr_t phys_fq;

		mcc->initfq.we_mask |= QM_INITFQ_WE_CONTEXTB;
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
		mcc->initfq.fqd.context_b = cpu_to_be32(fq->key);
#else
		mcc->initfq.fqd.context_b = (u32)(uintptr_t)fq;
#endif
		/*
		 *  and the physical address - NB, if the user wasn't trying to
		 * set CONTEXTA, clear the stashing settings.
		 */
		if (!(mcc->initfq.we_mask & QM_INITFQ_WE_CONTEXTA)) {
			mcc->initfq.we_mask |= QM_INITFQ_WE_CONTEXTA;
			memset(&mcc->initfq.fqd.context_a, 0,
			       sizeof(mcc->initfq.fqd.context_a));
		} else {
			phys_fq = rte_mem_virt2iova(fq);
			qm_fqd_stashing_set64(&mcc->initfq.fqd, phys_fq);
		}
	}
	if (flags & QMAN_INITFQ_FLAG_LOCAL) {
		mcc->initfq.fqd.dest.channel = p->config->channel;
		if (!(mcc->initfq.we_mask & QM_INITFQ_WE_DESTWQ)) {
			mcc->initfq.we_mask |= QM_INITFQ_WE_DESTWQ;
			mcc->initfq.fqd.dest.wq = 4;
		}
	}
	mcc->initfq.we_mask = cpu_to_be16(mcc->initfq.we_mask);
	cpu_to_hw_fqd(&mcc->initfq.fqd);
	qm_mc_commit(&p->p, myverb);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == myverb);
	res = mcr->result;
	if (res != QM_MCR_RESULT_OK) {
		FQUNLOCK(fq);
		return -EIO;
	}
	if (opts) {
		if (opts->we_mask & QM_INITFQ_WE_FQCTRL) {
			if (opts->fqd.fq_ctrl & QM_FQCTRL_CGE)
				fq_set(fq, QMAN_FQ_STATE_CGR_EN);
			else
				fq_clear(fq, QMAN_FQ_STATE_CGR_EN);
		}
		if (opts->we_mask & QM_INITFQ_WE_CGID)
			fq->cgr_groupid = opts->fqd.cgid;
	}
	fq->state = (flags & QMAN_INITFQ_FLAG_SCHED) ?
		qman_fq_state_sched : qman_fq_state_parked;
	FQUNLOCK(fq);
	return 0;
}

int qman_schedule_fq(struct qman_fq *fq)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p;

	int ret = 0;
	u8 res;

	if (fq->state != qman_fq_state_parked)
		return -EINVAL;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (unlikely(fq_isset(fq, QMAN_FQ_FLAG_NO_MODIFY)))
		return -EINVAL;
#endif
	/* Issue a ALTERFQ_SCHED management command */
	p = get_affine_portal();

	FQLOCK(fq);
	if (unlikely((fq_isset(fq, QMAN_FQ_STATE_CHANGING)) ||
		     (fq->state != qman_fq_state_parked))) {
		ret = -EBUSY;
		goto out;
	}
	mcc = qm_mc_start(&p->p);
	mcc->alterfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_ALTER_SCHED);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_ALTER_SCHED);
	res = mcr->result;
	if (res != QM_MCR_RESULT_OK) {
		ret = -EIO;
		goto out;
	}
	fq->state = qman_fq_state_sched;
out:
	FQUNLOCK(fq);

	return ret;
}

int qman_retire_fq(struct qman_fq *fq, u32 *flags)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p;

	int rval;
	u8 res;

	if ((fq->state != qman_fq_state_parked) &&
	    (fq->state != qman_fq_state_sched))
		return -EINVAL;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (unlikely(fq_isset(fq, QMAN_FQ_FLAG_NO_MODIFY)))
		return -EINVAL;
#endif
	p = get_affine_portal();

	FQLOCK(fq);
	if (unlikely((fq_isset(fq, QMAN_FQ_STATE_CHANGING)) ||
		     (fq->state == qman_fq_state_retired) ||
				(fq->state == qman_fq_state_oos))) {
		rval = -EBUSY;
		goto out;
	}
	rval = table_push_fq(p, fq);
	if (rval)
		goto out;
	mcc = qm_mc_start(&p->p);
	mcc->alterfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_ALTER_RETIRE);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_ALTER_RETIRE);
	res = mcr->result;
	/*
	 * "Elegant" would be to treat OK/PENDING the same way; set CHANGING,
	 * and defer the flags until FQRNI or FQRN (respectively) show up. But
	 * "Friendly" is to process OK immediately, and not set CHANGING. We do
	 * friendly, otherwise the caller doesn't necessarily have a fully
	 * "retired" FQ on return even if the retirement was immediate. However
	 * this does mean some code duplication between here and
	 * fq_state_change().
	 */
	if (likely(res == QM_MCR_RESULT_OK)) {
		rval = 0;
		/* Process 'fq' right away, we'll ignore FQRNI */
		if (mcr->alterfq.fqs & QM_MCR_FQS_NOTEMPTY)
			fq_set(fq, QMAN_FQ_STATE_NE);
		if (mcr->alterfq.fqs & QM_MCR_FQS_ORLPRESENT)
			fq_set(fq, QMAN_FQ_STATE_ORL);
		else
			table_del_fq(p, fq);
		if (flags)
			*flags = fq->flags;
		fq->state = qman_fq_state_retired;
		if (fq->cb.fqs) {
			/*
			 * Another issue with supporting "immediate" retirement
			 * is that we're forced to drop FQRNIs, because by the
			 * time they're seen it may already be "too late" (the
			 * fq may have been OOS'd and free()'d already). But if
			 * the upper layer wants a callback whether it's
			 * immediate or not, we have to fake a "MR" entry to
			 * look like an FQRNI...
			 */
			struct qm_mr_entry msg;

			msg.ern.verb = QM_MR_VERB_FQRNI;
			msg.fq.fqs = mcr->alterfq.fqs;
			msg.fq.fqid = fq->fqid;
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
			msg.fq.contextB = fq->key;
#else
			msg.fq.contextB = (u32)(uintptr_t)fq;
#endif
			fq->cb.fqs(p, fq, &msg);
		}
	} else if (res == QM_MCR_RESULT_PENDING) {
		rval = 1;
		fq_set(fq, QMAN_FQ_STATE_CHANGING);
	} else {
		rval = -EIO;
		table_del_fq(p, fq);
	}
out:
	FQUNLOCK(fq);
	return rval;
}

int qman_oos_fq(struct qman_fq *fq)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p;

	int ret = 0;
	u8 res;

	if (fq->state != qman_fq_state_retired)
		return -EINVAL;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (unlikely(fq_isset(fq, QMAN_FQ_FLAG_NO_MODIFY)))
		return -EINVAL;
#endif
	p = get_affine_portal();
	FQLOCK(fq);
	if (unlikely((fq_isset(fq, QMAN_FQ_STATE_BLOCKOOS)) ||
		     (fq->state != qman_fq_state_retired))) {
		ret = -EBUSY;
		goto out;
	}
	mcc = qm_mc_start(&p->p);
	mcc->alterfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_ALTER_OOS);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_ALTER_OOS);
	res = mcr->result;
	if (res != QM_MCR_RESULT_OK) {
		ret = -EIO;
		goto out;
	}
	fq->state = qman_fq_state_oos;
out:
	FQUNLOCK(fq);
	return ret;
}

int qman_fq_flow_control(struct qman_fq *fq, int xon)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p;

	int ret = 0;
	u8 res;
	u8 myverb;

	if ((fq->state == qman_fq_state_oos) ||
	    (fq->state == qman_fq_state_retired) ||
		(fq->state == qman_fq_state_parked))
		return -EINVAL;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (unlikely(fq_isset(fq, QMAN_FQ_FLAG_NO_MODIFY)))
		return -EINVAL;
#endif
	/* Issue a ALTER_FQXON or ALTER_FQXOFF management command */
	p = get_affine_portal();
	FQLOCK(fq);
	if (unlikely((fq_isset(fq, QMAN_FQ_STATE_CHANGING)) ||
		     (fq->state == qman_fq_state_parked) ||
			(fq->state == qman_fq_state_oos) ||
			(fq->state == qman_fq_state_retired))) {
		ret = -EBUSY;
		goto out;
	}
	mcc = qm_mc_start(&p->p);
	mcc->alterfq.fqid = fq->fqid;
	mcc->alterfq.count = 0;
	myverb = xon ? QM_MCC_VERB_ALTER_FQXON : QM_MCC_VERB_ALTER_FQXOFF;

	qm_mc_commit(&p->p, myverb);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == myverb);

	res = mcr->result;
	if (res != QM_MCR_RESULT_OK) {
		ret = -EIO;
		goto out;
	}
out:
	FQUNLOCK(fq);
	return ret;
}

int qman_query_fq(struct qman_fq *fq, struct qm_fqd *fqd)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	u8 res;

	mcc = qm_mc_start(&p->p);
	mcc->queryfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYFQ);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_QUERYFQ);
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK)
		*fqd = mcr->queryfq.fqd;
	hw_fqd_to_cpu(fqd);
	if (res != QM_MCR_RESULT_OK)
		return -EIO;
	return 0;
}

int qman_query_fq_has_pkts(struct qman_fq *fq)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	int ret = 0;
	u8 res;

	mcc = qm_mc_start(&p->p);
	mcc->queryfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYFQ_NP);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK)
		ret = !!mcr->queryfq_np.frm_cnt;
	return ret;
}

int qman_query_fq_np(struct qman_fq *fq, struct qm_mcr_queryfq_np *np)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	u8 res;

	mcc = qm_mc_start(&p->p);
	mcc->queryfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYFQ_NP);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_QUERYFQ_NP);
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK) {
		*np = mcr->queryfq_np;
		np->fqd_link = be24_to_cpu(np->fqd_link);
		np->odp_seq = be16_to_cpu(np->odp_seq);
		np->orp_nesn = be16_to_cpu(np->orp_nesn);
		np->orp_ea_hseq  = be16_to_cpu(np->orp_ea_hseq);
		np->orp_ea_tseq  = be16_to_cpu(np->orp_ea_tseq);
		np->orp_ea_hptr = be24_to_cpu(np->orp_ea_hptr);
		np->orp_ea_tptr = be24_to_cpu(np->orp_ea_tptr);
		np->pfdr_hptr = be24_to_cpu(np->pfdr_hptr);
		np->pfdr_tptr = be24_to_cpu(np->pfdr_tptr);
		np->ics_surp = be16_to_cpu(np->ics_surp);
		np->byte_cnt = be32_to_cpu(np->byte_cnt);
		np->frm_cnt = be24_to_cpu(np->frm_cnt);
		np->ra1_sfdr = be16_to_cpu(np->ra1_sfdr);
		np->ra2_sfdr = be16_to_cpu(np->ra2_sfdr);
		np->od1_sfdr = be16_to_cpu(np->od1_sfdr);
		np->od2_sfdr = be16_to_cpu(np->od2_sfdr);
		np->od3_sfdr = be16_to_cpu(np->od3_sfdr);
	}
	if (res == QM_MCR_RESULT_ERR_FQID)
		return -ERANGE;
	else if (res != QM_MCR_RESULT_OK)
		return -EIO;
	return 0;
}

int qman_query_fq_frm_cnt(struct qman_fq *fq, u32 *frm_cnt)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	mcc = qm_mc_start(&p->p);
	mcc->queryfq.fqid = cpu_to_be32(fq->fqid);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYFQ_NP);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_QUERYFQ_NP);

	if (mcr->result == QM_MCR_RESULT_OK)
		*frm_cnt = be24_to_cpu(mcr->queryfq_np.frm_cnt);
	else if (mcr->result == QM_MCR_RESULT_ERR_FQID)
		return -ERANGE;
	else if (mcr->result != QM_MCR_RESULT_OK)
		return -EIO;
	return 0;
}

int qman_query_wq(u8 query_dedicated, struct qm_mcr_querywq *wq)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	u8 res, myverb;

	myverb = (query_dedicated) ? QM_MCR_VERB_QUERYWQ_DEDICATED :
				 QM_MCR_VERB_QUERYWQ;
	mcc = qm_mc_start(&p->p);
	mcc->querywq.channel.id = cpu_to_be16(wq->channel.id);
	qm_mc_commit(&p->p, myverb);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == myverb);
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK) {
		int i, array_len;

		wq->channel.id = be16_to_cpu(mcr->querywq.channel.id);
		array_len = ARRAY_SIZE(mcr->querywq.wq_len);
		for (i = 0; i < array_len; i++)
			wq->wq_len[i] = be32_to_cpu(mcr->querywq.wq_len[i]);
	}
	if (res != QM_MCR_RESULT_OK) {
		pr_err("QUERYWQ failed: %s\n", mcr_result_str(res));
		return -EIO;
	}
	return 0;
}

int qman_testwrite_cgr(struct qman_cgr *cgr, u64 i_bcnt,
		       struct qm_mcr_cgrtestwrite *result)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	u8 res;

	mcc = qm_mc_start(&p->p);
	mcc->cgrtestwrite.cgid = cgr->cgrid;
	mcc->cgrtestwrite.i_bcnt_hi = (u8)(i_bcnt >> 32);
	mcc->cgrtestwrite.i_bcnt_lo = (u32)i_bcnt;
	qm_mc_commit(&p->p, QM_MCC_VERB_CGRTESTWRITE);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCC_VERB_CGRTESTWRITE);
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK)
		*result = mcr->cgrtestwrite;
	if (res != QM_MCR_RESULT_OK) {
		pr_err("CGR TEST WRITE failed: %s\n", mcr_result_str(res));
		return -EIO;
	}
	return 0;
}

int qman_query_cgr(struct qman_cgr *cgr, struct qm_mcr_querycgr *cgrd)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();
	u8 res;
	unsigned int i;

	mcc = qm_mc_start(&p->p);
	mcc->querycgr.cgid = cgr->cgrid;
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYCGR);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCC_VERB_QUERYCGR);
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK)
		*cgrd = mcr->querycgr;
	if (res != QM_MCR_RESULT_OK) {
		pr_err("QUERY_CGR failed: %s\n", mcr_result_str(res));
		return -EIO;
	}
	cgrd->cgr.wr_parm_g.word =
		be32_to_cpu(cgrd->cgr.wr_parm_g.word);
	cgrd->cgr.wr_parm_y.word =
		be32_to_cpu(cgrd->cgr.wr_parm_y.word);
	cgrd->cgr.wr_parm_r.word =
		be32_to_cpu(cgrd->cgr.wr_parm_r.word);
	cgrd->cgr.cscn_targ =  be32_to_cpu(cgrd->cgr.cscn_targ);
	cgrd->cgr.__cs_thres = be16_to_cpu(cgrd->cgr.__cs_thres);
	for (i = 0; i < ARRAY_SIZE(cgrd->cscn_targ_swp); i++)
		cgrd->cscn_targ_swp[i] =
			be32_to_cpu(cgrd->cscn_targ_swp[i]);
	return 0;
}

int qman_query_congestion(struct qm_mcr_querycongestion *congestion)
{
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();
	u8 res;
	unsigned int i;

	qm_mc_start(&p->p);
	qm_mc_commit(&p->p, QM_MCC_VERB_QUERYCONGESTION);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) ==
			QM_MCC_VERB_QUERYCONGESTION);
	res = mcr->result;
	if (res == QM_MCR_RESULT_OK)
		*congestion = mcr->querycongestion;
	if (res != QM_MCR_RESULT_OK) {
		pr_err("QUERY_CONGESTION failed: %s\n", mcr_result_str(res));
		return -EIO;
	}
	for (i = 0; i < ARRAY_SIZE(congestion->state.state); i++)
		congestion->state.state[i] =
			be32_to_cpu(congestion->state.state[i]);
	return 0;
}

int qman_set_vdq(struct qman_fq *fq, u16 num, uint32_t vdqcr_flags)
{
	struct qman_portal *p = get_affine_portal();
	uint32_t vdqcr;
	int ret = -EBUSY;

	vdqcr = vdqcr_flags;
	vdqcr |= QM_VDQCR_NUMFRAMES_SET(num);

	if ((fq->state != qman_fq_state_parked) &&
	    (fq->state != qman_fq_state_retired)) {
		ret = -EINVAL;
		goto out;
	}
	if (fq_isset(fq, QMAN_FQ_STATE_VDQCR)) {
		ret = -EBUSY;
		goto out;
	}
	vdqcr = (vdqcr & ~QM_VDQCR_FQID_MASK) | fq->fqid;

	if (!p->vdqcr_owned) {
		FQLOCK(fq);
		if (fq_isset(fq, QMAN_FQ_STATE_VDQCR))
			goto escape;
		fq_set(fq, QMAN_FQ_STATE_VDQCR);
		FQUNLOCK(fq);
		p->vdqcr_owned = fq;
		ret = 0;
	}
escape:
	if (!ret)
		qm_dqrr_vdqcr_set(&p->p, vdqcr);

out:
	return ret;
}

int qman_volatile_dequeue(struct qman_fq *fq, u32 flags __maybe_unused,
			  u32 vdqcr)
{
	struct qman_portal *p;
	int ret = -EBUSY;

	if ((fq->state != qman_fq_state_parked) &&
	    (fq->state != qman_fq_state_retired))
		return -EINVAL;
	if (vdqcr & QM_VDQCR_FQID_MASK)
		return -EINVAL;
	if (fq_isset(fq, QMAN_FQ_STATE_VDQCR))
		return -EBUSY;
	vdqcr = (vdqcr & ~QM_VDQCR_FQID_MASK) | fq->fqid;

	p = get_affine_portal();

	if (!p->vdqcr_owned) {
		FQLOCK(fq);
		if (fq_isset(fq, QMAN_FQ_STATE_VDQCR))
			goto escape;
		fq_set(fq, QMAN_FQ_STATE_VDQCR);
		FQUNLOCK(fq);
		p->vdqcr_owned = fq;
		ret = 0;
	}
escape:
	if (ret)
		return ret;

	/* VDQCR is set */
	qm_dqrr_vdqcr_set(&p->p, vdqcr);
	return 0;
}

static noinline void update_eqcr_ci(struct qman_portal *p, u8 avail)
{
	if (avail)
		qm_eqcr_cce_prefetch(&p->p);
	else
		qm_eqcr_cce_update(&p->p);
}

int qman_eqcr_is_empty(void)
{
	struct qman_portal *p = get_affine_portal();
	u8 avail;

	update_eqcr_ci(p, 0);
	avail = qm_eqcr_get_fill(&p->p);
	return (avail == 0);
}

void qman_set_dc_ern(qman_cb_dc_ern handler, int affine)
{
	if (affine) {
		struct qman_portal *p = get_affine_portal();

		p->cb_dc_ern = handler;
	} else
		cb_dc_ern = handler;
}

static inline struct qm_eqcr_entry *try_p_eq_start(struct qman_portal *p,
					struct qman_fq *fq,
					const struct qm_fd *fd,
					u32 flags)
{
	struct qm_eqcr_entry *eq;
	u8 avail;

	if (p->use_eqcr_ci_stashing) {
		/*
		 * The stashing case is easy, only update if we need to in
		 * order to try and liberate ring entries.
		 */
		eq = qm_eqcr_start_stash(&p->p);
	} else {
		/*
		 * The non-stashing case is harder, need to prefetch ahead of
		 * time.
		 */
		avail = qm_eqcr_get_avail(&p->p);
		if (avail < 2)
			update_eqcr_ci(p, avail);
		eq = qm_eqcr_start_no_stash(&p->p);
	}

	if (unlikely(!eq))
		return NULL;

	if (flags & QMAN_ENQUEUE_FLAG_DCA)
		eq->dca = QM_EQCR_DCA_ENABLE |
			((flags & QMAN_ENQUEUE_FLAG_DCA_PARK) ?
					QM_EQCR_DCA_PARK : 0) |
			((flags >> 8) & QM_EQCR_DCA_IDXMASK);
	eq->fqid = cpu_to_be32(fq->fqid);
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
	eq->tag = cpu_to_be32(fq->key);
#else
	eq->tag = cpu_to_be32((u32)(uintptr_t)fq);
#endif
	eq->fd = *fd;
	cpu_to_hw_fd(&eq->fd);
	return eq;
}

int qman_enqueue(struct qman_fq *fq, const struct qm_fd *fd, u32 flags)
{
	struct qman_portal *p = get_affine_portal();
	struct qm_eqcr_entry *eq;

	eq = try_p_eq_start(p, fq, fd, flags);
	if (!eq)
		return -EBUSY;
	/* Note: QM_EQCR_VERB_INTERRUPT == QMAN_ENQUEUE_FLAG_WAIT_SYNC */
	qm_eqcr_pvb_commit(&p->p, QM_EQCR_VERB_CMD_ENQUEUE |
		(flags & (QM_EQCR_VERB_COLOUR_MASK | QM_EQCR_VERB_INTERRUPT)));
	/* Factor the below out, it's used from qman_enqueue_orp() too */
	return 0;
}

int qman_enqueue_multi(struct qman_fq *fq,
		       const struct qm_fd *fd, u32 *flags,
		int frames_to_send)
{
	struct qman_portal *p = get_affine_portal();
	struct qm_portal *portal = &p->p;

	register struct qm_eqcr *eqcr = &portal->eqcr;
	struct qm_eqcr_entry *eq = eqcr->cursor, *prev_eq;

	u8 i = 0, diff, old_ci, sent = 0;

	/* Update the available entries if no entry is free */
	if (!eqcr->available) {
		old_ci = eqcr->ci;
		eqcr->ci = qm_cl_in(EQCR_CI) & (QM_EQCR_SIZE - 1);
		diff = qm_cyc_diff(QM_EQCR_SIZE, old_ci, eqcr->ci);
		eqcr->available += diff;
		if (!diff)
			return 0;
	}

	/* try to send as many frames as possible */
	while (eqcr->available && frames_to_send--) {
		eq->fqid = fq->fqid_le;
		eq->fd.opaque_addr = fd->opaque_addr;
		eq->fd.addr = cpu_to_be40(fd->addr);
		eq->fd.status = cpu_to_be32(fd->status);
		eq->fd.opaque = cpu_to_be32(fd->opaque);
		if (flags && (flags[i] & QMAN_ENQUEUE_FLAG_DCA)) {
			eq->dca = QM_EQCR_DCA_ENABLE |
				((flags[i] >> 8) & QM_EQCR_DCA_IDXMASK);
		}
		i++;
		eq = (void *)((unsigned long)(eq + 1) &
			(~(unsigned long)(QM_EQCR_SIZE << 6)));
		eqcr->available--;
		sent++;
		fd++;
	}
	lwsync();

	/* In order for flushes to complete faster, all lines are recorded in
	 * 32 bit word.
	 */
	eq = eqcr->cursor;
	for (i = 0; i < sent; i++) {
		eq->__dont_write_directly__verb =
			QM_EQCR_VERB_CMD_ENQUEUE | eqcr->vbit;
		prev_eq = eq;
		eq = (void *)((unsigned long)(eq + 1) &
			(~(unsigned long)(QM_EQCR_SIZE << 6)));
		if (unlikely((prev_eq + 1) != eq))
			eqcr->vbit ^= QM_EQCR_VERB_VBIT;
	}

	/* We need  to flush all the lines but without load/store operations
	 * between them
	 */
	eq = eqcr->cursor;
	for (i = 0; i < sent; i++) {
		dcbf(eq);
		eq = (void *)((unsigned long)(eq + 1) &
			(~(unsigned long)(QM_EQCR_SIZE << 6)));
	}
	/* Update cursor for the next call */
	eqcr->cursor = eq;
	return sent;
}

int
qman_enqueue_multi_fq(struct qman_fq *fq[], const struct qm_fd *fd,
		      int frames_to_send)
{
	struct qman_portal *p = get_affine_portal();
	struct qm_portal *portal = &p->p;

	register struct qm_eqcr *eqcr = &portal->eqcr;
	struct qm_eqcr_entry *eq = eqcr->cursor, *prev_eq;

	u8 i, diff, old_ci, sent = 0;

	/* Update the available entries if no entry is free */
	if (!eqcr->available) {
		old_ci = eqcr->ci;
		eqcr->ci = qm_cl_in(EQCR_CI) & (QM_EQCR_SIZE - 1);
		diff = qm_cyc_diff(QM_EQCR_SIZE, old_ci, eqcr->ci);
		eqcr->available += diff;
		if (!diff)
			return 0;
	}

	/* try to send as many frames as possible */
	while (eqcr->available && frames_to_send--) {
		eq->fqid = fq[sent]->fqid_le;
		eq->fd.opaque_addr = fd->opaque_addr;
		eq->fd.addr = cpu_to_be40(fd->addr);
		eq->fd.status = cpu_to_be32(fd->status);
		eq->fd.opaque = cpu_to_be32(fd->opaque);

		eq = (void *)((unsigned long)(eq + 1) &
			(~(unsigned long)(QM_EQCR_SIZE << 6)));
		eqcr->available--;
		sent++;
		fd++;
	}
	lwsync();

	/* In order for flushes to complete faster, all lines are recorded in
	 * 32 bit word.
	 */
	eq = eqcr->cursor;
	for (i = 0; i < sent; i++) {
		eq->__dont_write_directly__verb =
			QM_EQCR_VERB_CMD_ENQUEUE | eqcr->vbit;
		prev_eq = eq;
		eq = (void *)((unsigned long)(eq + 1) &
			(~(unsigned long)(QM_EQCR_SIZE << 6)));
		if (unlikely((prev_eq + 1) != eq))
			eqcr->vbit ^= QM_EQCR_VERB_VBIT;
	}

	/* We need  to flush all the lines but without load/store operations
	 * between them
	 */
	eq = eqcr->cursor;
	for (i = 0; i < sent; i++) {
		dcbf(eq);
		eq = (void *)((unsigned long)(eq + 1) &
			(~(unsigned long)(QM_EQCR_SIZE << 6)));
	}
	/* Update cursor for the next call */
	eqcr->cursor = eq;
	return sent;
}

int qman_enqueue_orp(struct qman_fq *fq, const struct qm_fd *fd, u32 flags,
		     struct qman_fq *orp, u16 orp_seqnum)
{
	struct qman_portal *p  = get_affine_portal();
	struct qm_eqcr_entry *eq;

	eq = try_p_eq_start(p, fq, fd, flags);
	if (!eq)
		return -EBUSY;
	/* Process ORP-specifics here */
	if (flags & QMAN_ENQUEUE_FLAG_NLIS)
		orp_seqnum |= QM_EQCR_SEQNUM_NLIS;
	else {
		orp_seqnum &= ~QM_EQCR_SEQNUM_NLIS;
		if (flags & QMAN_ENQUEUE_FLAG_NESN)
			orp_seqnum |= QM_EQCR_SEQNUM_NESN;
		else
			/* No need to check 4 QMAN_ENQUEUE_FLAG_HOLE */
			orp_seqnum &= ~QM_EQCR_SEQNUM_NESN;
	}
	eq->seqnum = cpu_to_be16(orp_seqnum);
	eq->orp = cpu_to_be32(orp->fqid);
	/* Note: QM_EQCR_VERB_INTERRUPT == QMAN_ENQUEUE_FLAG_WAIT_SYNC */
	qm_eqcr_pvb_commit(&p->p, QM_EQCR_VERB_ORP |
		((flags & (QMAN_ENQUEUE_FLAG_HOLE | QMAN_ENQUEUE_FLAG_NESN)) ?
				0 : QM_EQCR_VERB_CMD_ENQUEUE) |
		(flags & (QM_EQCR_VERB_COLOUR_MASK | QM_EQCR_VERB_INTERRUPT)));

	return 0;
}

int qman_modify_cgr(struct qman_cgr *cgr, u32 flags,
		    struct qm_mcc_initcgr *opts)
{
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	struct qman_portal *p = get_affine_portal();

	u8 res;
	u8 verb = QM_MCC_VERB_MODIFYCGR;

	mcc = qm_mc_start(&p->p);
	if (opts)
		mcc->initcgr = *opts;
	mcc->initcgr.we_mask = cpu_to_be16(mcc->initcgr.we_mask);
	mcc->initcgr.cgr.wr_parm_g.word =
		cpu_to_be32(mcc->initcgr.cgr.wr_parm_g.word);
	mcc->initcgr.cgr.wr_parm_y.word =
		cpu_to_be32(mcc->initcgr.cgr.wr_parm_y.word);
	mcc->initcgr.cgr.wr_parm_r.word =
		cpu_to_be32(mcc->initcgr.cgr.wr_parm_r.word);
	mcc->initcgr.cgr.cscn_targ =  cpu_to_be32(mcc->initcgr.cgr.cscn_targ);
	mcc->initcgr.cgr.__cs_thres = cpu_to_be16(mcc->initcgr.cgr.__cs_thres);

	mcc->initcgr.cgid = cgr->cgrid;
	if (flags & QMAN_CGR_FLAG_USE_INIT)
		verb = QM_MCC_VERB_INITCGR;
	qm_mc_commit(&p->p, verb);
	while (!(mcr = qm_mc_result(&p->p)))
		cpu_relax();

	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == verb);
	res = mcr->result;
	return (res == QM_MCR_RESULT_OK) ? 0 : -EIO;
}

#define TARG_MASK(n) (0x80000000 >> (n->config->channel - \
					QM_CHANNEL_SWPORTAL0))
#define TARG_DCP_MASK(n) (0x80000000 >> (10 + n))
#define PORTAL_IDX(n) (n->config->channel - QM_CHANNEL_SWPORTAL0)

int qman_create_cgr(struct qman_cgr *cgr, u32 flags,
		    struct qm_mcc_initcgr *opts)
{
	struct qm_mcr_querycgr cgr_state;
	struct qm_mcc_initcgr local_opts;
	int ret;
	struct qman_portal *p;

	/* We have to check that the provided CGRID is within the limits of the
	 * data-structures, for obvious reasons. However we'll let h/w take
	 * care of determining whether it's within the limits of what exists on
	 * the SoC.
	 */
	if (cgr->cgrid >= __CGR_NUM)
		return -EINVAL;

	p = get_affine_portal();

	memset(&local_opts, 0, sizeof(struct qm_mcc_initcgr));
	cgr->chan = p->config->channel;
	spin_lock(&p->cgr_lock);

	/* if no opts specified, just add it to the list */
	if (!opts)
		goto add_list;

	ret = qman_query_cgr(cgr, &cgr_state);
	if (ret)
		goto release_lock;
	if (opts)
		local_opts = *opts;
	if ((qman_ip_rev & 0xFF00) >= QMAN_REV30)
		local_opts.cgr.cscn_targ_upd_ctrl =
			QM_CGR_TARG_UDP_CTRL_WRITE_BIT | PORTAL_IDX(p);
	else
		/* Overwrite TARG */
		local_opts.cgr.cscn_targ = cgr_state.cgr.cscn_targ |
							TARG_MASK(p);
	local_opts.we_mask |= QM_CGR_WE_CSCN_TARG;

	/* send init if flags indicate so */
	if (opts && (flags & QMAN_CGR_FLAG_USE_INIT))
		ret = qman_modify_cgr(cgr, QMAN_CGR_FLAG_USE_INIT, &local_opts);
	else
		ret = qman_modify_cgr(cgr, 0, &local_opts);
	if (ret)
		goto release_lock;
add_list:
	list_add(&cgr->node, &p->cgr_cbs);

	/* Determine if newly added object requires its callback to be called */
	ret = qman_query_cgr(cgr, &cgr_state);
	if (ret) {
		/* we can't go back, so proceed and return success, but screen
		 * and wail to the log file.
		 */
		pr_crit("CGR HW state partially modified\n");
		ret = 0;
		goto release_lock;
	}
	if (cgr->cb && cgr_state.cgr.cscn_en && qman_cgrs_get(&p->cgrs[1],
							      cgr->cgrid))
		cgr->cb(p, cgr, 1);
release_lock:
	spin_unlock(&p->cgr_lock);
	return ret;
}

int qman_create_cgr_to_dcp(struct qman_cgr *cgr, u32 flags, u16 dcp_portal,
			   struct qm_mcc_initcgr *opts)
{
	struct qm_mcc_initcgr local_opts;
	struct qm_mcr_querycgr cgr_state;
	int ret;

	if ((qman_ip_rev & 0xFF00) < QMAN_REV30) {
		pr_warn("QMan version doesn't support CSCN => DCP portal\n");
		return -EINVAL;
	}
	/* We have to check that the provided CGRID is within the limits of the
	 * data-structures, for obvious reasons. However we'll let h/w take
	 * care of determining whether it's within the limits of what exists on
	 * the SoC.
	 */
	if (cgr->cgrid >= __CGR_NUM)
		return -EINVAL;

	ret = qman_query_cgr(cgr, &cgr_state);
	if (ret)
		return ret;

	memset(&local_opts, 0, sizeof(struct qm_mcc_initcgr));
	if (opts)
		local_opts = *opts;

	if ((qman_ip_rev & 0xFF00) >= QMAN_REV30)
		local_opts.cgr.cscn_targ_upd_ctrl =
				QM_CGR_TARG_UDP_CTRL_WRITE_BIT |
				QM_CGR_TARG_UDP_CTRL_DCP | dcp_portal;
	else
		local_opts.cgr.cscn_targ = cgr_state.cgr.cscn_targ |
					TARG_DCP_MASK(dcp_portal);
	local_opts.we_mask |= QM_CGR_WE_CSCN_TARG;

	/* send init if flags indicate so */
	if (opts && (flags & QMAN_CGR_FLAG_USE_INIT))
		ret = qman_modify_cgr(cgr, QMAN_CGR_FLAG_USE_INIT,
				      &local_opts);
	else
		ret = qman_modify_cgr(cgr, 0, &local_opts);

	return ret;
}

int qman_delete_cgr(struct qman_cgr *cgr)
{
	struct qm_mcr_querycgr cgr_state;
	struct qm_mcc_initcgr local_opts;
	int ret = 0;
	struct qman_cgr *i;
	struct qman_portal *p = get_affine_portal();

	if (cgr->chan != p->config->channel) {
		pr_crit("Attempting to delete cgr from different portal than"
			" it was create: create 0x%x, delete 0x%x\n",
			cgr->chan, p->config->channel);
		ret = -EINVAL;
		goto put_portal;
	}
	memset(&local_opts, 0, sizeof(struct qm_mcc_initcgr));
	spin_lock(&p->cgr_lock);
	list_del(&cgr->node);
	/*
	 * If there are no other CGR objects for this CGRID in the list,
	 * update CSCN_TARG accordingly
	 */
	list_for_each_entry(i, &p->cgr_cbs, node)
		if ((i->cgrid == cgr->cgrid) && i->cb)
			goto release_lock;
	ret = qman_query_cgr(cgr, &cgr_state);
	if (ret)  {
		/* add back to the list */
		list_add(&cgr->node, &p->cgr_cbs);
		goto release_lock;
	}
	/* Overwrite TARG */
	local_opts.we_mask = QM_CGR_WE_CSCN_TARG;
	if ((qman_ip_rev & 0xFF00) >= QMAN_REV30)
		local_opts.cgr.cscn_targ_upd_ctrl = PORTAL_IDX(p);
	else
		local_opts.cgr.cscn_targ = cgr_state.cgr.cscn_targ &
							 ~(TARG_MASK(p));
	ret = qman_modify_cgr(cgr, 0, &local_opts);
	if (ret)
		/* add back to the list */
		list_add(&cgr->node, &p->cgr_cbs);
release_lock:
	spin_unlock(&p->cgr_lock);
put_portal:
	return ret;
}

int qman_shutdown_fq(u32 fqid)
{
	struct qman_portal *p;
	struct qm_portal *low_p;
	struct qm_mc_command *mcc;
	struct qm_mc_result *mcr;
	u8 state;
	int orl_empty, fq_empty, drain = 0;
	u32 result;
	u32 channel, wq;
	u16 dest_wq;

	p = get_affine_portal();
	low_p = &p->p;

	/* Determine the state of the FQID */
	mcc = qm_mc_start(low_p);
	mcc->queryfq_np.fqid = cpu_to_be32(fqid);
	qm_mc_commit(low_p, QM_MCC_VERB_QUERYFQ_NP);
	while (!(mcr = qm_mc_result(low_p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_QUERYFQ_NP);
	state = mcr->queryfq_np.state & QM_MCR_NP_STATE_MASK;
	if (state == QM_MCR_NP_STATE_OOS)
		return 0; /* Already OOS, no need to do anymore checks */

	/* Query which channel the FQ is using */
	mcc = qm_mc_start(low_p);
	mcc->queryfq.fqid = cpu_to_be32(fqid);
	qm_mc_commit(low_p, QM_MCC_VERB_QUERYFQ);
	while (!(mcr = qm_mc_result(low_p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) == QM_MCR_VERB_QUERYFQ);

	/* Need to store these since the MCR gets reused */
	dest_wq = be16_to_cpu(mcr->queryfq.fqd.dest_wq);
	channel = dest_wq & 0x7;
	wq = dest_wq >> 3;

	switch (state) {
	case QM_MCR_NP_STATE_TEN_SCHED:
	case QM_MCR_NP_STATE_TRU_SCHED:
	case QM_MCR_NP_STATE_ACTIVE:
	case QM_MCR_NP_STATE_PARKED:
		orl_empty = 0;
		mcc = qm_mc_start(low_p);
		mcc->alterfq.fqid = cpu_to_be32(fqid);
		qm_mc_commit(low_p, QM_MCC_VERB_ALTER_RETIRE);
		while (!(mcr = qm_mc_result(low_p)))
			cpu_relax();
		DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) ==
			   QM_MCR_VERB_ALTER_RETIRE);
		result = mcr->result; /* Make a copy as we reuse MCR below */

		if (result == QM_MCR_RESULT_PENDING) {
			/* Need to wait for the FQRN in the message ring, which
			 * will only occur once the FQ has been drained.  In
			 * order for the FQ to drain the portal needs to be set
			 * to dequeue from the channel the FQ is scheduled on
			 */
			const struct qm_mr_entry *msg;
			const struct qm_dqrr_entry *dqrr = NULL;
			int found_fqrn = 0;
			__maybe_unused u16 dequeue_wq = 0;

			/* Flag that we need to drain FQ */
			drain = 1;

			if (channel >= qm_channel_pool1 &&
			    channel < (u16)(qm_channel_pool1 + 15)) {
				/* Pool channel, enable the bit in the portal */
				dequeue_wq = (channel -
					      qm_channel_pool1 + 1) << 4 | wq;
			} else if (channel < qm_channel_pool1) {
				/* Dedicated channel */
				dequeue_wq = wq;
			} else {
				pr_info("Cannot recover FQ 0x%x,"
					" it is scheduled on channel 0x%x",
					fqid, channel);
				return -EBUSY;
			}
			/* Set the sdqcr to drain this channel */
			if (channel < qm_channel_pool1)
				qm_dqrr_sdqcr_set(low_p,
						  QM_SDQCR_TYPE_ACTIVE |
					  QM_SDQCR_CHANNELS_DEDICATED);
			else
				qm_dqrr_sdqcr_set(low_p,
						  QM_SDQCR_TYPE_ACTIVE |
						  QM_SDQCR_CHANNELS_POOL_CONV
						  (channel));
			while (!found_fqrn) {
				/* Keep draining DQRR while checking the MR*/
				qm_dqrr_pvb_update(low_p);
				dqrr = qm_dqrr_current(low_p);
				while (dqrr) {
					qm_dqrr_cdc_consume_1ptr(
						low_p, dqrr, 0);
					qm_dqrr_pvb_update(low_p);
					qm_dqrr_next(low_p);
					dqrr = qm_dqrr_current(low_p);
				}
				/* Process message ring too */
				qm_mr_pvb_update(low_p);
				msg = qm_mr_current(low_p);
				while (msg) {
					if ((msg->ern.verb &
					     QM_MR_VERB_TYPE_MASK)
					    == QM_MR_VERB_FQRN)
						found_fqrn = 1;
					qm_mr_next(low_p);
					qm_mr_cci_consume_to_current(low_p);
					qm_mr_pvb_update(low_p);
					msg = qm_mr_current(low_p);
				}
				cpu_relax();
			}
		}
		if (result != QM_MCR_RESULT_OK &&
		    result !=  QM_MCR_RESULT_PENDING) {
			/* error */
			pr_err("qman_retire_fq failed on FQ 0x%x,"
			       " result=0x%x\n", fqid, result);
			return -1;
		}
		if (!(mcr->alterfq.fqs & QM_MCR_FQS_ORLPRESENT)) {
			/* ORL had no entries, no need to wait until the
			 * ERNs come in.
			 */
			orl_empty = 1;
		}
		/* Retirement succeeded, check to see if FQ needs
		 * to be drained.
		 */
		if (drain || mcr->alterfq.fqs & QM_MCR_FQS_NOTEMPTY) {
			/* FQ is Not Empty, drain using volatile DQ commands */
			fq_empty = 0;
			do {
				const struct qm_dqrr_entry *dqrr = NULL;
				u32 vdqcr = fqid | QM_VDQCR_NUMFRAMES_SET(3);

				qm_dqrr_vdqcr_set(low_p, vdqcr);

				/* Wait for a dequeue to occur */
				while (dqrr == NULL) {
					qm_dqrr_pvb_update(low_p);
					dqrr = qm_dqrr_current(low_p);
					if (!dqrr)
						cpu_relax();
				}
				/* Process the dequeues, making sure to
				 * empty the ring completely.
				 */
				while (dqrr) {
					if (dqrr->fqid == fqid &&
					    dqrr->stat & QM_DQRR_STAT_FQ_EMPTY)
						fq_empty = 1;
					qm_dqrr_cdc_consume_1ptr(low_p,
								 dqrr, 0);
					qm_dqrr_pvb_update(low_p);
					qm_dqrr_next(low_p);
					dqrr = qm_dqrr_current(low_p);
				}
			} while (fq_empty == 0);
		}
		qm_dqrr_sdqcr_set(low_p, 0);

		/* Wait for the ORL to have been completely drained */
		while (orl_empty == 0) {
			const struct qm_mr_entry *msg;

			qm_mr_pvb_update(low_p);
			msg = qm_mr_current(low_p);
			while (msg) {
				if ((msg->ern.verb & QM_MR_VERB_TYPE_MASK) ==
				    QM_MR_VERB_FQRL)
					orl_empty = 1;
				qm_mr_next(low_p);
				qm_mr_cci_consume_to_current(low_p);
				qm_mr_pvb_update(low_p);
				msg = qm_mr_current(low_p);
			}
			cpu_relax();
		}
		mcc = qm_mc_start(low_p);
		mcc->alterfq.fqid = cpu_to_be32(fqid);
		qm_mc_commit(low_p, QM_MCC_VERB_ALTER_OOS);
		while (!(mcr = qm_mc_result(low_p)))
			cpu_relax();
		DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) ==
			   QM_MCR_VERB_ALTER_OOS);
		if (mcr->result != QM_MCR_RESULT_OK) {
			pr_err(
			"OOS after drain Failed on FQID 0x%x, result 0x%x\n",
			       fqid, mcr->result);
			return -1;
		}
		return 0;

	case QM_MCR_NP_STATE_RETIRED:
		/* Send OOS Command */
		mcc = qm_mc_start(low_p);
		mcc->alterfq.fqid = cpu_to_be32(fqid);
		qm_mc_commit(low_p, QM_MCC_VERB_ALTER_OOS);
		while (!(mcr = qm_mc_result(low_p)))
			cpu_relax();
		DPAA_ASSERT((mcr->verb & QM_MCR_VERB_MASK) ==
			   QM_MCR_VERB_ALTER_OOS);
		if (mcr->result) {
			pr_err("OOS Failed on FQID 0x%x\n", fqid);
			return -1;
		}
		return 0;

	}
	return -1;
}
