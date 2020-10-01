/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>

#include "rte_swx_pipeline.h"

#define CHECK(condition, err_code)                                             \
do {                                                                           \
	if (!(condition))                                                      \
		return -(err_code);                                            \
} while (0)

#define CHECK_NAME(name, err_code)                                             \
	CHECK((name) && (name)[0], err_code)

/*
 * Struct.
 */
struct field {
	char name[RTE_SWX_NAME_SIZE];
	uint32_t n_bits;
	uint32_t offset;
};

struct struct_type {
	TAILQ_ENTRY(struct_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct field *fields;
	uint32_t n_fields;
	uint32_t n_bits;
};

TAILQ_HEAD(struct_type_tailq, struct_type);

/*
 * Input port.
 */
struct port_in_type {
	TAILQ_ENTRY(port_in_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct rte_swx_port_in_ops ops;
};

TAILQ_HEAD(port_in_type_tailq, port_in_type);

struct port_in {
	TAILQ_ENTRY(port_in) node;
	struct port_in_type *type;
	void *obj;
	uint32_t id;
};

TAILQ_HEAD(port_in_tailq, port_in);

struct port_in_runtime {
	rte_swx_port_in_pkt_rx_t pkt_rx;
	void *obj;
};

/*
 * Output port.
 */
struct port_out_type {
	TAILQ_ENTRY(port_out_type) node;
	char name[RTE_SWX_NAME_SIZE];
	struct rte_swx_port_out_ops ops;
};

TAILQ_HEAD(port_out_type_tailq, port_out_type);

struct port_out {
	TAILQ_ENTRY(port_out) node;
	struct port_out_type *type;
	void *obj;
	uint32_t id;
};

TAILQ_HEAD(port_out_tailq, port_out);

struct port_out_runtime {
	rte_swx_port_out_pkt_tx_t pkt_tx;
	rte_swx_port_out_flush_t flush;
	void *obj;
};

/*
 * Header.
 */
struct header {
	TAILQ_ENTRY(header) node;
	char name[RTE_SWX_NAME_SIZE];
	struct struct_type *st;
	uint32_t struct_id;
	uint32_t id;
};

TAILQ_HEAD(header_tailq, header);

struct header_runtime {
	uint8_t *ptr0;
};

struct header_out_runtime {
	uint8_t *ptr0;
	uint8_t *ptr;
	uint32_t n_bytes;
};

/*
 * Pipeline.
 */
struct thread {
	/* Structures. */
	uint8_t **structs;

	/* Packet headers. */
	struct header_runtime *headers; /* Extracted or generated headers. */
	struct header_out_runtime *headers_out; /* Emitted headers. */
	uint8_t *header_storage;
	uint8_t *header_out_storage;
	uint64_t valid_headers;
	uint32_t n_headers_out;

	/* Packet meta-data. */
	uint8_t *metadata;
};

#ifndef RTE_SWX_PIPELINE_THREADS_MAX
#define RTE_SWX_PIPELINE_THREADS_MAX 16
#endif

struct rte_swx_pipeline {
	struct struct_type_tailq struct_types;
	struct port_in_type_tailq port_in_types;
	struct port_in_tailq ports_in;
	struct port_out_type_tailq port_out_types;
	struct port_out_tailq ports_out;
	struct header_tailq headers;
	struct struct_type *metadata_st;
	uint32_t metadata_struct_id;

	struct port_in_runtime *in;
	struct port_out_runtime *out;
	struct thread threads[RTE_SWX_PIPELINE_THREADS_MAX];

	uint32_t n_structs;
	uint32_t n_ports_in;
	uint32_t n_ports_out;
	uint32_t n_headers;
	int build_done;
	int numa_node;
};

/*
 * Struct.
 */
static struct struct_type *
struct_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct struct_type *elem;

	TAILQ_FOREACH(elem, &p->struct_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_struct_type_register(struct rte_swx_pipeline *p,
				      const char *name,
				      struct rte_swx_field_params *fields,
				      uint32_t n_fields)
{
	struct struct_type *st;
	uint32_t i;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(fields, EINVAL);
	CHECK(n_fields, EINVAL);

	for (i = 0; i < n_fields; i++) {
		struct rte_swx_field_params *f = &fields[i];
		uint32_t j;

		CHECK_NAME(f->name, EINVAL);
		CHECK(f->n_bits, EINVAL);
		CHECK(f->n_bits <= 64, EINVAL);
		CHECK((f->n_bits & 7) == 0, EINVAL);

		for (j = 0; j < i; j++) {
			struct rte_swx_field_params *f_prev = &fields[j];

			CHECK(strcmp(f->name, f_prev->name), EINVAL);
		}
	}

	CHECK(!struct_type_find(p, name), EEXIST);

	/* Node allocation. */
	st = calloc(1, sizeof(struct struct_type));
	CHECK(st, ENOMEM);

	st->fields = calloc(n_fields, sizeof(struct field));
	if (!st->fields) {
		free(st);
		CHECK(0, ENOMEM);
	}

	/* Node initialization. */
	strcpy(st->name, name);
	for (i = 0; i < n_fields; i++) {
		struct field *dst = &st->fields[i];
		struct rte_swx_field_params *src = &fields[i];

		strcpy(dst->name, src->name);
		dst->n_bits = src->n_bits;
		dst->offset = st->n_bits;

		st->n_bits += src->n_bits;
	}
	st->n_fields = n_fields;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->struct_types, st, node);

	return 0;
}

static int
struct_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		t->structs = calloc(p->n_structs, sizeof(uint8_t *));
		CHECK(t->structs, ENOMEM);
	}

	return 0;
}

static void
struct_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		free(t->structs);
		t->structs = NULL;
	}
}

static void
struct_free(struct rte_swx_pipeline *p)
{
	struct_build_free(p);

	/* Struct types. */
	for ( ; ; ) {
		struct struct_type *elem;

		elem = TAILQ_FIRST(&p->struct_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->struct_types, elem, node);
		free(elem->fields);
		free(elem);
	}
}

/*
 * Input port.
 */
static struct port_in_type *
port_in_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct port_in_type *elem;

	if (!name)
		return NULL;

	TAILQ_FOREACH(elem, &p->port_in_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_port_in_type_register(struct rte_swx_pipeline *p,
				       const char *name,
				       struct rte_swx_port_in_ops *ops)
{
	struct port_in_type *elem;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(ops, EINVAL);
	CHECK(ops->create, EINVAL);
	CHECK(ops->free, EINVAL);
	CHECK(ops->pkt_rx, EINVAL);
	CHECK(ops->stats_read, EINVAL);

	CHECK(!port_in_type_find(p, name), EEXIST);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct port_in_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	memcpy(&elem->ops, ops, sizeof(*ops));

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->port_in_types, elem, node);

	return 0;
}

static struct port_in *
port_in_find(struct rte_swx_pipeline *p, uint32_t port_id)
{
	struct port_in *port;

	TAILQ_FOREACH(port, &p->ports_in, node)
		if (port->id == port_id)
			return port;

	return NULL;
}

int
rte_swx_pipeline_port_in_config(struct rte_swx_pipeline *p,
				uint32_t port_id,
				const char *port_type_name,
				void *args)
{
	struct port_in_type *type = NULL;
	struct port_in *port = NULL;
	void *obj = NULL;

	CHECK(p, EINVAL);

	CHECK(!port_in_find(p, port_id), EINVAL);

	CHECK_NAME(port_type_name, EINVAL);
	type = port_in_type_find(p, port_type_name);
	CHECK(type, EINVAL);

	obj = type->ops.create(args);
	CHECK(obj, ENODEV);

	/* Node allocation. */
	port = calloc(1, sizeof(struct port_in));
	CHECK(port, ENOMEM);

	/* Node initialization. */
	port->type = type;
	port->obj = obj;
	port->id = port_id;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->ports_in, port, node);
	if (p->n_ports_in < port_id + 1)
		p->n_ports_in = port_id + 1;

	return 0;
}

static int
port_in_build(struct rte_swx_pipeline *p)
{
	struct port_in *port;
	uint32_t i;

	CHECK(p->n_ports_in, EINVAL);
	CHECK(rte_is_power_of_2(p->n_ports_in), EINVAL);

	for (i = 0; i < p->n_ports_in; i++)
		CHECK(port_in_find(p, i), EINVAL);

	p->in = calloc(p->n_ports_in, sizeof(struct port_in_runtime));
	CHECK(p->in, ENOMEM);

	TAILQ_FOREACH(port, &p->ports_in, node) {
		struct port_in_runtime *in = &p->in[port->id];

		in->pkt_rx = port->type->ops.pkt_rx;
		in->obj = port->obj;
	}

	return 0;
}

static void
port_in_build_free(struct rte_swx_pipeline *p)
{
	free(p->in);
	p->in = NULL;
}

static void
port_in_free(struct rte_swx_pipeline *p)
{
	port_in_build_free(p);

	/* Input ports. */
	for ( ; ; ) {
		struct port_in *port;

		port = TAILQ_FIRST(&p->ports_in);
		if (!port)
			break;

		TAILQ_REMOVE(&p->ports_in, port, node);
		port->type->ops.free(port->obj);
		free(port);
	}

	/* Input port types. */
	for ( ; ; ) {
		struct port_in_type *elem;

		elem = TAILQ_FIRST(&p->port_in_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->port_in_types, elem, node);
		free(elem);
	}
}

/*
 * Output port.
 */
static struct port_out_type *
port_out_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct port_out_type *elem;

	if (!name)
		return NULL;

	TAILQ_FOREACH(elem, &p->port_out_types, node)
		if (!strcmp(elem->name, name))
			return elem;

	return NULL;
}

int
rte_swx_pipeline_port_out_type_register(struct rte_swx_pipeline *p,
					const char *name,
					struct rte_swx_port_out_ops *ops)
{
	struct port_out_type *elem;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(ops, EINVAL);
	CHECK(ops->create, EINVAL);
	CHECK(ops->free, EINVAL);
	CHECK(ops->pkt_tx, EINVAL);
	CHECK(ops->stats_read, EINVAL);

	CHECK(!port_out_type_find(p, name), EEXIST);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct port_out_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	memcpy(&elem->ops, ops, sizeof(*ops));

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->port_out_types, elem, node);

	return 0;
}

static struct port_out *
port_out_find(struct rte_swx_pipeline *p, uint32_t port_id)
{
	struct port_out *port;

	TAILQ_FOREACH(port, &p->ports_out, node)
		if (port->id == port_id)
			return port;

	return NULL;
}

int
rte_swx_pipeline_port_out_config(struct rte_swx_pipeline *p,
				 uint32_t port_id,
				 const char *port_type_name,
				 void *args)
{
	struct port_out_type *type = NULL;
	struct port_out *port = NULL;
	void *obj = NULL;

	CHECK(p, EINVAL);

	CHECK(!port_out_find(p, port_id), EINVAL);

	CHECK_NAME(port_type_name, EINVAL);
	type = port_out_type_find(p, port_type_name);
	CHECK(type, EINVAL);

	obj = type->ops.create(args);
	CHECK(obj, ENODEV);

	/* Node allocation. */
	port = calloc(1, sizeof(struct port_out));
	CHECK(port, ENOMEM);

	/* Node initialization. */
	port->type = type;
	port->obj = obj;
	port->id = port_id;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->ports_out, port, node);
	if (p->n_ports_out < port_id + 1)
		p->n_ports_out = port_id + 1;

	return 0;
}

static int
port_out_build(struct rte_swx_pipeline *p)
{
	struct port_out *port;
	uint32_t i;

	CHECK(p->n_ports_out, EINVAL);

	for (i = 0; i < p->n_ports_out; i++)
		CHECK(port_out_find(p, i), EINVAL);

	p->out = calloc(p->n_ports_out, sizeof(struct port_out_runtime));
	CHECK(p->out, ENOMEM);

	TAILQ_FOREACH(port, &p->ports_out, node) {
		struct port_out_runtime *out = &p->out[port->id];

		out->pkt_tx = port->type->ops.pkt_tx;
		out->flush = port->type->ops.flush;
		out->obj = port->obj;
	}

	return 0;
}

static void
port_out_build_free(struct rte_swx_pipeline *p)
{
	free(p->out);
	p->out = NULL;
}

static void
port_out_free(struct rte_swx_pipeline *p)
{
	port_out_build_free(p);

	/* Output ports. */
	for ( ; ; ) {
		struct port_out *port;

		port = TAILQ_FIRST(&p->ports_out);
		if (!port)
			break;

		TAILQ_REMOVE(&p->ports_out, port, node);
		port->type->ops.free(port->obj);
		free(port);
	}

	/* Output port types. */
	for ( ; ; ) {
		struct port_out_type *elem;

		elem = TAILQ_FIRST(&p->port_out_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->port_out_types, elem, node);
		free(elem);
	}
}

/*
 * Header.
 */
static struct header *
header_find(struct rte_swx_pipeline *p, const char *name)
{
	struct header *elem;

	TAILQ_FOREACH(elem, &p->headers, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_packet_header_register(struct rte_swx_pipeline *p,
					const char *name,
					const char *struct_type_name)
{
	struct struct_type *st;
	struct header *h;
	size_t n_headers_max;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK_NAME(struct_type_name, EINVAL);

	CHECK(!header_find(p, name), EEXIST);

	st = struct_type_find(p, struct_type_name);
	CHECK(st, EINVAL);

	n_headers_max = RTE_SIZEOF_FIELD(struct thread, valid_headers) * 8;
	CHECK(p->n_headers < n_headers_max, ENOSPC);

	/* Node allocation. */
	h = calloc(1, sizeof(struct header));
	CHECK(h, ENOMEM);

	/* Node initialization. */
	strcpy(h->name, name);
	h->st = st;
	h->struct_id = p->n_structs;
	h->id = p->n_headers;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->headers, h, node);
	p->n_headers++;
	p->n_structs++;

	return 0;
}

static int
header_build(struct rte_swx_pipeline *p)
{
	struct header *h;
	uint32_t n_bytes = 0, i;

	TAILQ_FOREACH(h, &p->headers, node) {
		n_bytes += h->st->n_bits / 8;
	}

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t offset = 0;

		t->headers = calloc(p->n_headers,
				    sizeof(struct header_runtime));
		CHECK(t->headers, ENOMEM);

		t->headers_out = calloc(p->n_headers,
					sizeof(struct header_out_runtime));
		CHECK(t->headers_out, ENOMEM);

		t->header_storage = calloc(1, n_bytes);
		CHECK(t->header_storage, ENOMEM);

		t->header_out_storage = calloc(1, n_bytes);
		CHECK(t->header_out_storage, ENOMEM);

		TAILQ_FOREACH(h, &p->headers, node) {
			uint8_t *header_storage;

			header_storage = &t->header_storage[offset];
			offset += h->st->n_bits / 8;

			t->headers[h->id].ptr0 = header_storage;
			t->structs[h->struct_id] = header_storage;
		}
	}

	return 0;
}

static void
header_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		free(t->headers_out);
		t->headers_out = NULL;

		free(t->headers);
		t->headers = NULL;

		free(t->header_out_storage);
		t->header_out_storage = NULL;

		free(t->header_storage);
		t->header_storage = NULL;
	}
}

static void
header_free(struct rte_swx_pipeline *p)
{
	header_build_free(p);

	for ( ; ; ) {
		struct header *elem;

		elem = TAILQ_FIRST(&p->headers);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->headers, elem, node);
		free(elem);
	}
}

/*
 * Meta-data.
 */
int
rte_swx_pipeline_packet_metadata_register(struct rte_swx_pipeline *p,
					  const char *struct_type_name)
{
	struct struct_type *st = NULL;

	CHECK(p, EINVAL);

	CHECK_NAME(struct_type_name, EINVAL);
	st  = struct_type_find(p, struct_type_name);
	CHECK(st, EINVAL);
	CHECK(!p->metadata_st, EINVAL);

	p->metadata_st = st;
	p->metadata_struct_id = p->n_structs;

	p->n_structs++;

	return 0;
}

static int
metadata_build(struct rte_swx_pipeline *p)
{
	uint32_t n_bytes = p->metadata_st->n_bits / 8;
	uint32_t i;

	/* Thread-level initialization. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint8_t *metadata;

		metadata = calloc(1, n_bytes);
		CHECK(metadata, ENOMEM);

		t->metadata = metadata;
		t->structs[p->metadata_struct_id] = metadata;
	}

	return 0;
}

static void
metadata_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		free(t->metadata);
		t->metadata = NULL;
	}
}

static void
metadata_free(struct rte_swx_pipeline *p)
{
	metadata_build_free(p);
}

/*
 * Pipeline.
 */
int
rte_swx_pipeline_config(struct rte_swx_pipeline **p, int numa_node)
{
	struct rte_swx_pipeline *pipeline;

	/* Check input parameters. */
	CHECK(p, EINVAL);

	/* Memory allocation. */
	pipeline = calloc(1, sizeof(struct rte_swx_pipeline));
	CHECK(pipeline, ENOMEM);

	/* Initialization. */
	TAILQ_INIT(&pipeline->struct_types);
	TAILQ_INIT(&pipeline->port_in_types);
	TAILQ_INIT(&pipeline->ports_in);
	TAILQ_INIT(&pipeline->port_out_types);
	TAILQ_INIT(&pipeline->ports_out);
	TAILQ_INIT(&pipeline->headers);

	pipeline->n_structs = 1; /* Struct 0 is reserved for action_data. */
	pipeline->numa_node = numa_node;

	*p = pipeline;
	return 0;
}

void
rte_swx_pipeline_free(struct rte_swx_pipeline *p)
{
	if (!p)
		return;

	metadata_free(p);
	header_free(p);
	port_out_free(p);
	port_in_free(p);
	struct_free(p);

	free(p);
}

int
rte_swx_pipeline_build(struct rte_swx_pipeline *p)
{
	int status;

	CHECK(p, EINVAL);
	CHECK(p->build_done == 0, EEXIST);

	status = port_in_build(p);
	if (status)
		goto error;

	status = port_out_build(p);
	if (status)
		goto error;

	status = struct_build(p);
	if (status)
		goto error;

	status = header_build(p);
	if (status)
		goto error;

	status = metadata_build(p);
	if (status)
		goto error;

	p->build_done = 1;
	return 0;

error:
	metadata_build_free(p);
	header_build_free(p);
	port_out_build_free(p);
	port_in_build_free(p);
	struct_build_free(p);

	return status;
}
