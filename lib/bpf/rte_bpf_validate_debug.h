/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _RTE_BPF_VALIDATE_DEBUG_H_
#define _RTE_BPF_VALIDATE_DEBUG_H_

/**
 * @file rte_bpf_validate_debug.h
 *
 * Debugging interface for BPF validation.
 *
 * Can be used for debugging BPF validation problems as well as in tests.
 */

#include <bpf_def.h>
#include <rte_compat.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_BPF_VALIDATE_DEBUG_MAY_BE_FALSE	RTE_BIT32(0)
#define RTE_BPF_VALIDATE_DEBUG_MAY_BE_TRUE	RTE_BIT32(1)

/**
 * Supported validate events.
 *
 * Valid events begin from 0 and end before `RTE_BPF_VALIDATE_DEBUG_EVENT_END`.
 */
enum rte_bpf_validate_debug_event {
	/* Just before every instruction, at branch or validation end. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_STEP,
	/* Validator has failed its internal self-checks. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_INVALID_STATE,
	/* Start of validation. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_START,
	/* Successful finish of validation. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_SUCCESS,
	/* Finish of validation with error. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_FAILURE,
	/* Beginning of a branch just after the jump. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_ENTER,
	/* Pruning branch as verified earlier. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_PRUNE,
	/* End of branch verification, after the last verified instruction. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_RETURN,
	/* Number of valid event values. */
	RTE_BPF_VALIDATE_DEBUG_EVENT_END,
};

struct rte_bpf_validate_debug;
struct rte_bpf_validate_debug_point;

/** User callback description. */
struct rte_bpf_validate_debug_callback {
	int (*fn)(struct rte_bpf_validate_debug *debug, void *ctx);
	void *ctx;
};

/** Invoked by rte_bpf_validate_debug_for_each_point for each breakpoint and catchpoint. */
typedef int (*rte_bpf_validate_debug_point_process_t)(struct rte_bpf_validate_debug_point *point,
	void *ctx);

/**
 * Create new debug instance.
 *
 * @return
 *   Debug instance in case of success.
 *   NULL with rte_errno set in case of a failure.
 */
__rte_experimental
struct rte_bpf_validate_debug *
rte_bpf_validate_debug_create(void);

/**
 * Destroy debug instance.
 *
 * Behavior is undefined if validation with this debug instance is ongoing.
 *
 * @param debug
 *   Debug instance, or NULL.
 */
__rte_experimental
void
rte_bpf_validate_debug_destroy(struct rte_bpf_validate_debug *debug);

/**
 * Create new breakpoint at specified location.
 *
 * Can be called before the validation has started. If at validation start later
 * the program will not have the specified instruction, the start will fail.
 *
 * It is allowed to create breakpoints for the same location a callback is
 * currently executing for, but it will not be invoked in the same cycle.
 *
 * @param debug
 *   Debug instance.
 * @param pc
 *   Program counter to create breakpoint at.
 * @param callback
 *   Callback to invoke.
 * @return
 *   New breakpoint on success, NULL with rte_errno set on failure.
 */
__rte_experimental
struct rte_bpf_validate_debug_point *
rte_bpf_validate_debug_break(struct rte_bpf_validate_debug *debug, uint32_t pc,
	const struct rte_bpf_validate_debug_callback *callback);

/**
 * Create new catchpoint for specified event.
 *
 * Can be called before the validation has started.
 *
 * It is allowed to create catchpoints for the same event a callback is
 * currently executing for, but it will not be invoked in the same cycle.
 *
 * @param debug
 *   Debug instance.
 * @param event
 *   Validation event to create catchpoint for.
 * @param callback
 *   Callback to invoke.
 * @return
 *   New breakpoint on success, NULL with rte_errno set on failure.
 */
__rte_experimental
struct rte_bpf_validate_debug_point *
rte_bpf_validate_debug_catch(struct rte_bpf_validate_debug *debug,
	enum rte_bpf_validate_debug_event event,
	const struct rte_bpf_validate_debug_callback *callback);

/**
 * Delete breakpoint or catchpoint and free all associated resources.
 *
 * If a callback is currently being executed, calling this API is allowed for:
 * - breakpoint or catchpoint the callback is executed for;
 * - breakpoints or catchpoints for other locations or events;
 * and NOT allowed for:
 * - other breakpoints or catchpoints for the same location or event.
 *
 * @param point
 *   Breakpoint or catchpoint to destroy, or NULL.
 */
__rte_experimental
void
rte_bpf_validate_debug_point_destroy(struct rte_bpf_validate_debug_point *point);

/**
 * Get effective eBPF parameters struct.
 *
 * @param debug
 *   Debug instance.
 * @return
 *   Parameters struct of the validated eBPF program, including code with all
 *   patches and relocations applied.
 */
__rte_experimental
const struct rte_bpf_prm_ex *
rte_bpf_validate_debug_get_bpf_param(const struct rte_bpf_validate_debug *debug);

/**
 * Get pointer to effective eBPF program instructions.
 *
 * @param debug
 *   Debug instance.
 * @param ins
 *   Upon return, program instructions with all patches and relocations applied.
 * @param nb_ins
 *   Upon return, number of program instructions.
 * @return
 *   Non-negative value on success, negative errno on failure.
 */
__rte_experimental
int
rte_bpf_validate_debug_get_ins(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn **ins, uint32_t *nb_ins);

/**
 * Get last triggered breakpoint or catchpoint.
 *
 * Can be used to destroy currently processed breakpoint or catchpoint.
 *
 * The pointer may be invalid if the breakpoint or catchpoint has already been
 * destroyed earlier.
 *
 * @param debug
 *   Debug instance.
 * @return
 *   Last triggered breakpoint or callpoint, including one the callback is
 *   currently executing for.
 *   NULL of none were triggered in the current validation process.
 */
__rte_experimental
struct rte_bpf_validate_debug_point *
rte_bpf_validate_debug_get_last_point(const struct rte_bpf_validate_debug *debug);

/**
 * Get current instruction index, or one after last if finishing.
 *
 * @param debug
 *   Debug instance.
 * @return
 *   Current program counter being validated, or one after last.
 *   UINT32_MAX if no program is being validated.
 */
__rte_experimental
uint32_t
rte_bpf_validate_debug_get_pc(const struct rte_bpf_validate_debug *debug);

/**
 * Get the validation result, if it has finished.
 *
 * @param debug
 *   Debug instance.
 * @param result
 *   Upon successful return, the validation result (negative if validation failed).
 * @return
 *   Non-negative value if validation has finished and result variable was written;
 *   -EAGAIN if validation is still ongoing;
 *   other negative errno in case of failure;
 */
__rte_experimental
int
rte_bpf_validate_debug_get_validation_result(const struct rte_bpf_validate_debug *debug,
	int *result);

/**
 * Check if specified memory access instruction is currently valid.
 *
 * @param debug
 *   Debug instance.
 * @param access
 *   Memory load or store eBPF instruction.
 * @param off64
 *   Additional 64-bit offset added to ins->off.
 * @return
 *   1 if specified memory access is currently valid;
 *   0 if specified memory access is currently invalid;
 *   negative errno in case of failure;
 */
__rte_experimental
int
rte_bpf_validate_debug_can_access(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn *access, uint64_t off64);

/**
 * Get possible truth values of the specified jump condition.
 *
 * @param debug
 *   Debug instance.
 * @param jump
 *   Conditional jump instruction specifying the condition.
 * @param imm64
 *   Additional 64-bit immediate added to the source.
 * @return
 *   in case of success, bitwise combination of:
 *     RTE_BPF_VALIDATE_DEBUG_MAY_BE_FALSE if the jump condition may be false;
 *     RTE_BPF_VALIDATE_DEBUG_MAY_BE_TRUE if the jump condition may be true;
 *   negative errno in case of failure.
 */
__rte_experimental
int
rte_bpf_validate_debug_may_jump(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn *jump, uint64_t imm64);

/**
 * Format information about specified register for the user.
 *
 * Parameters buffer, bufsz and return value work the same way as for snprintf.
 *
 * @param debug
 *   Debug instance.
 * @param buffer
 *   Buffer to fill with register information.
 * @param bufsz
 *   Buffer size (including space for terminating zero).
 * @param reg
 *   Register to provide information about.
 * @return
 *   Number of characters needed _excluding_ terminating zero.
 */
__rte_experimental
int
rte_bpf_validate_debug_format_register_info(const struct rte_bpf_validate_debug *debug,
	char *buffer, size_t bufsz, uint8_t reg);

/**
 * Format information about specified stack frame location for the user.
 *
 * Parameters buffer, bufsz and return value work the same way as for snprintf.
 *
 * @param debug
 *   Debug instance.
 * @param buffer
 *   Buffer to fill with register information.
 * @param bufsz
 *   Buffer size (including space for terminating zero).
 * @param offset
 *   Stack frame offset to provide information about, in bytes.
 *   Typically a negative multiple of 8.
 * @return
 *   Number of characters needed _excluding_ terminating zero.
 */
__rte_experimental
int
rte_bpf_validate_debug_format_frame_info(const struct rte_bpf_validate_debug *debug,
	char *buffer, size_t bufsz, int32_t offset);

/**
 * Get program stack frame size.
 *
 * @param debug
 *   Debug instance.
 * @return
 *   Program stack frame size in bytes.
 */
__rte_experimental
int32_t
rte_bpf_validate_debug_get_frame_size(const struct rte_bpf_validate_debug *debug);

/**
 * Format value following the style of register format function.
 *
 * Parameters buffer, bufsz and return value work the same way as for snprintf.
 *
 * @param buffer
 *   Buffer to fill with register information.
 * @param bufsz
 *   Buffer size (including space for terminating zero).
 * @param format
 *   One of characters 'd' or 'x' for signed or hexadecimal format.
 * @param value
 *   Formatted value, can be signed typecast to unsigned.
 * @return
 *   Number of characters needed _excluding_ terminating zero.
 */
__rte_experimental
int
rte_bpf_validate_debug_format_value(char *buffer, size_t bufsz, char format,
	uint64_t value);

/**
 * Format interval following the style of register format function.
 *
 * Parameters buffer, bufsz and return value work the same way as for snprintf.
 *
 * @param buffer
 *   Buffer to fill with register information.
 * @param bufsz
 *   Buffer size (including space for terminating zero).
 * @param format
 *   One of characters 'd' or 'x' for signed or hexadecimal format.
 * @param min
 *   Minimum value of the interval, can be signed typecast to unsigned.
 * @param max
 *   Maximum value of the interval, can be signed typecast to unsigned.
 * @return
 *   Number of characters needed _excluding_ terminating zero.
 */
__rte_experimental
int
rte_bpf_validate_debug_format_interval(char *buffer, size_t bufsz, char format,
	uint64_t min, uint64_t max);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BPF_VALIDATE_DEBUG_H_ */
