/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <jansson.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_metrics.h>
#include <rte_option.h>
#include <rte_string_fns.h>

#include "rte_telemetry.h"
#include "rte_telemetry_internal.h"
#include "rte_telemetry_parser.h"

#define BUF_SIZE 1024
#define ACTION_POST 1
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

int32_t
rte_telemetry_write_to_socket(struct telemetry_impl *telemetry,
	const char *json_string)
{
	int ret;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Could not initialise TELEMETRY_API");
		return -1;
	}

	if (telemetry->request_client == NULL) {
		TELEMETRY_LOG_ERR("No client has been chosen to write to");
		return -1;
	}

	if (json_string == NULL) {
		TELEMETRY_LOG_ERR("Invalid JSON string!");
		return -1;
	}

	ret = send(telemetry->request_client->fd,
			json_string, strlen(json_string), 0);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Failed to write to socket for client: %s",
				telemetry->request_client->file_path);
		return -1;
	}

	return 0;
}

int32_t
rte_telemetry_send_error_response(struct telemetry_impl *telemetry,
	int error_type)
{
	int ret;
	const char *status_code, *json_buffer;
	json_t *root;

	if (error_type == -EPERM)
		status_code = "Status Error: Unknown";
	else if (error_type == -EINVAL)
		status_code = "Status Error: Invalid Argument 404";
	else if (error_type == -ENOMEM)
		status_code = "Status Error: Memory Allocation Error";
	else {
		TELEMETRY_LOG_ERR("Invalid error type");
		return -EINVAL;
	}

	root = json_object();

	if (root == NULL) {
		TELEMETRY_LOG_ERR("Could not create root JSON object");
		return -EPERM;
	}

	ret = json_object_set_new(root, "status_code", json_string(status_code));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Status code field cannot be set");
		json_decref(root);
		return -EPERM;
	}

	ret = json_object_set_new(root, "data", json_null());
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Data field cannot be set");
		json_decref(root);
		return -EPERM;
	}

	json_buffer = json_dumps(root, JSON_INDENT(2));
	json_decref(root);

	ret = rte_telemetry_write_to_socket(telemetry, json_buffer);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not write to socket");
		return -EPERM;
	}

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
		telemetry->reg_index = rte_telemetry_reg_ethdev_to_metrics(pid);
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
rte_telemetry_read_client(struct telemetry_impl *telemetry)
{
	char buf[BUF_SIZE];
	int ret, buffer_read;

	buffer_read = read(telemetry->accept_fd, buf, BUF_SIZE-1);

	if (buffer_read == -1) {
		TELEMETRY_LOG_ERR("Read error");
		return -1;
	} else if (buffer_read == 0) {
		goto close_socket;
	} else {
		buf[buffer_read] = '\0';
		ret = rte_telemetry_parse_client_message(telemetry, buf);
		if (ret < 0)
			TELEMETRY_LOG_WARN("Parse message failed");
		goto close_socket;
	}

close_socket:
	if (close(telemetry->accept_fd) < 0) {
		TELEMETRY_LOG_ERR("Close TELEMETRY socket failed");
		free(telemetry);
		return -EPERM;
	}
	telemetry->accept_fd = 0;

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
	} else {
		ret = rte_telemetry_read_client(telemetry);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Failed to read socket buffer");
			return -1;
		}
	}

	return 0;
}

static int32_t
rte_telemetry_read_client_sockets(struct telemetry_impl *telemetry)
{
	int ret;
	telemetry_client *client;
	char client_buf[BUF_SIZE];
	int bytes;

	TAILQ_FOREACH(client, &telemetry->client_list_head, client_list) {
		bytes = read(client->fd, client_buf, BUF_SIZE-1);

		if (bytes > 0) {
			client_buf[bytes] = '\0';
			telemetry->request_client = client;
			ret = rte_telemetry_parse(telemetry, client_buf);
			if (ret < 0) {
				TELEMETRY_LOG_WARN("Parse socket input failed: %i",
						ret);
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

	ret = rte_telemetry_read_client_sockets(telemetry);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Client socket read failed");
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
	TAILQ_INIT(&static_telemetry->client_list_head);

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

static int32_t
rte_telemetry_client_cleanup(struct telemetry_client *client)
{
	int ret;

	ret = close(client->fd);
	free(client->file_path);
	free(client);

	if (ret < 0) {
		TELEMETRY_LOG_ERR("Close client socket failed");
		return -EPERM;
	}

	return 0;
}

int32_t __rte_experimental
rte_telemetry_cleanup(void)
{
	int ret;
	struct telemetry_impl *telemetry = static_telemetry;
	telemetry_client *client, *temp_client;

	TAILQ_FOREACH_SAFE(client, &telemetry->client_list_head, client_list,
		temp_client) {
		TAILQ_REMOVE(&telemetry->client_list_head, client, client_list);
		ret = rte_telemetry_client_cleanup(client);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Client cleanup failed");
			return -EPERM;
		}
	}

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

int32_t
rte_telemetry_unregister_client(struct telemetry_impl *telemetry,
	const char *client_path)
{
	int ret;
	telemetry_client *client, *temp_client;

	if (telemetry == NULL) {
		TELEMETRY_LOG_WARN("TELEMETRY is not initialised");
		return -ENODEV;
	}

	if (client_path == NULL) {
		TELEMETRY_LOG_ERR("Invalid client path");
		goto einval_fail;
	}

	if (TAILQ_EMPTY(&telemetry->client_list_head)) {
		TELEMETRY_LOG_ERR("There are no clients currently registered");
		return -EPERM;
	}

	TAILQ_FOREACH_SAFE(client, &telemetry->client_list_head, client_list,
			temp_client) {
		if (strcmp(client_path, client->file_path) == 0) {
			TAILQ_REMOVE(&telemetry->client_list_head, client,
				client_list);
			ret = rte_telemetry_client_cleanup(client);

			if (ret < 0) {
				TELEMETRY_LOG_ERR("Client cleanup failed");
				return -EPERM;
			}

			return 0;
		}
	}

	TELEMETRY_LOG_WARN("Couldn't find client, possibly not registered yet.");
	return -1;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -EINVAL;
}

int32_t
rte_telemetry_register_client(struct telemetry_impl *telemetry,
	const char *client_path)
{
	int ret, fd;
	struct sockaddr_un addrs;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Could not initialize TELEMETRY API");
		return -ENODEV;
	}

	if (client_path == NULL) {
		TELEMETRY_LOG_ERR("Invalid client path");
		return -EINVAL;
	}

	telemetry_client *client;
	TAILQ_FOREACH(client, &telemetry->client_list_head, client_list) {
		if (strcmp(client_path, client->file_path) == 0) {
			TELEMETRY_LOG_WARN("'%s' already registered",
					client_path);
			return -EINVAL;
		}
	}

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (fd == -1) {
		TELEMETRY_LOG_ERR("Client socket error");
		return -EACCES;
	}

	ret = rte_telemetry_set_socket_nonblock(fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not set socket to NONBLOCK");
		return -EPERM;
	}

	addrs.sun_family = AF_UNIX;
	strlcpy(addrs.sun_path, client_path, sizeof(addrs.sun_path));
	telemetry_client *new_client = malloc(sizeof(telemetry_client));
	new_client->file_path = strdup(client_path);
	new_client->fd = fd;

	if (connect(fd, (struct sockaddr *)&addrs, sizeof(addrs)) == -1) {
		TELEMETRY_LOG_ERR("TELEMETRY client connect to %s didn't work",
				client_path);
		ret = rte_telemetry_client_cleanup(new_client);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Client cleanup failed");
			return -EPERM;
		}
		return -EINVAL;
	}

	TAILQ_INSERT_HEAD(&telemetry->client_list_head, new_client, client_list);

	return 0;
}

int32_t
rte_telemetry_parse_client_message(struct telemetry_impl *telemetry, char *buf)
{
	int ret, action_int;
	json_error_t error;
	json_t *root = json_loads(buf, 0, &error);

	if (root == NULL) {
		TELEMETRY_LOG_WARN("Could not load JSON object from data passed in : %s",
				error.text);
		goto fail;
	} else if (!json_is_object(root)) {
		TELEMETRY_LOG_WARN("JSON Request is not a JSON object");
		goto fail;
	}

	json_t *action = json_object_get(root, "action");
	if (action == NULL) {
		TELEMETRY_LOG_WARN("Request does not have action field");
		goto fail;
	} else if (!json_is_integer(action)) {
		TELEMETRY_LOG_WARN("Action value is not an integer");
		goto fail;
	}

	json_t *command = json_object_get(root, "command");
	if (command == NULL) {
		TELEMETRY_LOG_WARN("Request does not have command field");
		goto fail;
	} else if (!json_is_string(command)) {
		TELEMETRY_LOG_WARN("Command value is not a string");
		goto fail;
	}

	action_int = json_integer_value(action);
	if (action_int != ACTION_POST) {
		TELEMETRY_LOG_WARN("Invalid action code");
		goto fail;
	}

	if (strcmp(json_string_value(command), "clients") != 0) {
		TELEMETRY_LOG_WARN("Invalid command");
		goto fail;
	}

	json_t *data = json_object_get(root, "data");
	if (data == NULL) {
		TELEMETRY_LOG_WARN("Request does not have data field");
		goto fail;
	}

	json_t *client_path = json_object_get(data, "client_path");
	if (client_path == NULL) {
		TELEMETRY_LOG_WARN("Request does not have client_path field");
		goto fail;
	}

	if (!json_is_string(client_path)) {
		TELEMETRY_LOG_WARN("Client_path value is not a string");
		goto fail;
	}

	ret = rte_telemetry_register_client(telemetry,
			json_string_value(client_path));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not register client");
		telemetry->register_fail_count++;
		goto fail;
	}

	return 0;

fail:
	TELEMETRY_LOG_WARN("Client attempted to register with invalid message");
	json_decref(root);
	return -1;
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
