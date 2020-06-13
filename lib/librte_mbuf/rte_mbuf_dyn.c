/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 6WIND S.A.
 */

#include <sys/queue.h>
#include <stdint.h>
#include <limits.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_tailq.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#define RTE_MBUF_DYN_MZNAME "rte_mbuf_dyn"

struct mbuf_dynfield_elt {
	struct rte_mbuf_dynfield params;
	size_t offset;
};
TAILQ_HEAD(mbuf_dynfield_list, rte_tailq_entry);

static struct rte_tailq_elem mbuf_dynfield_tailq = {
	.name = "RTE_MBUF_DYNFIELD",
};
EAL_REGISTER_TAILQ(mbuf_dynfield_tailq);

struct mbuf_dynflag_elt {
	struct rte_mbuf_dynflag params;
	unsigned int bitnum;
};
TAILQ_HEAD(mbuf_dynflag_list, rte_tailq_entry);

static struct rte_tailq_elem mbuf_dynflag_tailq = {
	.name = "RTE_MBUF_DYNFLAG",
};
EAL_REGISTER_TAILQ(mbuf_dynflag_tailq);

struct mbuf_dyn_shm {
	/**
	 * For each mbuf byte, free_space[i] != 0 if space is free.
	 * The value is the size of the biggest aligned element that
	 * can fit in the zone.
	 */
	uint8_t free_space[sizeof(struct rte_mbuf)];
	/** Bitfield of available flags. */
	uint64_t free_flags;
};
static struct mbuf_dyn_shm *shm;

/* Set the value of free_space[] according to the size and alignment of
 * the free areas. This helps to select the best place when reserving a
 * dynamic field. Assume tailq is locked.
 */
static void
process_score(void)
{
	size_t off, align, size, i;

	/* first, erase previous info */
	for (i = 0; i < sizeof(struct rte_mbuf); i++) {
		if (shm->free_space[i])
			shm->free_space[i] = 1;
	}

	off = 0;
	while (off < sizeof(struct rte_mbuf)) {
		/* get the size of the free zone */
		for (size = 0; (off + size) < sizeof(struct rte_mbuf) &&
			     shm->free_space[off + size]; size++)
			;
		if (size == 0) {
			off++;
			continue;
		}

		/* get the alignment of biggest object that can fit in
		 * the zone at this offset.
		 */
		for (align = 1;
		     (off % (align << 1)) == 0 && (align << 1) <= size;
		     align <<= 1)
			;

		/* save it in free_space[] */
		for (i = off; i < off + align; i++)
			shm->free_space[i] = RTE_MAX(align, shm->free_space[i]);

		off += align;
	}
}

/* Mark the area occupied by a mbuf field as available in the shm. */
#define mark_free(field)						\
	memset(&shm->free_space[offsetof(struct rte_mbuf, field)],	\
		1, sizeof(((struct rte_mbuf *)0)->field))

/* Allocate and initialize the shared memory. Assume tailq is locked */
static int
init_shared_mem(void)
{
	const struct rte_memzone *mz;
	uint64_t mask;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve_aligned(RTE_MBUF_DYN_MZNAME,
						sizeof(struct mbuf_dyn_shm),
						SOCKET_ID_ANY, 0,
						RTE_CACHE_LINE_SIZE);
	} else {
		mz = rte_memzone_lookup(RTE_MBUF_DYN_MZNAME);
	}
	if (mz == NULL)
		return -1;

	shm = mz->addr;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* init free_space, keep it sync'd with
		 * rte_mbuf_dynfield_copy().
		 */
		memset(shm, 0, sizeof(*shm));
		mark_free(dynfield1);

		/* init free_flags */
		for (mask = PKT_FIRST_FREE; mask <= PKT_LAST_FREE; mask <<= 1)
			shm->free_flags |= mask;

		process_score();
	}

	return 0;
}

/* check if this offset can be used */
static int
check_offset(size_t offset, size_t size, size_t align)
{
	size_t i;

	if ((offset & (align - 1)) != 0)
		return -1;
	if (offset + size > sizeof(struct rte_mbuf))
		return -1;

	for (i = 0; i < size; i++) {
		if (!shm->free_space[i + offset])
			return -1;
	}

	return 0;
}

/* assume tailq is locked */
static struct mbuf_dynfield_elt *
__mbuf_dynfield_lookup(const char *name)
{
	struct mbuf_dynfield_list *mbuf_dynfield_list;
	struct mbuf_dynfield_elt *mbuf_dynfield;
	struct rte_tailq_entry *te;

	mbuf_dynfield_list = RTE_TAILQ_CAST(
		mbuf_dynfield_tailq.head, mbuf_dynfield_list);

	TAILQ_FOREACH(te, mbuf_dynfield_list, next) {
		mbuf_dynfield = (struct mbuf_dynfield_elt *)te->data;
		if (strcmp(name, mbuf_dynfield->params.name) == 0)
			break;
	}

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return mbuf_dynfield;
}

int
rte_mbuf_dynfield_lookup(const char *name, struct rte_mbuf_dynfield *params)
{
	struct mbuf_dynfield_elt *mbuf_dynfield;

	if (shm == NULL) {
		rte_errno = ENOENT;
		return -1;
	}

	rte_mcfg_tailq_read_lock();
	mbuf_dynfield = __mbuf_dynfield_lookup(name);
	rte_mcfg_tailq_read_unlock();

	if (mbuf_dynfield == NULL) {
		rte_errno = ENOENT;
		return -1;
	}

	if (params != NULL)
		memcpy(params, &mbuf_dynfield->params, sizeof(*params));

	return mbuf_dynfield->offset;
}

static int mbuf_dynfield_cmp(const struct rte_mbuf_dynfield *params1,
		const struct rte_mbuf_dynfield *params2)
{
	if (strcmp(params1->name, params2->name))
		return -1;
	if (params1->size != params2->size)
		return -1;
	if (params1->align != params2->align)
		return -1;
	if (params1->flags != params2->flags)
		return -1;
	return 0;
}

/* assume tailq is locked */
static int
__rte_mbuf_dynfield_register_offset(const struct rte_mbuf_dynfield *params,
				size_t req)
{
	struct mbuf_dynfield_list *mbuf_dynfield_list;
	struct mbuf_dynfield_elt *mbuf_dynfield = NULL;
	struct rte_tailq_entry *te = NULL;
	unsigned int best_zone = UINT_MAX;
	size_t i, offset;
	int ret;

	if (shm == NULL && init_shared_mem() < 0)
		return -1;

	mbuf_dynfield = __mbuf_dynfield_lookup(params->name);
	if (mbuf_dynfield != NULL) {
		if (req != SIZE_MAX && req != mbuf_dynfield->offset) {
			rte_errno = EEXIST;
			return -1;
		}
		if (mbuf_dynfield_cmp(params, &mbuf_dynfield->params) < 0) {
			rte_errno = EEXIST;
			return -1;
		}
		return mbuf_dynfield->offset;
	}

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		rte_errno = EPERM;
		return -1;
	}

	if (req == SIZE_MAX) {
		/* Find the best place to put this field: we search the
		 * lowest value of shm->free_space[offset]: the zones
		 * containing room for larger fields are kept for later.
		 */
		for (offset = 0;
		     offset < sizeof(struct rte_mbuf);
		     offset++) {
			if (check_offset(offset, params->size,
						params->align) == 0 &&
					shm->free_space[offset] < best_zone) {
				best_zone = shm->free_space[offset];
				req = offset;
			}
		}
		if (req == SIZE_MAX) {
			rte_errno = ENOENT;
			return -1;
		}
	} else {
		if (check_offset(req, params->size, params->align) < 0) {
			rte_errno = EBUSY;
			return -1;
		}
	}

	offset = req;
	mbuf_dynfield_list = RTE_TAILQ_CAST(
		mbuf_dynfield_tailq.head, mbuf_dynfield_list);

	te = rte_zmalloc("MBUF_DYNFIELD_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		rte_errno = ENOMEM;
		return -1;
	}

	mbuf_dynfield = rte_zmalloc("mbuf_dynfield", sizeof(*mbuf_dynfield), 0);
	if (mbuf_dynfield == NULL) {
		rte_free(te);
		rte_errno = ENOMEM;
		return -1;
	}

	ret = strlcpy(mbuf_dynfield->params.name, params->name,
		sizeof(mbuf_dynfield->params.name));
	if (ret < 0 || ret >= (int)sizeof(mbuf_dynfield->params.name)) {
		rte_errno = ENAMETOOLONG;
		rte_free(mbuf_dynfield);
		rte_free(te);
		return -1;
	}
	memcpy(&mbuf_dynfield->params, params, sizeof(mbuf_dynfield->params));
	mbuf_dynfield->offset = offset;
	te->data = mbuf_dynfield;

	TAILQ_INSERT_TAIL(mbuf_dynfield_list, te, next);

	for (i = offset; i < offset + params->size; i++)
		shm->free_space[i] = 0;
	process_score();

	RTE_LOG(DEBUG, MBUF, "Registered dynamic field %s (sz=%zu, al=%zu, fl=0x%x) -> %zd\n",
		params->name, params->size, params->align, params->flags,
		offset);

	return offset;
}

int
rte_mbuf_dynfield_register_offset(const struct rte_mbuf_dynfield *params,
				size_t req)
{
	int ret;

	if (params->size >= sizeof(struct rte_mbuf)) {
		rte_errno = EINVAL;
		return -1;
	}
	if (!rte_is_power_of_2(params->align)) {
		rte_errno = EINVAL;
		return -1;
	}
	if (params->flags != 0) {
		rte_errno = EINVAL;
		return -1;
	}

	rte_mcfg_tailq_write_lock();
	ret = __rte_mbuf_dynfield_register_offset(params, req);
	rte_mcfg_tailq_write_unlock();

	return ret;
}

int
rte_mbuf_dynfield_register(const struct rte_mbuf_dynfield *params)
{
	return rte_mbuf_dynfield_register_offset(params, SIZE_MAX);
}

/* assume tailq is locked */
static struct mbuf_dynflag_elt *
__mbuf_dynflag_lookup(const char *name)
{
	struct mbuf_dynflag_list *mbuf_dynflag_list;
	struct mbuf_dynflag_elt *mbuf_dynflag;
	struct rte_tailq_entry *te;

	mbuf_dynflag_list = RTE_TAILQ_CAST(
		mbuf_dynflag_tailq.head, mbuf_dynflag_list);

	TAILQ_FOREACH(te, mbuf_dynflag_list, next) {
		mbuf_dynflag = (struct mbuf_dynflag_elt *)te->data;
		if (strncmp(name, mbuf_dynflag->params.name,
				RTE_MBUF_DYN_NAMESIZE) == 0)
			break;
	}

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return mbuf_dynflag;
}

int
rte_mbuf_dynflag_lookup(const char *name,
			struct rte_mbuf_dynflag *params)
{
	struct mbuf_dynflag_elt *mbuf_dynflag;

	if (shm == NULL) {
		rte_errno = ENOENT;
		return -1;
	}

	rte_mcfg_tailq_read_lock();
	mbuf_dynflag = __mbuf_dynflag_lookup(name);
	rte_mcfg_tailq_read_unlock();

	if (mbuf_dynflag == NULL) {
		rte_errno = ENOENT;
		return -1;
	}

	if (params != NULL)
		memcpy(params, &mbuf_dynflag->params, sizeof(*params));

	return mbuf_dynflag->bitnum;
}

static int mbuf_dynflag_cmp(const struct rte_mbuf_dynflag *params1,
		const struct rte_mbuf_dynflag *params2)
{
	if (strcmp(params1->name, params2->name))
		return -1;
	if (params1->flags != params2->flags)
		return -1;
	return 0;
}

/* assume tailq is locked */
static int
__rte_mbuf_dynflag_register_bitnum(const struct rte_mbuf_dynflag *params,
				unsigned int req)
{
	struct mbuf_dynflag_list *mbuf_dynflag_list;
	struct mbuf_dynflag_elt *mbuf_dynflag = NULL;
	struct rte_tailq_entry *te = NULL;
	unsigned int bitnum;
	int ret;

	if (shm == NULL && init_shared_mem() < 0)
		return -1;

	mbuf_dynflag = __mbuf_dynflag_lookup(params->name);
	if (mbuf_dynflag != NULL) {
		if (req != UINT_MAX && req != mbuf_dynflag->bitnum) {
			rte_errno = EEXIST;
			return -1;
		}
		if (mbuf_dynflag_cmp(params, &mbuf_dynflag->params) < 0) {
			rte_errno = EEXIST;
			return -1;
		}
		return mbuf_dynflag->bitnum;
	}

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		rte_errno = EPERM;
		return -1;
	}

	if (req == UINT_MAX) {
		if (shm->free_flags == 0) {
			rte_errno = ENOENT;
			return -1;
		}
		bitnum = rte_bsf64(shm->free_flags);
	} else {
		if ((shm->free_flags & (1ULL << req)) == 0) {
			rte_errno = EBUSY;
			return -1;
		}
		bitnum = req;
	}

	mbuf_dynflag_list = RTE_TAILQ_CAST(
		mbuf_dynflag_tailq.head, mbuf_dynflag_list);

	te = rte_zmalloc("MBUF_DYNFLAG_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		rte_errno = ENOMEM;
		return -1;
	}

	mbuf_dynflag = rte_zmalloc("mbuf_dynflag", sizeof(*mbuf_dynflag), 0);
	if (mbuf_dynflag == NULL) {
		rte_free(te);
		rte_errno = ENOMEM;
		return -1;
	}

	ret = strlcpy(mbuf_dynflag->params.name, params->name,
		sizeof(mbuf_dynflag->params.name));
	if (ret < 0 || ret >= (int)sizeof(mbuf_dynflag->params.name)) {
		rte_free(mbuf_dynflag);
		rte_free(te);
		rte_errno = ENAMETOOLONG;
		return -1;
	}
	mbuf_dynflag->bitnum = bitnum;
	te->data = mbuf_dynflag;

	TAILQ_INSERT_TAIL(mbuf_dynflag_list, te, next);

	shm->free_flags &= ~(1ULL << bitnum);

	RTE_LOG(DEBUG, MBUF, "Registered dynamic flag %s (fl=0x%x) -> %u\n",
		params->name, params->flags, bitnum);

	return bitnum;
}

int
rte_mbuf_dynflag_register_bitnum(const struct rte_mbuf_dynflag *params,
				unsigned int req)
{
	int ret;

	if (req >= RTE_SIZEOF_FIELD(struct rte_mbuf, ol_flags) * CHAR_BIT &&
			req != UINT_MAX) {
		rte_errno = EINVAL;
		return -1;
	}

	rte_mcfg_tailq_write_lock();
	ret = __rte_mbuf_dynflag_register_bitnum(params, req);
	rte_mcfg_tailq_write_unlock();

	return ret;
}

int
rte_mbuf_dynflag_register(const struct rte_mbuf_dynflag *params)
{
	return rte_mbuf_dynflag_register_bitnum(params, UINT_MAX);
}

void rte_mbuf_dyn_dump(FILE *out)
{
	struct mbuf_dynfield_list *mbuf_dynfield_list;
	struct mbuf_dynfield_elt *dynfield;
	struct mbuf_dynflag_list *mbuf_dynflag_list;
	struct mbuf_dynflag_elt *dynflag;
	struct rte_tailq_entry *te;
	size_t i;

	rte_mcfg_tailq_write_lock();
	init_shared_mem();
	fprintf(out, "Reserved fields:\n");
	mbuf_dynfield_list = RTE_TAILQ_CAST(
		mbuf_dynfield_tailq.head, mbuf_dynfield_list);
	TAILQ_FOREACH(te, mbuf_dynfield_list, next) {
		dynfield = (struct mbuf_dynfield_elt *)te->data;
		fprintf(out, "  name=%s offset=%zd size=%zd align=%zd flags=%x\n",
			dynfield->params.name, dynfield->offset,
			dynfield->params.size, dynfield->params.align,
			dynfield->params.flags);
	}
	fprintf(out, "Reserved flags:\n");
	mbuf_dynflag_list = RTE_TAILQ_CAST(
		mbuf_dynflag_tailq.head, mbuf_dynflag_list);
	TAILQ_FOREACH(te, mbuf_dynflag_list, next) {
		dynflag = (struct mbuf_dynflag_elt *)te->data;
		fprintf(out, "  name=%s bitnum=%u flags=%x\n",
			dynflag->params.name, dynflag->bitnum,
			dynflag->params.flags);
	}
	fprintf(out, "Free space in mbuf (0 = occupied, value = free zone alignment):\n");
	for (i = 0; i < sizeof(struct rte_mbuf); i++) {
		if ((i % 8) == 0)
			fprintf(out, "  %4.4zx: ", i);
		fprintf(out, "%2.2x%s", shm->free_space[i],
			(i % 8 != 7) ? " " : "\n");
	}
	rte_mcfg_tailq_write_unlock();
}
