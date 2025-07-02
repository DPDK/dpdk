/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#include "hinic3_mml_lib.h"
#include "hinic3_mml_cmd.h"

#define MAX_ARGC 20

/**
 * Free all memory associated with the command adapter, including the command
 * states and command buffer.
 *
 * @param[in] adapter
 * Pointer to command adapter.
 */
static void
cmd_deinit(cmd_adapter_t *adapter)
{
	int i;

	for (i = 0; i < COMMAND_MAX_MAJORS; i++) {
		if (adapter->p_major_cmd[i]) {
			if (adapter->p_major_cmd[i]->cmd_st) {
				free(adapter->p_major_cmd[i]->cmd_st);
				adapter->p_major_cmd[i]->cmd_st = NULL;
			}

			free(adapter->p_major_cmd[i]);
			adapter->p_major_cmd[i] = NULL;
		}
	}

	if (adapter->cmd_buf) {
		free(adapter->cmd_buf);
		adapter->cmd_buf = NULL;
	}
}

static int
cmd_init(cmd_adapter_t *adapter)
{
	int err;

	err = cmd_show_q_init(adapter);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Init cmd show queue fail");
		return err;
	}

	return UDA_SUCCESS;
}

/**
 * Separate the input command string into arguments.
 *
 * @param[in] adapter
 * Pointer to command adapter.
 * @param[in] buf_in
 * The input command string.
 * @param[in] in_size
 * The size of the input command string.
 * @param[out] argv
 * The array to store separated arguments.
 *
 * @return
 * The number of arguments on success, a negative error code otherwise.
 */
static int
cmd_separate(cmd_adapter_t *adapter, const char *buf_in, uint32_t in_size,
	     char **argv)
{
	char *cmd_buf = NULL;
	char *tmp = NULL;
	char *saveptr = NULL;
	int i;

	cmd_buf = calloc(1, in_size + 1);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd_buf");
		return -UDA_ENONMEM;
	}

	memcpy(cmd_buf, buf_in, in_size);

	tmp = cmd_buf;
	for (i = 1; i < MAX_ARGC; i++) {
		argv[i] = strtok_r(tmp, " ", &saveptr);
		if (!argv[i])
			break;
		tmp = NULL;
	}

	if (i == MAX_ARGC) {
		PMD_DRV_LOG(ERR, "Parameters is too many");
		free(cmd_buf);
		return -UDA_FAIL;
	}

	adapter->cmd_buf = cmd_buf;
	return i;
}

/**
 * Process the input command string, parse arguments, and return the result.
 *
 * @param[in] buf_in
 * The input command string.
 * @param[in] in_size
 * The size of the input command string.
 * @param[out] buf_out
 * The output buffer to store the command result.
 * @param[out] out_len
 * The length of the output buffer.
 * @param[in] max_buf_out_len
 * The maximum size of the output buffer.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_pmd_mml_lib(const char *buf_in, uint32_t in_size, char *buf_out,
		   uint32_t *out_len, uint32_t max_buf_out_len)
{
	cmd_adapter_t *adapter = NULL;
	char *argv[MAX_ARGC];
	int argc;
	int err = -UDA_EINVAL;

	if (!buf_in || !in_size) {
		PMD_DRV_LOG(ERR, "Invalid param, buf_in: %d, in_size: 0x%x",
			    !!buf_in, in_size);
		return err;
	}

	if (!buf_out || max_buf_out_len < MAX_SHOW_STR_LEN) {
		PMD_DRV_LOG(ERR,
			"Invalid param, buf_out: %d, max_buf_out_len: 0x%x",
			!!buf_out, max_buf_out_len);
		return err;
	}

	adapter = calloc(1, sizeof(cmd_adapter_t));
	if (!adapter) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd adapter");
		return -UDA_ENONMEM;
	}

	err = cmd_init(adapter);
	if (err != 0)
		goto parse_cmd_fail;

	argc = cmd_separate(adapter, buf_in, in_size, argv);
	if (argc < 0) {
		err = -UDA_FAIL;
		goto parse_cmd_fail;
	}

	memset(buf_out, 0, max_buf_out_len);
	command_parse(adapter, argc, argv, buf_out, out_len);

parse_cmd_fail:
	cmd_deinit(adapter);
	free(adapter);

	return err;
}
