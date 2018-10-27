/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_tailq.h>
#include <rte_string_fns.h>

#include "rte_telemetry_parser.h"

enum choices {
	INV_ACTION_VAL,
	INV_COMMAND_VAL,
	INV_DATA_VAL,
	INV_ACTION_FIELD,
	INV_COMMAND_FIELD,
	INV_DATA_FIELD,
	INV_JSON_FORMAT,
	VALID_REQ
};


#define TEST_CLIENT "/var/run/dpdk/test_client"

int32_t
rte_telemetry_create_test_socket(struct telemetry_impl *telemetry,
	const char *test_client_path)
{
	int ret, sockfd;
	struct sockaddr_un addr = {0};
	struct telemetry_client *client;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Telemetry argument has not been initialised");
		return -EINVAL;
	}

	sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		TELEMETRY_LOG_ERR("Test socket creation failure");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, test_client_path, sizeof(addr.sun_path));
	unlink(test_client_path);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		TELEMETRY_LOG_ERR("Test socket binding failure");
		return -1;
	}

	if (listen(sockfd, 1) < 0) {
		TELEMETRY_LOG_ERR("Listen failure");
		return -1;
	}

	ret = rte_telemetry_register_client(telemetry, test_client_path);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Register dummy client failed: %i", ret);
		return -1;
	}

	ret = accept(sockfd, NULL, NULL);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Socket accept failed");
		return -1;
	}

	TAILQ_FOREACH(client, &telemetry->client_list_head, client_list)
		telemetry->request_client = client;

	return 0;
}

int32_t
rte_telemetry_format_port_stat_ids(int *port_ids, int num_port_ids,
	const char * const *stat_names, int num_stat_names, json_t **data)
{

	int ret;
	json_t *stat_names_json_array = NULL;
	json_t *port_ids_json_array = NULL;
	uint32_t i;

	if (num_port_ids < 0) {
		TELEMETRY_LOG_ERR("Port Ids Count invalid");
		goto fail;
	}

	*data = json_object();
	if (*data == NULL) {
		TELEMETRY_LOG_ERR("Data json object creation failed");
		goto fail;
	}

	port_ids_json_array = json_array();
	if (port_ids_json_array == NULL) {
		TELEMETRY_LOG_ERR("port_ids_json_array creation failed");
		goto fail;
	}

	for (i = 0; i < (uint32_t)num_port_ids; i++) {
		ret = json_array_append(port_ids_json_array,
				json_integer(port_ids[i]));
		if (ret < 0) {
			TELEMETRY_LOG_ERR("JSON array creation failed");
			goto fail;
		}
	}

	ret = json_object_set_new(*data, "ports", port_ids_json_array);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Setting 'ports' value in data object failed");
		goto fail;
	}

	if (stat_names) {
		if (num_stat_names < 0) {
			TELEMETRY_LOG_ERR("Stat Names Count invalid");
			goto fail;
		}

		stat_names_json_array = json_array();
		if (stat_names_json_array == NULL) {
			TELEMETRY_LOG_ERR("stat_names_json_array creation failed");
			goto fail;
		}

		uint32_t i;
		for (i = 0; i < (uint32_t)num_stat_names; i++) {
			ret = json_array_append(stat_names_json_array,
				 json_string(stat_names[i]));
			if (ret < 0) {
				TELEMETRY_LOG_ERR("JSON array creation failed");
				goto fail;
			}
		}

		ret = json_object_set_new(*data, "stats", stat_names_json_array);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting 'stats' value in data object failed");
			goto fail;
		}
	}

	return 0;

fail:
	if (*data)
		json_decref(*data);
	if (stat_names_json_array)
		json_decref(stat_names_json_array);
	if (port_ids_json_array)
		json_decref(port_ids_json_array);
	return -1;
}

int32_t
rte_telemetry_create_json_request(int action, char *command,
	const char *client_path, int *port_ids, int num_port_ids,
	const char * const *stat_names, int num_stat_names, char **request,
	int inv_choice)
{
	int ret;
	json_t *root = json_object();
	json_t *data;

	if (root == NULL) {
		TELEMETRY_LOG_ERR("Could not create root json object");
		goto fail;
	}

	if (inv_choice == INV_ACTION_FIELD) {
		ret = json_object_set_new(root, "ac--on", json_integer(action));
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting invalid action field in root object failed");
			goto fail;
		}
	} else {
		ret = json_object_set_new(root, "action", json_integer(action));
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting valid action field in root object failed");
			goto fail;
		}
	}

	if (inv_choice == INV_COMMAND_FIELD) {
		ret = json_object_set_new(root, "co---nd", json_string(command));
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting invalid command field in root object failed");
			goto fail;
		}
	} else {
		ret = json_object_set_new(root, "command", json_string(command));
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting valid command field in root object failed");
			goto fail;
		}
	}

	data = json_null();
	if (client_path) {
		data = json_object();
		if (data == NULL) {
			TELEMETRY_LOG_ERR("Data json object creation failed");
			goto fail;
		}

		ret = json_object_set_new(data, "client_path",
				json_string(client_path));
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting valid client_path field in data object failed");
			goto fail;
		}

	} else if (port_ids) {
		ret = rte_telemetry_format_port_stat_ids(port_ids, num_port_ids,
				stat_names, num_stat_names, &data);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Formatting Port/Stat arrays failed");
			goto fail;
		}

	}

	if (inv_choice == INV_DATA_FIELD) {
		ret = json_object_set_new(root, "d--a", data);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting invalid data field in data object failed");
			goto fail;
		}
	} else {
		ret = json_object_set_new(root, "data", data);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Setting valid data field in data object failed");
			goto fail;
		}
	}

	*request = json_dumps(root, 0);
	if (*request == NULL) {
		TELEMETRY_LOG_ERR("Converting JSON root object to char* failed");
		goto fail;
	}

	json_decref(root);
	return 0;

fail:
	if (root)
		json_decref(root);
	return -1;
}

int32_t
rte_telemetry_send_get_ports_and_stats_request(struct telemetry_impl *telemetry,
	int action_choice, char *command_choice, int inv_choice)
{
	int ret;
	char *request;
	char *client_path_data = NULL;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Telemetry argument has not been initialised");
		return -EINVAL;
	}


	if (inv_choice == INV_ACTION_VAL)
		action_choice = -1;
	else if (inv_choice == INV_COMMAND_VAL)
		command_choice = "INVALID_COMMAND";
	else if (inv_choice == INV_DATA_VAL)
		client_path_data = "INVALID_DATA";

	ret = rte_telemetry_create_json_request(action_choice, command_choice,
		client_path_data, NULL, -1, NULL, -1, &request, inv_choice);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not create JSON Request");
		return -1;
	}

	if (inv_choice == INV_JSON_FORMAT)
		request++;

	ret = rte_telemetry_parse(telemetry, request);
	if (ret < 0) {
		TELEMETRY_LOG_WARN("Could not parse JSON Request");
		return -1;
	}

	return 0;
}

int32_t
rte_telemetry_send_get_ports_details_request(struct telemetry_impl *telemetry,
	int action_choice, int *port_ids, int num_port_ids, int inv_choice)
{
	int ret;
	char *request;
	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Telemetry argument has not been initialised");
		return -EINVAL;
	}

	char *command = "ports_details";

	if (inv_choice == INV_ACTION_VAL)
		action_choice = -1;
	else if (inv_choice == INV_COMMAND_VAL)
		command = "INVALID_COMMAND";
	else if (inv_choice == INV_DATA_VAL)
		port_ids = NULL;


	ret = rte_telemetry_create_json_request(action_choice, command, NULL,
		port_ids, num_port_ids, NULL, -1, &request, inv_choice);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not create JSON Request");
		return -1;
	}

	if (inv_choice == INV_JSON_FORMAT)
		request++;

	ret = rte_telemetry_parse(telemetry, request);
	if (ret < 0) {
		TELEMETRY_LOG_WARN("Could not parse JSON Request");
		return -1;
	}

	return 0;
}

int32_t
rte_telemetry_send_stats_values_by_name_request(struct telemetry_impl
	*telemetry, int action_choice, int *port_ids, int num_port_ids,
	const char * const *stat_names, int num_stat_names,
	int inv_choice)
{
	int ret;
	char *request;
	char *command = "ports_stats_values_by_name";

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Telemetry argument has not been initialised");
		return -EINVAL;
	}

	if (inv_choice == INV_ACTION_VAL)
		action_choice = -1;
	else if (inv_choice == INV_COMMAND_VAL)
		command = "INVALID_COMMAND";
	else if (inv_choice == INV_DATA_VAL) {
		port_ids = NULL;
		stat_names = NULL;
	}

	ret = rte_telemetry_create_json_request(action_choice, command, NULL,
		port_ids, num_port_ids, stat_names, num_stat_names, &request,
		inv_choice);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not create JSON Request");
		return -1;
	}

	if (inv_choice == INV_JSON_FORMAT)
		request++;

	ret = rte_telemetry_parse(telemetry, request);
	if (ret < 0) {
		TELEMETRY_LOG_WARN("Could not parse JSON Request");
		return -1;
	}

	return 0;
}

int32_t
rte_telemetry_send_unreg_request(struct telemetry_impl *telemetry,
	int action_choice, const char *client_path, int inv_choice)
{
	int ret;
	char *request;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Telemetry argument has not been initialised");
		return -EINVAL;
	}

	char *command = "clients";

	if (inv_choice == INV_ACTION_VAL)
		action_choice = -1;
	else if (inv_choice == INV_COMMAND_VAL)
		command = "INVALID_COMMAND";
	else if (inv_choice == INV_DATA_VAL)
		client_path = NULL;

	ret = rte_telemetry_create_json_request(action_choice, command,
		client_path, NULL, -1, NULL, -1, &request, inv_choice);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not create JSON Request");
		return -1;
	}

	if (inv_choice == INV_JSON_FORMAT)
		request++;

	ret = rte_telemetry_parse(telemetry, request);
	if (ret < 0) {
		TELEMETRY_LOG_WARN("Could not parse JSON Request");
		return -1;
	}

	return 0;
}

int32_t
rte_telemetry_parser_test(struct telemetry_impl *telemetry)
{
	int ret;
	const char *client_path = TEST_CLIENT;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Telemetry argument has not been initialised");
		return -EINVAL;
	}

	ret = rte_telemetry_create_test_socket(telemetry, client_path);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not create test request client socket");
		return -1;
	}

	int port_ids[] = {0, 1};
	int num_port_ids = RTE_DIM(port_ids);

	static const char * const stat_names[] = {"tx_good_packets",
		"rx_good_packets"};
	int num_stat_names = RTE_DIM(stat_names);

	static const char * const test_types[] = {
		"INVALID ACTION VALUE TESTS",
		"INVALID COMMAND VALUE TESTS",
		"INVALID DATA VALUE TESTS",
		"INVALID ACTION FIELD TESTS",
		"INVALID COMMAND FIELD TESTS",
		"INVALID DATA FIELD TESTS",
		"INVALID JSON FORMAT TESTS",
		"VALID TESTS"
	};


#define NUM_TEST_TYPES (sizeof(test_types)/sizeof(const char * const))

	uint32_t i;
	for (i = 0; i < NUM_TEST_TYPES; i++) {
		TELEMETRY_LOG_INFO("%s", test_types[i]);

		ret = rte_telemetry_send_get_ports_and_stats_request(telemetry,
			ACTION_GET, "ports", i);
		if (ret != 0 && i == VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports valid test failed");
			return -EPERM;
		} else if (ret != -1 && i != VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports invalid test failed");
			return -EPERM;
		}

		TELEMETRY_LOG_INFO("Success - Get ports test passed");

		ret = rte_telemetry_send_get_ports_details_request(telemetry,
			ACTION_GET, port_ids, num_port_ids, i);
		if (ret != 0 && i == VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports details valid");
			return -EPERM;
		} else if (ret != -1 && i != VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports details invalid");
			return -EPERM;
		}

		TELEMETRY_LOG_INFO("Success - Get ports details test passed");

		ret = rte_telemetry_send_get_ports_and_stats_request(telemetry,
			ACTION_GET, "port_stats", i);
		if (ret != 0  && i == VALID_REQ) {
			TELEMETRY_LOG_ERR("Get port stats valid test");
			return -EPERM;
		} else if (ret != -1 && i != VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports stats invalid test failed");
			return -EPERM;
		}

		TELEMETRY_LOG_INFO("Success - Get ports stats test passed");

		ret = rte_telemetry_send_stats_values_by_name_request(telemetry,
			ACTION_GET, port_ids, num_port_ids, stat_names,
			num_stat_names, i);
		if (ret != 0 && i == VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports stats values by name valid test failed");
			return -EPERM;
		} else if (ret != -1 && i != VALID_REQ) {
			TELEMETRY_LOG_ERR("Get ports stats values by name invalid test failed");
			return -EPERM;
		}

		TELEMETRY_LOG_INFO("Success - Get ports stats values by name test passed");

		ret = rte_telemetry_send_unreg_request(telemetry, ACTION_DELETE,
			client_path, i);
		if (ret != 0 && i == VALID_REQ) {
			TELEMETRY_LOG_ERR("Deregister valid test failed");
			return -EPERM;
		} else if (ret != -1 && i != VALID_REQ) {
			TELEMETRY_LOG_ERR("Deregister invalid test failed");
			return -EPERM;
		}

		TELEMETRY_LOG_INFO("Success - Deregister test passed");
	}

	return 0;
}
