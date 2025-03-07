/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Huawei Technologies Co., Ltd
 */

#include <inttypes.h>

#include <eal_export.h>
#include <rte_string_fns.h>

#include "soring.h"

RTE_LOG_REGISTER_DEFAULT(soring_logtype, INFO);

static uint32_t
soring_calc_elem_num(uint32_t count)
{
	return rte_align32pow2(count + 1);
}

static int
soring_check_param(uint32_t esize, uint32_t msize, uint32_t count,
	uint32_t stages)
{
	if (stages == 0) {
		SORING_LOG(ERR, "invalid number of stages: %u", stages);
		return -EINVAL;
	}

	/* Check if element size is a multiple of 4B */
	if (esize == 0 || esize % 4 != 0) {
		SORING_LOG(ERR, "invalid element size: %u", esize);
		return -EINVAL;
	}

	/* Check if size of metadata is a multiple of 4B */
	if (msize % 4 != 0) {
		SORING_LOG(ERR, "invalid metadata size: %u", msize);
		return -EINVAL;
	}

	 /* count must be a power of 2 */
	if (rte_is_power_of_2(count) == 0 ||
			(count > RTE_SORING_ELEM_MAX + 1)) {
		SORING_LOG(ERR, "invalid number of elements: %u", count);
		return -EINVAL;
	}

	return 0;
}

/*
 * Calculate size offsets for SORING internal data layout.
 */
static size_t
soring_get_szofs(uint32_t esize, uint32_t msize, uint32_t count,
	uint32_t stages, size_t *elst_ofs, size_t *state_ofs,
	size_t *stage_ofs)
{
	size_t sz;
	const struct rte_soring * const r = NULL;

	sz = sizeof(r[0]) + (size_t)count * esize;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	if (elst_ofs != NULL)
		*elst_ofs = sz;

	sz = sz + (size_t)count * msize;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	if (state_ofs != NULL)
		*state_ofs = sz;

	sz += sizeof(r->state[0]) * count;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	if (stage_ofs != NULL)
		*stage_ofs = sz;

	sz += sizeof(r->stage[0]) * stages;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	return sz;
}

static void
soring_dump_stage_headtail(FILE *f, const char *prefix,
		struct soring_stage *st)
{
	fprintf(f, "%stail.pos=%"PRIu32"\n", prefix, st->sht.tail.pos);
	fprintf(f, "%stail.sync=%"PRIu32"\n", prefix, st->sht.tail.sync);
	fprintf(f, "%shead=%"PRIu32"\n", prefix, st->sht.head);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_dump, 25.03)
void
rte_soring_dump(FILE *f, const struct rte_soring *r)
{
	uint32_t i;
	char buf[32];

	if (f == NULL || r == NULL)
		return;

	fprintf(f, "soring <%s>@%p\n", r->name, r);
	fprintf(f, "  size=%"PRIu32"\n", r->size);
	fprintf(f, "  capacity=%"PRIu32"\n", r->capacity);
	fprintf(f, "  esize=%"PRIu32"\n", r->esize);
	fprintf(f, "  msize=%"PRIu32"\n", r->msize);
	fprintf(f, "  used=%u\n", rte_soring_count(r));
	fprintf(f, "  avail=%u\n", rte_soring_free_count(r));

	rte_ring_headtail_dump(f, "  cons.", &(r->cons.ht));
	rte_ring_headtail_dump(f, "  prod.", &(r->prod.ht));

	fprintf(f, "  nb_stage=%"PRIu32"\n", r->nb_stage);
	for (i = 0; i < r->nb_stage; i++) {
		snprintf(buf, sizeof(buf), "  stage[%u].", i);
		soring_dump_stage_headtail(f, buf, r->stage + i);
	}
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_get_memsize, 25.03)
ssize_t
rte_soring_get_memsize(const struct rte_soring_param *prm)
{
	int32_t rc;
	uint32_t count;

	count = soring_calc_elem_num(prm->elems);
	rc = soring_check_param(prm->elem_size, prm->meta_size, count,
			prm->stages);
	if (rc != 0)
		return rc;

	return soring_get_szofs(prm->elem_size, prm->meta_size, count,
			prm->stages, NULL, NULL, NULL);
}

/* compilation-time checks */
static void
soring_compilation_checks(void)
{
	RTE_BUILD_BUG_ON((sizeof(struct rte_soring) &
			RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_soring, cons) &
			RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_soring, prod) &
			RTE_CACHE_LINE_MASK) != 0);

	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, tail) !=
		offsetof(struct soring_stage_headtail, tail.pos));
	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, sync_type) !=
		offsetof(struct soring_stage_headtail, unused));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_soring_init, 25.03)
int
rte_soring_init(struct rte_soring *r, const struct rte_soring_param *prm)
{
	int32_t rc;
	uint32_t n;
	size_t meta_ofs, stage_ofs, state_ofs;

	soring_compilation_checks();

	if (r == NULL || prm == NULL)
		return -EINVAL;

	n = soring_calc_elem_num(prm->elems);
	rc = soring_check_param(prm->elem_size, prm->meta_size, n, prm->stages);
	if (rc != 0)
		return rc;

	soring_get_szofs(prm->elem_size, prm->meta_size, n, prm->stages,
			&meta_ofs, &state_ofs, &stage_ofs);

	memset(r, 0, sizeof(*r));
	rc = strlcpy(r->name, prm->name, sizeof(r->name));
	if (rc < 0 || rc >= (int)sizeof(r->name))
		return -ENAMETOOLONG;

	r->size = n;
	r->mask = r->size - 1;
	r->capacity = prm->elems;
	r->esize = prm->elem_size;
	r->msize = prm->meta_size;

	r->prod.ht.sync_type = prm->prod_synt;
	r->cons.ht.sync_type = prm->cons_synt;

	r->state = (union soring_state *)((uintptr_t)r + state_ofs);
	memset(r->state, 0, sizeof(r->state[0]) * r->size);

	r->stage = (struct soring_stage *)((uintptr_t)r + stage_ofs);
	r->nb_stage = prm->stages;
	memset(r->stage, 0, r->nb_stage * sizeof(r->stage[0]));

	if (r->msize != 0)
		r->meta = (void *)((uintptr_t)r + meta_ofs);

	return 0;
}
