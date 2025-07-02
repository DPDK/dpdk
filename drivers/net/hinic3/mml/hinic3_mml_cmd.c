/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#include "hinic3_mml_lib.h"
#include "hinic3_compat.h"
#include "hinic3_hwdev.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_ethdev.h"
#include "hinic3_mml_cmd.h"

/**
 * Compares two strings for equality.
 *
 * @param[in] command
 * The first string to compare.
 * @param[in] argument
 * The second string to compare.
 *
 * @return
 * UDA_TRUE if the strings are equal, otherwise UDA_FALSE.
 */
static int
string_cmp(const char *command, const char *argument)
{
	const char *cmd = command;
	const char *arg = argument;

	if (!cmd || !arg)
		return UDA_FALSE;

	if (strlen(cmd) != strlen(arg))
		return UDA_FALSE;

	do {
		if (*cmd != *arg)
			return UDA_FALSE;
		cmd++;
		arg++;
	} while (*cmd != '\0');

	return UDA_TRUE;
}

static void
show_tool_version(cmd_adapter_t *adapter)
{
	hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
			   "hinic3 pmd version %s", HINIC3_PMD_DRV_VERSION);
}

static void
show_tool_help(cmd_adapter_t *adapter)
{
	int i;
	major_cmd_t *major_cmd = NULL;

	if (!adapter)
		return;

	hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
			   "\n Usage:evsadm exec dump-hinic-status <major_cmd> "
			   "[option]\n");
	hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
			   "	-h, --help show help information");
	hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
			   "	-v, --version show version information");
	hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
			   "\n Major Commands:\n");

	for (i = 0; i < adapter->major_cmds; i++) {
		major_cmd = adapter->p_major_cmd[i];
		hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
				   "	%-23s %s", major_cmd->name,
				   major_cmd->description);
	}
	hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len, "\n");
}

void
major_command_option(major_cmd_t *major_cmd, const char *little,
		     const char *large, uint32_t have_param,
		     command_record_t record)
{
	cmd_option_t *option = NULL;

	if (major_cmd == NULL || (little == NULL && large == NULL) || !record) {
		PMD_DRV_LOG(ERR, "Invalid input parameter.");
		return;
	}

	if (major_cmd->option_count >= COMMAND_MAX_OPTIONS) {
		PMD_DRV_LOG(ERR, "Do not support more than %d options",
			    COMMAND_MAX_OPTIONS);
		return;
	}

	option = &major_cmd->options[major_cmd->option_count];
	major_cmd->options_repeat_flag[major_cmd->option_count] = 0;
	major_cmd->option_count++;

	option->record = record;
	option->little = little;
	option->large = large;
	option->have_param = have_param;
}

void
major_command_register(cmd_adapter_t *adapter, major_cmd_t *major_cmd)
{
	int i = 0;

	if (adapter == NULL || major_cmd == NULL) {
		PMD_DRV_LOG(ERR, "Invalid input parameter.");
		return;
	}

	if (adapter->major_cmds >= COMMAND_MAX_MAJORS) {
		PMD_DRV_LOG(ERR, "Major Commands is full");
		return;
	}
	while (adapter->p_major_cmd[i] != NULL)
		i++;
	adapter->p_major_cmd[i] = major_cmd;
	adapter->major_cmds++;
	major_cmd->adapter = adapter;
	major_cmd->err_no = UDA_SUCCESS;
	memset(major_cmd->err_str, 0, sizeof(major_cmd->err_str));
}

static int
is_help_version(cmd_adapter_t *adapter, int argc, char *arg)
{
	if (COMMAND_HELP_POSITION(argc) &&
	    (string_cmp("-h", arg) || string_cmp("--help", arg))) {
		show_tool_help(adapter);
		return UDA_TRUE;
	}

	if (COMMAND_VERSION_POSITION(argc) &&
	    (string_cmp("-v", arg) || string_cmp("--version", arg))) {
		show_tool_version(adapter);
		return UDA_TRUE;
	}

	return UDA_FALSE;
}

static int
check_command_length(int argc, char **argv)
{
	int i;
	unsigned long long str_len = 0;

	for (i = 1; i < argc; i++)
		str_len += strlen(argv[i]);

	if (str_len > COMMAND_MAX_STRING)
		return -UDA_EINVAL;

	return UDA_SUCCESS;
}

static inline int
char_check(const char cmd)
{
	if (cmd >= 'a' && cmd <= 'z')
		return UDA_SUCCESS;

	if (cmd >= 'A' && cmd <= 'Z')
		return UDA_SUCCESS;
	return UDA_FAIL;
}

static int
major_command_check_param(cmd_option_t *option, char *arg)
{
	if (!option)
		return -UDA_EINVAL;
	if (option->have_param != 0) {
		if (!arg || ((arg[0] == '-') && char_check(arg[1])))
			return -UDA_EINVAL;
		return UDA_SUCCESS;
	}

	return -UDA_ENOOBJ;
}

static int
major_cmd_repeat_option_set(major_cmd_t *major_cmd, const cmd_option_t *option,
			    u32 *options_repeat_flag)
{
	int err;

	if (*options_repeat_flag != 0) {
		major_cmd->err_no = -UDA_EINVAL;
		err = snprintf(major_cmd->err_str, COMMANDER_ERR_MAX_STRING - 1,
			       "Repeated option %s|%s.", option->little,
			       option->large);
		if (err <= 0) {
			PMD_DRV_LOG(ERR,
				"snprintf cmd repeat option failed, err: %d.",
				err);
		}
		return -UDA_EINVAL;
	}
	*options_repeat_flag = 1;
	return UDA_SUCCESS;
}

static int
major_cmd_option_check(major_cmd_t *major_cmd, char **argv, int *index)
{
	int j, ret, err, option_ok, intermediate_var;
	cmd_option_t *option = NULL;
	char *arg = argv[*index];

	/* Find command. */
	for (j = 0; j < major_cmd->option_count; j++) {
		option = &major_cmd->options[j];
		option_ok = (((option->little != NULL) &&
			      string_cmp(option->little, arg)) ||
			     ((option->large != NULL) &&
			      string_cmp(option->large, arg)));
		if (!option_ok)
			continue;
		/* Find same option. */
		ret = major_cmd_repeat_option_set(major_cmd,
			option, &major_cmd->options_repeat_flag[j]);
		if (ret != UDA_SUCCESS)
			return ret;

		arg = NULL;
		/* If this option need parameters. */
		intermediate_var = (*index) + 1;
		ret = major_command_check_param(option, argv[intermediate_var]);
		if (ret == UDA_SUCCESS) {
			(*index)++;
			arg = argv[*index];
		} else if (ret == -UDA_EINVAL) {
			major_cmd->err_no = -UDA_EINVAL;
			err = snprintf(major_cmd->err_str,
				       COMMANDER_ERR_MAX_STRING - 1,
				       "%s|%s option need parameter.",
				       option->little, option->large);
			if (err <= 0) {
				PMD_DRV_LOG(ERR,
					    "snprintf cmd option need para "
					    "failed, err: %d.",
					    err);
			}
			return -UDA_EINVAL;
		}

		/* Record messages. */
		ret = option->record(major_cmd, arg);
		if (ret != UDA_SUCCESS)
			return ret;
		break;
	}

	/* Illegal option. */
	if (j == major_cmd->option_count) {
		major_cmd->err_no = -UDA_EINVAL;
		err = snprintf(major_cmd->err_str, COMMANDER_ERR_MAX_STRING - 1,
			       "%s is not option needed.", arg);
		if (err <= 0) {
			PMD_DRV_LOG(ERR,
				"snprintf cmd option invalid failed, err: %d.",
				err);
		}
		return -UDA_EINVAL;
	}
	return UDA_SUCCESS;
}

static int
major_command_parse(major_cmd_t *major_cmd, int argc, char **argv)
{
	int i, err;

	for (i = 0; i < argc; i++) {
		err = major_cmd_option_check(major_cmd, argv, &i);
		if (err != UDA_SUCCESS)
			return err;
	}

	return UDA_SUCCESS;
}

static int
copy_result_to_buffer(void *buf_out, char *result, int len)
{
	int ret;

	ret = snprintf(buf_out, len - 1, "%s", result);
	if (ret <= 0)
		return 0;

	return ret + 1;
}

void
command_parse(cmd_adapter_t *adapter, int argc, char **argv, void *buf_out,
	      uint32_t *out_len)
{
	int i;
	major_cmd_t *major_cmd = NULL;
	char *arg = argv[1];

	if (is_help_version(adapter, argc, arg) == UDA_TRUE) {
		*out_len = (u32)copy_result_to_buffer(buf_out,
			adapter->show_str, MAX_SHOW_STR_LEN);
		return;
	}

	for (i = 0; i < adapter->major_cmds; i++) {
		major_cmd = adapter->p_major_cmd[i];

		/* Find major command. */
		if (!string_cmp(major_cmd->name, arg))
			continue;
		if (check_command_length(argc, argv) != UDA_SUCCESS) {
			major_cmd->err_no = -UDA_EINVAL;
			snprintf(major_cmd->err_str,
				       COMMANDER_ERR_MAX_STRING - 1,
				       "Command input too long.");
			break;
		}

		/* Deal sub command. */
		if (argc > SUB_COMMAND_OFFSET) {
			if (major_command_parse(major_cmd,
				    argc - SUB_COMMAND_OFFSET,
				    argv + SUB_COMMAND_OFFSET) != UDA_SUCCESS) {
				goto PARSE_OUT;
			}
		}

		/* Command exec. */
		major_cmd->execute(major_cmd);
		break;
	}

	/* Not find command. */
	if (i == adapter->major_cmds) {
		hinic3_pmd_mml_log(adapter->show_str, &adapter->show_len,
				   "Unknown major command, assign 'evsadm exec "
				   "dump-hinic-status -h' for help.");
		*out_len = (u32)copy_result_to_buffer(buf_out,
			adapter->show_str, MAX_SHOW_STR_LEN);
		return;
	}

PARSE_OUT:
	if (major_cmd->err_no != UDA_SUCCESS &&
	    major_cmd->err_no != -UDA_CANCEL) {
		PMD_DRV_LOG(ERR, "%s command error(%d): %s", major_cmd->name,
			    major_cmd->err_no, major_cmd->err_str);

		hinic3_pmd_mml_log(major_cmd->show_str, &major_cmd->show_len,
				   "%s command error(%d): %s",
				   major_cmd->name, major_cmd->err_no,
				   major_cmd->err_str);
	}
	*out_len = (u32)copy_result_to_buffer(buf_out, major_cmd->show_str,
					      MAX_SHOW_STR_LEN);
}

void
tool_target_init(int *bus_num, char *dev_name, int len)
{
	*bus_num = TRGET_UNKNOWN_BUS_NUM;
	memset(dev_name, 0, len);
}
