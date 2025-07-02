/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef _HINIC3_MML_CMD
#define _HINIC3_MML_CMD

#include <stdint.h>

#define COMMAND_HELP_POSITION(argc)            \
	({                                    \
		typeof(argc) __argc = (argc); \
		(__argc == 1 || __argc == 2); \
	})
#define COMMAND_VERSION_POSITION(argc) ((argc) == 2)
#define SUB_COMMAND_OFFSET	      2

#define COMMAND_MAX_MAJORS	 128
#define COMMAND_MAX_OPTIONS	 64
#define PARAM_MAX_STRING	 128
#define COMMAND_MAX_STRING	 512
#define COMMANDER_ERR_MAX_STRING 128

#define MAX_NAME_LEN	 32
#define MAX_DES_LEN	 128
#define MAX_SHOW_STR_LEN 2048

struct tag_major_cmd_t;
struct tag_cmd_adapter_t;

typedef int (*command_record_t)(struct tag_major_cmd_t *major, char *param);
typedef void (*command_executeute_t)(struct tag_major_cmd_t *major);

typedef struct {
	const char *little;
	const char *large;
	unsigned int have_param;
	command_record_t record;
} cmd_option_t;

/* Major command structure for save command details and options. */
typedef struct tag_major_cmd_t {
	struct tag_cmd_adapter_t *adapter;
	char name[MAX_NAME_LEN];
	int option_count;
	cmd_option_t options[COMMAND_MAX_OPTIONS];
	uint32_t options_repeat_flag[COMMAND_MAX_OPTIONS];
	command_executeute_t execute;
	int err_no;
	char err_str[COMMANDER_ERR_MAX_STRING];
	char show_str[MAX_SHOW_STR_LEN];
	int show_len;
	char description[MAX_DES_LEN];
	void *cmd_st; /**< Command show queue state structure. */
} major_cmd_t;

typedef struct tag_cmd_adapter_t {
	const char *name;
	const char *version;
	major_cmd_t *p_major_cmd[COMMAND_MAX_MAJORS];
	int major_cmds;
	char show_str[MAX_SHOW_STR_LEN];
	int show_len;
	char *cmd_buf;
} cmd_adapter_t;

/**
 * Add an option to a major command.
 *
 * This function adds a command option with its short and long forms, whether it
 * requires a parameter, and the function to handle it.
 *
 * @param[in] major_cmd
 * Pointer to the major command structure.
 * @param[in] little
 * Short form of the option.
 * @param[in] large
 * Long form of the option.
 * @param[in] have_param
 * Flag indicating whether the option requires a parameter.
 * @param[in] record
 * Function to handle the option's action.
 */
void major_command_option(major_cmd_t *major_cmd, const char *little,
			  const char *large, uint32_t have_param,
			  command_record_t record);

/**
 * Register a major command with adapter.
 *
 * @param[in] adapter
 * Pointer to command adapter.
 * @param[in] major_cmd
 * The major command to be registered with the adapter.
 */
void major_command_register(cmd_adapter_t *adapter, major_cmd_t *major_cmd);

/**
 * Parse and execute commands.
 *
 * @param[in] adapter
 * Pointer to command adapter.
 * @param[in] argc
 * The number of command arguments.
 * @param[in] argv
 * The array of command arguments.
 * @param[out] buf_out
 * The buffer used to store the output result.
 * @param[out] out_len
 * The length (in bytes) of the output result.
 */
void command_parse(cmd_adapter_t *adapter, int argc, char **argv, void *buf_out,
		   uint32_t *out_len);

/**
 * Initialize the target bus number and device name.
 *
 * @param[out] bus_num
 * Pointer to the bus number, which will be set to a default unknown value.
 * @param[out] dev_name
 * Pointer to the device name buffer, which will be cleared (set to zeros).
 * @param[in] len
 * The length of the device name buffer.
 */
void tool_target_init(int *bus_num, char *dev_name, int len);

int cmd_show_q_init(cmd_adapter_t *adapter);
int cmd_show_xstats_init(cmd_adapter_t *adapter);
int cmd_show_dump_init(cmd_adapter_t *adapter);

#endif /* _HINIC3_MML_CMD */
