/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <string.h>

#include <rte_string_fns.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_rwlock.h>
#include <rte_tailq.h>

#include "rte_stack.h"
#include "stack_pvt.h"

TAILQ_HEAD(rte_stack_list, rte_tailq_entry);

static struct rte_tailq_elem rte_stack_tailq = {
	.name = RTE_TAILQ_STACK_NAME,
};
EAL_REGISTER_TAILQ(rte_stack_tailq)


static void
rte_stack_init(struct rte_stack *s, unsigned int count, uint32_t flags)
{
	memset(s, 0, sizeof(*s));

	if (flags & RTE_STACK_F_LF)
		rte_stack_lf_init(s, count);
	else
		rte_stack_std_init(s);
}

static ssize_t
rte_stack_get_memsize(unsigned int count, uint32_t flags)
{
	if (flags & RTE_STACK_F_LF)
		return rte_stack_lf_get_memsize(count);
	else
		return rte_stack_std_get_memsize(count);
}

struct rte_stack *
rte_stack_create(const char *name, unsigned int count, int socket_id,
		 uint32_t flags)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	struct rte_stack_list *stack_list;
	const struct rte_memzone *mz;
	struct rte_tailq_entry *te;
	struct rte_stack *s;
	unsigned int sz;
	int ret;

	if (flags & ~(RTE_STACK_F_LF)) {
		STACK_LOG_ERR("Unsupported stack flags %#x\n", flags);
		return NULL;
	}

#ifdef RTE_ARCH_64
	RTE_BUILD_BUG_ON(sizeof(struct rte_stack_lf_head) != 16);
#endif
#if !defined(RTE_STACK_LF_SUPPORTED)
	if (flags & RTE_STACK_F_LF) {
		STACK_LOG_ERR("Lock-free stack is not supported on your platform\n");
		rte_errno = ENOTSUP;
		return NULL;
	}
#endif

	sz = rte_stack_get_memsize(count, flags);

	ret = snprintf(mz_name, sizeof(mz_name), "%s%s",
		       RTE_STACK_MZ_PREFIX, name);
	if (ret < 0 || ret >= (int)sizeof(mz_name)) {
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	te = rte_zmalloc("STACK_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		STACK_LOG_ERR("Cannot reserve memory for tailq\n");
		rte_errno = ENOMEM;
		return NULL;
	}

	rte_mcfg_tailq_write_lock();

	mz = rte_memzone_reserve_aligned(mz_name, sz, socket_id,
					 0, __alignof__(*s));
	if (mz == NULL) {
		STACK_LOG_ERR("Cannot reserve stack memzone!\n");
		rte_mcfg_tailq_write_unlock();
		rte_free(te);
		return NULL;
	}

	s = mz->addr;

	rte_stack_init(s, count, flags);

	/* Store the name for later lookups */
	ret = strlcpy(s->name, name, sizeof(s->name));
	if (ret < 0 || ret >= (int)sizeof(s->name)) {
		rte_mcfg_tailq_write_unlock();

		rte_errno = ENAMETOOLONG;
		rte_free(te);
		rte_memzone_free(mz);
		return NULL;
	}

	s->memzone = mz;
	s->capacity = count;
	s->flags = flags;

	te->data = s;

	stack_list = RTE_TAILQ_CAST(rte_stack_tailq.head, rte_stack_list);

	TAILQ_INSERT_TAIL(stack_list, te, next);

	rte_mcfg_tailq_write_unlock();

	return s;
}

void
rte_stack_free(struct rte_stack *s)
{
	struct rte_stack_list *stack_list;
	struct rte_tailq_entry *te;

	if (s == NULL)
		return;

	stack_list = RTE_TAILQ_CAST(rte_stack_tailq.head, rte_stack_list);
	rte_mcfg_tailq_write_lock();

	/* find out tailq entry */
	TAILQ_FOREACH(te, stack_list, next) {
		if (te->data == s)
			break;
	}

	if (te == NULL) {
		rte_mcfg_tailq_write_unlock();
		return;
	}

	TAILQ_REMOVE(stack_list, te, next);

	rte_mcfg_tailq_write_unlock();

	rte_free(te);

	rte_memzone_free(s->memzone);
}

struct rte_stack *
rte_stack_lookup(const char *name)
{
	struct rte_stack_list *stack_list;
	struct rte_tailq_entry *te;
	struct rte_stack *r = NULL;

	if (name == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	stack_list = RTE_TAILQ_CAST(rte_stack_tailq.head, rte_stack_list);

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, stack_list, next) {
		r = (struct rte_stack *) te->data;
		if (strncmp(name, r->name, RTE_STACK_NAMESIZE) == 0)
			break;
	}

	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return r;
}

RTE_LOG_REGISTER(stack_logtype, lib.stack, NOTICE);
