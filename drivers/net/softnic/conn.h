/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef __INCLUDE_CONN_H__
#define __INCLUDE_CONN_H__

#include <stdint.h>

struct softnic_conn;

#ifndef CONN_WELCOME_LEN_MAX
#define CONN_WELCOME_LEN_MAX                               1024
#endif

#ifndef CONN_PROMPT_LEN_MAX
#define CONN_PROMPT_LEN_MAX                                16
#endif

typedef void (*softnic_conn_msg_handle_t)(char *msg_in,
	char *msg_out,
	size_t msg_out_len_max,
	void *arg);

struct softnic_conn_params {
	const char *welcome;
	const char *prompt;
	const char *addr;
	uint16_t port;
	size_t buf_size;
	size_t msg_in_len_max;
	size_t msg_out_len_max;
	softnic_conn_msg_handle_t msg_handle;
	void *msg_handle_arg;
};

struct softnic_conn *
softnic_conn_init(struct softnic_conn_params *p);

void
softnic_conn_free(struct softnic_conn *conn);

int
softnic_conn_poll_for_conn(struct softnic_conn *conn);

int
softnic_conn_poll_for_msg(struct softnic_conn *conn);

#endif
