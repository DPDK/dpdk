/*
 * jsonrpc-c.h
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
 */

#ifndef JSONRPCC_H_
#define JSONRPCC_H_

#include "cJSON.h"
#include <ev.h>

/*
 *
 * http://www.jsonrpc.org/specification
 *
 * code	message	meaning
 * -32700	Parse error	Invalid JSON was received by the server.
 * An error occurred on the server while parsing the JSON text.
 * -32600	Invalid Request	The JSON sent is not a valid Request object.
 * -32601	Method not found	The method does not exist / is not available.
 * -32602	Invalid params	Invalid method parameter(s).
 * -32603	Internal error	Internal JSON-RPC error.
 * -32000 to -32099	Server error	Reserved for implementation-defined server-errors.
 */

#define JRPC_PARSE_ERROR -32700
#define JRPC_INVALID_REQUEST -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS -32603
#define JRPC_INTERNAL_ERROR -32693

typedef struct {
	void *data;
	int error_code;
	char *error_message;
} jrpc_context;

typedef cJSON* (*jrpc_function)(jrpc_context *context, cJSON *params, cJSON *id);

struct jrpc_procedure {
	char *name;
	jrpc_function function;
	void *data;
};

struct jrpc_server {
	int port_number;
	struct ev_loop *loop;
	ev_io listen_watcher;
	int procedure_count;
	struct jrpc_procedure *procedures;
	int debug_level;
};

struct jrpc_connection {
	struct ev_io io;
	int fd;
	int pos;
	unsigned int buffer_size;
	char *buffer;
	int debug_level;
};

int
jrpc_server_init(struct jrpc_server *server, int port_number);

int
jrpc_server_init_with_ev_loop(struct jrpc_server *server,
	int port_number, struct ev_loop *loop);

void
jrpc_server_run(struct jrpc_server *server);

int
jrpc_server_stop(struct jrpc_server *server);

void
jrpc_server_destroy(struct jrpc_server *server);

int
jrpc_register_procedure(struct jrpc_server *server,
		jrpc_function function_pointer, const char *name, void *data);

int
jrpc_deregister_procedure(struct jrpc_server *server, char *name);
#endif
