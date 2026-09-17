/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "bpf_def.h"

#include "debug_command.h"
#include "internal.h"

#include <rte_bpf_validate_debug.h>
#include <rte_errno.h>

#include <inttypes.h>
#include <stdlib.h>


/* Write single line to the user, currently just to stdout. */
#define PRINTLN(fmt, ...) do { \
	RTE_LOG_CHECK_NO_NEWLINE(fmt); \
	printf(fmt "\n", ##__VA_ARGS__); \
} while (0)

#define PROMPT "(validate) "

#define INITIAL_CAPACITY 8u

static const char *const event_names[] = {
	[RTE_BPF_VALIDATE_DEBUG_EVENT_INVALID_STATE] = "invalid-state",
	[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_ENTER] = "branch-enter",
	[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_PRUNE] = "branch-prune",
	[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_RETURN] = "branch-return",
	[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_UNREACHABLE] = "branch-unreachable",
	[RTE_BPF_VALIDATE_DEBUG_EVENT_JUMP_ALWAYS] = "jump-always",
	[RTE_BPF_VALIDATE_DEBUG_EVENT_JUMP_CONDITIONAL] = "jump-conditional",
};

static const char *const register_names[] = {
	[EBPF_REG_0] = "r0",
	[EBPF_REG_1] = "r1",
	[EBPF_REG_2] = "r2",
	[EBPF_REG_3] = "r3",
	[EBPF_REG_4] = "r4",
	[EBPF_REG_5] = "r5",
	[EBPF_REG_6] = "r6",
	[EBPF_REG_7] = "r7",
	[EBPF_REG_8] = "r8",
	[EBPF_REG_9] = "r9",
	[EBPF_REG_10] = "r10",
};

static const char *const comparison_operator_names[] = {
	[BPF_JEQ] = "==",
	[BPF_JGT] = ">",
	[BPF_JGE] = ">=",
	[EBPF_JNE] = "!=",
	[EBPF_JSGT] = "s>",
	[EBPF_JSGE] = "s>=",
	[EBPF_JLT] = "<",
	[EBPF_JLE] = "<=",
	[EBPF_JSLT] = "s<",
	[EBPF_JSLE] = "s<=",
};

enum point_type {
	POINT_TYPE_BREAK,
	POINT_TYPE_CATCH,
};

/* Local representation of points with additional info for UI. */
struct point_info {
	struct rte_bpf_validate_debug_point *point;
	enum point_type type;
	union {
		uint32_t pc;
		enum rte_bpf_validate_debug_event event;
	};
};

/* Dynamically growing list of point infos. */
struct point_infos {
	struct point_info *elements;
	uint32_t length;
	uint32_t capacity;
};

/* Information about a single conditional or unconditional jump in code path. */
struct branch_info {
	uint32_t jump_pc;
	uint32_t target_pc;
	bool is_conditional;
};

/* Dynamically growing stack to track code path. */
struct branch_stack {
	struct branch_info *branches;
	uint32_t length;
	uint32_t capacity;
};

/* Flags telling if we should validate again, stopping or not at start. */
static bool validate_again;

/* List of point infos; element index is its ID for UI. */
static struct point_infos point_infos;

/* Catchpoint invisible to the user used for step-by-step validation. */
static struct rte_bpf_validate_debug_point *step_point;

/* Tracking entered branches. */
static struct branch_stack branch_stack;
static uint32_t pending_jump_pc = UINT32_MAX;
static struct rte_bpf_validate_debug_point *jump_always_step_point;

static int
step_cb(struct rte_bpf_validate_debug *debug, void *ctx);
static int
point_cb(struct rte_bpf_validate_debug *debug, void *ctx);

/* Find index of the string in the list. */
static int
find_name(const char *name, const char *const *names, int nb_names)
{
	if (nb_names < 0)
		return -EINVAL;

	for (int index = 0; index != nb_names; ++index)
		if (names[index] != NULL && strcmp(names[index], name) == 0)
			return index;

	return -ENOENT;
}

/* Convert event name to its enum value. */
static enum rte_bpf_validate_debug_event
parse_event_name(const char *name)
{
	const int rc = find_name(name, event_names, RTE_DIM(event_names));
	if (rc < 0)
		PRINTLN("Error: invalid event name.");

	return rc;
}

/* Convert comparison operator name to opcode. */
static int
parse_comparison_operator(const char *name)
{
	const int rc = find_name(name, comparison_operator_names,
		RTE_DIM(comparison_operator_names));
	if (rc < 0)
		PRINTLN("Error: invalid comparison operator name.");

	return rc;
}

/* Convert register name to its number. */
static int
parse_register(const char *name)
{
	const int rc = find_name(name, register_names, RTE_DIM(register_names));
	if (rc < 0)
		PRINTLN("Error: invalid register name.");

	return rc;
}

/* Free local list of point infos (do not destroy points). */
static void
point_infos_free(void)
{
	free(point_infos.elements);
	point_infos = (struct point_infos){};
}

/* Free local branch stack. */
static void
branch_stack_free(void)
{
	free(branch_stack.branches);
	branch_stack = (struct branch_stack){};
}

/* Return existing element from the list of point infos. */
static struct point_info *
point_infos_at(uint32_t point_number)
{
	RTE_ASSERT(point_number < point_infos.length);
	RTE_ASSERT(point_infos.elements[point_number].point != NULL);
	return &point_infos.elements[point_number];
}

/* Print existing element from the list of point infos. */
static void
point_infos_print_at(uint32_t point_number)
{
	const struct point_info *const point_info = point_infos_at(point_number);

	switch (point_info->type) {
	case POINT_TYPE_BREAK:
		PRINTLN("Breakpoint %d at %d.", point_number,
			point_info->pc);
		break;
	case POINT_TYPE_CATCH:
		PRINTLN("Catchpoint %d on %s.", point_number,
			event_names[point_info->event]);
		break;
	default:
		PRINTLN("Point %d of unknown type", point_number);
		break;
	}
}

/* Print all point infos. */
static int
point_infos_print_all(void)
{
	uint32_t nb_printed = 0;
	for (uint32_t pn = 0; pn != point_infos.length; ++pn) {
		const struct point_info *const point_info =
			&point_infos.elements[pn];
		if (point_info->point != NULL) {
			point_infos_print_at(pn);
			++nb_printed;
		}
	}

	if (nb_printed == 0) {
		PRINTLN("No breakpoints or catchpoints set.");
		return -ENOENT;
	}

	return nb_printed;
}

/* Allocate space for a new element in the list of point infos. */
static uint32_t
point_infos_append(void)
{
	if (point_infos.length == point_infos.capacity) {
		/* Set to initial capacity or double previous one. */
		point_infos.capacity = RTE_MAX(INITIAL_CAPACITY,
			point_infos.capacity * 2);
		point_infos.elements = realloc(point_infos.elements,
			point_infos.capacity * sizeof(point_infos.elements[0]));
		RTE_VERIFY(point_infos.elements != NULL);
	}
	return point_infos.length++;
}

/* Allocate space for a new element in the branch stack. */
static void
branch_stack_append(const struct branch_info *branch)
{
	if (branch_stack.length == branch_stack.capacity) {
		/* Set to initial capacity or double previous one. */
		branch_stack.capacity = RTE_MAX(INITIAL_CAPACITY,
			branch_stack.capacity * 2);
		branch_stack.branches = realloc(branch_stack.branches,
			branch_stack.capacity * sizeof(branch_stack.branches[0]));
		RTE_VERIFY(branch_stack.branches != NULL);
	}
	branch_stack.branches[branch_stack.length++] = *branch;
}

/* Destroy existing element from the list of point infos, printing it first. */
static void
point_infos_destroy_existing(uint32_t point_number)
{
	point_infos_print_at(point_number);

	struct point_info *const point_info = point_infos_at(point_number);
	rte_bpf_validate_debug_point_destroy(point_info->point);
	*point_info = (struct point_info){};
}

/* Destroy point with specified number if it exists. */
static int
point_infos_destroy_at(uint32_t point_number)
{
	if (point_number >= point_infos.length ||
			point_infos.elements[point_number].point == NULL) {
		PRINTLN("No breakpoint number %d.", point_number);
		return -ENOENT;
	}

	point_infos_destroy_existing(point_number);
	return 1;
}

/* Destroy all point infos. */
static void
point_infos_destroy_all(void)
{
	uint32_t nb_destroyed = 0;
	for (uint32_t pn = 0; pn < point_infos.length; ++pn) {
		const struct point_info *const point_info =
			&point_infos.elements[pn];
		if (point_info->point != NULL) {
			point_infos_destroy_existing(pn);
			++nb_destroyed;
		}
	}

	if (nb_destroyed == 0)
		PRINTLN("No breakpoints or catchpoints set.");
}

/* Destroy all breakpoints at specified location. */
static void
point_infos_destroy_breakpoints(uint32_t pc)
{
	uint32_t nb_destroyed = 0;
	for (uint32_t pn = 0; pn < point_infos.length; ++pn) {
		const struct point_info *const point_info =
			&point_infos.elements[pn];
		if (point_info->point != NULL &&
				point_info->type == POINT_TYPE_BREAK &&
				point_info->pc == pc) {
			point_infos_destroy_existing(pn);
			++nb_destroyed;
		}
	}

	if (nb_destroyed == 0)
		PRINTLN("No breakpoint at %u.", pc);
}

/* Destroy all catchpoints for specified event. */
static void
point_infos_destroy_catchpoints(int event)
{
	if (event < 0)
		/* Error was already printed by parse_event_name. */
		return;

	uint32_t nb_destroyed = 0;
	for (uint32_t pn = 0; pn < point_infos.length; ++pn) {
		const struct point_info *const point_info =
			&point_infos.elements[pn];
		if (point_info->point != NULL &&
				point_info->type == POINT_TYPE_CATCH &&
				(int)point_info->event == event) {
			point_infos_destroy_existing(pn);
			++nb_destroyed;
		}
	}

	if (nb_destroyed == 0)
		PRINTLN("No catchpoint on %s.", event_names[event]);
}

/* Create new breakpoint at specified location. */
static int
add_breakpoint(struct rte_bpf_validate_debug *debug, uint32_t nb_ins, uint32_t pc)
{
	const uint32_t point_number = point_infos_append();

	if (pc >= nb_ins) {
		PRINTLN("Error: program only has %u instructions.", nb_ins);
		return -ENOENT;
	}

	struct rte_bpf_validate_debug_point *const point =
		rte_bpf_validate_debug_break(debug, pc,
			&(struct rte_bpf_validate_debug_callback){
				.fn = point_cb,
				.ctx = (void *)(uintptr_t)point_number,
			});
	if (point == NULL) {
		PRINTLN("Library error %d.", rte_errno);
		return -rte_errno;
	}

	point_infos.elements[point_number] = (struct point_info){
			.point = point,
			.type = POINT_TYPE_BREAK,
			.pc = pc,
		};
	point_infos_print_at(point_number);
	return 0;
}

/* Create new catchpoint at specified location. */
static int
add_catchpoint(struct rte_bpf_validate_debug *debug, int event)
{
	if (event < 0)
		/* Error was already printed by parse_event_name. */
		return event;

	const uint32_t point_number = point_infos_append();
	struct rte_bpf_validate_debug_point *const point =
		rte_bpf_validate_debug_catch(debug, event,
			&(struct rte_bpf_validate_debug_callback){
				.fn = point_cb,
				.ctx = (void *)(uintptr_t)point_number,
			});
	if (point == NULL) {
		PRINTLN("Library error %d.", rte_errno);
		return -rte_errno;
	}

	point_infos.elements[point_number] = (struct point_info){
			.point = point,
			.type = POINT_TYPE_CATCH,
			.event = event,
		};
	point_infos_print_at(point_number);
	return 0;
}

static bool
is_step_enabled(void)
{
	return step_point != NULL;
}

/* Enable step-by-step validation: make sure catchpoint is set on step event. */
static int
enable_step(struct rte_bpf_validate_debug *debug)
{
	if (is_step_enabled())
		return 0;

	step_point = rte_bpf_validate_debug_catch(debug,
		RTE_BPF_VALIDATE_DEBUG_EVENT_STEP,
		&(struct rte_bpf_validate_debug_callback){ step_cb });
	if (step_point == NULL) {
		PRINTLN("Library error %d.", rte_errno);
		return -rte_errno;
	}

	return 0;
}

/* Disable step-by-step validation: destroy catchpoint on step event if any. */
static void
disable_step(void)
{
	rte_bpf_validate_debug_point_destroy(step_point);
	step_point = NULL;
}

/* Format and print information about specified frame offset. */
static int
print_frame_offset(struct rte_bpf_validate_debug *debug, int32_t offset)
{
	char *info;
	int info_size, rc;

	if (offset >= 0 || offset % sizeof(uint64_t) != 0) {
		PRINTLN("Invalid frame offset, must be a negative multiple of %zu.",
			sizeof(uint64_t));
		return -EINVAL;
	}

	rc = rte_bpf_validate_debug_format_frame_info(debug, NULL, 0, offset);
	if (rc == -ERANGE) {
		PRINTLN("Offset is out of frame range.");
		return rc;
	}
	if (rc < 0) {
		PRINTLN("Error %d printing information.", -rc);
		return rc;
	}

	info_size = rc + 1;
	info = malloc(info_size);
	RTE_VERIFY(info != NULL);

	rc = rte_bpf_validate_debug_format_frame_info(debug, info, info_size,
		offset);
	if (rc + 1 != info_size) {
		if (rc >= 0) {
			PRINTLN("Expect format return value %d, got %d.",
				info_size, rc);
			rc = -EINVAL;
		} else
			PRINTLN("Error %d printing information.", -rc);
		free(info);
		return rc;
	}

	printf("%5" PRId32 ": \t%s\n", offset, info);
	free(info);
	return 0;
}

/* Format and print informatiion about the frame. */
static int
print_frame(struct rte_bpf_validate_debug *debug)
{
	int32_t frame_size;
	int rc;

	frame_size = rte_bpf_validate_debug_get_frame_size(debug);
	if (frame_size < 0) {
		PRINTLN("Error %d getting frame size.", -frame_size);
		return frame_size;
	}

	for (int32_t neg_ofs = sizeof(uint64_t); neg_ofs <= frame_size;
			neg_ofs += sizeof(uint64_t)) {
		rc = print_frame_offset(debug, -neg_ofs);
		if (rc < 0)
			return rc;
	}

	return 0;
}

/* Format and print informatiion about specified register. */
static int
print_register(struct rte_bpf_validate_debug *debug, int reg)
{
	char *info;
	int info_size, rc;

	if (reg < 0)
		/* Error was already printed by parse_register. */
		return reg;

	rc = rte_bpf_validate_debug_format_register_info(debug, NULL, 0, reg);
	if (rc < 0) {
		PRINTLN("Error %d printing information.", -rc);
		return rc;
	}

	info_size = rc + 1;
	info = malloc(info_size);
	RTE_VERIFY(info != NULL);

	rc = rte_bpf_validate_debug_format_register_info(debug, info, info_size,
		reg);
	if (rc + 1 != info_size) {
		if (rc >= 0) {
			PRINTLN("Expect format return value %d, got %d.",
				info_size, rc);
			rc = -EINVAL;
		} else
			PRINTLN("Error %d printing information.", -rc);
		free(info);
		return rc;
	}

	printf("%5s: \t%s\n", register_names[reg], info);
	free(info);
	return 0;
}

/* Format and print informatiion about all registers. */
static int
print_registers(struct rte_bpf_validate_debug *debug)
{
	int rc = 0;

	for (int reg = 0; reg != EBPF_REG_NUM; ++reg)
		rc = rc < 0 ? rc : print_register(debug, reg);

	return rc;
}

/* List one eBPF program instruction. */
static int
list_one(const struct rte_ebpf_insn *ins, uint32_t nb_ins, uint32_t offset,
	uint32_t pc, const char *comment)
{
	char hexadecimal[256], disassembly[256];

	if (offset >= nb_ins) {
		PRINTLN("Error: program only has %u instructions.", nb_ins);
		return -EINVAL;
	}

	ins += offset;

	if (offset == nb_ins - 1 && rte_bpf_insn_is_wide(ins)) {
		PRINTLN("Error: truncated last instruction.");
		return -EINVAL;
	}

	rte_bpf_format(hexadecimal, sizeof(hexadecimal), ins, 0,
		RTE_BPF_FORMAT_FLAG_HEXADECIMAL |
		RTE_BPF_FORMAT_FLAG_NEVER_WIDE);
	rte_bpf_format(disassembly, sizeof(disassembly), ins, offset,
		RTE_BPF_FORMAT_FLAG_DISASSEMBLY |
		RTE_BPF_FORMAT_FLAG_ABSOLUTE_JUMPS);

	if (comment == NULL)
		comment = "";
	PRINTLN("%2s %10u: \t%s \t%s%s%s",
		offset == pc ? "=>" : "", offset, hexadecimal, disassembly,
		comment[0] != '\0' ? " \t; " : "", comment);

	if (rte_bpf_insn_is_wide(ins)) {
		rte_bpf_format(hexadecimal, sizeof(hexadecimal), ins + 1, 0,
			RTE_BPF_FORMAT_FLAG_HEXADECIMAL |
			RTE_BPF_FORMAT_FLAG_NEVER_WIDE);
		PRINTLN("%15s\t%s", "", hexadecimal);
	}

	return 0;
}

/* List specified range of eBPF program instructions, updating start offset. */
static int
list(const struct rte_ebpf_insn *ins, uint32_t nb_ins, uint32_t *offset,
	uint32_t count, uint32_t pc)
{
	uint32_t local_offset = 0;
	if (offset == NULL)
		offset = &local_offset;

	if (*offset > nb_ins) {
		PRINTLN("Error: program only has %u instructions.", nb_ins);
		return -EINVAL;
	}

	const uint32_t end =
		/* Calculate end in a way preventing overflow: */
		*offset + RTE_MIN(nb_ins - *offset, count);
	while (*offset < end) {
		const int rc = list_one(ins, nb_ins, *offset, pc, NULL);
		if (rc < 0)
			return rc;

		*offset += 1 + rte_bpf_insn_is_wide(&ins[*offset]);
	}

	return 0;
}

/* Print if specified conditional jump _may_ be executed. */
static int
print_if_may(struct rte_bpf_validate_debug *debug, const struct rte_ebpf_insn *jump,
	uint64_t imm64)
{
	const int result = rte_bpf_validate_debug_may_jump(debug, jump, imm64);

	switch (result) {
	case 0:
	case RTE_BPF_VALIDATE_DEBUG_MAY_BE_FALSE:
		PRINTLN("NO");
		break;
	case RTE_BPF_VALIDATE_DEBUG_MAY_BE_TRUE:
	case RTE_BPF_VALIDATE_DEBUG_MAY_BE_FALSE | RTE_BPF_VALIDATE_DEBUG_MAY_BE_TRUE:
		PRINTLN("YES");
		break;
	default:
		PRINTLN("Error %d getting result.", -result);
		break;
	}

	return result;
}

/* Print if specified condition with literal right hand side _may_ be true. */
static int
print_if_may_literal_rhs(struct rte_bpf_validate_debug *debug,
	const struct debug_command_comparison *comparison)
{
	const int lhs = parse_register(comparison->lhs);
	const int op = parse_comparison_operator(comparison->op);
	const int64_t rhs = comparison->literal_rhs;

	if (lhs < 0 || op < 0)
		/* Error was already printed by parse function. */
		return lhs < 0 ? lhs : op;

	return print_if_may(debug, &(struct rte_ebpf_insn){
		.code = BPF_JMP | op | BPF_K,
		.dst_reg = lhs,
	}, /* imm64 = */ rhs);
}

/* Print if specified condition with register right hand side _may_ be true. */
static int
print_if_may_register_rhs(struct rte_bpf_validate_debug *debug,
	const struct debug_command_comparison *comparison)
{
	const int lhs = parse_register(comparison->lhs);
	const int op = parse_comparison_operator(comparison->op);
	const int rhs = parse_register(comparison->register_rhs);

	if (lhs < 0 || op < 0 || rhs < 0)
		/* Error was already printed by parse function. */
		return lhs < 0 ? lhs : op < 0 ? op : rhs;

	return print_if_may(debug, &(struct rte_ebpf_insn){
		.code = BPF_JMP | op | BPF_X,
		.dst_reg = lhs,
		.src_reg = rhs,
	}, /* imm64 = */ 0);
}

/* Return 1 on validation success, 0 on failure, -EAGAIN if still running. */
static int
get_validation_success(struct rte_bpf_validate_debug *debug)
{
	int validation_result, rc;

	rc = rte_bpf_validate_debug_get_validation_result(debug,
		&validation_result);
	return rc < 0 ? rc : (validation_result >= 0);
}

static void
print_status(const struct rte_ebpf_insn *ins, uint32_t nb_ins,
	int validation_success, uint32_t pc)
{
	if (validation_success == 1) {
		PRINTLN("Validation succeeded.");
		return;
	}

	list_one(ins, nb_ins, pc, pc, NULL);

	if (validation_success == 0)
		PRINTLN("Validation failed.");
}

static void
debug_command_where(const struct rte_ebpf_insn *ins, uint32_t nb_ins,
	int validation_success, uint32_t pc)
{
	for (uint32_t bi = 0; bi != branch_stack.length; ++bi) {
		const uint32_t jump_pc = branch_stack.branches[bi].jump_pc;
		const uint32_t target_pc = branch_stack.branches[bi].target_pc;
		const char *const comment =
			!branch_stack.branches[bi].is_conditional ? NULL :
			target_pc == jump_pc + 1 ? "fallen-through" : "taken";
		list_one(ins, nb_ins, jump_pc, UINT32_MAX, comment);
	}
	print_status(ins, nb_ins, validation_success, pc);
}

/* Return true if pc is defined, otherwise print an error message. */
static bool
ensure_pc_defined(uint32_t pc)
{
	if (pc != UINT32_MAX)
		return true;

	PRINTLN("No current instruction.");
	return false;
}

/* Return true if still validating, otherwise print an error message. */
static bool
ensure_still_validating(int validation_success)
{
	if (validation_success == -EAGAIN)
		return true;

	PRINTLN("Finished, use `start` or `run` to restart, `quit` to quit.");
	return false;
}

/* Step-by-step validation callback: read and process user commands. */
static int
step_cb(struct rte_bpf_validate_debug *debug, __rte_unused void *ctx)
{
	int rc;
	int validation_success;
	const struct rte_ebpf_insn *ins;
	uint32_t nb_ins, pc, list_offset;

	validate_again = false;

	rc = rte_bpf_validate_debug_get_ins(debug, &ins, &nb_ins);
	if (rc < 0) {
		PRINTLN("Error %d getting program instructions.", -rc);
		return rc;
	}

	validation_success = get_validation_success(debug);
	if (validation_success < 0 && validation_success != -EAGAIN) {
		PRINTLN("Error %d getting validation result.",
			-validation_success);
		return validation_success;
	}

	pc = validation_success == 1 ? UINT32_MAX :
		rte_bpf_validate_debug_get_pc(debug);

	print_status(ins, nb_ins, validation_success, pc);

	list_offset = pc;

	while (true) {
		switch (debug_command_get(PROMPT)) {
		case DEBUG_COMMAND_BREAK:
			if (ensure_pc_defined(pc))
				add_breakpoint(debug, nb_ins, pc);
			continue;
		case DEBUG_COMMAND_BREAK_PC:
			add_breakpoint(debug, nb_ins, debug_command_parsed.pc);
			continue;
		case DEBUG_COMMAND_CATCH:
			add_catchpoint(debug,
				parse_event_name(debug_command_parsed.event));
			continue;
		case DEBUG_COMMAND_CLEAR:
			if (ensure_pc_defined(pc))
				point_infos_destroy_breakpoints(pc);
			continue;
		case DEBUG_COMMAND_CLEAR_EVENT:
			point_infos_destroy_catchpoints(
				parse_event_name(debug_command_parsed.event));
			continue;
		case DEBUG_COMMAND_CLEAR_PC:
			point_infos_destroy_breakpoints(
				debug_command_parsed.pc);
			continue;
		case DEBUG_COMMAND_CONTINUE:
			if (ensure_still_validating(validation_success)) {
				disable_step();
				return 0;
			}
			continue;
		case DEBUG_COMMAND_DELETE:
			point_infos_destroy_all();
			continue;
		case DEBUG_COMMAND_DELETE_NUMBER:
			point_infos_destroy_at(
				debug_command_parsed.point_number);
			continue;
		case DEBUG_COMMAND_INFO_FRAME:
			print_frame(debug);
			continue;
		case DEBUG_COMMAND_INFO_FRAME_OFFSET:
			print_frame_offset(debug,
				debug_command_parsed.frame_offset);
			continue;
		case DEBUG_COMMAND_INFO_POINTS:
			point_infos_print_all();
			continue;
		case DEBUG_COMMAND_INFO_REGISTER:
			print_register(debug,
				parse_register(debug_command_parsed.register_));
			continue;
		case DEBUG_COMMAND_INFO_REGISTERS:
			print_registers(debug);
			continue;
		case DEBUG_COMMAND_LIST:
			if (ensure_pc_defined(pc))
				list(ins, nb_ins, &list_offset, 10, pc);
			continue;
		case DEBUG_COMMAND_LIST_COUNT:
			if (ensure_pc_defined(pc))
				list(ins, nb_ins, &list_offset,
					debug_command_parsed.instruction_count, pc);
			continue;
		case DEBUG_COMMAND_LIST_PROGRAM:
			if (ensure_pc_defined(pc))
				list(ins, nb_ins, NULL, UINT32_MAX, pc);
			continue;
		case DEBUG_COMMAND_MAY_LITERAL_RHS:
			print_if_may_literal_rhs(debug,
				&debug_command_parsed.comparison);
			continue;
		case DEBUG_COMMAND_MAY_REGISTER_RHS:
			print_if_may_register_rhs(debug,
				&debug_command_parsed.comparison);
			continue;
		case DEBUG_COMMAND_EOF:
		case DEBUG_COMMAND_QUIT:
			PRINTLN("Quitting...");
			return validation_success == -EAGAIN ? -ECANCELED : 0;
		case DEBUG_COMMAND_RUN:
			PRINTLN("Re-running...");
			disable_step();
			validate_again = true;
			return -ECANCELED;
		case DEBUG_COMMAND_START:
			PRINTLN("Re-starting...");
			rc = enable_step(debug);
			if (rc < 0)
				return rc;
			validate_again = true;
			return -ECANCELED;
		case DEBUG_COMMAND_STEP:
			if (ensure_still_validating(validation_success))
				return 0;
			continue;
		case DEBUG_COMMAND_WHERE:
			debug_command_where(ins, nb_ins, validation_success, pc);
			continue;
		default:
			PRINTLN("INTERNAL ERROR");
			return -ENOTSUP;
		}
	}
}

/* Any point callback: print it and enable step-by-step validation. */
static int
point_cb(struct rte_bpf_validate_debug *debug, void *ctx)
{
	const uint32_t point_number = (uintptr_t)ctx;
	point_infos_print_at(point_number);
	return enable_step(debug);
}

/* Branch stack machinery. */

static void
clear_jump_always_step_point(void)
{
	rte_bpf_validate_debug_point_destroy(jump_always_step_point);
	jump_always_step_point = NULL;
}

static int
reset_branch_tracking(struct rte_bpf_validate_debug *debug __rte_unused,
	void *ctx __rte_unused)
{
	branch_stack.length = 0;
	pending_jump_pc = UINT32_MAX;
	clear_jump_always_step_point();
	return 0;
}

/*
 * Handling the jump-always instructions:
 * - upon a jump-always event save the pc and set a custom step callback;
 * - ignore the first custom step callback call (still on the same jump);
 * - on the second call push the jump pc into stack and delete the callback;
 */

static int
jump_always_step_cb(struct rte_bpf_validate_debug *debug, void *ctx __rte_unused)
{
	/* Step event is also emitted at the end of the jump instruction itself. */
	if (rte_bpf_validate_debug_get_pc(debug) == pending_jump_pc)
		return 0;

	branch_stack_append(&(struct branch_info){
		.jump_pc = pending_jump_pc,
		.target_pc = rte_bpf_validate_debug_get_pc(debug),
		.is_conditional = false,
	});
	clear_jump_always_step_point();
	return 0;
}

static int
jump_always_cb(struct rte_bpf_validate_debug *debug, void *ctx __rte_unused)
{
	pending_jump_pc = rte_bpf_validate_debug_get_pc(debug);
	clear_jump_always_step_point();
	jump_always_step_point = rte_bpf_validate_debug_catch(debug,
		RTE_BPF_VALIDATE_DEBUG_EVENT_STEP,
		&(struct rte_bpf_validate_debug_callback){ jump_always_step_cb });
	return 0;
}

/*
 * Handling the jump-conditional instructions:
 * - upon a jump-conditional event save the pc;
 * - upon a branch-enter event push the conditional jump pc into stack;
 * - upon a branch-return event pop conditional jump and all unconditional jumps
 *   preceding it from the stack;
 * - during step execution notify the user about the step events;
 */

static int
jump_conditional_cb(struct rte_bpf_validate_debug *debug, void *ctx __rte_unused)
{
	pending_jump_pc = rte_bpf_validate_debug_get_pc(debug);
	return 0;
}

static int
branch_enter_cb(struct rte_bpf_validate_debug *debug, void *ctx __rte_unused)
{
	branch_stack_append(&(struct branch_info){
		.jump_pc = pending_jump_pc,
		.target_pc = rte_bpf_validate_debug_get_pc(debug),
		.is_conditional = true,
	});
	if (is_step_enabled())
		PRINTLN("Entered new branch at pc %u.", pending_jump_pc);
	return 0;
}

static int
branch_return_cb(struct rte_bpf_validate_debug *debug __rte_unused,
	void *ctx __rte_unused)
{
	clear_jump_always_step_point();

	while (branch_stack.length > 0) {
		const struct branch_info branch_info =
			branch_stack.branches[--branch_stack.length];
		pending_jump_pc = branch_info.jump_pc;
		if (branch_info.is_conditional)
			break;
	}
	if (is_step_enabled())
		PRINTLN("Returned from branch at pc %u.", pending_jump_pc);
	return 0;
}

/* Notify user when skipping branches in step mode. */

static int
branch_prune_cb(struct rte_bpf_validate_debug *debug __rte_unused,
	void *ctx __rte_unused)
{
	if (is_step_enabled())
		PRINTLN("Prunned branch at pc %u.", pending_jump_pc);
	return 0;
}

static int
branch_unreachable_cb(struct rte_bpf_validate_debug *debug __rte_unused,
	void *ctx __rte_unused)
{
	if (is_step_enabled())
		PRINTLN("Unreachable branch at pc %u.", pending_jump_pc);
	return 0;
}

/* Global initialization and cleanup functions. */

static int
set_callbacks(struct rte_bpf_validate_debug *debug)
{
	static const struct rte_bpf_validate_debug_callback events_callback[
			RTE_BPF_VALIDATE_DEBUG_EVENT_END] = {
		[RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_START] = { reset_branch_tracking },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_SUCCESS] = { step_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_VALIDATION_FAILURE] = { step_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_JUMP_CONDITIONAL] = { jump_conditional_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_JUMP_ALWAYS] = { jump_always_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_ENTER] = { branch_enter_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_RETURN] = { branch_return_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_PRUNE] = { branch_prune_cb },
		[RTE_BPF_VALIDATE_DEBUG_EVENT_BRANCH_UNREACHABLE] = { branch_unreachable_cb },
	};

	for (enum rte_bpf_validate_debug_event event = 0;
			event < RTE_BPF_VALIDATE_DEBUG_EVENT_END; ++event)
		if (events_callback[event].fn != NULL &&
				rte_bpf_validate_debug_catch(debug, event,
					&events_callback[event]) == NULL)
			return -rte_errno;

	return 0;
}

struct rte_bpf_validate_debug *
debug_create(void)
{

	int rc = 0;

	struct rte_bpf_validate_debug *const debug = rte_bpf_validate_debug_create();
	if (debug == NULL)
		rc = -rte_errno;

	rc = rc < 0 ? rc : set_callbacks(debug);

	rc = rc < 0 ? rc : enable_step(debug);

	if (rc < 0) {
		debug_destroy(debug);
		rte_errno = -rc;
		return NULL;
	}

	return debug;
}

void
debug_destroy(struct rte_bpf_validate_debug *debug)
{
	/* No need to destroy created points, destroying debug will do it. */
	point_infos_free();
	branch_stack_free();
	rte_bpf_validate_debug_destroy(debug);
}

bool
debug_validate_again(void)
{
	return validate_again;
}
