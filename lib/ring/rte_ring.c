/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2015 Intel Corporation
 * Copyright (c) 2007,2008 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_tailq.h>
#include <rte_telemetry.h>

#include "rte_ring.h"
#include "rte_ring_elem.h"

RTE_LOG_REGISTER_DEFAULT(ring_logtype, INFO);
#define RTE_LOGTYPE_RING ring_logtype
#define RING_LOG(level, ...) \
	RTE_LOG_LINE(level, RING, "" __VA_ARGS__)

TAILQ_HEAD(rte_ring_list, rte_tailq_entry);

static struct rte_tailq_elem rte_ring_tailq = {
	.name = RTE_TAILQ_RING_NAME,
};
EAL_REGISTER_TAILQ(rte_ring_tailq)

/* mask of all valid flag values to ring_create() */
#define RING_F_MASK (RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ | \
		     RING_F_MP_RTS_ENQ | RING_F_MC_RTS_DEQ |	       \
		     RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ)

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

/* by default set head/tail distance as 1/8 of ring capacity */
#define HTD_MAX_DEF	8

/* return the size of memory occupied by a ring */
RTE_EXPORT_SYMBOL(rte_ring_get_memsize_elem)
ssize_t
rte_ring_get_memsize_elem(unsigned int esize, unsigned int count)
{
	ssize_t sz;

	/* Check if element size is a multiple of 4B */
	if (esize % 4 != 0) {
		RING_LOG(ERR, "element size is not a multiple of 4");

		return -EINVAL;
	}

	/* count must be a power of 2 */
	if ((!POWEROF2(count)) || (count > RTE_RING_SZ_MASK )) {
		RING_LOG(ERR,
			"Requested number of elements is invalid, must be power of 2, and not exceed %u",
			RTE_RING_SZ_MASK);

		return -EINVAL;
	}

	sz = sizeof(struct rte_ring) + (ssize_t)count * esize;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);
	return sz;
}

/* return the size of memory occupied by a ring */
RTE_EXPORT_SYMBOL(rte_ring_get_memsize)
ssize_t
rte_ring_get_memsize(unsigned int count)
{
	return rte_ring_get_memsize_elem(sizeof(void *), count);
}

/*
 * internal helper function to reset prod/cons head-tail values.
 */
static void
reset_headtail(void *p)
{
	struct rte_ring_headtail *ht;
	struct rte_ring_hts_headtail *ht_hts;
	struct rte_ring_rts_headtail *ht_rts;

	ht = p;
	ht_hts = p;
	ht_rts = p;

	switch (ht->sync_type) {
	case RTE_RING_SYNC_MT:
	case RTE_RING_SYNC_ST:
		ht->head = 0;
		ht->tail = 0;
		break;
	case RTE_RING_SYNC_MT_RTS:
		ht_rts->head.raw = 0;
		ht_rts->tail.raw = 0;
		break;
	case RTE_RING_SYNC_MT_HTS:
		ht_hts->ht.raw = 0;
		break;
	default:
		/* unknown sync mode */
		RTE_ASSERT(0);
	}
}

RTE_EXPORT_SYMBOL(rte_ring_reset)
void
rte_ring_reset(struct rte_ring *r)
{
	reset_headtail(&r->prod);
	reset_headtail(&r->cons);
}

/*
 * helper function, calculates sync_type values for prod and cons
 * based on input flags. Returns zero at success or negative
 * errno value otherwise.
 */
static int
get_sync_type(uint32_t flags, enum rte_ring_sync_type *prod_st,
	enum rte_ring_sync_type *cons_st)
{
	static const uint32_t prod_st_flags =
		(RING_F_SP_ENQ | RING_F_MP_RTS_ENQ | RING_F_MP_HTS_ENQ);
	static const uint32_t cons_st_flags =
		(RING_F_SC_DEQ | RING_F_MC_RTS_DEQ | RING_F_MC_HTS_DEQ);

	switch (flags & prod_st_flags) {
	case 0:
		*prod_st = RTE_RING_SYNC_MT;
		break;
	case RING_F_SP_ENQ:
		*prod_st = RTE_RING_SYNC_ST;
		break;
	case RING_F_MP_RTS_ENQ:
		*prod_st = RTE_RING_SYNC_MT_RTS;
		break;
	case RING_F_MP_HTS_ENQ:
		*prod_st = RTE_RING_SYNC_MT_HTS;
		break;
	default:
		return -EINVAL;
	}

	switch (flags & cons_st_flags) {
	case 0:
		*cons_st = RTE_RING_SYNC_MT;
		break;
	case RING_F_SC_DEQ:
		*cons_st = RTE_RING_SYNC_ST;
		break;
	case RING_F_MC_RTS_DEQ:
		*cons_st = RTE_RING_SYNC_MT_RTS;
		break;
	case RING_F_MC_HTS_DEQ:
		*cons_st = RTE_RING_SYNC_MT_HTS;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

RTE_EXPORT_SYMBOL(rte_ring_init)
int
rte_ring_init(struct rte_ring *r, const char *name, unsigned int count,
	unsigned int flags)
{
	int ret;

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(struct rte_ring) &
			  RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_ring, cons) &
			  RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_ring, prod) &
			  RTE_CACHE_LINE_MASK) != 0);

	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, sync_type) !=
		offsetof(struct rte_ring_hts_headtail, sync_type));
	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, tail) !=
		offsetof(struct rte_ring_hts_headtail, ht.pos.tail));

	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, sync_type) !=
		offsetof(struct rte_ring_rts_headtail, sync_type));
	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, tail) !=
		offsetof(struct rte_ring_rts_headtail, tail.val.pos));

	/* future proof flags, only allow supported values */
	if (flags & ~RING_F_MASK) {
		RING_LOG(ERR,
			"Unsupported flags requested %#x", flags);
		return -EINVAL;
	}

	/* init the ring structure */
	memset(r, 0, sizeof(*r));
	ret = strlcpy(r->name, name, sizeof(r->name));
	if (ret < 0 || ret >= (int)sizeof(r->name))
		return -ENAMETOOLONG;
	r->flags = flags;
	ret = get_sync_type(flags, &r->prod.sync_type, &r->cons.sync_type);
	if (ret != 0)
		return ret;

	if (flags & RING_F_EXACT_SZ) {
		r->size = rte_align32pow2(count + 1);
		r->mask = r->size - 1;
		r->capacity = count;
	} else {
		if ((!POWEROF2(count)) || (count > RTE_RING_SZ_MASK)) {
			RING_LOG(ERR,
				"Requested size is invalid, must be power of 2, and not exceed the size limit %u",
				RTE_RING_SZ_MASK);
			return -EINVAL;
		}
		r->size = count;
		r->mask = count - 1;
		r->capacity = r->mask;
	}

	/* set default values for head-tail distance */
	if (flags & RING_F_MP_RTS_ENQ)
		rte_ring_set_prod_htd_max(r, r->capacity / HTD_MAX_DEF);
	if (flags & RING_F_MC_RTS_DEQ)
		rte_ring_set_cons_htd_max(r, r->capacity / HTD_MAX_DEF);

	return 0;
}

/* create the ring for a given element size */
RTE_EXPORT_SYMBOL(rte_ring_create_elem)
struct rte_ring *
rte_ring_create_elem(const char *name, unsigned int esize, unsigned int count,
		int socket_id, unsigned int flags)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct rte_ring *r;
	struct rte_tailq_entry *te;
	const struct rte_memzone *mz;
	ssize_t ring_size;
	int mz_flags = 0;
	struct rte_ring_list* ring_list = NULL;
	const unsigned int requested_count = count;
	int ret;

	ring_list = RTE_TAILQ_CAST(rte_ring_tailq.head, rte_ring_list);

	/* for an exact size ring, round up from count to a power of two */
	if (flags & RING_F_EXACT_SZ)
		count = rte_align32pow2(count + 1);

	ring_size = rte_ring_get_memsize_elem(esize, count);
	if (ring_size < 0) {
		rte_errno = -ring_size;
		return NULL;
	}

	ret = snprintf(mz_name, sizeof(mz_name), "%s%s",
		RTE_RING_MZ_PREFIX, name);
	if (ret < 0 || ret >= (int)sizeof(mz_name)) {
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	te = rte_zmalloc("RING_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RING_LOG(ERR, "Cannot reserve memory for tailq");
		rte_errno = ENOMEM;
		return NULL;
	}

	rte_mcfg_tailq_write_lock();

	/* reserve a memory zone for this ring. If we can't get rte_config or
	 * we are secondary process, the memzone_reserve function will set
	 * rte_errno for us appropriately - hence no check in this function
	 */
	mz = rte_memzone_reserve_aligned(mz_name, ring_size, socket_id,
					 mz_flags, alignof(typeof(*r)));
	if (mz != NULL) {
		r = mz->addr;
		/* no need to check return value here, we already checked the
		 * arguments above */
		rte_ring_init(r, name, requested_count, flags);

		te->data = (void *) r;
		r->memzone = mz;

		TAILQ_INSERT_TAIL(ring_list, te, next);
	} else {
		r = NULL;
		RING_LOG(ERR, "Cannot reserve memory");
		rte_free(te);
	}
	rte_mcfg_tailq_write_unlock();

	return r;
}

/* create the ring */
RTE_EXPORT_SYMBOL(rte_ring_create)
struct rte_ring *
rte_ring_create(const char *name, unsigned int count, int socket_id,
		unsigned int flags)
{
	return rte_ring_create_elem(name, sizeof(void *), count, socket_id,
		flags);
}

/* free the ring */
RTE_EXPORT_SYMBOL(rte_ring_free)
void
rte_ring_free(struct rte_ring *r)
{
	struct rte_ring_list *ring_list = NULL;
	struct rte_tailq_entry *te;

	if (r == NULL)
		return;

	/*
	 * Ring was not created with rte_ring_create,
	 * therefore, there is no memzone to free.
	 */
	if (r->memzone == NULL) {
		RING_LOG(ERR,
			"Cannot free ring, not created with rte_ring_create()");
		return;
	}

	ring_list = RTE_TAILQ_CAST(rte_ring_tailq.head, rte_ring_list);
	rte_mcfg_tailq_write_lock();

	/* find out tailq entry */
	TAILQ_FOREACH(te, ring_list, next) {
		if (te->data == (void *) r)
			break;
	}

	if (te == NULL) {
		rte_mcfg_tailq_write_unlock();
		return;
	}

	TAILQ_REMOVE(ring_list, te, next);

	rte_mcfg_tailq_write_unlock();

	if (rte_memzone_free(r->memzone) != 0)
		RING_LOG(ERR, "Cannot free memory");

	rte_free(te);
}

static const char *
ring_get_sync_type(const enum rte_ring_sync_type st)
{
	switch (st) {
	case RTE_RING_SYNC_ST:
		return "ST";
	case RTE_RING_SYNC_MT:
		return "MT";
	case RTE_RING_SYNC_MT_RTS:
		return "MT_RTS";
	case RTE_RING_SYNC_MT_HTS:
		return "MT_HTS";
	default:
		return "unknown";
	}
}

static void
ring_dump_ht_headtail(FILE *f, const char *prefix,
		const struct rte_ring_headtail *ht)
{
	fprintf(f, "%ssync_type=%s\n", prefix,
			ring_get_sync_type(ht->sync_type));
	fprintf(f, "%shead=%"PRIu32"\n", prefix, ht->head);
	fprintf(f, "%stail=%"PRIu32"\n", prefix, ht->tail);
}

static void
ring_dump_rts_headtail(FILE *f, const char *prefix,
		const struct rte_ring_rts_headtail *rts)
{
	fprintf(f, "%ssync_type=%s\n", prefix,
			ring_get_sync_type(rts->sync_type));
	fprintf(f, "%shead.pos=%"PRIu32"\n", prefix, rts->head.val.pos);
	fprintf(f, "%shead.cnt=%"PRIu32"\n", prefix, rts->head.val.cnt);
	fprintf(f, "%stail.pos=%"PRIu32"\n", prefix, rts->tail.val.pos);
	fprintf(f, "%stail.cnt=%"PRIu32"\n", prefix, rts->tail.val.cnt);
	fprintf(f, "%shtd_max=%"PRIu32"\n", prefix, rts->htd_max);
}

static void
ring_dump_hts_headtail(FILE *f, const char *prefix,
		const struct rte_ring_hts_headtail *hts)
{
	fprintf(f, "%ssync_type=%s\n", prefix,
			ring_get_sync_type(hts->sync_type));
	fprintf(f, "%shead=%"PRIu32"\n", prefix, hts->ht.pos.head);
	fprintf(f, "%stail=%"PRIu32"\n", prefix, hts->ht.pos.tail);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ring_headtail_dump, 25.03)
void
rte_ring_headtail_dump(FILE *f, const char *prefix,
		const struct rte_ring_headtail *r)
{
	if (f == NULL || r == NULL)
		return;

	prefix = (prefix != NULL) ? prefix : "";

	switch (r->sync_type) {
	case RTE_RING_SYNC_ST:
	case RTE_RING_SYNC_MT:
		ring_dump_ht_headtail(f, prefix, r);
		break;
	case RTE_RING_SYNC_MT_RTS:
		ring_dump_rts_headtail(f, prefix,
				(const struct rte_ring_rts_headtail *)r);
		break;
	case RTE_RING_SYNC_MT_HTS:
		ring_dump_hts_headtail(f, prefix,
				(const struct rte_ring_hts_headtail *)r);
		break;
	default:
		RING_LOG(ERR, "Invalid ring sync type detected");
	}
}

/* dump the status of the ring on the console */
RTE_EXPORT_SYMBOL(rte_ring_dump)
void
rte_ring_dump(FILE *f, const struct rte_ring *r)
{
	if (f == NULL || r == NULL)
		return;

	fprintf(f, "ring <%s>@%p\n", r->name, r);
	fprintf(f, "  flags=%x\n", r->flags);
	fprintf(f, "  size=%"PRIu32"\n", r->size);
	fprintf(f, "  capacity=%"PRIu32"\n", r->capacity);
	fprintf(f, "  used=%u\n", rte_ring_count(r));
	fprintf(f, "  avail=%u\n", rte_ring_free_count(r));

	rte_ring_headtail_dump(f, "  cons.", &(r->cons));
	rte_ring_headtail_dump(f, "  prod.", &(r->prod));
}

/* dump the status of all rings on the console */
RTE_EXPORT_SYMBOL(rte_ring_list_dump)
void
rte_ring_list_dump(FILE *f)
{
	const struct rte_tailq_entry *te;
	struct rte_ring_list *ring_list;

	ring_list = RTE_TAILQ_CAST(rte_ring_tailq.head, rte_ring_list);

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, ring_list, next) {
		rte_ring_dump(f, (struct rte_ring *) te->data);
	}

	rte_mcfg_tailq_read_unlock();
}

/* search a ring from its name */
RTE_EXPORT_SYMBOL(rte_ring_lookup)
struct rte_ring *
rte_ring_lookup(const char *name)
{
	struct rte_tailq_entry *te;
	struct rte_ring *r = NULL;
	struct rte_ring_list *ring_list;

	ring_list = RTE_TAILQ_CAST(rte_ring_tailq.head, rte_ring_list);

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, ring_list, next) {
		r = (struct rte_ring *) te->data;
		if (strncmp(name, r->name, RTE_RING_NAMESIZE) == 0)
			break;
	}

	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return r;
}

static void
ring_walk(void (*func)(struct rte_ring *, void *), void *arg)
{
	struct rte_ring_list *ring_list;
	struct rte_tailq_entry *tailq_entry;

	ring_list = RTE_TAILQ_CAST(rte_ring_tailq.head, rte_ring_list);
	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(tailq_entry, ring_list, next) {
		(*func)((struct rte_ring *) tailq_entry->data, arg);
	}

	rte_mcfg_tailq_read_unlock();
}

static void
ring_list_cb(struct rte_ring *r, void *arg)
{
	struct rte_tel_data *d = (struct rte_tel_data *)arg;

	rte_tel_data_add_array_string(d, r->name);
}

static int
ring_handle_list(const char *cmd __rte_unused,
		const char *params __rte_unused, struct rte_tel_data *d)
{
	rte_tel_data_start_array(d, RTE_TEL_STRING_VAL);
	ring_walk(ring_list_cb, d);
	return 0;
}

static const char *
ring_prod_sync_type_to_name(struct rte_ring *r)
{
	switch (r->prod.sync_type) {
	case RTE_RING_SYNC_MT:
		return "MP";
	case RTE_RING_SYNC_ST:
		return "SP";
	case RTE_RING_SYNC_MT_RTS:
		return "MP_RTS";
	case RTE_RING_SYNC_MT_HTS:
		return "MP_HTS";
	default:
		return "Unknown";
	}
}

static const char *
ring_cons_sync_type_to_name(struct rte_ring *r)
{
	switch (r->cons.sync_type) {
	case RTE_RING_SYNC_MT:
		return "MC";
	case RTE_RING_SYNC_ST:
		return "SC";
	case RTE_RING_SYNC_MT_RTS:
		return "MC_RTS";
	case RTE_RING_SYNC_MT_HTS:
		return "MC_HTS";
	default:
		return "Unknown";
	}
}

struct ring_info_cb_arg {
	char *ring_name;
	struct rte_tel_data *d;
};

static void
ring_info_cb(struct rte_ring *r, void *arg)
{
	struct ring_info_cb_arg *ring_arg = (struct ring_info_cb_arg *)arg;
	struct rte_tel_data *d = ring_arg->d;
	const struct rte_memzone *mz;

	if (strncmp(r->name, ring_arg->ring_name, RTE_RING_NAMESIZE))
		return;

	rte_tel_data_add_dict_string(d, "name", r->name);
	rte_tel_data_add_dict_int(d, "socket", r->memzone->socket_id);
	rte_tel_data_add_dict_int(d, "flags", r->flags);
	rte_tel_data_add_dict_string(d, "producer_type",
		ring_prod_sync_type_to_name(r));
	rte_tel_data_add_dict_string(d, "consumer_type",
		ring_cons_sync_type_to_name(r));
	rte_tel_data_add_dict_uint(d, "size", r->size);
	rte_tel_data_add_dict_uint_hex(d, "mask", r->mask, 0);
	rte_tel_data_add_dict_uint(d, "capacity", r->capacity);
	rte_tel_data_add_dict_uint(d, "used_count", rte_ring_count(r));

	mz = r->memzone;
	if (mz == NULL)
		return;
	rte_tel_data_add_dict_string(d, "mz_name", mz->name);
	rte_tel_data_add_dict_uint(d, "mz_len", mz->len);
	rte_tel_data_add_dict_uint(d, "mz_hugepage_sz", mz->hugepage_sz);
	rte_tel_data_add_dict_int(d, "mz_socket_id", mz->socket_id);
	rte_tel_data_add_dict_uint_hex(d, "mz_flags", mz->flags, 0);
}

static int
ring_handle_info(const char *cmd __rte_unused, const char *params,
		struct rte_tel_data *d)
{
	char name[RTE_RING_NAMESIZE] = {0};
	struct ring_info_cb_arg ring_arg;

	if (params == NULL || strlen(params) == 0 ||
		strlen(params) >= RTE_RING_NAMESIZE)
		return -EINVAL;

	rte_strlcpy(name, params, RTE_RING_NAMESIZE);

	ring_arg.ring_name = name;
	ring_arg.d = d;

	rte_tel_data_start_dict(d);
	ring_walk(ring_info_cb, &ring_arg);

	return 0;
}

RTE_INIT(ring_init_telemetry)
{
	rte_telemetry_register_cmd("/ring/list", ring_handle_list,
		"Returns list of available rings. Takes no parameters");
	rte_telemetry_register_cmd("/ring/info", ring_handle_info,
		"Returns ring info. Parameters: ring_name.");
}
