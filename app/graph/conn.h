/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_CONN_H
#define APP_GRAPH_CONN_H

#define CONN_WELCOME_LEN_MAX 1024
#define CONN_PROMPT_LEN_MAX 16

typedef void (*conn_msg_handle_t)(char *msg_in, char *msg_out, size_t msg_out_len_max, void *arg);

struct conn {
	char *welcome;
	char *prompt;
	char *buf;
	char *msg_in;
	char *msg_out;
	size_t buf_size;
	size_t msg_in_len_max;
	size_t msg_out_len_max;
	size_t msg_in_len;
	int fd_server;
	int fd_client_group;
	conn_msg_handle_t msg_handle;
	void *msg_handle_arg;
};

struct conn_params {
	const char *welcome;
	const char *prompt;
	const char *addr;
	uint16_t port;
	size_t buf_size;
	size_t msg_in_len_max;
	size_t msg_out_len_max;
	conn_msg_handle_t msg_handle;
	void *msg_handle_arg;
};

struct conn *conn_init(struct conn_params *p);
void conn_free(struct conn *conn);
int conn_req_poll(struct conn *conn);
int conn_msg_poll(struct conn *conn);

#endif
