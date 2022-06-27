/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021, NVIDIA CORPORATION & AFFILIATES.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include "jsonrpc-client.h"

static void jsonrpc_client_build_request(const char *method, cJSON *parameter,
					 char **result)
{
	cJSON *request = cJSON_CreateObject();

	cJSON_AddStringToObject(request, KEY_PROTOCOL_VERSION, "2.0");
	cJSON_AddStringToObject(request, KEY_PROCEDURE_NAME, method);
	if (parameter)
		cJSON_AddItemToObject(request, KEY_PARAMETER, parameter);
	cJSON_AddNumberToObject(request, KEY_ID, 1);
	*result = cJSON_PrintUnformatted(request);
	cJSON_Delete(request);
}

static int jsonrpc_client_connect(const char *ip, int port)
{
	struct sockaddr_in address;
	int socket_fd;
	int err;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		err = errno;
		switch (err) {
		case EACCES:
		case EAFNOSUPPORT:
		case EINVAL:
		case EMFILE:
		case ENOBUFS:
		case ENOMEM:
		case EPROTONOSUPPORT:
			fprintf(stderr, "Failed to create socket: %s\n",
			strerror(err));
			break;
		default:
			fprintf(stderr, "Failed to create socket");
		}
		perror("Failed to open socket");
		return -err;
	}
	memset(&address, 0, sizeof(struct sockaddr_in));

	address.sin_family = AF_INET;
	inet_aton(ip, &(address.sin_addr));
	address.sin_port = htons(port);

	if (connect(socket_fd, (struct sockaddr *)&address,
	    sizeof(struct sockaddr_in)) != 0) {
		err = errno;
		switch (err) {
		case EACCES:
		case EPERM:
		case EADDRINUSE:
		case EAFNOSUPPORT:
		case EAGAIN:
		case EALREADY:
		case EBADF:
		case ECONNREFUSED:
		case EFAULT:
		case EINPROGRESS:
		case EINTR:
		case EISCONN:
		case ENETUNREACH:
		case ENOTSOCK:
		case ETIMEDOUT:
			fprintf(stderr, "Failed to connect: %s\n",
			strerror(err));
			break;
		default:
			fprintf(stderr, "Failed to connect");
		}
		close(socket_fd);
		perror("Failed to connect socket");
		return -err;
	}

	return socket_fd;
}

static char *recv_reply_timeout(int sock_fd, int timeout)
{
#define CHUNK_SIZE 4096
	int size_recv = 0, total_size = 0;
	struct timeval begin, now;
	char *reply;
	double timediff;

	fcntl(sock_fd, F_SETFL, O_NONBLOCK);

	gettimeofday(&begin, NULL);

	reply = calloc(CHUNK_SIZE, sizeof(char));
	if (!reply) {
		perror("Failed to allocate memory\n");
		return NULL;
	}

	while (1) {
		gettimeofday(&now, NULL);
		timediff = (now.tv_sec - begin.tv_sec) +
			    1e-6 * (now.tv_usec - begin.tv_usec);

		if (timediff > timeout) {
			if (total_size > 0 && cJSON_Parse(reply))
				return reply;
			perror("Failed to get a valid json reply, timeout\n");
			return NULL;
		}

		if (total_size > 0 && size_recv > 0) {
			reply = realloc(reply, total_size + CHUNK_SIZE);
			if (!reply) {
				perror("Failed to allocate memory\n");
				return NULL;
			}
			memset(reply + total_size, 0, CHUNK_SIZE);
		}

		size_recv = recv(sock_fd, reply + total_size, CHUNK_SIZE, 0);
		if (size_recv < 0) {
			usleep(100000);
			continue;
		}

		if (cJSON_Parse(reply))
			break;

		if (size_recv != CHUNK_SIZE) {
			perror("Failed to recv json reply\n");
			return NULL;
		}

		total_size += CHUNK_SIZE;
		gettimeofday(&begin, NULL);
	}

	return reply;
}

static int send_request(int sock_fd, const char *req)
{
	if (send(sock_fd, req, strlen(req), 0) < 0) {
		perror("Failed to send the request");
		return -errno;
	}

	return 0;
}

static int jsonrpc_client_send_rpc_message(struct jsonrpc_client *client,
					   const char *message, char **result)
{
	int sock_fd;
	int ret;

	sock_fd = jsonrpc_client_connect(client->ip, client->port);
	if (sock_fd < 0)
		return sock_fd;

	ret = send_request(sock_fd, message);
	if (ret)
		goto out;

	*result = recv_reply_timeout(sock_fd, 120);
	if (!*result)
		ret = -1;

out:
	close(sock_fd);
	return ret;
}

static void jsonrpc_client_handle_response(cJSON *response, cJSON **result)
{
	*result = cJSON_GetObjectItem(response, KEY_RESULT);
}

static void _jsonrpc_client_call_method(struct jsonrpc_client *client,
					const char *name, cJSON *parameter,
					cJSON **result)
{
	char *req, *resp = NULL;
	int ret;

	jsonrpc_client_build_request(name, parameter, &req);

	ret = jsonrpc_client_send_rpc_message(client, req, &resp);
	if (ret)
		goto err;

	jsonrpc_client_handle_response(cJSON_Parse(resp), result);

	free(resp);
err:
	free(req);
}

cJSON *jsonrpc_client_call_method(struct jsonrpc_client *client, const char *name,
				  char *param)
{
	cJSON *result = NULL;
	cJSON *parameter;

	parameter = cJSON_Parse(param);
	if (!parameter)
		return cJSON_CreateString("Invalid json parameters provided");

	_jsonrpc_client_call_method(client, name, parameter, &result);
	if (result)
		result->next = NULL;

	return result;
}
