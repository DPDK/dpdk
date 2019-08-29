/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017,2019 NXP
 *
 */

#ifndef __QMAN_PRIV_H
#define __QMAN_PRIV_H

#include "dpaa_sys.h"
#include <fsl_qman.h>

/* Congestion Groups */
/*
 * This wrapper represents a bit-array for the state of the 256 QMan congestion
 * groups. Is also used as a *mask* for congestion groups, eg. so we ignore
 * those that don't concern us. We harness the structure and accessor details
 * already used in the management command to query congestion groups.
 */
struct qman_cgrs {
	struct __qm_mcr_querycongestion q;
};

static inline void qman_cgrs_init(struct qman_cgrs *c)
{
	memset(c, 0, sizeof(*c));
}

static inline void qman_cgrs_fill(struct qman_cgrs *c)
{
	memset(c, 0xff, sizeof(*c));
}

static inline int qman_cgrs_get(struct qman_cgrs *c, int num)
{
	return QM_MCR_QUERYCONGESTION(&c->q, num);
}

static inline void qman_cgrs_set(struct qman_cgrs *c, int num)
{
	c->q.state[__CGR_WORD(num)] |= (0x80000000 >> __CGR_SHIFT(num));
}

static inline void qman_cgrs_unset(struct qman_cgrs *c, int num)
{
	c->q.state[__CGR_WORD(num)] &= ~(0x80000000 >> __CGR_SHIFT(num));
}

static inline int qman_cgrs_next(struct qman_cgrs *c, int num)
{
	while ((++num < (int)__CGR_NUM) && !qman_cgrs_get(c, num))
		;
	return num;
}

static inline void qman_cgrs_cp(struct qman_cgrs *dest,
				const struct qman_cgrs *src)
{
	memcpy(dest, src, sizeof(*dest));
}

static inline void qman_cgrs_and(struct qman_cgrs *dest,
				 const struct qman_cgrs *a,
				 const struct qman_cgrs *b)
{
	int ret;
	u32 *_d = dest->q.state;
	const u32 *_a = a->q.state;
	const u32 *_b = b->q.state;

	for (ret = 0; ret < 8; ret++)
		*(_d++) = *(_a++) & *(_b++);
}

static inline void qman_cgrs_xor(struct qman_cgrs *dest,
				 const struct qman_cgrs *a,
				 const struct qman_cgrs *b)
{
	int ret;
	u32 *_d = dest->q.state;
	const u32 *_a = a->q.state;
	const u32 *_b = b->q.state;

	for (ret = 0; ret < 8; ret++)
		*(_d++) = *(_a++) ^ *(_b++);
}

/* used by CCSR and portal interrupt code */
enum qm_isr_reg {
	qm_isr_status = 0,
	qm_isr_enable = 1,
	qm_isr_disable = 2,
	qm_isr_inhibit = 3
};

struct qm_portal_config {
	/*
	 * Corenet portal addresses;
	 * [0]==cache-enabled, [1]==cache-inhibited.
	 */
	void __iomem *addr_virt[2];
	struct device_node *node;
	/* Allow these to be joined in lists */
	struct list_head list;
	/* User-visible portal configuration settings */
	/* If the caller enables DQRR stashing (and thus wishes to operate the
	 * portal from only one cpu), this is the logical CPU that the portal
	 * will stash to. Whether stashing is enabled or not, this setting is
	 * also used for any "core-affine" portals, ie. default portals
	 * associated to the corresponding cpu. -1 implies that there is no
	 * core affinity configured.
	 */
	int cpu;
	/* portal interrupt line */
	int irq;
	/* the unique index of this portal */
	u32 index;
	/* Is this portal shared? (If so, it has coarser locking and demuxes
	 * processing on behalf of other CPUs.).
	 */
	int is_shared;
	/* The portal's dedicated channel id, use this value for initialising
	 * frame queues to target this portal when scheduled.
	 */
	u16 channel;
	/* A mask of which pool channels this portal has dequeue access to
	 * (using QM_SDQCR_CHANNELS_POOL(n) for the bitmask).
	 */
	u32 pools;

};

/* Revision info (for errata and feature handling) */
#define QMAN_REV11 0x0101
#define QMAN_REV12 0x0102
#define QMAN_REV20 0x0200
#define QMAN_REV30 0x0300
#define QMAN_REV31 0x0301
#define QMAN_REV32 0x0302
extern u16 qman_ip_rev; /* 0 if uninitialised, otherwise QMAN_REVx */

int qm_set_wpm(int wpm);
int qm_get_wpm(int *wpm);

struct qman_portal *qman_create_affine_portal(
			const struct qm_portal_config *config,
			const struct qman_cgrs *cgrs);
const struct qm_portal_config *
qman_destroy_affine_portal(struct qman_portal *q);

struct qman_portal *
qman_init_portal(struct qman_portal *portal,
		   const struct qm_portal_config *c,
		   const struct qman_cgrs *cgrs);

struct qman_portal *qman_alloc_global_portal(struct qm_portal_config *q_pcfg);
int qman_free_global_portal(struct qman_portal *portal);

void qman_portal_uninhibit_isr(struct qman_portal *portal);

struct qm_portal_config *qm_get_unused_portal(void);
struct qm_portal_config *qm_get_unused_portal_idx(uint32_t idx);

void qm_put_unused_portal(struct qm_portal_config *pcfg);
void qm_set_liodns(struct qm_portal_config *pcfg);

/* This CGR feature is supported by h/w and required by unit-tests and the
 * debugfs hooks, so is implemented in the driver. However it allows an explicit
 * corruption of h/w fields by s/w that are usually incorruptible (because the
 * counters are usually maintained entirely within h/w). As such, we declare
 * this API internally.
 */
int qman_testwrite_cgr(struct qman_cgr *cgr, u64 i_bcnt,
		       struct qm_mcr_cgrtestwrite *result);

#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
/* If the fq object pointer is greater than the size of context_b field,
 * than a lookup table is required.
 */
int qman_setup_fq_lookup_table(size_t num_entries);
#endif

/*   QMan s/w corenet portal, low-level i/face	 */

/*
 * For Choose one SOURCE. Choose one COUNT. Choose one
 * dequeue TYPE. Choose TOKEN (8-bit).
 * If SOURCE == CHANNELS,
 *   Choose CHANNELS_DEDICATED and/or CHANNELS_POOL(n).
 *   You can choose DEDICATED_PRECEDENCE if the portal channel should have
 *   priority.
 * If SOURCE == SPECIFICWQ,
 *     Either select the work-queue ID with SPECIFICWQ_WQ(), or select the
 *     channel (SPECIFICWQ_DEDICATED or SPECIFICWQ_POOL()) and specify the
 *     work-queue priority (0-7) with SPECIFICWQ_WQ() - either way, you get the
 *     same value.
 */
#define QM_SDQCR_SOURCE_CHANNELS	0x0
#define QM_SDQCR_SOURCE_SPECIFICWQ	0x40000000
#define QM_SDQCR_COUNT_EXACT1		0x0
#define QM_SDQCR_COUNT_UPTO3		0x20000000
#define QM_SDQCR_DEDICATED_PRECEDENCE	0x10000000
#define QM_SDQCR_TYPE_MASK		0x03000000
#define QM_SDQCR_TYPE_NULL		0x0
#define QM_SDQCR_TYPE_PRIO_QOS		0x01000000
#define QM_SDQCR_TYPE_ACTIVE_QOS	0x02000000
#define QM_SDQCR_TYPE_ACTIVE		0x03000000
#define QM_SDQCR_TOKEN_MASK		0x00ff0000
#define QM_SDQCR_TOKEN_SET(v)		(((v) & 0xff) << 16)
#define QM_SDQCR_TOKEN_GET(v)		(((v) >> 16) & 0xff)
#define QM_SDQCR_CHANNELS_DEDICATED	0x00008000
#define QM_SDQCR_SPECIFICWQ_MASK	0x000000f7
#define QM_SDQCR_SPECIFICWQ_DEDICATED	0x00000000
#define QM_SDQCR_SPECIFICWQ_POOL(n)	((n) << 4)
#define QM_SDQCR_SPECIFICWQ_WQ(n)	(n)

#define QM_VDQCR_FQID_MASK		0x00ffffff
#define QM_VDQCR_FQID(n)		((n) & QM_VDQCR_FQID_MASK)

#define QM_EQCR_VERB_VBIT		0x80
#define QM_EQCR_VERB_CMD_MASK		0x61	/* but only one value; */
#define QM_EQCR_VERB_CMD_ENQUEUE	0x01
#define QM_EQCR_VERB_COLOUR_MASK	0x18	/* 4 possible values; */
#define QM_EQCR_VERB_COLOUR_GREEN	0x00
#define QM_EQCR_VERB_COLOUR_YELLOW	0x08
#define QM_EQCR_VERB_COLOUR_RED		0x10
#define QM_EQCR_VERB_COLOUR_OVERRIDE	0x18
#define QM_EQCR_VERB_INTERRUPT		0x04	/* on command consumption */
#define QM_EQCR_VERB_ORP		0x02	/* enable order restoration */
#define QM_EQCR_DCA_ENABLE		0x80
#define QM_EQCR_DCA_PARK		0x40
#define QM_EQCR_DCA_IDXMASK		0x0f	/* "DQRR::idx" goes here */
#define QM_EQCR_SEQNUM_NESN		0x8000	/* Advance NESN */
#define QM_EQCR_SEQNUM_NLIS		0x4000	/* More fragments to come */
#define QM_EQCR_SEQNUM_SEQMASK		0x3fff	/* sequence number goes here */
#define QM_EQCR_FQID_NULL		0	/* eg. for an ORP seqnum hole */

#define QM_MCC_VERB_VBIT		0x80
#define QM_MCC_VERB_MASK		0x7f	/* where the verb contains; */
#define QM_MCC_VERB_INITFQ_PARKED	0x40
#define QM_MCC_VERB_INITFQ_SCHED	0x41
#define QM_MCC_VERB_QUERYFQ		0x44
#define QM_MCC_VERB_QUERYFQ_NP		0x45	/* "non-programmable" fields */
#define QM_MCC_VERB_QUERYWQ		0x46
#define QM_MCC_VERB_QUERYWQ_DEDICATED	0x47
#define QM_MCC_VERB_ALTER_SCHED		0x48	/* Schedule FQ */
#define QM_MCC_VERB_ALTER_FE		0x49	/* Force Eligible FQ */
#define QM_MCC_VERB_ALTER_RETIRE	0x4a	/* Retire FQ */
#define QM_MCC_VERB_ALTER_OOS		0x4b	/* Take FQ out of service */
#define QM_MCC_VERB_ALTER_FQXON		0x4d	/* FQ XON */
#define QM_MCC_VERB_ALTER_FQXOFF	0x4e	/* FQ XOFF */
#define QM_MCC_VERB_INITCGR		0x50
#define QM_MCC_VERB_MODIFYCGR		0x51
#define QM_MCC_VERB_CGRTESTWRITE	0x52
#define QM_MCC_VERB_QUERYCGR		0x58
#define QM_MCC_VERB_QUERYCONGESTION	0x59

/*
 * Used by all portal interrupt registers except 'inhibit'
 * Channels with frame availability
 */
#define QM_PIRQ_DQAVAIL	0x0000ffff

/* The DQAVAIL interrupt fields break down into these bits; */
#define QM_DQAVAIL_PORTAL	0x8000		/* Portal channel */
#define QM_DQAVAIL_POOL(n)	(0x8000 >> (n))	/* Pool channel, n==[1..15] */
#define QM_DQAVAIL_MASK		0xffff
/* This mask contains all the "irqsource" bits visible to API users */
#define QM_PIRQ_VISIBLE	(QM_PIRQ_SLOW | QM_PIRQ_DQRI)

/* These are qm_<reg>_<verb>(). So for example, qm_disable_write() means "write
 * the disable register" rather than "disable the ability to write".
 */
#define qm_isr_status_read(qm)		__qm_isr_read(qm, qm_isr_status)
#define qm_isr_status_clear(qm, m)	__qm_isr_write(qm, qm_isr_status, m)
#define qm_isr_enable_read(qm)		__qm_isr_read(qm, qm_isr_enable)
#define qm_isr_enable_write(qm, v)	__qm_isr_write(qm, qm_isr_enable, v)
#define qm_isr_disable_read(qm)		__qm_isr_read(qm, qm_isr_disable)
#define qm_isr_disable_write(qm, v)	__qm_isr_write(qm, qm_isr_disable, v)
/* TODO: unfortunate name-clash here, reword? */
#define qm_isr_inhibit(qm)		__qm_isr_write(qm, qm_isr_inhibit, 1)
#define qm_isr_uninhibit(qm)		__qm_isr_write(qm, qm_isr_inhibit, 0)

#define QMAN_PORTAL_IRQ_PATH "/dev/fsl-usdpaa-irq"

#endif /* _QMAN_PRIV_H */
