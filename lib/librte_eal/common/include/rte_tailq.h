/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#ifndef _RTE_TAILQ_H_
#define _RTE_TAILQ_H_

/**
 * @file
 *  Here defines rte_tailq APIs for only internal use
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>
#include <stdio.h>

/** dummy structure type used by the rte_tailq APIs */
struct rte_dummy {
	TAILQ_ENTRY(rte_dummy) next; /**< Pointer entries for a tailq list */
};
/** dummy */
TAILQ_HEAD(rte_dummy_head, rte_dummy);

#define RTE_TAILQ_NAMESIZE 32

/**
 * The structure defining a tailq header entry for storing
 * in the rte_config structure in shared memory. Each tailq
 * is identified by name.
 * Any library storing a set of objects e.g. rings, mempools, hash-tables,
 * is recommended to use an entry here, so as to make it easy for
 * a multi-process app to find already-created elements in shared memory.
 */
struct rte_tailq_head {
	struct rte_dummy_head tailq_head; /**< NOTE: must be first element */
};

/**
 * Utility macro to make reserving a tailqueue for a particular struct easier.
 *
 * @param name
 *   The name to be given to the tailq - used by lookup to find it later
 *
 * @param struct_name
 *   The name of the list type we are using. (Generally this is the same as the
 *   first parameter passed to TAILQ_HEAD macro)
 *
 * @return
 *   The return value from rte_eal_tailq_reserve, typecast to the appropriate
 *   structure pointer type.
 *   NULL on error, since the tailq_head is the first
 *   element in the rte_tailq_head structure.
 */
#define RTE_TAILQ_RESERVE(name, struct_name) \
	(struct struct_name *)(&rte_eal_tailq_reserve(name)->tailq_head)

/**
 * Utility macro to make reserving a tailqueue for a particular struct easier.
 *
 * @param idx
 *   The tailq idx defined in rte_tail_t to be given to the tail queue.
 *       - used by lookup to find it later
 *
 * @param struct_name
 *   The name of the list type we are using. (Generally this is the same as the
 *   first parameter passed to TAILQ_HEAD macro)
 *
 * @return
 *   The return value from rte_eal_tailq_reserve, typecast to the appropriate
 *   structure pointer type.
 *   NULL on error, since the tailq_head is the first
 *   element in the rte_tailq_head structure.
 */
#define RTE_TAILQ_RESERVE_BY_IDX(idx, struct_name) \
	(struct struct_name *)(&rte_eal_tailq_reserve_by_idx(idx)->tailq_head)

/**
 * Utility macro to make looking up a tailqueue for a particular struct easier.
 *
 * @param name
 *   The name of tailq
 *
 * @param struct_name
 *   The name of the list type we are using. (Generally this is the same as the
 *   first parameter passed to TAILQ_HEAD macro)
 *
 * @return
 *   The return value from rte_eal_tailq_lookup, typecast to the appropriate
 *   structure pointer type.
 *   NULL on error, since the tailq_head is the first
 *   element in the rte_tailq_head structure.
 */
#define RTE_TAILQ_LOOKUP(name, struct_name) \
	(struct struct_name *)(&rte_eal_tailq_lookup(name)->tailq_head)

/**
 * Utility macro to make looking up a tailqueue for a particular struct easier.
 *
 * @param idx
 *   The tailq idx defined in rte_tail_t to be given to the tail queue.
 *
 * @param struct_name
 *   The name of the list type we are using. (Generally this is the same as the
 *   first parameter passed to TAILQ_HEAD macro)
 *
 * @return
 *   The return value from rte_eal_tailq_lookup, typecast to the appropriate
 *   structure pointer type.
 *   NULL on error, since the tailq_head is the first
 *   element in the rte_tailq_head structure.
 */
#define RTE_TAILQ_LOOKUP_BY_IDX(idx, struct_name) \
	(struct struct_name *)(&rte_eal_tailq_lookup_by_idx(idx)->tailq_head)

/**
 * Reserve a slot in the tailq list for a particular tailq header
 * Note: this function, along with rte_tailq_lookup, is not multi-thread safe,
 * and both these functions should only be called from a single thread at a time
 *
 * @param name
 *   The name to be given to the tail queue.
 * @return
 *   A pointer to the newly reserved tailq entry
 */
struct rte_tailq_head *rte_eal_tailq_reserve(const char *name);

/**
 * Reserve a slot in the tailq list for a particular tailq header
 * Note: this function, along with rte_tailq_lookup, is not multi-thread safe,
 * and both these functions should only be called from a single thread at a time
 *
 * @param idx
 *   The tailq idx defined in rte_tail_t to be given to the tail queue.
 * @return
 *   A pointer to the newly reserved tailq entry
 */
struct rte_tailq_head *rte_eal_tailq_reserve_by_idx(const unsigned idx);

/**
 * Dump tail queues to the console.
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_dump_tailq(FILE *f);

/**
 * Lookup for a tail queue.
 *
 * Get a pointer to a tail queue header of an already reserved tail
 * queue identified by the name given as an argument.
 * Note: this function, along with rte_tailq_reserve, is not multi-thread safe,
 * and both these functions should only be called from a single thread at a time
 *
 * @param name
 *   The name of the queue.
 * @return
 *   A pointer to the tail queue head structure.
 */
struct rte_tailq_head *rte_eal_tailq_lookup(const char *name);

/**
 * Lookup for a tail queue.
 *
 * Get a pointer to a tail queue header of an already reserved tail
 * queue identified by the name given as an argument.
 * Note: this function, along with rte_tailq_reserve, is not multi-thread safe,
 * and both these functions should only be called from a single thread at a time
 *
 * @param idx
 *   The tailq idx defined in rte_tail_t to be given to the tail queue.
 * @return
 *   A pointer to the tail queue head structure.
 */
struct rte_tailq_head *rte_eal_tailq_lookup_by_idx(const unsigned idx);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_TAILQ_H_ */
