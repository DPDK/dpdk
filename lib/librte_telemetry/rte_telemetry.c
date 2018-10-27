/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_metrics.h>
#include <rte_option.h>
#include <rte_string_fns.h>

#include "rte_telemetry.h"
#include "rte_telemetry_internal.h"

#define BUF_SIZE 1024
#define SLEEP_TIME 10

static telemetry_impl *static_telemetry;

static void
rte_telemetry_get_runtime_dir(char *socket_path, size_t size)
{
	snprintf(socket_path, size, "%s/telemetry", rte_eal_get_runtime_dir());
}

int32_t
rte_telemetry_is_port_active(int port_id)
{
	int ret;

	ret = rte_eth_find_next(port_id);
	if (ret == port_id)
		return 1;

	TELEMETRY_LOG_ERR("port_id: %d is invalid, not active",
		port_id);
	return 0;
}

static int32_t
rte_telemetry_reg_ethdev_to_metrics(uint16_t port_id)
{
	int ret, num_xstats, ret_val, i;
	struct rte_eth_xstat *eth_xstats = NULL;
	struct rte_eth_xstat_name *eth_xstats_names = NULL;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		TELEMETRY_LOG_ERR("port_id: %d is invalid", port_id);
		return -EINVAL;
	}

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get(%u) failed: %d",
				port_id, num_xstats);
		return -EPERM;
	}

	eth_xstats = malloc(sizeof(struct rte_eth_xstat) * num_xstats);
	if (eth_xstats == NULL) {
		TELEMETRY_LOG_ERR("Failed to malloc memory for xstats");
		return -ENOMEM;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	const char *xstats_names[num_xstats];
	eth_xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * num_xstats);
	if (ret < 0 || ret > num_xstats) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret_val = -EPERM;
		goto free_xstats;
	}

	if (eth_xstats_names == NULL) {
		TELEMETRY_LOG_ERR("Failed to malloc memory for xstats_names");
		ret_val = -ENOMEM;
		goto free_xstats;
	}

	ret = rte_eth_xstats_get_names(port_id, eth_xstats_names, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get_names(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret_val = -EPERM;
		goto free_xstats;
	}

	for (i = 0; i < num_xstats; i++)
		xstats_names[i] = eth_xstats_names[eth_xstats[i].id].name;

	ret_val = rte_metrics_reg_names(xstats_names, num_xstats);
	if (ret_val < 0) {
		TELEMETRY_LOG_ERR("rte_metrics_reg_names failed - metrics may already be registered");
		ret_val = -1;
		goto free_xstats;
	}

	goto free_xstats;

free_xstats:
	free(eth_xstats);
	free(eth_xstats_names);
	return ret_val;
}

static int32_t
rte_telemetry_initial_accept(struct telemetry_impl *telemetry)
{
	uint16_t pid;

	RTE_ETH_FOREACH_DEV(pid) {
		telemetry->reg_index =
			rte_telemetry_reg_ethdev_to_metrics(pid);
		break;
	}

	if (telemetry->reg_index < 0) {
		TELEMETRY_LOG_ERR("Failed to register ethdev metrics");
		return -1;
	}

	telemetry->metrics_register_done = 1;

	return 0;
}

static int32_t
rte_telemetry_accept_new_client(struct telemetry_impl *telemetry)
{
	int ret;

	if (telemetry->accept_fd <= 0) {
		ret = listen(telemetry->server_fd, 1);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Listening error with server fd");
			return -1;
		}
		telemetry->accept_fd = accept(telemetry->server_fd, NULL, NULL);

		if (telemetry->accept_fd >= 0 &&
			telemetry->metrics_register_done == 0) {
			ret = rte_telemetry_initial_accept(telemetry);
			if (ret < 0) {
				TELEMETRY_LOG_ERR("Failed to run initial configurations/tests");
				return -1;
			}
		}
	}

	return 0;
}

static int32_t
rte_telemetry_run(void *userdata)
{
	int ret;
	struct telemetry_impl *telemetry = userdata;

	if (telemetry == NULL) {
		TELEMETRY_LOG_WARN("TELEMETRY could not be initialised");
		return -1;
	}

	ret = rte_telemetry_accept_new_client(telemetry);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Accept and read new client failed");
		return -1;
	}

	return 0;
}

static void
*rte_telemetry_run_thread_func(void *userdata)
{
	int ret;
	struct telemetry_impl *telemetry = userdata;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("%s passed a NULL instance", __func__);
		pthread_exit(0);
	}

	while (telemetry->thread_status) {
		rte_telemetry_run(telemetry);
		ret = usleep(SLEEP_TIME);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Calling thread could not be put to sleep");
	}
	pthread_exit(0);
}

static int32_t
rte_telemetry_set_socket_nonblock(int fd)
{
	int flags;

	if (fd < 0) {
		TELEMETRY_LOG_ERR("Invalid fd provided");
		return -1;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int32_t
rte_telemetry_create_socket(struct telemetry_impl *telemetry)
{
	int ret;
	struct sockaddr_un addr;
	char socket_path[BUF_SIZE];

	if (telemetry == NULL)
		return -1;

	telemetry->server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (telemetry->server_fd == -1) {
		TELEMETRY_LOG_ERR("Failed to open socket");
		return -1;
	}

	ret  = rte_telemetry_set_socket_nonblock(telemetry->server_fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not set socket to NONBLOCK");
		goto close_socket;
	}

	addr.sun_family = AF_UNIX;
	rte_telemetry_get_runtime_dir(socket_path, sizeof(socket_path));
	strlcpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
	unlink(socket_path);

	if (bind(telemetry->server_fd, (struct sockaddr *)&addr,
		sizeof(addr)) < 0) {
		TELEMETRY_LOG_ERR("Socket binding error");
		goto close_socket;
	}

	return 0;

close_socket:
	if (close(telemetry->server_fd) < 0) {
		TELEMETRY_LOG_ERR("Close TELEMETRY socket failed");
		return -EPERM;
	}

	return -1;
}

int32_t __rte_experimental
rte_telemetry_init()
{
	int ret;
	pthread_attr_t attr;
	const char *telemetry_ctrl_thread = "telemetry";

	if (static_telemetry) {
		TELEMETRY_LOG_WARN("TELEMETRY structure already initialised");
		return -EALREADY;
	}

	static_telemetry = calloc(1, sizeof(struct telemetry_impl));
	if (static_telemetry == NULL) {
		TELEMETRY_LOG_ERR("Memory could not be allocated");
		return -ENOMEM;
	}

	static_telemetry->socket_id = rte_socket_id();
	rte_metrics_init(static_telemetry->socket_id);

	ret = pthread_attr_init(&attr);
	if (ret != 0) {
		TELEMETRY_LOG_ERR("Pthread attribute init failed");
		return -EPERM;
	}

	ret = rte_telemetry_create_socket(static_telemetry);
	if (ret < 0) {
		ret = rte_telemetry_cleanup();
		if (ret < 0)
			TELEMETRY_LOG_ERR("TELEMETRY cleanup failed");
		return -EPERM;
	}

	ret = rte_ctrl_thread_create(&static_telemetry->thread_id,
		telemetry_ctrl_thread, &attr, rte_telemetry_run_thread_func,
		(void *)static_telemetry);
	static_telemetry->thread_status = 1;

	if (ret < 0) {
		ret = rte_telemetry_cleanup();
		if (ret < 0)
			TELEMETRY_LOG_ERR("TELEMETRY cleanup failed");
		return -EPERM;
	}

	return 0;
}

int32_t __rte_experimental
rte_telemetry_cleanup(void)
{
	int ret;
	struct telemetry_impl *telemetry = static_telemetry;

	ret = close(telemetry->server_fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Close TELEMETRY socket failed");
		free(telemetry);
		return -EPERM;
	}

	telemetry->thread_status = 0;
	pthread_join(telemetry->thread_id, NULL);
	free(telemetry);
	static_telemetry = NULL;

	return 0;
}

int telemetry_log_level;

static struct rte_option option = {
	.opt_str = "--telemetry",
	.cb = &rte_telemetry_init,
	.enabled = 0
};

RTE_INIT(rte_telemetry_register)
{
	telemetry_log_level = rte_log_register("lib.telemetry");
	if (telemetry_log_level >= 0)
		rte_log_set_level(telemetry_log_level, RTE_LOG_ERR);

	rte_option_register(&option);
}
