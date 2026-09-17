/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <cmdline_parse_string.h>

#include <stdint.h>


struct debug_command_comparison {
	cmdline_fixed_string_t lhs;
	cmdline_fixed_string_t op;
	union {
		cmdline_fixed_string_t register_rhs;
		int64_t literal_rhs;
	};
};

struct debug_command_parsed {
	cmdline_fixed_string_t keyword;  /* Any keyword we don't need */
	union {
		struct debug_command_comparison comparison;
		cmdline_fixed_string_t event;
		int32_t frame_offset;
		uint32_t instruction_count;
		uint32_t instruction_offset;
		uint32_t pc;
		uint32_t point_number;
		cmdline_fixed_string_t register_;
	};
};

enum debug_command {
	DEBUG_COMMAND_EOF,
	DEBUG_COMMAND_BREAK,
	DEBUG_COMMAND_BREAK_PC,
	DEBUG_COMMAND_CATCH,
	DEBUG_COMMAND_CLEAR,
	DEBUG_COMMAND_CLEAR_EVENT,
	DEBUG_COMMAND_CLEAR_PC,
	DEBUG_COMMAND_CONTINUE,
	DEBUG_COMMAND_DELETE,
	DEBUG_COMMAND_DELETE_NUMBER,
	DEBUG_COMMAND_INFO_FRAME,
	DEBUG_COMMAND_INFO_FRAME_OFFSET,
	DEBUG_COMMAND_INFO_POINTS,
	DEBUG_COMMAND_INFO_REGISTER,
	DEBUG_COMMAND_INFO_REGISTERS,
	DEBUG_COMMAND_LIST,
	DEBUG_COMMAND_LIST_COUNT,
	DEBUG_COMMAND_LIST_PROGRAM,
	DEBUG_COMMAND_MAY_LITERAL_RHS,
	DEBUG_COMMAND_MAY_REGISTER_RHS,
	DEBUG_COMMAND_QUIT,
	DEBUG_COMMAND_RUN,
	DEBUG_COMMAND_START,
	DEBUG_COMMAND_STEP,
	DEBUG_COMMAND_WHERE,
};

extern struct debug_command_parsed debug_command_parsed;

/** Get and return one command line command, storing its data in the struct above. */
enum debug_command
debug_command_get(const char *prompt);
