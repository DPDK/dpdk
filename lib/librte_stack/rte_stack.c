/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <string.h>

#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_rwlock.h>
#include <rte_tailq.h>

#include "rte_stack.h"
#include "rte_stack_pvt.h"

int stack_logtype;

TAILQ_HEAD(rte_stack_list, rte_tailq_entry);

static struct rte_tailq_elem rte_stack_tailq = {
	.name = RTE_TAILQ_STACK_NAME,
};
EAL_REGISTER_TAILQ(rte_stack_tailq)

static void
rte_stack_init(struct rte_stack *s)
{
	memset(s, 0, sizeof(*s));

	rte_stack_std_init(s);
}

static ssize_t
rte_stack_get_memsize(unsigned int count)
{
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

	RTE_SET_USED(flags);

	sz = rte_stack_get_memsize(count);

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

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	mz = rte_memzone_reserve_aligned(mz_name, sz, socket_id,
					 0, __alignof__(*s));
	if (mz == NULL) {
		STACK_LOG_ERR("Cannot reserve stack memzone!\n");
		rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
		rte_free(te);
		return NULL;
	}

	s = mz->addr;

	rte_stack_init(s);

	/* Store the name for later lookups */
	ret = snprintf(s->name, sizeof(s->name), "%s", name);
	if (ret < 0 || ret >= (int)sizeof(s->name)) {
		rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

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

	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

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
	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* find out tailq entry */
	TAILQ_FOREACH(te, stack_list, next) {
		if (te->data == s)
			break;
	}

	if (te == NULL) {
		rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
		return;
	}

	TAILQ_REMOVE(stack_list, te, next);

	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

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

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);

	TAILQ_FOREACH(te, stack_list, next) {
		r = (struct rte_stack *) te->data;
		if (strncmp(name, r->name, RTE_STACK_NAMESIZE) == 0)
			break;
	}

	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return r;
}

RTE_INIT(librte_stack_init_log)
{
	stack_logtype = rte_log_register("lib.stack");
	if (stack_logtype >= 0)
		rte_log_set_level(stack_logtype, RTE_LOG_NOTICE);
}
