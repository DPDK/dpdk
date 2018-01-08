/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __THUNDERX_NICVF_SVF_H__
#define __THUNDERX_NICVF_SVF_H__

struct nicvf;

/**
 * Enqueue new VF to secondary qsets.
 *
 * @param entry
 *   Entry to be enqueued.
 */
void
nicvf_svf_push(struct nicvf *vf);

/**
 * Dequeue a VF from secondary qsets.
 *
 * @return
 *   Dequeued entry.
 */
struct nicvf *
nicvf_svf_pop(void);

/**
 * Check if the queue of secondary qsets is empty.
 *
 * @return
 *   0 on non-empty
 *   otherwise empty
 */
int
nicvf_svf_empty(void);

#endif /* __THUNDERX_NICVF_SVF_H__  */
