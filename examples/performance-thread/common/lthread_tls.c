/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sched.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_ring.h>
#include <rte_atomic_64.h>

#include "lthread_tls.h"
#include "lthread_queue.h"
#include "lthread_objcache.h"
#include "lthread_sched.h"

static struct rte_ring *key_pool;
static uint64_t key_pool_init;

/* needed to cause section start and end to be defined */
RTE_DEFINE_PER_LTHREAD(void *, dummy);

static struct lthread_key key_table[LTHREAD_MAX_KEYS];

void lthread_tls_ctor(void) __attribute__((constructor));

void lthread_tls_ctor(void)
{
	key_pool = NULL;
	key_pool_init = 0;
}

/*
 * Initialize a pool of keys
 * These are unique tokens that can be obtained by threads
 * calling lthread_key_create()
 */
void _lthread_key_pool_init(void)
{
	static struct rte_ring *pool;
	struct lthread_key *new_key;
	char name[MAX_LTHREAD_NAME_SIZE];

	bzero(key_table, sizeof(key_table));

	/* only one lcore should do this */
	if (rte_atomic64_cmpset(&key_pool_init, 0, 1)) {

		snprintf(name,
			MAX_LTHREAD_NAME_SIZE,
			"lthread_key_pool_%d",
			getpid());

		pool = rte_ring_create(name,
					LTHREAD_MAX_KEYS, 0, 0);
		RTE_ASSERT(pool);

		int i;

		for (i = 1; i < LTHREAD_MAX_KEYS; i++) {
			new_key = &key_table[i];
			rte_ring_mp_enqueue((struct rte_ring *)pool,
						(void *)new_key);
		}
		key_pool = pool;
	}
	/* other lcores wait here till done */
	while (key_pool == NULL) {
		rte_compiler_barrier();
		sched_yield();
	};
}

/*
 * Create a key
 * this means getting a key from the the pool
 */
int lthread_key_create(unsigned int *key, tls_destructor_func destructor)
{
	if (key == NULL)
		return POSIX_ERRNO(EINVAL);

	struct lthread_key *new_key;

	if (rte_ring_mc_dequeue((struct rte_ring *)key_pool, (void **)&new_key)
	    == 0) {
		new_key->destructor = destructor;
		*key = (new_key - key_table);

		return 0;
	}
	return POSIX_ERRNO(EAGAIN);
}


/*
 * Delete a key
 */
int lthread_key_delete(unsigned int k)
{
	struct lthread_key *key;

	key = (struct lthread_key *) &key_table[k];

	if (k > LTHREAD_MAX_KEYS)
		return POSIX_ERRNO(EINVAL);

	key->destructor = NULL;
	rte_ring_mp_enqueue((struct rte_ring *)key_pool,
					(void *)key);
	return 0;
}



/*
 * Break association for all keys in use by this thread
 * invoke the destructor if available.
 * Since a destructor can create keys we could enter an infinite loop
 * therefore we give up after LTHREAD_DESTRUCTOR_ITERATIONS
 * the behavior is modelled on pthread
 */
void _lthread_tls_destroy(struct lthread *lt)
{
	int i, k;
	int nb_keys;
	void *data;

	for (i = 0; i < LTHREAD_DESTRUCTOR_ITERATIONS; i++) {

		for (k = 1; k < LTHREAD_MAX_KEYS; k++) {

			/* no keys in use ? */
			nb_keys = lt->tls->nb_keys_inuse;
			if (nb_keys == 0)
				return;

			/* this key not in use ? */
			if (lt->tls->data[k] == NULL)
				continue;

			/* remove this key */
			data = lt->tls->data[k];
			lt->tls->data[k] = NULL;
			lt->tls->nb_keys_inuse = nb_keys-1;

			/* invoke destructor */
			if (key_table[k].destructor != NULL)
				key_table[k].destructor(data);
		}
	}
}

/*
 * Return the pointer associated with a key
 * If the key is no longer valid return NULL
 */
void
*lthread_getspecific(unsigned int k)
{

	if (k > LTHREAD_MAX_KEYS)
		return NULL;

	return THIS_LTHREAD->tls->data[k];
}

/*
 * Set a value against a key
 * If the key is no longer valid return an error
 * when storing value
 */
int lthread_setspecific(unsigned int k, const void *data)
{
	if (k > LTHREAD_MAX_KEYS)
		return POSIX_ERRNO(EINVAL);

	int n = THIS_LTHREAD->tls->nb_keys_inuse;

	/* discard const qualifier */
	char *p = (char *) (uintptr_t) data;


	if (data != NULL) {
		if (THIS_LTHREAD->tls->data[k] == NULL)
			THIS_LTHREAD->tls->nb_keys_inuse = n+1;
	}

	THIS_LTHREAD->tls->data[k] = (void *) p;
	return 0;
}

/*
 * Allocate data for TLS cache
*/
void _lthread_tls_alloc(struct lthread *lt)
{
	struct lthread_tls *tls;

	tls = _lthread_objcache_alloc((THIS_SCHED)->tls_cache);

	RTE_ASSERT(tls != NULL);

	tls->root_sched = (THIS_SCHED);
	lt->tls = tls;

	/* allocate data for TLS varaiables using RTE_PER_LTHREAD macros */
	if (sizeof(void *) < (uint64_t)RTE_PER_LTHREAD_SECTION_SIZE) {
		lt->per_lthread_data =
		    _lthread_objcache_alloc((THIS_SCHED)->per_lthread_cache);
	}
}
