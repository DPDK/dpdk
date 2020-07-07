/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 * Copyright 2018-2020 NXP
 *
 */

#include "qbman_sys.h"
#include "qbman_portal.h"

/* QBMan portal management command codes */
#define QBMAN_MC_ACQUIRE       0x30
#define QBMAN_WQCHAN_CONFIGURE 0x46

/* Reverse mapping of QBMAN_CENA_SWP_DQRR() */
#define QBMAN_IDX_FROM_DQRR(p) (((unsigned long)p & 0x1ff) >> 6)

/* QBMan FQ management command codes */
#define QBMAN_FQ_SCHEDULE	0x48
#define QBMAN_FQ_FORCE		0x49
#define QBMAN_FQ_XON		0x4d
#define QBMAN_FQ_XOFF		0x4e

/*******************************/
/* Pre-defined attribute codes */
/*******************************/

#define QBMAN_RESPONSE_VERB_MASK   0x7f

/*************************/
/* SDQCR attribute codes */
/*************************/
#define QB_SDQCR_FC_SHIFT   29
#define QB_SDQCR_FC_MASK    0x1
#define QB_SDQCR_DCT_SHIFT  24
#define QB_SDQCR_DCT_MASK   0x3
#define QB_SDQCR_TOK_SHIFT  16
#define QB_SDQCR_TOK_MASK   0xff
#define QB_SDQCR_SRC_SHIFT  0
#define QB_SDQCR_SRC_MASK   0xffff

/* opaque token for static dequeues */
#define QMAN_SDQCR_TOKEN    0xbb

enum qbman_sdqcr_dct {
	qbman_sdqcr_dct_null = 0,
	qbman_sdqcr_dct_prio_ics,
	qbman_sdqcr_dct_active_ics,
	qbman_sdqcr_dct_active
};

enum qbman_sdqcr_fc {
	qbman_sdqcr_fc_one = 0,
	qbman_sdqcr_fc_up_to_3 = 1
};

/* We need to keep track of which SWP triggered a pull command
 * so keep an array of portal IDs and use the token field to
 * be able to find the proper portal
 */
#define MAX_QBMAN_PORTALS  64
static struct qbman_swp *portal_idx_map[MAX_QBMAN_PORTALS];

uint32_t qman_version;

/* Internal Function declaration */
static int
qbman_swp_enqueue_array_mode_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd);
static int
qbman_swp_enqueue_array_mode_mem_back(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd);

static int
qbman_swp_enqueue_ring_mode_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd);
static int
qbman_swp_enqueue_ring_mode_cinh_read_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd);
static int
qbman_swp_enqueue_ring_mode_cinh_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd);
static int
qbman_swp_enqueue_ring_mode_mem_back(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd);

static int
qbman_swp_enqueue_multiple_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames);
static int
qbman_swp_enqueue_multiple_cinh_read_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames);
static int
qbman_swp_enqueue_multiple_cinh_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames);
static int
qbman_swp_enqueue_multiple_mem_back(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames);

static int
qbman_swp_enqueue_multiple_fd_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames);
static int
qbman_swp_enqueue_multiple_fd_cinh_read_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames);
static int
qbman_swp_enqueue_multiple_fd_cinh_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames);
static int
qbman_swp_enqueue_multiple_fd_mem_back(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames);

static int
qbman_swp_enqueue_multiple_desc_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames);
static int
qbman_swp_enqueue_multiple_desc_cinh_read_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames);
static int
qbman_swp_enqueue_multiple_desc_cinh_direct(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames);
static int
qbman_swp_enqueue_multiple_desc_mem_back(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames);

static int
qbman_swp_pull_direct(struct qbman_swp *s, struct qbman_pull_desc *d);
static int
qbman_swp_pull_cinh_direct(struct qbman_swp *s, struct qbman_pull_desc *d);
static int
qbman_swp_pull_mem_back(struct qbman_swp *s, struct qbman_pull_desc *d);

const struct qbman_result *qbman_swp_dqrr_next_direct(struct qbman_swp *s);
const struct qbman_result *qbman_swp_dqrr_next_cinh_direct(struct qbman_swp *s);
const struct qbman_result *qbman_swp_dqrr_next_mem_back(struct qbman_swp *s);

static int
qbman_swp_release_direct(struct qbman_swp *s,
		const struct qbman_release_desc *d,
		const uint64_t *buffers, unsigned int num_buffers);
static int
qbman_swp_release_cinh_direct(struct qbman_swp *s,
		const struct qbman_release_desc *d,
		const uint64_t *buffers, unsigned int num_buffers);
static int
qbman_swp_release_mem_back(struct qbman_swp *s,
		const struct qbman_release_desc *d,
		const uint64_t *buffers, unsigned int num_buffers);

/* Function pointers */
static int (*qbman_swp_enqueue_array_mode_ptr)(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd)
	= qbman_swp_enqueue_array_mode_direct;

static int (*qbman_swp_enqueue_ring_mode_ptr)(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd)
	= qbman_swp_enqueue_ring_mode_direct;

static int (*qbman_swp_enqueue_multiple_ptr)(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames)
	= qbman_swp_enqueue_multiple_direct;

static int (*qbman_swp_enqueue_multiple_fd_ptr)(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames)
	= qbman_swp_enqueue_multiple_fd_direct;

static int (*qbman_swp_enqueue_multiple_desc_ptr)(struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames)
	= qbman_swp_enqueue_multiple_desc_direct;

static int (*qbman_swp_pull_ptr)(struct qbman_swp *s,
		struct qbman_pull_desc *d)
	= qbman_swp_pull_direct;

const struct qbman_result *(*qbman_swp_dqrr_next_ptr)(struct qbman_swp *s)
		= qbman_swp_dqrr_next_direct;

static int (*qbman_swp_release_ptr)(struct qbman_swp *s,
			const struct qbman_release_desc *d,
			const uint64_t *buffers, unsigned int num_buffers)
			= qbman_swp_release_direct;

/*********************************/
/* Portal constructor/destructor */
/*********************************/

/* Software portals should always be in the power-on state when we initialise,
 * due to the CCSR-based portal reset functionality that MC has.
 *
 * Erk! Turns out that QMan versions prior to 4.1 do not correctly reset DQRR
 * valid-bits, so we need to support a workaround where we don't trust
 * valid-bits when detecting new entries until any stale ring entries have been
 * overwritten at least once. The idea is that we read PI for the first few
 * entries, then switch to valid-bit after that. The trick is to clear the
 * bug-work-around boolean once the PI wraps around the ring for the first time.
 *
 * Note: this still carries a slight additional cost once the decrementer hits
 * zero.
 */
struct qbman_swp *qbman_swp_init(const struct qbman_swp_desc *d)
{
	int ret;
	uint32_t eqcr_pi;
	uint32_t mask_size;
	struct qbman_swp *p = malloc(sizeof(*p));

	if (!p)
		return NULL;

	memset(p, 0, sizeof(struct qbman_swp));

	p->desc = *d;
#ifdef QBMAN_CHECKING
	p->mc.check = swp_mc_can_start;
#endif
	p->mc.valid_bit = QB_VALID_BIT;
	p->sdq |= qbman_sdqcr_dct_prio_ics << QB_SDQCR_DCT_SHIFT;
	p->sdq |= qbman_sdqcr_fc_up_to_3 << QB_SDQCR_FC_SHIFT;
	p->sdq |= QMAN_SDQCR_TOKEN << QB_SDQCR_TOK_SHIFT;
	if ((d->qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
			&& (d->cena_access_mode == qman_cena_fastest_access))
		p->mr.valid_bit = QB_VALID_BIT;

	atomic_set(&p->vdq.busy, 1);
	p->vdq.valid_bit = QB_VALID_BIT;
	p->dqrr.valid_bit = QB_VALID_BIT;
	qman_version = p->desc.qman_version;
	if ((qman_version & QMAN_REV_MASK) < QMAN_REV_4100) {
		p->dqrr.dqrr_size = 4;
		p->dqrr.reset_bug = 1;
	} else {
		p->dqrr.dqrr_size = 8;
		p->dqrr.reset_bug = 0;
	}

	ret = qbman_swp_sys_init(&p->sys, d, p->dqrr.dqrr_size);
	if (ret) {
		free(p);
		pr_err("qbman_swp_sys_init() failed %d\n", ret);
		return NULL;
	}

	/* Verify that the DQRRPI is 0 - if it is not the portal isn't
	 * in default state which is an error
	 */
	if (qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_DQPI) & 0xF) {
		pr_err("qbman DQRR PI is not zero, portal is not clean\n");
		free(p);
		return NULL;
	}

	/* SDQCR needs to be initialized to 0 when no channels are
	 * being dequeued from or else the QMan HW will indicate an
	 * error.  The values that were calculated above will be
	 * applied when dequeues from a specific channel are enabled.
	 */
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_SDQCR, 0);

	p->eqcr.pi_ring_size = 8;
	if ((qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
			&& (d->cena_access_mode == qman_cena_fastest_access)) {
		p->eqcr.pi_ring_size = 32;
		qbman_swp_enqueue_array_mode_ptr =
			qbman_swp_enqueue_array_mode_mem_back;
		qbman_swp_enqueue_ring_mode_ptr =
			qbman_swp_enqueue_ring_mode_mem_back;
		qbman_swp_enqueue_multiple_ptr =
			qbman_swp_enqueue_multiple_mem_back;
		qbman_swp_enqueue_multiple_fd_ptr =
			qbman_swp_enqueue_multiple_fd_mem_back;
		qbman_swp_enqueue_multiple_desc_ptr =
			qbman_swp_enqueue_multiple_desc_mem_back;
		qbman_swp_pull_ptr = qbman_swp_pull_mem_back;
		qbman_swp_dqrr_next_ptr = qbman_swp_dqrr_next_mem_back;
		qbman_swp_release_ptr = qbman_swp_release_mem_back;
	}

	if (dpaa2_svr_family == SVR_LS1080A) {
		qbman_swp_enqueue_ring_mode_ptr =
			qbman_swp_enqueue_ring_mode_cinh_read_direct;
		qbman_swp_enqueue_multiple_ptr =
			qbman_swp_enqueue_multiple_cinh_read_direct;
		qbman_swp_enqueue_multiple_fd_ptr =
			qbman_swp_enqueue_multiple_fd_cinh_read_direct;
		qbman_swp_enqueue_multiple_desc_ptr =
			qbman_swp_enqueue_multiple_desc_cinh_read_direct;
	}

	for (mask_size = p->eqcr.pi_ring_size; mask_size > 0; mask_size >>= 1)
		p->eqcr.pi_ci_mask = (p->eqcr.pi_ci_mask<<1) + 1;
	eqcr_pi = qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_EQCR_PI);
	p->eqcr.pi = eqcr_pi & p->eqcr.pi_ci_mask;
	p->eqcr.pi_vb = eqcr_pi & QB_VALID_BIT;
	if ((p->desc.qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
			&& (d->cena_access_mode == qman_cena_fastest_access))
		p->eqcr.ci = qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_EQCR_PI)
					     & p->eqcr.pi_ci_mask;
	else
		p->eqcr.ci = qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_EQCR_CI)
					     & p->eqcr.pi_ci_mask;
	p->eqcr.available = p->eqcr.pi_ring_size -
				qm_cyc_diff(p->eqcr.pi_ring_size,
				p->eqcr.ci & (p->eqcr.pi_ci_mask<<1),
				p->eqcr.pi & (p->eqcr.pi_ci_mask<<1));

	portal_idx_map[p->desc.idx] = p;
	return p;
}

int qbman_swp_update(struct qbman_swp *p, int stash_off)
{
	const struct qbman_swp_desc *d = &p->desc;
	struct qbman_swp_sys *s = &p->sys;
	int ret;

	/* Nothing needs to be done for QBMAN rev > 5000 with fast access */
	if ((qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
			&& (d->cena_access_mode == qman_cena_fastest_access))
		return 0;

	ret = qbman_swp_sys_update(s, d, p->dqrr.dqrr_size, stash_off);
	if (ret) {
		pr_err("qbman_swp_sys_init() failed %d\n", ret);
		return ret;
	}

	p->stash_off = stash_off;

	return 0;
}

void qbman_swp_finish(struct qbman_swp *p)
{
#ifdef QBMAN_CHECKING
	QBMAN_BUG_ON(p->mc.check != swp_mc_can_start);
#endif
	qbman_swp_sys_finish(&p->sys);
	portal_idx_map[p->desc.idx] = NULL;
	free(p);
}

const struct qbman_swp_desc *qbman_swp_get_desc(struct qbman_swp *p)
{
	return &p->desc;
}

/**************/
/* Interrupts */
/**************/

uint32_t qbman_swp_interrupt_get_vanish(struct qbman_swp *p)
{
	return qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_ISDR);
}

void qbman_swp_interrupt_set_vanish(struct qbman_swp *p, uint32_t mask)
{
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_ISDR, mask);
}

uint32_t qbman_swp_interrupt_read_status(struct qbman_swp *p)
{
	return qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_ISR);
}

void qbman_swp_interrupt_clear_status(struct qbman_swp *p, uint32_t mask)
{
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_ISR, mask);
}

uint32_t qbman_swp_dqrr_thrshld_read_status(struct qbman_swp *p)
{
	return qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_DQRR_ITR);
}

void qbman_swp_dqrr_thrshld_write(struct qbman_swp *p, uint32_t mask)
{
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_DQRR_ITR, mask);
}

uint32_t qbman_swp_intr_timeout_read_status(struct qbman_swp *p)
{
	return qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_ITPR);
}

void qbman_swp_intr_timeout_write(struct qbman_swp *p, uint32_t mask)
{
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_ITPR, mask);
}

uint32_t qbman_swp_interrupt_get_trigger(struct qbman_swp *p)
{
	return qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_IER);
}

void qbman_swp_interrupt_set_trigger(struct qbman_swp *p, uint32_t mask)
{
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_IER, mask);
}

int qbman_swp_interrupt_get_inhibit(struct qbman_swp *p)
{
	return qbman_cinh_read(&p->sys, QBMAN_CINH_SWP_IIR);
}

void qbman_swp_interrupt_set_inhibit(struct qbman_swp *p, int inhibit)
{
	qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_IIR,
			 inhibit ? 0xffffffff : 0);
}

/***********************/
/* Management commands */
/***********************/

/*
 * Internal code common to all types of management commands.
 */

void *qbman_swp_mc_start(struct qbman_swp *p)
{
	void *ret;
#ifdef QBMAN_CHECKING
	QBMAN_BUG_ON(p->mc.check != swp_mc_can_start);
#endif
	if ((p->desc.qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
		    && (p->desc.cena_access_mode == qman_cena_fastest_access))
		ret = qbman_cena_write_start(&p->sys, QBMAN_CENA_SWP_CR_MEM);
	else
		ret = qbman_cena_write_start(&p->sys, QBMAN_CENA_SWP_CR);
#ifdef QBMAN_CHECKING
	if (!ret)
		p->mc.check = swp_mc_can_submit;
#endif
	return ret;
}

void qbman_swp_mc_submit(struct qbman_swp *p, void *cmd, uint8_t cmd_verb)
{
	uint8_t *v = cmd;
#ifdef QBMAN_CHECKING
	QBMAN_BUG_ON(!(p->mc.check != swp_mc_can_submit));
#endif
	/* TBD: "|=" is going to hurt performance. Need to move as many fields
	 * out of word zero, and for those that remain, the "OR" needs to occur
	 * at the caller side. This debug check helps to catch cases where the
	 * caller wants to OR but has forgotten to do so.
	 */
	QBMAN_BUG_ON((*v & cmd_verb) != *v);
	if ((p->desc.qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
		    && (p->desc.cena_access_mode == qman_cena_fastest_access)) {
		*v = cmd_verb | p->mr.valid_bit;
		qbman_cena_write_complete(&p->sys, QBMAN_CENA_SWP_CR_MEM, cmd);
		dma_wmb();
		qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_CR_RT, QMAN_RT_MODE);
	} else {
		dma_wmb();
		*v = cmd_verb | p->mc.valid_bit;
		qbman_cena_write_complete(&p->sys, QBMAN_CENA_SWP_CR, cmd);
		clean(cmd);
	}
#ifdef QBMAN_CHECKING
	p->mc.check = swp_mc_can_poll;
#endif
}

void qbman_swp_mc_submit_cinh(struct qbman_swp *p, void *cmd, uint8_t cmd_verb)
{
	uint8_t *v = cmd;
#ifdef QBMAN_CHECKING
	QBMAN_BUG_ON(!(p->mc.check != swp_mc_can_submit));
#endif
	/* TBD: "|=" is going to hurt performance. Need to move as many fields
	 * out of word zero, and for those that remain, the "OR" needs to occur
	 * at the caller side. This debug check helps to catch cases where the
	 * caller wants to OR but has forgotten to do so.
	 */
	QBMAN_BUG_ON((*v & cmd_verb) != *v);
	dma_wmb();
	*v = cmd_verb | p->mc.valid_bit;
	qbman_cinh_write_complete(&p->sys, QBMAN_CENA_SWP_CR, cmd);
	clean(cmd);
#ifdef QBMAN_CHECKING
	p->mc.check = swp_mc_can_poll;
#endif
}

void *qbman_swp_mc_result(struct qbman_swp *p)
{
	uint32_t *ret, verb;
#ifdef QBMAN_CHECKING
	QBMAN_BUG_ON(p->mc.check != swp_mc_can_poll);
#endif
	if ((p->desc.qman_version & QMAN_REV_MASK) >= QMAN_REV_5000
		&& (p->desc.cena_access_mode == qman_cena_fastest_access)) {
		ret = qbman_cena_read(&p->sys, QBMAN_CENA_SWP_RR_MEM);
		/* Command completed if the valid bit is toggled */
		if (p->mr.valid_bit != (ret[0] & QB_VALID_BIT))
			return NULL;
		/* Remove the valid-bit -
		 * command completed iff the rest is non-zero
		 */
		verb = ret[0] & ~QB_VALID_BIT;
		if (!verb)
			return NULL;
		p->mr.valid_bit ^= QB_VALID_BIT;
	} else {
		qbman_cena_invalidate_prefetch(&p->sys,
			QBMAN_CENA_SWP_RR(p->mc.valid_bit));
		ret = qbman_cena_read(&p->sys,
				      QBMAN_CENA_SWP_RR(p->mc.valid_bit));
		/* Remove the valid-bit -
		 * command completed iff the rest is non-zero
		 */
		verb = ret[0] & ~QB_VALID_BIT;
		if (!verb)
			return NULL;
		p->mc.valid_bit ^= QB_VALID_BIT;
	}
#ifdef QBMAN_CHECKING
	p->mc.check = swp_mc_can_start;
#endif
	return ret;
}

void *qbman_swp_mc_result_cinh(struct qbman_swp *p)
{
	uint32_t *ret, verb;
#ifdef QBMAN_CHECKING
	QBMAN_BUG_ON(p->mc.check != swp_mc_can_poll);
#endif
	ret = qbman_cinh_read_shadow(&p->sys,
			      QBMAN_CENA_SWP_RR(p->mc.valid_bit));
	/* Remove the valid-bit -
	 * command completed iff the rest is non-zero
	 */
	verb = ret[0] & ~QB_VALID_BIT;
	if (!verb)
		return NULL;
	p->mc.valid_bit ^= QB_VALID_BIT;
#ifdef QBMAN_CHECKING
	p->mc.check = swp_mc_can_start;
#endif
	return ret;
}

/***********/
/* Enqueue */
/***********/

#define QB_ENQUEUE_CMD_OPTIONS_SHIFT    0
enum qb_enqueue_commands {
	enqueue_empty = 0,
	enqueue_response_always = 1,
	enqueue_rejects_to_fq = 2
};

#define QB_ENQUEUE_CMD_EC_OPTION_MASK        0x3
#define QB_ENQUEUE_CMD_ORP_ENABLE_SHIFT      2
#define QB_ENQUEUE_CMD_IRQ_ON_DISPATCH_SHIFT 3
#define QB_ENQUEUE_CMD_TARGET_TYPE_SHIFT     4
#define QB_ENQUEUE_CMD_DCA_PK_SHIFT          6
#define QB_ENQUEUE_CMD_DCA_EN_SHIFT          7
#define QB_ENQUEUE_CMD_NLIS_SHIFT            14
#define QB_ENQUEUE_CMD_IS_NESN_SHIFT         15

void qbman_eq_desc_clear(struct qbman_eq_desc *d)
{
	memset(d, 0, sizeof(*d));
}

void qbman_eq_desc_set_no_orp(struct qbman_eq_desc *d, int respond_success)
{
	d->eq.verb &= ~(1 << QB_ENQUEUE_CMD_ORP_ENABLE_SHIFT);
	if (respond_success)
		d->eq.verb |= enqueue_response_always;
	else
		d->eq.verb |= enqueue_rejects_to_fq;
}

void qbman_eq_desc_set_orp(struct qbman_eq_desc *d, int respond_success,
			   uint16_t opr_id, uint16_t seqnum, int incomplete)
{
	d->eq.verb |= 1 << QB_ENQUEUE_CMD_ORP_ENABLE_SHIFT;
	if (respond_success)
		d->eq.verb |= enqueue_response_always;
	else
		d->eq.verb |= enqueue_rejects_to_fq;

	d->eq.orpid = opr_id;
	d->eq.seqnum = seqnum;
	if (incomplete)
		d->eq.seqnum |= 1 << QB_ENQUEUE_CMD_NLIS_SHIFT;
	else
		d->eq.seqnum &= ~(1 << QB_ENQUEUE_CMD_NLIS_SHIFT);
}

void qbman_eq_desc_set_orp_hole(struct qbman_eq_desc *d, uint16_t opr_id,
				uint16_t seqnum)
{
	d->eq.verb |= 1 << QB_ENQUEUE_CMD_ORP_ENABLE_SHIFT;
	d->eq.verb &= ~QB_ENQUEUE_CMD_EC_OPTION_MASK;
	d->eq.orpid = opr_id;
	d->eq.seqnum = seqnum;
	d->eq.seqnum &= ~(1 << QB_ENQUEUE_CMD_NLIS_SHIFT);
	d->eq.seqnum &= ~(1 << QB_ENQUEUE_CMD_IS_NESN_SHIFT);
}

void qbman_eq_desc_set_orp_nesn(struct qbman_eq_desc *d, uint16_t opr_id,
				uint16_t seqnum)
{
	d->eq.verb |= 1 << QB_ENQUEUE_CMD_ORP_ENABLE_SHIFT;
	d->eq.verb &= ~QB_ENQUEUE_CMD_EC_OPTION_MASK;
	d->eq.orpid = opr_id;
	d->eq.seqnum = seqnum;
	d->eq.seqnum &= ~(1 << QB_ENQUEUE_CMD_NLIS_SHIFT);
	d->eq.seqnum |= 1 << QB_ENQUEUE_CMD_IS_NESN_SHIFT;
}

void qbman_eq_desc_set_response(struct qbman_eq_desc *d,
				dma_addr_t storage_phys,
				int stash)
{
	d->eq.rsp_addr = storage_phys;
	d->eq.wae = stash;
}

void qbman_eq_desc_set_token(struct qbman_eq_desc *d, uint8_t token)
{
	d->eq.rspid = token;
}

void qbman_eq_desc_set_fq(struct qbman_eq_desc *d, uint32_t fqid)
{
	d->eq.verb &= ~(1 << QB_ENQUEUE_CMD_TARGET_TYPE_SHIFT);
	d->eq.tgtid = fqid;
}

void qbman_eq_desc_set_qd(struct qbman_eq_desc *d, uint32_t qdid,
			  uint16_t qd_bin, uint8_t qd_prio)
{
	d->eq.verb |= 1 << QB_ENQUEUE_CMD_TARGET_TYPE_SHIFT;
	d->eq.tgtid = qdid;
	d->eq.qdbin = qd_bin;
	d->eq.qpri = qd_prio;
}

void qbman_eq_desc_set_eqdi(struct qbman_eq_desc *d, int enable)
{
	if (enable)
		d->eq.verb |= 1 << QB_ENQUEUE_CMD_IRQ_ON_DISPATCH_SHIFT;
	else
		d->eq.verb &= ~(1 << QB_ENQUEUE_CMD_IRQ_ON_DISPATCH_SHIFT);
}

void qbman_eq_desc_set_dca(struct qbman_eq_desc *d, int enable,
			   uint8_t dqrr_idx, int park)
{
	if (enable) {
		d->eq.dca = dqrr_idx;
		if (park)
			d->eq.dca |= 1 << QB_ENQUEUE_CMD_DCA_PK_SHIFT;
		else
			d->eq.dca &= ~(1 << QB_ENQUEUE_CMD_DCA_PK_SHIFT);
		d->eq.dca |= 1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT;
	} else {
		d->eq.dca &= ~(1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT);
	}
}

#define EQAR_IDX(eqar)     ((eqar) & 0x1f)
#define EQAR_VB(eqar)      ((eqar) & 0x80)
#define EQAR_SUCCESS(eqar) ((eqar) & 0x100)

static inline void qbman_write_eqcr_am_rt_register(struct qbman_swp *p,
						   uint8_t idx)
{
	if (idx < 16)
		qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_EQCR_AM_RT + idx * 4,
				     QMAN_RT_MODE);
	else
		qbman_cinh_write(&p->sys, QBMAN_CINH_SWP_EQCR_AM_RT2 +
				     (idx - 16) * 4,
				     QMAN_RT_MODE);
}

static void memcpy_byte_by_byte(void *to, const void *from, size_t n)
{
	const uint8_t *src = from;
	volatile uint8_t *dest = to;
	size_t i;

	for (i = 0; i < n; i++)
		dest[i] = src[i];
}


static int qbman_swp_enqueue_array_mode_direct(struct qbman_swp *s,
					       const struct qbman_eq_desc *d,
					       const struct qbman_fd *fd)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqar = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_EQAR);

	pr_debug("EQAR=%08x\n", eqar);
	if (!EQAR_SUCCESS(eqar))
		return -EBUSY;
	p = qbman_cena_write_start_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(EQAR_IDX(eqar)));
	memcpy(&p[1], &cl[1], 28);
	memcpy(&p[8], fd, sizeof(*fd));

	/* Set the verb byte, have to substitute in the valid-bit */
	dma_wmb();
	p[0] = cl[0] | EQAR_VB(eqar);
	qbman_cena_write_complete_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(EQAR_IDX(eqar)));
	return 0;
}
static int qbman_swp_enqueue_array_mode_mem_back(struct qbman_swp *s,
						 const struct qbman_eq_desc *d,
						 const struct qbman_fd *fd)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqar = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_EQAR);

	pr_debug("EQAR=%08x\n", eqar);
	if (!EQAR_SUCCESS(eqar))
		return -EBUSY;
	p = qbman_cena_write_start_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(EQAR_IDX(eqar)));
	memcpy(&p[1], &cl[1], 28);
	memcpy(&p[8], fd, sizeof(*fd));

	/* Set the verb byte, have to substitute in the valid-bit */
	p[0] = cl[0] | EQAR_VB(eqar);
	dma_wmb();
	qbman_write_eqcr_am_rt_register(s, EQAR_IDX(eqar));
	return 0;
}

static inline int qbman_swp_enqueue_array_mode(struct qbman_swp *s,
					       const struct qbman_eq_desc *d,
					       const struct qbman_fd *fd)
{
	return qbman_swp_enqueue_array_mode_ptr(s, d, fd);
}

static int qbman_swp_enqueue_ring_mode_direct(struct qbman_swp *s,
					      const struct qbman_eq_desc *d,
					      const struct qbman_fd *fd)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, full_mask, half_mask;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return -EBUSY;
	}

	p = qbman_cena_write_start_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(s->eqcr.pi & half_mask));
	memcpy(&p[1], &cl[1], 28);
	memcpy(&p[8], fd, sizeof(*fd));
	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	p[0] = cl[0] | s->eqcr.pi_vb;
	qbman_cena_write_complete_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(s->eqcr.pi & half_mask));
	s->eqcr.pi++;
	s->eqcr.pi &= full_mask;
	s->eqcr.available--;
	if (!(s->eqcr.pi & half_mask))
		s->eqcr.pi_vb ^= QB_VALID_BIT;

	return 0;
}

static int qbman_swp_enqueue_ring_mode_cinh_read_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, full_mask, half_mask;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return -EBUSY;
	}

	p = qbman_cinh_write_start_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(s->eqcr.pi & half_mask));
	memcpy(&p[1], &cl[1], 28);
	memcpy(&p[8], fd, sizeof(*fd));
	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	p[0] = cl[0] | s->eqcr.pi_vb;
	s->eqcr.pi++;
	s->eqcr.pi &= full_mask;
	s->eqcr.available--;
	if (!(s->eqcr.pi & half_mask))
		s->eqcr.pi_vb ^= QB_VALID_BIT;

	return 0;
}

static int qbman_swp_enqueue_ring_mode_cinh_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, full_mask, half_mask;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return -EBUSY;
	}

	p = qbman_cinh_write_start_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(s->eqcr.pi & half_mask));
	memcpy_byte_by_byte(&p[1], &cl[1], 28);
	memcpy_byte_by_byte(&p[8], fd, sizeof(*fd));
	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	p[0] = cl[0] | s->eqcr.pi_vb;
	s->eqcr.pi++;
	s->eqcr.pi &= full_mask;
	s->eqcr.available--;
	if (!(s->eqcr.pi & half_mask))
		s->eqcr.pi_vb ^= QB_VALID_BIT;

	return 0;
}

static int qbman_swp_enqueue_ring_mode_mem_back(struct qbman_swp *s,
						const struct qbman_eq_desc *d,
						const struct qbman_fd *fd)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, full_mask, half_mask;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI_MEMBACK) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return -EBUSY;
	}

	p = qbman_cena_write_start_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_EQCR(s->eqcr.pi & half_mask));
	memcpy(&p[1], &cl[1], 28);
	memcpy(&p[8], fd, sizeof(*fd));

	/* Set the verb byte, have to substitute in the valid-bit */
	p[0] = cl[0] | s->eqcr.pi_vb;
	s->eqcr.pi++;
	s->eqcr.pi &= full_mask;
	s->eqcr.available--;
	if (!(s->eqcr.pi & half_mask))
		s->eqcr.pi_vb ^= QB_VALID_BIT;
	dma_wmb();
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_EQCR_PI,
				(QB_RT_BIT)|(s->eqcr.pi)|s->eqcr.pi_vb);
	return 0;
}

static int qbman_swp_enqueue_ring_mode(struct qbman_swp *s,
				       const struct qbman_eq_desc *d,
				       const struct qbman_fd *fd)
{
	if (!s->stash_off)
		return qbman_swp_enqueue_ring_mode_ptr(s, d, fd);
	else
		return qbman_swp_enqueue_ring_mode_cinh_direct(s, d, fd);
}

int qbman_swp_enqueue(struct qbman_swp *s, const struct qbman_eq_desc *d,
		      const struct qbman_fd *fd)
{
	if (s->sys.eqcr_mode == qman_eqcr_vb_array)
		return qbman_swp_enqueue_array_mode(s, d, fd);
	else    /* Use ring mode by default */
		return qbman_swp_enqueue_ring_mode(s, d, fd);
}

static int qbman_swp_enqueue_multiple_direct(struct qbman_swp *s,
					     const struct qbman_eq_desc *d,
					     const struct qbman_fd *fd,
					     uint32_t *flags,
					     int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;
	uint64_t addr_cena;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	/* Flush all the cacheline without load/store in between */
	eqcr_pi = s->eqcr.pi;
	addr_cena = (size_t)s->sys.addr_cena;
	for (i = 0; i < num_enqueued; i++) {
		dcbf((uintptr_t)(addr_cena +
			QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask)));
		eqcr_pi++;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_cinh_read_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;
	uint64_t addr_cena;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	/* Flush all the cacheline without load/store in between */
	eqcr_pi = s->eqcr.pi;
	addr_cena = (size_t)s->sys.addr_cena;
	for (i = 0; i < num_enqueued; i++) {
		dcbf(addr_cena +
			QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		eqcr_pi++;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_cinh_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		uint32_t *flags,
		int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cinh_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy_byte_by_byte(&p[1], &cl[1], 28);
		memcpy_byte_by_byte(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cinh_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_mem_back(struct qbman_swp *s,
					       const struct qbman_eq_desc *d,
					       const struct qbman_fd *fd,
					       uint32_t *flags,
					       int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI_MEMBACK) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
					eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], &fd[i], sizeof(*fd));
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		p[0] = cl[0] | s->eqcr.pi_vb;

		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	dma_wmb();
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_EQCR_PI,
				(QB_RT_BIT)|(s->eqcr.pi)|s->eqcr.pi_vb);
	return num_enqueued;
}

int qbman_swp_enqueue_multiple(struct qbman_swp *s,
				      const struct qbman_eq_desc *d,
				      const struct qbman_fd *fd,
				      uint32_t *flags,
				      int num_frames)
{
	if (!s->stash_off)
		return qbman_swp_enqueue_multiple_ptr(s, d, fd, flags,
						num_frames);
	else
		return qbman_swp_enqueue_multiple_cinh_direct(s, d, fd, flags,
						num_frames);
}

static int qbman_swp_enqueue_multiple_fd_direct(struct qbman_swp *s,
						const struct qbman_eq_desc *d,
						struct qbman_fd **fd,
						uint32_t *flags,
						int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;
	uint64_t addr_cena;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], fd[i], sizeof(struct qbman_fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	/* Flush all the cacheline without load/store in between */
	eqcr_pi = s->eqcr.pi;
	addr_cena = (size_t)s->sys.addr_cena;
	for (i = 0; i < num_enqueued; i++) {
		dcbf(addr_cena +
			QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		eqcr_pi++;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_fd_cinh_read_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;
	uint64_t addr_cena;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], fd[i], sizeof(struct qbman_fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	/* Flush all the cacheline without load/store in between */
	eqcr_pi = s->eqcr.pi;
	addr_cena = (size_t)s->sys.addr_cena;
	for (i = 0; i < num_enqueued; i++) {
		dcbf(addr_cena +
			QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		eqcr_pi++;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_fd_cinh_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		struct qbman_fd **fd,
		uint32_t *flags,
		int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
				eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cinh_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy_byte_by_byte(&p[1], &cl[1], 28);
		memcpy_byte_by_byte(&p[8], fd[i], sizeof(struct qbman_fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cinh_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_fd_mem_back(struct qbman_swp *s,
						  const struct qbman_eq_desc *d,
						  struct qbman_fd **fd,
						  uint32_t *flags,
						  int num_frames)
{
	uint32_t *p = NULL;
	const uint32_t *cl = qb_cl(d);
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI_MEMBACK) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
					eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], fd[i], sizeof(struct qbman_fd));
		eqcr_pi++;
	}

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		p[0] = cl[0] | s->eqcr.pi_vb;
		if (flags && (flags[i] & QBMAN_ENQUEUE_FLAG_DCA)) {
			struct qbman_eq_desc *d = (struct qbman_eq_desc *)p;

			d->eq.dca = (1 << QB_ENQUEUE_CMD_DCA_EN_SHIFT) |
				((flags[i]) & QBMAN_EQCR_DCA_IDXMASK);
		}
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	dma_wmb();
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_EQCR_PI,
				(QB_RT_BIT)|(s->eqcr.pi)|s->eqcr.pi_vb);
	return num_enqueued;
}

int qbman_swp_enqueue_multiple_fd(struct qbman_swp *s,
					 const struct qbman_eq_desc *d,
					 struct qbman_fd **fd,
					 uint32_t *flags,
					 int num_frames)
{
	if (!s->stash_off)
		return qbman_swp_enqueue_multiple_fd_ptr(s, d, fd, flags,
					num_frames);
	else
		return qbman_swp_enqueue_multiple_fd_cinh_direct(s, d, fd,
					flags, num_frames);
}

static int qbman_swp_enqueue_multiple_desc_direct(struct qbman_swp *s,
					const struct qbman_eq_desc *d,
					const struct qbman_fd *fd,
					int num_frames)
{
	uint32_t *p;
	const uint32_t *cl;
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;
	uint64_t addr_cena;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
					eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		p[0] = cl[0] | s->eqcr.pi_vb;
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	/* Flush all the cacheline without load/store in between */
	eqcr_pi = s->eqcr.pi;
	addr_cena = (size_t)s->sys.addr_cena;
	for (i = 0; i < num_enqueued; i++) {
		dcbf((uintptr_t)(addr_cena +
			QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask)));
		eqcr_pi++;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_desc_cinh_read_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames)
{
	uint32_t *p;
	const uint32_t *cl;
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;
	uint64_t addr_cena;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
					eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		p[0] = cl[0] | s->eqcr.pi_vb;
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	/* Flush all the cacheline without load/store in between */
	eqcr_pi = s->eqcr.pi;
	addr_cena = (size_t)s->sys.addr_cena;
	for (i = 0; i < num_enqueued; i++) {
		dcbf(addr_cena +
			QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		eqcr_pi++;
	}
	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_desc_cinh_direct(
		struct qbman_swp *s,
		const struct qbman_eq_desc *d,
		const struct qbman_fd *fd,
		int num_frames)
{
	uint32_t *p;
	const uint32_t *cl;
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cinh_read(&s->sys,
				QBMAN_CINH_SWP_EQCR_CI) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
					eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cinh_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		memcpy_byte_by_byte(&p[1], &cl[1], 28);
		memcpy_byte_by_byte(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	lwsync();

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cinh_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		p[0] = cl[0] | s->eqcr.pi_vb;
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	s->eqcr.pi = eqcr_pi & full_mask;

	return num_enqueued;
}

static int qbman_swp_enqueue_multiple_desc_mem_back(struct qbman_swp *s,
					const struct qbman_eq_desc *d,
					const struct qbman_fd *fd,
					int num_frames)
{
	uint32_t *p;
	const uint32_t *cl;
	uint32_t eqcr_ci, eqcr_pi, half_mask, full_mask;
	int i, num_enqueued = 0;

	half_mask = (s->eqcr.pi_ci_mask>>1);
	full_mask = s->eqcr.pi_ci_mask;
	if (!s->eqcr.available) {
		eqcr_ci = s->eqcr.ci;
		s->eqcr.ci = qbman_cena_read_reg(&s->sys,
				QBMAN_CENA_SWP_EQCR_CI_MEMBACK) & full_mask;
		s->eqcr.available = qm_cyc_diff(s->eqcr.pi_ring_size,
					eqcr_ci, s->eqcr.ci);
		if (!s->eqcr.available)
			return 0;
	}

	eqcr_pi = s->eqcr.pi;
	num_enqueued = (s->eqcr.available < num_frames) ?
			s->eqcr.available : num_frames;
	s->eqcr.available -= num_enqueued;
	/* Fill in the EQCR ring */
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		memcpy(&p[1], &cl[1], 28);
		memcpy(&p[8], &fd[i], sizeof(*fd));
		eqcr_pi++;
	}

	/* Set the verb byte, have to substitute in the valid-bit */
	eqcr_pi = s->eqcr.pi;
	for (i = 0; i < num_enqueued; i++) {
		p = qbman_cena_write_start_wo_shadow(&s->sys,
				QBMAN_CENA_SWP_EQCR(eqcr_pi & half_mask));
		cl = qb_cl(&d[i]);
		p[0] = cl[0] | s->eqcr.pi_vb;
		eqcr_pi++;
		if (!(eqcr_pi & half_mask))
			s->eqcr.pi_vb ^= QB_VALID_BIT;
	}

	s->eqcr.pi = eqcr_pi & full_mask;

	dma_wmb();
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_EQCR_PI,
				(QB_RT_BIT)|(s->eqcr.pi)|s->eqcr.pi_vb);

	return num_enqueued;
}
int qbman_swp_enqueue_multiple_desc(struct qbman_swp *s,
					   const struct qbman_eq_desc *d,
					   const struct qbman_fd *fd,
					   int num_frames)
{
	if (!s->stash_off)
		return qbman_swp_enqueue_multiple_desc_ptr(s, d, fd,
					num_frames);
	else
		return qbman_swp_enqueue_multiple_desc_cinh_direct(s, d, fd,
					num_frames);

}

/*************************/
/* Static (push) dequeue */
/*************************/

void qbman_swp_push_get(struct qbman_swp *s, uint8_t channel_idx, int *enabled)
{
	uint16_t src = (s->sdq >> QB_SDQCR_SRC_SHIFT) & QB_SDQCR_SRC_MASK;

	QBMAN_BUG_ON(channel_idx > 15);
	*enabled = src | (1 << channel_idx);
}

void qbman_swp_push_set(struct qbman_swp *s, uint8_t channel_idx, int enable)
{
	uint16_t dqsrc;

	QBMAN_BUG_ON(channel_idx > 15);
	if (enable)
		s->sdq |= 1 << channel_idx;
	else
		s->sdq &= ~(1 << channel_idx);

	/* Read make the complete src map.  If no channels are enabled
	 * the SDQCR must be 0 or else QMan will assert errors
	 */
	dqsrc = (s->sdq >> QB_SDQCR_SRC_SHIFT) & QB_SDQCR_SRC_MASK;
	if (dqsrc != 0)
		qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_SDQCR, s->sdq);
	else
		qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_SDQCR, 0);
}

/***************************/
/* Volatile (pull) dequeue */
/***************************/

/* These should be const, eventually */
#define QB_VDQCR_VERB_DCT_SHIFT    0
#define QB_VDQCR_VERB_DT_SHIFT     2
#define QB_VDQCR_VERB_RLS_SHIFT    4
#define QB_VDQCR_VERB_WAE_SHIFT    5
#define QB_VDQCR_VERB_RAD_SHIFT    6

enum qb_pull_dt_e {
	qb_pull_dt_channel,
	qb_pull_dt_workqueue,
	qb_pull_dt_framequeue
};

void qbman_pull_desc_clear(struct qbman_pull_desc *d)
{
	memset(d, 0, sizeof(*d));
}

void qbman_pull_desc_set_storage(struct qbman_pull_desc *d,
				 struct qbman_result *storage,
				 dma_addr_t storage_phys,
				 int stash)
{
	d->pull.rsp_addr_virt = (size_t)storage;

	if (!storage) {
		d->pull.verb &= ~(1 << QB_VDQCR_VERB_RLS_SHIFT);
		return;
	}
	d->pull.verb |= 1 << QB_VDQCR_VERB_RLS_SHIFT;
	if (stash)
		d->pull.verb |= 1 << QB_VDQCR_VERB_WAE_SHIFT;
	else
		d->pull.verb &= ~(1 << QB_VDQCR_VERB_WAE_SHIFT);

	d->pull.rsp_addr = storage_phys;
}

void qbman_pull_desc_set_numframes(struct qbman_pull_desc *d,
				   uint8_t numframes)
{
	d->pull.numf = numframes - 1;
}

void qbman_pull_desc_set_token(struct qbman_pull_desc *d, uint8_t token)
{
	d->pull.tok = token;
}

void qbman_pull_desc_set_fq(struct qbman_pull_desc *d, uint32_t fqid)
{
	d->pull.verb |= 1 << QB_VDQCR_VERB_DCT_SHIFT;
	d->pull.verb |= qb_pull_dt_framequeue << QB_VDQCR_VERB_DT_SHIFT;
	d->pull.dq_src = fqid;
}

void qbman_pull_desc_set_wq(struct qbman_pull_desc *d, uint32_t wqid,
			    enum qbman_pull_type_e dct)
{
	d->pull.verb |= dct << QB_VDQCR_VERB_DCT_SHIFT;
	d->pull.verb |= qb_pull_dt_workqueue << QB_VDQCR_VERB_DT_SHIFT;
	d->pull.dq_src = wqid;
}

void qbman_pull_desc_set_channel(struct qbman_pull_desc *d, uint32_t chid,
				 enum qbman_pull_type_e dct)
{
	d->pull.verb |= dct << QB_VDQCR_VERB_DCT_SHIFT;
	d->pull.verb |= qb_pull_dt_channel << QB_VDQCR_VERB_DT_SHIFT;
	d->pull.dq_src = chid;
}

void qbman_pull_desc_set_rad(struct qbman_pull_desc *d, int rad)
{
	if (d->pull.verb & (1 << QB_VDQCR_VERB_RLS_SHIFT)) {
		if (rad)
			d->pull.verb |= 1 << QB_VDQCR_VERB_RAD_SHIFT;
		else
			d->pull.verb &= ~(1 << QB_VDQCR_VERB_RAD_SHIFT);
	} else {
		printf("The RAD feature is not valid when RLS = 0\n");
	}
}

static int qbman_swp_pull_direct(struct qbman_swp *s,
				 struct qbman_pull_desc *d)
{
	uint32_t *p;
	uint32_t *cl = qb_cl(d);

	if (!atomic_dec_and_test(&s->vdq.busy)) {
		atomic_inc(&s->vdq.busy);
		return -EBUSY;
	}

	d->pull.tok = s->sys.idx + 1;
	s->vdq.storage = (void *)(size_t)d->pull.rsp_addr_virt;
	p = qbman_cena_write_start_wo_shadow(&s->sys, QBMAN_CENA_SWP_VDQCR);
	memcpy(&p[1], &cl[1], 12);

	/* Set the verb byte, have to substitute in the valid-bit */
	lwsync();
	p[0] = cl[0] | s->vdq.valid_bit;
	s->vdq.valid_bit ^= QB_VALID_BIT;
	qbman_cena_write_complete_wo_shadow(&s->sys, QBMAN_CENA_SWP_VDQCR);

	return 0;
}

static int qbman_swp_pull_cinh_direct(struct qbman_swp *s,
				 struct qbman_pull_desc *d)
{
	uint32_t *p;
	uint32_t *cl = qb_cl(d);

	if (!atomic_dec_and_test(&s->vdq.busy)) {
		atomic_inc(&s->vdq.busy);
		return -EBUSY;
	}

	d->pull.tok = s->sys.idx + 1;
	s->vdq.storage = (void *)(size_t)d->pull.rsp_addr_virt;
	p = qbman_cinh_write_start_wo_shadow(&s->sys, QBMAN_CENA_SWP_VDQCR);
	memcpy_byte_by_byte(&p[1], &cl[1], 12);

	/* Set the verb byte, have to substitute in the valid-bit */
	lwsync();
	p[0] = cl[0] | s->vdq.valid_bit;
	s->vdq.valid_bit ^= QB_VALID_BIT;

	return 0;
}

static int qbman_swp_pull_mem_back(struct qbman_swp *s,
				   struct qbman_pull_desc *d)
{
	uint32_t *p;
	uint32_t *cl = qb_cl(d);

	if (!atomic_dec_and_test(&s->vdq.busy)) {
		atomic_inc(&s->vdq.busy);
		return -EBUSY;
	}

	d->pull.tok = s->sys.idx + 1;
	s->vdq.storage = (void *)(size_t)d->pull.rsp_addr_virt;
	p = qbman_cena_write_start_wo_shadow(&s->sys, QBMAN_CENA_SWP_VDQCR_MEM);
	memcpy(&p[1], &cl[1], 12);

	/* Set the verb byte, have to substitute in the valid-bit */
	p[0] = cl[0] | s->vdq.valid_bit;
	s->vdq.valid_bit ^= QB_VALID_BIT;
	dma_wmb();
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_VDQCR_RT, QMAN_RT_MODE);

	return 0;
}

int qbman_swp_pull(struct qbman_swp *s, struct qbman_pull_desc *d)
{
	if (!s->stash_off)
		return qbman_swp_pull_ptr(s, d);
	else
		return qbman_swp_pull_cinh_direct(s, d);
}

/****************/
/* Polling DQRR */
/****************/

#define QMAN_DQRR_PI_MASK              0xf

#define QBMAN_RESULT_DQ        0x60
#define QBMAN_RESULT_FQRN      0x21
#define QBMAN_RESULT_FQRNI     0x22
#define QBMAN_RESULT_FQPN      0x24
#define QBMAN_RESULT_FQDAN     0x25
#define QBMAN_RESULT_CDAN      0x26
#define QBMAN_RESULT_CSCN_MEM  0x27
#define QBMAN_RESULT_CGCU      0x28
#define QBMAN_RESULT_BPSCN     0x29
#define QBMAN_RESULT_CSCN_WQ   0x2a

#include <rte_prefetch.h>

void qbman_swp_prefetch_dqrr_next(struct qbman_swp *s)
{
	const struct qbman_result *p;

	p = qbman_cena_read_wo_shadow(&s->sys,
		QBMAN_CENA_SWP_DQRR(s->dqrr.next_idx));
	rte_prefetch0(p);
}

/* NULL return if there are no unconsumed DQRR entries. Returns a DQRR entry
 * only once, so repeated calls can return a sequence of DQRR entries, without
 * requiring they be consumed immediately or in any particular order.
 */
const struct qbman_result *qbman_swp_dqrr_next(struct qbman_swp *s)
{
	if (!s->stash_off)
		return qbman_swp_dqrr_next_ptr(s);
	else
		return qbman_swp_dqrr_next_cinh_direct(s);
}

const struct qbman_result *qbman_swp_dqrr_next_direct(struct qbman_swp *s)
{
	uint32_t verb;
	uint32_t response_verb;
	uint32_t flags;
	const struct qbman_result *p;

	/* Before using valid-bit to detect if something is there, we have to
	 * handle the case of the DQRR reset bug...
	 */
	if (s->dqrr.reset_bug) {
		/* We pick up new entries by cache-inhibited producer index,
		 * which means that a non-coherent mapping would require us to
		 * invalidate and read *only* once that PI has indicated that
		 * there's an entry here. The first trip around the DQRR ring
		 * will be much less efficient than all subsequent trips around
		 * it...
		 */
		uint8_t pi = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_DQPI) &
			     QMAN_DQRR_PI_MASK;

		/* there are new entries if pi != next_idx */
		if (pi == s->dqrr.next_idx)
			return NULL;

		/* if next_idx is/was the last ring index, and 'pi' is
		 * different, we can disable the workaround as all the ring
		 * entries have now been DMA'd to so valid-bit checking is
		 * repaired. Note: this logic needs to be based on next_idx
		 * (which increments one at a time), rather than on pi (which
		 * can burst and wrap-around between our snapshots of it).
		 */
		QBMAN_BUG_ON((s->dqrr.dqrr_size - 1) < 0);
		if (s->dqrr.next_idx == (s->dqrr.dqrr_size - 1u)) {
			pr_debug("DEBUG: next_idx=%d, pi=%d, clear reset bug\n",
				 s->dqrr.next_idx, pi);
			s->dqrr.reset_bug = 0;
		}
		qbman_cena_invalidate_prefetch(&s->sys,
					QBMAN_CENA_SWP_DQRR(s->dqrr.next_idx));
	}
	p = qbman_cena_read_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_DQRR(s->dqrr.next_idx));

	verb = p->dq.verb;

	/* If the valid-bit isn't of the expected polarity, nothing there. Note,
	 * in the DQRR reset bug workaround, we shouldn't need to skip these
	 * check, because we've already determined that a new entry is available
	 * and we've invalidated the cacheline before reading it, so the
	 * valid-bit behaviour is repaired and should tell us what we already
	 * knew from reading PI.
	 */
	if ((verb & QB_VALID_BIT) != s->dqrr.valid_bit)
		return NULL;

	/* There's something there. Move "next_idx" attention to the next ring
	 * entry (and prefetch it) before returning what we found.
	 */
	s->dqrr.next_idx++;
	if (s->dqrr.next_idx == s->dqrr.dqrr_size) {
		s->dqrr.next_idx = 0;
		s->dqrr.valid_bit ^= QB_VALID_BIT;
	}
	/* If this is the final response to a volatile dequeue command
	 * indicate that the vdq is no longer busy
	 */
	flags = p->dq.stat;
	response_verb = verb & QBMAN_RESPONSE_VERB_MASK;
	if ((response_verb == QBMAN_RESULT_DQ) &&
	    (flags & QBMAN_DQ_STAT_VOLATILE) &&
	    (flags & QBMAN_DQ_STAT_EXPIRED))
		atomic_inc(&s->vdq.busy);

	return p;
}

const struct qbman_result *qbman_swp_dqrr_next_cinh_direct(struct qbman_swp *s)
{
	uint32_t verb;
	uint32_t response_verb;
	uint32_t flags;
	const struct qbman_result *p;

	/* Before using valid-bit to detect if something is there, we have to
	 * handle the case of the DQRR reset bug...
	 */
	if (s->dqrr.reset_bug) {
		/* We pick up new entries by cache-inhibited producer index,
		 * which means that a non-coherent mapping would require us to
		 * invalidate and read *only* once that PI has indicated that
		 * there's an entry here. The first trip around the DQRR ring
		 * will be much less efficient than all subsequent trips around
		 * it...
		 */
		uint8_t pi = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_DQPI) &
			     QMAN_DQRR_PI_MASK;

		/* there are new entries if pi != next_idx */
		if (pi == s->dqrr.next_idx)
			return NULL;

		/* if next_idx is/was the last ring index, and 'pi' is
		 * different, we can disable the workaround as all the ring
		 * entries have now been DMA'd to so valid-bit checking is
		 * repaired. Note: this logic needs to be based on next_idx
		 * (which increments one at a time), rather than on pi (which
		 * can burst and wrap-around between our snapshots of it).
		 */
		QBMAN_BUG_ON((s->dqrr.dqrr_size - 1) < 0);
		if (s->dqrr.next_idx == (s->dqrr.dqrr_size - 1u)) {
			pr_debug("DEBUG: next_idx=%d, pi=%d, clear reset bug\n",
				 s->dqrr.next_idx, pi);
			s->dqrr.reset_bug = 0;
		}
	}
	p = qbman_cinh_read_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_DQRR(s->dqrr.next_idx));

	verb = p->dq.verb;

	/* If the valid-bit isn't of the expected polarity, nothing there. Note,
	 * in the DQRR reset bug workaround, we shouldn't need to skip these
	 * check, because we've already determined that a new entry is available
	 * and we've invalidated the cacheline before reading it, so the
	 * valid-bit behaviour is repaired and should tell us what we already
	 * knew from reading PI.
	 */
	if ((verb & QB_VALID_BIT) != s->dqrr.valid_bit)
		return NULL;

	/* There's something there. Move "next_idx" attention to the next ring
	 * entry (and prefetch it) before returning what we found.
	 */
	s->dqrr.next_idx++;
	if (s->dqrr.next_idx == s->dqrr.dqrr_size) {
		s->dqrr.next_idx = 0;
		s->dqrr.valid_bit ^= QB_VALID_BIT;
	}
	/* If this is the final response to a volatile dequeue command
	 * indicate that the vdq is no longer busy
	 */
	flags = p->dq.stat;
	response_verb = verb & QBMAN_RESPONSE_VERB_MASK;
	if ((response_verb == QBMAN_RESULT_DQ) &&
	    (flags & QBMAN_DQ_STAT_VOLATILE) &&
	    (flags & QBMAN_DQ_STAT_EXPIRED))
		atomic_inc(&s->vdq.busy);

	return p;
}

const struct qbman_result *qbman_swp_dqrr_next_mem_back(struct qbman_swp *s)
{
	uint32_t verb;
	uint32_t response_verb;
	uint32_t flags;
	const struct qbman_result *p;

	p = qbman_cena_read_wo_shadow(&s->sys,
			QBMAN_CENA_SWP_DQRR_MEM(s->dqrr.next_idx));

	verb = p->dq.verb;

	/* If the valid-bit isn't of the expected polarity, nothing there. Note,
	 * in the DQRR reset bug workaround, we shouldn't need to skip these
	 * check, because we've already determined that a new entry is available
	 * and we've invalidated the cacheline before reading it, so the
	 * valid-bit behaviour is repaired and should tell us what we already
	 * knew from reading PI.
	 */
	if ((verb & QB_VALID_BIT) != s->dqrr.valid_bit)
		return NULL;

	/* There's something there. Move "next_idx" attention to the next ring
	 * entry (and prefetch it) before returning what we found.
	 */
	s->dqrr.next_idx++;
	if (s->dqrr.next_idx == s->dqrr.dqrr_size) {
		s->dqrr.next_idx = 0;
		s->dqrr.valid_bit ^= QB_VALID_BIT;
	}
	/* If this is the final response to a volatile dequeue command
	 * indicate that the vdq is no longer busy
	 */
	flags = p->dq.stat;
	response_verb = verb & QBMAN_RESPONSE_VERB_MASK;
	if ((response_verb == QBMAN_RESULT_DQ)
			&& (flags & QBMAN_DQ_STAT_VOLATILE)
			&& (flags & QBMAN_DQ_STAT_EXPIRED))
		atomic_inc(&s->vdq.busy);
	return p;
}

/* Consume DQRR entries previously returned from qbman_swp_dqrr_next(). */
void qbman_swp_dqrr_consume(struct qbman_swp *s,
			    const struct qbman_result *dq)
{
	qbman_cinh_write(&s->sys,
			QBMAN_CINH_SWP_DCAP, QBMAN_IDX_FROM_DQRR(dq));
}

/* Consume DQRR entries previously returned from qbman_swp_dqrr_next(). */
void qbman_swp_dqrr_idx_consume(struct qbman_swp *s,
			    uint8_t dqrr_index)
{
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_DCAP, dqrr_index);
}

/*********************************/
/* Polling user-provided storage */
/*********************************/

int qbman_result_has_new_result(struct qbman_swp *s,
				struct qbman_result *dq)
{
	if (dq->dq.tok == 0)
		return 0;

	/*
	 * Set token to be 0 so we will detect change back to 1
	 * next time the looping is traversed. Const is cast away here
	 * as we want users to treat the dequeue responses as read only.
	 */
	((struct qbman_result *)dq)->dq.tok = 0;

	/*
	 * VDQCR "no longer busy" hook - not quite the same as DQRR, because
	 * the fact "VDQCR" shows busy doesn't mean that we hold the result
	 * that makes it available. Eg. we may be looking at our 10th dequeue
	 * result, having released VDQCR after the 1st result and it is now
	 * busy due to some other command!
	 */
	if (s->vdq.storage == dq) {
		s->vdq.storage = NULL;
		atomic_inc(&s->vdq.busy);
	}

	return 1;
}

int qbman_check_new_result(struct qbman_result *dq)
{
	if (dq->dq.tok == 0)
		return 0;

	/*
	 * Set token to be 0 so we will detect change back to 1
	 * next time the looping is traversed. Const is cast away here
	 * as we want users to treat the dequeue responses as read only.
	 */
	((struct qbman_result *)dq)->dq.tok = 0;

	return 1;
}

int qbman_check_command_complete(struct qbman_result *dq)
{
	struct qbman_swp *s;

	if (dq->dq.tok == 0)
		return 0;

	s = portal_idx_map[dq->dq.tok - 1];
	/*
	 * VDQCR "no longer busy" hook - not quite the same as DQRR, because
	 * the fact "VDQCR" shows busy doesn't mean that we hold the result
	 * that makes it available. Eg. we may be looking at our 10th dequeue
	 * result, having released VDQCR after the 1st result and it is now
	 * busy due to some other command!
	 */
	if (s->vdq.storage == dq) {
		s->vdq.storage = NULL;
		atomic_inc(&s->vdq.busy);
	}

	return 1;
}

/********************************/
/* Categorising qbman results   */
/********************************/

static inline int __qbman_result_is_x(const struct qbman_result *dq,
				      uint8_t x)
{
	uint8_t response_verb = dq->dq.verb & QBMAN_RESPONSE_VERB_MASK;

	return (response_verb == x);
}

int qbman_result_is_DQ(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_DQ);
}

int qbman_result_is_FQDAN(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_FQDAN);
}

int qbman_result_is_CDAN(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_CDAN);
}

int qbman_result_is_CSCN(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_CSCN_MEM) ||
		__qbman_result_is_x(dq, QBMAN_RESULT_CSCN_WQ);
}

int qbman_result_is_BPSCN(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_BPSCN);
}

int qbman_result_is_CGCU(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_CGCU);
}

int qbman_result_is_FQRN(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_FQRN);
}

int qbman_result_is_FQRNI(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_FQRNI);
}

int qbman_result_is_FQPN(const struct qbman_result *dq)
{
	return __qbman_result_is_x(dq, QBMAN_RESULT_FQPN);
}

/*********************************/
/* Parsing frame dequeue results */
/*********************************/

/* These APIs assume qbman_result_is_DQ() is TRUE */

uint8_t qbman_result_DQ_flags(const struct qbman_result *dq)
{
	return dq->dq.stat;
}

uint16_t qbman_result_DQ_seqnum(const struct qbman_result *dq)
{
	return dq->dq.seqnum;
}

uint16_t qbman_result_DQ_odpid(const struct qbman_result *dq)
{
	return dq->dq.oprid;
}

uint32_t qbman_result_DQ_fqid(const struct qbman_result *dq)
{
	return dq->dq.fqid;
}

uint32_t qbman_result_DQ_byte_count(const struct qbman_result *dq)
{
	return dq->dq.fq_byte_cnt;
}

uint32_t qbman_result_DQ_frame_count(const struct qbman_result *dq)
{
	return dq->dq.fq_frm_cnt;
}

uint64_t qbman_result_DQ_fqd_ctx(const struct qbman_result *dq)
{
	return dq->dq.fqd_ctx;
}

const struct qbman_fd *qbman_result_DQ_fd(const struct qbman_result *dq)
{
	return (const struct qbman_fd *)&dq->dq.fd[0];
}

/**************************************/
/* Parsing state-change notifications */
/**************************************/
uint8_t qbman_result_SCN_state(const struct qbman_result *scn)
{
	return scn->scn.state;
}

uint32_t qbman_result_SCN_rid(const struct qbman_result *scn)
{
	return scn->scn.rid_tok;
}

uint64_t qbman_result_SCN_ctx(const struct qbman_result *scn)
{
	return scn->scn.ctx;
}

/*****************/
/* Parsing BPSCN */
/*****************/
uint16_t qbman_result_bpscn_bpid(const struct qbman_result *scn)
{
	return (uint16_t)qbman_result_SCN_rid(scn) & 0x3FFF;
}

int qbman_result_bpscn_has_free_bufs(const struct qbman_result *scn)
{
	return !(int)(qbman_result_SCN_state(scn) & 0x1);
}

int qbman_result_bpscn_is_depleted(const struct qbman_result *scn)
{
	return (int)(qbman_result_SCN_state(scn) & 0x2);
}

int qbman_result_bpscn_is_surplus(const struct qbman_result *scn)
{
	return (int)(qbman_result_SCN_state(scn) & 0x4);
}

uint64_t qbman_result_bpscn_ctx(const struct qbman_result *scn)
{
	return qbman_result_SCN_ctx(scn);
}

/*****************/
/* Parsing CGCU  */
/*****************/
uint16_t qbman_result_cgcu_cgid(const struct qbman_result *scn)
{
	return (uint16_t)qbman_result_SCN_rid(scn) & 0xFFFF;
}

uint64_t qbman_result_cgcu_icnt(const struct qbman_result *scn)
{
	return qbman_result_SCN_ctx(scn);
}

/********************/
/* Parsing EQ RESP  */
/********************/
struct qbman_fd *qbman_result_eqresp_fd(struct qbman_result *eqresp)
{
	return (struct qbman_fd *)&eqresp->eq_resp.fd[0];
}

void qbman_result_eqresp_set_rspid(struct qbman_result *eqresp, uint8_t val)
{
	eqresp->eq_resp.rspid = val;
}

uint8_t qbman_result_eqresp_rspid(struct qbman_result *eqresp)
{
	return eqresp->eq_resp.rspid;
}

uint8_t qbman_result_eqresp_rc(struct qbman_result *eqresp)
{
	if (eqresp->eq_resp.rc == 0xE)
		return 0;
	else
		return -1;
}

/******************/
/* Buffer release */
/******************/
#define QB_BR_RC_VALID_SHIFT  5
#define QB_BR_RCDI_SHIFT      6

void qbman_release_desc_clear(struct qbman_release_desc *d)
{
	memset(d, 0, sizeof(*d));
	d->br.verb = 1 << QB_BR_RC_VALID_SHIFT;
}

void qbman_release_desc_set_bpid(struct qbman_release_desc *d, uint16_t bpid)
{
	d->br.bpid = bpid;
}

void qbman_release_desc_set_rcdi(struct qbman_release_desc *d, int enable)
{
	if (enable)
		d->br.verb |= 1 << QB_BR_RCDI_SHIFT;
	else
		d->br.verb &= ~(1 << QB_BR_RCDI_SHIFT);
}

#define RAR_IDX(rar)     ((rar) & 0x7)
#define RAR_VB(rar)      ((rar) & 0x80)
#define RAR_SUCCESS(rar) ((rar) & 0x100)

static int qbman_swp_release_direct(struct qbman_swp *s,
				    const struct qbman_release_desc *d,
				    const uint64_t *buffers,
				    unsigned int num_buffers)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t rar = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_RAR);

	pr_debug("RAR=%08x\n", rar);
	if (!RAR_SUCCESS(rar))
		return -EBUSY;

	QBMAN_BUG_ON(!num_buffers || (num_buffers > 7));

	/* Start the release command */
	p = qbman_cena_write_start_wo_shadow(&s->sys,
				     QBMAN_CENA_SWP_RCR(RAR_IDX(rar)));

	/* Copy the caller's buffer pointers to the command */
	u64_to_le32_copy(&p[2], buffers, num_buffers);

	/* Set the verb byte, have to substitute in the valid-bit and the
	 * number of buffers.
	 */
	lwsync();
	p[0] = cl[0] | RAR_VB(rar) | num_buffers;
	qbman_cena_write_complete_wo_shadow(&s->sys,
				    QBMAN_CENA_SWP_RCR(RAR_IDX(rar)));

	return 0;
}

static int qbman_swp_release_cinh_direct(struct qbman_swp *s,
				    const struct qbman_release_desc *d,
				    const uint64_t *buffers,
				    unsigned int num_buffers)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t rar = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_RAR);

	pr_debug("RAR=%08x\n", rar);
	if (!RAR_SUCCESS(rar))
		return -EBUSY;

	QBMAN_BUG_ON(!num_buffers || (num_buffers > 7));

	/* Start the release command */
	p = qbman_cinh_write_start_wo_shadow(&s->sys,
				     QBMAN_CENA_SWP_RCR(RAR_IDX(rar)));

	/* Copy the caller's buffer pointers to the command */
	memcpy_byte_by_byte(&p[2], buffers, num_buffers * sizeof(uint64_t));

	/* Set the verb byte, have to substitute in the valid-bit and the
	 * number of buffers.
	 */
	lwsync();
	p[0] = cl[0] | RAR_VB(rar) | num_buffers;

	return 0;
}

static int qbman_swp_release_mem_back(struct qbman_swp *s,
				      const struct qbman_release_desc *d,
				      const uint64_t *buffers,
				      unsigned int num_buffers)
{
	uint32_t *p;
	const uint32_t *cl = qb_cl(d);
	uint32_t rar = qbman_cinh_read(&s->sys, QBMAN_CINH_SWP_RAR);

	pr_debug("RAR=%08x\n", rar);
	if (!RAR_SUCCESS(rar))
		return -EBUSY;

	QBMAN_BUG_ON(!num_buffers || (num_buffers > 7));

	/* Start the release command */
	p = qbman_cena_write_start_wo_shadow(&s->sys,
		QBMAN_CENA_SWP_RCR_MEM(RAR_IDX(rar)));

	/* Copy the caller's buffer pointers to the command */
	u64_to_le32_copy(&p[2], buffers, num_buffers);

	/* Set the verb byte, have to substitute in the valid-bit and the
	 * number of buffers.
	 */
	p[0] = cl[0] | RAR_VB(rar) | num_buffers;
	lwsync();
	qbman_cinh_write(&s->sys, QBMAN_CINH_SWP_RCR_AM_RT +
		RAR_IDX(rar) * 4, QMAN_RT_MODE);

	return 0;
}

int qbman_swp_release(struct qbman_swp *s,
			     const struct qbman_release_desc *d,
			     const uint64_t *buffers,
			     unsigned int num_buffers)
{
	if (!s->stash_off)
		return qbman_swp_release_ptr(s, d, buffers, num_buffers);
	else
		return qbman_swp_release_cinh_direct(s, d, buffers,
						num_buffers);
}

/*******************/
/* Buffer acquires */
/*******************/
struct qbman_acquire_desc {
	uint8_t verb;
	uint8_t reserved;
	uint16_t bpid;
	uint8_t num;
	uint8_t reserved2[59];
};

struct qbman_acquire_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint16_t reserved;
	uint8_t num;
	uint8_t reserved2[3];
	uint64_t buf[7];
};

static int qbman_swp_acquire_direct(struct qbman_swp *s, uint16_t bpid,
				uint64_t *buffers, unsigned int num_buffers)
{
	struct qbman_acquire_desc *p;
	struct qbman_acquire_rslt *r;

	if (!num_buffers || (num_buffers > 7))
		return -EINVAL;

	/* Start the management command */
	p = qbman_swp_mc_start(s);

	if (!p)
		return -EBUSY;

	/* Encode the caller-provided attributes */
	p->bpid = bpid;
	p->num = num_buffers;

	/* Complete the management command */
	r = qbman_swp_mc_complete(s, p, QBMAN_MC_ACQUIRE);
	if (!r) {
		pr_err("qbman: acquire from BPID %d failed, no response\n",
		       bpid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_MC_ACQUIRE);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Acquire buffers from BPID 0x%x failed, code=0x%02x\n",
		       bpid, r->rslt);
		return -EIO;
	}

	QBMAN_BUG_ON(r->num > num_buffers);

	/* Copy the acquired buffers to the caller's array */
	u64_from_le32_copy(buffers, &r->buf[0], r->num);

	return (int)r->num;
}

static int qbman_swp_acquire_cinh_direct(struct qbman_swp *s, uint16_t bpid,
			uint64_t *buffers, unsigned int num_buffers)
{
	struct qbman_acquire_desc *p;
	struct qbman_acquire_rslt *r;

	if (!num_buffers || (num_buffers > 7))
		return -EINVAL;

	/* Start the management command */
	p = qbman_swp_mc_start(s);

	if (!p)
		return -EBUSY;

	/* Encode the caller-provided attributes */
	p->bpid = bpid;
	p->num = num_buffers;

	/* Complete the management command */
	r = qbman_swp_mc_complete_cinh(s, p, QBMAN_MC_ACQUIRE);
	if (!r) {
		pr_err("qbman: acquire from BPID %d failed, no response\n",
		       bpid);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != QBMAN_MC_ACQUIRE);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("Acquire buffers from BPID 0x%x failed, code=0x%02x\n",
		       bpid, r->rslt);
		return -EIO;
	}

	QBMAN_BUG_ON(r->num > num_buffers);

	/* Copy the acquired buffers to the caller's array */
	u64_from_le32_copy(buffers, &r->buf[0], r->num);

	return (int)r->num;
}

int qbman_swp_acquire(struct qbman_swp *s, uint16_t bpid, uint64_t *buffers,
		      unsigned int num_buffers)
{
	if (!s->stash_off)
		return qbman_swp_acquire_direct(s, bpid, buffers, num_buffers);
	else
		return qbman_swp_acquire_cinh_direct(s, bpid, buffers,
					num_buffers);
}

/*****************/
/* FQ management */
/*****************/
struct qbman_alt_fq_state_desc {
	uint8_t verb;
	uint8_t reserved[3];
	uint32_t fqid;
	uint8_t reserved2[56];
};

struct qbman_alt_fq_state_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint8_t reserved[62];
};

#define ALT_FQ_FQID_MASK 0x00FFFFFF

static int qbman_swp_alt_fq_state(struct qbman_swp *s, uint32_t fqid,
				  uint8_t alt_fq_verb)
{
	struct qbman_alt_fq_state_desc *p;
	struct qbman_alt_fq_state_rslt *r;

	/* Start the management command */
	p = qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	p->fqid = fqid & ALT_FQ_FQID_MASK;

	/* Complete the management command */
	r = qbman_swp_mc_complete(s, p, alt_fq_verb);
	if (!r) {
		pr_err("qbman: mgmt cmd failed, no response (verb=0x%x)\n",
		       alt_fq_verb);
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK) != alt_fq_verb);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("ALT FQID %d failed: verb = 0x%08x, code = 0x%02x\n",
		       fqid, alt_fq_verb, r->rslt);
		return -EIO;
	}

	return 0;
}

int qbman_swp_fq_schedule(struct qbman_swp *s, uint32_t fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_SCHEDULE);
}

int qbman_swp_fq_force(struct qbman_swp *s, uint32_t fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_FORCE);
}

int qbman_swp_fq_xon(struct qbman_swp *s, uint32_t fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_XON);
}

int qbman_swp_fq_xoff(struct qbman_swp *s, uint32_t fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_XOFF);
}

/**********************/
/* Channel management */
/**********************/

struct qbman_cdan_ctrl_desc {
	uint8_t verb;
	uint8_t reserved;
	uint16_t ch;
	uint8_t we;
	uint8_t ctrl;
	uint16_t reserved2;
	uint64_t cdan_ctx;
	uint8_t reserved3[48];

};

struct qbman_cdan_ctrl_rslt {
	uint8_t verb;
	uint8_t rslt;
	uint16_t ch;
	uint8_t reserved[60];
};

/* Hide "ICD" for now as we don't use it, don't set it, and don't test it, so it
 * would be irresponsible to expose it.
 */
#define CODE_CDAN_WE_EN    0x1
#define CODE_CDAN_WE_CTX   0x4

static int qbman_swp_CDAN_set(struct qbman_swp *s, uint16_t channelid,
			      uint8_t we_mask, uint8_t cdan_en,
			      uint64_t ctx)
{
	struct qbman_cdan_ctrl_desc *p;
	struct qbman_cdan_ctrl_rslt *r;

	/* Start the management command */
	p = qbman_swp_mc_start(s);
	if (!p)
		return -EBUSY;

	/* Encode the caller-provided attributes */
	p->ch = channelid;
	p->we = we_mask;
	if (cdan_en)
		p->ctrl = 1;
	else
		p->ctrl = 0;
	p->cdan_ctx = ctx;

	/* Complete the management command */
	r = qbman_swp_mc_complete(s, p, QBMAN_WQCHAN_CONFIGURE);
	if (!r) {
		pr_err("qbman: wqchan config failed, no response\n");
		return -EIO;
	}

	/* Decode the outcome */
	QBMAN_BUG_ON((r->verb & QBMAN_RESPONSE_VERB_MASK)
		     != QBMAN_WQCHAN_CONFIGURE);

	/* Determine success or failure */
	if (r->rslt != QBMAN_MC_RSLT_OK) {
		pr_err("CDAN cQID %d failed: code = 0x%02x\n",
		       channelid, r->rslt);
		return -EIO;
	}

	return 0;
}

int qbman_swp_CDAN_set_context(struct qbman_swp *s, uint16_t channelid,
			       uint64_t ctx)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_CTX,
				  0, ctx);
}

int qbman_swp_CDAN_enable(struct qbman_swp *s, uint16_t channelid)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_EN,
				  1, 0);
}

int qbman_swp_CDAN_disable(struct qbman_swp *s, uint16_t channelid)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_EN,
				  0, 0);
}

int qbman_swp_CDAN_set_context_enable(struct qbman_swp *s, uint16_t channelid,
				      uint64_t ctx)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_EN | CODE_CDAN_WE_CTX,
				  1, ctx);
}

uint8_t qbman_get_dqrr_idx(const struct qbman_result *dqrr)
{
	return QBMAN_IDX_FROM_DQRR(dqrr);
}

struct qbman_result *qbman_get_dqrr_from_idx(struct qbman_swp *s, uint8_t idx)
{
	struct qbman_result *dq;

	dq = qbman_cena_read(&s->sys, QBMAN_CENA_SWP_DQRR(idx));
	return dq;
}
