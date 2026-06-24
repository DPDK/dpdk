/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "bpf_impl.h"
#include "bpf_validate.h"
#include "bpf_validate_debug.h"

#include <eal_export.h>
#include <rte_bpf_validate_debug.h>
#include <rte_errno.h>
#include <rte_per_lcore.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef LIST_FOREACH_SAFE
/* We need this macro which neither Linux nor EAL for Linux include yet. */
#define	LIST_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = LIST_FIRST((head));				\
	    (var) && ((tvar) = LIST_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

#define EVENT_ARRAY_LENGTH RTE_BPF_VALIDATE_DEBUG_EVENT_END

struct rte_bpf_validate_debug_point {
	LIST_ENTRY(rte_bpf_validate_debug_point) list;
	struct rte_bpf_validate_debug_callback callback;
	uint32_t pc;
};

LIST_HEAD(point_list, rte_bpf_validate_debug_point);

struct rte_bpf_validate_debug {
	/* Accessible immediately after object creation. */
	struct point_list pending_breakpoints;
	struct point_list *catchpoint_lists;
	struct rte_bpf_validate_debug_callback step_callback;

	/* Accessible only after evaluate start. */
	const struct bpf_verifier *verifier;
	const struct rte_bpf_prm_ex *bpf_prm;
	struct point_list *breakpoint_lists;
	struct rte_bpf_validate_debug_point *last_point;
	uint32_t pc;
	/* Evaluate stage (only tracking `evaluate` part at the moment). */
	bool evaluate_started;
	bool evaluate_finished;
	int evaluate_result;  /* Only valid if `evaluate_finished` is true. */
};

/* Point lists functions. */

/* Destroy all points in the list. */
static void
point_list_destroy(struct point_list *point_list)
{
	struct rte_bpf_validate_debug_point *point, *next;

	LIST_FOREACH_SAFE(point, point_list, list, next)
		rte_bpf_validate_debug_point_destroy(point);

	RTE_ASSERT(LIST_EMPTY(point_list));
}

/* Destroy all points in all lists in the array and free the array. */
static void
point_lists_destroy(struct point_list *point_lists, uint32_t length)
{
	if (point_lists == NULL)
		return;

	for (uint32_t pli = 0; pli != length; ++pli)
		point_list_destroy(&point_lists[pli]);

	free(point_lists);
}

/* Dynamically allocate and initialize an array of point lists. */
static struct point_list *
point_lists_create(uint32_t length)
{
	/* Allocate at least one element to avoid calloc(0, ...) shenanigans. */
	struct point_list *const array =
		calloc(RTE_MAX(1u, length), sizeof(*array));
	if (array == NULL)
		return NULL;

	for (uint32_t pli = 0; pli != length; ++pli)
		LIST_INIT(&array[pli]);

	return array;
}

/* Move point to a different list. */
static inline void
point_move(struct rte_bpf_validate_debug_point *point,
	struct point_list *destination)
{
	LIST_REMOVE(point, list);
	LIST_INSERT_HEAD(destination, point, list);
}

/* Move all points between lists (the order is inverted). */
static void
points_move(struct point_list *source, struct point_list *destination)
{
	struct rte_bpf_validate_debug_point *point, *next;

	LIST_FOREACH_SAFE(point, source, list, next)
		point_move(point, destination);
	RTE_ASSERT(LIST_EMPTY(source));
}

/* Pending breakpoints. */

/* Return true if all pending breakpoints have pc less than nb_ins. */
static bool
debug_pending_breakpoints_are_valid(const struct rte_bpf_validate_debug *debug,
	uint32_t nb_ins)
{
	const struct rte_bpf_validate_debug_point *breakpoint;

	LIST_FOREACH(breakpoint, &debug->pending_breakpoints, list)
		if (breakpoint->pc >= nb_ins)
			return false;

	return true;
}

/* Move all pending breakpoints to correct per-pc lists. */
static void
debug_pending_breakpoints_restore(struct rte_bpf_validate_debug *debug)
{
	struct rte_bpf_validate_debug_point *breakpoint, *next;
	struct point_list breakpoints;

	/* Invert the list first to preserve point order when we move them. */
	LIST_INIT(&breakpoints);
	points_move(&debug->pending_breakpoints, &breakpoints);

	LIST_FOREACH_SAFE(breakpoint, &breakpoints, list, next)
		point_move(breakpoint, &debug->breakpoint_lists[breakpoint->pc]);
	RTE_ASSERT(LIST_EMPTY(&breakpoints));
}

/* Move all breakpoints from per-pc lists to the pending one. */
static void
debug_pending_breakpoints_save(struct rte_bpf_validate_debug *debug)
{
	struct point_list breakpoints;

	LIST_INIT(&breakpoints);
	for (uint32_t pc = 0; pc != debug->bpf_prm->raw.nb_ins; ++pc)
		points_move(&debug->breakpoint_lists[pc], &breakpoints);

	/* Invert the list to restore point order after we moved them. */
	RTE_ASSERT(LIST_EMPTY(&debug->pending_breakpoints));
	points_move(&breakpoints, &debug->pending_breakpoints);
}

/* Debug instance creation and destruction. */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_destroy, 26.07)
void
rte_bpf_validate_debug_destroy(struct rte_bpf_validate_debug *debug)
{
	if (debug == NULL)
		return;

	/* Cannot destroy the instance during validation. */
	RTE_ASSERT(!debug->evaluate_started);

	point_lists_destroy(debug->catchpoint_lists, EVENT_ARRAY_LENGTH);
	point_list_destroy(&debug->pending_breakpoints);
	free(debug);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_create, 26.07)
struct rte_bpf_validate_debug *
rte_bpf_validate_debug_create(void)
{
	struct rte_bpf_validate_debug *const debug = calloc(1, sizeof(*debug));
	if (debug == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	LIST_INIT(&debug->pending_breakpoints);

	debug->catchpoint_lists = point_lists_create(EVENT_ARRAY_LENGTH);
	if (debug->catchpoint_lists == NULL) {
		free(debug);
		rte_errno = ENOMEM;
		return NULL;
	}

	return debug;
}

/* Managing callbacks. */

/* Call back the user function with correct arguments for a point. */
static inline int
debug_point_call_back(struct rte_bpf_validate_debug *debug,
	struct rte_bpf_validate_debug_point *point)
{
	debug->last_point = point;
	return point->callback.fn(debug, point->callback.ctx);
}

/* Call back all points in point_list. */
static int
debug_points_call_back(struct rte_bpf_validate_debug *debug,
	const struct point_list *point_list)
{
	struct rte_bpf_validate_debug_point *point, *next;
	int rc = 0;

	LIST_FOREACH_SAFE(point, point_list, list, next)
		rc = rc < 0 ? rc : debug_point_call_back(debug, point);

	return rc;
}

/* Call back all catchpoints for the specified event. */
static int
debug_send_event(struct rte_bpf_validate_debug *debug, debug_event_t event)
{
	return debug_points_call_back(debug, &debug->catchpoint_lists[event]);
}

/* Create new point and insert it into the specified list. */
static struct rte_bpf_validate_debug_point *
point_list_insert(struct point_list *point_list,
	const struct rte_bpf_validate_debug_callback *callback, uint32_t pc)
{
	struct rte_bpf_validate_debug_point *const point =
		malloc(sizeof(*point));
	if (point == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	LIST_INSERT_HEAD(point_list, point, list);
	point->callback = *callback;
	point->pc = pc;
	return point;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_break, 26.07)
struct rte_bpf_validate_debug_point *
rte_bpf_validate_debug_break(struct rte_bpf_validate_debug *debug, uint32_t pc,
	const struct rte_bpf_validate_debug_callback *callback)
{
	if (debug == NULL || callback == NULL || callback->fn == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	if (!debug->evaluate_started)
		return point_list_insert(&debug->pending_breakpoints,
			callback, pc);

	if (pc >= debug->bpf_prm->raw.nb_ins) {
		rte_errno = ENOENT;
		return NULL;
	}

	return point_list_insert(&debug->breakpoint_lists[pc], callback, pc);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_catch, 26.07)
struct rte_bpf_validate_debug_point *
rte_bpf_validate_debug_catch(struct rte_bpf_validate_debug *debug,
	debug_event_t event, const struct rte_bpf_validate_debug_callback *callback)
{
	if (debug == NULL || callback == NULL || callback->fn == NULL ||
			event < 0 || event >= RTE_BPF_VALIDATE_DEBUG_EVENT_END) {
		rte_errno = EINVAL;
		return NULL;
	}

	return point_list_insert(&debug->catchpoint_lists[event], callback, 0);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_point_destroy, 26.07)
void
rte_bpf_validate_debug_point_destroy(struct rte_bpf_validate_debug_point *point)
{
	if (point == NULL)
		return;

	LIST_REMOVE(point, list);
	free(point);
}

/* Querying execution state. */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_get_bpf_param, 26.07)
const struct rte_bpf_prm_ex *
rte_bpf_validate_debug_get_bpf_param(const struct rte_bpf_validate_debug *debug)
{
	if (debug == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	if (!debug->evaluate_started) {
		rte_errno = ECHILD;
		return NULL;
	}

	return debug->bpf_prm;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_get_ins, 26.07)
int
rte_bpf_validate_debug_get_ins(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn **ins, uint32_t *nb_ins)
{
	if (debug == NULL)
		return -EINVAL;

	if (!debug->evaluate_started)
		return -ECHILD;

	if (debug->bpf_prm->origin != RTE_BPF_ORIGIN_RAW)
		return -ENOTSUP;

	*ins = debug->bpf_prm->raw.ins;
	*nb_ins = debug->bpf_prm->raw.nb_ins;
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_get_last_point, 26.07)
struct rte_bpf_validate_debug_point *
rte_bpf_validate_debug_get_last_point(const struct rte_bpf_validate_debug *debug)
{
	if (debug == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	return debug->last_point;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_get_pc, 26.07)
uint32_t
rte_bpf_validate_debug_get_pc(const struct rte_bpf_validate_debug *debug)
{
	if (debug == NULL || !debug->evaluate_started)
		return UINT32_MAX;

	return debug->pc;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_get_validation_result, 26.07)
int
rte_bpf_validate_debug_get_validation_result(const struct rte_bpf_validate_debug *debug,
	int *result)
{
	if (debug == NULL)
		return -EINVAL;

	if (!debug->evaluate_finished)
		return -EAGAIN;

	*result = debug->evaluate_result;

	return 0;
}

/* Querying VM state. */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_can_access, 26.07)
int
rte_bpf_validate_debug_can_access(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn *access, uint64_t off64)
{
	if (debug == NULL || access == NULL)
		return -EINVAL;

	if (!debug->evaluate_started)
		return -ECHILD;

	return __rte_bpf_validate_can_access(debug->verifier, access, off64);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_may_jump, 26.07)
int
rte_bpf_validate_debug_may_jump(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn *jump, uint64_t imm64)
{
	if (debug == NULL || jump == NULL)
		return -EINVAL;

	if (!debug->evaluate_started)
		return -ECHILD;

	return __rte_bpf_validate_may_jump(debug->verifier, jump, imm64);
}

/* Formatting VM state for user. */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_format_register_info, 26.07)
int
rte_bpf_validate_debug_format_register_info(const struct rte_bpf_validate_debug *debug,
	char *buffer, size_t bufsz, uint8_t reg)
{
	if (debug == NULL)
		return -EINVAL;

	if (!debug->evaluate_started)
		return -ECHILD;

	return __rte_bpf_validate_format_register_info(debug->verifier, buffer,
		bufsz, reg);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_format_frame_info, 26.07)
int
rte_bpf_validate_debug_format_frame_info(const struct rte_bpf_validate_debug *debug,
	char *buffer, size_t bufsz, int32_t offset)
{
	if (debug == NULL)
		return -EINVAL;

	if (!debug->evaluate_started)
		return -ECHILD;

	return __rte_bpf_validate_format_frame_info(debug->verifier, buffer,
		bufsz, offset);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_get_frame_size, 26.07)
int32_t
rte_bpf_validate_debug_get_frame_size(const struct rte_bpf_validate_debug *debug)
{
	if (debug == NULL)
		return -EINVAL;

	if (!debug->evaluate_started)
		return -ECHILD;

	return __rte_bpf_validate_get_frame_size(debug->verifier);
}

/* Courtesy formatting functions for user-supplied values. */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_format_value, 26.07)
int
rte_bpf_validate_debug_format_value(char *buffer, size_t bufsz, char format,
	uint64_t value)
{
	static const struct {
		uint64_t value;
		const char *name;
	} constants[] = {
		{ .value = INT64_MIN, .name = "INT64_MIN" },
		{ .value = INT32_MIN, .name = "INT32_MIN" },
		{ .value = INT16_MIN, .name = "INT16_MIN" },
		{ .value = INT8_MIN, .name = "INT8_MIN" },
		{ .value = INT8_MAX, .name = "INT8_MAX" },
		{ .value = UINT8_MAX, .name = "UINT8_MAX" },
		{ .value = INT16_MAX, .name = "INT16_MAX" },
		{ .value = UINT16_MAX, .name = "UINT16_MAX" },
		{ .value = INT32_MAX, .name = "INT32_MAX" },
		{ .value = UINT32_MAX, .name = "UINT32_MAX" },
		{ .value = INT64_MAX, .name = "INT64_MAX" },
		/* UINT64_MAX omitted on purpose, it looks better as -1 */
	};

	switch (format) {
	case 'd':
		for (int ci = 0; ci != RTE_DIM(constants); ++ci)
			if (constants[ci].value == value)
				return snprintf(buffer, bufsz, "%s", constants[ci].name);
		/*
		 * Special case numbers close to int32_t or int64_t range ends,
		 * since they are hard to recognize in decimal otherwise.
		 */
		if (value - INT64_MIN < 1000000)
			return snprintf(buffer, bufsz, "INT64_MIN+%" PRId64,
				value - INT64_MIN);
		if (INT64_MAX - value < 1000000)
			return snprintf(buffer, bufsz, "INT64_MAX-%" PRId64,
				INT64_MAX - value);
		if (value - INT32_MIN < 1000)
			return snprintf(buffer, bufsz, "INT32_MIN+%" PRId64,
				value - INT32_MIN);
		if (INT32_MAX - value < 1000)
			return snprintf(buffer, bufsz, "INT32_MAX-%" PRId64,
				INT32_MAX - value);
		return snprintf(buffer, bufsz, "%" PRId64, value);
	case 'x':
		/* Special case only the common case of UINT64_MAX. */
		if (value == UINT64_MAX)
			return snprintf(buffer, bufsz, "%s", "UINT64_MAX");
		return snprintf(buffer, bufsz, "%#" PRIx64, value);
	default:
		return -EINVAL;
	}
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_validate_debug_format_interval, 26.07)
int
rte_bpf_validate_debug_format_interval(char *buffer, size_t bufsz, char format,
	uint64_t min, uint64_t max)
{
	char min_buffer[32], max_buffer[32];
	int rc;

	if (min == max)
		return rte_bpf_validate_debug_format_value(buffer, bufsz, format, min);

	rc = rte_bpf_validate_debug_format_value(min_buffer, sizeof(min_buffer), format, min);
	if (rc < 0)
		return rc;

	rc = rte_bpf_validate_debug_format_value(max_buffer, sizeof(max_buffer), format, max);
	if (rc < 0)
		return rc;

	return snprintf(buffer, bufsz, "%s..%s", min_buffer, max_buffer);
}

/* Evaluation start and finish. */

/* Free all resources associated with current evaluation. */
static void
debug_evaluate_close(struct rte_bpf_validate_debug *debug)
{
	RTE_ASSERT(debug->evaluate_started);
	debug_pending_breakpoints_save(debug);
	free(debug->breakpoint_lists);
	debug->breakpoint_lists = NULL;
	debug->evaluate_started = false;
}

int
__rte_bpf_validate_debug_evaluate_start(struct rte_bpf_validate_debug *debug,
	const struct bpf_verifier *verifier, const struct rte_bpf_prm_ex *bpf_prm)
{
	if (debug == NULL)
		return 0;

	if (verifier == NULL || bpf_prm == NULL ||
			bpf_prm->origin != RTE_BPF_ORIGIN_RAW)
		return -EINVAL;

	if (debug->evaluate_started) {
		RTE_BPF_LOG_FUNC_LINE(ERR, "already started");
		return -EEXIST;
	}

	if (!debug_pending_breakpoints_are_valid(debug, bpf_prm->raw.nb_ins))
		return -ENOENT;

	debug->verifier = verifier;
	debug->bpf_prm = bpf_prm;
	debug->breakpoint_lists = point_lists_create(bpf_prm->raw.nb_ins);
	if (debug->breakpoint_lists == NULL)
		return -ENOMEM;
	debug_pending_breakpoints_restore(debug);
	debug->last_point = NULL;
	debug->pc = 0;
	debug->evaluate_started = true;

	const int rc = debug_send_event(debug,
		RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_START);
	if (rc < 0) {
		debug_evaluate_close(debug);
		return rc;
	}

	RTE_BPF_LOG_FUNC_LINE(DEBUG, "evaluate started");
	return 0;
}

int
__rte_bpf_validate_debug_evaluate_step(struct rte_bpf_validate_debug *debug,
	uint32_t pc, debug_event_t event)
{
	int rc;

	if (debug == NULL)
		return 0;

	if (!debug->evaluate_started) {
		RTE_BPF_LOG_FUNC_LINE(ERR, "not started");
		return -ECHILD;
	}

	if (pc > debug->bpf_prm->raw.nb_ins || event < 0 ||
			event >= RTE_BPF_VALIDATE_DEBUG_EVENT_END)
		return -EINVAL;

	debug->pc = pc;

	rc = __rte_bpf_validate_state_is_valid(debug->verifier);
	if (rc == 0)
		rc = debug_send_event(debug,
			RTE_BPF_VALIDATE_DEBUG_EVENT_INVALID_STATE);

	if (event != RTE_BPF_VALIDATE_DEBUG_EVENT_STEP)
		rc = rc < 0 ? rc : debug_send_event(debug, event);

	if (event == RTE_BPF_VALIDATE_DEBUG_EVENT_STEP ||
			event == RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_ENTER)
		/* Stepping into a real instruction to execute. */
		rc = rc < 0 ? rc : debug_points_call_back(debug,
			&debug->breakpoint_lists[pc]);

	rc = rc < 0 ? rc : debug_send_event(debug,
		RTE_BPF_VALIDATE_DEBUG_EVENT_STEP);

	return rc;
}

int
__rte_bpf_validate_debug_evaluate_finish(struct rte_bpf_validate_debug *debug,
	int result)
{
	int rc = 0;
	uint32_t pc;
	debug_event_t event;

	if (debug == NULL)
		return 0;

	if (!debug->evaluate_started) {
		RTE_BPF_LOG_FUNC_LINE(ERR, "not started");
		return -ECHILD;
	}

	debug->evaluate_finished = true;
	debug->evaluate_result = result;

	if (result != -ECANCELED) {
		if (result < 0) {
			/* Last known pc is the place we failed. */
			pc = debug->pc;
			event = RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_FAILURE;
		} else {
			/* Show program end, not particular instruction. */
			pc = debug->bpf_prm->raw.nb_ins;
			event = RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_SUCCESS;
		}

		rc = __rte_bpf_validate_debug_evaluate_step(debug, pc, event);
	}

	debug_evaluate_close(debug);

	return rc;
}
