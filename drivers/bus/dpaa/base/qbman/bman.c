/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include "bman.h"
#include <rte_branch_prediction.h>

/* Compilation constants */
#define RCR_THRESH	2	/* reread h/w CI when running out of space */
#define IRQNAME		"BMan portal %d"
#define MAX_IRQNAME	16	/* big enough for "BMan portal %d" */

struct bman_portal {
	struct bm_portal p;
	/* 2-element array. pools[0] is mask, pools[1] is snapshot. */
	struct bman_depletion *pools;
	int thresh_set;
	unsigned long irq_sources;
	u32 slowpoll;	/* only used when interrupts are off */
	/* When the cpu-affine portal is activated, this is non-NULL */
	const struct bm_portal_config *config;
	char irqname[MAX_IRQNAME];
};

static cpumask_t affine_mask;
static DEFINE_SPINLOCK(affine_mask_lock);
static RTE_DEFINE_PER_LCORE(struct bman_portal, bman_affine_portal);

static inline struct bman_portal *get_affine_portal(void)
{
	return &RTE_PER_LCORE(bman_affine_portal);
}

/*
 * This object type refers to a pool, it isn't *the* pool. There may be
 * more than one such object per BMan buffer pool, eg. if different users of
 * the pool are operating via different portals.
 */
struct bman_pool {
	struct bman_pool_params params;
	/* Used for hash-table admin when using depletion notifications. */
	struct bman_portal *portal;
	struct bman_pool *next;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	atomic_t in_use;
#endif
};

static inline
struct bman_portal *bman_create_portal(struct bman_portal *portal,
				       const struct bm_portal_config *c)
{
	struct bm_portal *p;
	const struct bman_depletion *pools = &c->mask;
	int ret;
	u8 bpid = 0;

	p = &portal->p;
	/*
	 * prep the low-level portal struct with the mapped addresses from the
	 * config, everything that follows depends on it and "config" is more
	 * for (de)reference...
	 */
	p->addr.ce = c->addr_virt[DPAA_PORTAL_CE];
	p->addr.ci = c->addr_virt[DPAA_PORTAL_CI];
	if (bm_rcr_init(p, bm_rcr_pvb, bm_rcr_cce)) {
		pr_err("Bman RCR initialisation failed\n");
		return NULL;
	}
	if (bm_mc_init(p)) {
		pr_err("Bman MC initialisation failed\n");
		goto fail_mc;
	}
	portal->pools = kmalloc(2 * sizeof(*pools), GFP_KERNEL);
	if (!portal->pools)
		goto fail_pools;
	portal->pools[0] = *pools;
	bman_depletion_init(portal->pools + 1);
	while (bpid < bman_pool_max) {
		/*
		 * Default to all BPIDs disabled, we enable as required at
		 * run-time.
		 */
		bm_isr_bscn_mask(p, bpid, 0);
		bpid++;
	}
	portal->slowpoll = 0;
	/* Write-to-clear any stale interrupt status bits */
	bm_isr_disable_write(p, 0xffffffff);
	portal->irq_sources = 0;
	bm_isr_enable_write(p, portal->irq_sources);
	bm_isr_status_clear(p, 0xffffffff);
	snprintf(portal->irqname, MAX_IRQNAME, IRQNAME, c->cpu);
	if (request_irq(c->irq, NULL, 0, portal->irqname,
			portal)) {
		pr_err("request_irq() failed\n");
		goto fail_irq;
	}

	/* Need RCR to be empty before continuing */
	ret = bm_rcr_get_fill(p);
	if (ret) {
		pr_err("Bman RCR unclean\n");
		goto fail_rcr_empty;
	}
	/* Success */
	portal->config = c;

	bm_isr_disable_write(p, 0);
	bm_isr_uninhibit(p);
	return portal;
fail_rcr_empty:
	free_irq(c->irq, portal);
fail_irq:
	kfree(portal->pools);
fail_pools:
	bm_mc_finish(p);
fail_mc:
	bm_rcr_finish(p);
	return NULL;
}

struct bman_portal *
bman_create_affine_portal(const struct bm_portal_config *c)
{
	struct bman_portal *portal = get_affine_portal();

	/*This function is called from the context which is already affine to
	 *CPU or in other words this in non-migratable to other CPUs.
	 */
	portal = bman_create_portal(portal, c);
	if (portal) {
		spin_lock(&affine_mask_lock);
		CPU_SET(c->cpu, &affine_mask);
		spin_unlock(&affine_mask_lock);
	}
	return portal;
}

static inline
void bman_destroy_portal(struct bman_portal *bm)
{
	const struct bm_portal_config *pcfg;

	pcfg = bm->config;
	bm_rcr_cce_update(&bm->p);
	bm_rcr_cce_update(&bm->p);

	free_irq(pcfg->irq, bm);

	kfree(bm->pools);
	bm_mc_finish(&bm->p);
	bm_rcr_finish(&bm->p);
	bm->config = NULL;
}

const struct
bm_portal_config *bman_destroy_affine_portal(void)
{
	struct bman_portal *bm = get_affine_portal();
	const struct bm_portal_config *pcfg;

	pcfg = bm->config;
	bman_destroy_portal(bm);
	spin_lock(&affine_mask_lock);
	CPU_CLR(pcfg->cpu, &affine_mask);
	spin_unlock(&affine_mask_lock);
	return pcfg;
}

int
bman_get_portal_index(void)
{
	struct bman_portal *p = get_affine_portal();
	return p->config->index;
}

static const u32 zero_thresholds[4] = {0, 0, 0, 0};

struct bman_pool *bman_new_pool(const struct bman_pool_params *params)
{
	struct bman_pool *pool = NULL;
	u32 bpid;

	if (params->flags & BMAN_POOL_FLAG_DYNAMIC_BPID) {
		int ret = bman_alloc_bpid(&bpid);

		if (ret)
			return NULL;
	} else {
		if (params->bpid >= bman_pool_max)
			return NULL;
		bpid = params->bpid;
	}
	if (params->flags & BMAN_POOL_FLAG_THRESH) {
		int ret = bm_pool_set(bpid, params->thresholds);

		if (ret)
			goto err;
	}

	pool = kmalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		goto err;
	pool->params = *params;
#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	atomic_set(&pool->in_use, 1);
#endif
	if (params->flags & BMAN_POOL_FLAG_DYNAMIC_BPID)
		pool->params.bpid = bpid;

	return pool;
err:
	if (params->flags & BMAN_POOL_FLAG_THRESH)
		bm_pool_set(bpid, zero_thresholds);

	if (params->flags & BMAN_POOL_FLAG_DYNAMIC_BPID)
		bman_release_bpid(bpid);
	kfree(pool);

	return NULL;
}

void bman_free_pool(struct bman_pool *pool)
{
	if (pool->params.flags & BMAN_POOL_FLAG_THRESH)
		bm_pool_set(pool->params.bpid, zero_thresholds);
	if (pool->params.flags & BMAN_POOL_FLAG_DYNAMIC_BPID)
		bman_release_bpid(pool->params.bpid);
	kfree(pool);
}

const struct bman_pool_params *bman_get_params(const struct bman_pool *pool)
{
	return &pool->params;
}

static void update_rcr_ci(struct bman_portal *p, int avail)
{
	if (avail)
		bm_rcr_cce_prefetch(&p->p);
	else
		bm_rcr_cce_update(&p->p);
}

#define BMAN_BUF_MASK 0x0000fffffffffffful
int bman_release(struct bman_pool *pool, const struct bm_buffer *bufs, u8 num,
		 u32 flags __maybe_unused)
{
	struct bman_portal *p;
	struct bm_rcr_entry *r;
	u32 i = num - 1;
	u8 avail;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (!num || (num > 8))
		return -EINVAL;
	if (pool->params.flags & BMAN_POOL_FLAG_NO_RELEASE)
		return -EINVAL;
#endif

	p = get_affine_portal();
	avail = bm_rcr_get_avail(&p->p);
	if (avail < 2)
		update_rcr_ci(p, avail);
	r = bm_rcr_start(&p->p);
	if (unlikely(!r))
		return -EBUSY;

	/*
	 * we can copy all but the first entry, as this can trigger badness
	 * with the valid-bit
	 */
	r->bufs[0].opaque =
		cpu_to_be64(((u64)pool->params.bpid << 48) |
			    (bufs[0].opaque & BMAN_BUF_MASK));
	if (i) {
		for (i = 1; i < num; i++)
			r->bufs[i].opaque =
				cpu_to_be64(bufs[i].opaque & BMAN_BUF_MASK);
	}

	bm_rcr_pvb_commit(&p->p, BM_RCR_VERB_CMD_BPID_SINGLE |
			  (num & BM_RCR_VERB_BUFCOUNT_MASK));

	return 0;
}

int bman_acquire(struct bman_pool *pool, struct bm_buffer *bufs, u8 num,
		 u32 flags __maybe_unused)
{
	struct bman_portal *p = get_affine_portal();
	struct bm_mc_command *mcc;
	struct bm_mc_result *mcr;
	int ret, i;

#ifdef RTE_LIBRTE_DPAA_HWDEBUG
	if (!num || (num > 8))
		return -EINVAL;
	if (pool->params.flags & BMAN_POOL_FLAG_ONLY_RELEASE)
		return -EINVAL;
#endif

	mcc = bm_mc_start(&p->p);
	mcc->acquire.bpid = pool->params.bpid;
	bm_mc_commit(&p->p, BM_MCC_VERB_CMD_ACQUIRE |
			(num & BM_MCC_VERB_ACQUIRE_BUFCOUNT));
	while (!(mcr = bm_mc_result(&p->p)))
		cpu_relax();
	ret = mcr->verb & BM_MCR_VERB_ACQUIRE_BUFCOUNT;
	if (bufs) {
		for (i = 0; i < num; i++)
			bufs[i].opaque =
				be64_to_cpu(mcr->acquire.bufs[i].opaque);
	}
	if (ret != num)
		ret = -ENOMEM;
	return ret;
}

int bman_query_pools(struct bm_pool_state *state)
{
	struct bman_portal *p = get_affine_portal();
	struct bm_mc_result *mcr;

	bm_mc_start(&p->p);
	bm_mc_commit(&p->p, BM_MCC_VERB_CMD_QUERY);
	while (!(mcr = bm_mc_result(&p->p)))
		cpu_relax();
	DPAA_ASSERT((mcr->verb & BM_MCR_VERB_CMD_MASK) ==
		    BM_MCR_VERB_CMD_QUERY);
	*state = mcr->query;
	state->as.state.state[0] = be32_to_cpu(state->as.state.state[0]);
	state->as.state.state[1] = be32_to_cpu(state->as.state.state[1]);
	state->ds.state.state[0] = be32_to_cpu(state->ds.state.state[0]);
	state->ds.state.state[1] = be32_to_cpu(state->ds.state.state[1]);
	return 0;
}

u32 bman_query_free_buffers(struct bman_pool *pool)
{
	return bm_pool_free_buffers(pool->params.bpid);
}

int bman_update_pool_thresholds(struct bman_pool *pool, const u32 *thresholds)
{
	u32 bpid;

	bpid = bman_get_params(pool)->bpid;

	return bm_pool_set(bpid, thresholds);
}

int bman_shutdown_pool(u32 bpid)
{
	struct bman_portal *p = get_affine_portal();
	return bm_shutdown_pool(&p->p, bpid);
}
