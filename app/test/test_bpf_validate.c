/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "test.h"

#include <bpf_def.h>
#include <rte_bpf.h>
#include <rte_bpf_validate_debug.h>
#include <rte_errno.h>

/*
 * Tests of BPF validation.
 */

extern int test_bpf_validate_logtype;
#define RTE_LOGTYPE_TEST_BPF_VALIDATE test_bpf_validate_logtype
#define TEST_LOG_LINE(level, ...) \
	RTE_LOG_LINE(level, TEST_BPF_VALIDATE, "" __VA_ARGS__)

RTE_LOG_REGISTER(test_bpf_validate_logtype, test.bpf_validate, NOTICE);

/* Special value indicating that program counter variable is not being used. */
#define NO_PROGRAM_COUNTER UINT32_MAX

/* Special value indicating that register variable is not being used. */
#define NO_REGISTER UINT8_MAX

/* Sizes of text buffers used for formatting various debug outputs. */
#define VALUE_FORMAT_BUFFER_SIZE 32
#define INTERVAL_FORMAT_BUFFER_SIZE 64
#define REGISTER_FORMAT_BUFFER_SIZE 256
#define DISASSEMBLY_FORMAT_BUFFER_SIZE 64

#define COMPARISON_INDEX_IMMEDIATE RTE_BIT32(0)
#define COMPARISON_INDEX_GREATER   RTE_BIT32(1)
#define COMPARISON_INDEX_INCLUSIVE RTE_BIT32(2)
#define COMPARISON_INDEX_SIGNED    RTE_BIT32(3)

/* List comparison opcodes to make their index bits match constants above.  */
static const uint8_t comparisons_opcode[] = {
	(BPF_JMP | EBPF_JLT  | BPF_X),
	(BPF_JMP | EBPF_JLT  | BPF_K),
	(BPF_JMP |  BPF_JGT  | BPF_X),
	(BPF_JMP |  BPF_JGT  | BPF_K),
	(BPF_JMP | EBPF_JLE  | BPF_X),
	(BPF_JMP | EBPF_JLE  | BPF_K),
	(BPF_JMP |  BPF_JGE  | BPF_X),
	(BPF_JMP |  BPF_JGE  | BPF_K),
	(BPF_JMP | EBPF_JSLT | BPF_X),
	(BPF_JMP | EBPF_JSLT | BPF_K),
	(BPF_JMP | EBPF_JSGT | BPF_X),
	(BPF_JMP | EBPF_JSGT | BPF_K),
	(BPF_JMP | EBPF_JSLE | BPF_X),
	(BPF_JMP | EBPF_JSLE | BPF_K),
	(BPF_JMP | EBPF_JSGE | BPF_X),
	(BPF_JMP | EBPF_JSGE | BPF_K),
};

/* Interval bounded by two signed values, inclusive; min <= max. */
struct signed_interval {
	int64_t min;
	int64_t max;
};

/* Interval bounded by two unsigned values, inclusive; min <= max. */
struct unsigned_interval {
	uint64_t min;
	uint64_t max;
};

/*
 * Expected interval of register values.
 *
 * If `is_defined` is not set, domain is considered to be unused in verification
 * parameters (instruction is not accessing corresponding register).
 * It's not the same as `unknown` domain which describes register that is being
 * used but can hold any value.
 *
 * Flag `is_pointer` tells if the interval is relative to some memory area base.
 */
struct domain {
	bool is_defined;
	bool is_pointer;
	struct signed_interval s;
	struct unsigned_interval u;
};

/* Expected validation state at certain point. */
struct state {
	/* Specifies that the branch is dynamically unreachable. */
	bool is_unreachable;
	struct domain dst;
	struct domain src;
};

/* Instruction verification parameters. */
struct verify_instruction_param {
	struct ebpf_insn tested_instruction;
	size_t area_size;
	/* States just before the tested instruction, just after, or if jumped. */
	struct state pre;
	struct state post;
	struct state jump;
};

/* Point (pre/post/jump) specific verification context. */
struct point_context {
	uint32_t program_counter;
	uint32_t hit_count;
	char formatted_dst[REGISTER_FORMAT_BUFFER_SIZE];
	char formatted_src[REGISTER_FORMAT_BUFFER_SIZE];
};

/* Verification context. */
struct verify_instruction_context {
	struct verify_instruction_param prm;
	/* Allocation of registers in the generated program. */
	uint8_t base_reg;
	uint8_t dst_reg;
	uint8_t src_reg;
	uint8_t tmp_reg;
	/* Number of times invalid state callback was called. */
	uint32_t invalid_state_count;
	/* Contexts just before the tested instruction, just after, or if jumped. */
	struct point_context pre;
	struct point_context post;
	struct point_context jump;
};

/* Domain with both signed and unsigned interval having maximum size. */
static const struct domain unknown = {
	.is_defined = true,
	.s = { .min = INT64_MIN, .max = INT64_MAX },
	.u = { .min = 0, .max = UINT64_MAX },
};

/* Unreachable state. */
static const struct state unreachable = {
	.is_unreachable = true,
};


/* BUILDING DOMAINS */

/* Create domain from singleton interval. */
static struct domain
make_singleton_domain(uint64_t value)
{
	return (struct domain){
		.is_defined = true,
		.s = { .min = value, .max = value },
		.u = { .min = value, .max = value },
	};
}

/* Create domain from signed interval. */
static struct domain
make_signed_domain(int64_t min, int64_t max)
{
	RTE_VERIFY(min <= max);
	return (struct domain){
		.is_defined = true,
		.s = { .min = min, .max = max },
		.u = (min ^ max) >= 0 ?
			(struct unsigned_interval){ .min = min, .max = max } :
			unknown.u,
	};
}

/* Create domain from unsigned interval. */
static struct domain
make_unsigned_domain(uint64_t min, uint64_t max)
{
	RTE_VERIFY(min <= max);
	return (struct domain){
		.is_defined = true,
		.s = (int64_t)(min ^ max) >= 0 ?
			(struct signed_interval){ .min = min, .max = max } :
			unknown.s,
		.u = { .min = min, .max = max },
	};
}

/* Create domain from signed interval. */
static struct domain
make_pointer_domain(int64_t min, int64_t max)
{
	struct domain result = make_signed_domain(min, max);
	result.is_pointer = true;
	return result;
}

/* Return true if domain is a scalar or pointer singleton. */
static bool
domain_is_singleton(const struct domain *domain)
{
	return domain->s.min == domain->s.max &&
		(uint64_t)domain->s.max == domain->u.min &&
		domain->u.min == domain->u.max;
}

/* Print error message into buffer if rc signifies error or overflow. */
static void
handle_format_errors(char *buffer, size_t bufsz, int rc)
{
	if (rc < 0)
		snprintf(buffer, bufsz, "FORMAT ERROR %d!", -rc);
	else if ((unsigned int)rc >= bufsz)
		snprintf(buffer, bufsz, "FORMAT OVERFLOW!");
}

/* Format register information into provided buffer and return the buffer. */
static const char *
format_value(char *buffer, size_t bufsz, char format, uint64_t value)
{
	handle_format_errors(buffer, bufsz,
		rte_bpf_validate_debug_format_value(buffer, bufsz, format, value));
	return buffer;
}

/* Format register information into provided buffer and return the buffer. */
static const char *
format_interval(char *buffer, size_t bufsz, char format, uint64_t min, uint64_t max)
{
	handle_format_errors(buffer, bufsz,
		rte_bpf_validate_debug_format_interval(buffer, bufsz, format, min, max));
	return buffer;
}

/* Format domain information into provided buffer and return the buffer. */
static const char *
format_domain(char *buffer, size_t bufsz, const struct domain *domain)
{
	char signed_buffer[INTERVAL_FORMAT_BUFFER_SIZE];
	char unsigned_buffer[INTERVAL_FORMAT_BUFFER_SIZE];

	const int rc = !domain->is_defined ?
		snprintf(buffer, bufsz, "UNDEFINED") :
		snprintf(buffer, bufsz, "%s %s INTERSECT %s",
			domain->is_pointer ? "pointer" : "scalar",
			format_interval(signed_buffer, sizeof(signed_buffer), 'd',
				domain->s.min, domain->s.max),
			format_interval(unsigned_buffer, sizeof(unsigned_buffer), 'x',
				domain->u.min, domain->u.max));

	handle_format_errors(buffer, bufsz, rc < 0 ? -errno : rc);

	return buffer;
}

/* Format register information into provided buffer and return the buffer. */
static const char *
format_register(struct rte_bpf_validate_debug *debug, char *buffer, size_t bufsz, uint8_t reg)
{
	handle_format_errors(buffer, bufsz,
		rte_bpf_validate_debug_format_register_info(debug, buffer, bufsz, reg));
	return buffer;
}


/* CHECKING REGISTER ACTUAL DOMAINS */

/* Return true the specified conditional jump _may_ occur at current state. */
static bool
may_jump(const struct rte_bpf_validate_debug *debug,
	const struct ebpf_insn *jump, uint64_t imm64)
{
	const int result = rte_bpf_validate_debug_may_jump(debug, jump, imm64);
	RTE_VERIFY(result >= 0);
	return (result & RTE_BPF_VALIDATE_DEBUG_MAY_BE_TRUE) != 0;
}

/* Check interval of the register interpreted as signed scalar. */
static int
check_signed_interval(struct rte_bpf_validate_debug *debug,
	uint8_t reg, struct signed_interval interval)
{
	char buffer[VALUE_FORMAT_BUFFER_SIZE];

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | EBPF_JSLT | BPF_K),
			.dst_reg = reg,
		}, interval.min),
		false,
		"r%hhu s< %s is impossible", reg,
		format_value(buffer, sizeof(buffer), 'd', interval.min));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JEQ | BPF_K),
			.dst_reg = reg,
		}, interval.min),
		true,
		"r%hhu == %s is possible", reg,
		format_value(buffer, sizeof(buffer), 'd', interval.min));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JEQ | BPF_K),
			.dst_reg = reg,
		}, interval.max),
		true,
		"r%hhu == %s is possible", reg,
		format_value(buffer, sizeof(buffer), 'd', interval.max));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | EBPF_JSGT | BPF_K),
			.dst_reg = reg,
		}, interval.max),
		false,
		"r%hhu s> %s is impossible", reg,
		format_value(buffer, sizeof(buffer), 'd', interval.max));

	return TEST_SUCCESS;
}

/* Check interval of the register interpreted as unsigned scalar. */
static int
check_unsigned_interval(struct rte_bpf_validate_debug *debug,
	uint8_t reg, struct unsigned_interval interval)
{
	char buffer[VALUE_FORMAT_BUFFER_SIZE];

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | EBPF_JLT | BPF_K),
			.dst_reg = reg,
		}, interval.min),
		false,
		"r%hhu u< %s is impossible", reg,
		format_value(buffer, sizeof(buffer), 'x', interval.min));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JEQ | BPF_K),
			.dst_reg = reg,
		}, interval.min),
		true,
		"r%hhu == %s is possible", reg,
		format_value(buffer, sizeof(buffer), 'x', interval.min));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JEQ | BPF_K),
			.dst_reg = reg,
		}, interval.max),
		true,
		"r%hhu == %s is possible", reg,
		format_value(buffer, sizeof(buffer), 'x', interval.max));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JGT | BPF_K),
			.dst_reg = reg,
		}, interval.max),
		false,
		"r%hhu u> %s is impossible", reg,
		format_value(buffer, sizeof(buffer), 'x', interval.max));

	return TEST_SUCCESS;
}

/* Check interval of the register relative to the base register. */
static int
check_relative_interval(struct rte_bpf_validate_debug *debug,
	uint8_t reg, struct signed_interval interval, uint8_t base_reg)
{
	char buffer[VALUE_FORMAT_BUFFER_SIZE];

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
			.dst_reg = reg,
			.src_reg = base_reg,
		}, interval.min),
		false,
		"r%hhu u< r%hhu + %s is impossible", reg, base_reg,
		format_value(buffer, sizeof(buffer), 'd', interval.min));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JEQ | BPF_X),
			.dst_reg = reg,
			.src_reg = base_reg,
		}, interval.min),
		true,
		"r%hhu == r%hhu + %s is possible", reg, base_reg,
		format_value(buffer, sizeof(buffer), 'd', interval.min));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JEQ | BPF_X),
			.dst_reg = reg,
			.src_reg = base_reg,
		}, interval.max),
		true,
		"r%hhu == r%hhu + %s is possible", reg, base_reg,
		format_value(buffer, sizeof(buffer), 'd', interval.max));

	TEST_ASSERT_EQUAL(may_jump(debug,
		&(struct ebpf_insn){
			.code = (BPF_JMP | BPF_JGT | BPF_X),
			.dst_reg = reg,
			.src_reg = base_reg,
		}, interval.max),
		false,
		"r%hhu u> r%hhu + %s is impossible", reg, base_reg,
		format_value(buffer, sizeof(buffer), 'd', interval.max));

	return TEST_SUCCESS;
}

/*
 * Check access of the register interpreted as pointer.
 *
 * Unlike other similar functions, min > max is not a problem here,
 * so either signed or unsigned pair can be passed without any issues.
 *
 * This is the reason we are not using signed_interval or unsigned_interval here
 * to avoid confusion.
 */
static int
check_pointer_access(struct rte_bpf_validate_debug *debug, uint8_t reg,
	uint64_t min, uint64_t max, size_t area_size)
{
	char buffer[VALUE_FORMAT_BUFFER_SIZE];

	/* Start and end of the valid offsets window (unless empty). */
	const uint64_t window_begin = -min;
	const uint64_t window_end = area_size - max;

	/* Only have accessible bytes if the interval is smaller than the area. */
	const uint64_t interval_size = max - min;
	const bool window_empty = (interval_size >= area_size);

	TEST_ASSERT_EQUAL(rte_bpf_validate_debug_can_access(debug,
		&(struct ebpf_insn){
			.code = (BPF_LDX | BPF_B | BPF_MEM),
			.src_reg = reg
		}, window_begin - 1),
		false,
		"r%hhu + %s (before window begin) dereference is invalid", reg,
		format_value(buffer, sizeof(buffer), 'd', window_begin - 1));

	TEST_ASSERT_EQUAL(rte_bpf_validate_debug_can_access(debug,
		&(struct ebpf_insn){
			.code = (BPF_LDX | BPF_B | BPF_MEM),
			.src_reg = reg
		}, window_begin),
		!window_empty,
		"r%hhu + %s (after window begin) dereference is %s", reg,
		format_value(buffer, sizeof(buffer), 'd', window_begin),
		window_empty ? "invalid for empty window" : "valid");

	TEST_ASSERT_EQUAL(rte_bpf_validate_debug_can_access(debug,
		&(struct ebpf_insn){
			.code = (BPF_LDX | BPF_B | BPF_MEM),
			.src_reg = reg
		}, window_end - 1),
		!window_empty,
		"r%hhu + %s (before window end) dereference is %s", reg,
		format_value(buffer, sizeof(buffer), 'd', window_end - 1),
		window_empty ? "invalid for empty window" : "valid");

	TEST_ASSERT_EQUAL(rte_bpf_validate_debug_can_access(debug,
		&(struct ebpf_insn){
			.code = (BPF_LDX | BPF_B | BPF_MEM),
			.src_reg = reg
		}, window_end),
		false,
		"r%hhu + %s (after window end) dereference is invalid", reg,
		format_value(buffer, sizeof(buffer), 'd', window_end));

	return TEST_SUCCESS;
}

/* Check domain of the register interpreted as absolute value. */
static int
check_scalar_domain(struct rte_bpf_validate_debug *debug, uint8_t reg,
	const struct domain *domain)
{
	TEST_ASSERT_SUCCESS(
		check_signed_interval(debug, reg, domain->s),
		"absolute signed interval check");

	TEST_ASSERT_SUCCESS(
		check_unsigned_interval(debug, reg, domain->u),
		"absolute unsigned interval check");

	return TEST_SUCCESS;
}

/* Check domain of the register interpreted as relative pointer. */
static int
check_pointer_domain(struct rte_bpf_validate_debug *debug, uint8_t reg,
	const struct domain *domain, uint8_t base_reg, size_t area_size)
{
	TEST_ASSERT_SUCCESS(
		check_relative_interval(debug, reg, domain->s, base_reg),
		"relative interval check");

	TEST_ASSERT_SUCCESS(
		check_pointer_access(debug, reg, domain->s.min, domain->s.max,
			area_size),
		"pointer signed access check");

	TEST_ASSERT_SUCCESS(
		check_pointer_access(debug, reg, domain->u.min, domain->u.max,
			area_size),
		"pointer unsigned access check");

	return TEST_SUCCESS;
}

/* Check domain of the register and format the values in case of an error. */
static int
check_domain(struct rte_bpf_validate_debug *debug, uint8_t reg,
	const struct domain *domain, uint8_t base_reg, size_t area_size)
{
	char buffer[REGISTER_FORMAT_BUFFER_SIZE];

	const int rc = domain->is_pointer ?
		check_pointer_domain(debug, reg, domain, base_reg, area_size) :
		check_scalar_domain(debug, reg, domain);

	if (rc != TEST_SUCCESS) {
		TEST_LOG_LINE(WARNING, "\tExpected: r%hhu = %s", reg,
			format_domain(buffer, sizeof(buffer), domain));

		TEST_LOG_LINE(WARNING, "\tFound: r%hhu = %s", reg,
			format_register(debug, buffer, sizeof(buffer), reg));
	}

	return rc;
}


/* GENERATING TEST PROGRAM */

static bool
fits_in_imm32(int64_t value)
{
	return value >= INT32_MIN && value <= INT32_MAX;
}

/* Load constant into the register.  */
static void
load_constant(struct ebpf_insn **ins, uint8_t reg, int64_t value)
{
	if (fits_in_imm32(value)) {
		*(*ins)++ = (struct ebpf_insn){
			.code = (EBPF_ALU64 | EBPF_MOV | BPF_K),
			.dst_reg = reg,
			.imm = (int32_t)value,
		};
	} else {
		/* Load imm64 into tmp_reg using wide load, lower bits first... */
		*(*ins)++ = (struct ebpf_insn){
			.code = (BPF_LD | BPF_IMM | EBPF_DW),
			.dst_reg = reg,
			.imm = (uint32_t)value,
		};
		/* ... then higher bits. */
		*(*ins)++ = (struct ebpf_insn){
			.imm = (uint32_t)(value >> 32),
		};
	}
}

/*
 * Compare specified register to value and jump.
 *
 * Jump offset is not filled and should be patched in by the caller.
 */
static void
compare_and_jump(struct ebpf_insn **ins, uint8_t op, uint8_t reg,
	int64_t value, uint8_t tmp_reg)
{
	if (fits_in_imm32(value)) {
		/* Jump on specified condition between reg and immediate. */
		*(*ins)++ = (struct ebpf_insn){
			.code = (BPF_JMP | op | BPF_K),
			.dst_reg = reg,
			.imm = (int32_t)value,
		};
	} else {
		/* Load value into tmp_reg. */
		load_constant(ins, tmp_reg, value);

		/* Jump on specified condition between reg and tmp_reg. */
		*(*ins)++ = (struct ebpf_insn){
			.code = (BPF_JMP | op | BPF_X),
			.dst_reg = reg,
			.src_reg = tmp_reg,
		};
	}
}

/*
 * Prepare register to be in the specified scalar domain.
 *
 * Unless singleton, load unknown value into it and clamp it with conditional jumps.
 * (Jump offsets are not filled and should be patched in by the caller.)
 */
static void
prepare_scalar_domain(struct ebpf_insn **ins, uint8_t reg,
	const struct domain *domain, uint8_t base_reg, int *service_cell_count,
	uint8_t tmp_reg)
{
	if (domain_is_singleton(domain)) {
		/* Don't need any uncertainty for a singleton. */
		load_constant(ins, reg, domain->s.min);
		return;
	}

	/* Load value from memory area into the register. */
	*(*ins)++ = (struct ebpf_insn){
		.code = (BPF_LDX | EBPF_DW | BPF_MEM),
		.dst_reg = reg,
		.src_reg = base_reg,
		.off = sizeof(uint64_t) * (*service_cell_count)++,
	};

	/*
	 * Use both signed and unsigned conditions, even if redundant.
	 * It makes it more robust if conditional jump verification itself
	 * contains bugs like not updating the other type of interval.
	 * Jump instructions themselves can be tested separately to catch
	 * these bugs, this preparation phase is not a test for them.
	 */
	if (domain->u.min > unknown.u.min)
		compare_and_jump(ins, EBPF_JLT, reg, domain->u.min, tmp_reg);
	if (domain->u.max < unknown.u.max)
		compare_and_jump(ins, BPF_JGT, reg, domain->u.max, tmp_reg);
	if (domain->s.min > unknown.s.min)
		compare_and_jump(ins, EBPF_JSLT, reg, domain->s.min, tmp_reg);
	if (domain->s.max < unknown.s.max)
		compare_and_jump(ins, EBPF_JSGT, reg, domain->s.max, tmp_reg);
}

/*
 * Prepare register to be in the specified scalar or pointer domain, if any.
 *
 * If `domain` is NULL, do nothing. Otherwise prepare scalar domain,
 * and then add base register to it to convert it to a pointer, if needed.
 */
static void
prepare_domain(struct ebpf_insn **ins, uint8_t reg,
	const struct domain *domain, uint8_t base_reg, int *service_cell_count,
	uint8_t tmp_reg)
{
	prepare_scalar_domain(ins, reg, domain, base_reg, service_cell_count, tmp_reg);

	if (domain->is_pointer)
		/* Add base_reg to convert resulting scalar into a pointer. */
		*(*ins)++ = (struct ebpf_insn){
			.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
			.dst_reg = reg,
			.src_reg = base_reg,
		};
}

static void
fill_verify_instruction_defaults(struct verify_instruction_param *prm)
{

	if (BPF_CLASS(prm->tested_instruction.code) != BPF_JMP)
		prm->jump.is_unreachable = true;

	RTE_VERIFY(!prm->pre.is_unreachable);
	if (prm->post.is_unreachable) {
		RTE_VERIFY(!prm->post.dst.is_defined);
		RTE_VERIFY(!prm->post.src.is_defined);
	} else {
		if (!prm->post.dst.is_defined)
			prm->post.dst = prm->pre.dst;
		if (!prm->post.src.is_defined)
			prm->post.src = prm->pre.src;
	}

	if (prm->jump.is_unreachable) {
		RTE_VERIFY(!prm->jump.dst.is_defined);
		RTE_VERIFY(!prm->jump.src.is_defined);
	} else {
		if (!prm->jump.dst.is_defined)
			prm->jump.dst = prm->pre.dst;
		if (!prm->jump.src.is_defined)
			prm->jump.src = prm->pre.src;
	}
}

/* Generate program for the tested instruction and domains from the context.
 *
 * Return number of instructions.
 *
 * Destination and source registers in tested_instruction should not be specified,
 * they are filled in by the function as long as domains for them are specified.
 * Jump offset should not be specified, it is filled in by the function.
 *
 * If `pre.dst` or `pre.src` domain is not defined, corresponding register
 * is not prepared.
 *
 * For non-jump instructions `jump.is_unreachable` is always set automatically.
 *
 * If any of the post or jump domains are not defined, they are copied from src
 * unless corresponding branch is unreachable.
 *
 * Memory area size is automatically expanded to have enough space for loading
 * unknown dst and src register values, thus testing sizes less than 16 bytes is
 * not guaranteed.
 *
 * Limitations:
 * - Support for jump instructions is incomplete (e.g. exit, ja).
 * - Wide instructions are not supported yet.
 */
static uint32_t
generate_program(struct verify_instruction_context *ctx, struct ebpf_insn *ins)
{
	struct ebpf_insn *const ins_buf = ins;
	/* Number of double words used for service purposes. */
	int service_cell_count = 0;

	/* Make sure we actually support provided instruction. */
	switch (BPF_CLASS(ctx->prm.tested_instruction.code)) {
	case BPF_LD:
		/* Wide instructions are not supported yet. */
		RTE_VERIFY(!rte_bpf_insn_is_wide(&ctx->prm.tested_instruction));
		break;
	}

	fill_verify_instruction_defaults(&ctx->prm);

	/* Allocate registers, base_reg is received as program argument. */
	ctx->base_reg = EBPF_REG_1;
	ctx->dst_reg = (ctx->prm.pre.dst.is_defined || ctx->prm.post.dst.is_defined ||
		ctx->prm.jump.dst.is_defined) ? EBPF_REG_2 : NO_REGISTER;
	ctx->src_reg = (ctx->prm.pre.src.is_defined || ctx->prm.post.src.is_defined ||
		ctx->prm.jump.src.is_defined) ? EBPF_REG_3 : NO_REGISTER;
	ctx->tmp_reg = EBPF_REG_4;

	/* Clear r0 to make it eligible as a return value. */
	load_constant(&ins, EBPF_REG_0, 0);

	/* Fill dst register in the instruction if defined anywhere, prepare if needed. */
	if (ctx->dst_reg != NO_REGISTER) {
		RTE_VERIFY(ctx->prm.tested_instruction.dst_reg == 0);
		ctx->prm.tested_instruction.dst_reg = ctx->dst_reg;

		if (ctx->prm.pre.dst.is_defined)
			prepare_domain(&ins, ctx->dst_reg, &ctx->prm.pre.dst,
				ctx->base_reg, &service_cell_count, ctx->tmp_reg);
		else
			TEST_LOG_LINE(DEBUG, "Not preparing undefined r%hhu", ctx->dst_reg);
	}

	/* Fill src register in the instruction if defined anywhere, prepare if needed. */
	if (ctx->src_reg != NO_REGISTER) {
		RTE_VERIFY(ctx->prm.tested_instruction.src_reg == 0);
		ctx->prm.tested_instruction.src_reg = ctx->src_reg;

		if (ctx->prm.pre.src.is_defined)
			prepare_domain(&ins, ctx->src_reg, &ctx->prm.pre.src,
				ctx->base_reg, &service_cell_count, ctx->tmp_reg);
		else
			TEST_LOG_LINE(DEBUG, "Not preparing undefined r%hhu", ctx->src_reg);
	}

	/* Automatically increase area size if needed. */
	ctx->prm.area_size = RTE_MAX(ctx->prm.area_size, service_cell_count * sizeof(uint64_t));

	/* Issue tested instruction. */
	ctx->pre.program_counter = ins - ins_buf;
	*ins++ = ctx->prm.tested_instruction;

	/* Issue post instruction (for setting post breakpoint). */
	ctx->post.program_counter = ins - ins_buf;
	load_constant(&ins, EBPF_REG_0, 1);

	/* Issue jump branch for the jump instruction, even if dynamically unreachable. */
	if (BPF_CLASS(ctx->prm.tested_instruction.code) != BPF_JMP)
		ctx->jump.program_counter = NO_PROGRAM_COUNTER;
	else {
		/* Finish previous branch by issuing exit. */
		*ins++ = (struct ebpf_insn){ .code = (BPF_JMP | EBPF_EXIT) };

		/* Issue jump target instruction (for setting jump breakpoint). */
		ctx->jump.program_counter = ins - ins_buf;
		load_constant(&ins, EBPF_REG_0, 2);

		/* Patch jump in tested jump instruction. */
		RTE_VERIFY(ins_buf[ctx->pre.program_counter].off == 0);
		ins_buf[ctx->pre.program_counter].off =
			ctx->jump.program_counter - ctx->post.program_counter;
	}

	/* Issue exit instruction. */
	const uint32_t exit_pc = ins - ins_buf;
	*ins++ = (struct ebpf_insn){ .code = (BPF_JMP | EBPF_EXIT) };

	/* Patch all jumps to point to exit. */
	for (uint32_t pc = 0; pc != ctx->pre.program_counter; ++pc)
		if (BPF_CLASS(ins_buf[pc].code) == BPF_JMP) {
			RTE_ASSERT(ins_buf[pc].off == 0);
			ins_buf[pc].off = exit_pc - (pc + 1);
		}

	const uint32_t nb_ins = ins - ins_buf;
	return nb_ins;
}


/* VERIFICATION OF AN ARBITRARY INSTRUCTION */

/* Invoked when invalid state is detected. */
static int
invalid_state_cb(struct rte_bpf_validate_debug *debug, void *void_ctx)
{
	struct verify_instruction_context *const ctx = void_ctx;

	++ctx->invalid_state_count;

	TEST_LOG_LINE(WARNING,
		"Invalid state detected at pc %u",
		rte_bpf_validate_debug_get_pc(debug));

	RTE_SET_USED(debug);

	return TEST_SUCCESS;
}

static int
point_callback(struct rte_bpf_validate_debug *debug, const struct verify_instruction_context *ctx,
	struct point_context *point_ctx, const struct state *state)
{
	TEST_ASSERT_EQUAL(point_ctx->hit_count, 0, "not called before");

	const uint32_t pc = rte_bpf_validate_debug_get_pc(debug);
	TEST_ASSERT_EQUAL(pc, point_ctx->program_counter,
		"Expected program counter: %" PRIu32 ", found: %" PRIu32,
			point_ctx->program_counter, pc);

	if (ctx->dst_reg != NO_REGISTER) {
		format_register(debug, point_ctx->formatted_dst,
			sizeof(point_ctx->formatted_dst), ctx->dst_reg);

		if (state->dst.is_defined) {
			TEST_ASSERT_SUCCESS(
				check_domain(debug, ctx->dst_reg, &state->dst,
					ctx->base_reg, ctx->prm.area_size),
				"dst domain check");
			TEST_LOG_LINE(DEBUG, "Successfully checked r%hhu.", ctx->dst_reg);
		} else
			TEST_LOG_LINE(DEBUG, "Not checking undefined r%hhu.", ctx->dst_reg);
	}

	if (ctx->src_reg != NO_REGISTER) {
		format_register(debug, point_ctx->formatted_src,
			sizeof(point_ctx->formatted_src), ctx->src_reg);

		if (state->src.is_defined) {
			TEST_ASSERT_SUCCESS(
				check_domain(debug, ctx->src_reg, &state->src,
					ctx->base_reg, ctx->prm.area_size),
				"src domain check");
			TEST_LOG_LINE(DEBUG, "Successfully checked r%hhu.", ctx->src_reg);
		} else
			TEST_LOG_LINE(DEBUG, "Not checking undefined r%hhu.", ctx->src_reg);
	}

	++point_ctx->hit_count;

	return TEST_SUCCESS;
}

/*
 * Invoked before the tested instruction and checks pre-conditions.
 *
 * Also formats registers in the pre state for postmortem, if needed.
 */
static int
pre_callback(struct rte_bpf_validate_debug *debug, void *void_ctx)
{
	struct verify_instruction_context *const ctx = void_ctx;

	TEST_LOG_LINE(DEBUG, "Pre callback invoked.");

	TEST_ASSERT_SUCCESS(
		point_callback(debug, ctx, &ctx->pre, &ctx->prm.pre),
		"pre-state check");

	return TEST_SUCCESS;
}

/* Invoked after the tested instruction and checks post-conditions. */
static int
post_callback(struct rte_bpf_validate_debug *debug, void *void_ctx)
{
	struct verify_instruction_context *const ctx = void_ctx;

	TEST_LOG_LINE(DEBUG, "Post callback invoked.");

	TEST_ASSERT_SUCCESS(
		point_callback(debug, ctx, &ctx->post, &ctx->prm.post),
		"post-state check");

	return TEST_SUCCESS;
}

/* Invoked after the tested instruction jumped and checks jump post-conditions. */
static int
jump_callback(struct rte_bpf_validate_debug *debug, void *void_ctx)
{
	struct verify_instruction_context *const ctx = void_ctx;

	TEST_LOG_LINE(DEBUG, "Jump callback invoked.");

	TEST_ASSERT_SUCCESS(
		point_callback(debug, ctx, &ctx->jump, &ctx->prm.jump),
		"jump-state check");

	return TEST_SUCCESS;
}

static int
debug_validation(struct verify_instruction_context *ctx, const struct ebpf_insn *ins,
	uint32_t nb_ins)
{
	struct rte_bpf_validate_debug *const debug = rte_bpf_validate_debug_create();
	TEST_ASSERT_NOT_NULL(debug, "validate debug create error %d", rte_errno);

	const struct rte_bpf_prm_ex prm = {
		.sz = sizeof(struct rte_bpf_prm_ex),
		.origin = RTE_BPF_ORIGIN_RAW,
		.raw.ins = ins,
		.raw.nb_ins = nb_ins,
		.prog_arg[0] = {
			.type = RTE_BPF_ARG_PTR,
			.size = ctx->prm.area_size,
		},
		.nb_prog_arg = 1,
		.debug = debug,
	};

	/* Catch invalid states. */
	TEST_ASSERT_NOT_NULL(rte_bpf_validate_debug_catch(debug,
		RTE_BPF_VALIDATE_DEBUG_EVENT_INVALID_STATE,
		&(struct rte_bpf_validate_debug_callback){
			.fn = invalid_state_cb,
			.ctx = ctx,
		}), "add catchpoint error %d", rte_errno);

	/* Break on pre test instruction. */
	TEST_ASSERT_NOT_NULL(rte_bpf_validate_debug_break(debug, ctx->pre.program_counter,
		&(struct rte_bpf_validate_debug_callback){
			.fn = pre_callback,
			.ctx = ctx,
		}), "add pre breakpoint error %d", rte_errno);

	/* Break on post test instruction. */
	TEST_ASSERT_NOT_NULL(rte_bpf_validate_debug_break(debug, ctx->post.program_counter,
		&(struct rte_bpf_validate_debug_callback){
			.fn = post_callback,
			.ctx = ctx,
		}), "add post breakpoint error %d", rte_errno);

	if (ctx->jump.program_counter != NO_PROGRAM_COUNTER)
		/* Break on jump test instruction. */
		TEST_ASSERT_NOT_NULL(rte_bpf_validate_debug_break(debug, ctx->jump.program_counter,
			&(struct rte_bpf_validate_debug_callback){
				.fn = jump_callback,
				.ctx = ctx,
			}), "add jump breakpoint error %d", rte_errno);

	struct rte_bpf *const bpf = rte_bpf_load_ex(&prm);
	const int validation_errno = rte_errno;

	rte_bpf_destroy(bpf);
	rte_bpf_validate_debug_destroy(debug);

	TEST_ASSERT_NOT_NULL(bpf, "validation error %d", validation_errno);

	TEST_ASSERT_EQUAL(ctx->pre.hit_count, !ctx->prm.pre.is_unreachable,
		"pre hit_count = %d", ctx->pre.hit_count);
	TEST_ASSERT_EQUAL(ctx->post.hit_count, !ctx->prm.post.is_unreachable,
		"post hit_count = %d", ctx->post.hit_count);
	TEST_ASSERT_EQUAL(ctx->jump.hit_count, !ctx->prm.jump.is_unreachable,
		"jump hit_count = %d", ctx->jump.hit_count);

	return TEST_SUCCESS;
}

/* Dump whole program to log. */
static void
log_program_dump(const struct ebpf_insn *ins, uint32_t nb_ins, uint32_t pre_pc)
{
	char hexadecimal[DISASSEMBLY_FORMAT_BUFFER_SIZE];
	char disassembly[DISASSEMBLY_FORMAT_BUFFER_SIZE];

	TEST_LOG_LINE(NOTICE, "\tTested program:");
	for (uint32_t pc = 0; pc != nb_ins; ++pc) {
		rte_bpf_format(hexadecimal, sizeof(hexadecimal), &ins[pc], pc,
			RTE_BPF_FORMAT_FLAG_HEXADECIMAL |
			RTE_BPF_FORMAT_FLAG_NEVER_WIDE);
		rte_bpf_format(disassembly, sizeof(disassembly), &ins[pc], pc,
			RTE_BPF_FORMAT_FLAG_DISASSEMBLY |
			RTE_BPF_FORMAT_FLAG_ABSOLUTE_JUMPS);
		TEST_LOG_LINE(NOTICE, "\t%5u: \t%s \t%s%s",
			pc, hexadecimal, disassembly,
			pc != pre_pc ? "" : "  ; tested instruction");

		if (!rte_bpf_insn_is_wide(&ins[pc]))
			continue;

		++pc;

		rte_bpf_format(hexadecimal, sizeof(hexadecimal), &ins[pc], pc,
			RTE_BPF_FORMAT_FLAG_HEXADECIMAL |
			RTE_BPF_FORMAT_FLAG_NEVER_WIDE);
		TEST_LOG_LINE(NOTICE, "\t%6s \t%s", "", hexadecimal);
	}
}

static void
log_formatted_registers(const char *heading, const struct verify_instruction_context *ctx,
	const struct point_context *point_ctx)
{
	char register_name[8];

	TEST_LOG_LINE(NOTICE, "\t%s", heading);
	if (ctx->dst_reg != NO_REGISTER) {
		snprintf(register_name, sizeof(register_name), "r%hhu", ctx->dst_reg);
		TEST_LOG_LINE(NOTICE, "\t%5s: \t%s", register_name, point_ctx->formatted_dst);
	}
	if (ctx->src_reg != NO_REGISTER) {
		snprintf(register_name, sizeof(register_name), "r%hhu", ctx->src_reg);
		TEST_LOG_LINE(NOTICE, "\t%5s: \t%s", register_name, point_ctx->formatted_src);
	}
}

/*
 * Verify instruction validation behaviour described by prm.
 *
 * Generate the program containing specified instruction on the code path with
 * specified register pre-domains and verify specified register post-domains.
 *
 * See comment to `generate_program` for more requirements and limitations.
 */
static int
verify_instruction(struct verify_instruction_param prm)
{
	struct verify_instruction_context ctx = {
		.prm = prm,
	};
	struct ebpf_insn ins_buf[64];

	const uint32_t nb_ins = generate_program(&ctx, ins_buf);
	RTE_ASSERT(nb_ins <= RTE_DIM(ins_buf));

	const int rc = debug_validation(&ctx, ins_buf, nb_ins);

	/* Log more data at DEBUG level on success, NOTICE on failure. */
	if (rte_log_can_log(RTE_LOGTYPE_TEST_BPF_VALIDATE, RTE_LOG_DEBUG) ||
			rc != TEST_SUCCESS) {
		log_program_dump(ins_buf, nb_ins, ctx.pre.program_counter);
		log_formatted_registers("Pre-state:", &ctx, &ctx.pre);
		log_formatted_registers("Post-state:", &ctx, &ctx.post);
		if (ctx.jump.program_counter != NO_PROGRAM_COUNTER)
			log_formatted_registers("Jump-state:", &ctx, &ctx.jump);
	}

	return rc;
}

static int
opcode_comparison_index(uint8_t opcode)
{
	for (int index = 0; index != RTE_DIM(comparisons_opcode); ++index)
		if (comparisons_opcode[index] == opcode)
			return index;
	TEST_LOG_LINE(ERR, "Unsupported or not a comparison opcode: %hhx", opcode);
	RTE_VERIFY(false);
}

/* Change two-register comparison verification to immediate one. */
static bool
make_comparison_immediate(struct verify_instruction_param *prm)
{
	int comparison_index = opcode_comparison_index(prm->tested_instruction.code);
	const int64_t value = prm->pre.src.s.min;

	if ((comparison_index & COMPARISON_INDEX_IMMEDIATE) != 0) {
		TEST_LOG_LINE(ERR, "Comparison %hhx is already immediate.",
			prm->tested_instruction.code);
		RTE_VERIFY(false);
	}

	if (!domain_is_singleton(&prm->pre.src) || !domain_is_singleton(&prm->post.src) ||
			!domain_is_singleton(&prm->jump.src)) {
		TEST_LOG_LINE(DEBUG, "Cannot make immediate out of a non-singleton domain.");
		return false;
	}
	if (prm->pre.src.is_pointer || prm->post.src.is_pointer || prm->jump.src.is_pointer) {
		TEST_LOG_LINE(DEBUG, "Cannot make immediate out of a pointer.");
		return false;
	}
	if (prm->post.src.s.min != value || prm->jump.src.s.min != value) {
		TEST_LOG_LINE(DEBUG, "Cannot make immediate if the value changes.");
		return false;
	}
	if (!fits_in_imm32(value)) {
		TEST_LOG_LINE(ERR, "Cannot make immediate unless value fits in int32.");
		return false;
	}

	comparison_index |= COMPARISON_INDEX_IMMEDIATE;
	prm->tested_instruction.code = comparisons_opcode[comparison_index];
	prm->tested_instruction.imm = value;

	RTE_VERIFY(prm->pre.src.is_defined);
	prm->pre.src.is_defined = false;

	if (!prm->post.is_unreachable) {
		RTE_VERIFY(prm->post.src.is_defined);
		prm->post.src.is_defined = false;
	}

	if (!prm->jump.is_unreachable) {
		RTE_VERIFY(prm->jump.src.is_defined);
		prm->jump.src.is_defined = false;
	}

	return true;
}

/* Change immediate comparison verification to two-register one. */
static void
make_comparison_two_register(struct verify_instruction_param *prm)
{
	int comparison_index = opcode_comparison_index(prm->tested_instruction.code);
	const int64_t value = prm->tested_instruction.imm;

	if ((comparison_index & COMPARISON_INDEX_IMMEDIATE) == 0) {
		TEST_LOG_LINE(ERR, "Comparison %hhx is already two-register.",
			prm->tested_instruction.code);
		RTE_VERIFY(false);
	}

	comparison_index &= ~COMPARISON_INDEX_IMMEDIATE;
	prm->tested_instruction.code = comparisons_opcode[comparison_index];
	prm->tested_instruction.imm = 0;

	RTE_VERIFY(!prm->pre.src.is_defined);
	prm->pre.src = make_singleton_domain(value);

	if (!prm->post.is_unreachable) {
		RTE_VERIFY(!prm->post.src.is_defined);
		prm->post.src = prm->pre.src;
	}

	if (!prm->jump.is_unreachable) {
		RTE_VERIFY(!prm->jump.src.is_defined);
		prm->jump.src = prm->pre.src;
	}
}

/* Change comparison verification to complement (negated result) one. */
static void
make_comparison_complement(struct verify_instruction_param *prm)
{
	int comparison_index = opcode_comparison_index(prm->tested_instruction.code);
	comparison_index ^= COMPARISON_INDEX_GREATER | COMPARISON_INDEX_INCLUSIVE;
	prm->tested_instruction.code = comparisons_opcode[comparison_index];
	RTE_SWAP(prm->post, prm->jump);
}

/* Change comparison verification to converse (swapped operands) one. */
static void
make_comparison_converse(struct verify_instruction_param *prm)
{
	int comparison_index = opcode_comparison_index(prm->tested_instruction.code);
	comparison_index ^= COMPARISON_INDEX_GREATER;
	prm->tested_instruction.code = comparisons_opcode[comparison_index];
	RTE_SWAP(prm->pre.dst, prm->pre.src);
	RTE_SWAP(prm->post.dst, prm->post.src);
	RTE_SWAP(prm->jump.dst, prm->jump.src);
}

/* Change signed comparison verification to unsigned one. */
static void
make_comparison_signed(struct verify_instruction_param *prm)
{
	int comparison_index = opcode_comparison_index(prm->tested_instruction.code);
	if ((comparison_index & COMPARISON_INDEX_SIGNED) != 0) {
		TEST_LOG_LINE(ERR, "Comparison %hhx is already signed.",
			prm->tested_instruction.code);
		RTE_VERIFY(false);
	}
	comparison_index |= COMPARISON_INDEX_SIGNED;
	prm->tested_instruction.code = comparisons_opcode[comparison_index];
}

/* Verify specified two-register comparison and, if possible, immediate one. */
static int
verify_comparison_subcase(struct verify_instruction_param prm)
{
	TEST_ASSERT_SUCCESS(verify_instruction(prm), "two-register version check");

	if (make_comparison_immediate(&prm))
		TEST_ASSERT_SUCCESS(verify_instruction(prm), "immediate version check");

	return TEST_SUCCESS;
}

/*
 * Verify comparison instruction validation behaviour.
 *
 * Call `verify_instruction` for all valid variations of the instruction.
 *
 * For instance, `jgt r2, r3` verifies:
 * * `jgt r2, r3`;
 * * `jlt r3, r2` src and dst swapped with each other;
 * * `jle r2, r3` with post and jump domains swapped with each other;
 * * `jge r3, r2` with all corresponding swaps;
 * * immediate versions of everything above where possible,
 *   that is, register on the right is an int32 scalar singleton;
 * * signed versions of everything above if `also_signed` is true;
 *
 * Regardless if passed instruction compares with immediate or singleton src
 * both cases are generated and tested.
 */
static int
verify_comparison(struct verify_instruction_param prm, bool also_signed)
{
	fill_verify_instruction_defaults(&prm);

	if (!prm.pre.src.is_defined)
		/* Convert from immediate form to simplify further logic. */
		make_comparison_two_register(&prm);

	/* All reachable domains must be defined by this point. */
	RTE_VERIFY(prm.pre.dst.is_defined);
	RTE_VERIFY(prm.pre.src.is_defined);
	if (!prm.post.is_unreachable) {
		RTE_VERIFY(prm.post.dst.is_defined);
		RTE_VERIFY(prm.post.src.is_defined);
	}
	if (!prm.jump.is_unreachable) {
		RTE_VERIFY(prm.jump.dst.is_defined);
		RTE_VERIFY(prm.jump.src.is_defined);
	}

	for (int make_signed = 0; make_signed <= also_signed; ++make_signed) {
		if (make_signed)
			make_comparison_signed(&prm);

		for (int complement = false; complement <= true; ++complement) {

			for (int converse = false; converse <= true; ++converse) {

				TEST_ASSERT_SUCCESS(verify_comparison_subcase(prm),
					"make_signed=%d, complement=%d, converse=%d",
					make_signed, complement, converse);

				make_comparison_converse(&prm);
			}

			make_comparison_complement(&prm);
		}
	}

	return TEST_SUCCESS;
}


/* TESTS FOR SPECIFIC INSTRUCTIONS */

/* 64-bit addition of immediate to a range. */
static int
test_alu64_add_k(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
			.imm = 17,
		},
		.pre.dst = make_signed_domain(11, 29),
		.post.dst = make_signed_domain(11 + 17, 29 + 17),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_add_k_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_add_k);

/* 64-bit addition of immediate to a pointer range. */
static int
test_alu64_add_k_pointer(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_ADD | BPF_K),
			.imm = 17,
		},
		.area_size = 256,
		.pre.dst = make_pointer_domain(11, 29),
		.post.dst = make_pointer_domain(11 + 17, 29 + 17),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_add_k_pointer_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_add_k_pointer);

/* 64-bit addition of pointer to a pointer. */
static int
test_alu64_add_x_pointer_pointer(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		},
		.area_size = 256,
		.pre.dst = make_pointer_domain(11, 29),
		.pre.src = make_pointer_domain(17, 23),
		.post.dst = unknown,
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_add_x_pointer_pointer_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_add_x_pointer_pointer);

/* 64-bit addition of scalar to a pointer. */
static int
test_alu64_add_x_pointer_scalar(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		},
		.area_size = 256,
		.pre.dst = make_pointer_domain(11, 29),
		.pre.src = make_signed_domain(17, 23),
		.post.dst = make_pointer_domain(11 + 17, 29 + 23),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_add_x_pointer_scalar_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_add_x_pointer_scalar);

/* 64-bit addition of pointer to a scalar. */
static int
test_alu64_add_x_scalar_pointer(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		},
		.area_size = 256,
		.pre.dst = make_signed_domain(11, 29),
		.pre.src = make_pointer_domain(17, 23),
		.post.dst = make_pointer_domain(11 + 17, 29 + 23),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_add_x_scalar_pointer_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_add_x_scalar_pointer);

/* 64-bit addition of scalar to a scalar. */
static int
test_alu64_add_x_scalar_scalar(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_ADD | BPF_X),
		},
		.area_size = 256,
		.pre.dst = make_signed_domain(11, 29),
		.pre.src = make_signed_domain(17, 23),
		.post.dst = make_signed_domain(11 + 17, 29 + 23),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_add_x_scalar_scalar_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_add_x_scalar_scalar);

/* 64-bit bitwise AND between a scalar range and immediate. */
static int
test_alu64_and_k(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_AND | BPF_K),
			.imm = 5,
		},
		.pre.dst = make_signed_domain(6, 8),
		.post.dst = make_signed_domain(0, 7),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_and_k_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_and_k);

/* 64-bit division and modulo of UINT64_MAX*2/3. */
static int
test_alu64_div_mod_big_constant(void)
{
	const uint64_t dividend = UINT64_MAX / 3 * 2;
	static const uint64_t divisors[] = {
		1,
		2,
		3,
		UINT64_MAX / 3,
		INT64_MAX,
		INT64_MIN,
		UINT64_MAX / 3 * 2,
		UINT64_MAX / 4 * 3,
		UINT64_MAX,
	};
	for (int index = 0; index != RTE_DIM(divisors); ++index) {
		const uint64_t divisor = divisors[index];

		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_DIV | BPF_X),
			},
			.pre.dst = make_singleton_domain(dividend),
			.pre.src = make_singleton_domain(divisor),
			.post.dst = make_singleton_domain(dividend / divisor),
		}), "(EBPF_ALU64 | BPF_DIV | BPF_X) check, index=%d", index);

		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_MOD | BPF_X),
			},
			.pre.dst = make_singleton_domain(dividend),
			.pre.src = make_singleton_domain(divisor),
			.post.dst = make_singleton_domain(dividend % divisor),
		}), "(EBPF_ALU64 | BPF_MOD | BPF_X) check, index=%d", index);
	}

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_div_mod_big_constant_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_div_mod_big_constant);

/* 64-bit division and modulo of UINT64_MAX/3..UINT64_MAX*2/3 by a constant. */
static int
test_alu64_div_mod_big_range(void)
{
	const uint64_t dividend_first = UINT64_MAX / 3;
	const uint64_t dividend_last = UINT64_MAX / 3 * 2;
	static const uint64_t divisors[] = {
		1,
		2,
		3,
		UINT64_MAX / 3,
		INT64_MAX,
		INT64_MIN,
		UINT64_MAX / 3 * 2,
		UINT64_MAX / 4 * 3,
		UINT64_MAX,
	};
	for (int index = 0; index != RTE_DIM(divisors); ++index) {
		const uint64_t divisor = divisors[index];

		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_DIV | BPF_X),
			},
			.pre.dst = make_unsigned_domain(dividend_first, dividend_last),
			.pre.src = make_singleton_domain(divisor),
			.post.dst = make_unsigned_domain(0, dividend_last),
		}), "(EBPF_ALU64 | BPF_DIV | BPF_X) check, index=%d", index);

		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_MOD | BPF_X),
			},
			.pre.dst = make_unsigned_domain(dividend_first, dividend_last),
			.pre.src = make_singleton_domain(divisor),
			.post.dst = make_unsigned_domain(0, RTE_MIN(dividend_last, divisor - 1)),
		}), "(EBPF_ALU64 | BPF_MOD | BPF_X) check, index=%d", index);
	}

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_div_mod_big_range_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_div_mod_big_range);

/* 64-bit division and modulo of INT64_MIN by -1. */
static int
test_alu64_div_mod_overflow(void)
{
	TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_DIV | BPF_K),
			.imm = -1,
		},
		.pre.dst = make_singleton_domain(INT64_MIN),
		.post.dst = make_singleton_domain(0),
	}), "(EBPF_ALU64 | BPF_DIV | BPF_K) check");

	TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_DIV | BPF_X),
		},
		.pre.dst = make_singleton_domain(INT64_MIN),
		.pre.src = make_singleton_domain(-1),
		.post.dst = make_singleton_domain(0),
	}), "(EBPF_ALU64 | BPF_DIV | BPF_X) check");

	TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_MOD | BPF_K),
			.imm = -1,
		},
		.pre.dst = make_singleton_domain(INT64_MIN),
		.post.dst = make_singleton_domain(INT64_MIN),
	}), "(EBPF_ALU64 | BPF_MOD | BPF_K) check");

	TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_MOD | BPF_X),
		},
		.pre.dst = make_singleton_domain(INT64_MIN),
		.pre.src = make_singleton_domain(-1),
		.post.dst = make_singleton_domain(INT64_MIN),
	}), "(EBPF_ALU64 | BPF_MOD | BPF_X) check");

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_div_mod_overflow_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_div_mod_overflow);

/* 64-bit left shift by 63. */
static int
test_alu64_lsh_63(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_LSH | BPF_K),
			.imm = 63,
		},
		.pre.dst = make_signed_domain(3, 5),
		.post.dst = unknown,
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_lsh_63_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_lsh_63);

/* 64-bit multiplication of constant and immediate with overflow. */
static int
test_alu64_mul_k_overflow(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_MUL | BPF_K),
			.imm = 0x12345678,
		},
		.pre.dst = make_singleton_domain(0x9876543210),
		.post.dst = make_singleton_domain(0x9876543210u * 0x12345678),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_mul_k_overflow_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_mul_k_overflow);

/* 64-bit mul of small scalar range and immediate. */
static int
test_alu64_mul_k_range_small(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (EBPF_ALU64 | BPF_MUL | BPF_K),
			.imm = 11,
		},
		.pre.dst = make_unsigned_domain(17, 29),
		.post.dst = make_unsigned_domain(17 * 11, 29 * 11),
	});
}

REGISTER_FAST_TEST(bpf_validate_alu64_mul_k_range_small_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_mul_k_range_small);

/* 64-bit negation when interval first element is INT64_MIN. */
static int
test_alu64_neg_int64min_first(void)
{
	static const int64_t other_values[] = {
		INT64_MIN,
		INT64_MIN + 1,
		INT64_MIN + 13,
		-17,
		-1,
		0,
		1,
		19,
		INT64_MAX - 23,
		INT64_MAX - 1,
		INT64_MAX,
	};
	for (int other_index = 0; other_index != RTE_DIM(other_values); ++other_index) {
		const int64_t other_value = other_values[other_index];
		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_NEG),
			},
			.pre.dst = make_signed_domain(INT64_MIN, other_value),
			.post.dst = other_value > 0 ? unknown :
				make_unsigned_domain(-(uint64_t)other_value, INT64_MIN),
		}), "other_index=%d", other_index);
	}
	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_neg_int64min_first_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_neg_int64min_first);

/* 64-bit negation when interval last element is INT64_MIN. */
static int
test_alu64_neg_int64min_last(void)
{
	static const uint64_t other_values[] = {
		0,
		1,
		19,
		INT64_MAX - 23,
		INT64_MAX - 1,
		INT64_MAX,
		INT64_MIN,
	};
	for (int other_index = 0; other_index != RTE_DIM(other_values); ++other_index) {
		const int64_t other_value = other_values[other_index];
		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_NEG),
			},
			.pre.dst = make_unsigned_domain(other_value, INT64_MIN),
			.post.dst = make_signed_domain(INT64_MIN, -(uint64_t)other_value),
		}), "other_index=%d", other_index);
	}
	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_neg_int64min_last_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_neg_int64min_last);

/* 64-bit negation when interval first element is zero. */
static int
test_alu64_neg_zero_first(void)
{
	static const uint64_t other_values[] = {
		0,
		1,
		19,
		INT64_MAX - 23,
		INT64_MAX - 1,
		INT64_MAX,
		INT64_MIN,
		INT64_MIN + 1,
		INT64_MIN + 13,
		-17,
		-1,
	};
	for (int other_index = 0; other_index != RTE_DIM(other_values); ++other_index) {
		const uint64_t other_value = other_values[other_index];
		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_NEG),
			},
			.pre.dst = make_unsigned_domain(0, other_value),
			.post.dst = other_value > (uint64_t)INT64_MIN ? unknown :
				make_signed_domain(-(uint64_t)other_value, 0),
		}), "other_index=%d", other_index);
	}
	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_neg_zero_first_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_neg_zero_first);

/* 64-bit negation when interval last element is zero. */
static int
test_alu64_neg_zero_last(void)
{
	static const int64_t other_values[] = {
		INT64_MIN,
		INT64_MIN + 1,
		INT64_MIN + 13,
		-17,
		-1,
		0,
	};
	for (int other_index = 0; other_index != RTE_DIM(other_values); ++other_index) {
		const int64_t other_value = other_values[other_index];
		TEST_ASSERT_SUCCESS(verify_instruction((struct verify_instruction_param){
			.tested_instruction = {
				.code = (EBPF_ALU64 | BPF_NEG),
			},
			.pre.dst = make_signed_domain(other_value, 0),
			.post.dst = make_unsigned_domain(0, -(uint64_t)other_value),
		}), "other_index=%d", other_index);
	}

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_alu64_neg_zero_last_autotest, NOHUGE_OK, ASAN_OK,
	test_alu64_neg_zero_last);

/* Jump if greater than immediate. */
static int
test_jmp64_jeq_k(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGT | BPF_K),
			.imm = 0,
		},
		.pre.dst = make_unsigned_domain(0, 1),
		.post.dst = make_singleton_domain(0),
		.jump.dst = make_singleton_domain(1),
	});
}

REGISTER_FAST_TEST(bpf_validate_jmp64_jeq_k_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_jeq_k);

/* Jump if signed less than another register. */
static int
test_jmp64_jslt_x(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JSLT | BPF_X),
		},
		.pre.dst = make_signed_domain(-3, 3),
		.pre.src = make_signed_domain(0, 0),
		.post.dst = make_signed_domain(0, 3),
		.jump.dst = make_signed_domain(-3, -1),
	});
}

REGISTER_FAST_TEST(bpf_validate_jmp64_jslt_x_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_jslt_x);

/* Jump on ordering comparisons with potential bound overflow. */
static int
test_jmp64_ordering_overflow(void)
{
	/* In this test signed and unsigned cases are spelled out explicitly. */
	const bool also_signed = false;

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JSLT | BPF_X),
		},
		.pre.dst = make_singleton_domain(42),
		.pre.src = make_singleton_domain(INT64_MIN),
		.jump = unreachable,
	}, also_signed), "signed less than INT64_MIN");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JSGT | BPF_X),
		},
		.pre.dst = make_singleton_domain(42),
		.pre.src = make_singleton_domain(INT64_MAX),
		.jump = unreachable,
	}, also_signed), "signed greater than INT64_MAX");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_singleton_domain(42),
		.pre.src = make_singleton_domain(0),
		.jump = unreachable,
	}, also_signed), "unsigned less than zero");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGT | BPF_X),
		},
		.pre.dst = make_singleton_domain(42),
		.pre.src = make_singleton_domain(UINT64_MAX),
		.jump = unreachable,
	}, also_signed), "unsigned greater than UINT64_MAX");

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_jmp64_ordering_overflow_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_ordering_overflow);

/* Jump on ordering comparisons between two ranges. */
static int
test_jmp64_ordering_ranges(void)
{
	/* All ranges used are valid for both signed and unsigned comparisons. */
	const bool also_signed = true;

	/*
	 *               20 ---- dst ---- 60
	 * 0 - src - 10
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(0, 10),
		.jump = unreachable,
	}, also_signed), "strict, dst range strongly greater than src range");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(0, 10),
		.jump = unreachable,
	}, also_signed), "non-strict, dst range strongly greater than src range");

	/*
	 *     20 ---- dst ---- 60
	 * 10 -- src -- 40
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(10, 40),
		.jump.dst = make_signed_domain(20, 39),
		.jump.src = make_signed_domain(21, 40),
	}, also_signed), "strict, dst range weakly greater than src range");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(10, 40),
		.jump.dst = make_signed_domain(20, 40),
		.jump.src = make_signed_domain(20, 40),
	}, also_signed), "non-strict, dst range weakly greater than src range");

	/*
	 *     20 ---- dst ---- 60
	 * 10 -------- src -------- 70
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(10, 70),
		.post.src = make_signed_domain(10, 60),
		.jump.src = make_signed_domain(21, 70),
	}, also_signed), "strict, dst range included in src range");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(10, 70),
		.post.src = make_signed_domain(10, 59),
		.jump.src = make_signed_domain(20, 70),
	}, also_signed), "non-strict, dst range included in src range");

	/*
	 *     20 ---- dst ---- 60
	 *        30 - src - 50
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(30, 50),
		.post.dst = make_signed_domain(30, 60),
		.jump.dst = make_signed_domain(20, 49),
	}, also_signed), "strict, dst range includes src range");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(30, 50),
		.post.dst = make_signed_domain(31, 60),
		.jump.dst = make_signed_domain(20, 50),
	}, also_signed), "non-strict, dst range includes src range");

	/*
	 *     20 ---- dst ---- 60
	 *             40 -- src -- 70
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(40, 70),
		.post.dst = make_signed_domain(40, 60),
		.post.src = make_signed_domain(40, 60),
	}, also_signed), "strict, dst range weakly less than src range");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(40, 70),
		.post.dst = make_signed_domain(41, 60),
		.post.src = make_signed_domain(40, 59),
	}, also_signed), "non-strict, dst range weakly less than src range");

	/*
	 *     20 ---- dst ---- 60
	 *                          70 - src - 80
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(70, 80),
		.post = unreachable,
	}, also_signed), "strict, dst range strongly less than src range");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_X),
		},
		.pre.dst = make_signed_domain(20, 60),
		.pre.src = make_signed_domain(70, 80),
		.post = unreachable,
	}, also_signed), "non-strict, dst range strongly less than src range");

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_jmp64_ordering_ranges_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_ordering_ranges);

/* Jump on ordering comparisons with singleton inside the range. */
static int
test_jmp64_ordering_singleton_inside(void)
{
	/* All ranges used are valid for both signed and unsigned comparisons. */
	const bool also_signed = true;

	/*
	 *     20 ---- dst ---- 60
	 *             imm
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_K),
			.imm = 40,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post.dst = make_signed_domain(40, 60),
		.jump.dst = make_signed_domain(20, 39),
	}, also_signed), "(BPF_JMP | EBPF_JLT | BPF_K) check");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGT | BPF_K),
			.imm = 40,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post.dst = make_signed_domain(20, 40),
		.jump.dst = make_signed_domain(41, 60),
	}, also_signed), "(BPF_JMP | EBPF_JGT | BPF_K) check");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_K),
			.imm = 40,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post.dst = make_signed_domain(41, 60),
		.jump.dst = make_signed_domain(20, 40),
	}, also_signed), "(BPF_JMP | EBPF_JLE | BPF_K) check");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGE | BPF_K),
			.imm = 40,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post.dst = make_signed_domain(20, 39),
		.jump.dst = make_signed_domain(40, 60),
	}, also_signed), "(BPF_JMP | EBPF_JGE | BPF_K) check");

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_jmp64_ordering_singleton_inside_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_ordering_singleton_inside);

/* Jump on ordering comparisons with singleton outside the range. */
static int
test_jmp64_ordering_singleton_outside(void)
{
	/* All ranges used are valid for both signed and unsigned comparisons. */
	const bool also_signed = true;

	/*
	 *       20 ---- dst ---- 60
	 *  imm
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_K),
			.imm = 10,
		},
		.pre.dst = make_signed_domain(20, 60),
		.jump = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JLT | BPF_K) check, range greater than imm");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_K),
			.imm = 10,
		},
		.pre.dst = make_signed_domain(20, 60),
		.jump = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JLE | BPF_K) check, range greater than imm");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGT | BPF_K),
			.imm = 10,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JGT | BPF_K) check, range greater than imm");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGE | BPF_K),
			.imm = 10,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JGE | BPF_K) check, range greater than imm");

	/*
	 *       20 ---- dst ---- 60
	 *                            imm
	 */

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLT | BPF_K),
			.imm = 70,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JLT | BPF_K) check, range less than imm");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | EBPF_JLE | BPF_K),
			.imm = 70,
		},
		.pre.dst = make_signed_domain(20, 60),
		.post = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JLE | BPF_K) check, range less than imm");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGT | BPF_K),
			.imm = 70,
		},
		.pre.dst = make_signed_domain(20, 60),
		.jump = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JGT | BPF_K) check, range less than imm");

	TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_JMP | BPF_JGE | BPF_K),
			.imm = 70,
		},
		.pre.dst = make_signed_domain(20, 60),
		.jump = unreachable,
	}, also_signed), "(BPF_JMP | EBPF_JGE | BPF_K) check, range less than imm");

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_jmp64_ordering_singleton_outside_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_ordering_singleton_outside);

/* Jump on ordering comparisons with ranges "touching" each other. */
static int
test_jmp64_ordering_touching(void)
{
	/* All ranges used are valid for both signed and unsigned comparisons. */
	const bool also_signed = true;

	for (int overlap = 0; overlap != 3; ++overlap) {

		/*
		 *                  20 - dst - 30
		 * 10 - src - (19 + overlap)
		 */

		TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
			.tested_instruction = {
				.code = (BPF_JMP | EBPF_JLT | BPF_X),
			},
			.pre.dst = make_signed_domain(20, 30),
			.pre.src = make_signed_domain(10, 19 + overlap),
			.jump = overlap <= 1 ? unreachable : (struct state){
				.dst = make_singleton_domain(20),
				.src = make_singleton_domain(21),
			},
		}, also_signed), "strict, dst left touching src right, overlap=%d", overlap);

		TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
			.tested_instruction = {
				.code = (BPF_JMP | EBPF_JLE | BPF_X),
			},
			.pre.dst = make_signed_domain(20, 30),
			.pre.src = make_signed_domain(10, 19 + overlap),
			.jump = overlap < 1 ? unreachable : (struct state){
				.dst = make_signed_domain(20, 19 + overlap),
				.src = make_signed_domain(20, 19 + overlap),
			},
		}, also_signed), "non-strict, dst left touching src right, overlap=%d", overlap);

		/*
		 * 10 - dst - (19 + overlap)
		 *                  20 - src - 30
		 */

		TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
			.tested_instruction = {
				.code = (BPF_JMP | EBPF_JLT | BPF_X),
			},
			.pre.dst = make_signed_domain(10, 19 + overlap),
			.pre.src = make_signed_domain(20, 30),
			.post = overlap < 1 ? unreachable : (struct state){
				.dst = make_signed_domain(20, 19 + overlap),
				.src = make_signed_domain(20, 19 + overlap),
			},
		}, also_signed), "strict, dst right touching src left, overlap=%d", overlap);

		TEST_ASSERT_SUCCESS(verify_comparison((struct verify_instruction_param){
			.tested_instruction = {
				.code = (BPF_JMP | EBPF_JLE | BPF_X),
			},
			.pre.dst = make_signed_domain(10, 19 + overlap),
			.pre.src = make_signed_domain(20, 30),
			.post = overlap <= 1 ? unreachable : (struct state){
				.dst = make_singleton_domain(21),
				.src = make_singleton_domain(20),
			},
		}, also_signed), "non-strict, dst right touching src left, overlap=%d", overlap);
	}

	return TEST_SUCCESS;
}

REGISTER_FAST_TEST(bpf_validate_jmp64_ordering_touching_autotest, NOHUGE_OK, ASAN_OK,
	test_jmp64_ordering_touching);

/* 64-bit load from heap (should be set to unknown). */
static int
test_mem_ldx_dw_heap(void)
{
	return verify_instruction((struct verify_instruction_param){
		.tested_instruction = {
			.code = (BPF_MEM | BPF_LDX | EBPF_DW),
			.off = 16,
		},
		.area_size = 24,
		.pre.src = make_pointer_domain(0, 0),
		.post.dst = unknown,
	});
}

REGISTER_FAST_TEST(bpf_validate_mem_ldx_dw_heap_autotest, NOHUGE_OK, ASAN_OK,
	test_mem_ldx_dw_heap);
