/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __THUNDERX_NICVF_BSVF_H__
#define __THUNDERX_NICVF_BSVF_H__

#include <sys/queue.h>

struct nicvf;

/**
 * The base queue structure to hold secondary qsets.
 */
struct svf_entry {
	STAILQ_ENTRY(svf_entry) next; /**< Next element's pointer */
	struct nicvf *vf;              /**< Holder of a secondary qset */
};

/**
 * Enqueue new entry to secondary qsets.
 *
 * @param entry
 *   Entry to be enqueued.
 */
void
nicvf_bsvf_push(struct svf_entry *entry);

/**
 * Dequeue an entry from secondary qsets.
 *
 * @return
 *   Dequeued entry.
 */
struct svf_entry *
nicvf_bsvf_pop(void);

/**
 * Check if the queue of secondary qsets is empty.
 *
 * @return
 *   0 on non-empty
 *   otherwise empty
 */
int
nicvf_bsvf_empty(void);

#endif /* __THUNDERX_NICVF_BSVF_H__  */
