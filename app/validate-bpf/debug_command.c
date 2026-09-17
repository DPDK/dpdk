/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "debug_command.h"

#include <rte_debug.h>
#include <cmdline.h>
#include <cmdline_socket.h>
#include <cmdline_parse_num.h>

#include <string.h>


#define EVENT_PATTERN \
	"invalid-state#" \
	"branch-enter#branch-prune#branch-return#branch-unreachable#" \
	"jump-always#jump-conditional"

#define REGISTER_PATTERN "r0#r1#r2#r3#r4#r5#r6#r7#r8#r9#r10"

#define COMPARISON_OP_PATTERN "==#!=#<#<=#>#>=#s<#s<=#s>#s>="

static void
handle_command(void *parsed_result, struct cmdline *cl, void *data);

/* Keywords */
static cmdline_parse_token_string_t break_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "b#break");
static cmdline_parse_token_string_t may_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "may");
static cmdline_parse_token_string_t catch_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "catch");
static cmdline_parse_token_string_t clear_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "clear");
static cmdline_parse_token_string_t continue_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "c#continue");
static cmdline_parse_token_string_t delete_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "delete");
static cmdline_parse_token_string_t info_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "i#info");
static cmdline_parse_token_string_t info_points_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword,
		"b#break#breakpoints#points");
static cmdline_parse_token_string_t info_frame_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "f#frame");
static cmdline_parse_token_string_t info_registers_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "r#registers");
static cmdline_parse_token_string_t list_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "l#list");
static cmdline_parse_token_string_t program_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "program");
static cmdline_parse_token_string_t quit_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "q#quit");
static cmdline_parse_token_string_t run_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "run");
static cmdline_parse_token_string_t start_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "start");
static cmdline_parse_token_string_t step_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "s#step");
static cmdline_parse_token_string_t where_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, keyword, "where");

/* Variable tokens */
static cmdline_parse_token_string_t comparison_lhs_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, comparison.lhs, REGISTER_PATTERN);
static cmdline_parse_token_string_t comparison_op_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, comparison.op, COMPARISON_OP_PATTERN);
static cmdline_parse_token_num_t comparison_rhs_literal_tok =
	TOKEN_NUM_INITIALIZER(struct debug_command_parsed, comparison.literal_rhs, RTE_INT64);
static cmdline_parse_token_string_t comparison_rhs_register_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, comparison.register_rhs,
		REGISTER_PATTERN);
static cmdline_parse_token_string_t event_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, event, EVENT_PATTERN);
static cmdline_parse_token_num_t frame_offset_tok =
	TOKEN_NUM_INITIALIZER(struct debug_command_parsed, frame_offset, RTE_INT64);
static cmdline_parse_token_num_t instruction_count_tok =
	TOKEN_NUM_INITIALIZER(struct debug_command_parsed, instruction_count, RTE_UINT32);
static cmdline_parse_token_num_t pc_tok =
	TOKEN_NUM_INITIALIZER(struct debug_command_parsed, instruction_offset, RTE_UINT32);
static cmdline_parse_token_num_t point_number_tok =
	TOKEN_NUM_INITIALIZER(struct debug_command_parsed, point_number, RTE_UINT32);
static cmdline_parse_token_string_t register_tok =
	TOKEN_STRING_INITIALIZER(struct debug_command_parsed, register_, REGISTER_PATTERN);


/* Commands */
static cmdline_parse_inst_t cmd_break = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_BREAK,
	.help_str = "b|break: break at current instruction",
	.tokens = {
		(void *)&break_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_break_pc = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_BREAK_PC,
	.help_str = "b|break <pc>: break at specified instruction",
	.tokens = {
		(void *)&break_tok,
		(void *)&pc_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_catch = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_CATCH,
	.help_str = "catch <event>: catch specified event",
	.tokens = {
		(void *)&catch_tok,
		(void *)&event_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_clear = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_CLEAR,
	.help_str = "clear: delete all breakpoints at current instruction",
	.tokens = {
		(void *)&clear_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_clear_event = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_CLEAR_EVENT,
	.help_str = "clear <event>: delete all catchpoints for specified event",
	.tokens = {
		(void *)&clear_tok,
		(void *)&event_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_clear_pc = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_CLEAR_PC,
	.help_str = "clear <pc>: delete all breakpoints at specified instruction",
	.tokens = {
		(void *)&clear_tok,
		(void *)&pc_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_continue = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_CONTINUE,
	.help_str = "c|continue: continue validation",
	.tokens = {
		(void *)&continue_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_delete = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_DELETE,
	.help_str = "delete: delete all breakpoints and catchpoints",
	.tokens = {
		(void *)&delete_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_delete_number = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_DELETE_NUMBER,
	.help_str = "delete <point>: delete specified breakpoint or catchpoint",
	.tokens = {
		(void *)&delete_tok,
		(void *)&point_number_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_info_frame = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_INFO_FRAME,
	.help_str = "i|info f|frame: show information about all frame locations",
	.tokens = {
		(void *)&info_tok,
		(void *)&info_frame_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_info_frame_offset = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_INFO_FRAME_OFFSET,
	.help_str = "i|info f|frame -<offset>: show information about specified frame location",
	.tokens = {
		(void *)&info_tok,
		(void *)&info_frame_tok,
		(void *)&frame_offset_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_info_points = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_INFO_POINTS,
	.help_str = "i|info b|break|breakpoints|points: "
		"show information about all breakpoints and catchpoints",
	.tokens = {
		(void *)&info_tok,
		(void *)&info_points_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_info_register = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_INFO_REGISTER,
	.help_str = "i|info <register>: show information about specified register",
	.tokens = {
		(void *)&info_tok,
		(void *)&register_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_info_registers = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_INFO_REGISTERS,
	.help_str = "i|info r|registers: show information about all registers",
	.tokens = {
		(void *)&info_tok,
		(void *)&info_registers_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_list = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_LIST,
	.help_str = "l|list: list ten instructions",
	.tokens = {
		(void *)&list_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_list_count = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_LIST_COUNT,
	.help_str = "l|list <number>: list specified number of instructions",
	.tokens = {
		(void *)&list_tok,
		(void *)&instruction_count_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_list_program = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_LIST_PROGRAM,
	.help_str = "l|list program: list whole program",
	.tokens = {
		(void *)&list_tok,
		(void *)&program_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_may_literal_rhs = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_MAY_LITERAL_RHS,
	.help_str = "may <register> <comparison> <number>: "
		"check if specified condition _may_ be true",
	.tokens = {
		(void *)&may_tok,
		(void *)&comparison_lhs_tok,
		(void *)&comparison_op_tok,
		(void *)&comparison_rhs_literal_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_may_register_rhs = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_MAY_REGISTER_RHS,
	.help_str = "may <register> <comparison> <register>: "
		"check if specified condition _may_ be true",
	.tokens = {
		(void *)&may_tok,
		(void *)&comparison_lhs_tok,
		(void *)&comparison_op_tok,
		(void *)&comparison_rhs_register_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_quit = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_QUIT,
	.help_str = "quit: q|quit debugger",
	.tokens = {
		(void *)&quit_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_run = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_RUN,
	.help_str = "run: re-run validation from the start",
	.tokens = {
		(void *)&run_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_start = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_START,
	.help_str = "start: re-start validation and stop at start",
	.tokens = {
		(void *)&start_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_step = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_STEP,
	.help_str = "s|step: validate one instruction",
	.tokens = {
		(void *)&step_tok,
		NULL,
	}
};
static cmdline_parse_inst_t cmd_where = {
	.f = handle_command,
	.data = (void *)DEBUG_COMMAND_WHERE,
	.help_str = "where: show current branch stack",
	.tokens = {
		(void *)&where_tok,
		NULL,
	}
};

static cmdline_parse_ctx_t debug_ctx[] = {
	&cmd_break,
	&cmd_break_pc,
	&cmd_catch,
	&cmd_clear,
	&cmd_clear_event,
	&cmd_clear_pc,
	&cmd_continue,
	&cmd_delete,
	&cmd_delete_number,
	&cmd_info_frame,
	&cmd_info_frame_offset,
	&cmd_info_points,
	&cmd_info_register,
	&cmd_info_registers,
	&cmd_list,
	&cmd_list_count,
	&cmd_list_program,
	&cmd_may_literal_rhs,
	&cmd_may_register_rhs,
	&cmd_quit,
	&cmd_run,
	&cmd_start,
	&cmd_step,
	&cmd_where,
	NULL
};

/* Receive, fill and return one command. */

static enum debug_command debug_command;

struct debug_command_parsed debug_command_parsed;

static void
handle_command(void *parsed_result, struct cmdline *cl, void *data)
{
	RTE_BUILD_BUG_ON(sizeof(debug_command_parsed) > CMDLINE_PARSE_RESULT_BUFSIZE);
	memcpy(&debug_command_parsed, parsed_result, sizeof(debug_command_parsed));
	debug_command = (uintptr_t)data;
	cmdline_quit(cl);
}

enum debug_command
debug_command_get(const char *prompt)
{
	debug_command = DEBUG_COMMAND_EOF;
	struct cmdline *const cmdline = cmdline_stdin_new(debug_ctx, prompt);
	RTE_VERIFY(cmdline != NULL);
	cmdline_interact(cmdline);
	cmdline_stdin_exit(cmdline);
	/* Clear prompt, or it would prepend first message that follows. */
	printf("\r%*s\r", (int)strlen(prompt), "");
	fflush(stdout);
	return debug_command;
}
