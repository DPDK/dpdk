/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdbool.h>

#include "rte_telemetry_internal.h"

#ifndef _RTE_TELEMETRY_SOCKET_TESTING_H_
#define _RTE_TELEMETRY_SOCKET_TESTING_H_

int32_t
rte_telemetry_json_socket_message_test(struct telemetry_impl *telemetry,
	int fd);

int32_t
rte_telemetry_invalid_json_test(struct telemetry_impl *telemetry, int fd);

int32_t
rte_telemetry_valid_json_test(struct telemetry_impl *telemetry, int fd);

int32_t
rte_telemetry_json_contents_test(struct telemetry_impl *telemetry, int fd);

int32_t
rte_telemetry_json_empty_test(struct telemetry_impl *telemetry, int fd);

int32_t
rte_telemetry_socket_register_test(struct telemetry_impl *telemetry, int *fd,
	int send_fd, int recv_fd);

int32_t
rte_telemetry_socket_test_setup(struct telemetry_impl *telemetry, int *send_fd,
	int *recv_fd);

#endif
