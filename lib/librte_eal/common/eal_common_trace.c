/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <inttypes.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_string_fns.h>

#include "eal_trace.h"

RTE_DEFINE_PER_LCORE(volatile int, trace_point_sz);
static RTE_DEFINE_PER_LCORE(char, ctf_field[TRACE_CTF_FIELD_SIZE]);
static RTE_DEFINE_PER_LCORE(int, ctf_count);

static struct trace_point_head tp_list = STAILQ_HEAD_INITIALIZER(tp_list);
static struct trace trace;

int
__rte_trace_point_register(rte_trace_point_t *handle, const char *name,
		void (*register_fn)(void))
{
	char *field = RTE_PER_LCORE(ctf_field);
	struct trace_point *tp;
	uint16_t sz;

	/* Sanity checks of arguments */
	if (name == NULL || register_fn == NULL || handle == NULL) {
		trace_err("invalid arguments");
		rte_errno = EINVAL;
		goto fail;
	}

	/* Check the size of the trace point object */
	RTE_PER_LCORE(trace_point_sz) = 0;
	RTE_PER_LCORE(ctf_count) = 0;
	register_fn();
	if (RTE_PER_LCORE(trace_point_sz) == 0) {
		trace_err("missing rte_trace_emit_header() in register fn");
		rte_errno = EBADF;
		goto fail;
	}

	/* Is size overflowed */
	if (RTE_PER_LCORE(trace_point_sz) > UINT16_MAX) {
		trace_err("trace point size overflowed");
		rte_errno = ENOSPC;
		goto fail;
	}

	/* Are we running out of space to store trace points? */
	if (trace.nb_trace_points > UINT16_MAX) {
		trace_err("trace point exceeds the max count");
		rte_errno = ENOSPC;
		goto fail;
	}

	/* Get the size of the trace point */
	sz = RTE_PER_LCORE(trace_point_sz);
	tp = calloc(1, sizeof(struct trace_point));
	if (tp == NULL) {
		trace_err("fail to allocate trace point memory");
		rte_errno = ENOMEM;
		goto fail;
	}

	/* Initialize the trace point */
	if (rte_strscpy(tp->name, name, TRACE_POINT_NAME_SIZE) < 0) {
		trace_err("name is too long");
		rte_errno = E2BIG;
		goto free;
	}

	/* Copy the field data for future use */
	if (rte_strscpy(tp->ctf_field, field, TRACE_CTF_FIELD_SIZE) < 0) {
		trace_err("CTF field size is too long");
		rte_errno = E2BIG;
		goto free;
	}

	/* Clear field memory for the next event */
	memset(field, 0, TRACE_CTF_FIELD_SIZE);

	/* Form the trace handle */
	*handle = sz;
	*handle |= trace.nb_trace_points << __RTE_TRACE_FIELD_ID_SHIFT;

	trace.nb_trace_points++;
	tp->handle = handle;

	/* Add the trace point at tail */
	STAILQ_INSERT_TAIL(&tp_list, tp, next);
	__atomic_thread_fence(__ATOMIC_RELEASE);

	/* All Good !!! */
	return 0;
free:
	free(tp);
fail:
	if (trace.register_errno == 0)
		trace.register_errno = rte_errno;

	return -rte_errno;
}
