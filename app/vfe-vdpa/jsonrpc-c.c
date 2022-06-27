/*
 * jsonrpc-c.c
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
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

#include "jsonrpc-c.h"

static int __jrpc_server_start(struct jrpc_server *server);
static void jrpc_procedure_destroy(struct jrpc_procedure *procedure);

/* get sockaddr, IPv4 or IPv6: */
static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa)->sin_addr);
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

static int send_response(struct jrpc_connection *conn, char *response)
{
	int fd = conn->fd;
	int ret;
	if (conn->debug_level > 1)
		printf("JSON Response:\n%s\n", response);
	ret = write(fd, response, strlen(response));
	if (ret == -1)
		return ret;
	ret = write(fd, "\n", 1);
	if (ret == -1)
		return ret;
	return 0;
}

static int send_error(struct jrpc_connection *conn, int code, char *message,
		cJSON *id)
{
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();
	cJSON *error_root = cJSON_CreateObject();
	cJSON_AddNumberToObject(error_root, "code", code);
	cJSON_AddStringToObject(error_root, "message", message);
	cJSON_AddItemToObject(result_root, "error", error_root);
	cJSON_AddItemToObject(result_root, "id", id);
	char *str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	free(message);
	return return_value;
}

static int send_result(struct jrpc_connection *conn, cJSON *result,
		cJSON *id)
{
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();
	if (result)
		cJSON_AddItemToObject(result_root, "result", result);
	cJSON_AddItemToObject(result_root, "id", id);

	char *str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	return return_value;
}


static int invoke_procedure(struct jrpc_server *server,
		struct jrpc_connection *conn, char *name, cJSON *params, cJSON *id)
{
	cJSON *returned = NULL;
	int procedure_found = 0;
	jrpc_context ctx;
	ctx.error_code = 0;
	ctx.error_message = NULL;
	int i = server->procedure_count;
	while (i--) {
		if (!strcmp(server->procedures[i].name, name)) {
			procedure_found = 1;
			ctx.data = server->procedures[i].data;
			returned = server->procedures[i].function(&ctx, params, id);
			break;
		}
	}
	if (!procedure_found) {
		return send_error(conn, JRPC_METHOD_NOT_FOUND,
				strdup("Method not found."), id);
	} else {
		if (ctx.error_code)
			return send_error(conn, ctx.error_code, ctx.error_message, id);
		else
			return send_result(conn, returned, id);
	}
}

static int eval_request(struct jrpc_server *server,
		struct jrpc_connection *conn, cJSON *root)
{
	cJSON *method, *params, *id;
	method = cJSON_GetObjectItem(root, "method");
	if (method != NULL && method->type == cJSON_String) {
		params = cJSON_GetObjectItem(root, "params");
		if (params == NULL || params->type == cJSON_Array
		|| params->type == cJSON_Object) {
			id = cJSON_GetObjectItem(root, "id");
			if (id == NULL || id->type == cJSON_String
				|| id->type == cJSON_Number) {
			/* We have to copy ID because using it on the reply and
			 * deleting the response Object will also delete ID
			 */
				cJSON *id_copy = NULL;
				if (id != NULL)
					id_copy =
					(id->type == cJSON_String) ? cJSON_CreateString(
						id->valuestring) :
						cJSON_CreateNumber(id->valueint);
				if (server->debug_level)
					printf("Method Invoked: %s\n", method->valuestring);
				return invoke_procedure(server, conn, method->valuestring,
						params, id_copy);
			}
		}
	}
	send_error(conn, JRPC_INVALID_REQUEST,
			strdup("The JSON sent is not a valid Request object."), NULL);
	return -1;
}

static void close_connection(struct ev_loop *loop, ev_io *w)
{
	ev_io_stop(loop, w);
	close(((struct jrpc_connection *)w)->fd);
	free(((struct jrpc_connection *)w)->buffer);
	free(((struct jrpc_connection *)w));
}

static void connection_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	struct jrpc_connection *conn;
	struct jrpc_server *server = (struct jrpc_server *) w->data;
	size_t bytes_read = 0;
	/* get our 'subclassed' event watcher */
	conn = (struct jrpc_connection *) w;
	int fd = conn->fd;
	if (conn->pos == (int)(conn->buffer_size - 1)) {
		char *new_buffer = realloc(conn->buffer, conn->buffer_size *= 2);
		if (new_buffer == NULL) {
			perror("Memory error");
			return close_connection(loop, w);
		}
		conn->buffer = new_buffer;
		memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos);
	}
	/* can not fill the entire buffer, string must be NULL terminated */
	int max_read_size = conn->buffer_size - conn->pos - 1;
	bytes_read = read(fd, conn->buffer + conn->pos, max_read_size);
	if ((int)bytes_read == (int)-1) {
		perror("read");
		return close_connection(loop, w);
	}
	if (!bytes_read) {
		/* client closed the sending half of the connection */
		if (server->debug_level)
			printf("Client closed connection revents %d.\n", revents);
		return close_connection(loop, w);
	}
	cJSON *root;
	char *end_ptr = NULL;
	conn->pos += bytes_read;

	root = cJSON_Parse_Stream(conn->buffer, &end_ptr);
	if (root != NULL) {
		if (server->debug_level > 1) {
			char *str_result = cJSON_Print(root);
			printf("Valid JSON Received:\n%s\n", str_result);
			free(str_result);
		}

		if (root->type == cJSON_Object)
			eval_request(server, conn, root);
		/* shift processed request, discarding it */
		memmove(conn->buffer, end_ptr, strlen(end_ptr) + 2);

		conn->pos = strlen(end_ptr);
		memset(conn->buffer + conn->pos, 0,
					conn->buffer_size - conn->pos - 1);

		cJSON_Delete(root);
	} else {
		/* did we parse the all buffer? If so, just wait for more.
		 * else there was an error before the buffer's end
		 */
		if (end_ptr != (conn->buffer + conn->pos)) {
			if (server->debug_level) {
				printf("INVALID JSON Received:\n---\n%s\n---\n",
					conn->buffer);
			}
			send_error(conn, JRPC_PARSE_ERROR,
				strdup(
				"Parse error. Invalid JSON was received by the server."),
				NULL);
			return close_connection(loop, w);
		}
	}
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	char s[INET6_ADDRSTRLEN];
	struct jrpc_connection *connection_watcher;
	connection_watcher = malloc(sizeof(struct jrpc_connection));
	struct sockaddr_storage their_addr; /* connector's address information */
	socklen_t sin_size;
	sin_size = sizeof(their_addr);
	connection_watcher->fd = accept(w->fd, (struct sockaddr *) &their_addr,
			&sin_size);
	if (connection_watcher->fd == -1) {
		perror("accept");
		free(connection_watcher);
	} else {
		if (((struct jrpc_server *) w->data)->debug_level) {
			inet_ntop(their_addr.ss_family,
					get_in_addr((struct sockaddr *) &their_addr), s, sizeof(s));
			printf("server: got connection from %s revents %d\n", s, revents);
		}
		ev_io_init(&connection_watcher->io, connection_cb,
				connection_watcher->fd, EV_READ);
		/* copy pointer to struct jrpc_server */
		connection_watcher->io.data = w->data;
		connection_watcher->buffer_size = 1500;
		connection_watcher->buffer = malloc(1500);
		memset(connection_watcher->buffer, 0, 1500);
		connection_watcher->pos = 0;
		/* copy debug_level, struct jrpc_connection has no pointer to struct jrpc_server */
		connection_watcher->debug_level =
				((struct jrpc_server *) w->data)->debug_level;
		ev_io_start(loop, &connection_watcher->io);
	}
}

int jrpc_server_init(struct jrpc_server *server, int port_number)
{
	struct ev_loop *loop = ev_loop_new(0);
	return jrpc_server_init_with_ev_loop(server, port_number, loop);
}

int jrpc_server_init_with_ev_loop(struct jrpc_server *server,
	int port_number, struct ev_loop *loop)
{
	memset(server, 0, sizeof(struct jrpc_server));
	server->loop = loop;
	server->port_number = port_number;
	char *debug_level_env = getenv("JRPC_DEBUG");
	if (debug_level_env == NULL) {
		server->debug_level = 0;
	} else {
		server->debug_level = strtol(debug_level_env, NULL, 10);
		printf("JSONRPC-C Debug level %d\n", server->debug_level);
	}
	return __jrpc_server_start(server);
}

static int __jrpc_server_start(struct jrpc_server *server)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in sockaddr;
	unsigned int len;
	int yes = 1;
	int rv;
	char PORT[6];
	sprintf(PORT, "%d", server->port_number);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; /* use my IP */

	rv = getaddrinfo("localhost", PORT, &hints, &servinfo);
	if (rv) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	/* loop through all the results and bind to the first we can */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		len = sizeof(sockaddr);
		if (getsockname(sockfd, (struct sockaddr *) &sockaddr, &len) == -1) {
			close(sockfd);
			perror("server: getsockname");
			continue;
		}
		server->port_number = ntohs(sockaddr.sin_port);

		printf("RPC connected to %s\n",
		       inet_ntoa(sockaddr.sin_addr));
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); /* all done with this structure */

	if (listen(sockfd, 5) == -1) {
		perror("listen");
		exit(1);
	}
	if (server->debug_level)
		printf("server: waiting for connections...\n");

	ev_io_init(&server->listen_watcher, accept_cb, sockfd, EV_READ);
	server->listen_watcher.data = server;
	ev_io_start(server->loop, &server->listen_watcher);
	return 0;
}

/* Make the code work with both the old (ev_loop/ev_unloop)
 * and new (ev_run/ev_break) versions of libev.
 */
#ifdef EVUNLOOP_ALL
  #define EV_RUN ev_loop
  #define EV_BREAK ev_unloop
  #define EVBREAK_ALL EVUNLOOP_ALL
#else
  #define EV_RUN ev_run
  #define EV_BREAK ev_break
#endif

void jrpc_server_run(struct jrpc_server *server)
{
	EV_RUN(server->loop, 0);
}

int jrpc_server_stop(struct jrpc_server *server)
{
	int sockfd = server->listen_watcher.fd;
	EV_BREAK(server->loop, EVBREAK_ALL);
	close(sockfd);
	ev_loop_destroy(server->loop);
	return 0;
}

void jrpc_server_destroy(struct jrpc_server *server)
{
	/* Don't destroy server */
	int i;
	for (i = 0; i < server->procedure_count; i++)
		jrpc_procedure_destroy(&(server->procedures[i]));
	free(server->procedures);
}

static void jrpc_procedure_destroy(struct jrpc_procedure *procedure)
{
	if (procedure->name) {
		free(procedure->name);
		procedure->name = NULL;
	}
	if (procedure->data) {
		free(procedure->data);
		procedure->data = NULL;
	}
}

int jrpc_register_procedure(struct jrpc_server *server,
		jrpc_function function_pointer, const char *name, void *data)
{
	int i = server->procedure_count++;
	if (!server->procedures) {
		server->procedures = malloc(sizeof(struct jrpc_procedure));
	} else {
		struct jrpc_procedure *ptr = realloc(server->procedures,
				sizeof(struct jrpc_procedure) * server->procedure_count);
		if (!ptr)
			return -1;
		server->procedures = ptr;

	}
	server->procedures[i].name = strdup(name);
	if (server->procedures[i].name == NULL)
		return -1;
	server->procedures[i].function = function_pointer;
	server->procedures[i].data = data;
	return 0;
}

int jrpc_deregister_procedure(struct jrpc_server *server, char *name)
{
	/* Search the procedure to deregister */
	int i;
	int found = 0;
	if (server->procedures) {
		for (i = 0; i < server->procedure_count; i++) {
			if (found) {
				server->procedures[i-1] = server->procedures[i];
			} else if (!strcmp(name, server->procedures[i].name)) {
				found = 1;
				jrpc_procedure_destroy(&(server->procedures[i]));
			}
		}
		if (found) {
			server->procedure_count--;
			if (server->procedure_count) {
				struct jrpc_procedure *ptr = realloc(server->procedures,
					sizeof(struct jrpc_procedure) * server->procedure_count);
				if (!ptr) {
					perror("realloc");
					return -1;
				}
				server->procedures = ptr;
			} else {
				server->procedures = NULL;
			}
		}
	} else {
		fprintf(stderr, "server : procedure '%s' not found\n", name);
		return -1;
	}
	return 0;
}
