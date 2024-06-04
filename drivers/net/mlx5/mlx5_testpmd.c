/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef RTE_EXEC_ENV_WINDOWS
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_alarm.h>
#include <rte_pmd_mlx5.h>
#include <rte_ethdev.h>

#include "mlx5_testpmd.h"
#include "testpmd.h"

#define SHAPER_DISABLE_DELAY_US 100000 /* 100ms */
#define MAX_GENEVE_OPTIONS_RESOURCES 7
#define PARSE_DELIMITER " \f\n\r\t\v"
#define SPACE_DELIMITER (" ")

static uint8_t host_shaper_avail_thresh_triggered[RTE_MAX_ETHPORTS];

struct mlx5_port {
	void *geneve_tlv_parser_handle;
};

static struct mlx5_port private_port[RTE_MAX_ETHPORTS] = {{0}};

struct tlv_list_manager {
	uint8_t nb_options;
	struct rte_pmd_mlx5_geneve_tlv tlv_list[MAX_GENEVE_OPTIONS_RESOURCES];
};

static struct tlv_list_manager tlv_mng = {.nb_options = 0};

static int
parse_uint(uint64_t *value, const char *str)
{
	char *next = NULL;
	uint64_t n;

	errno = 0;
	/* Parse number string */
	if (!strncasecmp(str, "0x", 2)) {
		str += 2;
		n = strtol(str, &next, 16);
	} else {
		n = strtol(str, &next, 10);
	}
	if (errno != 0 || str == next || *next != '\0')
		return -1;

	*value = n;

	return 0;
}

/**
 * Disable the host shaper and re-arm available descriptor threshold event.
 *
 * @param[in] args
 *   uint32_t integer combining port_id and rxq_id.
 */
static void
mlx5_test_host_shaper_disable(void *args)
{
	uint32_t port_rxq_id = (uint32_t)(uintptr_t)args;
	uint16_t port_id = port_rxq_id & 0xffff;
	uint16_t qid = (port_rxq_id >> 16) & 0xffff;
	struct rte_eth_rxq_info qinfo;
	struct rte_port *port;

	port = &ports[port_id];
	if (port->port_status != RTE_PORT_STARTED) {
		printf("%s port_status(%d) is incorrect, stop avail_thresh "
		       "event processing.\n",
		       __func__, port->port_status);
		return;
	}
	printf("%s disable shaper\n", __func__);
	if (rte_eth_rx_queue_info_get(port_id, qid, &qinfo)) {
		printf("rx_queue_info_get returns error\n");
		return;
	}
	/* Rearm the available descriptor threshold event. */
	if (rte_eth_rx_avail_thresh_set(port_id, qid, qinfo.avail_thresh)) {
		printf("config avail_thresh returns error\n");
		return;
	}
	/* Only disable the shaper when avail_thresh_triggered is set. */
	if (host_shaper_avail_thresh_triggered[port_id] &&
	    rte_pmd_mlx5_host_shaper_config(port_id, 0, 0))
		printf("%s disable shaper returns error\n", __func__);
}

void
mlx5_test_avail_thresh_event_handler(uint16_t port_id, uint16_t rxq_id)
{
	struct rte_eth_dev_info dev_info;
	uint32_t port_rxq_id = port_id | (rxq_id << 16);

	/* Ensure it's MLX5 port. */
	if (rte_eth_dev_info_get(port_id, &dev_info) != 0 ||
	    (strncmp(dev_info.driver_name, "mlx5", 4) != 0))
		return;
	rte_eal_alarm_set(SHAPER_DISABLE_DELAY_US,
			  mlx5_test_host_shaper_disable,
			  (void *)(uintptr_t)port_rxq_id);
	printf("%s port_id:%u rxq_id:%u\n", __func__, port_id, rxq_id);
}

/**
 * Configure host shaper's avail_thresh_triggered and current rate.
 *
 * @param[in] avail_thresh_triggered
 *   Disable/enable avail_thresh_triggered.
 * @param[in] rate
 *   Configure current host shaper rate.
 * @return
 *   On success, returns 0.
 *   On failure, returns < 0.
 */
static int
mlx5_test_set_port_host_shaper(uint16_t port_id, uint16_t avail_thresh_triggered, uint8_t rate)
{
	struct rte_eth_link link;
	bool port_id_valid = false;
	uint16_t pid;
	int ret;

	RTE_ETH_FOREACH_DEV(pid)
		if (port_id == pid) {
			port_id_valid = true;
			break;
		}
	if (!port_id_valid)
		return -EINVAL;
	ret = rte_eth_link_get_nowait(port_id, &link);
	if (ret < 0)
		return ret;
	host_shaper_avail_thresh_triggered[port_id] = avail_thresh_triggered ? 1 : 0;
	if (!avail_thresh_triggered) {
		ret = rte_pmd_mlx5_host_shaper_config(port_id, 0,
		RTE_BIT32(RTE_PMD_MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED));
	} else {
		ret = rte_pmd_mlx5_host_shaper_config(port_id, 1,
		RTE_BIT32(RTE_PMD_MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED));
	}
	if (ret)
		return ret;
	ret = rte_pmd_mlx5_host_shaper_config(port_id, rate, 0);
	if (ret)
		return ret;
	return 0;
}

#ifndef RTE_EXEC_ENV_WINDOWS
static const char*
mlx5_test_get_socket_path(char *extend)
{
	if (strstr(extend, "socket=") == extend) {
		const char *socket_path = strchr(extend, '=') + 1;

		TESTPMD_LOG(DEBUG, "MLX5 socket path is %s\n", socket_path);
		return socket_path;
	}

	TESTPMD_LOG(ERR, "Failed to extract a valid socket path from %s\n",
		    extend);
	return NULL;
}

static int
mlx5_test_extend_devargs(char *identifier, char *extend)
{
	struct sockaddr_un un = {
		.sun_family = AF_UNIX,
	};
	int cmd_fd;
	int pd_handle;
	struct iovec iov = {
		.iov_base = &pd_handle,
		.iov_len = sizeof(int),
	};
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} control;
	struct msghdr msgh = {
		.msg_iov = NULL,
		.msg_iovlen = 0,
	};
	struct cmsghdr *cmsg;
	const char *path = mlx5_test_get_socket_path(extend + 1);
	size_t len = 1;
	int socket_fd;
	int ret;

	if (path == NULL) {
		TESTPMD_LOG(ERR, "Invalid devargs extension is specified\n");
		return -1;
	}

	/* Initialize IPC channel. */
	socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socket_fd < 0) {
		TESTPMD_LOG(ERR, "Failed to create unix socket: %s\n",
			    strerror(errno));
		return -1;
	}
	rte_strlcpy(un.sun_path, path, sizeof(un.sun_path));
	if (connect(socket_fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
		TESTPMD_LOG(ERR, "Failed to connect %s: %s\n", un.sun_path,
			    strerror(errno));
		close(socket_fd);
		return -1;
	}

	/* Send the request message. */
	do {
		ret = sendmsg(socket_fd, &msgh, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		TESTPMD_LOG(ERR, "Failed to send request to (%s): %s\n", path,
			    strerror(errno));
		close(socket_fd);
		return -1;
	}

	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control.buf;
	msgh.msg_controllen = sizeof(control.buf);
	do {
		ret = recvmsg(socket_fd, &msgh, 0);
	} while (ret < 0);
	if (ret != sizeof(int) || (msgh.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) {
		TESTPMD_LOG(ERR, "truncated msg");
		close(socket_fd);
		return -1;
	}

	/* Translate the FD. */
	cmsg = CMSG_FIRSTHDR(&msgh);
	if (cmsg == NULL || cmsg->cmsg_len != CMSG_LEN(sizeof(int)) ||
	    cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
		TESTPMD_LOG(ERR, "Fail to get FD using SCM_RIGHTS mechanism\n");
		close(socket_fd);
		unlink(un.sun_path);
		return -1;
	}
	memcpy(&cmd_fd, CMSG_DATA(cmsg), sizeof(int));

	TESTPMD_LOG(DEBUG, "Command FD (%d) and PD handle (%d) "
		    "are successfully imported from remote process\n",
		    cmd_fd, pd_handle);

	/* Cleanup IPC channel. */
	close(socket_fd);

	/* Calculate the new length of devargs string. */
	len += snprintf(NULL, 0, ",cmd_fd=%d,pd_handle=%d", cmd_fd, pd_handle);
	/* Extend the devargs string. */
	snprintf(extend, len, ",cmd_fd=%d,pd_handle=%d", cmd_fd, pd_handle);

	TESTPMD_LOG(DEBUG, "Attach port with extra devargs %s\n", identifier);
	return 0;
}

static bool
is_delimiter_path_spaces(char *extend)
{
	while (*extend != '\0') {
		if (*extend != ' ')
			return true;
		extend++;
	}
	return false;
}

/*
 * Extend devargs list with "cmd_fd" and "pd_handle" coming from external
 * process. It happens only in this format:
 *  testpmd> mlx5 port attach (identifier) socket=<socket path>
 * all "(identifier) socket=<socket path>" is in the same string pointed
 * by the input parameter 'identifier'.
 *
 * @param identifier
 *   Identifier of port attach command line.
 */
static void
mlx5_test_attach_port_extend_devargs(char *identifier)
{
	char *extend;

	if (identifier == NULL) {
		fprintf(stderr, "Invalid parameters are specified\n");
		return;
	}

	extend = strchr(identifier, ' ');
	if (extend != NULL && is_delimiter_path_spaces(extend) &&
	    mlx5_test_extend_devargs(identifier, extend) < 0) {
		TESTPMD_LOG(ERR, "Failed to extend devargs for port %s\n",
			    identifier);
		return;
	}

	attach_port(identifier);
}
#endif

static inline const char *
mode2string(uint8_t mode)
{
	switch (mode) {
	case 0:
		return "ignored\t";
	case 1:
		return "fixed\t";
	case 2:
		return "matchable";
	default:
		break;
	}
	return "unknown";
}

static inline uint8_t
string2mode(const char *mode)
{
	if (strcmp(mode, "ignored") == 0)
		return 0;
	if (strcmp(mode, "fixed") == 0)
		return 1;
	if (strcmp(mode, "matchable") == 0)
		return 2;
	return UINT8_MAX;
}

static int
mlx5_test_parse_geneve_option_data(const char *buff, uint8_t data_len,
				   rte_be32_t **match_data_mask)
{
	rte_be32_t *data;
	char *buff2;
	char *token;
	uint8_t i = 0;

	if (data_len == 0) {
		*match_data_mask = NULL;
		return 0;
	}

	data = calloc(data_len, sizeof(rte_be32_t));
	if (data == NULL) {
		TESTPMD_LOG(ERR, "Fail to allocate memory for GENEVE TLV option data\n");
		return -ENOMEM;
	}

	buff2 = strdup(buff);
	if (buff2 == NULL) {
		TESTPMD_LOG(ERR,
			    "Fail to duplicate GENEVE TLV option data string (%s)\n",
			    buff);
		free(data);
		return -ENOMEM;
	}

	token = strtok(buff2, SPACE_DELIMITER);
	while (token != NULL) {
		if (i == data_len) {
			TESTPMD_LOG(ERR,
				    "GENEVE TLV option has more data then given data length %u\n",
				    data_len);
			free(buff2);
			free(data);
			return -EINVAL;
		}

		if (strcmp(token, "0xffffffff") == 0)
			data[i] = 0xffffffff;
		else
			data[i] = 0x0;

		token = strtok(NULL, SPACE_DELIMITER);
		i++;
	}

	free(buff2);
	*match_data_mask = data;
	return 0;
}

/* *** SET HOST_SHAPER FOR A PORT *** */
struct cmd_port_host_shaper_result {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t port;
	uint16_t port_num;
	cmdline_fixed_string_t host_shaper;
	cmdline_fixed_string_t avail_thresh_triggered;
	uint16_t fr;
	cmdline_fixed_string_t rate;
	uint8_t rate_num;
};

static void cmd_port_host_shaper_parsed(void *parsed_result,
		__rte_unused struct cmdline *cl,
		__rte_unused void *data)
{
	struct cmd_port_host_shaper_result *res = parsed_result;
	int ret = 0;

	if ((strcmp(res->mlx5, "mlx5") == 0) &&
	    (strcmp(res->set, "set") == 0) &&
	    (strcmp(res->port, "port") == 0) &&
	    (strcmp(res->host_shaper, "host_shaper") == 0) &&
	    (strcmp(res->avail_thresh_triggered, "avail_thresh_triggered") == 0) &&
	    (strcmp(res->rate, "rate") == 0))
		ret = mlx5_test_set_port_host_shaper(res->port_num, res->fr,
					   res->rate_num);
	if (ret < 0)
		printf("cmd_port_host_shaper error: (%s)\n", strerror(-ret));
}

static cmdline_parse_token_string_t cmd_port_host_shaper_mlx5 =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				mlx5, "mlx5");
static cmdline_parse_token_string_t cmd_port_host_shaper_set =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				set, "set");
static cmdline_parse_token_string_t cmd_port_host_shaper_port =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				port, "port");
static cmdline_parse_token_num_t cmd_port_host_shaper_portnum =
	TOKEN_NUM_INITIALIZER(struct cmd_port_host_shaper_result,
				port_num, RTE_UINT16);
static cmdline_parse_token_string_t cmd_port_host_shaper_host_shaper =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				 host_shaper, "host_shaper");
static cmdline_parse_token_string_t cmd_port_host_shaper_avail_thresh_triggered =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				 avail_thresh_triggered, "avail_thresh_triggered");
static cmdline_parse_token_num_t cmd_port_host_shaper_fr =
	TOKEN_NUM_INITIALIZER(struct cmd_port_host_shaper_result,
			      fr, RTE_UINT16);
static cmdline_parse_token_string_t cmd_port_host_shaper_rate =
	TOKEN_STRING_INITIALIZER(struct cmd_port_host_shaper_result,
				 rate, "rate");
static cmdline_parse_token_num_t cmd_port_host_shaper_rate_num =
	TOKEN_NUM_INITIALIZER(struct cmd_port_host_shaper_result,
			      rate_num, RTE_UINT8);
static cmdline_parse_inst_t mlx5_test_cmd_port_host_shaper = {
	.f = cmd_port_host_shaper_parsed,
	.data = (void *)0,
	.help_str = "mlx5 set port <port_id> host_shaper avail_thresh_triggered <0|1> "
	"rate <rate_num>: Set HOST_SHAPER avail_thresh_triggered and rate with port_id",
	.tokens = {
		(void *)&cmd_port_host_shaper_mlx5,
		(void *)&cmd_port_host_shaper_set,
		(void *)&cmd_port_host_shaper_port,
		(void *)&cmd_port_host_shaper_portnum,
		(void *)&cmd_port_host_shaper_host_shaper,
		(void *)&cmd_port_host_shaper_avail_thresh_triggered,
		(void *)&cmd_port_host_shaper_fr,
		(void *)&cmd_port_host_shaper_rate,
		(void *)&cmd_port_host_shaper_rate_num,
		NULL,
	}
};

#ifndef RTE_EXEC_ENV_WINDOWS
/* *** attach a specified port *** */
struct mlx5_cmd_operate_attach_port_result {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t keyword;
	cmdline_multi_string_t identifier;
};

static void mlx5_cmd_operate_attach_port_parsed(void *parsed_result,
						__rte_unused struct cmdline *cl,
						__rte_unused void *data)
{
	struct mlx5_cmd_operate_attach_port_result *res = parsed_result;

	if (!strcmp(res->keyword, "attach"))
		mlx5_test_attach_port_extend_devargs(res->identifier);
	else
		fprintf(stderr, "Unknown parameter\n");
}

static cmdline_parse_token_string_t mlx5_cmd_operate_attach_port_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_operate_attach_port_result,
				 mlx5, "mlx5");
static cmdline_parse_token_string_t mlx5_cmd_operate_attach_port_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_operate_attach_port_result,
				 port, "port");
static cmdline_parse_token_string_t mlx5_cmd_operate_attach_port_keyword =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_operate_attach_port_result,
				 keyword, "attach");
static cmdline_parse_token_string_t mlx5_cmd_operate_attach_port_identifier =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_operate_attach_port_result,
				 identifier, TOKEN_STRING_MULTI);

static cmdline_parse_inst_t mlx5_cmd_operate_attach_port = {
	.f = mlx5_cmd_operate_attach_port_parsed,
	.data = NULL,
	.help_str = "mlx5 port attach <identifier> socket=<path>: "
		"(identifier: pci address or virtual dev name"
		", path (optional): socket path to get cmd FD and PD handle)",
	.tokens = {
		(void *)&mlx5_cmd_operate_attach_port_mlx5,
		(void *)&mlx5_cmd_operate_attach_port_port,
		(void *)&mlx5_cmd_operate_attach_port_keyword,
		(void *)&mlx5_cmd_operate_attach_port_identifier,
		NULL,
	},
};
#endif

/* Map HW queue index to rte queue index. */
struct mlx5_cmd_map_ext_rxq {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t ext_rxq;
	cmdline_fixed_string_t map;
	uint16_t sw_queue_id;
	uint32_t hw_queue_id;
};

cmdline_parse_token_string_t mlx5_cmd_map_ext_rxq_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_map_ext_rxq, mlx5, "mlx5");
cmdline_parse_token_string_t mlx5_cmd_map_ext_rxq_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_map_ext_rxq, port, "port");
cmdline_parse_token_num_t mlx5_cmd_map_ext_rxq_port_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_map_ext_rxq, port_id, RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_map_ext_rxq_ext_rxq =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_map_ext_rxq, ext_rxq,
				 "ext_rxq");
cmdline_parse_token_string_t mlx5_cmd_map_ext_rxq_map =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_map_ext_rxq, map, "map");
cmdline_parse_token_num_t mlx5_cmd_map_ext_rxq_sw_queue_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_map_ext_rxq, sw_queue_id,
			      RTE_UINT16);
cmdline_parse_token_num_t mlx5_cmd_map_ext_rxq_hw_queue_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_map_ext_rxq, hw_queue_id,
			      RTE_UINT32);

static void
mlx5_cmd_map_ext_rxq_parsed(void *parsed_result,
			    __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	struct mlx5_cmd_map_ext_rxq *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;
	ret = rte_pmd_mlx5_external_rx_queue_id_map(res->port_id,
						    res->sw_queue_id,
						    res->hw_queue_id);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid ethdev index (%u), out of range\n",
			res->sw_queue_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %u\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	case -EEXIST:
		fprintf(stderr, "mapping with index %u already exists\n",
			res->sw_queue_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t mlx5_cmd_map_ext_rxq = {
	.f = mlx5_cmd_map_ext_rxq_parsed,
	.data = NULL,
	.help_str = "mlx5 port <port_id> ext_rxq map <sw_queue_id> <hw_queue_id>",
	.tokens = {
		(void *)&mlx5_cmd_map_ext_rxq_mlx5,
		(void *)&mlx5_cmd_map_ext_rxq_port,
		(void *)&mlx5_cmd_map_ext_rxq_port_id,
		(void *)&mlx5_cmd_map_ext_rxq_ext_rxq,
		(void *)&mlx5_cmd_map_ext_rxq_map,
		(void *)&mlx5_cmd_map_ext_rxq_sw_queue_id,
		(void *)&mlx5_cmd_map_ext_rxq_hw_queue_id,
		NULL,
	}
};

/* Unmap HW queue index to rte queue index. */
struct mlx5_cmd_unmap_ext_rxq {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t ext_rxq;
	cmdline_fixed_string_t unmap;
	uint16_t queue_id;
};

cmdline_parse_token_string_t mlx5_cmd_unmap_ext_rxq_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_unmap_ext_rxq, mlx5, "mlx5");
cmdline_parse_token_string_t mlx5_cmd_unmap_ext_rxq_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_unmap_ext_rxq, port, "port");
cmdline_parse_token_num_t mlx5_cmd_unmap_ext_rxq_port_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_unmap_ext_rxq, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_unmap_ext_rxq_ext_rxq =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_unmap_ext_rxq, ext_rxq,
				 "ext_rxq");
cmdline_parse_token_string_t mlx5_cmd_unmap_ext_rxq_unmap =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_unmap_ext_rxq, unmap, "unmap");
cmdline_parse_token_num_t mlx5_cmd_unmap_ext_rxq_queue_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_unmap_ext_rxq, queue_id,
			      RTE_UINT16);

static void
mlx5_cmd_unmap_ext_rxq_parsed(void *parsed_result,
			      __rte_unused struct cmdline *cl,
			      __rte_unused void *data)
{
	struct mlx5_cmd_unmap_ext_rxq *res = parsed_result;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;
	ret = rte_pmd_mlx5_external_rx_queue_id_unmap(res->port_id,
						      res->queue_id);
	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid rte_flow index (%u), "
			"out of range, doesn't exist or still referenced\n",
			res->queue_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %u\n", res->port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "function not implemented or supported\n");
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
	}
}

cmdline_parse_inst_t mlx5_cmd_unmap_ext_rxq = {
	.f = mlx5_cmd_unmap_ext_rxq_parsed,
	.data = NULL,
	.help_str = "mlx5 port <port_id> ext_rxq unmap <queue_id>",
	.tokens = {
		(void *)&mlx5_cmd_unmap_ext_rxq_mlx5,
		(void *)&mlx5_cmd_unmap_ext_rxq_port,
		(void *)&mlx5_cmd_unmap_ext_rxq_port_id,
		(void *)&mlx5_cmd_unmap_ext_rxq_ext_rxq,
		(void *)&mlx5_cmd_unmap_ext_rxq_unmap,
		(void *)&mlx5_cmd_unmap_ext_rxq_queue_id,
		NULL,
	}
};

/* Set flow engine mode with flags command. */
struct mlx5_cmd_set_flow_engine_mode {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t flow_engine;
	cmdline_multi_string_t mode;
};

static int
parse_multi_token_flow_engine_mode(char *t_str,
		enum rte_pmd_mlx5_flow_engine_mode *mode, uint32_t *flag)
{
	uint64_t val;
	char *token;
	int ret;

	*flag = 0;
	/* First token: mode string */
	token = strtok_r(t_str, PARSE_DELIMITER, &t_str);
	if (token ==  NULL)
		return -1;

	if (!strcmp(token, "active"))
		*mode = RTE_PMD_MLX5_FLOW_ENGINE_MODE_ACTIVE;
	else if (!strcmp(token, "standby"))
		*mode = RTE_PMD_MLX5_FLOW_ENGINE_MODE_STANDBY;
	else
		return -1;

	/* Second token: flag */
	token = strtok_r(t_str, PARSE_DELIMITER, &t_str);
	if (token == NULL)
		return 0;

	ret = parse_uint(&val, token);
	if (ret != 0 || val > UINT32_MAX)
		return -1;

	*flag = val;
	return 0;
}

static void
mlx5_cmd_set_flow_engine_mode_parsed(void *parsed_result,
				     __rte_unused struct cmdline *cl,
				     __rte_unused void *data)
{
	struct mlx5_cmd_set_flow_engine_mode *res = parsed_result;
	enum rte_pmd_mlx5_flow_engine_mode mode;
	uint32_t flag;
	int ret;

	ret = parse_multi_token_flow_engine_mode(res->mode, &mode, &flag);

	if (ret < 0) {
		fprintf(stderr, "Bad input\n");
		return;
	}

	ret = rte_pmd_mlx5_flow_engine_set_mode(mode, flag);

	if (ret < 0)
		fprintf(stderr, "Fail to set flow_engine to %s mode with flag 0x%x, error %s\n",
			mode == RTE_PMD_MLX5_FLOW_ENGINE_MODE_ACTIVE ? "active" : "standby", flag,
			strerror(-ret));
	else
		TESTPMD_LOG(DEBUG, "Set %d ports flow_engine to %s mode with flag 0x%x\n", ret,
			mode == RTE_PMD_MLX5_FLOW_ENGINE_MODE_ACTIVE ? "active" : "standby", flag);
}

cmdline_parse_token_string_t mlx5_cmd_set_flow_engine_mode_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_flow_engine_mode, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_set_flow_engine_mode_set =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_flow_engine_mode, set,
				 "set");
cmdline_parse_token_string_t mlx5_cmd_set_flow_engine_mode_flow_engine =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_flow_engine_mode, flow_engine,
				 "flow_engine");
cmdline_parse_token_string_t mlx5_cmd_set_flow_engine_mode_mode =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_flow_engine_mode, mode,
				 TOKEN_STRING_MULTI);

cmdline_parse_inst_t mlx5_cmd_set_flow_engine_mode = {
	.f = &mlx5_cmd_set_flow_engine_mode_parsed,
	.data = NULL,
	.help_str = "mlx5 set flow_engine <active|standby> [<flag>]",
	.tokens = {
		(void *)&mlx5_cmd_set_flow_engine_mode_mlx5,
		(void *)&mlx5_cmd_set_flow_engine_mode_set,
		(void *)&mlx5_cmd_set_flow_engine_mode_flow_engine,
		(void *)&mlx5_cmd_set_flow_engine_mode_mode,
		NULL,
	}
};

/* Prepare single GENEVE TLV option and add it into global option list. */
struct mlx5_cmd_set_tlv_option {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t set;
	cmdline_fixed_string_t tlv_option;
	cmdline_fixed_string_t class;
	uint16_t class_id;
	cmdline_fixed_string_t type;
	uint8_t type_id;
	cmdline_fixed_string_t len;
	uint8_t option_len;
	cmdline_fixed_string_t offset;
	uint8_t off;
	cmdline_fixed_string_t sample_len;
	uint8_t length;
	cmdline_fixed_string_t class_mode;
	cmdline_fixed_string_t cmode;
	cmdline_fixed_string_t data;
	cmdline_fixed_string_t data_mask;
};

cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, mlx5, "mlx5");
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_set =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, set, "set");
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_tlv_option =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, tlv_option,
				 "tlv_option");
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_class =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, class,
				 "class");
cmdline_parse_token_num_t mlx5_cmd_set_tlv_option_class_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_set_tlv_option, class_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_type =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, type, "type");
cmdline_parse_token_num_t mlx5_cmd_set_tlv_option_type_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_set_tlv_option, type_id,
			      RTE_UINT8);
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_len =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, len, "len");
cmdline_parse_token_num_t mlx5_cmd_set_tlv_option_option_len =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_set_tlv_option, option_len,
			      RTE_UINT8);
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_offset =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, offset,
				 "offset");
cmdline_parse_token_num_t mlx5_cmd_set_tlv_option_off =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_set_tlv_option, off, RTE_UINT8);
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_sample_len =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, sample_len,
				 "sample_len");
cmdline_parse_token_num_t mlx5_cmd_set_tlv_option_length =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_set_tlv_option, length,
			      RTE_UINT8);
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_class_mode =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, class_mode,
				 "class_mode");
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_cmode =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, cmode,
				 "ignored#fixed#matchable");
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_data =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, data, "data");
cmdline_parse_token_string_t mlx5_cmd_set_tlv_option_data_mask =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_set_tlv_option, data_mask, "");

static void
mlx5_cmd_set_tlv_option_parsed(void *parsed_result,
			       __rte_unused struct cmdline *cl,
			       __rte_unused void *data)
{
	struct mlx5_cmd_set_tlv_option *res = parsed_result;
	struct rte_pmd_mlx5_geneve_tlv *option;
	uint8_t class_mode;
	int ret;

	if (tlv_mng.nb_options == MAX_GENEVE_OPTIONS_RESOURCES) {
		fprintf(stderr, "GENEVE TLV option list is full\n");
		return;
	}

	if (res->option_len < res->length + res->off) {
		fprintf(stderr,
			"GENEVE TLV option length (%u) cannot be less than offset (%u) + sample_len (%u)\n",
			res->option_len, res->length, res->off);
		return;
	}

	if (res->option_len > 32) {
		fprintf(stderr,
			"GENEVE TLV option length (%u) must be less than 32\n",
			res->option_len);
		return;
	}

	class_mode = string2mode(res->cmode);
	if (class_mode == UINT8_MAX) {
		fprintf(stderr, "Invalid class mode \"%s\"\n", res->cmode);
		return;
	}

	if (res->length > 0) {
		if (strcmp(res->data, "data") || !strcmp(res->data_mask, "")) {
			fprintf(stderr,
				"sample_len is %u but any data isn't provided\n",
				res->length);
			return;
		}
	} else {
		if (!strcmp(res->data, "data") && strcmp(res->data_mask, "")) {
			fprintf(stderr,
				"sample_len is 0 but data is provided (%s)\n",
				res->data_mask);
			return;
		}
	}

	option = &tlv_mng.tlv_list[tlv_mng.nb_options];
	ret = mlx5_test_parse_geneve_option_data(res->data_mask, res->length,
						 &option->match_data_mask);
	if (ret < 0)
		return;

	option->match_on_class_mode = class_mode;
	option->option_class = rte_cpu_to_be_16(res->class_id);
	option->option_type = res->type_id;
	option->option_len = res->option_len;
	option->offset = res->off;
	option->sample_len = res->length;
	tlv_mng.nb_options++;

	TESTPMD_LOG(DEBUG,
		    "set new option in global list, now it has %u options\n",
		    tlv_mng.nb_options);
}

cmdline_parse_inst_t mlx5_cmd_set_tlv_option = {
	.f = mlx5_cmd_set_tlv_option_parsed,
	.data = NULL,
	.help_str = "mlx5 set tlv_option class <class_id> type <type_id> len "
		"<option_len> offset <sample_offset> sample_len "
		"<sample_length> class_mode <ignored|fixed|matchable> data <mask1 [mask2 [...]>",
	.tokens = {
		(void *)&mlx5_cmd_set_tlv_option_mlx5,
		(void *)&mlx5_cmd_set_tlv_option_set,
		(void *)&mlx5_cmd_set_tlv_option_tlv_option,
		(void *)&mlx5_cmd_set_tlv_option_class,
		(void *)&mlx5_cmd_set_tlv_option_class_id,
		(void *)&mlx5_cmd_set_tlv_option_type,
		(void *)&mlx5_cmd_set_tlv_option_type_id,
		(void *)&mlx5_cmd_set_tlv_option_len,
		(void *)&mlx5_cmd_set_tlv_option_option_len,
		(void *)&mlx5_cmd_set_tlv_option_offset,
		(void *)&mlx5_cmd_set_tlv_option_off,
		(void *)&mlx5_cmd_set_tlv_option_sample_len,
		(void *)&mlx5_cmd_set_tlv_option_length,
		(void *)&mlx5_cmd_set_tlv_option_class_mode,
		(void *)&mlx5_cmd_set_tlv_option_cmode,
		(void *)&mlx5_cmd_set_tlv_option_data,
		(void *)&mlx5_cmd_set_tlv_option_data_mask,
		NULL,
	}
};

/* Print all GENEVE TLV options which are configured so far. */
struct mlx5_cmd_list_tlv_options {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t list;
	cmdline_fixed_string_t tlv_options;
};

cmdline_parse_token_string_t mlx5_cmd_list_tlv_options_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_list_tlv_options, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_list_tlv_options_list =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_list_tlv_options, list,
				 "list");
cmdline_parse_token_string_t mlx5_cmd_list_tlv_options_tlv_options =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_list_tlv_options, tlv_options,
				 "tlv_options");

static void
mlx5_cmd_list_tlv_options_parsed(__rte_unused void *parsed_result,
				 __rte_unused struct cmdline *cl,
				 __rte_unused void *data)
{
	struct rte_pmd_mlx5_geneve_tlv *option;
	uint8_t i, j;

	printf("ID\tType\tClass\tClass_mode\tLen\tOffset\tSample_len\tData\n");
	for (i = 0; i < tlv_mng.nb_options; ++i) {
		option = &tlv_mng.tlv_list[i];
		printf("%u\t%u\t%u\t%s\t%u\t%u\t%u\t\t", i,
		       option->option_type, rte_be_to_cpu_16(option->option_class),
		       mode2string(option->match_on_class_mode),
		       option->option_len,
		       option->offset, option->sample_len);
		for (j = 0; j < option->sample_len; ++j)
			printf("0x%x ", option->match_data_mask[j]);
		printf("\n");
	}
}

cmdline_parse_inst_t mlx5_cmd_list_tlv_options = {
	.f = mlx5_cmd_list_tlv_options_parsed,
	.data = NULL,
	.help_str = "mlx5 list tlv_options",
	.tokens = {
		(void *)&mlx5_cmd_list_tlv_options_mlx5,
		(void *)&mlx5_cmd_list_tlv_options_list,
		(void *)&mlx5_cmd_list_tlv_options_tlv_options,
		NULL,
	}
};

/* Clear all GENEVE TLV options which are configured so far. */
struct mlx5_cmd_flush_tlv_options {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t flush;
	cmdline_fixed_string_t tlv_options;
	cmdline_fixed_string_t max;
	uint8_t number;
};

cmdline_parse_token_string_t mlx5_cmd_flush_tlv_options_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_flush_tlv_options, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_flush_tlv_options_flush =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_flush_tlv_options, flush,
				 "flush");
cmdline_parse_token_string_t mlx5_cmd_flush_tlv_options_tlv_options =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_flush_tlv_options, tlv_options,
				 "tlv_options");
cmdline_parse_token_string_t mlx5_cmd_flush_tlv_options_max =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_flush_tlv_options, max, "max");
cmdline_parse_token_num_t mlx5_cmd_flush_tlv_options_number =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_flush_tlv_options, number,
			      RTE_UINT8);

static void
mlx5_cmd_flush_tlv_options_parsed(void *parsed_result,
				  __rte_unused struct cmdline *cl,
				  __rte_unused void *data)
{
	struct mlx5_cmd_flush_tlv_options *res = parsed_result;
	struct rte_pmd_mlx5_geneve_tlv *option;
	uint8_t nb_options_flush = tlv_mng.nb_options;
	uint8_t nb_options_left = 0;

	if (strcmp(res->max, "max") == 0 && res->number < tlv_mng.nb_options) {
		nb_options_left = tlv_mng.nb_options - res->number;
		nb_options_flush = RTE_MIN(res->number, nb_options_flush);
	}

	while (tlv_mng.nb_options > nb_options_left) {
		tlv_mng.nb_options--;
		option = &tlv_mng.tlv_list[tlv_mng.nb_options];
		if (option->match_data_mask) {
			free(option->match_data_mask);
			option->match_data_mask = NULL;
		}
	}

	TESTPMD_LOG(DEBUG, "Flush %u latest configured GENEVE TLV options, "
		    "current number of options in the list is %u\n",
		    nb_options_flush, nb_options_left);
}

cmdline_parse_inst_t mlx5_cmd_flush_tlv_options = {
	.f = mlx5_cmd_flush_tlv_options_parsed,
	.data = NULL,
	.help_str = "mlx5 flush tlv_options max <nb_options>",
	.tokens = {
		(void *)&mlx5_cmd_flush_tlv_options_mlx5,
		(void *)&mlx5_cmd_flush_tlv_options_flush,
		(void *)&mlx5_cmd_flush_tlv_options_tlv_options,
		(void *)&mlx5_cmd_flush_tlv_options_max,
		(void *)&mlx5_cmd_flush_tlv_options_number,
		NULL,
	}
};

/* Create GENEVE TLV parser using option list which is configured before. */
struct mlx5_cmd_apply_tlv_options {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t apply;
	cmdline_fixed_string_t tlv_options;
};

cmdline_parse_token_string_t mlx5_cmd_apply_tlv_options_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_apply_tlv_options, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_apply_tlv_options_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_apply_tlv_options, port,
				 "port");
cmdline_parse_token_num_t mlx5_cmd_apply_tlv_options_port_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_apply_tlv_options, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_apply_tlv_options_apply =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_apply_tlv_options, apply,
				 "apply");
cmdline_parse_token_string_t mlx5_cmd_apply_tlv_options_tlv_options =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_apply_tlv_options, tlv_options,
				 "tlv_options");

static void
mlx5_cmd_apply_tlv_options_parsed(void *parsed_result,
				  __rte_unused struct cmdline *cl,
				  __rte_unused void *data)
{
	struct mlx5_cmd_apply_tlv_options *res = parsed_result;
	struct mlx5_port *port;
	void *handle;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	if (tlv_mng.nb_options == 0) {
		fprintf(stderr, "The option list is empty, please set options\n");
		return;
	}

	handle = rte_pmd_mlx5_create_geneve_tlv_parser(res->port_id,
						       tlv_mng.tlv_list,
						       tlv_mng.nb_options);
	if (handle == NULL) {
		fprintf(stderr,
			"Fail to create GENEVE TLV parser, nb_option=%u: %s\n",
			tlv_mng.nb_options, strerror(rte_errno));
		return;
	}

	TESTPMD_LOG(DEBUG, "GENEVE TLV options parser is successfully created:"
		    " nb_option=%u, handle=%p\n", tlv_mng.nb_options, handle);

	port = &private_port[res->port_id];
	port->geneve_tlv_parser_handle = handle;
}

cmdline_parse_inst_t mlx5_cmd_apply_tlv_options = {
	.f = mlx5_cmd_apply_tlv_options_parsed,
	.data = NULL,
	.help_str = "mlx5 port <port_id> apply tlv_options",
	.tokens = {
		(void *)&mlx5_cmd_apply_tlv_options_mlx5,
		(void *)&mlx5_cmd_apply_tlv_options_port,
		(void *)&mlx5_cmd_apply_tlv_options_port_id,
		(void *)&mlx5_cmd_apply_tlv_options_apply,
		(void *)&mlx5_cmd_apply_tlv_options_tlv_options,
		NULL,
	}
};

/* Destroy GENEVE TLV parser created by apply command. */
struct mlx5_cmd_destroy_tlv_options {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t destroy;
	cmdline_fixed_string_t tlv_options;
};

cmdline_parse_token_string_t mlx5_cmd_destroy_tlv_options_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_destroy_tlv_options, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_destroy_tlv_options_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_destroy_tlv_options, port,
				 "port");
cmdline_parse_token_num_t mlx5_cmd_destroy_tlv_options_port_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_destroy_tlv_options, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_destroy_tlv_options_destroy =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_destroy_tlv_options, destroy,
				 "destroy");
cmdline_parse_token_string_t mlx5_cmd_destroy_tlv_options_tlv_options =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_destroy_tlv_options, tlv_options,
				 "tlv_options");

static void
mlx5_cmd_destroy_tlv_options_parsed(void *parsed_result,
				    __rte_unused struct cmdline *cl,
				    __rte_unused void *data)
{
	struct mlx5_cmd_destroy_tlv_options *res = parsed_result;
	struct mlx5_port *port;
	int ret;

	if (port_id_is_invalid(res->port_id, ENABLED_WARN))
		return;

	port = &private_port[res->port_id];
	if (!port->geneve_tlv_parser_handle)
		return;

	ret = rte_pmd_mlx5_destroy_geneve_tlv_parser(port->geneve_tlv_parser_handle);
	if (ret < 0) {
		fprintf(stderr, "Fail to destroy GENEVE TLV parser: %s\n",
			strerror(-ret));
		return;
	}

	TESTPMD_LOG(DEBUG, "GENEVE TLV options parser is successfully released:"
		    " handle=%p\n", port->geneve_tlv_parser_handle);

	port->geneve_tlv_parser_handle = NULL;
}

cmdline_parse_inst_t mlx5_cmd_destroy_tlv_options = {
	.f = mlx5_cmd_destroy_tlv_options_parsed,
	.data = NULL,
	.help_str = "mlx5 port <port_id> destroy tlv_options",
	.tokens = {
		(void *)&mlx5_cmd_destroy_tlv_options_mlx5,
		(void *)&mlx5_cmd_destroy_tlv_options_port,
		(void *)&mlx5_cmd_destroy_tlv_options_port_id,
		(void *)&mlx5_cmd_destroy_tlv_options_destroy,
		(void *)&mlx5_cmd_destroy_tlv_options_tlv_options,
		NULL,
	}
};

/* Dump SQ Context for a given port/queue*/
struct mlx5_cmd_dump_sq_context_options {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t queue;
	queueid_t queue_id;
	cmdline_fixed_string_t dump;
	cmdline_fixed_string_t sq_context;
	cmdline_fixed_string_t file_name;
};

cmdline_parse_token_string_t mlx5_cmd_dump_sq_context_options_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_dump_sq_context_options_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, port,
				 "port");
cmdline_parse_token_num_t mlx5_cmd_dump_sq_context_options_port_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_dump_sq_context_options_queue =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, queue,
				 "queue");
cmdline_parse_token_num_t mlx5_cmd_dump_sq_context_options_queue_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, queue_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_dump_sq_context_options_dump =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, dump,
				 "dump");
cmdline_parse_token_string_t mlx5_cmd_dump_sq_context_options_sq_context =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, sq_context,
				 "sq_context");
cmdline_parse_token_string_t mlx5_cmd_dump_sq_context_options_file_name =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_sq_context_options, file_name,
				 NULL);

static void
mlx5_cmd_dump_sq_context_options_parsed(void *parsed_result,
				    __rte_unused struct cmdline *cl,
				    __rte_unused void *data)
{
	struct mlx5_cmd_dump_sq_context_options *res = parsed_result;
	int ret;

	ret = rte_pmd_mlx5_txq_dump_contexts(res->port_id, res->queue_id, res->file_name);

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid queue index (%u), out of range\n",
			res->queue_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %u\n", res->port_id);
		break;
	case -EIO:
		fprintf(stderr, "File Access Error (%s)\n", strerror(rte_errno));
		break;
	default:
		fprintf(stderr, "Unable to dump SQ/CQ HW Context (%s)\n", strerror(rte_errno));
	}
}

cmdline_parse_inst_t mlx5_cmd_dump_sq_context_options = {
	.f = mlx5_cmd_dump_sq_context_options_parsed,
	.data = NULL,
	.help_str = "mlx5 port <port_id> queue <queue_id> dump sq_context <file_name>",
	.tokens = {
		(void *)&mlx5_cmd_dump_sq_context_options_mlx5,
		(void *)&mlx5_cmd_dump_sq_context_options_port,
		(void *)&mlx5_cmd_dump_sq_context_options_port_id,
		(void *)&mlx5_cmd_dump_sq_context_options_queue,
		(void *)&mlx5_cmd_dump_sq_context_options_queue_id,
		(void *)&mlx5_cmd_dump_sq_context_options_dump,
		(void *)&mlx5_cmd_dump_sq_context_options_sq_context,
		(void *)&mlx5_cmd_dump_sq_context_options_file_name,
		NULL,
	}
};

/* Dump RQ Context for a given port/queue*/
struct mlx5_cmd_dump_rq_context_options {
	cmdline_fixed_string_t mlx5;
	cmdline_fixed_string_t port;
	portid_t port_id;
	cmdline_fixed_string_t queue;
	queueid_t queue_id;
	cmdline_fixed_string_t dump;
	cmdline_fixed_string_t rq_context;
	cmdline_fixed_string_t file_name;
};

cmdline_parse_token_string_t mlx5_cmd_dump_rq_context_options_mlx5 =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, mlx5,
				 "mlx5");
cmdline_parse_token_string_t mlx5_cmd_dump_rq_context_options_port =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, port,
				 "port");
cmdline_parse_token_num_t mlx5_cmd_dump_rq_context_options_port_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, port_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_dump_rq_context_options_queue =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, queue,
				 "queue");
cmdline_parse_token_num_t mlx5_cmd_dump_rq_context_options_queue_id =
	TOKEN_NUM_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, queue_id,
			      RTE_UINT16);
cmdline_parse_token_string_t mlx5_cmd_dump_rq_context_options_dump =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, dump,
				 "dump");
cmdline_parse_token_string_t mlx5_cmd_dump_rq_context_options_rq_context =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, rq_context,
				 "rq_context");
cmdline_parse_token_string_t mlx5_cmd_dump_rq_context_options_file_name =
	TOKEN_STRING_INITIALIZER(struct mlx5_cmd_dump_rq_context_options, file_name,
				 NULL);

static void
mlx5_cmd_dump_rq_context_options_parsed(void *parsed_result,
				    __rte_unused struct cmdline *cl,
				    __rte_unused void *data)
{
	struct mlx5_cmd_dump_rq_context_options *res = parsed_result;
	int ret;

	ret = rte_pmd_mlx5_rxq_dump_contexts(res->port_id, res->queue_id, res->file_name);

	switch (ret) {
	case 0:
		break;
	case -EINVAL:
		fprintf(stderr, "invalid queue index (%u), out of range\n",
			res->queue_id);
		break;
	case -ENODEV:
		fprintf(stderr, "invalid port_id %u\n", res->port_id);
		break;
	case -EIO:
		fprintf(stderr, "File Access Error (%s)\n", strerror(rte_errno));
		break;
	default:
		fprintf(stderr, "Unable to dump RQ/CQ HW Context (%s)\n", strerror(rte_errno));
	}
}

cmdline_parse_inst_t mlx5_cmd_dump_rq_context_options = {
	.f = mlx5_cmd_dump_rq_context_options_parsed,
	.data = NULL,
	.help_str = "mlx5 port <port_id> queue <queue_id> dump rq_context <file_name>",
	.tokens = {
		(void *)&mlx5_cmd_dump_rq_context_options_mlx5,
		(void *)&mlx5_cmd_dump_rq_context_options_port,
		(void *)&mlx5_cmd_dump_rq_context_options_port_id,
		(void *)&mlx5_cmd_dump_rq_context_options_queue,
		(void *)&mlx5_cmd_dump_rq_context_options_queue_id,
		(void *)&mlx5_cmd_dump_rq_context_options_dump,
		(void *)&mlx5_cmd_dump_rq_context_options_rq_context,
		(void *)&mlx5_cmd_dump_rq_context_options_file_name,
		NULL,
	}
};

static struct testpmd_driver_commands mlx5_driver_cmds = {
	.commands = {
		{
			.ctx = &mlx5_test_cmd_port_host_shaper,
			.help = "mlx5 set port (port_id) host_shaper avail_thresh_triggered (on|off)"
				"rate (rate_num):\n"
				"    Set HOST_SHAPER avail_thresh_triggered and rate with port_id\n\n",
		},
#ifndef RTE_EXEC_ENV_WINDOWS
		{
			.ctx = &mlx5_cmd_operate_attach_port,
			.help = "mlx5 port attach (ident) socket=(path)\n"
				"    Attach physical or virtual dev by pci address or virtual device name "
				"and add \"cmd_fd\" and \"pd_handle\" devargs before attaching\n\n",
		},
#endif
		{
			.ctx = &mlx5_cmd_map_ext_rxq,
			.help = "mlx5 port (port_id) ext_rxq map (sw_queue_id) (hw_queue_id)\n"
				"    Map HW queue index (32-bit) to ethdev"
				" queue index (16-bit) for external RxQ\n\n",
		},
		{
			.ctx = &mlx5_cmd_unmap_ext_rxq,
			.help = "mlx5 port (port_id) ext_rxq unmap (sw_queue_id)\n"
				"    Unmap external Rx queue ethdev index mapping\n\n",
		},
		{
			.ctx = &mlx5_cmd_set_flow_engine_mode,
			.help = "mlx5 set flow_engine (active|standby) [(flag)]\n"
				"    Set flow_engine to the specific mode with flag.\n\n"
		},
		{
			.ctx = &mlx5_cmd_set_tlv_option,
			.help = "mlx5 set tlv_option class (class_id) type "
				"(type_id) len (option_length) offset "
				"(sample_offset) sample_len (sample_length) "
				"class_mode (ignored|fixed|matchable) "
				"data (mask1) [(mask2) [...]]\n"
				"    Set single GENEVE TLV option inside global list "
				"using later by apply command\n\n",
		},
		{
			.ctx = &mlx5_cmd_list_tlv_options,
			.help = "mlx5 list tlv_options\n"
				"    Print all GENEVE TLV options which are configured "
				"so far by TLV option set command\n\n",
		},
		{
			.ctx = &mlx5_cmd_flush_tlv_options,
			.help = "mlx5 flush tlv_options [max (number options)]\n"
				"    Clear all GENEVE TLV options which are configured "
				"so far by TLV option set command\n\n",
		},
		{
			.ctx = &mlx5_cmd_apply_tlv_options,
			.help = "mlx5 port (port_id) apply tlv_options\n"
				"    Create GENEVE TLV parser using option list which is "
				"configured before by TLV option set command\n\n",
		},
		{
			.ctx = &mlx5_cmd_destroy_tlv_options,
			.help = "mlx5 port (port_id) destroy tlv_options\n"
				"    Destroy GENEVE TLV parser\n\n",
		},
		{
			.ctx = &mlx5_cmd_dump_sq_context_options,
			.help = "mlx5 port (port_id) queue (queue_id) dump sq_context (file_name)\n"
				"    Dump mlx5 SQ Context\n\n",
		},
		{
			.ctx = &mlx5_cmd_dump_rq_context_options,
			.help = "mlx5 port (port_id) queue (queue_id) dump rq_context (file_name)\n"
				"    Dump mlx5 RQ Context\n\n",
		},
		{
			.ctx = NULL,
		},
	}
};
TESTPMD_ADD_DRIVER_COMMANDS(mlx5_driver_cmds);
