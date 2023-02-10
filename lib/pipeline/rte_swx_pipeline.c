/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <dlfcn.h>

#include <rte_tailq.h>
#include <rte_eal_memconfig.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

#include <rte_swx_port_ethdev.h>
#include <rte_swx_port_fd.h>
#include <rte_swx_port_ring.h>
#include "rte_swx_port_source_sink.h"

#include <rte_swx_table_em.h>
#include <rte_swx_table_wm.h>

#include "rte_swx_pipeline_internal.h"
#include "rte_swx_pipeline_spec.h"

#define CHECK(condition, err_code)                                             \
do {                                                                           \
	if (!(condition))                                                      \
		return -(err_code);                                            \
} while (0)

#define CHECK_NAME(name, err_code)                                             \
	CHECK((name) &&                                                        \
	      (name)[0] &&                                                     \
	      (strnlen((name), RTE_SWX_NAME_SIZE) < RTE_SWX_NAME_SIZE),        \
	      err_code)

#define CHECK_INSTRUCTION(instr, err_code)                                     \
	CHECK((instr) &&                                                       \
	      (instr)[0] &&                                                    \
	      (strnlen((instr), RTE_SWX_INSTRUCTION_SIZE) <                    \
	       RTE_SWX_INSTRUCTION_SIZE),                                      \
	      err_code)

/*
 * Environment.
 */
#ifndef RTE_SWX_PIPELINE_HUGE_PAGES_DISABLE

#include <rte_malloc.h>

static void *
env_malloc(size_t size, size_t alignment, int numa_node)
{
	return rte_zmalloc_socket(NULL, size, alignment, numa_node);
}

static void
env_free(void *start, size_t size __rte_unused)
{
	rte_free(start);
}

#else

#include <numa.h>

static void *
env_malloc(size_t size, size_t alignment __rte_unused, int numa_node)
{
	void *start;

	if (numa_available() == -1)
		return NULL;

	start = numa_alloc_onnode(size, numa_node);
	if (!start)
		return NULL;

	memset(start, 0, size);
	return start;
}

static void
env_free(void *start, size_t size)
{
	if (numa_available() == -1)
		return;

	numa_free(start, size);
}

#endif

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

static struct field *
struct_type_field_find(struct struct_type *st, const char *name)
{
	uint32_t i;

	for (i = 0; i < st->n_fields; i++) {
		struct field *f = &st->fields[i];

		if (strcmp(f->name, name) == 0)
			return f;
	}

	return NULL;
}

int
rte_swx_pipeline_struct_type_register(struct rte_swx_pipeline *p,
				      const char *name,
				      struct rte_swx_field_params *fields,
				      uint32_t n_fields,
				      int last_field_has_variable_size)
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
		int var_size = ((i == n_fields - 1) && last_field_has_variable_size) ? 1 : 0;

		strcpy(dst->name, src->name);
		dst->n_bits = src->n_bits;
		dst->offset = st->n_bits;
		dst->var_size = var_size;

		st->n_bits += src->n_bits;
		st->n_bits_min += var_size ? 0 : src->n_bits;
	}
	st->n_fields = n_fields;
	st->var_size = last_field_has_variable_size;

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
	CHECK(ops->pkt_fast_clone_tx, EINVAL);
	CHECK(ops->pkt_clone_tx, EINVAL);
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
		out->pkt_fast_clone_tx = port->type->ops.pkt_fast_clone_tx;
		out->pkt_clone_tx = port->type->ops.pkt_clone_tx;
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
 * Packet mirroring.
 */
int
rte_swx_pipeline_mirroring_config(struct rte_swx_pipeline *p,
				  struct rte_swx_pipeline_mirroring_params *params)
{
	CHECK(p, EINVAL);
	CHECK(params, EINVAL);
	CHECK(params->n_slots, EINVAL);
	CHECK(params->n_sessions, EINVAL);
	CHECK(!p->build_done, EEXIST);

	p->n_mirroring_slots = rte_align32pow2(params->n_slots);
	if (p->n_mirroring_slots > 64)
		p->n_mirroring_slots = 64;

	p->n_mirroring_sessions = rte_align32pow2(params->n_sessions);

	return 0;
}

static void
mirroring_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		/* mirroring_slots. */
		free(t->mirroring_slots);
		t->mirroring_slots = NULL;
	}

	/* mirroring_sessions. */
	free(p->mirroring_sessions);
	p->mirroring_sessions = NULL;
}

static void
mirroring_free(struct rte_swx_pipeline *p)
{
	mirroring_build_free(p);
}

static int
mirroring_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		/* mirroring_slots. */
		t->mirroring_slots = calloc(p->n_mirroring_slots, sizeof(uint32_t));
		if (!t->mirroring_slots)
			goto error;
	}

	/* mirroring_sessions. */
	p->mirroring_sessions = calloc(p->n_mirroring_sessions, sizeof(struct mirroring_session));
	if (!p->mirroring_sessions)
		goto error;

	return 0;

error:
	mirroring_build_free(p);
	return -ENOMEM;
}

/*
 * Extern object.
 */
static struct extern_type *
extern_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct extern_type *elem;

	TAILQ_FOREACH(elem, &p->extern_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct extern_type_member_func *
extern_type_member_func_find(struct extern_type *type, const char *name)
{
	struct extern_type_member_func *elem;

	TAILQ_FOREACH(elem, &type->funcs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct extern_obj *
extern_obj_find(struct rte_swx_pipeline *p, const char *name)
{
	struct extern_obj *elem;

	TAILQ_FOREACH(elem, &p->extern_objs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct extern_type_member_func *
extern_obj_member_func_parse(struct rte_swx_pipeline *p,
			     const char *name,
			     struct extern_obj **obj)
{
	struct extern_obj *object;
	struct extern_type_member_func *func;
	char *object_name, *func_name;

	if (name[0] != 'e' || name[1] != '.')
		return NULL;

	object_name = strdup(&name[2]);
	if (!object_name)
		return NULL;

	func_name = strchr(object_name, '.');
	if (!func_name) {
		free(object_name);
		return NULL;
	}

	*func_name = 0;
	func_name++;

	object = extern_obj_find(p, object_name);
	if (!object) {
		free(object_name);
		return NULL;
	}

	func = extern_type_member_func_find(object->type, func_name);
	if (!func) {
		free(object_name);
		return NULL;
	}

	if (obj)
		*obj = object;

	free(object_name);
	return func;
}

static struct field *
extern_obj_mailbox_field_parse(struct rte_swx_pipeline *p,
			       const char *name,
			       struct extern_obj **object)
{
	struct extern_obj *obj;
	struct field *f;
	char *obj_name, *field_name;

	if ((name[0] != 'e') || (name[1] != '.'))
		return NULL;

	obj_name = strdup(&name[2]);
	if (!obj_name)
		return NULL;

	field_name = strchr(obj_name, '.');
	if (!field_name) {
		free(obj_name);
		return NULL;
	}

	*field_name = 0;
	field_name++;

	obj = extern_obj_find(p, obj_name);
	if (!obj) {
		free(obj_name);
		return NULL;
	}

	f = struct_type_field_find(obj->type->mailbox_struct_type, field_name);
	if (!f) {
		free(obj_name);
		return NULL;
	}

	if (object)
		*object = obj;

	free(obj_name);
	return f;
}

int
rte_swx_pipeline_extern_type_register(struct rte_swx_pipeline *p,
	const char *name,
	const char *mailbox_struct_type_name,
	rte_swx_extern_type_constructor_t constructor,
	rte_swx_extern_type_destructor_t destructor)
{
	struct extern_type *elem;
	struct struct_type *mailbox_struct_type;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_type_find(p, name), EEXIST);

	CHECK_NAME(mailbox_struct_type_name, EINVAL);
	mailbox_struct_type = struct_type_find(p, mailbox_struct_type_name);
	CHECK(mailbox_struct_type, EINVAL);
	CHECK(!mailbox_struct_type->var_size, EINVAL);

	CHECK(constructor, EINVAL);
	CHECK(destructor, EINVAL);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct extern_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	elem->mailbox_struct_type = mailbox_struct_type;
	elem->constructor = constructor;
	elem->destructor = destructor;
	TAILQ_INIT(&elem->funcs);

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->extern_types, elem, node);

	return 0;
}

int
rte_swx_pipeline_extern_type_member_func_register(struct rte_swx_pipeline *p,
	const char *extern_type_name,
	const char *name,
	rte_swx_extern_type_member_func_t member_func)
{
	struct extern_type *type;
	struct extern_type_member_func *type_member;

	CHECK(p, EINVAL);

	CHECK_NAME(extern_type_name, EINVAL);
	type = extern_type_find(p, extern_type_name);
	CHECK(type, EINVAL);
	CHECK(type->n_funcs < RTE_SWX_EXTERN_TYPE_MEMBER_FUNCS_MAX, ENOSPC);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_type_member_func_find(type, name), EEXIST);

	CHECK(member_func, EINVAL);

	/* Node allocation. */
	type_member = calloc(1, sizeof(struct extern_type_member_func));
	CHECK(type_member, ENOMEM);

	/* Node initialization. */
	strcpy(type_member->name, name);
	type_member->func = member_func;
	type_member->id = type->n_funcs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&type->funcs, type_member, node);
	type->n_funcs++;

	return 0;
}

int
rte_swx_pipeline_extern_object_config(struct rte_swx_pipeline *p,
				      const char *extern_type_name,
				      const char *name,
				      const char *args)
{
	struct extern_type *type;
	struct extern_obj *obj;
	void *obj_handle;

	CHECK(p, EINVAL);

	CHECK_NAME(extern_type_name, EINVAL);
	type = extern_type_find(p, extern_type_name);
	CHECK(type, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_obj_find(p, name), EEXIST);

	/* Node allocation. */
	obj = calloc(1, sizeof(struct extern_obj));
	CHECK(obj, ENOMEM);

	/* Object construction. */
	obj_handle = type->constructor(args);
	if (!obj_handle) {
		free(obj);
		CHECK(0, ENODEV);
	}

	/* Node initialization. */
	strcpy(obj->name, name);
	obj->type = type;
	obj->obj = obj_handle;
	obj->struct_id = p->n_structs;
	obj->id = p->n_extern_objs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->extern_objs, obj, node);
	p->n_extern_objs++;
	p->n_structs++;

	return 0;
}

static int
extern_obj_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct extern_obj *obj;

		t->extern_objs = calloc(p->n_extern_objs,
					sizeof(struct extern_obj_runtime));
		CHECK(t->extern_objs, ENOMEM);

		TAILQ_FOREACH(obj, &p->extern_objs, node) {
			struct extern_obj_runtime *r =
				&t->extern_objs[obj->id];
			struct extern_type_member_func *func;
			uint32_t mailbox_size =
				obj->type->mailbox_struct_type->n_bits / 8;

			r->obj = obj->obj;

			r->mailbox = calloc(1, mailbox_size);
			CHECK(r->mailbox, ENOMEM);

			TAILQ_FOREACH(func, &obj->type->funcs, node)
				r->funcs[func->id] = func->func;

			t->structs[obj->struct_id] = r->mailbox;
		}
	}

	return 0;
}

static void
extern_obj_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->extern_objs)
			continue;

		for (j = 0; j < p->n_extern_objs; j++) {
			struct extern_obj_runtime *r = &t->extern_objs[j];

			free(r->mailbox);
		}

		free(t->extern_objs);
		t->extern_objs = NULL;
	}
}

static void
extern_obj_free(struct rte_swx_pipeline *p)
{
	extern_obj_build_free(p);

	/* Extern objects. */
	for ( ; ; ) {
		struct extern_obj *elem;

		elem = TAILQ_FIRST(&p->extern_objs);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->extern_objs, elem, node);
		if (elem->obj)
			elem->type->destructor(elem->obj);
		free(elem);
	}

	/* Extern types. */
	for ( ; ; ) {
		struct extern_type *elem;

		elem = TAILQ_FIRST(&p->extern_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->extern_types, elem, node);

		for ( ; ; ) {
			struct extern_type_member_func *func;

			func = TAILQ_FIRST(&elem->funcs);
			if (!func)
				break;

			TAILQ_REMOVE(&elem->funcs, func, node);
			free(func);
		}

		free(elem);
	}
}

/*
 * Extern function.
 */
static struct extern_func *
extern_func_find(struct rte_swx_pipeline *p, const char *name)
{
	struct extern_func *elem;

	TAILQ_FOREACH(elem, &p->extern_funcs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct extern_func *
extern_func_parse(struct rte_swx_pipeline *p,
		  const char *name)
{
	if (name[0] != 'f' || name[1] != '.')
		return NULL;

	return extern_func_find(p, &name[2]);
}

static struct field *
extern_func_mailbox_field_parse(struct rte_swx_pipeline *p,
				const char *name,
				struct extern_func **function)
{
	struct extern_func *func;
	struct field *f;
	char *func_name, *field_name;

	if ((name[0] != 'f') || (name[1] != '.'))
		return NULL;

	func_name = strdup(&name[2]);
	if (!func_name)
		return NULL;

	field_name = strchr(func_name, '.');
	if (!field_name) {
		free(func_name);
		return NULL;
	}

	*field_name = 0;
	field_name++;

	func = extern_func_find(p, func_name);
	if (!func) {
		free(func_name);
		return NULL;
	}

	f = struct_type_field_find(func->mailbox_struct_type, field_name);
	if (!f) {
		free(func_name);
		return NULL;
	}

	if (function)
		*function = func;

	free(func_name);
	return f;
}

int
rte_swx_pipeline_extern_func_register(struct rte_swx_pipeline *p,
				      const char *name,
				      const char *mailbox_struct_type_name,
				      rte_swx_extern_func_t func)
{
	struct extern_func *f;
	struct struct_type *mailbox_struct_type;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!extern_func_find(p, name), EEXIST);

	CHECK_NAME(mailbox_struct_type_name, EINVAL);
	mailbox_struct_type = struct_type_find(p, mailbox_struct_type_name);
	CHECK(mailbox_struct_type, EINVAL);
	CHECK(!mailbox_struct_type->var_size, EINVAL);

	CHECK(func, EINVAL);

	/* Node allocation. */
	f = calloc(1, sizeof(struct extern_func));
	CHECK(func, ENOMEM);

	/* Node initialization. */
	strcpy(f->name, name);
	f->mailbox_struct_type = mailbox_struct_type;
	f->func = func;
	f->struct_id = p->n_structs;
	f->id = p->n_extern_funcs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->extern_funcs, f, node);
	p->n_extern_funcs++;
	p->n_structs++;

	return 0;
}

static int
extern_func_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct extern_func *func;

		/* Memory allocation. */
		t->extern_funcs = calloc(p->n_extern_funcs,
					 sizeof(struct extern_func_runtime));
		CHECK(t->extern_funcs, ENOMEM);

		/* Extern function. */
		TAILQ_FOREACH(func, &p->extern_funcs, node) {
			struct extern_func_runtime *r =
				&t->extern_funcs[func->id];
			uint32_t mailbox_size =
				func->mailbox_struct_type->n_bits / 8;

			r->func = func->func;

			r->mailbox = calloc(1, mailbox_size);
			CHECK(r->mailbox, ENOMEM);

			t->structs[func->struct_id] = r->mailbox;
		}
	}

	return 0;
}

static void
extern_func_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->extern_funcs)
			continue;

		for (j = 0; j < p->n_extern_funcs; j++) {
			struct extern_func_runtime *r = &t->extern_funcs[j];

			free(r->mailbox);
		}

		free(t->extern_funcs);
		t->extern_funcs = NULL;
	}
}

static void
extern_func_free(struct rte_swx_pipeline *p)
{
	extern_func_build_free(p);

	for ( ; ; ) {
		struct extern_func *elem;

		elem = TAILQ_FIRST(&p->extern_funcs);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->extern_funcs, elem, node);
		free(elem);
	}
}

/*
 * Hash function.
 */
static struct hash_func *
hash_func_find(struct rte_swx_pipeline *p, const char *name)
{
	struct hash_func *elem;

	TAILQ_FOREACH(elem, &p->hash_funcs, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_hash_func_register(struct rte_swx_pipeline *p,
				    const char *name,
				    rte_swx_hash_func_t func)
{
	struct hash_func *f;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!hash_func_find(p, name), EEXIST);

	CHECK(func, EINVAL);

	/* Node allocation. */
	f = calloc(1, sizeof(struct hash_func));
	CHECK(func, ENOMEM);

	/* Node initialization. */
	strcpy(f->name, name);
	f->func = func;
	f->id = p->n_hash_funcs;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->hash_funcs, f, node);
	p->n_hash_funcs++;

	return 0;
}

static int
hash_func_build(struct rte_swx_pipeline *p)
{
	struct hash_func *func;

	/* Memory allocation. */
	p->hash_func_runtime = calloc(p->n_hash_funcs, sizeof(struct hash_func_runtime));
	CHECK(p->hash_func_runtime, ENOMEM);

	/* Hash function. */
	TAILQ_FOREACH(func, &p->hash_funcs, node) {
		struct hash_func_runtime *r = &p->hash_func_runtime[func->id];

		r->func = func->func;
	}

	return 0;
}

static void
hash_func_build_free(struct rte_swx_pipeline *p)
{
	free(p->hash_func_runtime);
	p->hash_func_runtime = NULL;
}

static void
hash_func_free(struct rte_swx_pipeline *p)
{
	hash_func_build_free(p);

	for ( ; ; ) {
		struct hash_func *elem;

		elem = TAILQ_FIRST(&p->hash_funcs);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->hash_funcs, elem, node);
		free(elem);
	}
}

/*
 * RSS.
 */
static struct rss *
rss_find(struct rte_swx_pipeline *p, const char *name)
{
	struct rss *elem;

	TAILQ_FOREACH(elem, &p->rss, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct rss *
rss_find_by_id(struct rte_swx_pipeline *p, uint32_t rss_obj_id)
{
	struct rss *elem;

	TAILQ_FOREACH(elem, &p->rss, node)
		if (elem->id == rss_obj_id)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_rss_config(struct rte_swx_pipeline *p, const char *name)
{
	struct rss *r;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!rss_find(p, name), EEXIST);

	/* Memory allocation. */
	r = calloc(1, sizeof(struct rss));
	CHECK(r, ENOMEM);

	/* Node initialization. */
	strcpy(r->name, name);
	r->id = p->n_rss;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->rss, r, node);
	p->n_rss++;

	return 0;
}

static void
rss_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	if (!p->rss_runtime)
		return;

	for (i = 0; i < p->n_rss; i++)
		free(p->rss_runtime[i]);

	free(p->rss_runtime);
	p->rss_runtime = NULL;
}

static const struct {
	uint32_t key_size;
	uint8_t key[4];
} rss_runtime_default = {
	.key_size = 4,
	.key = {0, 0, 0, 0},
};

static int
rss_build(struct rte_swx_pipeline *p)
{
	uint32_t i;
	int status = 0;

	/* Memory allocation. */
	p->rss_runtime = calloc(p->n_rss, sizeof(struct rss_runtime *));
	if (!p->rss_runtime) {
		status = -ENOMEM;
		goto error;
	}

	/* RSS. */
	for (i = 0; i < p->n_rss; i++) {
		p->rss_runtime[i] = malloc(sizeof(rss_runtime_default));
		if (!p->rss_runtime[i]) {
			status = -ENOMEM;
			goto error;
		}

		memcpy(p->rss_runtime[i], &rss_runtime_default, sizeof(rss_runtime_default));
	}

	return 0;

error:
	rss_build_free(p);
	return status;
}

static void
rss_free(struct rte_swx_pipeline *p)
{
	rss_build_free(p);

	for ( ; ; ) {
		struct rss *elem;

		elem = TAILQ_FIRST(&p->rss);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->rss, elem, node);
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

static struct header *
header_find_by_struct_id(struct rte_swx_pipeline *p, uint32_t struct_id)
{
	struct header *elem;

	TAILQ_FOREACH(elem, &p->headers, node)
		if (elem->struct_id == struct_id)
			return elem;

	return NULL;
}

static struct header *
header_parse(struct rte_swx_pipeline *p,
	     const char *name)
{
	if (name[0] != 'h' || name[1] != '.')
		return NULL;

	return header_find(p, &name[2]);
}

static struct field *
header_field_parse(struct rte_swx_pipeline *p,
		   const char *name,
		   struct header **header)
{
	struct header *h;
	struct field *f;
	char *header_name, *field_name;

	if ((name[0] != 'h') || (name[1] != '.'))
		return NULL;

	header_name = strdup(&name[2]);
	if (!header_name)
		return NULL;

	field_name = strchr(header_name, '.');
	if (!field_name) {
		free(header_name);
		return NULL;
	}

	*field_name = 0;
	field_name++;

	h = header_find(p, header_name);
	if (!h) {
		free(header_name);
		return NULL;
	}

	f = struct_type_field_find(h->st, field_name);
	if (!f) {
		free(header_name);
		return NULL;
	}

	if (header)
		*header = h;

	free(header_name);
	return f;
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
			uint32_t n_bytes =  h->st->n_bits / 8;

			header_storage = &t->header_storage[offset];
			offset += n_bytes;

			t->headers[h->id].ptr0 = header_storage;
			t->headers[h->id].n_bytes = n_bytes;

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
static struct field *
metadata_field_parse(struct rte_swx_pipeline *p, const char *name)
{
	if (!p->metadata_st)
		return NULL;

	if (name[0] != 'm' || name[1] != '.')
		return NULL;

	return struct_type_field_find(p->metadata_st, &name[2]);
}

int
rte_swx_pipeline_packet_metadata_register(struct rte_swx_pipeline *p,
					  const char *struct_type_name)
{
	struct struct_type *st = NULL;

	CHECK(p, EINVAL);

	CHECK_NAME(struct_type_name, EINVAL);
	st  = struct_type_find(p, struct_type_name);
	CHECK(st, EINVAL);
	CHECK(!st->var_size, EINVAL);
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
 * Instruction.
 */
static int
instruction_is_tx(enum instruction_type type)
{
	switch (type) {
	case INSTR_TX:
	case INSTR_TX_I:
	case INSTR_DROP:
		return 1;

	default:
		return 0;
	}
}

static int
instruction_does_tx(struct instruction *instr)
{
	switch (instr->type) {
	case INSTR_TX:
	case INSTR_TX_I:
	case INSTR_DROP:
	case INSTR_HDR_EMIT_TX:
	case INSTR_HDR_EMIT2_TX:
	case INSTR_HDR_EMIT3_TX:
	case INSTR_HDR_EMIT4_TX:
	case INSTR_HDR_EMIT5_TX:
	case INSTR_HDR_EMIT6_TX:
	case INSTR_HDR_EMIT7_TX:
	case INSTR_HDR_EMIT8_TX:
		return 1;
	default:
		return 0;
	}
}

static int
instruction_is_jmp(struct instruction *instr)
{
	switch (instr->type) {
	case INSTR_JMP:
	case INSTR_JMP_VALID:
	case INSTR_JMP_INVALID:
	case INSTR_JMP_HIT:
	case INSTR_JMP_MISS:
	case INSTR_JMP_ACTION_HIT:
	case INSTR_JMP_ACTION_MISS:
	case INSTR_JMP_EQ:
	case INSTR_JMP_EQ_MH:
	case INSTR_JMP_EQ_HM:
	case INSTR_JMP_EQ_HH:
	case INSTR_JMP_EQ_I:
	case INSTR_JMP_NEQ:
	case INSTR_JMP_NEQ_MH:
	case INSTR_JMP_NEQ_HM:
	case INSTR_JMP_NEQ_HH:
	case INSTR_JMP_NEQ_I:
	case INSTR_JMP_LT:
	case INSTR_JMP_LT_MH:
	case INSTR_JMP_LT_HM:
	case INSTR_JMP_LT_HH:
	case INSTR_JMP_LT_MI:
	case INSTR_JMP_LT_HI:
	case INSTR_JMP_GT:
	case INSTR_JMP_GT_MH:
	case INSTR_JMP_GT_HM:
	case INSTR_JMP_GT_HH:
	case INSTR_JMP_GT_MI:
	case INSTR_JMP_GT_HI:
		return 1;

	default:
		return 0;
	}
}

static int
instruction_does_thread_yield(struct instruction *instr)
{
	switch (instr->type) {
	case INSTR_RX:
	case INSTR_TABLE:
	case INSTR_TABLE_AF:
	case INSTR_SELECTOR:
	case INSTR_LEARNER:
	case INSTR_LEARNER_AF:
	case INSTR_EXTERN_OBJ:
	case INSTR_EXTERN_FUNC:
		return 1;
	default:
		return 0;
	}
}

static struct field *
action_field_parse(struct action *action, const char *name);

static struct field *
struct_field_parse(struct rte_swx_pipeline *p,
		   struct action *action,
		   const char *name,
		   uint32_t *struct_id)
{
	struct field *f;

	switch (name[0]) {
	case 'h':
	{
		struct header *header;

		f = header_field_parse(p, name, &header);
		if (!f)
			return NULL;

		*struct_id = header->struct_id;
		return f;
	}

	case 'm':
	{
		f = metadata_field_parse(p, name);
		if (!f)
			return NULL;

		*struct_id = p->metadata_struct_id;
		return f;
	}

	case 't':
	{
		if (!action)
			return NULL;

		f = action_field_parse(action, name);
		if (!f)
			return NULL;

		*struct_id = 0;
		return f;
	}

	case 'e':
	{
		struct extern_obj *obj;

		f = extern_obj_mailbox_field_parse(p, name, &obj);
		if (!f)
			return NULL;

		*struct_id = obj->struct_id;
		return f;
	}

	case 'f':
	{
		struct extern_func *func;

		f = extern_func_mailbox_field_parse(p, name, &func);
		if (!f)
			return NULL;

		*struct_id = func->struct_id;
		return f;
	}

	default:
		return NULL;
	}
}

/*
 * rx.
 */
static int
instr_rx_translate(struct rte_swx_pipeline *p,
		   struct action *action,
		   char **tokens,
		   int n_tokens,
		   struct instruction *instr,
		   struct instruction_data *data __rte_unused)
{
	struct field *f;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	f = metadata_field_parse(p, tokens[1]);
	CHECK(f, EINVAL);
	CHECK(f->n_bits <= 64, EINVAL);

	instr->type = INSTR_RX;
	instr->io.io.offset = f->offset / 8;
	instr->io.io.n_bits = f->n_bits;
	return 0;
}

/*
 * tx.
 */
static int
instr_tx_translate(struct rte_swx_pipeline *p,
		   struct action *action __rte_unused,
		   char **tokens,
		   int n_tokens,
		   struct instruction *instr,
		   struct instruction_data *data __rte_unused)
{
	char *port = tokens[1];
	struct field *f;
	uint32_t port_val;

	CHECK(n_tokens == 2, EINVAL);

	f = metadata_field_parse(p, port);
	if (f) {
		CHECK(f->n_bits <= 64, EINVAL);
		instr->type = INSTR_TX;
		instr->io.io.offset = f->offset / 8;
		instr->io.io.n_bits = f->n_bits;
		return 0;
	}

	/* TX_I. */
	port_val = strtoul(port, &port, 0);
	CHECK(!port[0], EINVAL);

	instr->type = INSTR_TX_I;
	instr->io.io.val = port_val;
	return 0;
}

static int
instr_drop_translate(struct rte_swx_pipeline *p __rte_unused,
		     struct action *action __rte_unused,
		     char **tokens __rte_unused,
		     int n_tokens,
		     struct instruction *instr,
		     struct instruction_data *data __rte_unused)
{
	CHECK(n_tokens == 1, EINVAL);

	/* DROP. */
	instr->type = INSTR_DROP;
	return 0;
}

static inline void
instr_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_tx_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_tx_i_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_drop_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_drop_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

/*
 * mirror.
 */
static int
instr_mirror_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, action, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(dst[0] != 'h', EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	CHECK(fsrc, EINVAL);
	CHECK(src[0] != 'h', EINVAL);
	CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

	instr->type = INSTR_MIRROR;
	instr->mirror.dst.struct_id = (uint8_t)dst_struct_id;
	instr->mirror.dst.n_bits = fdst->n_bits;
	instr->mirror.dst.offset = fdst->offset / 8;
	instr->mirror.src.struct_id = (uint8_t)src_struct_id;
	instr->mirror.src.n_bits = fsrc->n_bits;
	instr->mirror.src.offset = fsrc->offset / 8;

	return 0;
}

static inline void
instr_mirror_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mirror_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * recirculate.
 */
static int
instr_recirculate_translate(struct rte_swx_pipeline *p __rte_unused,
			    struct action *action __rte_unused,
			    char **tokens __rte_unused,
			    int n_tokens,
			    struct instruction *instr,
			    struct instruction_data *data __rte_unused)
{
	CHECK(n_tokens == 1, EINVAL);

	instr->type = INSTR_RECIRCULATE;
	return 0;
}

static int
instr_recircid_translate(struct rte_swx_pipeline *p,
			 struct action *action __rte_unused,
			 char **tokens,
			 int n_tokens,
			 struct instruction *instr,
			 struct instruction_data *data __rte_unused)
{
	struct field *f;

	CHECK(n_tokens == 2, EINVAL);

	f = metadata_field_parse(p, tokens[1]);
	CHECK(f, EINVAL);
	CHECK(f->n_bits <= 64, EINVAL);

	instr->type = INSTR_RECIRCID;
	instr->io.io.offset = f->offset / 8;
	instr->io.io.n_bits = f->n_bits;
	return 0;
}

static inline void
instr_recirculate_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_recirculate_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_recircid_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_recircid_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * extract.
 */
static int
instr_hdr_extract_translate(struct rte_swx_pipeline *p,
			    struct action *action,
			    char **tokens,
			    int n_tokens,
			    struct instruction *instr,
			    struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(!action, EINVAL);
	CHECK((n_tokens == 2) || (n_tokens == 3), EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	if (n_tokens == 2) {
		CHECK(!h->st->var_size, EINVAL);

		instr->type = INSTR_HDR_EXTRACT;
		instr->io.hdr.header_id[0] = h->id;
		instr->io.hdr.struct_id[0] = h->struct_id;
		instr->io.hdr.n_bytes[0] = h->st->n_bits / 8;
	} else {
		struct field *mf;

		CHECK(h->st->var_size, EINVAL);

		mf = metadata_field_parse(p, tokens[2]);
		CHECK(mf, EINVAL);
		CHECK(mf->n_bits <= 64, EINVAL);

		instr->type = INSTR_HDR_EXTRACT_M;
		instr->io.io.offset = mf->offset / 8;
		instr->io.io.n_bits = mf->n_bits;
		instr->io.hdr.header_id[0] = h->id;
		instr->io.hdr.struct_id[0] = h->struct_id;
		instr->io.hdr.n_bytes[0] = h->st->n_bits_min / 8;
	}

	return 0;
}

static int
instr_hdr_lookahead_translate(struct rte_swx_pipeline *p,
			      struct action *action,
			      char **tokens,
			      int n_tokens,
			      struct instruction *instr,
			      struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);
	CHECK(!h->st->var_size, EINVAL);

	instr->type = INSTR_HDR_LOOKAHEAD;
	instr->io.hdr.header_id[0] = h->id;
	instr->io.hdr.struct_id[0] = h->struct_id;
	instr->io.hdr.n_bytes[0] = 0; /* Unused. */

	return 0;
}

static inline void
instr_hdr_extract_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract2_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract2_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract3_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract3_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract4_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract4_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract5_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract5_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract6_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract6_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract7_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract7_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract8_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract8_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_extract_m_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_extract_m_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_lookahead_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_lookahead_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * emit.
 */
static int
instr_hdr_emit_translate(struct rte_swx_pipeline *p,
			 struct action *action __rte_unused,
			 char **tokens,
			 int n_tokens,
			 struct instruction *instr,
			 struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_EMIT;
	instr->io.hdr.header_id[0] = h->id;
	instr->io.hdr.struct_id[0] = h->struct_id;
	instr->io.hdr.n_bytes[0] = h->st->n_bits / 8;
	return 0;
}

static inline void
instr_hdr_emit_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_hdr_emit_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit2_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit2_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit3_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit3_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit4_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit4_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit5_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit5_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit6_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit6_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit7_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit7_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

static inline void
instr_hdr_emit8_tx_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_emit8_tx_exec(p, t, ip);

	/* Thread. */
	thread_ip_reset(p, t);
	instr_rx_exec(p);
}

/*
 * validate.
 */
static int
instr_hdr_validate_translate(struct rte_swx_pipeline *p,
			     struct action *action __rte_unused,
			     char **tokens,
			     int n_tokens,
			     struct instruction *instr,
			     struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_VALIDATE;
	instr->valid.header_id = h->id;
	instr->valid.struct_id = h->struct_id;
	return 0;
}

static inline void
instr_hdr_validate_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_validate_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * invalidate.
 */
static int
instr_hdr_invalidate_translate(struct rte_swx_pipeline *p,
			       struct action *action __rte_unused,
			       char **tokens,
			       int n_tokens,
			       struct instruction *instr,
			       struct instruction_data *data __rte_unused)
{
	struct header *h;

	CHECK(n_tokens == 2, EINVAL);

	h = header_parse(p, tokens[1]);
	CHECK(h, EINVAL);

	instr->type = INSTR_HDR_INVALIDATE;
	instr->valid.header_id = h->id;
	return 0;
}

static inline void
instr_hdr_invalidate_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_hdr_invalidate_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * table.
 */
static struct table *
table_find(struct rte_swx_pipeline *p, const char *name);

static struct selector *
selector_find(struct rte_swx_pipeline *p, const char *name);

static struct learner *
learner_find(struct rte_swx_pipeline *p, const char *name);

static int
instr_table_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	struct table *t;
	struct selector *s;
	struct learner *l;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	t = table_find(p, tokens[1]);
	if (t) {
		instr->type = INSTR_TABLE;
		instr->table.table_id = t->id;
		return 0;
	}

	s = selector_find(p, tokens[1]);
	if (s) {
		instr->type = INSTR_SELECTOR;
		instr->table.table_id = s->id;
		return 0;
	}

	l = learner_find(p, tokens[1]);
	if (l) {
		instr->type = INSTR_LEARNER;
		instr->table.table_id = l->id;
		return 0;
	}

	CHECK(0, EINVAL);
}

static inline void
instr_table_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t table_id = ip->table.table_id;
	struct rte_swx_table_state *ts = &t->table_state[table_id];
	struct table_runtime *table = &t->tables[table_id];
	struct table_statistics *stats = &p->table_stats[table_id];
	uint64_t action_id, n_pkts_hit, n_pkts_action;
	uint8_t *action_data;
	size_t entry_id;
	int done, hit;

	/* Table. */
	done = table->func(ts->obj,
			   table->mailbox,
			   table->key,
			   &action_id,
			   &action_data,
			   &entry_id,
			   &hit);
	if (!done) {
		/* Thread. */
		TRACE("[Thread %2u] table %u (not finalized)\n",
		      p->thread_id,
		      table_id);

		thread_yield(p);
		return;
	}

	action_id = hit ? action_id : ts->default_action_id;
	action_data = hit ? action_data : ts->default_action_data;
	entry_id = hit ? (1 + entry_id) : 0;
	n_pkts_hit = stats->n_pkts_hit[hit];
	n_pkts_action = stats->n_pkts_action[action_id];

	TRACE("[Thread %2u] table %u (%s, action %u)\n",
	      p->thread_id,
	      table_id,
	      hit ? "hit" : "miss",
	      (uint32_t)action_id);

	t->action_id = action_id;
	t->structs[0] = action_data;
	t->entry_id = entry_id;
	t->hit = hit;
	stats->n_pkts_hit[hit] = n_pkts_hit + 1;
	stats->n_pkts_action[action_id] = n_pkts_action + 1;

	/* Thread. */
	thread_ip_action_call(p, t, action_id);
}

static inline void
instr_table_af_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t table_id = ip->table.table_id;
	struct rte_swx_table_state *ts = &t->table_state[table_id];
	struct table_runtime *table = &t->tables[table_id];
	struct table_statistics *stats = &p->table_stats[table_id];
	uint64_t action_id, n_pkts_hit, n_pkts_action;
	uint8_t *action_data;
	size_t entry_id;
	action_func_t action_func;
	int done, hit;

	/* Table. */
	done = table->func(ts->obj,
			   table->mailbox,
			   table->key,
			   &action_id,
			   &action_data,
			   &entry_id,
			   &hit);
	if (!done) {
		/* Thread. */
		TRACE("[Thread %2u] table %u (not finalized)\n",
		      p->thread_id,
		      table_id);

		thread_yield(p);
		return;
	}

	action_id = hit ? action_id : ts->default_action_id;
	action_data = hit ? action_data : ts->default_action_data;
	entry_id = hit ? (1 + entry_id) : 0;
	action_func = p->action_funcs[action_id];
	n_pkts_hit = stats->n_pkts_hit[hit];
	n_pkts_action = stats->n_pkts_action[action_id];

	TRACE("[Thread %2u] table %u (%s, action %u)\n",
	      p->thread_id,
	      table_id,
	      hit ? "hit" : "miss",
	      (uint32_t)action_id);

	t->action_id = action_id;
	t->structs[0] = action_data;
	t->entry_id = entry_id;
	t->hit = hit;
	stats->n_pkts_hit[hit] = n_pkts_hit + 1;
	stats->n_pkts_action[action_id] = n_pkts_action + 1;

	/* Thread. */
	thread_ip_inc(p);

	/* Action. */
	action_func(p);
}

static inline void
instr_selector_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t selector_id = ip->table.table_id;
	struct rte_swx_table_state *ts = &t->table_state[p->n_tables + selector_id];
	struct selector_runtime *selector = &t->selectors[selector_id];
	struct selector_statistics *stats = &p->selector_stats[selector_id];
	uint64_t n_pkts = stats->n_pkts;
	int done;

	/* Table. */
	done = rte_swx_table_selector_select(ts->obj,
			   selector->mailbox,
			   selector->group_id_buffer,
			   selector->selector_buffer,
			   selector->member_id_buffer);
	if (!done) {
		/* Thread. */
		TRACE("[Thread %2u] selector %u (not finalized)\n",
		      p->thread_id,
		      selector_id);

		thread_yield(p);
		return;
	}


	TRACE("[Thread %2u] selector %u\n",
	      p->thread_id,
	      selector_id);

	stats->n_pkts = n_pkts + 1;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_learner_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t learner_id = ip->table.table_id;
	struct rte_swx_table_state *ts = &t->table_state[p->n_tables +
		p->n_selectors + learner_id];
	struct learner_runtime *l = &t->learners[learner_id];
	struct learner_statistics *stats = &p->learner_stats[learner_id];
	uint64_t action_id, n_pkts_hit, n_pkts_action, time;
	uint8_t *action_data;
	size_t entry_id;
	int done, hit;

	/* Table. */
	time = rte_get_tsc_cycles();

	done = rte_swx_table_learner_lookup(ts->obj,
					    l->mailbox,
					    time,
					    l->key,
					    &action_id,
					    &action_data,
					    &entry_id,
					    &hit);
	if (!done) {
		/* Thread. */
		TRACE("[Thread %2u] learner %u (not finalized)\n",
		      p->thread_id,
		      learner_id);

		thread_yield(p);
		return;
	}

	action_id = hit ? action_id : ts->default_action_id;
	action_data = hit ? action_data : ts->default_action_data;
	entry_id = hit ? (1 + entry_id) : 0;
	n_pkts_hit = stats->n_pkts_hit[hit];
	n_pkts_action = stats->n_pkts_action[action_id];

	TRACE("[Thread %2u] learner %u (%s, action %u)\n",
	      p->thread_id,
	      learner_id,
	      hit ? "hit" : "miss",
	      (uint32_t)action_id);

	t->action_id = action_id;
	t->structs[0] = action_data;
	t->entry_id = entry_id;
	t->hit = hit;
	t->learner_id = learner_id;
	t->time = time;
	stats->n_pkts_hit[hit] = n_pkts_hit + 1;
	stats->n_pkts_action[action_id] = n_pkts_action + 1;

	/* Thread. */
	thread_ip_action_call(p, t, action_id);
}

static inline void
instr_learner_af_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t learner_id = ip->table.table_id;
	struct rte_swx_table_state *ts = &t->table_state[p->n_tables +
		p->n_selectors + learner_id];
	struct learner_runtime *l = &t->learners[learner_id];
	struct learner_statistics *stats = &p->learner_stats[learner_id];
	uint64_t action_id, n_pkts_hit, n_pkts_action, time;
	uint8_t *action_data;
	size_t entry_id;
	action_func_t action_func;
	int done, hit;

	/* Table. */
	time = rte_get_tsc_cycles();

	done = rte_swx_table_learner_lookup(ts->obj,
					    l->mailbox,
					    time,
					    l->key,
					    &action_id,
					    &action_data,
					    &entry_id,
					    &hit);
	if (!done) {
		/* Thread. */
		TRACE("[Thread %2u] learner %u (not finalized)\n",
		      p->thread_id,
		      learner_id);

		thread_yield(p);
		return;
	}

	action_id = hit ? action_id : ts->default_action_id;
	action_data = hit ? action_data : ts->default_action_data;
	entry_id = hit ? (1 + entry_id) : 0;
	action_func = p->action_funcs[action_id];
	n_pkts_hit = stats->n_pkts_hit[hit];
	n_pkts_action = stats->n_pkts_action[action_id];

	TRACE("[Thread %2u] learner %u (%s, action %u)\n",
	      p->thread_id,
	      learner_id,
	      hit ? "hit" : "miss",
	      (uint32_t)action_id);

	t->action_id = action_id;
	t->structs[0] = action_data;
	t->entry_id = entry_id;
	t->hit = hit;
	t->learner_id = learner_id;
	t->time = time;
	stats->n_pkts_hit[hit] = n_pkts_hit + 1;
	stats->n_pkts_action[action_id] = n_pkts_action + 1;

	/* Thread. */
	thread_ip_inc(p);

	/* Action */
	action_func(p);
}

/*
 * learn.
 */
static struct action *
action_find(struct rte_swx_pipeline *p, const char *name);

static int
action_has_nbo_args(struct action *a);

static int
learner_action_args_check(struct rte_swx_pipeline *p, struct action *a, const char *mf_name);

static int
instr_learn_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	struct action *a;
	struct field *mf_first_arg = NULL, *mf_timeout_id = NULL;
	const char *mf_first_arg_name, *mf_timeout_id_name;

	CHECK(action, EINVAL);
	CHECK((n_tokens == 3) || (n_tokens == 4), EINVAL);

	/* Action. */
	a = action_find(p, tokens[1]);
	CHECK(a, EINVAL);
	CHECK(!action_has_nbo_args(a), EINVAL);

	/* Action first argument. */
	mf_first_arg_name = (n_tokens == 4) ? tokens[2] : NULL;
	CHECK(!learner_action_args_check(p, a, mf_first_arg_name), EINVAL);

	if (mf_first_arg_name) {
		mf_first_arg = metadata_field_parse(p, mf_first_arg_name);
		CHECK(mf_first_arg, EINVAL);
		CHECK(mf_first_arg->n_bits <= 64, EINVAL);
	}

	/* Timeout ID. */
	mf_timeout_id_name = (n_tokens == 4) ? tokens[3] : tokens[2];
	CHECK_NAME(mf_timeout_id_name, EINVAL);
	mf_timeout_id = metadata_field_parse(p, mf_timeout_id_name);
	CHECK(mf_timeout_id, EINVAL);
	CHECK(mf_timeout_id->n_bits <= 64, EINVAL);

	/* Instruction. */
	instr->type = INSTR_LEARNER_LEARN;
	instr->learn.action_id = a->id;
	instr->learn.mf_first_arg_offset = mf_first_arg ? (mf_first_arg->offset / 8) : 0;
	instr->learn.mf_timeout_id_offset = mf_timeout_id->offset / 8;
	instr->learn.mf_timeout_id_n_bits = mf_timeout_id->n_bits;

	return 0;
}

static inline void
instr_learn_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_learn_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * rearm.
 */
static int
instr_rearm_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	struct field *mf_timeout_id;
	const char *mf_timeout_id_name;

	CHECK(action, EINVAL);
	CHECK((n_tokens == 1) || (n_tokens == 2), EINVAL);

	/* INSTR_LEARNER_REARM. */
	if (n_tokens == 1) {
		instr->type = INSTR_LEARNER_REARM;
		return 0;
	}

	/* INSTR_LEARNER_REARM_NEW. */
	mf_timeout_id_name = tokens[1];
	CHECK_NAME(mf_timeout_id_name, EINVAL);
	mf_timeout_id = metadata_field_parse(p, mf_timeout_id_name);
	CHECK(mf_timeout_id, EINVAL);
	CHECK(mf_timeout_id->n_bits <= 64, EINVAL);

	instr->type = INSTR_LEARNER_REARM_NEW;
	instr->learn.mf_timeout_id_offset = mf_timeout_id->offset / 8;
	instr->learn.mf_timeout_id_n_bits = mf_timeout_id->n_bits;

	return 0;
}

static inline void
instr_rearm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_rearm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_rearm_new_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_rearm_new_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * forget.
 */
static int
instr_forget_translate(struct rte_swx_pipeline *p __rte_unused,
		       struct action *action,
		       char **tokens __rte_unused,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	CHECK(action, EINVAL);
	CHECK(n_tokens == 1, EINVAL);

	instr->type = INSTR_LEARNER_FORGET;

	return 0;
}

static inline void
instr_forget_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_forget_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * entryid.
 */
static int
instr_entryid_translate(struct rte_swx_pipeline *p,
			struct action *action __rte_unused,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	struct field *f;

	CHECK(n_tokens == 2, EINVAL);

	f = metadata_field_parse(p, tokens[1]);
	CHECK(f, EINVAL);
	CHECK(f->n_bits <= 64, EINVAL);

	instr->type = INSTR_ENTRYID;
	instr->mov.dst.n_bits = f->n_bits;
	instr->mov.dst.offset = f->offset / 8;
	return 0;
}

static inline void
instr_entryid_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_entryid_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * extern.
 */
static int
instr_extern_translate(struct rte_swx_pipeline *p,
		       struct action *action __rte_unused,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	char *token = tokens[1];

	CHECK(n_tokens == 2, EINVAL);

	if (token[0] == 'e') {
		struct extern_obj *obj;
		struct extern_type_member_func *func;

		func = extern_obj_member_func_parse(p, token, &obj);
		CHECK(func, EINVAL);

		instr->type = INSTR_EXTERN_OBJ;
		instr->ext_obj.ext_obj_id = obj->id;
		instr->ext_obj.func_id = func->id;

		return 0;
	}

	if (token[0] == 'f') {
		struct extern_func *func;

		func = extern_func_parse(p, token);
		CHECK(func, EINVAL);

		instr->type = INSTR_EXTERN_FUNC;
		instr->ext_func.ext_func_id = func->id;

		return 0;
	}

	CHECK(0, EINVAL);
}

static inline void
instr_extern_obj_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t done;

	/* Extern object member function execute. */
	done = __instr_extern_obj_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc_cond(t, done);
	thread_yield_cond(p, done ^ 1);
}

static inline void
instr_extern_func_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t done;

	/* Extern function execute. */
	done = __instr_extern_func_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc_cond(t, done);
	thread_yield_cond(p, done ^ 1);
}

/*
 * hash.
 */
static int
instr_hash_translate(struct rte_swx_pipeline *p,
		     struct action *action,
		     char **tokens,
		     int n_tokens,
		     struct instruction *instr,
		     struct instruction_data *data __rte_unused)
{
	struct hash_func *func;
	struct field *dst, *src_first, *src_last;
	uint32_t src_struct_id_first = 0, src_struct_id_last = 0;

	CHECK(n_tokens == 5, EINVAL);

	func = hash_func_find(p, tokens[1]);
	CHECK(func, EINVAL);

	dst = metadata_field_parse(p, tokens[2]);
	CHECK(dst, EINVAL);
	CHECK(dst->n_bits <= 64, EINVAL);

	src_first = struct_field_parse(p, action, tokens[3], &src_struct_id_first);
	CHECK(src_first, EINVAL);

	src_last = struct_field_parse(p, action, tokens[4], &src_struct_id_last);
	CHECK(src_last, EINVAL);
	CHECK(!src_last->var_size, EINVAL);
	CHECK(src_struct_id_first == src_struct_id_last, EINVAL);

	instr->type = INSTR_HASH_FUNC;
	instr->hash_func.hash_func_id = (uint8_t)func->id;
	instr->hash_func.dst.offset = (uint8_t)dst->offset / 8;
	instr->hash_func.dst.n_bits = (uint8_t)dst->n_bits;
	instr->hash_func.src.struct_id = (uint8_t)src_struct_id_first;
	instr->hash_func.src.offset = (uint16_t)src_first->offset / 8;
	instr->hash_func.src.n_bytes = (uint16_t)((src_last->offset + src_last->n_bits -
		src_first->offset) / 8);

	return 0;
}

static inline void
instr_hash_func_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Extern function execute. */
	__instr_hash_func_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * rss.
 */
static int
instr_rss_translate(struct rte_swx_pipeline *p,
		    struct action *action,
		    char **tokens,
		    int n_tokens,
		    struct instruction *instr,
		    struct instruction_data *data __rte_unused)
{
	struct rss *rss;
	struct field *dst, *src_first, *src_last;
	uint32_t src_struct_id_first = 0, src_struct_id_last = 0;

	CHECK(n_tokens == 5, EINVAL);

	rss = rss_find(p, tokens[1]);
	CHECK(rss, EINVAL);

	dst = metadata_field_parse(p, tokens[2]);
	CHECK(dst, EINVAL);
	CHECK(dst->n_bits <= 64, EINVAL);

	src_first = struct_field_parse(p, action, tokens[3], &src_struct_id_first);
	CHECK(src_first, EINVAL);

	src_last = struct_field_parse(p, action, tokens[4], &src_struct_id_last);
	CHECK(src_last, EINVAL);
	CHECK(!src_last->var_size, EINVAL);
	CHECK(src_struct_id_first == src_struct_id_last, EINVAL);

	instr->type = INSTR_RSS;
	instr->rss.rss_obj_id = (uint8_t)rss->id;
	instr->rss.dst.offset = (uint8_t)dst->offset / 8;
	instr->rss.dst.n_bits = (uint8_t)dst->n_bits;
	instr->rss.src.struct_id = (uint8_t)src_struct_id_first;
	instr->rss.src.offset = (uint16_t)src_first->offset / 8;
	instr->rss.src.n_bytes = (uint16_t)((src_last->offset + src_last->n_bits -
		src_first->offset) / 8);

	return 0;
}

static inline void
instr_rss_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Extern function execute. */
	__instr_rss_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * mov.
 */
static int
instr_mov_translate(struct rte_swx_pipeline *p,
		    struct action *action,
		    char **tokens,
		    int n_tokens,
		    struct instruction *instr,
		    struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size, EINVAL);

	/* MOV, MOV_MH, MOV_HM, MOV_HH, MOV16, MOVDMA. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

		if (fdst->n_bits <= 64 && fsrc->n_bits <= 64) {
			instr->type = INSTR_MOV;
			if (dst[0] != 'h' && src[0] == 'h')
				instr->type = INSTR_MOV_MH;
			if (dst[0] == 'h' && src[0] != 'h')
				instr->type = INSTR_MOV_HM;
			if (dst[0] == 'h' && src[0] == 'h')
				instr->type = INSTR_MOV_HH;
		} else {
			/* The big fields (field with size > 64 bits) are always expected in NBO,
			 * regardless of their type (H or MEFT). In case a big field is involved as
			 * either dst or src, the other field must also be NBO.
			 *
			 * In case the dst field is big, the src field must be either a big field
			 * (of the same or different size as dst) or a small H field. Similarly,
			 * in case the src field is big, the dst field must be either a big field
			 * (of the same or different size as src) or a small H field. Any other case
			 * involving a big field as either dst or src is rejected.
			 */
			CHECK(fdst->n_bits > 64 || dst[0] == 'h', EINVAL);
			CHECK(fsrc->n_bits > 64 || src[0] == 'h', EINVAL);

			instr->type = INSTR_MOV_DMA;
			if (fdst->n_bits == 128 && fsrc->n_bits == 128)
				instr->type = INSTR_MOV_128;
			if (fdst->n_bits == 128 && fsrc->n_bits == 32)
				instr->type = INSTR_MOV_128_32;
		}

		instr->mov.dst.struct_id = (uint8_t)dst_struct_id;
		instr->mov.dst.n_bits = fdst->n_bits;
		instr->mov.dst.offset = fdst->offset / 8;
		instr->mov.src.struct_id = (uint8_t)src_struct_id;
		instr->mov.src.n_bits = fsrc->n_bits;
		instr->mov.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* MOV_I. */
	CHECK(fdst->n_bits <= 64, EINVAL);
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = hton64(src_val) >> (64 - fdst->n_bits);

	instr->type = INSTR_MOV_I;
	instr->mov.dst.struct_id = (uint8_t)dst_struct_id;
	instr->mov.dst.n_bits = fdst->n_bits;
	instr->mov.dst.offset = fdst->offset / 8;
	instr->mov.src_val = src_val;
	return 0;
}

static inline void
instr_mov_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_dma_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_dma_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_128_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_128_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_128_32_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_128_32_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_mov_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_mov_i_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * dma.
 */
static inline void
instr_dma_ht_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht2_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht2_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht3_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht3_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht4_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht4_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht5_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht5_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht6_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht6_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht7_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht7_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_dma_ht8_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	__instr_dma_ht8_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * alu.
 */
static int
instr_alu_add_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* ADD, ADD_HM, ADD_MH, ADD_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_ADD;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_ADD_HM;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_ADD_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_ADD_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* ADD_MI, ADD_HI. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_ADD_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_ADD_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static int
instr_alu_sub_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* SUB, SUB_HM, SUB_MH, SUB_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_SUB;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_SUB_HM;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SUB_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SUB_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* SUB_MI, SUB_HI. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_SUB_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_SUB_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static int
instr_alu_ckadd_translate(struct rte_swx_pipeline *p,
			  struct action *action __rte_unused,
			  char **tokens,
			  int n_tokens,
			  struct instruction *instr,
			  struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct header *hdst, *hsrc;
	struct field *fdst, *fsrc;

	CHECK(n_tokens == 3, EINVAL);

	fdst = header_field_parse(p, dst, &hdst);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits == 16), EINVAL);

	/* CKADD_FIELD. */
	fsrc = header_field_parse(p, src, &hsrc);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_CKADD_FIELD;
		instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* CKADD_STRUCT, CKADD_STRUCT20. */
	hsrc = header_parse(p, src);
	CHECK(hsrc, EINVAL);

	instr->type = INSTR_ALU_CKADD_STRUCT;
	if (!hsrc->st->var_size && ((hsrc->st->n_bits / 8) == 20))
		instr->type = INSTR_ALU_CKADD_STRUCT20;

	instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
	instr->alu.src.n_bits = (uint8_t)hsrc->id; /* The src header ID is stored here. */
	instr->alu.src.offset = 0; /* Unused. */
	return 0;
}

static int
instr_alu_cksub_translate(struct rte_swx_pipeline *p,
			  struct action *action __rte_unused,
			  char **tokens,
			  int n_tokens,
			  struct instruction *instr,
			  struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct header *hdst, *hsrc;
	struct field *fdst, *fsrc;

	CHECK(n_tokens == 3, EINVAL);

	fdst = header_field_parse(p, dst, &hdst);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits == 16), EINVAL);

	fsrc = header_field_parse(p, src, &hsrc);
	CHECK(fsrc, EINVAL);
	CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

	instr->type = INSTR_ALU_CKSUB_FIELD;
	instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
	instr->alu.src.n_bits = fsrc->n_bits;
	instr->alu.src.offset = fsrc->offset / 8;
	return 0;
}

static int
instr_alu_shl_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* SHL, SHL_HM, SHL_MH, SHL_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_SHL;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_SHL_HM;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SHL_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SHL_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* SHL_MI, SHL_HI. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_SHL_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_SHL_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static int
instr_alu_shr_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* SHR, SHR_HM, SHR_MH, SHR_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_SHR;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_SHR_HM;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SHR_MH;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_SHR_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* SHR_MI, SHR_HI. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	instr->type = INSTR_ALU_SHR_MI;
	if (dst[0] == 'h')
		instr->type = INSTR_ALU_SHR_HI;

	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static int
instr_alu_and_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* AND, AND_MH, AND_HM, AND_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_AND;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_AND_MH;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_AND_HM;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_AND_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* AND_I. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = hton64(src_val) >> (64 - fdst->n_bits);

	instr->type = INSTR_ALU_AND_I;
	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static int
instr_alu_or_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* OR, OR_MH, OR_HM, OR_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_OR;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_OR_MH;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_OR_HM;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_OR_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* OR_I. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = hton64(src_val) >> (64 - fdst->n_bits);

	instr->type = INSTR_ALU_OR_I;
	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static int
instr_alu_xor_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *src = tokens[2];
	struct field *fdst, *fsrc;
	uint64_t src_val;
	uint32_t dst_struct_id = 0, src_struct_id = 0;

	CHECK(n_tokens == 3, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* XOR, XOR_MH, XOR_HM, XOR_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_ALU_XOR;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_XOR_MH;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_ALU_XOR_HM;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_ALU_XOR_HH;

		instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
		instr->alu.dst.n_bits = fdst->n_bits;
		instr->alu.dst.offset = fdst->offset / 8;
		instr->alu.src.struct_id = (uint8_t)src_struct_id;
		instr->alu.src.n_bits = fsrc->n_bits;
		instr->alu.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* XOR_I. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	if (dst[0] == 'h')
		src_val = hton64(src_val) >> (64 - fdst->n_bits);

	instr->type = INSTR_ALU_XOR_I;
	instr->alu.dst.struct_id = (uint8_t)dst_struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src_val = src_val;
	return 0;
}

static inline void
instr_alu_add_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs */
	__instr_alu_add_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_add_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_add_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_add_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_add_mi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_add_hi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_sub_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_sub_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_sub_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_sub_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_sub_mi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_sub_hi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shl_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shl_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shl_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shl_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shl_mi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shl_hi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shr_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shr_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shr_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shr_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shr_mi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_shr_hi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_and_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_and_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_and_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_and_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_and_i_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_or_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_or_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_or_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_or_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_or_i_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_xor_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_xor_mh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_xor_hm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_xor_hh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_xor_i_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_field_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_ckadd_field_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_cksub_field_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_cksub_field_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_struct20_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_ckadd_struct20_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_struct_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_alu_ckadd_struct_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * Register array.
 */
static struct regarray *
regarray_find(struct rte_swx_pipeline *p, const char *name);

static int
instr_regprefetch_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	char *regarray = tokens[1], *idx = tokens[2];
	struct regarray *r;
	struct field *fidx;
	uint32_t idx_struct_id, idx_val;

	CHECK(n_tokens == 3, EINVAL);

	r = regarray_find(p, regarray);
	CHECK(r, EINVAL);

	/* REGPREFETCH_RH, REGPREFETCH_RM. */
	fidx = struct_field_parse(p, action, idx, &idx_struct_id);
	if (fidx) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);

		instr->type = INSTR_REGPREFETCH_RM;
		if (idx[0] == 'h')
			instr->type = INSTR_REGPREFETCH_RH;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx.struct_id = (uint8_t)idx_struct_id;
		instr->regarray.idx.n_bits = fidx->n_bits;
		instr->regarray.idx.offset = fidx->offset / 8;
		instr->regarray.dstsrc_val = 0; /* Unused. */
		return 0;
	}

	/* REGPREFETCH_RI. */
	idx_val = strtoul(idx, &idx, 0);
	CHECK(!idx[0], EINVAL);

	instr->type = INSTR_REGPREFETCH_RI;
	instr->regarray.regarray_id = r->id;
	instr->regarray.idx_val = idx_val;
	instr->regarray.dstsrc_val = 0; /* Unused. */
	return 0;
}

static int
instr_regrd_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	char *dst = tokens[1], *regarray = tokens[2], *idx = tokens[3];
	struct regarray *r;
	struct field *fdst, *fidx;
	uint32_t dst_struct_id, idx_struct_id, idx_val;

	CHECK(n_tokens == 4, EINVAL);

	r = regarray_find(p, regarray);
	CHECK(r, EINVAL);

	fdst = struct_field_parse(p, NULL, dst, &dst_struct_id);
	CHECK(fdst, EINVAL);
	CHECK(!fdst->var_size && (fdst->n_bits <= 64), EINVAL);

	/* REGRD_HRH, REGRD_HRM, REGRD_MRH, REGRD_MRM. */
	fidx = struct_field_parse(p, action, idx, &idx_struct_id);
	if (fidx) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);

		instr->type = INSTR_REGRD_MRM;
		if (dst[0] == 'h' && idx[0] != 'h')
			instr->type = INSTR_REGRD_HRM;
		if (dst[0] != 'h' && idx[0] == 'h')
			instr->type = INSTR_REGRD_MRH;
		if (dst[0] == 'h' && idx[0] == 'h')
			instr->type = INSTR_REGRD_HRH;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx.struct_id = (uint8_t)idx_struct_id;
		instr->regarray.idx.n_bits = fidx->n_bits;
		instr->regarray.idx.offset = fidx->offset / 8;
		instr->regarray.dstsrc.struct_id = (uint8_t)dst_struct_id;
		instr->regarray.dstsrc.n_bits = fdst->n_bits;
		instr->regarray.dstsrc.offset = fdst->offset / 8;
		return 0;
	}

	/* REGRD_MRI, REGRD_HRI. */
	idx_val = strtoul(idx, &idx, 0);
	CHECK(!idx[0], EINVAL);

	instr->type = INSTR_REGRD_MRI;
	if (dst[0] == 'h')
		instr->type = INSTR_REGRD_HRI;

	instr->regarray.regarray_id = r->id;
	instr->regarray.idx_val = idx_val;
	instr->regarray.dstsrc.struct_id = (uint8_t)dst_struct_id;
	instr->regarray.dstsrc.n_bits = fdst->n_bits;
	instr->regarray.dstsrc.offset = fdst->offset / 8;
	return 0;
}

static int
instr_regwr_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	char *regarray = tokens[1], *idx = tokens[2], *src = tokens[3];
	struct regarray *r;
	struct field *fidx, *fsrc;
	uint64_t src_val;
	uint32_t idx_struct_id, idx_val, src_struct_id;

	CHECK(n_tokens == 4, EINVAL);

	r = regarray_find(p, regarray);
	CHECK(r, EINVAL);

	/* REGWR_RHH, REGWR_RHM, REGWR_RMH, REGWR_RMM. */
	fidx = struct_field_parse(p, action, idx, &idx_struct_id);
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fidx && fsrc) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_REGWR_RMM;
		if (idx[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_REGWR_RHM;
		if (idx[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_REGWR_RMH;
		if (idx[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_REGWR_RHH;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx.struct_id = (uint8_t)idx_struct_id;
		instr->regarray.idx.n_bits = fidx->n_bits;
		instr->regarray.idx.offset = fidx->offset / 8;
		instr->regarray.dstsrc.struct_id = (uint8_t)src_struct_id;
		instr->regarray.dstsrc.n_bits = fsrc->n_bits;
		instr->regarray.dstsrc.offset = fsrc->offset / 8;
		return 0;
	}

	/* REGWR_RHI, REGWR_RMI. */
	if (fidx && !fsrc) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);

		src_val = strtoull(src, &src, 0);
		CHECK(!src[0], EINVAL);

		instr->type = INSTR_REGWR_RMI;
		if (idx[0] == 'h')
			instr->type = INSTR_REGWR_RHI;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx.struct_id = (uint8_t)idx_struct_id;
		instr->regarray.idx.n_bits = fidx->n_bits;
		instr->regarray.idx.offset = fidx->offset / 8;
		instr->regarray.dstsrc_val = src_val;
		return 0;
	}

	/* REGWR_RIH, REGWR_RIM. */
	if (!fidx && fsrc) {
		idx_val = strtoul(idx, &idx, 0);
		CHECK(!idx[0], EINVAL);

		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_REGWR_RIM;
		if (src[0] == 'h')
			instr->type = INSTR_REGWR_RIH;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx_val = idx_val;
		instr->regarray.dstsrc.struct_id = (uint8_t)src_struct_id;
		instr->regarray.dstsrc.n_bits = fsrc->n_bits;
		instr->regarray.dstsrc.offset = fsrc->offset / 8;
		return 0;
	}

	/* REGWR_RII. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	idx_val = strtoul(idx, &idx, 0);
	CHECK(!idx[0], EINVAL);

	instr->type = INSTR_REGWR_RII;
	instr->regarray.idx_val = idx_val;
	instr->regarray.dstsrc_val = src_val;

	return 0;
}

static int
instr_regadd_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	char *regarray = tokens[1], *idx = tokens[2], *src = tokens[3];
	struct regarray *r;
	struct field *fidx, *fsrc;
	uint64_t src_val;
	uint32_t idx_struct_id, idx_val, src_struct_id;

	CHECK(n_tokens == 4, EINVAL);

	r = regarray_find(p, regarray);
	CHECK(r, EINVAL);

	/* REGADD_RHH, REGADD_RHM, REGADD_RMH, REGADD_RMM. */
	fidx = struct_field_parse(p, action, idx, &idx_struct_id);
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fidx && fsrc) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);
		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_REGADD_RMM;
		if (idx[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_REGADD_RHM;
		if (idx[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_REGADD_RMH;
		if (idx[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_REGADD_RHH;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx.struct_id = (uint8_t)idx_struct_id;
		instr->regarray.idx.n_bits = fidx->n_bits;
		instr->regarray.idx.offset = fidx->offset / 8;
		instr->regarray.dstsrc.struct_id = (uint8_t)src_struct_id;
		instr->regarray.dstsrc.n_bits = fsrc->n_bits;
		instr->regarray.dstsrc.offset = fsrc->offset / 8;
		return 0;
	}

	/* REGADD_RHI, REGADD_RMI. */
	if (fidx && !fsrc) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);

		src_val = strtoull(src, &src, 0);
		CHECK(!src[0], EINVAL);

		instr->type = INSTR_REGADD_RMI;
		if (idx[0] == 'h')
			instr->type = INSTR_REGADD_RHI;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx.struct_id = (uint8_t)idx_struct_id;
		instr->regarray.idx.n_bits = fidx->n_bits;
		instr->regarray.idx.offset = fidx->offset / 8;
		instr->regarray.dstsrc_val = src_val;
		return 0;
	}

	/* REGADD_RIH, REGADD_RIM. */
	if (!fidx && fsrc) {
		idx_val = strtoul(idx, &idx, 0);
		CHECK(!idx[0], EINVAL);

		CHECK(!fsrc->var_size && (fsrc->n_bits <= 64), EINVAL);

		instr->type = INSTR_REGADD_RIM;
		if (src[0] == 'h')
			instr->type = INSTR_REGADD_RIH;

		instr->regarray.regarray_id = r->id;
		instr->regarray.idx_val = idx_val;
		instr->regarray.dstsrc.struct_id = (uint8_t)src_struct_id;
		instr->regarray.dstsrc.n_bits = fsrc->n_bits;
		instr->regarray.dstsrc.offset = fsrc->offset / 8;
		return 0;
	}

	/* REGADD_RII. */
	src_val = strtoull(src, &src, 0);
	CHECK(!src[0], EINVAL);

	idx_val = strtoul(idx, &idx, 0);
	CHECK(!idx[0], EINVAL);

	instr->type = INSTR_REGADD_RII;
	instr->regarray.idx_val = idx_val;
	instr->regarray.dstsrc_val = src_val;
	return 0;
}

static inline void
instr_regprefetch_rh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regprefetch_rh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regprefetch_rm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regprefetch_rm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regprefetch_ri_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regprefetch_ri_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_hrh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regrd_hrh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_hrm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regrd_hrm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_mrh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regrd_mrh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_mrm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regrd_mrm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_hri_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regrd_hri_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_mri_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regrd_mri_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rhh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rhh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rhm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rmh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rmh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rmm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rhi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rmi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rih_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rih_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rim_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rim_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rii_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regwr_rii_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rhh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rhh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rhm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rmh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rmh_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rmm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rhi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rmi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rih_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rih_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rim_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rim_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rii_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_regadd_rii_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * metarray.
 */
static struct metarray *
metarray_find(struct rte_swx_pipeline *p, const char *name);

static int
instr_metprefetch_translate(struct rte_swx_pipeline *p,
			    struct action *action,
			    char **tokens,
			    int n_tokens,
			    struct instruction *instr,
			    struct instruction_data *data __rte_unused)
{
	char *metarray = tokens[1], *idx = tokens[2];
	struct metarray *m;
	struct field *fidx;
	uint32_t idx_struct_id, idx_val;

	CHECK(n_tokens == 3, EINVAL);

	m = metarray_find(p, metarray);
	CHECK(m, EINVAL);

	/* METPREFETCH_H, METPREFETCH_M. */
	fidx = struct_field_parse(p, action, idx, &idx_struct_id);
	if (fidx) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);

		instr->type = INSTR_METPREFETCH_M;
		if (idx[0] == 'h')
			instr->type = INSTR_METPREFETCH_H;

		instr->meter.metarray_id = m->id;
		instr->meter.idx.struct_id = (uint8_t)idx_struct_id;
		instr->meter.idx.n_bits = fidx->n_bits;
		instr->meter.idx.offset = fidx->offset / 8;
		return 0;
	}

	/* METPREFETCH_I. */
	idx_val = strtoul(idx, &idx, 0);
	CHECK(!idx[0], EINVAL);

	instr->type = INSTR_METPREFETCH_I;
	instr->meter.metarray_id = m->id;
	instr->meter.idx_val = idx_val;
	return 0;
}

static int
instr_meter_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	char *metarray = tokens[1], *idx = tokens[2], *length = tokens[3];
	char *color_in = tokens[4], *color_out = tokens[5];
	struct metarray *m;
	struct field *fidx, *flength, *fcin, *fcout;
	uint32_t idx_struct_id, length_struct_id;
	uint32_t color_in_struct_id, color_out_struct_id;

	CHECK(n_tokens == 6, EINVAL);

	m = metarray_find(p, metarray);
	CHECK(m, EINVAL);

	fidx = struct_field_parse(p, action, idx, &idx_struct_id);

	flength = struct_field_parse(p, action, length, &length_struct_id);
	CHECK(flength, EINVAL);
	CHECK(!flength->var_size && (flength->n_bits <= 64), EINVAL);

	fcin = struct_field_parse(p, action, color_in, &color_in_struct_id);

	fcout = struct_field_parse(p, NULL, color_out, &color_out_struct_id);
	CHECK(fcout, EINVAL);
	CHECK(!fcout->var_size  && (fcout->n_bits <= 64), EINVAL);

	/* index = HMEFT, length = HMEFT, color_in = MEFT, color_out = MEF. */
	if (fidx && fcin) {
		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);
		CHECK(!fcin->var_size && (fcin->n_bits <= 64), EINVAL);

		instr->type = INSTR_METER_MMM;
		if (idx[0] == 'h' && length[0] == 'h')
			instr->type = INSTR_METER_HHM;
		if (idx[0] == 'h' && length[0] != 'h')
			instr->type = INSTR_METER_HMM;
		if (idx[0] != 'h' && length[0] == 'h')
			instr->type = INSTR_METER_MHM;

		instr->meter.metarray_id = m->id;

		instr->meter.idx.struct_id = (uint8_t)idx_struct_id;
		instr->meter.idx.n_bits = fidx->n_bits;
		instr->meter.idx.offset = fidx->offset / 8;

		instr->meter.length.struct_id = (uint8_t)length_struct_id;
		instr->meter.length.n_bits = flength->n_bits;
		instr->meter.length.offset = flength->offset / 8;

		instr->meter.color_in.struct_id = (uint8_t)color_in_struct_id;
		instr->meter.color_in.n_bits = fcin->n_bits;
		instr->meter.color_in.offset = fcin->offset / 8;

		instr->meter.color_out.struct_id = (uint8_t)color_out_struct_id;
		instr->meter.color_out.n_bits = fcout->n_bits;
		instr->meter.color_out.offset = fcout->offset / 8;
	}

	/* index = HMEFT, length = HMEFT, color_in = I, color_out = MEF. */
	if (fidx && !fcin) {
		uint32_t color_in_val;

		CHECK(!fidx->var_size && (fidx->n_bits <= 64), EINVAL);

		color_in_val = strtoul(color_in, &color_in, 0);
		CHECK(!color_in[0], EINVAL);

		instr->type = INSTR_METER_MMI;
		if (idx[0] == 'h' && length[0] == 'h')
			instr->type = INSTR_METER_HHI;
		if (idx[0] == 'h' && length[0] != 'h')
			instr->type = INSTR_METER_HMI;
		if (idx[0] != 'h' && length[0] == 'h')
			instr->type = INSTR_METER_MHI;

		instr->meter.metarray_id = m->id;

		instr->meter.idx.struct_id = (uint8_t)idx_struct_id;
		instr->meter.idx.n_bits = fidx->n_bits;
		instr->meter.idx.offset = fidx->offset / 8;

		instr->meter.length.struct_id = (uint8_t)length_struct_id;
		instr->meter.length.n_bits = flength->n_bits;
		instr->meter.length.offset = flength->offset / 8;

		instr->meter.color_in_val = color_in_val;

		instr->meter.color_out.struct_id = (uint8_t)color_out_struct_id;
		instr->meter.color_out.n_bits = fcout->n_bits;
		instr->meter.color_out.offset = fcout->offset / 8;
	}

	/* index = I, length = HMEFT, color_in = MEFT, color_out = MEF. */
	if (!fidx && fcin) {
		uint32_t idx_val;

		idx_val = strtoul(idx, &idx, 0);
		CHECK(!idx[0], EINVAL);

		CHECK(!fcin->var_size && (fcin->n_bits <= 64), EINVAL);

		instr->type = INSTR_METER_IMM;
		if (length[0] == 'h')
			instr->type = INSTR_METER_IHM;

		instr->meter.metarray_id = m->id;

		instr->meter.idx_val = idx_val;

		instr->meter.length.struct_id = (uint8_t)length_struct_id;
		instr->meter.length.n_bits = flength->n_bits;
		instr->meter.length.offset = flength->offset / 8;

		instr->meter.color_in.struct_id = (uint8_t)color_in_struct_id;
		instr->meter.color_in.n_bits = fcin->n_bits;
		instr->meter.color_in.offset = fcin->offset / 8;

		instr->meter.color_out.struct_id = (uint8_t)color_out_struct_id;
		instr->meter.color_out.n_bits = fcout->n_bits;
		instr->meter.color_out.offset = fcout->offset / 8;
	}

	/* index = I, length = HMEFT, color_in = I, color_out = MEF. */
	if (!fidx && !fcin) {
		uint32_t idx_val, color_in_val;

		idx_val = strtoul(idx, &idx, 0);
		CHECK(!idx[0], EINVAL);

		color_in_val = strtoul(color_in, &color_in, 0);
		CHECK(!color_in[0], EINVAL);

		instr->type = INSTR_METER_IMI;
		if (length[0] == 'h')
			instr->type = INSTR_METER_IHI;

		instr->meter.metarray_id = m->id;

		instr->meter.idx_val = idx_val;

		instr->meter.length.struct_id = (uint8_t)length_struct_id;
		instr->meter.length.n_bits = flength->n_bits;
		instr->meter.length.offset = flength->offset / 8;

		instr->meter.color_in_val = color_in_val;

		instr->meter.color_out.struct_id = (uint8_t)color_out_struct_id;
		instr->meter.color_out.n_bits = fcout->n_bits;
		instr->meter.color_out.offset = fcout->offset / 8;
	}

	return 0;
}

static inline void
instr_metprefetch_h_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_metprefetch_h_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_metprefetch_m_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_metprefetch_m_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_metprefetch_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_metprefetch_i_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_hhm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_hhi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_hmm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_hmi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_mhm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_mhi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_mmm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_mmi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_ihm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_ihm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_ihi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_ihi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_imm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_imm_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_imi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	/* Structs. */
	__instr_meter_imi_exec(p, t, ip);

	/* Thread. */
	thread_ip_inc(p);
}

/*
 * jmp.
 */
static int
instr_jmp_translate(struct rte_swx_pipeline *p __rte_unused,
		    struct action *action __rte_unused,
		    char **tokens,
		    int n_tokens,
		    struct instruction *instr,
		    struct instruction_data *data)
{
	CHECK(n_tokens == 2, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	instr->type = INSTR_JMP;
	instr->jmp.ip = NULL; /* Resolved later. */
	return 0;
}

static int
instr_jmp_valid_translate(struct rte_swx_pipeline *p,
			  struct action *action __rte_unused,
			  char **tokens,
			  int n_tokens,
			  struct instruction *instr,
			  struct instruction_data *data)
{
	struct header *h;

	CHECK(n_tokens == 3, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	h = header_parse(p, tokens[2]);
	CHECK(h, EINVAL);

	instr->type = INSTR_JMP_VALID;
	instr->jmp.ip = NULL; /* Resolved later. */
	instr->jmp.header_id = h->id;
	return 0;
}

static int
instr_jmp_invalid_translate(struct rte_swx_pipeline *p,
			    struct action *action __rte_unused,
			    char **tokens,
			    int n_tokens,
			    struct instruction *instr,
			    struct instruction_data *data)
{
	struct header *h;

	CHECK(n_tokens == 3, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	h = header_parse(p, tokens[2]);
	CHECK(h, EINVAL);

	instr->type = INSTR_JMP_INVALID;
	instr->jmp.ip = NULL; /* Resolved later. */
	instr->jmp.header_id = h->id;
	return 0;
}

static int
instr_jmp_hit_translate(struct rte_swx_pipeline *p __rte_unused,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data)
{
	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	instr->type = INSTR_JMP_HIT;
	instr->jmp.ip = NULL; /* Resolved later. */
	return 0;
}

static int
instr_jmp_miss_translate(struct rte_swx_pipeline *p __rte_unused,
			 struct action *action,
			 char **tokens,
			 int n_tokens,
			 struct instruction *instr,
			 struct instruction_data *data)
{
	CHECK(!action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	instr->type = INSTR_JMP_MISS;
	instr->jmp.ip = NULL; /* Resolved later. */
	return 0;
}

static int
instr_jmp_action_hit_translate(struct rte_swx_pipeline *p,
			       struct action *action,
			       char **tokens,
			       int n_tokens,
			       struct instruction *instr,
			       struct instruction_data *data)
{
	struct action *a;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 3, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	a = action_find(p, tokens[2]);
	CHECK(a, EINVAL);

	instr->type = INSTR_JMP_ACTION_HIT;
	instr->jmp.ip = NULL; /* Resolved later. */
	instr->jmp.action_id = a->id;
	return 0;
}

static int
instr_jmp_action_miss_translate(struct rte_swx_pipeline *p,
				struct action *action,
				char **tokens,
				int n_tokens,
				struct instruction *instr,
				struct instruction_data *data)
{
	struct action *a;

	CHECK(!action, EINVAL);
	CHECK(n_tokens == 3, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	a = action_find(p, tokens[2]);
	CHECK(a, EINVAL);

	instr->type = INSTR_JMP_ACTION_MISS;
	instr->jmp.ip = NULL; /* Resolved later. */
	instr->jmp.action_id = a->id;
	return 0;
}

static int
instr_jmp_eq_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data)
{
	char *a = tokens[2], *b = tokens[3];
	struct field *fa, *fb;
	uint64_t b_val;
	uint32_t a_struct_id, b_struct_id;

	CHECK(n_tokens == 4, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	fa = struct_field_parse(p, action, a, &a_struct_id);
	CHECK(fa, EINVAL);
	CHECK(!fa->var_size && (fa->n_bits <= 64), EINVAL);

	/* JMP_EQ, JMP_EQ_MH, JMP_EQ_HM, JMP_EQ_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size && (fb->n_bits <= 64), EINVAL);

		instr->type = INSTR_JMP_EQ;
		if (a[0] != 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_EQ_MH;
		if (a[0] == 'h' && b[0] != 'h')
			instr->type = INSTR_JMP_EQ_HM;
		if (a[0] == 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_EQ_HH;
		instr->jmp.ip = NULL; /* Resolved later. */

		instr->jmp.a.struct_id = (uint8_t)a_struct_id;
		instr->jmp.a.n_bits = fa->n_bits;
		instr->jmp.a.offset = fa->offset / 8;
		instr->jmp.b.struct_id = (uint8_t)b_struct_id;
		instr->jmp.b.n_bits = fb->n_bits;
		instr->jmp.b.offset = fb->offset / 8;
		return 0;
	}

	/* JMP_EQ_I. */
	b_val = strtoull(b, &b, 0);
	CHECK(!b[0], EINVAL);

	if (a[0] == 'h')
		b_val = hton64(b_val) >> (64 - fa->n_bits);

	instr->type = INSTR_JMP_EQ_I;
	instr->jmp.ip = NULL; /* Resolved later. */
	instr->jmp.a.struct_id = (uint8_t)a_struct_id;
	instr->jmp.a.n_bits = fa->n_bits;
	instr->jmp.a.offset = fa->offset / 8;
	instr->jmp.b_val = b_val;
	return 0;
}

static int
instr_jmp_neq_translate(struct rte_swx_pipeline *p,
			struct action *action,
			char **tokens,
			int n_tokens,
			struct instruction *instr,
			struct instruction_data *data)
{
	char *a = tokens[2], *b = tokens[3];
	struct field *fa, *fb;
	uint64_t b_val;
	uint32_t a_struct_id, b_struct_id;

	CHECK(n_tokens == 4, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	fa = struct_field_parse(p, action, a, &a_struct_id);
	CHECK(fa, EINVAL);
	CHECK(!fa->var_size && (fa->n_bits <= 64), EINVAL);

	/* JMP_NEQ, JMP_NEQ_MH, JMP_NEQ_HM, JMP_NEQ_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size && (fb->n_bits <= 64), EINVAL);

		instr->type = INSTR_JMP_NEQ;
		if (a[0] != 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_NEQ_MH;
		if (a[0] == 'h' && b[0] != 'h')
			instr->type = INSTR_JMP_NEQ_HM;
		if (a[0] == 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_NEQ_HH;
		instr->jmp.ip = NULL; /* Resolved later. */

		instr->jmp.a.struct_id = (uint8_t)a_struct_id;
		instr->jmp.a.n_bits = fa->n_bits;
		instr->jmp.a.offset = fa->offset / 8;
		instr->jmp.b.struct_id = (uint8_t)b_struct_id;
		instr->jmp.b.n_bits = fb->n_bits;
		instr->jmp.b.offset = fb->offset / 8;
		return 0;
	}

	/* JMP_NEQ_I. */
	b_val = strtoull(b, &b, 0);
	CHECK(!b[0], EINVAL);

	if (a[0] == 'h')
		b_val = hton64(b_val) >> (64 - fa->n_bits);

	instr->type = INSTR_JMP_NEQ_I;
	instr->jmp.ip = NULL; /* Resolved later. */
	instr->jmp.a.struct_id = (uint8_t)a_struct_id;
	instr->jmp.a.n_bits = fa->n_bits;
	instr->jmp.a.offset = fa->offset / 8;
	instr->jmp.b_val = b_val;
	return 0;
}

static int
instr_jmp_lt_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data)
{
	char *a = tokens[2], *b = tokens[3];
	struct field *fa, *fb;
	uint64_t b_val;
	uint32_t a_struct_id, b_struct_id;

	CHECK(n_tokens == 4, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	fa = struct_field_parse(p, action, a, &a_struct_id);
	CHECK(fa, EINVAL);
	CHECK(!fa->var_size && (fa->n_bits <= 64), EINVAL);

	/* JMP_LT, JMP_LT_MH, JMP_LT_HM, JMP_LT_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size && (fb->n_bits <= 64), EINVAL);

		instr->type = INSTR_JMP_LT;
		if (a[0] == 'h' && b[0] != 'h')
			instr->type = INSTR_JMP_LT_HM;
		if (a[0] != 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_LT_MH;
		if (a[0] == 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_LT_HH;
		instr->jmp.ip = NULL; /* Resolved later. */

		instr->jmp.a.struct_id = (uint8_t)a_struct_id;
		instr->jmp.a.n_bits = fa->n_bits;
		instr->jmp.a.offset = fa->offset / 8;
		instr->jmp.b.struct_id = (uint8_t)b_struct_id;
		instr->jmp.b.n_bits = fb->n_bits;
		instr->jmp.b.offset = fb->offset / 8;
		return 0;
	}

	/* JMP_LT_MI, JMP_LT_HI. */
	b_val = strtoull(b, &b, 0);
	CHECK(!b[0], EINVAL);

	instr->type = INSTR_JMP_LT_MI;
	if (a[0] == 'h')
		instr->type = INSTR_JMP_LT_HI;
	instr->jmp.ip = NULL; /* Resolved later. */

	instr->jmp.a.struct_id = (uint8_t)a_struct_id;
	instr->jmp.a.n_bits = fa->n_bits;
	instr->jmp.a.offset = fa->offset / 8;
	instr->jmp.b_val = b_val;
	return 0;
}

static int
instr_jmp_gt_translate(struct rte_swx_pipeline *p,
		       struct action *action,
		       char **tokens,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data)
{
	char *a = tokens[2], *b = tokens[3];
	struct field *fa, *fb;
	uint64_t b_val;
	uint32_t a_struct_id, b_struct_id;

	CHECK(n_tokens == 4, EINVAL);

	strcpy(data->jmp_label, tokens[1]);

	fa = struct_field_parse(p, action, a, &a_struct_id);
	CHECK(fa, EINVAL);
	CHECK(!fa->var_size && (fa->n_bits <= 64), EINVAL);

	/* JMP_GT, JMP_GT_MH, JMP_GT_HM, JMP_GT_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size && (fb->n_bits <= 64), EINVAL);

		instr->type = INSTR_JMP_GT;
		if (a[0] == 'h' && b[0] != 'h')
			instr->type = INSTR_JMP_GT_HM;
		if (a[0] != 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_GT_MH;
		if (a[0] == 'h' && b[0] == 'h')
			instr->type = INSTR_JMP_GT_HH;
		instr->jmp.ip = NULL; /* Resolved later. */

		instr->jmp.a.struct_id = (uint8_t)a_struct_id;
		instr->jmp.a.n_bits = fa->n_bits;
		instr->jmp.a.offset = fa->offset / 8;
		instr->jmp.b.struct_id = (uint8_t)b_struct_id;
		instr->jmp.b.n_bits = fb->n_bits;
		instr->jmp.b.offset = fb->offset / 8;
		return 0;
	}

	/* JMP_GT_MI, JMP_GT_HI. */
	b_val = strtoull(b, &b, 0);
	CHECK(!b[0], EINVAL);

	instr->type = INSTR_JMP_GT_MI;
	if (a[0] == 'h')
		instr->type = INSTR_JMP_GT_HI;
	instr->jmp.ip = NULL; /* Resolved later. */

	instr->jmp.a.struct_id = (uint8_t)a_struct_id;
	instr->jmp.a.n_bits = fa->n_bits;
	instr->jmp.a.offset = fa->offset / 8;
	instr->jmp.b_val = b_val;
	return 0;
}

static inline void
instr_jmp_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmp\n", p->thread_id);

	thread_ip_set(t, ip->jmp.ip);
}

static inline void
instr_jmp_valid_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t header_id = ip->jmp.header_id;

	TRACE("[Thread %2u] jmpv\n", p->thread_id);

	t->ip = HEADER_VALID(t, header_id) ? ip->jmp.ip : (t->ip + 1);
}

static inline void
instr_jmp_invalid_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint32_t header_id = ip->jmp.header_id;

	TRACE("[Thread %2u] jmpnv\n", p->thread_id);

	t->ip = HEADER_VALID(t, header_id) ? (t->ip + 1) : ip->jmp.ip;
}

static inline void
instr_jmp_hit_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct instruction *ip_next[] = {t->ip + 1, ip->jmp.ip};

	TRACE("[Thread %2u] jmph\n", p->thread_id);

	t->ip = ip_next[t->hit];
}

static inline void
instr_jmp_miss_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct instruction *ip_next[] = {ip->jmp.ip, t->ip + 1};

	TRACE("[Thread %2u] jmpnh\n", p->thread_id);

	t->ip = ip_next[t->hit];
}

static inline void
instr_jmp_action_hit_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpa\n", p->thread_id);

	t->ip = (ip->jmp.action_id == t->action_id) ? ip->jmp.ip : (t->ip + 1);
}

static inline void
instr_jmp_action_miss_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpna\n", p->thread_id);

	t->ip = (ip->jmp.action_id == t->action_id) ? (t->ip + 1) : ip->jmp.ip;
}

static inline void
instr_jmp_eq_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpeq\n", p->thread_id);

	JMP_CMP(t, ip, ==);
}

static inline void
instr_jmp_eq_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpeq (mh)\n", p->thread_id);

	JMP_CMP_MH(t, ip, ==);
}

static inline void
instr_jmp_eq_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpeq (hm)\n", p->thread_id);

	JMP_CMP_HM(t, ip, ==);
}

static inline void
instr_jmp_eq_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpeq (hh)\n", p->thread_id);

	JMP_CMP_HH_FAST(t, ip, ==);
}

static inline void
instr_jmp_eq_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpeq (i)\n", p->thread_id);

	JMP_CMP_I(t, ip, ==);
}

static inline void
instr_jmp_neq_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpneq\n", p->thread_id);

	JMP_CMP(t, ip, !=);
}

static inline void
instr_jmp_neq_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpneq (mh)\n", p->thread_id);

	JMP_CMP_MH(t, ip, !=);
}

static inline void
instr_jmp_neq_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpneq (hm)\n", p->thread_id);

	JMP_CMP_HM(t, ip, !=);
}

static inline void
instr_jmp_neq_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpneq (hh)\n", p->thread_id);

	JMP_CMP_HH_FAST(t, ip, !=);
}

static inline void
instr_jmp_neq_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpneq (i)\n", p->thread_id);

	JMP_CMP_I(t, ip, !=);
}

static inline void
instr_jmp_lt_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmplt\n", p->thread_id);

	JMP_CMP(t, ip, <);
}

static inline void
instr_jmp_lt_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmplt (mh)\n", p->thread_id);

	JMP_CMP_MH(t, ip, <);
}

static inline void
instr_jmp_lt_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmplt (hm)\n", p->thread_id);

	JMP_CMP_HM(t, ip, <);
}

static inline void
instr_jmp_lt_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmplt (hh)\n", p->thread_id);

	JMP_CMP_HH(t, ip, <);
}

static inline void
instr_jmp_lt_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmplt (mi)\n", p->thread_id);

	JMP_CMP_MI(t, ip, <);
}

static inline void
instr_jmp_lt_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmplt (hi)\n", p->thread_id);

	JMP_CMP_HI(t, ip, <);
}

static inline void
instr_jmp_gt_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpgt\n", p->thread_id);

	JMP_CMP(t, ip, >);
}

static inline void
instr_jmp_gt_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpgt (mh)\n", p->thread_id);

	JMP_CMP_MH(t, ip, >);
}

static inline void
instr_jmp_gt_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpgt (hm)\n", p->thread_id);

	JMP_CMP_HM(t, ip, >);
}

static inline void
instr_jmp_gt_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpgt (hh)\n", p->thread_id);

	JMP_CMP_HH(t, ip, >);
}

static inline void
instr_jmp_gt_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpgt (mi)\n", p->thread_id);

	JMP_CMP_MI(t, ip, >);
}

static inline void
instr_jmp_gt_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] jmpgt (hi)\n", p->thread_id);

	JMP_CMP_HI(t, ip, >);
}

/*
 * return.
 */
static int
instr_return_translate(struct rte_swx_pipeline *p __rte_unused,
		       struct action *action,
		       char **tokens __rte_unused,
		       int n_tokens,
		       struct instruction *instr,
		       struct instruction_data *data __rte_unused)
{
	CHECK(action, EINVAL);
	CHECK(n_tokens == 1, EINVAL);

	instr->type = INSTR_RETURN;
	return 0;
}

static inline void
instr_return_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];

	TRACE("[Thread %2u] return\n", p->thread_id);

	t->ip = t->ret;
}

static int
instr_translate(struct rte_swx_pipeline *p,
		struct action *action,
		char *string,
		struct instruction *instr,
		struct instruction_data *data)
{
	char *tokens[RTE_SWX_INSTRUCTION_TOKENS_MAX];
	int n_tokens = 0, tpos = 0;

	/* Parse the instruction string into tokens. */
	for ( ; ; ) {
		char *token;

		token = strtok_r(string, " \t\v", &string);
		if (!token)
			break;

		CHECK(n_tokens < RTE_SWX_INSTRUCTION_TOKENS_MAX, EINVAL);
		CHECK_NAME(token, EINVAL);

		tokens[n_tokens] = token;
		n_tokens++;
	}

	CHECK(n_tokens, EINVAL);

	/* Handle the optional instruction label. */
	if ((n_tokens >= 2) && !strcmp(tokens[1], ":")) {
		strcpy(data->label, tokens[0]);

		tpos += 2;
		CHECK(n_tokens - tpos, EINVAL);
	}

	/* Identify the instruction type. */
	if (!strcmp(tokens[tpos], "rx"))
		return instr_rx_translate(p,
					  action,
					  &tokens[tpos],
					  n_tokens - tpos,
					  instr,
					  data);

	if (!strcmp(tokens[tpos], "tx"))
		return instr_tx_translate(p,
					  action,
					  &tokens[tpos],
					  n_tokens - tpos,
					  instr,
					  data);

	if (!strcmp(tokens[tpos], "drop"))
		return instr_drop_translate(p,
					    action,
					    &tokens[tpos],
					    n_tokens - tpos,
					    instr,
					    data);

	if (!strcmp(tokens[tpos], "mirror"))
		return instr_mirror_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "recirculate"))
		return instr_recirculate_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "recircid"))
		return instr_recircid_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "extract"))
		return instr_hdr_extract_translate(p,
						   action,
						   &tokens[tpos],
						   n_tokens - tpos,
						   instr,
						   data);

	if (!strcmp(tokens[tpos], "lookahead"))
		return instr_hdr_lookahead_translate(p,
						     action,
						     &tokens[tpos],
						     n_tokens - tpos,
						     instr,
						     data);

	if (!strcmp(tokens[tpos], "emit"))
		return instr_hdr_emit_translate(p,
						action,
						&tokens[tpos],
						n_tokens - tpos,
						instr,
						data);

	if (!strcmp(tokens[tpos], "validate"))
		return instr_hdr_validate_translate(p,
						    action,
						    &tokens[tpos],
						    n_tokens - tpos,
						    instr,
						    data);

	if (!strcmp(tokens[tpos], "invalidate"))
		return instr_hdr_invalidate_translate(p,
						      action,
						      &tokens[tpos],
						      n_tokens - tpos,
						      instr,
						      data);

	if (!strcmp(tokens[tpos], "mov"))
		return instr_mov_translate(p,
					   action,
					   &tokens[tpos],
					   n_tokens - tpos,
					   instr,
					   data);

	if (!strcmp(tokens[tpos], "add"))
		return instr_alu_add_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "sub"))
		return instr_alu_sub_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "ckadd"))
		return instr_alu_ckadd_translate(p,
						 action,
						 &tokens[tpos],
						 n_tokens - tpos,
						 instr,
						 data);

	if (!strcmp(tokens[tpos], "cksub"))
		return instr_alu_cksub_translate(p,
						 action,
						 &tokens[tpos],
						 n_tokens - tpos,
						 instr,
						 data);

	if (!strcmp(tokens[tpos], "and"))
		return instr_alu_and_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "or"))
		return instr_alu_or_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "xor"))
		return instr_alu_xor_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "shl"))
		return instr_alu_shl_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "shr"))
		return instr_alu_shr_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "regprefetch"))
		return instr_regprefetch_translate(p,
						   action,
						   &tokens[tpos],
						   n_tokens - tpos,
						   instr,
						   data);

	if (!strcmp(tokens[tpos], "regrd"))
		return instr_regrd_translate(p,
					     action,
					     &tokens[tpos],
					     n_tokens - tpos,
					     instr,
					     data);

	if (!strcmp(tokens[tpos], "regwr"))
		return instr_regwr_translate(p,
					     action,
					     &tokens[tpos],
					     n_tokens - tpos,
					     instr,
					     data);

	if (!strcmp(tokens[tpos], "regadd"))
		return instr_regadd_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "metprefetch"))
		return instr_metprefetch_translate(p,
						   action,
						   &tokens[tpos],
						   n_tokens - tpos,
						   instr,
						   data);

	if (!strcmp(tokens[tpos], "meter"))
		return instr_meter_translate(p,
					     action,
					     &tokens[tpos],
					     n_tokens - tpos,
					     instr,
					     data);

	if (!strcmp(tokens[tpos], "table"))
		return instr_table_translate(p,
					     action,
					     &tokens[tpos],
					     n_tokens - tpos,
					     instr,
					     data);

	if (!strcmp(tokens[tpos], "learn"))
		return instr_learn_translate(p,
					     action,
					     &tokens[tpos],
					     n_tokens - tpos,
					     instr,
					     data);
	if (!strcmp(tokens[tpos], "rearm"))
		return instr_rearm_translate(p,
					     action,
					     &tokens[tpos],
					     n_tokens - tpos,
					     instr,
					     data);

	if (!strcmp(tokens[tpos], "forget"))
		return instr_forget_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "entryid"))
		return instr_entryid_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "extern"))
		return instr_extern_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "hash"))
		return instr_hash_translate(p,
					    action,
					    &tokens[tpos],
					    n_tokens - tpos,
					    instr,
					    data);

	if (!strcmp(tokens[tpos], "rss"))
		return instr_rss_translate(p,
					   action,
					   &tokens[tpos],
					   n_tokens - tpos,
					   instr,
					   data);

	if (!strcmp(tokens[tpos], "jmp"))
		return instr_jmp_translate(p,
					   action,
					   &tokens[tpos],
					   n_tokens - tpos,
					   instr,
					   data);

	if (!strcmp(tokens[tpos], "jmpv"))
		return instr_jmp_valid_translate(p,
						 action,
						 &tokens[tpos],
						 n_tokens - tpos,
						 instr,
						 data);

	if (!strcmp(tokens[tpos], "jmpnv"))
		return instr_jmp_invalid_translate(p,
						   action,
						   &tokens[tpos],
						   n_tokens - tpos,
						   instr,
						   data);

	if (!strcmp(tokens[tpos], "jmph"))
		return instr_jmp_hit_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "jmpnh"))
		return instr_jmp_miss_translate(p,
						action,
						&tokens[tpos],
						n_tokens - tpos,
						instr,
						data);

	if (!strcmp(tokens[tpos], "jmpa"))
		return instr_jmp_action_hit_translate(p,
						      action,
						      &tokens[tpos],
						      n_tokens - tpos,
						      instr,
						      data);

	if (!strcmp(tokens[tpos], "jmpna"))
		return instr_jmp_action_miss_translate(p,
						       action,
						       &tokens[tpos],
						       n_tokens - tpos,
						       instr,
						       data);

	if (!strcmp(tokens[tpos], "jmpeq"))
		return instr_jmp_eq_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "jmpneq"))
		return instr_jmp_neq_translate(p,
					       action,
					       &tokens[tpos],
					       n_tokens - tpos,
					       instr,
					       data);

	if (!strcmp(tokens[tpos], "jmplt"))
		return instr_jmp_lt_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "jmpgt"))
		return instr_jmp_gt_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	if (!strcmp(tokens[tpos], "return"))
		return instr_return_translate(p,
					      action,
					      &tokens[tpos],
					      n_tokens - tpos,
					      instr,
					      data);

	return -EINVAL;
}

static struct instruction_data *
label_find(struct instruction_data *data, uint32_t n, const char *label)
{
	uint32_t i;

	for (i = 0; i < n; i++)
		if (!strcmp(label, data[i].label))
			return &data[i];

	return NULL;
}

static uint32_t
label_is_used(struct instruction_data *data, uint32_t n, const char *label)
{
	uint32_t count = 0, i;

	if (!label[0])
		return 0;

	for (i = 0; i < n; i++)
		if (!strcmp(label, data[i].jmp_label))
			count++;

	return count;
}

static int
instr_label_check(struct instruction_data *instruction_data,
		  uint32_t n_instructions)
{
	uint32_t i;

	/* Check that all instruction labels are unique. */
	for (i = 0; i < n_instructions; i++) {
		struct instruction_data *data = &instruction_data[i];
		char *label = data->label;
		uint32_t j;

		if (!label[0])
			continue;

		for (j = i + 1; j < n_instructions; j++)
			CHECK(strcmp(label, instruction_data[j].label), EINVAL);
	}

	/* Check that no jump instruction (either conditional or not) can jump to itself (loop). */
	for (i = 0; i < n_instructions; i++) {
		struct instruction_data *data = &instruction_data[i];
		char *label = data->label;
		char *jmp_label = data->jmp_label;

		/* Continue if this instruction does not have a label or it is not a jump. */
		if (!label[0] || !jmp_label[0])
			continue;

		CHECK(strcmp(label, jmp_label), EINVAL);
	}

	/* Get users for each instruction label. */
	for (i = 0; i < n_instructions; i++) {
		struct instruction_data *data = &instruction_data[i];
		char *label = data->label;

		data->n_users = label_is_used(instruction_data,
					      n_instructions,
					      label);
	}

	return 0;
}

static int
instr_jmp_resolve(struct instruction *instructions,
		  struct instruction_data *instruction_data,
		  uint32_t n_instructions)
{
	uint32_t i;

	for (i = 0; i < n_instructions; i++) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];
		struct instruction_data *found;

		if (!instruction_is_jmp(instr))
			continue;

		found = label_find(instruction_data,
				   n_instructions,
				   data->jmp_label);
		CHECK(found, EINVAL);

		instr->jmp.ip = &instructions[found - instruction_data];
	}

	return 0;
}

static int
instr_verify(struct rte_swx_pipeline *p __rte_unused,
	     struct action *a,
	     struct instruction *instr,
	     struct instruction_data *data __rte_unused,
	     uint32_t n_instructions)
{
	if (!a) {
		enum instruction_type type;
		uint32_t i;

		/* Check that the first instruction is rx. */
		CHECK(instr[0].type == INSTR_RX, EINVAL);

		/* Check that there is at least one tx instruction. */
		for (i = 0; i < n_instructions; i++) {
			type = instr[i].type;

			if (instruction_is_tx(type))
				break;
		}
		CHECK(i < n_instructions, EINVAL);

		/* Check that the last instruction is either tx or unconditional
		 * jump.
		 */
		type = instr[n_instructions - 1].type;
		CHECK(instruction_is_tx(type) || (type == INSTR_JMP), EINVAL);
	}

	if (a) {
		enum instruction_type type;
		uint32_t i;

		/* Check that there is at least one return or tx instruction. */
		for (i = 0; i < n_instructions; i++) {
			type = instr[i].type;

			if ((type == INSTR_RETURN) || instruction_is_tx(type))
				break;
		}
		CHECK(i < n_instructions, EINVAL);
	}

	return 0;
}

static uint32_t
instr_compact(struct instruction *instructions,
	      struct instruction_data *instruction_data,
	      uint32_t n_instructions)
{
	uint32_t i, pos = 0;

	/* Eliminate the invalid instructions that have been optimized out. */
	for (i = 0; i < n_instructions; i++) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];

		if (data->invalid)
			continue;

		if (i != pos) {
			memcpy(&instructions[pos], instr, sizeof(*instr));
			memcpy(&instruction_data[pos], data, sizeof(*data));
		}

		pos++;
	}

	return pos;
}

static int
instr_pattern_extract_many_search(struct instruction *instr,
				  struct instruction_data *data,
				  uint32_t n_instr,
				  uint32_t *n_pattern_instr)
{
	uint32_t i;

	for (i = 0; i < n_instr; i++) {
		if (data[i].invalid)
			break;

		if (instr[i].type != INSTR_HDR_EXTRACT)
			break;

		if (i == RTE_DIM(instr->io.hdr.header_id))
			break;

		if (i && data[i].n_users)
			break;
	}

	if (i < 2)
		return 0;

	*n_pattern_instr = i;
	return 1;
}

static void
instr_pattern_extract_many_replace(struct instruction *instr,
				   struct instruction_data *data,
				   uint32_t n_instr)
{
	uint32_t i;

	for (i = 1; i < n_instr; i++) {
		instr[0].type++;
		instr[0].io.hdr.header_id[i] = instr[i].io.hdr.header_id[0];
		instr[0].io.hdr.struct_id[i] = instr[i].io.hdr.struct_id[0];
		instr[0].io.hdr.n_bytes[i] = instr[i].io.hdr.n_bytes[0];

		data[i].invalid = 1;
	}
}

static uint32_t
instr_pattern_extract_many_optimize(struct instruction *instructions,
				    struct instruction_data *instruction_data,
				    uint32_t n_instructions)
{
	uint32_t i;

	for (i = 0; i < n_instructions; ) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];
		uint32_t n_instr = 0;
		int detected;

		/* Extract many. */
		detected = instr_pattern_extract_many_search(instr,
							     data,
							     n_instructions - i,
							     &n_instr);
		if (detected) {
			instr_pattern_extract_many_replace(instr,
							   data,
							   n_instr);
			i += n_instr;
			continue;
		}

		/* No pattern starting at the current instruction. */
		i++;
	}

	/* Eliminate the invalid instructions that have been optimized out. */
	n_instructions = instr_compact(instructions,
				       instruction_data,
				       n_instructions);

	return n_instructions;
}

static int
instr_pattern_emit_many_tx_search(struct instruction *instr,
				  struct instruction_data *data,
				  uint32_t n_instr,
				  uint32_t *n_pattern_instr)
{
	uint32_t i;

	for (i = 0; i < n_instr; i++) {
		if (data[i].invalid)
			break;

		if (instr[i].type != INSTR_HDR_EMIT)
			break;

		if (i == RTE_DIM(instr->io.hdr.header_id))
			break;

		if (i && data[i].n_users)
			break;
	}

	if (!i)
		return 0;

	if (instr[i].type != INSTR_TX)
		return 0;

	if (data[i].n_users)
		return 0;

	i++;

	*n_pattern_instr = i;
	return 1;
}

static void
instr_pattern_emit_many_tx_replace(struct instruction *instr,
				   struct instruction_data *data,
				   uint32_t n_instr)
{
	uint32_t i;

	/* Any emit instruction in addition to the first one. */
	for (i = 1; i < n_instr - 1; i++) {
		instr[0].type++;
		instr[0].io.hdr.header_id[i] = instr[i].io.hdr.header_id[0];
		instr[0].io.hdr.struct_id[i] = instr[i].io.hdr.struct_id[0];
		instr[0].io.hdr.n_bytes[i] = instr[i].io.hdr.n_bytes[0];

		data[i].invalid = 1;
	}

	/* The TX instruction is the last one in the pattern. */
	instr[0].type++;
	instr[0].io.io.offset = instr[i].io.io.offset;
	instr[0].io.io.n_bits = instr[i].io.io.n_bits;
	data[i].invalid = 1;
}

static uint32_t
instr_pattern_emit_many_tx_optimize(struct instruction *instructions,
				    struct instruction_data *instruction_data,
				    uint32_t n_instructions)
{
	uint32_t i;

	for (i = 0; i < n_instructions; ) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];
		uint32_t n_instr = 0;
		int detected;

		/* Emit many + TX. */
		detected = instr_pattern_emit_many_tx_search(instr,
							     data,
							     n_instructions - i,
							     &n_instr);
		if (detected) {
			instr_pattern_emit_many_tx_replace(instr,
							   data,
							   n_instr);
			i += n_instr;
			continue;
		}

		/* No pattern starting at the current instruction. */
		i++;
	}

	/* Eliminate the invalid instructions that have been optimized out. */
	n_instructions = instr_compact(instructions,
				       instruction_data,
				       n_instructions);

	return n_instructions;
}

static uint32_t
action_arg_src_mov_count(struct action *a,
			 uint32_t arg_id,
			 struct instruction *instructions,
			 struct instruction_data *instruction_data,
			 uint32_t n_instructions);

static int
instr_pattern_validate_mov_all_search(struct rte_swx_pipeline *p,
				      struct action *a,
				      struct instruction *instr,
				      struct instruction_data *data,
				      uint32_t n_instr,
				      struct instruction *instructions,
				      struct instruction_data *instruction_data,
				      uint32_t n_instructions,
				      uint32_t *n_pattern_instr)
{
	struct header *h;
	uint32_t src_field_id, i, j;

	/* Prerequisites. */
	if (!a || !a->st)
		return 0;

	/* First instruction: HDR_VALIDATE. Second instruction: MOV_HM, MOV_DMA or MOV_128. */
	if (data[0].invalid ||
	    (instr[0].type != INSTR_HDR_VALIDATE) ||
	    (n_instr < 2) ||
	    data[1].invalid ||
	    (instr[1].type != INSTR_MOV_HM &&
	     instr[1].type != INSTR_MOV_DMA &&
	     instr[1].type != INSTR_MOV_128) ||
	    instr[1].mov.src.struct_id)
		return 0;

	h = header_find_by_struct_id(p, instr[0].valid.struct_id);
	if (!h ||
	    h->st->var_size ||
	    (n_instr < 1 + h->st->n_fields))
		return 0;

	for (src_field_id = 0; src_field_id < a->st->n_fields; src_field_id++)
		if (instr[1].mov.src.offset == a->st->fields[src_field_id].offset / 8)
			break;

	if (src_field_id + h->st->n_fields > a->st->n_fields)
		return 0;

	/* Second and subsequent instructions: MOV_HM. */
	for (i = 0; i < h->st->n_fields; i++)
		if (data[1 + i].invalid ||
		    data[1 + i].n_users ||
		    (instr[1 + i].type != INSTR_MOV_HM &&
		     instr[1 + i].type != INSTR_MOV_DMA &&
		     instr[1 + i].type != INSTR_MOV_128) ||
		    (instr[1 + i].mov.dst.struct_id != h->struct_id) ||
		    (instr[1 + i].mov.dst.offset != h->st->fields[i].offset / 8) ||
		    (instr[1 + i].mov.dst.n_bits != h->st->fields[i].n_bits) ||
		    instr[1 + i].mov.src.struct_id ||
		    (instr[1 + i].mov.src.offset != a->st->fields[src_field_id + i].offset / 8) ||
		    (instr[1 + i].mov.src.n_bits != a->st->fields[src_field_id + i].n_bits) ||
		    (instr[1 + i].mov.dst.n_bits != instr[1 + i].mov.src.n_bits))
			return 0;

	/* Check that none of the action args that are used as source for this
	 * DMA transfer are not used as source in any other mov instruction.
	 */
	for (j = src_field_id; j < src_field_id + h->st->n_fields; j++) {
		uint32_t n_users;

		n_users = action_arg_src_mov_count(a,
						   j,
						   instructions,
						   instruction_data,
						   n_instructions);
		if (n_users > 1)
			return 0;
	}

	*n_pattern_instr = 1 + h->st->n_fields;
	return 1;
}

static void
instr_pattern_validate_mov_all_replace(struct rte_swx_pipeline *p,
				       struct action *a,
				       struct instruction *instr,
				       struct instruction_data *data,
				       uint32_t n_instr)
{
	struct header *h;
	uint32_t src_field_id, src_offset, i;

	/* Read from the instructions before they are modified. */
	h = header_find_by_struct_id(p, instr[1].mov.dst.struct_id);
	if (!h)
		return;

	src_offset = instr[1].mov.src.offset;

	for (src_field_id = 0; src_field_id < a->st->n_fields; src_field_id++)
		if (src_offset == a->st->fields[src_field_id].offset / 8)
			break;

	/* Modify the instructions. */
	instr[0].type = INSTR_DMA_HT;
	instr[0].dma.dst.header_id[0] = h->id;
	instr[0].dma.dst.struct_id[0] = h->struct_id;
	instr[0].dma.src.offset[0] = (uint8_t)src_offset;
	instr[0].dma.n_bytes[0] = h->st->n_bits / 8;

	for (i = 1; i < n_instr; i++)
		data[i].invalid = 1;

	/* Update the endianness of the action arguments to header endianness. */
	for (i = 0; i < h->st->n_fields; i++)
		a->args_endianness[src_field_id + i] = 1;
}

static uint32_t
instr_pattern_validate_mov_all_optimize(struct rte_swx_pipeline *p,
					struct action *a,
					struct instruction *instructions,
					struct instruction_data *instruction_data,
					uint32_t n_instructions)
{
	uint32_t i;

	if (!a || !a->st)
		return n_instructions;

	for (i = 0; i < n_instructions; ) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];
		uint32_t n_instr = 0;
		int detected;

		/* Validate + mov all. */
		detected = instr_pattern_validate_mov_all_search(p,
								 a,
								 instr,
								 data,
								 n_instructions - i,
								 instructions,
								 instruction_data,
								 n_instructions,
								 &n_instr);
		if (detected) {
			instr_pattern_validate_mov_all_replace(p, a, instr, data, n_instr);
			i += n_instr;
			continue;
		}

		/* No pattern starting at the current instruction. */
		i++;
	}

	/* Eliminate the invalid instructions that have been optimized out. */
	n_instructions = instr_compact(instructions,
				       instruction_data,
				       n_instructions);

	return n_instructions;
}

static int
instr_pattern_dma_many_search(struct instruction *instr,
			      struct instruction_data *data,
			      uint32_t n_instr,
			      uint32_t *n_pattern_instr)
{
	uint32_t i;

	for (i = 0; i < n_instr; i++) {
		if (data[i].invalid)
			break;

		if (instr[i].type != INSTR_DMA_HT)
			break;

		if (i == RTE_DIM(instr->dma.dst.header_id))
			break;

		if (i && data[i].n_users)
			break;
	}

	if (i < 2)
		return 0;

	*n_pattern_instr = i;
	return 1;
}

static void
instr_pattern_dma_many_replace(struct instruction *instr,
			       struct instruction_data *data,
			       uint32_t n_instr)
{
	uint32_t i;

	for (i = 1; i < n_instr; i++) {
		instr[0].type++;
		instr[0].dma.dst.header_id[i] = instr[i].dma.dst.header_id[0];
		instr[0].dma.dst.struct_id[i] = instr[i].dma.dst.struct_id[0];
		instr[0].dma.src.offset[i] = instr[i].dma.src.offset[0];
		instr[0].dma.n_bytes[i] = instr[i].dma.n_bytes[0];

		data[i].invalid = 1;
	}
}

static uint32_t
instr_pattern_dma_many_optimize(struct instruction *instructions,
	       struct instruction_data *instruction_data,
	       uint32_t n_instructions)
{
	uint32_t i;

	for (i = 0; i < n_instructions; ) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];
		uint32_t n_instr = 0;
		int detected;

		/* DMA many. */
		detected = instr_pattern_dma_many_search(instr,
							 data,
							 n_instructions - i,
							 &n_instr);
		if (detected) {
			instr_pattern_dma_many_replace(instr, data, n_instr);
			i += n_instr;
			continue;
		}

		/* No pattern starting at the current instruction. */
		i++;
	}

	/* Eliminate the invalid instructions that have been optimized out. */
	n_instructions = instr_compact(instructions,
				       instruction_data,
				       n_instructions);

	return n_instructions;
}

static uint32_t
instr_optimize(struct rte_swx_pipeline *p,
	       struct action *a,
	       struct instruction *instructions,
	       struct instruction_data *instruction_data,
	       uint32_t n_instructions)
{
	/* Extract many. */
	n_instructions = instr_pattern_extract_many_optimize(instructions,
							     instruction_data,
							     n_instructions);

	/* Emit many + TX. */
	n_instructions = instr_pattern_emit_many_tx_optimize(instructions,
							     instruction_data,
							     n_instructions);

	/* Validate + mov all. */
	n_instructions = instr_pattern_validate_mov_all_optimize(p,
								 a,
								 instructions,
								 instruction_data,
								 n_instructions);

	/* DMA many. */
	n_instructions = instr_pattern_dma_many_optimize(instructions,
							 instruction_data,
							 n_instructions);

	return n_instructions;
}

static int
instruction_config(struct rte_swx_pipeline *p,
		   struct action *a,
		   const char **instructions,
		   uint32_t n_instructions)
{
	struct instruction *instr = NULL;
	struct instruction_data *data = NULL;
	int err = 0;
	uint32_t i;

	CHECK(n_instructions, EINVAL);
	CHECK(instructions, EINVAL);
	for (i = 0; i < n_instructions; i++)
		CHECK_INSTRUCTION(instructions[i], EINVAL);

	/* Memory allocation. */
	instr = calloc(n_instructions, sizeof(struct instruction));
	if (!instr) {
		err = -ENOMEM;
		goto error;
	}

	data = calloc(n_instructions, sizeof(struct instruction_data));
	if (!data) {
		err = -ENOMEM;
		goto error;
	}

	for (i = 0; i < n_instructions; i++) {
		char *string = strdup(instructions[i]);
		if (!string) {
			err = -ENOMEM;
			goto error;
		}

		err = instr_translate(p, a, string, &instr[i], &data[i]);
		if (err) {
			free(string);
			goto error;
		}

		free(string);
	}

	err = instr_label_check(data, n_instructions);
	if (err)
		goto error;

	err = instr_verify(p, a, instr, data, n_instructions);
	if (err)
		goto error;

	n_instructions = instr_optimize(p, a, instr, data, n_instructions);

	err = instr_jmp_resolve(instr, data, n_instructions);
	if (err)
		goto error;

	if (a) {
		a->instructions = instr;
		a->instruction_data = data;
		a->n_instructions = n_instructions;
	} else {
		p->instructions = instr;
		p->instruction_data = data;
		p->n_instructions = n_instructions;
	}

	return 0;

error:
	free(data);
	free(instr);
	return err;
}

static instr_exec_t instruction_table[] = {
	[INSTR_RX] = instr_rx_exec,
	[INSTR_TX] = instr_tx_exec,
	[INSTR_TX_I] = instr_tx_i_exec,
	[INSTR_DROP] = instr_drop_exec,
	[INSTR_MIRROR] = instr_mirror_exec,
	[INSTR_RECIRCULATE] = instr_recirculate_exec,
	[INSTR_RECIRCID] = instr_recircid_exec,

	[INSTR_HDR_EXTRACT] = instr_hdr_extract_exec,
	[INSTR_HDR_EXTRACT2] = instr_hdr_extract2_exec,
	[INSTR_HDR_EXTRACT3] = instr_hdr_extract3_exec,
	[INSTR_HDR_EXTRACT4] = instr_hdr_extract4_exec,
	[INSTR_HDR_EXTRACT5] = instr_hdr_extract5_exec,
	[INSTR_HDR_EXTRACT6] = instr_hdr_extract6_exec,
	[INSTR_HDR_EXTRACT7] = instr_hdr_extract7_exec,
	[INSTR_HDR_EXTRACT8] = instr_hdr_extract8_exec,
	[INSTR_HDR_EXTRACT_M] = instr_hdr_extract_m_exec,
	[INSTR_HDR_LOOKAHEAD] = instr_hdr_lookahead_exec,

	[INSTR_HDR_EMIT] = instr_hdr_emit_exec,
	[INSTR_HDR_EMIT_TX] = instr_hdr_emit_tx_exec,
	[INSTR_HDR_EMIT2_TX] = instr_hdr_emit2_tx_exec,
	[INSTR_HDR_EMIT3_TX] = instr_hdr_emit3_tx_exec,
	[INSTR_HDR_EMIT4_TX] = instr_hdr_emit4_tx_exec,
	[INSTR_HDR_EMIT5_TX] = instr_hdr_emit5_tx_exec,
	[INSTR_HDR_EMIT6_TX] = instr_hdr_emit6_tx_exec,
	[INSTR_HDR_EMIT7_TX] = instr_hdr_emit7_tx_exec,
	[INSTR_HDR_EMIT8_TX] = instr_hdr_emit8_tx_exec,

	[INSTR_HDR_VALIDATE] = instr_hdr_validate_exec,
	[INSTR_HDR_INVALIDATE] = instr_hdr_invalidate_exec,

	[INSTR_MOV] = instr_mov_exec,
	[INSTR_MOV_MH] = instr_mov_mh_exec,
	[INSTR_MOV_HM] = instr_mov_hm_exec,
	[INSTR_MOV_HH] = instr_mov_hh_exec,
	[INSTR_MOV_DMA] = instr_mov_dma_exec,
	[INSTR_MOV_128] = instr_mov_128_exec,
	[INSTR_MOV_128_32] = instr_mov_128_32_exec,
	[INSTR_MOV_I] = instr_mov_i_exec,

	[INSTR_DMA_HT] = instr_dma_ht_exec,
	[INSTR_DMA_HT2] = instr_dma_ht2_exec,
	[INSTR_DMA_HT3] = instr_dma_ht3_exec,
	[INSTR_DMA_HT4] = instr_dma_ht4_exec,
	[INSTR_DMA_HT5] = instr_dma_ht5_exec,
	[INSTR_DMA_HT6] = instr_dma_ht6_exec,
	[INSTR_DMA_HT7] = instr_dma_ht7_exec,
	[INSTR_DMA_HT8] = instr_dma_ht8_exec,

	[INSTR_ALU_ADD] = instr_alu_add_exec,
	[INSTR_ALU_ADD_MH] = instr_alu_add_mh_exec,
	[INSTR_ALU_ADD_HM] = instr_alu_add_hm_exec,
	[INSTR_ALU_ADD_HH] = instr_alu_add_hh_exec,
	[INSTR_ALU_ADD_MI] = instr_alu_add_mi_exec,
	[INSTR_ALU_ADD_HI] = instr_alu_add_hi_exec,

	[INSTR_ALU_SUB] = instr_alu_sub_exec,
	[INSTR_ALU_SUB_MH] = instr_alu_sub_mh_exec,
	[INSTR_ALU_SUB_HM] = instr_alu_sub_hm_exec,
	[INSTR_ALU_SUB_HH] = instr_alu_sub_hh_exec,
	[INSTR_ALU_SUB_MI] = instr_alu_sub_mi_exec,
	[INSTR_ALU_SUB_HI] = instr_alu_sub_hi_exec,

	[INSTR_ALU_CKADD_FIELD] = instr_alu_ckadd_field_exec,
	[INSTR_ALU_CKADD_STRUCT] = instr_alu_ckadd_struct_exec,
	[INSTR_ALU_CKADD_STRUCT20] = instr_alu_ckadd_struct20_exec,
	[INSTR_ALU_CKSUB_FIELD] = instr_alu_cksub_field_exec,

	[INSTR_ALU_AND] = instr_alu_and_exec,
	[INSTR_ALU_AND_MH] = instr_alu_and_mh_exec,
	[INSTR_ALU_AND_HM] = instr_alu_and_hm_exec,
	[INSTR_ALU_AND_HH] = instr_alu_and_hh_exec,
	[INSTR_ALU_AND_I] = instr_alu_and_i_exec,

	[INSTR_ALU_OR] = instr_alu_or_exec,
	[INSTR_ALU_OR_MH] = instr_alu_or_mh_exec,
	[INSTR_ALU_OR_HM] = instr_alu_or_hm_exec,
	[INSTR_ALU_OR_HH] = instr_alu_or_hh_exec,
	[INSTR_ALU_OR_I] = instr_alu_or_i_exec,

	[INSTR_ALU_XOR] = instr_alu_xor_exec,
	[INSTR_ALU_XOR_MH] = instr_alu_xor_mh_exec,
	[INSTR_ALU_XOR_HM] = instr_alu_xor_hm_exec,
	[INSTR_ALU_XOR_HH] = instr_alu_xor_hh_exec,
	[INSTR_ALU_XOR_I] = instr_alu_xor_i_exec,

	[INSTR_ALU_SHL] = instr_alu_shl_exec,
	[INSTR_ALU_SHL_MH] = instr_alu_shl_mh_exec,
	[INSTR_ALU_SHL_HM] = instr_alu_shl_hm_exec,
	[INSTR_ALU_SHL_HH] = instr_alu_shl_hh_exec,
	[INSTR_ALU_SHL_MI] = instr_alu_shl_mi_exec,
	[INSTR_ALU_SHL_HI] = instr_alu_shl_hi_exec,

	[INSTR_ALU_SHR] = instr_alu_shr_exec,
	[INSTR_ALU_SHR_MH] = instr_alu_shr_mh_exec,
	[INSTR_ALU_SHR_HM] = instr_alu_shr_hm_exec,
	[INSTR_ALU_SHR_HH] = instr_alu_shr_hh_exec,
	[INSTR_ALU_SHR_MI] = instr_alu_shr_mi_exec,
	[INSTR_ALU_SHR_HI] = instr_alu_shr_hi_exec,

	[INSTR_REGPREFETCH_RH] = instr_regprefetch_rh_exec,
	[INSTR_REGPREFETCH_RM] = instr_regprefetch_rm_exec,
	[INSTR_REGPREFETCH_RI] = instr_regprefetch_ri_exec,

	[INSTR_REGRD_HRH] = instr_regrd_hrh_exec,
	[INSTR_REGRD_HRM] = instr_regrd_hrm_exec,
	[INSTR_REGRD_MRH] = instr_regrd_mrh_exec,
	[INSTR_REGRD_MRM] = instr_regrd_mrm_exec,
	[INSTR_REGRD_HRI] = instr_regrd_hri_exec,
	[INSTR_REGRD_MRI] = instr_regrd_mri_exec,

	[INSTR_REGWR_RHH] = instr_regwr_rhh_exec,
	[INSTR_REGWR_RHM] = instr_regwr_rhm_exec,
	[INSTR_REGWR_RMH] = instr_regwr_rmh_exec,
	[INSTR_REGWR_RMM] = instr_regwr_rmm_exec,
	[INSTR_REGWR_RHI] = instr_regwr_rhi_exec,
	[INSTR_REGWR_RMI] = instr_regwr_rmi_exec,
	[INSTR_REGWR_RIH] = instr_regwr_rih_exec,
	[INSTR_REGWR_RIM] = instr_regwr_rim_exec,
	[INSTR_REGWR_RII] = instr_regwr_rii_exec,

	[INSTR_REGADD_RHH] = instr_regadd_rhh_exec,
	[INSTR_REGADD_RHM] = instr_regadd_rhm_exec,
	[INSTR_REGADD_RMH] = instr_regadd_rmh_exec,
	[INSTR_REGADD_RMM] = instr_regadd_rmm_exec,
	[INSTR_REGADD_RHI] = instr_regadd_rhi_exec,
	[INSTR_REGADD_RMI] = instr_regadd_rmi_exec,
	[INSTR_REGADD_RIH] = instr_regadd_rih_exec,
	[INSTR_REGADD_RIM] = instr_regadd_rim_exec,
	[INSTR_REGADD_RII] = instr_regadd_rii_exec,

	[INSTR_METPREFETCH_H] = instr_metprefetch_h_exec,
	[INSTR_METPREFETCH_M] = instr_metprefetch_m_exec,
	[INSTR_METPREFETCH_I] = instr_metprefetch_i_exec,

	[INSTR_METER_HHM] = instr_meter_hhm_exec,
	[INSTR_METER_HHI] = instr_meter_hhi_exec,
	[INSTR_METER_HMM] = instr_meter_hmm_exec,
	[INSTR_METER_HMI] = instr_meter_hmi_exec,
	[INSTR_METER_MHM] = instr_meter_mhm_exec,
	[INSTR_METER_MHI] = instr_meter_mhi_exec,
	[INSTR_METER_MMM] = instr_meter_mmm_exec,
	[INSTR_METER_MMI] = instr_meter_mmi_exec,
	[INSTR_METER_IHM] = instr_meter_ihm_exec,
	[INSTR_METER_IHI] = instr_meter_ihi_exec,
	[INSTR_METER_IMM] = instr_meter_imm_exec,
	[INSTR_METER_IMI] = instr_meter_imi_exec,

	[INSTR_TABLE] = instr_table_exec,
	[INSTR_TABLE_AF] = instr_table_af_exec,
	[INSTR_SELECTOR] = instr_selector_exec,
	[INSTR_LEARNER] = instr_learner_exec,
	[INSTR_LEARNER_AF] = instr_learner_af_exec,
	[INSTR_LEARNER_LEARN] = instr_learn_exec,
	[INSTR_LEARNER_REARM] = instr_rearm_exec,
	[INSTR_LEARNER_REARM_NEW] = instr_rearm_new_exec,
	[INSTR_LEARNER_FORGET] = instr_forget_exec,
	[INSTR_ENTRYID] = instr_entryid_exec,

	[INSTR_EXTERN_OBJ] = instr_extern_obj_exec,
	[INSTR_EXTERN_FUNC] = instr_extern_func_exec,
	[INSTR_HASH_FUNC] = instr_hash_func_exec,
	[INSTR_RSS] = instr_rss_exec,

	[INSTR_JMP] = instr_jmp_exec,
	[INSTR_JMP_VALID] = instr_jmp_valid_exec,
	[INSTR_JMP_INVALID] = instr_jmp_invalid_exec,
	[INSTR_JMP_HIT] = instr_jmp_hit_exec,
	[INSTR_JMP_MISS] = instr_jmp_miss_exec,
	[INSTR_JMP_ACTION_HIT] = instr_jmp_action_hit_exec,
	[INSTR_JMP_ACTION_MISS] = instr_jmp_action_miss_exec,

	[INSTR_JMP_EQ] = instr_jmp_eq_exec,
	[INSTR_JMP_EQ_MH] = instr_jmp_eq_mh_exec,
	[INSTR_JMP_EQ_HM] = instr_jmp_eq_hm_exec,
	[INSTR_JMP_EQ_HH] = instr_jmp_eq_hh_exec,
	[INSTR_JMP_EQ_I] = instr_jmp_eq_i_exec,

	[INSTR_JMP_NEQ] = instr_jmp_neq_exec,
	[INSTR_JMP_NEQ_MH] = instr_jmp_neq_mh_exec,
	[INSTR_JMP_NEQ_HM] = instr_jmp_neq_hm_exec,
	[INSTR_JMP_NEQ_HH] = instr_jmp_neq_hh_exec,
	[INSTR_JMP_NEQ_I] = instr_jmp_neq_i_exec,

	[INSTR_JMP_LT] = instr_jmp_lt_exec,
	[INSTR_JMP_LT_MH] = instr_jmp_lt_mh_exec,
	[INSTR_JMP_LT_HM] = instr_jmp_lt_hm_exec,
	[INSTR_JMP_LT_HH] = instr_jmp_lt_hh_exec,
	[INSTR_JMP_LT_MI] = instr_jmp_lt_mi_exec,
	[INSTR_JMP_LT_HI] = instr_jmp_lt_hi_exec,

	[INSTR_JMP_GT] = instr_jmp_gt_exec,
	[INSTR_JMP_GT_MH] = instr_jmp_gt_mh_exec,
	[INSTR_JMP_GT_HM] = instr_jmp_gt_hm_exec,
	[INSTR_JMP_GT_HH] = instr_jmp_gt_hh_exec,
	[INSTR_JMP_GT_MI] = instr_jmp_gt_mi_exec,
	[INSTR_JMP_GT_HI] = instr_jmp_gt_hi_exec,

	[INSTR_RETURN] = instr_return_exec,
};

static int
instruction_table_build(struct rte_swx_pipeline *p)
{
	p->instruction_table = calloc(RTE_SWX_PIPELINE_INSTRUCTION_TABLE_SIZE_MAX,
				      sizeof(struct instr_exec_t *));
	if (!p->instruction_table)
		return -EINVAL;

	memcpy(p->instruction_table, instruction_table, sizeof(instruction_table));

	return 0;
}

static void
instruction_table_build_free(struct rte_swx_pipeline *p)
{
	if (!p->instruction_table)
		return;

	free(p->instruction_table);
	p->instruction_table = NULL;
}

static void
instruction_table_free(struct rte_swx_pipeline *p)
{
	instruction_table_build_free(p);
}

static inline void
instr_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	instr_exec_t instr = p->instruction_table[ip->type];

	instr(p);
}

/*
 * Action.
 */
static struct action *
action_find(struct rte_swx_pipeline *p, const char *name)
{
	struct action *elem;

	if (!name)
		return NULL;

	TAILQ_FOREACH(elem, &p->actions, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct action *
action_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct action *action = NULL;

	TAILQ_FOREACH(action, &p->actions, node)
		if (action->id == id)
			return action;

	return NULL;
}

static struct field *
action_field_find(struct action *a, const char *name)
{
	return a->st ? struct_type_field_find(a->st, name) : NULL;
}

static struct field *
action_field_parse(struct action *action, const char *name)
{
	if (name[0] != 't' || name[1] != '.')
		return NULL;

	return action_field_find(action, &name[2]);
}

static int
action_has_nbo_args(struct action *a)
{
	uint32_t i;

	/* Return if the action does not have any args. */
	if (!a->st)
		return 0; /* FALSE */

	for (i = 0; i < a->st->n_fields; i++)
		if (a->args_endianness[i])
			return 1; /* TRUE */

	return 0; /* FALSE */
}

static int
action_does_learning(struct action *a)
{
	uint32_t i;

	for (i = 0; i < a->n_instructions; i++)
		switch (a->instructions[i].type) {
		case INSTR_LEARNER_LEARN:
			return 1; /* TRUE */

		case INSTR_LEARNER_FORGET:
			return 1; /* TRUE */

		default:
			continue;
		}

	return 0; /* FALSE */
}

int
rte_swx_pipeline_action_config(struct rte_swx_pipeline *p,
			       const char *name,
			       const char *args_struct_type_name,
			       const char **instructions,
			       uint32_t n_instructions)
{
	struct struct_type *args_struct_type = NULL;
	struct action *a = NULL;
	int status = 0;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!action_find(p, name), EEXIST);

	if (args_struct_type_name) {
		CHECK_NAME(args_struct_type_name, EINVAL);
		args_struct_type = struct_type_find(p, args_struct_type_name);
		CHECK(args_struct_type, EINVAL);
		CHECK(!args_struct_type->var_size, EINVAL);
	}

	/* Node allocation. */
	a = calloc(1, sizeof(struct action));
	if (!a) {
		status = -ENOMEM;
		goto error;
	}

	if (args_struct_type) {
		a->args_endianness = calloc(args_struct_type->n_fields, sizeof(int));
		if (!a->args_endianness) {
			status = -ENOMEM;
			goto error;
		}
	}

	/* Node initialization. */
	strcpy(a->name, name);
	a->st = args_struct_type;
	a->id = p->n_actions;

	/* Instruction translation. */
	status = instruction_config(p, a, instructions, n_instructions);
	if (status)
		goto error;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->actions, a, node);
	p->n_actions++;

	return 0;

error:
	if (!a)
		return status;

	free(a->args_endianness);
	free(a->instructions);
	free(a->instruction_data);
	free(a);

	return status;
}

static int
action_build(struct rte_swx_pipeline *p)
{
	struct action *action;

	/* p->action_instructions. */
	p->action_instructions = calloc(p->n_actions, sizeof(struct instruction *));
	CHECK(p->action_instructions, ENOMEM);

	TAILQ_FOREACH(action, &p->actions, node)
		p->action_instructions[action->id] = action->instructions;

	/* p->action_funcs. */
	p->action_funcs = calloc(p->n_actions, sizeof(action_func_t));
	CHECK(p->action_funcs, ENOMEM);

	return 0;
}

static void
action_build_free(struct rte_swx_pipeline *p)
{
	free(p->action_funcs);
	p->action_funcs = NULL;

	free(p->action_instructions);
	p->action_instructions = NULL;
}

static void
action_free(struct rte_swx_pipeline *p)
{
	action_build_free(p);

	for ( ; ; ) {
		struct action *action;

		action = TAILQ_FIRST(&p->actions);
		if (!action)
			break;

		TAILQ_REMOVE(&p->actions, action, node);
		free(action->args_endianness);
		free(action->instructions);
		free(action->instruction_data);
		free(action);
	}
}

static uint32_t
action_arg_src_mov_count(struct action *a,
			 uint32_t arg_id,
			 struct instruction *instructions,
			 struct instruction_data *instruction_data,
			 uint32_t n_instructions)
{
	uint32_t offset, n_users = 0, i;

	if (!a->st ||
	    (arg_id >= a->st->n_fields) ||
	    !instructions ||
	    !instruction_data ||
	    !n_instructions)
		return 0;

	offset = a->st->fields[arg_id].offset / 8;

	for (i = 0; i < n_instructions; i++) {
		struct instruction *instr = &instructions[i];
		struct instruction_data *data = &instruction_data[i];

		if (data->invalid ||
		    ((instr->type != INSTR_MOV) && (instr->type != INSTR_MOV_HM)) ||
		    instr->mov.src.struct_id ||
		    (instr->mov.src.offset != offset))
			continue;

		n_users++;
	}

	return n_users;
}

static int
char_to_hex(char c, uint8_t *val)
{
	if (c >= '0' && c <= '9') {
		*val = c - '0';
		return 0;
	}

	if (c >= 'A' && c <= 'F') {
		*val = c - 'A' + 10;
		return 0;
	}

	if (c >= 'a' && c <= 'f') {
		*val = c - 'a' + 10;
		return 0;
	}

	return -EINVAL;
}

static int
hex_string_parse(char *src, uint8_t *dst, uint32_t n_dst_bytes)
{
	uint32_t i;

	/* Check input arguments. */
	if (!src || !src[0] || !dst || !n_dst_bytes)
		return -EINVAL;

	/* Skip any leading "0x" or "0X" in the src string. */
	if ((src[0] == '0') && (src[1] == 'x' || src[1] == 'X'))
		src += 2;

	/* Convert each group of two hex characters in the src string to one byte in dst array. */
	for (i = 0; i < n_dst_bytes; i++) {
		uint8_t a, b;
		int status;

		status = char_to_hex(*src, &a);
		if (status)
			return status;
		src++;

		status = char_to_hex(*src, &b);
		if (status)
			return status;
		src++;

		dst[i] = a * 16 + b;
	}

	/* Check for the end of the src string. */
	if (*src)
		return -EINVAL;

	return 0;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define field_ntoh(val, n_bits) (ntoh64((val) << (64 - n_bits)))
#define field_hton(val, n_bits) (hton64((val) << (64 - n_bits)))
#else
#define field_ntoh(val, n_bits) (val)
#define field_hton(val, n_bits) (val)
#endif

#define ACTION_ARGS_TOKENS_MAX 256

static int
action_args_parse(struct action *a, const char *args, uint8_t *data)
{
	char *tokens[ACTION_ARGS_TOKENS_MAX], *s0 = NULL, *s;
	uint32_t n_tokens = 0, offset = 0, i;
	int status = 0;

	/* Checks. */
	if (!a->st || !args || !args[0]) {
		status = -EINVAL;
		goto error;
	}

	/* Memory allocation. */
	s0 = strdup(args);
	if (!s0) {
		status = -ENOMEM;
		goto error;
	}

	/* Parse the string into tokens. */
	for (s = s0; ; ) {
		char *token;

		token = strtok_r(s, " \f\n\r\t\v", &s);
		if (!token)
			break;

		if (n_tokens >= RTE_DIM(tokens)) {
			status = -EINVAL;
			goto error;
		}

		tokens[n_tokens] = token;
		n_tokens++;
	}

	/* More checks. */
	if (n_tokens != a->st->n_fields * 2) {
		status = -EINVAL;
		goto error;
	}

	/* Process the action arguments. */
	for (i = 0; i < a->st->n_fields; i++) {
		struct field *f = &a->st->fields[i];
		char *arg_name = tokens[i * 2];
		char *arg_val = tokens[i * 2 + 1];

		if (strcmp(arg_name, f->name)) {
			status = -EINVAL;
			goto error;
		}

		if (f->n_bits <= 64) {
			uint64_t val;

			val = strtoull(arg_val, &arg_val, 0);
			if (arg_val[0]) {
				status = -EINVAL;
				goto error;
			}

			/* Endianness conversion. */
			if (a->args_endianness[i])
				val = field_hton(val, f->n_bits);

			/* Copy to entry. */
			memcpy(&data[offset], (uint8_t *)&val, f->n_bits / 8);
		} else {
			status = hex_string_parse(arg_val, &data[offset], f->n_bits / 8);
			if (status)
				goto error;
		}

		offset += f->n_bits / 8;
	}

error:
	free(s0);
	return status;
}

/*
 * Table.
 */
static struct table_type *
table_type_find(struct rte_swx_pipeline *p, const char *name)
{
	struct table_type *elem;

	TAILQ_FOREACH(elem, &p->table_types, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct table_type *
table_type_resolve(struct rte_swx_pipeline *p,
		   const char *recommended_type_name,
		   enum rte_swx_table_match_type match_type)
{
	struct table_type *elem;

	/* Only consider the recommended type if the match type is correct. */
	if (recommended_type_name)
		TAILQ_FOREACH(elem, &p->table_types, node)
			if (!strcmp(elem->name, recommended_type_name) &&
			    (elem->match_type == match_type))
				return elem;

	/* Ignore the recommended type and get the first element with this match
	 * type.
	 */
	TAILQ_FOREACH(elem, &p->table_types, node)
		if (elem->match_type == match_type)
			return elem;

	return NULL;
}

static struct table *
table_find(struct rte_swx_pipeline *p, const char *name)
{
	struct table *elem;

	TAILQ_FOREACH(elem, &p->tables, node)
		if (strcmp(elem->name, name) == 0)
			return elem;

	return NULL;
}

static struct table *
table_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct table *table = NULL;

	TAILQ_FOREACH(table, &p->tables, node)
		if (table->id == id)
			return table;

	return NULL;
}

int
rte_swx_pipeline_table_type_register(struct rte_swx_pipeline *p,
				     const char *name,
				     enum rte_swx_table_match_type match_type,
				     struct rte_swx_table_ops *ops)
{
	struct table_type *elem;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_type_find(p, name), EEXIST);

	CHECK(ops, EINVAL);
	CHECK(ops->create, EINVAL);
	CHECK(ops->lkp, EINVAL);
	CHECK(ops->free, EINVAL);

	/* Node allocation. */
	elem = calloc(1, sizeof(struct table_type));
	CHECK(elem, ENOMEM);

	/* Node initialization. */
	strcpy(elem->name, name);
	elem->match_type = match_type;
	memcpy(&elem->ops, ops, sizeof(*ops));

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->table_types, elem, node);

	return 0;
}

static int
table_match_type_resolve(struct rte_swx_match_field_params *fields,
			 uint32_t n_fields,
			 int contiguous_fields,
			 enum rte_swx_table_match_type *match_type)
{
	uint32_t n_fields_em = 0, n_fields_lpm = 0, i;

	for (i = 0; i < n_fields; i++) {
		struct rte_swx_match_field_params  *f = &fields[i];

		if (f->match_type == RTE_SWX_TABLE_MATCH_EXACT)
			n_fields_em++;

		if (f->match_type == RTE_SWX_TABLE_MATCH_LPM)
			n_fields_lpm++;
	}

	if ((n_fields_lpm > 1) ||
	    (n_fields_lpm && (n_fields_em != n_fields - 1)))
		return -EINVAL;

	*match_type = ((n_fields_em == n_fields) && contiguous_fields) ?
		       RTE_SWX_TABLE_MATCH_EXACT :
		       RTE_SWX_TABLE_MATCH_WILDCARD;

	return 0;
}

static int
table_match_fields_check(struct rte_swx_pipeline *p,
			 struct rte_swx_pipeline_table_params *params,
			 struct header **header,
			 int *contiguous_fields)
{
	struct header *h0 = NULL;
	struct field *hf, *mf;
	uint32_t *offset = NULL, *n_bits = NULL, n_fields_with_valid_next = 0, i;
	int status = 0;

	/* Return if no match fields. */
	if (!params->n_fields) {
		if (params->fields) {
			status = -EINVAL;
			goto end;
		}

		if (header)
			*header = NULL;

		if (contiguous_fields)
			*contiguous_fields = 0;

		return 0;
	}

	/* Memory allocation. */
	offset = calloc(params->n_fields, sizeof(uint32_t));
	n_bits = calloc(params->n_fields, sizeof(uint32_t));
	if (!offset || !n_bits) {
		status = -ENOMEM;
		goto end;
	}

	/* Check that all the match fields belong to either the same header or
	 * to the meta-data.
	 */
	hf = header_field_parse(p, params->fields[0].name, &h0);
	mf = metadata_field_parse(p, params->fields[0].name);
	if ((!hf && !mf) || (hf && hf->var_size)) {
		status = -EINVAL;
		goto end;
	}

	offset[0] = h0 ? hf->offset : mf->offset;
	n_bits[0] = h0 ? hf->n_bits : mf->n_bits;

	for (i = 1; i < params->n_fields; i++)
		if (h0) {
			struct header *h;

			hf = header_field_parse(p, params->fields[i].name, &h);
			if (!hf || (h->id != h0->id) || hf->var_size) {
				status = -EINVAL;
				goto end;
			}

			offset[i] = hf->offset;
			n_bits[i] = hf->n_bits;
		} else {
			mf = metadata_field_parse(p, params->fields[i].name);
			if (!mf) {
				status = -EINVAL;
				goto end;
			}

			offset[i] = mf->offset;
			n_bits[i] = mf->n_bits;
		}

	/* Check that there are no duplicated match fields. */
	for (i = 0; i < params->n_fields; i++) {
		uint32_t j;

		for (j = 0; j < i; j++)
			if (offset[j] == offset[i]) {
				status = -EINVAL;
				goto end;
			}
	}

	/* Detect if the match fields are contiguous or not. */
	for (i = 0; i < params->n_fields; i++) {
		uint32_t offset_next = offset[i] + n_bits[i];
		uint32_t j;

		for (j = 0; j < params->n_fields; j++)
			if (offset[j] == offset_next) {
				n_fields_with_valid_next++;
				break;
			}
	}

	/* Return. */
	if (header)
		*header = h0;

	if (contiguous_fields)
		*contiguous_fields = (n_fields_with_valid_next == params->n_fields - 1) ? 1 : 0;

end:
	free(offset);
	free(n_bits);
	return status;
}

int
rte_swx_pipeline_table_config(struct rte_swx_pipeline *p,
			      const char *name,
			      struct rte_swx_pipeline_table_params *params,
			      const char *recommended_table_type_name,
			      const char *args,
			      uint32_t size)
{
	struct table_type *type = NULL;
	struct table *t = NULL;
	struct action *default_action;
	struct header *header = NULL;
	struct hash_func *hf = NULL;
	uint32_t action_data_size_max = 0, i;
	int contiguous_fields = 0, status = 0;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_find(p, name), EEXIST);
	CHECK(!selector_find(p, name), EEXIST);
	CHECK(!learner_find(p, name), EEXIST);

	CHECK(params, EINVAL);

	/* Match checks. */
	status = table_match_fields_check(p, params, &header, &contiguous_fields);
	if (status)
		return status;

	/* Action checks. */
	CHECK(params->n_actions, EINVAL);
	CHECK(params->action_names, EINVAL);
	for (i = 0; i < params->n_actions; i++) {
		const char *action_name = params->action_names[i];
		struct action *a;
		uint32_t action_data_size;
		int action_is_for_table_entries = 1, action_is_for_default_entry = 1;

		CHECK_NAME(action_name, EINVAL);

		a = action_find(p, action_name);
		CHECK(a, EINVAL);
		CHECK(!action_does_learning(a), EINVAL);

		action_data_size = a->st ? a->st->n_bits / 8 : 0;
		if (action_data_size > action_data_size_max)
			action_data_size_max = action_data_size;

		if (params->action_is_for_table_entries)
			action_is_for_table_entries = params->action_is_for_table_entries[i];
		if (params->action_is_for_default_entry)
			action_is_for_default_entry = params->action_is_for_default_entry[i];
		CHECK(action_is_for_table_entries || action_is_for_default_entry, EINVAL);
	}

	CHECK_NAME(params->default_action_name, EINVAL);
	for (i = 0; i < p->n_actions; i++)
		if (!strcmp(params->action_names[i],
			    params->default_action_name))
			break;
	CHECK(i < params->n_actions, EINVAL);
	CHECK(!params->action_is_for_default_entry || params->action_is_for_default_entry[i],
	      EINVAL);

	default_action = action_find(p, params->default_action_name);
	CHECK((default_action->st && params->default_action_args) || !params->default_action_args,
	      EINVAL);

	/* Hash function checks. */
	if (params->hash_func_name) {
		hf = hash_func_find(p, params->hash_func_name);
		CHECK(hf, EINVAL);
	}

	/* Table type checks. */
	if (recommended_table_type_name)
		CHECK_NAME(recommended_table_type_name, EINVAL);

	if (params->n_fields) {
		enum rte_swx_table_match_type match_type;

		status = table_match_type_resolve(params->fields,
						  params->n_fields,
						  contiguous_fields,
						  &match_type);
		if (status)
			return status;

		type = table_type_resolve(p, recommended_table_type_name, match_type);
		CHECK(type, EINVAL);
	}

	/* Memory allocation. */
	t = calloc(1, sizeof(struct table));
	if (!t) {
		status = -ENOMEM;
		goto error;
	}

	t->fields = calloc(params->n_fields, sizeof(struct match_field));
	if (!t->fields) {
		status = -ENOMEM;
		goto error;
	}

	t->actions = calloc(params->n_actions, sizeof(struct action *));
	if (!t->actions) {
		status = -ENOMEM;
		goto error;
	}

	if (action_data_size_max) {
		t->default_action_data = calloc(1, action_data_size_max);
		if (!t->default_action_data) {
			status = -ENOMEM;
			goto error;
		}
	}

	t->action_is_for_table_entries = calloc(params->n_actions, sizeof(int));
	if (!t->action_is_for_table_entries) {
		status = -ENOMEM;
		goto error;
	}

	t->action_is_for_default_entry = calloc(params->n_actions, sizeof(int));
	if (!t->action_is_for_default_entry) {
		status = -ENOMEM;
		goto error;
	}

	/* Node initialization. */
	strcpy(t->name, name);
	if (args && args[0])
		strcpy(t->args, args);
	t->type = type;

	for (i = 0; i < params->n_fields; i++) {
		struct rte_swx_match_field_params *field = &params->fields[i];
		struct match_field *f = &t->fields[i];

		f->match_type = field->match_type;
		f->field = header ?
			header_field_parse(p, field->name, NULL) :
			metadata_field_parse(p, field->name);
	}
	t->n_fields = params->n_fields;
	t->header = header;

	for (i = 0; i < params->n_actions; i++) {
		int action_is_for_table_entries = 1, action_is_for_default_entry = 1;

		if (params->action_is_for_table_entries)
			action_is_for_table_entries = params->action_is_for_table_entries[i];
		if (params->action_is_for_default_entry)
			action_is_for_default_entry = params->action_is_for_default_entry[i];

		t->actions[i] = action_find(p, params->action_names[i]);
		t->action_is_for_table_entries[i] = action_is_for_table_entries;
		t->action_is_for_default_entry[i] = action_is_for_default_entry;
	}
	t->default_action = default_action;
	if (default_action->st) {
		status = action_args_parse(default_action,
					   params->default_action_args,
					   t->default_action_data);
		if (status)
			goto error;
	}

	t->n_actions = params->n_actions;
	t->default_action_is_const = params->default_action_is_const;
	t->action_data_size_max = action_data_size_max;

	t->hf = hf;
	t->size = size;
	t->id = p->n_tables;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->tables, t, node);
	p->n_tables++;

	return 0;

error:
	if (!t)
		return status;

	free(t->action_is_for_default_entry);
	free(t->action_is_for_table_entries);
	free(t->default_action_data);
	free(t->actions);
	free(t->fields);
	free(t);

	return status;
}

static uint32_t
table_params_offset_get(struct table *table)
{
	struct field *first;
	uint32_t i;

	first = table->fields[0].field;

	for (i = 1; i < table->n_fields; i++) {
		struct field *f = table->fields[i].field;

		if (f->offset < first->offset)
			first = f;
	}

	return first->offset / 8;
}

static struct rte_swx_table_params *
table_params_get(struct table *table)
{
	struct rte_swx_table_params *params;
	struct field *first, *last;
	uint8_t *key_mask;
	uint32_t key_size, key_offset, action_data_size, i;

	/* Memory allocation. */
	params = calloc(1, sizeof(struct rte_swx_table_params));
	if (!params)
		return NULL;

	/* Find first (smallest offset) and last (biggest offset) match fields. */
	first = table->fields[0].field;
	last = table->fields[0].field;

	for (i = 0; i < table->n_fields; i++) {
		struct field *f = table->fields[i].field;

		if (f->offset < first->offset)
			first = f;

		if (f->offset > last->offset)
			last = f;
	}

	/* Key offset and size. */
	key_offset = first->offset / 8;
	key_size = (last->offset + last->n_bits - first->offset) / 8;

	/* Memory allocation. */
	key_mask = calloc(1, key_size);
	if (!key_mask) {
		free(params);
		return NULL;
	}

	/* Key mask. */
	for (i = 0; i < table->n_fields; i++) {
		struct field *f = table->fields[i].field;
		uint32_t start = (f->offset - first->offset) / 8;
		size_t size = f->n_bits / 8;

		memset(&key_mask[start], 0xFF, size);
	}

	/* Action data size. */
	action_data_size = 0;
	for (i = 0; i < table->n_actions; i++) {
		struct action *action = table->actions[i];
		uint32_t ads = action->st ? action->st->n_bits / 8 : 0;

		if (ads > action_data_size)
			action_data_size = ads;
	}

	/* Fill in. */
	params->match_type = table->type->match_type;
	params->key_size = key_size;
	params->key_offset = key_offset;
	params->key_mask0 = key_mask;
	params->action_data_size = action_data_size;
	params->hash_func = table->hf ? table->hf->func : NULL;
	params->n_keys_max = table->size;

	return params;
}

static void
table_params_free(struct rte_swx_table_params *params)
{
	if (!params)
		return;

	free(params->key_mask0);
	free(params);
}

static int
table_stub_lkp(void *table __rte_unused,
	       void *mailbox __rte_unused,
	       uint8_t **key __rte_unused,
	       uint64_t *action_id __rte_unused,
	       uint8_t **action_data __rte_unused,
	       size_t *entry_id __rte_unused,
	       int *hit)
{
	*hit = 0;
	return 1; /* DONE. */
}

static int
table_build(struct rte_swx_pipeline *p)
{
	uint32_t i;

	/* Per pipeline: table statistics. */
	p->table_stats = calloc(p->n_tables, sizeof(struct table_statistics));
	CHECK(p->table_stats, ENOMEM);

	for (i = 0; i < p->n_tables; i++) {
		p->table_stats[i].n_pkts_action = calloc(p->n_actions, sizeof(uint64_t));
		CHECK(p->table_stats[i].n_pkts_action, ENOMEM);
	}

	/* Per thread: table runt-time. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct table *table;

		t->tables = calloc(p->n_tables, sizeof(struct table_runtime));
		CHECK(t->tables, ENOMEM);

		TAILQ_FOREACH(table, &p->tables, node) {
			struct table_runtime *r = &t->tables[table->id];

			if (table->type) {
				uint64_t size;

				size = table->type->ops.mailbox_size_get();

				/* r->func. */
				r->func = table->type->ops.lkp;

				/* r->mailbox. */
				if (size) {
					r->mailbox = calloc(1, size);
					CHECK(r->mailbox, ENOMEM);
				}

				/* r->key. */
				r->key = table->header ?
					&t->structs[table->header->struct_id] :
					&t->structs[p->metadata_struct_id];
			} else {
				r->func = table_stub_lkp;
			}
		}
	}

	return 0;
}

static void
table_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->tables)
			continue;

		for (j = 0; j < p->n_tables; j++) {
			struct table_runtime *r = &t->tables[j];

			free(r->mailbox);
		}

		free(t->tables);
		t->tables = NULL;
	}

	if (p->table_stats) {
		for (i = 0; i < p->n_tables; i++)
			free(p->table_stats[i].n_pkts_action);

		free(p->table_stats);
	}
}

static void
table_free(struct rte_swx_pipeline *p)
{
	table_build_free(p);

	/* Tables. */
	for ( ; ; ) {
		struct table *elem;

		elem = TAILQ_FIRST(&p->tables);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->tables, elem, node);
		free(elem->fields);
		free(elem->actions);
		free(elem->default_action_data);
		free(elem);
	}

	/* Table types. */
	for ( ; ; ) {
		struct table_type *elem;

		elem = TAILQ_FIRST(&p->table_types);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->table_types, elem, node);
		free(elem);
	}
}

/*
 * Selector.
 */
static struct selector *
selector_find(struct rte_swx_pipeline *p, const char *name)
{
	struct selector *s;

	TAILQ_FOREACH(s, &p->selectors, node)
		if (strcmp(s->name, name) == 0)
			return s;

	return NULL;
}

static struct selector *
selector_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct selector *s = NULL;

	TAILQ_FOREACH(s, &p->selectors, node)
		if (s->id == id)
			return s;

	return NULL;
}

static int
selector_fields_check(struct rte_swx_pipeline *p,
		      struct rte_swx_pipeline_selector_params *params,
		      struct header **header)
{
	struct header *h0 = NULL;
	struct field *hf, *mf;
	uint32_t i;

	/* Return if no selector fields. */
	if (!params->n_selector_fields || !params->selector_field_names)
		return -EINVAL;

	/* Check that all the selector fields either belong to the same header
	 * or are all meta-data fields.
	 */
	hf = header_field_parse(p, params->selector_field_names[0], &h0);
	mf = metadata_field_parse(p, params->selector_field_names[0]);
	if (!hf && !mf)
		return -EINVAL;

	for (i = 1; i < params->n_selector_fields; i++)
		if (h0) {
			struct header *h;

			hf = header_field_parse(p, params->selector_field_names[i], &h);
			if (!hf || (h->id != h0->id))
				return -EINVAL;
		} else {
			mf = metadata_field_parse(p, params->selector_field_names[i]);
			if (!mf)
				return -EINVAL;
		}

	/* Check that there are no duplicated match fields. */
	for (i = 0; i < params->n_selector_fields; i++) {
		const char *field_name = params->selector_field_names[i];
		uint32_t j;

		for (j = i + 1; j < params->n_selector_fields; j++)
			if (!strcmp(params->selector_field_names[j], field_name))
				return -EINVAL;
	}

	/* Return. */
	if (header)
		*header = h0;

	return 0;
}

int
rte_swx_pipeline_selector_config(struct rte_swx_pipeline *p,
				 const char *name,
				 struct rte_swx_pipeline_selector_params *params)
{
	struct selector *s;
	struct header *selector_header = NULL;
	struct field *group_id_field, *member_id_field;
	uint32_t i;
	int status = 0;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_find(p, name), EEXIST);
	CHECK(!selector_find(p, name), EEXIST);
	CHECK(!learner_find(p, name), EEXIST);

	CHECK(params, EINVAL);

	CHECK_NAME(params->group_id_field_name, EINVAL);
	group_id_field = metadata_field_parse(p, params->group_id_field_name);
	CHECK(group_id_field, EINVAL);

	for (i = 0; i < params->n_selector_fields; i++) {
		const char *field_name = params->selector_field_names[i];

		CHECK_NAME(field_name, EINVAL);
	}
	status = selector_fields_check(p, params, &selector_header);
	if (status)
		return status;

	CHECK_NAME(params->member_id_field_name, EINVAL);
	member_id_field = metadata_field_parse(p, params->member_id_field_name);
	CHECK(member_id_field, EINVAL);

	CHECK(params->n_groups_max, EINVAL);

	CHECK(params->n_members_per_group_max, EINVAL);

	/* Memory allocation. */
	s = calloc(1, sizeof(struct selector));
	if (!s) {
		status = -ENOMEM;
		goto error;
	}

	s->selector_fields = calloc(params->n_selector_fields, sizeof(struct field *));
	if (!s->selector_fields) {
		status = -ENOMEM;
		goto error;
	}

	/* Node initialization. */
	strcpy(s->name, name);

	s->group_id_field = group_id_field;

	for (i = 0; i < params->n_selector_fields; i++) {
		const char *field_name = params->selector_field_names[i];

		s->selector_fields[i] = selector_header ?
			header_field_parse(p, field_name, NULL) :
			metadata_field_parse(p, field_name);
	}

	s->n_selector_fields = params->n_selector_fields;

	s->selector_header = selector_header;

	s->member_id_field = member_id_field;

	s->n_groups_max = params->n_groups_max;

	s->n_members_per_group_max = params->n_members_per_group_max;

	s->id = p->n_selectors;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->selectors, s, node);
	p->n_selectors++;

	return 0;

error:
	if (!s)
		return status;

	free(s->selector_fields);

	free(s);

	return status;
}

static void
selector_params_free(struct rte_swx_table_selector_params *params)
{
	if (!params)
		return;

	free(params->selector_mask);

	free(params);
}

static struct rte_swx_table_selector_params *
selector_table_params_get(struct selector *s)
{
	struct rte_swx_table_selector_params *params = NULL;
	struct field *first, *last;
	uint32_t i;

	/* Memory allocation. */
	params = calloc(1, sizeof(struct rte_swx_table_selector_params));
	if (!params)
		goto error;

	/* Group ID. */
	params->group_id_offset = s->group_id_field->offset / 8;

	/* Find first (smallest offset) and last (biggest offset) selector fields. */
	first = s->selector_fields[0];
	last = s->selector_fields[0];

	for (i = 0; i < s->n_selector_fields; i++) {
		struct field *f = s->selector_fields[i];

		if (f->offset < first->offset)
			first = f;

		if (f->offset > last->offset)
			last = f;
	}

	/* Selector offset and size. */
	params->selector_offset = first->offset / 8;
	params->selector_size = (last->offset + last->n_bits - first->offset) / 8;

	/* Memory allocation. */
	params->selector_mask = calloc(1, params->selector_size);
	if (!params->selector_mask)
		goto error;

	/* Selector mask. */
	for (i = 0; i < s->n_selector_fields; i++) {
		struct field *f = s->selector_fields[i];
		uint32_t start = (f->offset - first->offset) / 8;
		size_t size = f->n_bits / 8;

		memset(&params->selector_mask[start], 0xFF, size);
	}

	/* Member ID. */
	params->member_id_offset = s->member_id_field->offset / 8;

	/* Maximum number of groups. */
	params->n_groups_max = s->n_groups_max;

	/* Maximum number of members per group. */
	params->n_members_per_group_max = s->n_members_per_group_max;

	return params;

error:
	selector_params_free(params);
	return NULL;
}

static void
selector_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->selectors)
			continue;

		for (j = 0; j < p->n_selectors; j++) {
			struct selector_runtime *r = &t->selectors[j];

			free(r->mailbox);
		}

		free(t->selectors);
		t->selectors = NULL;
	}

	free(p->selector_stats);
	p->selector_stats = NULL;
}

static int
selector_build(struct rte_swx_pipeline *p)
{
	uint32_t i;
	int status = 0;

	/* Per pipeline: selector statistics. */
	p->selector_stats = calloc(p->n_selectors, sizeof(struct selector_statistics));
	if (!p->selector_stats) {
		status = -ENOMEM;
		goto error;
	}

	/* Per thread: selector run-time. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct selector *s;

		t->selectors = calloc(p->n_selectors, sizeof(struct selector_runtime));
		if (!t->selectors) {
			status = -ENOMEM;
			goto error;
		}

		TAILQ_FOREACH(s, &p->selectors, node) {
			struct selector_runtime *r = &t->selectors[s->id];
			uint64_t size;

			/* r->mailbox. */
			size = rte_swx_table_selector_mailbox_size_get();
			if (size) {
				r->mailbox = calloc(1, size);
				if (!r->mailbox) {
					status = -ENOMEM;
					goto error;
				}
			}

			/* r->group_id_buffer. */
			r->group_id_buffer = &t->structs[p->metadata_struct_id];

			/* r->selector_buffer. */
			r->selector_buffer = s->selector_header ?
				&t->structs[s->selector_header->struct_id] :
				&t->structs[p->metadata_struct_id];

			/* r->member_id_buffer. */
			r->member_id_buffer = &t->structs[p->metadata_struct_id];
		}
	}

	return 0;

error:
	selector_build_free(p);
	return status;
}

static void
selector_free(struct rte_swx_pipeline *p)
{
	selector_build_free(p);

	/* Selector tables. */
	for ( ; ; ) {
		struct selector *elem;

		elem = TAILQ_FIRST(&p->selectors);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->selectors, elem, node);
		free(elem->selector_fields);
		free(elem);
	}
}

/*
 * Learner table.
 */
static struct learner *
learner_find(struct rte_swx_pipeline *p, const char *name)
{
	struct learner *l;

	TAILQ_FOREACH(l, &p->learners, node)
		if (!strcmp(l->name, name))
			return l;

	return NULL;
}

static struct learner *
learner_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct learner *l = NULL;

	TAILQ_FOREACH(l, &p->learners, node)
		if (l->id == id)
			return l;

	return NULL;
}

static int
learner_match_fields_check(struct rte_swx_pipeline *p,
			   struct rte_swx_pipeline_learner_params *params,
			   struct header **header)
{
	struct header *h0 = NULL;
	struct field *hf, *mf;
	uint32_t *offset = NULL, *n_bits = NULL, n_fields_with_valid_next = 0, i;
	int status = 0;

	/* Return if no match fields. */
	if (!params->n_fields || !params->field_names)
		return -EINVAL;

	/* Memory allocation. */
	offset = calloc(params->n_fields, sizeof(uint32_t));
	n_bits = calloc(params->n_fields, sizeof(uint32_t));
	if (!offset || !n_bits) {
		status = -ENOMEM;
		goto end;
	}

	/* Check that all the match fields either belong to the same header
	 * or are all meta-data fields.
	 */
	hf = header_field_parse(p, params->field_names[0], &h0);
	mf = metadata_field_parse(p, params->field_names[0]);
	if ((!hf && !mf) || (hf && hf->var_size)) {
		status = -EINVAL;
		goto end;
	}

	offset[0] = h0 ? hf->offset : mf->offset;
	n_bits[0] = h0 ? hf->n_bits : mf->n_bits;

	for (i = 1; i < params->n_fields; i++)
		if (h0) {
			struct header *h;

			hf = header_field_parse(p, params->field_names[i], &h);
			if (!hf || (h->id != h0->id) || hf->var_size) {
				status = -EINVAL;
				goto end;
			}

			offset[i] = hf->offset;
			n_bits[i] = hf->n_bits;
		} else {
			mf = metadata_field_parse(p, params->field_names[i]);
			if (!mf) {
				status = -EINVAL;
				goto end;
			}

			offset[i] = mf->offset;
			n_bits[i] = mf->n_bits;
		}

	/* Check that there are no duplicated match fields. */
	for (i = 0; i < params->n_fields; i++) {
		const char *field_name = params->field_names[i];
		uint32_t j;

		for (j = i + 1; j < params->n_fields; j++)
			if (!strcmp(params->field_names[j], field_name)) {
				status = -EINVAL;
				goto end;
			}
	}

	/* Check that the match fields are contiguous. */
	for (i = 0; i < params->n_fields; i++) {
		uint32_t offset_next = offset[i] + n_bits[i];
		uint32_t j;

		for (j = 0; j < params->n_fields; j++)
			if (offset[j] == offset_next) {
				n_fields_with_valid_next++;
				break;
			}
	}

	if (n_fields_with_valid_next != params->n_fields - 1) {
		status = -EINVAL;
		goto end;
	}

	/* Return. */
	if (header)
		*header = h0;

end:
	free(offset);
	free(n_bits);
	return status;
}

static int
learner_action_args_check(struct rte_swx_pipeline *p, struct action *a, const char *mf_name)
{
	struct struct_type *mst = p->metadata_st, *ast = a->st;
	struct field *mf, *af;
	uint32_t mf_pos, i;

	if (!ast) {
		if (mf_name)
			return -EINVAL;

		return 0;
	}

	/* Check that mf_name is the name of a valid meta-data field. */
	CHECK_NAME(mf_name, EINVAL);
	mf = metadata_field_parse(p, mf_name);
	CHECK(mf, EINVAL);

	/* Check that there are enough meta-data fields, starting with the mf_name field, to cover
	 * all the action arguments.
	 */
	mf_pos = mf - mst->fields;
	CHECK(mst->n_fields - mf_pos >= ast->n_fields, EINVAL);

	/* Check that the size of each of the identified meta-data fields matches exactly the size
	 * of the corresponding action argument.
	 */
	for (i = 0; i < ast->n_fields; i++) {
		mf = &mst->fields[mf_pos + i];
		af = &ast->fields[i];

		CHECK(mf->n_bits == af->n_bits, EINVAL);
	}

	return 0;
}

static int
learner_action_learning_check(struct rte_swx_pipeline *p,
			      struct action *action,
			      const char **action_names,
			      uint32_t n_actions)
{
	uint32_t i;

	/* For each "learn" instruction of the current action, check that the learned action (i.e.
	 * the action passed as argument to the "learn" instruction) is also enabled for the
	 * current learner table.
	 */
	for (i = 0; i < action->n_instructions; i++) {
		struct instruction *instr = &action->instructions[i];
		uint32_t found = 0, j;

		if (instr->type != INSTR_LEARNER_LEARN)
			continue;

		for (j = 0; j < n_actions; j++) {
			struct action *a;

			a = action_find(p, action_names[j]);
			if (!a)
				return -EINVAL;

			if (a->id == instr->learn.action_id)
				found = 1;
		}

		if (!found)
			return -EINVAL;
	}

	return 0;
}

int
rte_swx_pipeline_learner_config(struct rte_swx_pipeline *p,
			      const char *name,
			      struct rte_swx_pipeline_learner_params *params,
			      uint32_t size,
			      uint32_t *timeout,
			      uint32_t n_timeouts)
{
	struct learner *l = NULL;
	struct action *default_action;
	struct header *header = NULL;
	struct hash_func *hf = NULL;
	uint32_t action_data_size_max = 0, i;
	int status = 0;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_find(p, name), EEXIST);
	CHECK(!selector_find(p, name), EEXIST);
	CHECK(!learner_find(p, name), EEXIST);

	CHECK(params, EINVAL);

	/* Match checks. */
	status = learner_match_fields_check(p, params, &header);
	if (status)
		return status;

	/* Action checks. */
	CHECK(params->n_actions, EINVAL);
	CHECK(params->action_names, EINVAL);
	for (i = 0; i < params->n_actions; i++) {
		const char *action_name = params->action_names[i];
		struct action *a;
		uint32_t action_data_size;
		int action_is_for_table_entries = 1, action_is_for_default_entry = 1;

		CHECK_NAME(action_name, EINVAL);

		a = action_find(p, action_name);
		CHECK(a, EINVAL);

		status = learner_action_learning_check(p,
						       a,
						       params->action_names,
						       params->n_actions);
		if (status)
			return status;

		action_data_size = a->st ? a->st->n_bits / 8 : 0;
		if (action_data_size > action_data_size_max)
			action_data_size_max = action_data_size;

		if (params->action_is_for_table_entries)
			action_is_for_table_entries = params->action_is_for_table_entries[i];
		if (params->action_is_for_default_entry)
			action_is_for_default_entry = params->action_is_for_default_entry[i];
		CHECK(action_is_for_table_entries || action_is_for_default_entry, EINVAL);
	}

	CHECK_NAME(params->default_action_name, EINVAL);
	for (i = 0; i < p->n_actions; i++)
		if (!strcmp(params->action_names[i],
			    params->default_action_name))
			break;
	CHECK(i < params->n_actions, EINVAL);
	CHECK(!params->action_is_for_default_entry || params->action_is_for_default_entry[i],
	      EINVAL);

	default_action = action_find(p, params->default_action_name);
	CHECK((default_action->st && params->default_action_args) || !params->default_action_args,
	      EINVAL);

	/* Hash function checks. */
	if (params->hash_func_name) {
		hf = hash_func_find(p, params->hash_func_name);
		CHECK(hf, EINVAL);
	}

	/* Any other checks. */
	CHECK(size, EINVAL);
	CHECK(timeout, EINVAL);
	CHECK(n_timeouts && (n_timeouts <= RTE_SWX_TABLE_LEARNER_N_KEY_TIMEOUTS_MAX), EINVAL);

	/* Memory allocation. */
	l = calloc(1, sizeof(struct learner));
	if (!l) {
		status = -ENOMEM;
		goto error;
	}

	l->fields = calloc(params->n_fields, sizeof(struct field *));
	if (!l->fields) {
		status = -ENOMEM;
		goto error;
	}

	l->actions = calloc(params->n_actions, sizeof(struct action *));
	if (!l->actions) {
		status = -ENOMEM;
		goto error;
	}

	if (action_data_size_max) {
		l->default_action_data = calloc(1, action_data_size_max);
		if (!l->default_action_data) {
			status = -ENOMEM;
			goto error;
		}
	}

	l->action_is_for_table_entries = calloc(params->n_actions, sizeof(int));
	if (!l->action_is_for_table_entries) {
		status = -ENOMEM;
		goto error;
	}

	l->action_is_for_default_entry = calloc(params->n_actions, sizeof(int));
	if (!l->action_is_for_default_entry) {
		status = -ENOMEM;
		goto error;
	}

	/* Node initialization. */
	strcpy(l->name, name);

	for (i = 0; i < params->n_fields; i++) {
		const char *field_name = params->field_names[i];

		l->fields[i] = header ?
			header_field_parse(p, field_name, NULL) :
			metadata_field_parse(p, field_name);
	}

	l->n_fields = params->n_fields;

	l->header = header;

	for (i = 0; i < params->n_actions; i++) {
		int action_is_for_table_entries = 1, action_is_for_default_entry = 1;

		if (params->action_is_for_table_entries)
			action_is_for_table_entries = params->action_is_for_table_entries[i];
		if (params->action_is_for_default_entry)
			action_is_for_default_entry = params->action_is_for_default_entry[i];

		l->actions[i] = action_find(p, params->action_names[i]);
		l->action_is_for_table_entries[i] = action_is_for_table_entries;
		l->action_is_for_default_entry[i] = action_is_for_default_entry;
	}

	l->default_action = default_action;

	if (default_action->st) {
		status = action_args_parse(default_action,
					   params->default_action_args,
					   l->default_action_data);
		if (status)
			goto error;
	}

	l->n_actions = params->n_actions;

	l->default_action_is_const = params->default_action_is_const;

	l->action_data_size_max = action_data_size_max;

	l->hf = hf;

	l->size = size;

	for (i = 0; i < n_timeouts; i++)
		l->timeout[i] = timeout[i];

	l->n_timeouts = n_timeouts;

	l->id = p->n_learners;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->learners, l, node);
	p->n_learners++;

	return 0;

error:
	if (!l)
		return status;

	free(l->action_is_for_default_entry);
	free(l->action_is_for_table_entries);
	free(l->default_action_data);
	free(l->actions);
	free(l->fields);
	free(l);

	return status;
}

static uint32_t
learner_params_offset_get(struct learner *l)
{
	struct field *first;
	uint32_t i;

	first = l->fields[0];

	for (i = 1; i < l->n_fields; i++) {
		struct field *f = l->fields[i];

		if (f->offset < first->offset)
			first = f;
	}

	return first->offset / 8;
}

static void
learner_params_free(struct rte_swx_table_learner_params *params)
{
	if (!params)
		return;

	free(params->key_mask0);

	free(params->key_timeout);

	free(params);
}

static struct rte_swx_table_learner_params *
learner_params_get(struct learner *l)
{
	struct rte_swx_table_learner_params *params = NULL;
	struct field *first, *last;
	uint32_t i;

	/* Memory allocation. */
	params = calloc(1, sizeof(struct rte_swx_table_learner_params));
	if (!params)
		goto error;

	/* Find first (smallest offset) and last (biggest offset) match fields. */
	first = l->fields[0];
	last = l->fields[0];

	for (i = 0; i < l->n_fields; i++) {
		struct field *f = l->fields[i];

		if (f->offset < first->offset)
			first = f;

		if (f->offset > last->offset)
			last = f;
	}

	/* Key offset and size. */
	params->key_offset = first->offset / 8;
	params->key_size = (last->offset + last->n_bits - first->offset) / 8;

	/* Memory allocation. */
	params->key_mask0 = calloc(1, params->key_size);
	if (!params->key_mask0)
		goto error;

	/* Key mask. */
	for (i = 0; i < l->n_fields; i++) {
		struct field *f = l->fields[i];
		uint32_t start = (f->offset - first->offset) / 8;
		size_t size = f->n_bits / 8;

		memset(&params->key_mask0[start], 0xFF, size);
	}

	/* Action data size. */
	params->action_data_size = l->action_data_size_max;

	/* Hash function. */
	params->hash_func = l->hf ? l->hf->func : NULL;

	/* Maximum number of keys. */
	params->n_keys_max = l->size;

	/* Memory allocation. */
	params->key_timeout = calloc(l->n_timeouts, sizeof(uint32_t));
	if (!params->key_timeout)
		goto error;

	/* Timeout. */
	for (i = 0; i < l->n_timeouts; i++)
		params->key_timeout[i] = l->timeout[i];

	params->n_key_timeouts = l->n_timeouts;

	return params;

error:
	learner_params_free(params);
	return NULL;
}

static void
learner_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		uint32_t j;

		if (!t->learners)
			continue;

		for (j = 0; j < p->n_learners; j++) {
			struct learner_runtime *r = &t->learners[j];

			free(r->mailbox);
		}

		free(t->learners);
		t->learners = NULL;
	}

	if (p->learner_stats) {
		for (i = 0; i < p->n_learners; i++)
			free(p->learner_stats[i].n_pkts_action);

		free(p->learner_stats);
	}
}

static int
learner_build(struct rte_swx_pipeline *p)
{
	uint32_t i;
	int status = 0;

	/* Per pipeline: learner statistics. */
	p->learner_stats = calloc(p->n_learners, sizeof(struct learner_statistics));
	CHECK(p->learner_stats, ENOMEM);

	for (i = 0; i < p->n_learners; i++) {
		p->learner_stats[i].n_pkts_action = calloc(p->n_actions, sizeof(uint64_t));
		CHECK(p->learner_stats[i].n_pkts_action, ENOMEM);
	}

	/* Per thread: learner run-time. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];
		struct learner *l;

		t->learners = calloc(p->n_learners, sizeof(struct learner_runtime));
		if (!t->learners) {
			status = -ENOMEM;
			goto error;
		}

		TAILQ_FOREACH(l, &p->learners, node) {
			struct learner_runtime *r = &t->learners[l->id];
			uint64_t size;

			/* r->mailbox. */
			size = rte_swx_table_learner_mailbox_size_get();
			if (size) {
				r->mailbox = calloc(1, size);
				if (!r->mailbox) {
					status = -ENOMEM;
					goto error;
				}
			}

			/* r->key. */
			r->key = l->header ?
				&t->structs[l->header->struct_id] :
				&t->structs[p->metadata_struct_id];
		}
	}

	return 0;

error:
	learner_build_free(p);
	return status;
}

static void
learner_free(struct rte_swx_pipeline *p)
{
	learner_build_free(p);

	/* Learner tables. */
	for ( ; ; ) {
		struct learner *l;

		l = TAILQ_FIRST(&p->learners);
		if (!l)
			break;

		TAILQ_REMOVE(&p->learners, l, node);
		free(l->fields);
		free(l->actions);
		free(l->default_action_data);
		free(l);
	}
}

/*
 * Table state.
 */
static int
table_state_build(struct rte_swx_pipeline *p)
{
	struct table *table;
	struct selector *s;
	struct learner *l;

	p->table_state = calloc(p->n_tables + p->n_selectors + p->n_learners,
				sizeof(struct rte_swx_table_state));
	CHECK(p->table_state, ENOMEM);

	TAILQ_FOREACH(table, &p->tables, node) {
		struct rte_swx_table_state *ts = &p->table_state[table->id];

		if (table->type) {
			struct rte_swx_table_params *params;

			/* ts->obj. */
			params = table_params_get(table);
			CHECK(params, ENOMEM);

			ts->obj = table->type->ops.create(params,
				NULL,
				table->args,
				p->numa_node);

			table_params_free(params);
			CHECK(ts->obj, ENODEV);
		}

		/* ts->default_action_data. */
		if (table->action_data_size_max) {
			ts->default_action_data =
				malloc(table->action_data_size_max);
			CHECK(ts->default_action_data, ENOMEM);

			memcpy(ts->default_action_data,
			       table->default_action_data,
			       table->action_data_size_max);
		}

		/* ts->default_action_id. */
		ts->default_action_id = table->default_action->id;
	}

	TAILQ_FOREACH(s, &p->selectors, node) {
		struct rte_swx_table_state *ts = &p->table_state[p->n_tables + s->id];
		struct rte_swx_table_selector_params *params;

		/* ts->obj. */
		params = selector_table_params_get(s);
		CHECK(params, ENOMEM);

		ts->obj = rte_swx_table_selector_create(params, NULL, p->numa_node);

		selector_params_free(params);
		CHECK(ts->obj, ENODEV);
	}

	TAILQ_FOREACH(l, &p->learners, node) {
		struct rte_swx_table_state *ts = &p->table_state[p->n_tables +
			p->n_selectors + l->id];
		struct rte_swx_table_learner_params *params;

		/* ts->obj. */
		params = learner_params_get(l);
		CHECK(params, ENOMEM);

		ts->obj = rte_swx_table_learner_create(params, p->numa_node);
		learner_params_free(params);
		CHECK(ts->obj, ENODEV);

		/* ts->default_action_data. */
		if (l->action_data_size_max) {
			ts->default_action_data = malloc(l->action_data_size_max);
			CHECK(ts->default_action_data, ENOMEM);

			memcpy(ts->default_action_data,
			       l->default_action_data,
			       l->action_data_size_max);
		}

		/* ts->default_action_id. */
		ts->default_action_id = l->default_action->id;
	}

	return 0;
}

static void
table_state_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	if (!p->table_state)
		return;

	for (i = 0; i < p->n_tables; i++) {
		struct rte_swx_table_state *ts = &p->table_state[i];
		struct table *table = table_find_by_id(p, i);

		/* ts->obj. */
		if (table->type && ts->obj)
			table->type->ops.free(ts->obj);

		/* ts->default_action_data. */
		free(ts->default_action_data);
	}

	for (i = 0; i < p->n_selectors; i++) {
		struct rte_swx_table_state *ts = &p->table_state[p->n_tables + i];

		/* ts->obj. */
		rte_swx_table_selector_free(ts->obj);
	}

	for (i = 0; i < p->n_learners; i++) {
		struct rte_swx_table_state *ts = &p->table_state[p->n_tables + p->n_selectors + i];

		/* ts->obj. */
		rte_swx_table_learner_free(ts->obj);

		/* ts->default_action_data. */
		free(ts->default_action_data);
	}

	free(p->table_state);
	p->table_state = NULL;
}

static void
table_state_free(struct rte_swx_pipeline *p)
{
	table_state_build_free(p);
}

/*
 * Register array.
 */
static struct regarray *
regarray_find(struct rte_swx_pipeline *p, const char *name)
{
	struct regarray *elem;

	TAILQ_FOREACH(elem, &p->regarrays, node)
		if (!strcmp(elem->name, name))
			return elem;

	return NULL;
}

static struct regarray *
regarray_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct regarray *elem = NULL;

	TAILQ_FOREACH(elem, &p->regarrays, node)
		if (elem->id == id)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_regarray_config(struct rte_swx_pipeline *p,
			      const char *name,
			      uint32_t size,
			      uint64_t init_val)
{
	struct regarray *r;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!regarray_find(p, name), EEXIST);

	CHECK(size, EINVAL);
	size = rte_align32pow2(size);

	/* Memory allocation. */
	r = calloc(1, sizeof(struct regarray));
	CHECK(r, ENOMEM);

	/* Node initialization. */
	strcpy(r->name, name);
	r->init_val = init_val;
	r->size = size;
	r->id = p->n_regarrays;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->regarrays, r, node);
	p->n_regarrays++;

	return 0;
}

static int
regarray_build(struct rte_swx_pipeline *p)
{
	struct regarray *regarray;

	if (!p->n_regarrays)
		return 0;

	p->regarray_runtime = calloc(p->n_regarrays, sizeof(struct regarray_runtime));
	CHECK(p->regarray_runtime, ENOMEM);

	TAILQ_FOREACH(regarray, &p->regarrays, node) {
		struct regarray_runtime *r = &p->regarray_runtime[regarray->id];
		uint32_t i;

		r->regarray = env_malloc(regarray->size * sizeof(uint64_t),
					 RTE_CACHE_LINE_SIZE,
					 p->numa_node);
		CHECK(r->regarray, ENOMEM);

		if (regarray->init_val)
			for (i = 0; i < regarray->size; i++)
				r->regarray[i] = regarray->init_val;

		r->size_mask = regarray->size - 1;
	}

	return 0;
}

static void
regarray_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	if (!p->regarray_runtime)
		return;

	for (i = 0; i < p->n_regarrays; i++) {
		struct regarray *regarray = regarray_find_by_id(p, i);
		struct regarray_runtime *r = &p->regarray_runtime[i];

		env_free(r->regarray, regarray->size * sizeof(uint64_t));
	}

	free(p->regarray_runtime);
	p->regarray_runtime = NULL;
}

static void
regarray_free(struct rte_swx_pipeline *p)
{
	regarray_build_free(p);

	for ( ; ; ) {
		struct regarray *elem;

		elem = TAILQ_FIRST(&p->regarrays);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->regarrays, elem, node);
		free(elem);
	}
}

/*
 * Meter array.
 */
static struct meter_profile *
meter_profile_find(struct rte_swx_pipeline *p, const char *name)
{
	struct meter_profile *elem;

	TAILQ_FOREACH(elem, &p->meter_profiles, node)
		if (!strcmp(elem->name, name))
			return elem;

	return NULL;
}

static struct metarray *
metarray_find(struct rte_swx_pipeline *p, const char *name)
{
	struct metarray *elem;

	TAILQ_FOREACH(elem, &p->metarrays, node)
		if (!strcmp(elem->name, name))
			return elem;

	return NULL;
}

static struct metarray *
metarray_find_by_id(struct rte_swx_pipeline *p, uint32_t id)
{
	struct metarray *elem = NULL;

	TAILQ_FOREACH(elem, &p->metarrays, node)
		if (elem->id == id)
			return elem;

	return NULL;
}

int
rte_swx_pipeline_metarray_config(struct rte_swx_pipeline *p,
				 const char *name,
				 uint32_t size)
{
	struct metarray *m;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!metarray_find(p, name), EEXIST);

	CHECK(size, EINVAL);
	size = rte_align32pow2(size);

	/* Memory allocation. */
	m = calloc(1, sizeof(struct metarray));
	CHECK(m, ENOMEM);

	/* Node initialization. */
	strcpy(m->name, name);
	m->size = size;
	m->id = p->n_metarrays;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->metarrays, m, node);
	p->n_metarrays++;

	return 0;
}

struct meter_profile meter_profile_default = {
	.node = {0},
	.name = "",
	.params = {0},

	.profile = {
		.cbs = 10000,
		.pbs = 10000,
		.cir_period = 1,
		.cir_bytes_per_period = 1,
		.pir_period = 1,
		.pir_bytes_per_period = 1,
	},

	.n_users = 0,
};

static void
meter_init(struct meter *m)
{
	memset(m, 0, sizeof(struct meter));
	rte_meter_trtcm_config(&m->m, &meter_profile_default.profile);
	m->profile = &meter_profile_default;
	m->color_mask = RTE_COLOR_GREEN;

	meter_profile_default.n_users++;
}

static int
metarray_build(struct rte_swx_pipeline *p)
{
	struct metarray *m;

	if (!p->n_metarrays)
		return 0;

	p->metarray_runtime = calloc(p->n_metarrays, sizeof(struct metarray_runtime));
	CHECK(p->metarray_runtime, ENOMEM);

	TAILQ_FOREACH(m, &p->metarrays, node) {
		struct metarray_runtime *r = &p->metarray_runtime[m->id];
		uint32_t i;

		r->metarray = env_malloc(m->size * sizeof(struct meter),
					 RTE_CACHE_LINE_SIZE,
					 p->numa_node);
		CHECK(r->metarray, ENOMEM);

		for (i = 0; i < m->size; i++)
			meter_init(&r->metarray[i]);

		r->size_mask = m->size - 1;
	}

	return 0;
}

static void
metarray_build_free(struct rte_swx_pipeline *p)
{
	uint32_t i;

	if (!p->metarray_runtime)
		return;

	for (i = 0; i < p->n_metarrays; i++) {
		struct metarray *m = metarray_find_by_id(p, i);
		struct metarray_runtime *r = &p->metarray_runtime[i];

		env_free(r->metarray, m->size * sizeof(struct meter));
	}

	free(p->metarray_runtime);
	p->metarray_runtime = NULL;
}

static void
metarray_free(struct rte_swx_pipeline *p)
{
	metarray_build_free(p);

	/* Meter arrays. */
	for ( ; ; ) {
		struct metarray *elem;

		elem = TAILQ_FIRST(&p->metarrays);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->metarrays, elem, node);
		free(elem);
	}

	/* Meter profiles. */
	for ( ; ; ) {
		struct meter_profile *elem;

		elem = TAILQ_FIRST(&p->meter_profiles);
		if (!elem)
			break;

		TAILQ_REMOVE(&p->meter_profiles, elem, node);
		free(elem);
	}
}

/*
 * Pipeline.
 */

/* Global list of pipeline instances. */
TAILQ_HEAD(rte_swx_pipeline_list, rte_tailq_entry);

static struct rte_tailq_elem rte_swx_pipeline_tailq = {
	.name = "RTE_SWX_PIPELINE",
};

EAL_REGISTER_TAILQ(rte_swx_pipeline_tailq)

struct rte_swx_pipeline *
rte_swx_pipeline_find(const char *name)
{
	struct rte_swx_pipeline_list *pipeline_list;
	struct rte_tailq_entry *te = NULL;

	if (!name || !name[0] || (strnlen(name, RTE_SWX_NAME_SIZE) >= RTE_SWX_NAME_SIZE))
		return NULL;

	pipeline_list = RTE_TAILQ_CAST(rte_swx_pipeline_tailq.head, rte_swx_pipeline_list);

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, pipeline_list, next) {
		struct rte_swx_pipeline *p = (struct rte_swx_pipeline *)te->data;

		if (!strncmp(name, p->name, sizeof(p->name))) {
			rte_mcfg_tailq_read_unlock();
			return p;
		}
	}

	rte_mcfg_tailq_read_unlock();
	return NULL;
}

static int
pipeline_register(struct rte_swx_pipeline *p)
{
	struct rte_swx_pipeline_list *pipeline_list;
	struct rte_tailq_entry *te = NULL;

	pipeline_list = RTE_TAILQ_CAST(rte_swx_pipeline_tailq.head, rte_swx_pipeline_list);

	rte_mcfg_tailq_write_lock();

	TAILQ_FOREACH(te, pipeline_list, next) {
		struct rte_swx_pipeline *pipeline = (struct rte_swx_pipeline *)te->data;

		if (!strncmp(p->name, pipeline->name, sizeof(p->name))) {
			rte_mcfg_tailq_write_unlock();
			return -EEXIST;
		}
	}

	te = calloc(1, sizeof(struct rte_tailq_entry));
	if (!te) {
		rte_mcfg_tailq_write_unlock();
		return -ENOMEM;
	}

	te->data = (void *)p;
	TAILQ_INSERT_TAIL(pipeline_list, te, next);
	rte_mcfg_tailq_write_unlock();
	return 0;
}

static void
pipeline_unregister(struct rte_swx_pipeline *p)
{
	struct rte_swx_pipeline_list *pipeline_list;
	struct rte_tailq_entry *te = NULL;

	pipeline_list = RTE_TAILQ_CAST(rte_swx_pipeline_tailq.head, rte_swx_pipeline_list);

	rte_mcfg_tailq_write_lock();

	TAILQ_FOREACH(te, pipeline_list, next) {
		if (te->data == (void *)p) {
			TAILQ_REMOVE(pipeline_list, te, next);
			rte_mcfg_tailq_write_unlock();
			free(te);
			return;
		}
	}

	rte_mcfg_tailq_write_unlock();
}

void
rte_swx_pipeline_free(struct rte_swx_pipeline *p)
{
	void *lib;

	if (!p)
		return;

	if (p->name[0])
		pipeline_unregister(p);

	lib = p->lib;

	free(p->instruction_data);
	free(p->instructions);

	metarray_free(p);
	regarray_free(p);
	table_state_free(p);
	learner_free(p);
	selector_free(p);
	table_free(p);
	action_free(p);
	instruction_table_free(p);
	metadata_free(p);
	header_free(p);
	rss_free(p);
	hash_func_free(p);
	extern_func_free(p);
	extern_obj_free(p);
	mirroring_free(p);
	port_out_free(p);
	port_in_free(p);
	struct_free(p);

	free(p);

	if (lib)
		dlclose(lib);
}

static int
port_in_types_register(struct rte_swx_pipeline *p)
{
	int status;

	status = rte_swx_pipeline_port_in_type_register(p,
		"ethdev",
		&rte_swx_port_ethdev_reader_ops);
	if (status)
		return status;

	status = rte_swx_pipeline_port_in_type_register(p,
		"ring",
		&rte_swx_port_ring_reader_ops);
	if (status)
		return status;

#ifdef RTE_PORT_PCAP
	status = rte_swx_pipeline_port_in_type_register(p,
		"source",
		&rte_swx_port_source_ops);
	if (status)
		return status;
#endif

	status = rte_swx_pipeline_port_in_type_register(p,
		"fd",
		&rte_swx_port_fd_reader_ops);
	if (status)
		return status;

	return 0;
}

static int
port_out_types_register(struct rte_swx_pipeline *p)
{
	int status;

	status = rte_swx_pipeline_port_out_type_register(p,
		"ethdev",
		&rte_swx_port_ethdev_writer_ops);
	if (status)
		return status;

	status = rte_swx_pipeline_port_out_type_register(p,
		"ring",
		&rte_swx_port_ring_writer_ops);
	if (status)
		return status;

	status = rte_swx_pipeline_port_out_type_register(p,
		"sink",
		&rte_swx_port_sink_ops);
	if (status)
		return status;

	status = rte_swx_pipeline_port_out_type_register(p,
		"fd",
		&rte_swx_port_fd_writer_ops);
	if (status)
		return status;

	return 0;
}

static int
table_types_register(struct rte_swx_pipeline *p)
{
	int status;

	status = rte_swx_pipeline_table_type_register(p,
		"exact",
		RTE_SWX_TABLE_MATCH_EXACT,
		&rte_swx_table_exact_match_ops);
	if (status)
		return status;

	status = rte_swx_pipeline_table_type_register(p,
		"wildcard",
		RTE_SWX_TABLE_MATCH_WILDCARD,
		&rte_swx_table_wildcard_match_ops);
	if (status)
		return status;

	return 0;
}

static int
hash_funcs_register(struct rte_swx_pipeline *p)
{
	int status;

	status = rte_swx_pipeline_hash_func_register(p, "jhash", rte_jhash);
	if (status)
		return status;

	status = rte_swx_pipeline_hash_func_register(p, "crc32", rte_hash_crc);
	if (status)
		return status;

	return 0;
}

int
rte_swx_pipeline_config(struct rte_swx_pipeline **p, const char *name, int numa_node)
{
	struct rte_swx_pipeline *pipeline = NULL;
	int status = 0;

	/* Check input parameters. */
	CHECK(p, EINVAL);
	CHECK(!name || (strnlen(name, RTE_SWX_NAME_SIZE) < RTE_SWX_NAME_SIZE), EINVAL);

	/* Memory allocation. */
	pipeline = calloc(1, sizeof(struct rte_swx_pipeline));
	if (!pipeline) {
		status = -ENOMEM;
		goto error;
	}

	/* Initialization. */
	if (name)
		strcpy(pipeline->name, name);

	TAILQ_INIT(&pipeline->struct_types);
	TAILQ_INIT(&pipeline->port_in_types);
	TAILQ_INIT(&pipeline->ports_in);
	TAILQ_INIT(&pipeline->port_out_types);
	TAILQ_INIT(&pipeline->ports_out);
	TAILQ_INIT(&pipeline->extern_types);
	TAILQ_INIT(&pipeline->extern_objs);
	TAILQ_INIT(&pipeline->extern_funcs);
	TAILQ_INIT(&pipeline->hash_funcs);
	TAILQ_INIT(&pipeline->rss);
	TAILQ_INIT(&pipeline->headers);
	TAILQ_INIT(&pipeline->actions);
	TAILQ_INIT(&pipeline->table_types);
	TAILQ_INIT(&pipeline->tables);
	TAILQ_INIT(&pipeline->selectors);
	TAILQ_INIT(&pipeline->learners);
	TAILQ_INIT(&pipeline->regarrays);
	TAILQ_INIT(&pipeline->meter_profiles);
	TAILQ_INIT(&pipeline->metarrays);

	pipeline->n_structs = 1; /* Struct 0 is reserved for action_data. */
	pipeline->n_mirroring_slots = RTE_SWX_PACKET_MIRRORING_SLOTS_DEFAULT;
	pipeline->n_mirroring_sessions = RTE_SWX_PACKET_MIRRORING_SESSIONS_DEFAULT;
	pipeline->numa_node = numa_node;

	status = port_in_types_register(pipeline);
	if (status)
		goto error;

	status = port_out_types_register(pipeline);
	if (status)
		goto error;

	status = table_types_register(pipeline);
	if (status)
		goto error;

	status = hash_funcs_register(pipeline);
	if (status)
		goto error;

	if (pipeline->name[0]) {
		status = pipeline_register(pipeline);
		if (status)
			goto error;
	}

	*p = pipeline;
	return 0;

error:
	rte_swx_pipeline_free(pipeline);
	return status;
}

int
rte_swx_pipeline_instructions_config(struct rte_swx_pipeline *p,
				     const char **instructions,
				     uint32_t n_instructions)
{
	int err;
	uint32_t i;

	err = instruction_config(p, NULL, instructions, n_instructions);
	if (err)
		return err;

	/* Thread instruction pointer reset. */
	for (i = 0; i < RTE_SWX_PIPELINE_THREADS_MAX; i++) {
		struct thread *t = &p->threads[i];

		thread_ip_reset(p, t);
	}

	return 0;
}

int
rte_swx_pipeline_build(struct rte_swx_pipeline *p)
{
	struct rte_swx_port_sink_params drop_port_params = {
		.file_name = NULL,
	};
	int status;

	CHECK(p, EINVAL);
	CHECK(p->build_done == 0, EEXIST);

	status = port_in_build(p);
	if (status)
		goto error;

	/* Drop port. */
	status = rte_swx_pipeline_port_out_config(p,
						  p->n_ports_out,
						  "sink",
						  &drop_port_params);
	if (status)
		goto error;

	status = port_out_build(p);
	if (status)
		goto error;

	status = mirroring_build(p);
	if (status)
		goto error;

	status = struct_build(p);
	if (status)
		goto error;

	status = extern_obj_build(p);
	if (status)
		goto error;

	status = extern_func_build(p);
	if (status)
		goto error;

	status = hash_func_build(p);
	if (status)
		goto error;

	status = rss_build(p);
	if (status)
		goto error;

	status = header_build(p);
	if (status)
		goto error;

	status = metadata_build(p);
	if (status)
		goto error;

	status = instruction_table_build(p);
	if (status)
		goto error;

	status = action_build(p);
	if (status)
		goto error;

	status = table_build(p);
	if (status)
		goto error;

	status = selector_build(p);
	if (status)
		goto error;

	status = learner_build(p);
	if (status)
		goto error;

	status = table_state_build(p);
	if (status)
		goto error;

	status = regarray_build(p);
	if (status)
		goto error;

	status = metarray_build(p);
	if (status)
		goto error;

	p->build_done = 1;

	return 0;

error:
	metarray_build_free(p);
	regarray_build_free(p);
	table_state_build_free(p);
	learner_build_free(p);
	selector_build_free(p);
	table_build_free(p);
	action_build_free(p);
	instruction_table_build_free(p);
	metadata_build_free(p);
	header_build_free(p);
	rss_build_free(p);
	hash_func_build_free(p);
	extern_func_build_free(p);
	extern_obj_build_free(p);
	mirroring_build_free(p);
	port_out_build_free(p);
	port_in_build_free(p);
	struct_build_free(p);

	return status;
}

void
rte_swx_pipeline_run(struct rte_swx_pipeline *p, uint32_t n_instructions)
{
	uint32_t i;

	for (i = 0; i < n_instructions; i++)
		instr_exec(p);
}

void
rte_swx_pipeline_flush(struct rte_swx_pipeline *p)
{
	uint32_t i;

	for (i = 0; i < p->n_ports_out; i++) {
		struct port_out_runtime *port = &p->out[i];

		if (port->flush)
			port->flush(port->obj);
	}
}

/*
 * Control.
 */
int
rte_swx_ctl_pipeline_info_get(struct rte_swx_pipeline *p,
			      struct rte_swx_ctl_pipeline_info *pipeline)
{
	struct action *action;
	struct table *table;
	uint32_t n_actions = 0, n_tables = 0;

	if (!p || !pipeline)
		return -EINVAL;

	TAILQ_FOREACH(action, &p->actions, node)
		n_actions++;

	TAILQ_FOREACH(table, &p->tables, node)
		n_tables++;

	strcpy(pipeline->name, p->name);
	pipeline->n_ports_in = p->n_ports_in;
	pipeline->n_ports_out = p->n_ports_out;
	pipeline->n_mirroring_slots = p->n_mirroring_slots;
	pipeline->n_mirroring_sessions = p->n_mirroring_sessions;
	pipeline->n_actions = n_actions;
	pipeline->n_tables = n_tables;
	pipeline->n_selectors = p->n_selectors;
	pipeline->n_learners = p->n_learners;
	pipeline->n_regarrays = p->n_regarrays;
	pipeline->n_metarrays = p->n_metarrays;
	pipeline->n_rss = p->n_rss;

	return 0;
}

int
rte_swx_ctl_pipeline_numa_node_get(struct rte_swx_pipeline *p, int *numa_node)
{
	if (!p || !numa_node)
		return -EINVAL;

	*numa_node = p->numa_node;
	return 0;
}

int
rte_swx_ctl_action_info_get(struct rte_swx_pipeline *p,
			    uint32_t action_id,
			    struct rte_swx_ctl_action_info *action)
{
	struct action *a = NULL;

	if (!p || (action_id >= p->n_actions) || !action)
		return -EINVAL;

	a = action_find_by_id(p, action_id);
	if (!a)
		return -EINVAL;

	strcpy(action->name, a->name);
	action->n_args = a->st ? a->st->n_fields : 0;
	return 0;
}

int
rte_swx_ctl_action_arg_info_get(struct rte_swx_pipeline *p,
				uint32_t action_id,
				uint32_t action_arg_id,
				struct rte_swx_ctl_action_arg_info *action_arg)
{
	struct action *a = NULL;
	struct field *arg = NULL;

	if (!p || (action_id >= p->n_actions) || !action_arg)
		return -EINVAL;

	a = action_find_by_id(p, action_id);
	if (!a || !a->st || (action_arg_id >= a->st->n_fields))
		return -EINVAL;

	arg = &a->st->fields[action_arg_id];
	strcpy(action_arg->name, arg->name);
	action_arg->n_bits = arg->n_bits;
	action_arg->is_network_byte_order = a->args_endianness[action_arg_id];

	return 0;
}

int
rte_swx_ctl_table_info_get(struct rte_swx_pipeline *p,
			   uint32_t table_id,
			   struct rte_swx_ctl_table_info *table)
{
	struct table *t = NULL;

	if (!p || !table)
		return -EINVAL;

	t = table_find_by_id(p, table_id);
	if (!t)
		return -EINVAL;

	strcpy(table->name, t->name);
	strcpy(table->args, t->args);
	table->n_match_fields = t->n_fields;
	table->n_actions = t->n_actions;
	table->default_action_is_const = t->default_action_is_const;
	table->hash_func = t->hf ? t->hf->func : NULL;
	table->size = t->size;
	return 0;
}

int
rte_swx_ctl_table_match_field_info_get(struct rte_swx_pipeline *p,
	uint32_t table_id,
	uint32_t match_field_id,
	struct rte_swx_ctl_table_match_field_info *match_field)
{
	struct table *t;
	struct match_field *f;

	if (!p || (table_id >= p->n_tables) || !match_field)
		return -EINVAL;

	t = table_find_by_id(p, table_id);
	if (!t || (match_field_id >= t->n_fields))
		return -EINVAL;

	f = &t->fields[match_field_id];
	match_field->match_type = f->match_type;
	match_field->is_header = t->header ? 1 : 0;
	match_field->n_bits = f->field->n_bits;
	match_field->offset = f->field->offset;

	return 0;
}

int
rte_swx_ctl_table_action_info_get(struct rte_swx_pipeline *p,
	uint32_t table_id,
	uint32_t table_action_id,
	struct rte_swx_ctl_table_action_info *table_action)
{
	struct table *t;

	if (!p || (table_id >= p->n_tables) || !table_action)
		return -EINVAL;

	t = table_find_by_id(p, table_id);
	if (!t || (table_action_id >= t->n_actions))
		return -EINVAL;

	table_action->action_id = t->actions[table_action_id]->id;

	table_action->action_is_for_table_entries = t->action_is_for_table_entries[table_action_id];
	table_action->action_is_for_default_entry = t->action_is_for_default_entry[table_action_id];

	return 0;
}

int
rte_swx_ctl_table_ops_get(struct rte_swx_pipeline *p,
			  uint32_t table_id,
			  struct rte_swx_table_ops *table_ops,
			  int *is_stub)
{
	struct table *t;

	if (!p || (table_id >= p->n_tables))
		return -EINVAL;

	t = table_find_by_id(p, table_id);
	if (!t)
		return -EINVAL;

	if (t->type) {
		if (table_ops)
			memcpy(table_ops, &t->type->ops, sizeof(*table_ops));
		*is_stub = 0;
	} else {
		*is_stub = 1;
	}

	return 0;
}

int
rte_swx_ctl_selector_info_get(struct rte_swx_pipeline *p,
			      uint32_t selector_id,
			      struct rte_swx_ctl_selector_info *selector)
{
	struct selector *s = NULL;

	if (!p || !selector)
		return -EINVAL;

	s = selector_find_by_id(p, selector_id);
	if (!s)
		return -EINVAL;

	strcpy(selector->name, s->name);

	selector->n_selector_fields = s->n_selector_fields;
	selector->n_groups_max = s->n_groups_max;
	selector->n_members_per_group_max = s->n_members_per_group_max;

	return 0;
}

int
rte_swx_ctl_selector_group_id_field_info_get(struct rte_swx_pipeline *p,
	 uint32_t selector_id,
	 struct rte_swx_ctl_table_match_field_info *field)
{
	struct selector *s;

	if (!p || (selector_id >= p->n_selectors) || !field)
		return -EINVAL;

	s = selector_find_by_id(p, selector_id);
	if (!s)
		return -EINVAL;

	field->match_type = RTE_SWX_TABLE_MATCH_EXACT;
	field->is_header = 0;
	field->n_bits = s->group_id_field->n_bits;
	field->offset = s->group_id_field->offset;

	return 0;
}

int
rte_swx_ctl_selector_field_info_get(struct rte_swx_pipeline *p,
	 uint32_t selector_id,
	 uint32_t selector_field_id,
	 struct rte_swx_ctl_table_match_field_info *field)
{
	struct selector *s;
	struct field *f;

	if (!p || (selector_id >= p->n_selectors) || !field)
		return -EINVAL;

	s = selector_find_by_id(p, selector_id);
	if (!s || (selector_field_id >= s->n_selector_fields))
		return -EINVAL;

	f = s->selector_fields[selector_field_id];
	field->match_type = RTE_SWX_TABLE_MATCH_EXACT;
	field->is_header = s->selector_header ? 1 : 0;
	field->n_bits = f->n_bits;
	field->offset = f->offset;

	return 0;
}

int
rte_swx_ctl_selector_member_id_field_info_get(struct rte_swx_pipeline *p,
	 uint32_t selector_id,
	 struct rte_swx_ctl_table_match_field_info *field)
{
	struct selector *s;

	if (!p || (selector_id >= p->n_selectors) || !field)
		return -EINVAL;

	s = selector_find_by_id(p, selector_id);
	if (!s)
		return -EINVAL;

	field->match_type = RTE_SWX_TABLE_MATCH_EXACT;
	field->is_header = 0;
	field->n_bits = s->member_id_field->n_bits;
	field->offset = s->member_id_field->offset;

	return 0;
}

int
rte_swx_ctl_learner_info_get(struct rte_swx_pipeline *p,
			     uint32_t learner_id,
			     struct rte_swx_ctl_learner_info *learner)
{
	struct learner *l = NULL;

	if (!p || !learner)
		return -EINVAL;

	l = learner_find_by_id(p, learner_id);
	if (!l)
		return -EINVAL;

	strcpy(learner->name, l->name);

	learner->n_match_fields = l->n_fields;
	learner->n_actions = l->n_actions;
	learner->default_action_is_const = l->default_action_is_const;
	learner->size = l->size;
	learner->n_key_timeouts = l->n_timeouts;

	return 0;
}

int
rte_swx_ctl_learner_match_field_info_get(struct rte_swx_pipeline *p,
					 uint32_t learner_id,
					 uint32_t match_field_id,
					 struct rte_swx_ctl_table_match_field_info *match_field)
{
	struct learner *l;
	struct field *f;

	if (!p || (learner_id >= p->n_learners) || !match_field)
		return -EINVAL;

	l = learner_find_by_id(p, learner_id);
	if (!l || (match_field_id >= l->n_fields))
		return -EINVAL;

	f = l->fields[match_field_id];
	match_field->match_type = RTE_SWX_TABLE_MATCH_EXACT;
	match_field->is_header = l->header ? 1 : 0;
	match_field->n_bits = f->n_bits;
	match_field->offset = f->offset;

	return 0;
}

int
rte_swx_ctl_learner_action_info_get(struct rte_swx_pipeline *p,
				    uint32_t learner_id,
				    uint32_t learner_action_id,
				    struct rte_swx_ctl_table_action_info *learner_action)
{
	struct learner *l;

	if (!p || (learner_id >= p->n_learners) || !learner_action)
		return -EINVAL;

	l = learner_find_by_id(p, learner_id);
	if (!l || (learner_action_id >= l->n_actions))
		return -EINVAL;

	learner_action->action_id = l->actions[learner_action_id]->id;

	learner_action->action_is_for_table_entries =
		l->action_is_for_table_entries[learner_action_id];

	learner_action->action_is_for_default_entry =
		l->action_is_for_default_entry[learner_action_id];

	return 0;
}

int
rte_swx_ctl_pipeline_learner_timeout_get(struct rte_swx_pipeline *p,
					 uint32_t learner_id,
					 uint32_t timeout_id,
					 uint32_t *timeout)
{
	struct learner *l;

	if (!p || (learner_id >= p->n_learners) || !timeout)
		return -EINVAL;

	l = learner_find_by_id(p, learner_id);
	if (!l || (timeout_id >= l->n_timeouts))
		return -EINVAL;

	*timeout = l->timeout[timeout_id];
	return 0;
}

int
rte_swx_ctl_pipeline_learner_timeout_set(struct rte_swx_pipeline *p,
					 uint32_t learner_id,
					 uint32_t timeout_id,
					 uint32_t timeout)
{
	struct learner *l;
	struct rte_swx_table_state *ts;
	int status;

	if (!p || (learner_id >= p->n_learners) || !timeout)
		return -EINVAL;

	l = learner_find_by_id(p, learner_id);
	if (!l || (timeout_id >= l->n_timeouts))
		return -EINVAL;

	if (!p->build_done)
		return -EINVAL;

	ts = &p->table_state[p->n_tables + p->n_selectors + l->id];

	status = rte_swx_table_learner_timeout_update(ts->obj, timeout_id, timeout);
	if (status)
		return -EINVAL;

	l->timeout[timeout_id] = timeout;

	return 0;
}

int
rte_swx_pipeline_table_state_get(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state **table_state)
{
	if (!p || !table_state || !p->build_done)
		return -EINVAL;

	*table_state = p->table_state;
	return 0;
}

int
rte_swx_pipeline_table_state_set(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state *table_state)
{
	if (!p || !table_state || !p->build_done)
		return -EINVAL;

	p->table_state = table_state;
	return 0;
}

int
rte_swx_ctl_pipeline_port_in_stats_read(struct rte_swx_pipeline *p,
					uint32_t port_id,
					struct rte_swx_port_in_stats *stats)
{
	struct port_in *port;

	if (!p || !stats)
		return -EINVAL;

	port = port_in_find(p, port_id);
	if (!port)
		return -EINVAL;

	port->type->ops.stats_read(port->obj, stats);
	return 0;
}

int
rte_swx_ctl_pipeline_port_out_stats_read(struct rte_swx_pipeline *p,
					 uint32_t port_id,
					 struct rte_swx_port_out_stats *stats)
{
	struct port_out *port;

	if (!p || !stats)
		return -EINVAL;

	port = port_out_find(p, port_id);
	if (!port)
		return -EINVAL;

	port->type->ops.stats_read(port->obj, stats);
	return 0;
}

int
rte_swx_ctl_pipeline_table_stats_read(struct rte_swx_pipeline *p,
				      const char *table_name,
				      struct rte_swx_table_stats *stats)
{
	struct table *table;
	struct table_statistics *table_stats;

	if (!p || !table_name || !table_name[0] || !stats || !stats->n_pkts_action)
		return -EINVAL;

	table = table_find(p, table_name);
	if (!table)
		return -EINVAL;

	table_stats = &p->table_stats[table->id];

	memcpy(stats->n_pkts_action,
	       table_stats->n_pkts_action,
	       p->n_actions * sizeof(uint64_t));

	stats->n_pkts_hit = table_stats->n_pkts_hit[1];
	stats->n_pkts_miss = table_stats->n_pkts_hit[0];

	return 0;
}

int
rte_swx_ctl_pipeline_selector_stats_read(struct rte_swx_pipeline *p,
	const char *selector_name,
	struct rte_swx_pipeline_selector_stats *stats)
{
	struct selector *s;

	if (!p || !selector_name || !selector_name[0] || !stats)
		return -EINVAL;

	s = selector_find(p, selector_name);
	if (!s)
		return -EINVAL;

	stats->n_pkts = p->selector_stats[s->id].n_pkts;

	return 0;
}

int
rte_swx_ctl_pipeline_learner_stats_read(struct rte_swx_pipeline *p,
					const char *learner_name,
					struct rte_swx_learner_stats *stats)
{
	struct learner *l;
	struct learner_statistics *learner_stats;

	if (!p || !learner_name || !learner_name[0] || !stats || !stats->n_pkts_action)
		return -EINVAL;

	l = learner_find(p, learner_name);
	if (!l)
		return -EINVAL;

	learner_stats = &p->learner_stats[l->id];

	memcpy(stats->n_pkts_action,
	       learner_stats->n_pkts_action,
	       p->n_actions * sizeof(uint64_t));

	stats->n_pkts_hit = learner_stats->n_pkts_hit[1];
	stats->n_pkts_miss = learner_stats->n_pkts_hit[0];

	stats->n_pkts_learn_ok = learner_stats->n_pkts_learn[0];
	stats->n_pkts_learn_err = learner_stats->n_pkts_learn[1];

	stats->n_pkts_rearm = learner_stats->n_pkts_rearm;
	stats->n_pkts_forget = learner_stats->n_pkts_forget;

	return 0;
}

int
rte_swx_ctl_regarray_info_get(struct rte_swx_pipeline *p,
			      uint32_t regarray_id,
			      struct rte_swx_ctl_regarray_info *regarray)
{
	struct regarray *r;

	if (!p || !regarray)
		return -EINVAL;

	r = regarray_find_by_id(p, regarray_id);
	if (!r)
		return -EINVAL;

	strcpy(regarray->name, r->name);
	regarray->size = r->size;
	return 0;
}

int
rte_swx_ctl_pipeline_regarray_read(struct rte_swx_pipeline *p,
				   const char *regarray_name,
				   uint32_t regarray_index,
				   uint64_t *value)
{
	struct regarray *regarray;
	struct regarray_runtime *r;

	if (!p || !regarray_name || !value)
		return -EINVAL;

	regarray = regarray_find(p, regarray_name);
	if (!regarray || (regarray_index >= regarray->size))
		return -EINVAL;

	r = &p->regarray_runtime[regarray->id];
	*value = r->regarray[regarray_index];
	return 0;
}

int
rte_swx_ctl_pipeline_regarray_write(struct rte_swx_pipeline *p,
				   const char *regarray_name,
				   uint32_t regarray_index,
				   uint64_t value)
{
	struct regarray *regarray;
	struct regarray_runtime *r;

	if (!p || !regarray_name)
		return -EINVAL;

	regarray = regarray_find(p, regarray_name);
	if (!regarray || (regarray_index >= regarray->size))
		return -EINVAL;

	r = &p->regarray_runtime[regarray->id];
	r->regarray[regarray_index] = value;
	return 0;
}

int
rte_swx_ctl_metarray_info_get(struct rte_swx_pipeline *p,
			      uint32_t metarray_id,
			      struct rte_swx_ctl_metarray_info *metarray)
{
	struct metarray *m;

	if (!p || !metarray)
		return -EINVAL;

	m = metarray_find_by_id(p, metarray_id);
	if (!m)
		return -EINVAL;

	strcpy(metarray->name, m->name);
	metarray->size = m->size;
	return 0;
}

int
rte_swx_ctl_meter_profile_add(struct rte_swx_pipeline *p,
			      const char *name,
			      struct rte_meter_trtcm_params *params)
{
	struct meter_profile *mp;
	int status;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);
	CHECK(params, EINVAL);
	CHECK(!meter_profile_find(p, name), EEXIST);

	/* Node allocation. */
	mp = calloc(1, sizeof(struct meter_profile));
	CHECK(mp, ENOMEM);

	/* Node initialization. */
	strcpy(mp->name, name);
	memcpy(&mp->params, params, sizeof(struct rte_meter_trtcm_params));
	status = rte_meter_trtcm_profile_config(&mp->profile, params);
	if (status) {
		free(mp);
		CHECK(0, EINVAL);
	}

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->meter_profiles, mp, node);

	return 0;
}

int
rte_swx_ctl_meter_profile_delete(struct rte_swx_pipeline *p,
				 const char *name)
{
	struct meter_profile *mp;

	CHECK(p, EINVAL);
	CHECK_NAME(name, EINVAL);

	mp = meter_profile_find(p, name);
	CHECK(mp, EINVAL);
	CHECK(!mp->n_users, EBUSY);

	/* Remove node from tailq. */
	TAILQ_REMOVE(&p->meter_profiles, mp, node);
	free(mp);

	return 0;
}

int
rte_swx_ctl_meter_reset(struct rte_swx_pipeline *p,
			const char *metarray_name,
			uint32_t metarray_index)
{
	struct meter_profile *mp_old;
	struct metarray *metarray;
	struct metarray_runtime *metarray_runtime;
	struct meter *m;

	CHECK(p, EINVAL);
	CHECK_NAME(metarray_name, EINVAL);

	metarray = metarray_find(p, metarray_name);
	CHECK(metarray, EINVAL);
	CHECK(metarray_index < metarray->size, EINVAL);

	metarray_runtime = &p->metarray_runtime[metarray->id];
	m = &metarray_runtime->metarray[metarray_index];
	mp_old = m->profile;

	meter_init(m);

	mp_old->n_users--;

	return 0;
}

int
rte_swx_ctl_meter_set(struct rte_swx_pipeline *p,
		      const char *metarray_name,
		      uint32_t metarray_index,
		      const char *profile_name)
{
	struct meter_profile *mp, *mp_old;
	struct metarray *metarray;
	struct metarray_runtime *metarray_runtime;
	struct meter *m;

	CHECK(p, EINVAL);
	CHECK_NAME(metarray_name, EINVAL);

	metarray = metarray_find(p, metarray_name);
	CHECK(metarray, EINVAL);
	CHECK(metarray_index < metarray->size, EINVAL);

	mp = meter_profile_find(p, profile_name);
	CHECK(mp, EINVAL);

	metarray_runtime = &p->metarray_runtime[metarray->id];
	m = &metarray_runtime->metarray[metarray_index];
	mp_old = m->profile;

	memset(m, 0, sizeof(struct meter));
	rte_meter_trtcm_config(&m->m, &mp->profile);
	m->profile = mp;
	m->color_mask = RTE_COLORS;

	mp->n_users++;
	mp_old->n_users--;

	return 0;
}

int
rte_swx_ctl_meter_stats_read(struct rte_swx_pipeline *p,
			     const char *metarray_name,
			     uint32_t metarray_index,
			     struct rte_swx_ctl_meter_stats *stats)
{
	struct metarray *metarray;
	struct metarray_runtime *metarray_runtime;
	struct meter *m;

	CHECK(p, EINVAL);
	CHECK_NAME(metarray_name, EINVAL);

	metarray = metarray_find(p, metarray_name);
	CHECK(metarray, EINVAL);
	CHECK(metarray_index < metarray->size, EINVAL);

	CHECK(stats, EINVAL);

	metarray_runtime = &p->metarray_runtime[metarray->id];
	m = &metarray_runtime->metarray[metarray_index];

	memcpy(stats->n_pkts, m->n_pkts, sizeof(m->n_pkts));
	memcpy(stats->n_bytes, m->n_bytes, sizeof(m->n_bytes));

	return 0;
}

int
rte_swx_ctl_pipeline_mirroring_session_set(struct rte_swx_pipeline *p,
					   uint32_t session_id,
					   struct rte_swx_pipeline_mirroring_session_params *params)
{
	struct mirroring_session *s;

	CHECK(p, EINVAL);
	CHECK(p->build_done, EEXIST);
	CHECK(session_id < p->n_mirroring_sessions, EINVAL);
	CHECK(params, EINVAL);
	CHECK(params->port_id < p->n_ports_out, EINVAL);

	s = &p->mirroring_sessions[session_id];
	s->port_id = params->port_id;
	s->fast_clone = params->fast_clone;
	s->truncation_length = params->truncation_length ? params->truncation_length : UINT32_MAX;

	return 0;
}

static int
rte_swx_ctl_pipeline_table_lookup(struct rte_swx_pipeline *p,
				  const char *table_name,
				  uint8_t *key,
				  uint64_t *action_id,
				  uint8_t **action_data,
				  size_t *entry_id,
				  int *hit)
{
	struct table *t;
	void *mailbox = NULL;

	/* Check input arguments. */
	if (!p ||
	    !p->build_done ||
	    !table_name ||
	    !table_name[0] ||
	    !key ||
	    !entry_id ||
	    !hit)
		return -EINVAL;

	/* Find the table. */
	t = table_find(p, table_name);
	if (!t)
		return -EINVAL;

	if (!t->type) {
		*hit = 0;
		return 0;
	}

	/* Setup mailbox.  */
	if (t->type->ops.mailbox_size_get) {
		uint64_t mailbox_size;

		mailbox_size = t->type->ops.mailbox_size_get();
		if (mailbox_size) {
			mailbox = calloc(1, mailbox_size);
			if (!mailbox)
				return -ENOMEM;
		}
	}

	/* Table lookup operation. */
	key -= table_params_offset_get(t);

	for ( ; ; ) {
		struct rte_swx_table_state *ts = &p->table_state[t->id];
		int done;

		done = t->type->ops.lkp(ts->obj,
					mailbox,
					&key,
					action_id,
					action_data,
					entry_id,
					hit);
		if (done)
			break;
	}

	/* Free mailbox. */
	free(mailbox);

	return 0;
}

static int
rte_swx_ctl_pipeline_learner_lookup(struct rte_swx_pipeline *p,
				    const char *learner_name,
				    uint8_t *key,
				    uint64_t *action_id,
				    uint8_t **action_data,
				    size_t *entry_id,
				    int *hit)
{
	struct learner *l;
	void *mailbox = NULL;
	uint64_t mailbox_size, time;

	/* Check input arguments. */
	if (!p ||
	    !p->build_done ||
	    !learner_name ||
	    !learner_name[0] ||
	    !key ||
	    !entry_id ||
	    !hit)
		return -EINVAL;

	/* Find the learner table. */
	l = learner_find(p, learner_name);
	if (!l)
		return -EINVAL;

	/* Setup mailbox.  */
	mailbox_size = rte_swx_table_learner_mailbox_size_get();
	if (mailbox_size) {
		mailbox = calloc(1, mailbox_size);
		if (!mailbox)
			return -ENOMEM;
	}

	/* Learner table lookup operation. */
	key -= learner_params_offset_get(l);

	time = rte_get_tsc_cycles();

	for ( ; ; ) {
		uint32_t pos = p->n_tables + p->n_selectors + l->id;
		struct rte_swx_table_state *ts = &p->table_state[pos];
		int done;

		done = rte_swx_table_learner_lookup(ts->obj,
						    mailbox,
						    time,
						    &key,
						    action_id,
						    action_data,
						    entry_id,
						    hit);
		if (done)
			break;
	}

	/* Free mailbox. */
	free(mailbox);

	return 0;
}

static int
rte_swx_ctl_pipeline_table_entry_id_get(struct rte_swx_pipeline *p,
					const char *table_name,
					uint8_t *table_key,
					size_t *table_entry_id)
{
	struct table *t;
	struct learner *l;
	uint64_t action_id;
	uint8_t *action_data;
	size_t entry_id = 0;
	int hit = 0, status;

	/* Check input arguments. */
	if (!p ||
	    !p->build_done ||
	    !table_name ||
	    !table_name[0] ||
	    !table_key ||
	    !table_entry_id)
		return -EINVAL;

	t = table_find(p, table_name);
	l = learner_find(p, table_name);
	if (!t && !l)
		return -EINVAL;

	/* Table lookup operation. */
	if (t)
		status = rte_swx_ctl_pipeline_table_lookup(p,
							   table_name,
							   table_key,
							   &action_id,
							   &action_data,
							   &entry_id,
							   &hit);
	else
		status = rte_swx_ctl_pipeline_learner_lookup(p,
							     table_name,
							     table_key,
							     &action_id,
							     &action_data,
							     &entry_id,
							     &hit);
	if (status)
		return status;

	/* Reserve entry ID 0 for the table default entry. */
	*table_entry_id = hit ? (1 + entry_id) : 0;

	return 0;
}

int
rte_swx_ctl_pipeline_regarray_read_with_key(struct rte_swx_pipeline *p,
					    const char *regarray_name,
					    const char *table_name,
					    uint8_t *table_key,
					    uint64_t *value)
{
	size_t entry_id = 0;
	int status;

	status = rte_swx_ctl_pipeline_table_entry_id_get(p, table_name, table_key, &entry_id);
	if (status)
		return status;

	return rte_swx_ctl_pipeline_regarray_read(p, regarray_name, entry_id, value);
}

int
rte_swx_ctl_pipeline_regarray_write_with_key(struct rte_swx_pipeline *p,
					     const char *regarray_name,
					     const char *table_name,
					     uint8_t *table_key,
					     uint64_t value)
{
	size_t entry_id = 0;
	int status;

	status = rte_swx_ctl_pipeline_table_entry_id_get(p, table_name, table_key, &entry_id);
	if (status)
		return status;

	return rte_swx_ctl_pipeline_regarray_write(p, regarray_name, entry_id, value);
}

int
rte_swx_ctl_meter_reset_with_key(struct rte_swx_pipeline *p,
				 const char *metarray_name,
				 const char *table_name,
				 uint8_t *table_key)
{
	size_t entry_id = 0;
	int status;

	status = rte_swx_ctl_pipeline_table_entry_id_get(p, table_name, table_key, &entry_id);
	if (status)
		return status;

	return rte_swx_ctl_meter_reset(p, metarray_name, entry_id);
}

int
rte_swx_ctl_meter_set_with_key(struct rte_swx_pipeline *p,
			       const char *metarray_name,
			       const char *table_name,
			       uint8_t *table_key,
			       const char *profile_name)
{
	size_t entry_id = 0;
	int status;

	status = rte_swx_ctl_pipeline_table_entry_id_get(p, table_name, table_key, &entry_id);
	if (status)
		return status;

	return rte_swx_ctl_meter_set(p, metarray_name, entry_id, profile_name);
}

int
rte_swx_ctl_meter_stats_read_with_key(struct rte_swx_pipeline *p,
				      const char *metarray_name,
				      const char *table_name,
				      uint8_t *table_key,
				      struct rte_swx_ctl_meter_stats *stats)
{
	size_t entry_id = 0;
	int status;

	status = rte_swx_ctl_pipeline_table_entry_id_get(p, table_name, table_key, &entry_id);
	if (status)
		return status;

	return rte_swx_ctl_meter_stats_read(p, metarray_name, entry_id, stats);
}

int
rte_swx_ctl_rss_info_get(struct rte_swx_pipeline *p,
			 uint32_t rss_obj_id,
			 struct rte_swx_ctl_rss_info *info)
{
	struct rss *rss;

	/* Check the input arguments. */
	if (!p || !info)
		return -EINVAL;

	rss = rss_find_by_id(p, rss_obj_id);
	if (!rss)
		return -EINVAL;

	/* Read from the internal data structures. */
	strcpy(info->name, rss->name);
	return 0;
}

int
rte_swx_ctl_pipeline_rss_key_size_read(struct rte_swx_pipeline *p,
				       const char *rss_name,
				       uint32_t *key_size)
{
	struct rss *rss;
	struct rss_runtime *r;

	/* Check the input arguments. */
	CHECK(p, EINVAL);

	CHECK_NAME(rss_name, EINVAL);
	rss = rss_find(p, rss_name);
	CHECK(rss, EINVAL);
	r = p->rss_runtime[rss->id];

	CHECK(key_size, EINVAL);

	/* Read from the internal data structures. */
	*key_size = r->key_size;

	return 0;
}

int
rte_swx_ctl_pipeline_rss_key_read(struct rte_swx_pipeline *p,
				  const char *rss_name,
				  uint8_t *key)
{
	struct rss *rss;
	struct rss_runtime *r;

	/* Check the input arguments. */
	CHECK(p, EINVAL);

	CHECK_NAME(rss_name, EINVAL);
	rss = rss_find(p, rss_name);
	CHECK(rss, EINVAL);
	r = p->rss_runtime[rss->id];

	CHECK(key, EINVAL);

	/* Read from the internal data structures. */
	memcpy(key, r->key, r->key_size);

	return 0;
}

int
rte_swx_ctl_pipeline_rss_key_write(struct rte_swx_pipeline *p,
				   const char *rss_name,
				   uint32_t key_size,
				   uint8_t *key)
{
	struct rss *rss;
	struct rss_runtime *r, *r_new;

	/* Check the input arguments. */
	CHECK(p, EINVAL);

	CHECK_NAME(rss_name, EINVAL);
	rss = rss_find(p, rss_name);
	CHECK(rss, EINVAL);
	r = p->rss_runtime[rss->id];

	CHECK(key_size >= 4, EINVAL);
	CHECK(key, EINVAL);

	/* Allocate new RSS run-time entry. */
	r_new = malloc(sizeof(struct rss_runtime) + key_size * sizeof(uint32_t));
	if (!r_new)
		return -ENOMEM;

	/* Fill in the new RSS run-time entry. */
	r_new->key_size = key_size;
	memcpy(r_new->key, key, key_size);

	/* Commit the RSS run-time change atomically. */
	p->rss_runtime[rss->id] = r_new;

	/* Free the old RSS run-time entry. */
	free(r);

	return 0;
}

/*
 * Pipeline compilation.
 */
static const char *
instr_type_to_name(struct instruction *instr)
{
	switch (instr->type) {
	case INSTR_RX: return "INSTR_RX";

	case INSTR_TX: return "INSTR_TX";
	case INSTR_TX_I: return "INSTR_TX_I";
	case INSTR_DROP: return "INSTR_DROP";
	case INSTR_MIRROR: return "INSTR_MIRROR";
	case INSTR_RECIRCULATE: return "INSTR_RECIRCULATE";
	case INSTR_RECIRCID: return "INSTR_RECIRCID";

	case INSTR_HDR_EXTRACT: return "INSTR_HDR_EXTRACT";
	case INSTR_HDR_EXTRACT2: return "INSTR_HDR_EXTRACT2";
	case INSTR_HDR_EXTRACT3: return "INSTR_HDR_EXTRACT3";
	case INSTR_HDR_EXTRACT4: return "INSTR_HDR_EXTRACT4";
	case INSTR_HDR_EXTRACT5: return "INSTR_HDR_EXTRACT5";
	case INSTR_HDR_EXTRACT6: return "INSTR_HDR_EXTRACT6";
	case INSTR_HDR_EXTRACT7: return "INSTR_HDR_EXTRACT7";
	case INSTR_HDR_EXTRACT8: return "INSTR_HDR_EXTRACT8";

	case INSTR_HDR_EXTRACT_M: return "INSTR_HDR_EXTRACT_M";

	case INSTR_HDR_LOOKAHEAD: return "INSTR_HDR_LOOKAHEAD";

	case INSTR_HDR_EMIT: return "INSTR_HDR_EMIT";
	case INSTR_HDR_EMIT_TX: return "INSTR_HDR_EMIT_TX";
	case INSTR_HDR_EMIT2_TX: return "INSTR_HDR_EMIT2_TX";
	case INSTR_HDR_EMIT3_TX: return "INSTR_HDR_EMIT3_TX";
	case INSTR_HDR_EMIT4_TX: return "INSTR_HDR_EMIT4_TX";
	case INSTR_HDR_EMIT5_TX: return "INSTR_HDR_EMIT5_TX";
	case INSTR_HDR_EMIT6_TX: return "INSTR_HDR_EMIT6_TX";
	case INSTR_HDR_EMIT7_TX: return "INSTR_HDR_EMIT7_TX";
	case INSTR_HDR_EMIT8_TX: return "INSTR_HDR_EMIT8_TX";

	case INSTR_HDR_VALIDATE: return "INSTR_HDR_VALIDATE";
	case INSTR_HDR_INVALIDATE: return "INSTR_HDR_INVALIDATE";

	case INSTR_MOV: return "INSTR_MOV";
	case INSTR_MOV_MH: return "INSTR_MOV_MH";
	case INSTR_MOV_HM: return "INSTR_MOV_HM";
	case INSTR_MOV_HH: return "INSTR_MOV_HH";
	case INSTR_MOV_DMA: return "INSTR_MOV_DMA";
	case INSTR_MOV_128: return "INSTR_MOV_128";
	case INSTR_MOV_128_32: return "INSTR_MOV_128_32";
	case INSTR_MOV_I: return "INSTR_MOV_I";

	case INSTR_DMA_HT: return "INSTR_DMA_HT";
	case INSTR_DMA_HT2: return "INSTR_DMA_HT2";
	case INSTR_DMA_HT3: return "INSTR_DMA_HT3";
	case INSTR_DMA_HT4: return "INSTR_DMA_HT4";
	case INSTR_DMA_HT5: return "INSTR_DMA_HT5";
	case INSTR_DMA_HT6: return "INSTR_DMA_HT6";
	case INSTR_DMA_HT7: return "INSTR_DMA_HT7";
	case INSTR_DMA_HT8: return "INSTR_DMA_HT8";

	case INSTR_ALU_ADD: return "INSTR_ALU_ADD";
	case INSTR_ALU_ADD_MH: return "INSTR_ALU_ADD_MH";
	case INSTR_ALU_ADD_HM: return "INSTR_ALU_ADD_HM";
	case INSTR_ALU_ADD_HH: return "INSTR_ALU_ADD_HH";
	case INSTR_ALU_ADD_MI: return "INSTR_ALU_ADD_MI";
	case INSTR_ALU_ADD_HI: return "INSTR_ALU_ADD_HI";

	case INSTR_ALU_SUB: return "INSTR_ALU_SUB";
	case INSTR_ALU_SUB_MH: return "INSTR_ALU_SUB_MH";
	case INSTR_ALU_SUB_HM: return "INSTR_ALU_SUB_HM";
	case INSTR_ALU_SUB_HH: return "INSTR_ALU_SUB_HH";
	case INSTR_ALU_SUB_MI: return "INSTR_ALU_SUB_MI";
	case INSTR_ALU_SUB_HI: return "INSTR_ALU_SUB_HI";

	case INSTR_ALU_CKADD_FIELD: return "INSTR_ALU_CKADD_FIELD";
	case INSTR_ALU_CKADD_STRUCT20: return "INSTR_ALU_CKADD_STRUCT20";
	case INSTR_ALU_CKADD_STRUCT: return "INSTR_ALU_CKADD_STRUCT";
	case INSTR_ALU_CKSUB_FIELD: return "INSTR_ALU_CKSUB_FIELD";

	case INSTR_ALU_AND: return "INSTR_ALU_AND";
	case INSTR_ALU_AND_MH: return "INSTR_ALU_AND_MH";
	case INSTR_ALU_AND_HM: return "INSTR_ALU_AND_HM";
	case INSTR_ALU_AND_HH: return "INSTR_ALU_AND_HH";
	case INSTR_ALU_AND_I: return "INSTR_ALU_AND_I";

	case INSTR_ALU_OR: return "INSTR_ALU_OR";
	case INSTR_ALU_OR_MH: return "INSTR_ALU_OR_MH";
	case INSTR_ALU_OR_HM: return "INSTR_ALU_OR_HM";
	case INSTR_ALU_OR_HH: return "INSTR_ALU_OR_HH";
	case INSTR_ALU_OR_I: return "INSTR_ALU_OR_I";

	case INSTR_ALU_XOR: return "INSTR_ALU_XOR";
	case INSTR_ALU_XOR_MH: return "INSTR_ALU_XOR_MH";
	case INSTR_ALU_XOR_HM: return "INSTR_ALU_XOR_HM";
	case INSTR_ALU_XOR_HH: return "INSTR_ALU_XOR_HH";
	case INSTR_ALU_XOR_I: return "INSTR_ALU_XOR_I";

	case INSTR_ALU_SHL: return "INSTR_ALU_SHL";
	case INSTR_ALU_SHL_MH: return "INSTR_ALU_SHL_MH";
	case INSTR_ALU_SHL_HM: return "INSTR_ALU_SHL_HM";
	case INSTR_ALU_SHL_HH: return "INSTR_ALU_SHL_HH";
	case INSTR_ALU_SHL_MI: return "INSTR_ALU_SHL_MI";
	case INSTR_ALU_SHL_HI: return "INSTR_ALU_SHL_HI";

	case INSTR_ALU_SHR: return "INSTR_ALU_SHR";
	case INSTR_ALU_SHR_MH: return "INSTR_ALU_SHR_MH";
	case INSTR_ALU_SHR_HM: return "INSTR_ALU_SHR_HM";
	case INSTR_ALU_SHR_HH: return "INSTR_ALU_SHR_HH";
	case INSTR_ALU_SHR_MI: return "INSTR_ALU_SHR_MI";
	case INSTR_ALU_SHR_HI: return "INSTR_ALU_SHR_HI";

	case INSTR_REGPREFETCH_RH: return "INSTR_REGPREFETCH_RH";
	case INSTR_REGPREFETCH_RM: return "INSTR_REGPREFETCH_RM";
	case INSTR_REGPREFETCH_RI: return "INSTR_REGPREFETCH_RI";

	case INSTR_REGRD_HRH: return "INSTR_REGRD_HRH";
	case INSTR_REGRD_HRM: return "INSTR_REGRD_HRM";
	case INSTR_REGRD_HRI: return "INSTR_REGRD_HRI";
	case INSTR_REGRD_MRH: return "INSTR_REGRD_MRH";
	case INSTR_REGRD_MRM: return "INSTR_REGRD_MRM";
	case INSTR_REGRD_MRI: return "INSTR_REGRD_MRI";

	case INSTR_REGWR_RHH: return "INSTR_REGWR_RHH";
	case INSTR_REGWR_RHM: return "INSTR_REGWR_RHM";
	case INSTR_REGWR_RHI: return "INSTR_REGWR_RHI";
	case INSTR_REGWR_RMH: return "INSTR_REGWR_RMH";
	case INSTR_REGWR_RMM: return "INSTR_REGWR_RMM";
	case INSTR_REGWR_RMI: return "INSTR_REGWR_RMI";
	case INSTR_REGWR_RIH: return "INSTR_REGWR_RIH";
	case INSTR_REGWR_RIM: return "INSTR_REGWR_RIM";
	case INSTR_REGWR_RII: return "INSTR_REGWR_RII";

	case INSTR_REGADD_RHH: return "INSTR_REGADD_RHH";
	case INSTR_REGADD_RHM: return "INSTR_REGADD_RHM";
	case INSTR_REGADD_RHI: return "INSTR_REGADD_RHI";
	case INSTR_REGADD_RMH: return "INSTR_REGADD_RMH";
	case INSTR_REGADD_RMM: return "INSTR_REGADD_RMM";
	case INSTR_REGADD_RMI: return "INSTR_REGADD_RMI";
	case INSTR_REGADD_RIH: return "INSTR_REGADD_RIH";
	case INSTR_REGADD_RIM: return "INSTR_REGADD_RIM";
	case INSTR_REGADD_RII: return "INSTR_REGADD_RII";

	case INSTR_METPREFETCH_H: return "INSTR_METPREFETCH_H";
	case INSTR_METPREFETCH_M: return "INSTR_METPREFETCH_M";
	case INSTR_METPREFETCH_I: return "INSTR_METPREFETCH_I";

	case INSTR_METER_HHM: return "INSTR_METER_HHM";
	case INSTR_METER_HHI: return "INSTR_METER_HHI";
	case INSTR_METER_HMM: return "INSTR_METER_HMM";
	case INSTR_METER_HMI: return "INSTR_METER_HMI";
	case INSTR_METER_MHM: return "INSTR_METER_MHM";
	case INSTR_METER_MHI: return "INSTR_METER_MHI";
	case INSTR_METER_MMM: return "INSTR_METER_MMM";
	case INSTR_METER_MMI: return "INSTR_METER_MMI";
	case INSTR_METER_IHM: return "INSTR_METER_IHM";
	case INSTR_METER_IHI: return "INSTR_METER_IHI";
	case INSTR_METER_IMM: return "INSTR_METER_IMM";
	case INSTR_METER_IMI: return "INSTR_METER_IMI";

	case INSTR_TABLE: return "INSTR_TABLE";
	case INSTR_TABLE_AF: return "INSTR_TABLE_AF";
	case INSTR_SELECTOR: return "INSTR_SELECTOR";
	case INSTR_LEARNER: return "INSTR_LEARNER";
	case INSTR_LEARNER_AF: return "INSTR_LEARNER_AF";

	case INSTR_LEARNER_LEARN: return "INSTR_LEARNER_LEARN";
	case INSTR_LEARNER_REARM: return "INSTR_LEARNER_REARM";
	case INSTR_LEARNER_REARM_NEW: return "INSTR_LEARNER_REARM_NEW";
	case INSTR_LEARNER_FORGET: return "INSTR_LEARNER_FORGET";
	case INSTR_ENTRYID: return "INSTR_ENTRYID";

	case INSTR_EXTERN_OBJ: return "INSTR_EXTERN_OBJ";
	case INSTR_EXTERN_FUNC: return "INSTR_EXTERN_FUNC";
	case INSTR_HASH_FUNC: return "INSTR_HASH_FUNC";
	case INSTR_RSS: return "INSTR_RSS";

	case INSTR_JMP: return "INSTR_JMP";
	case INSTR_JMP_VALID: return "INSTR_JMP_VALID";
	case INSTR_JMP_INVALID: return "INSTR_JMP_INVALID";
	case INSTR_JMP_HIT: return "INSTR_JMP_HIT";
	case INSTR_JMP_MISS: return "INSTR_JMP_MISS";
	case INSTR_JMP_ACTION_HIT: return "INSTR_JMP_ACTION_HIT";
	case INSTR_JMP_ACTION_MISS: return "INSTR_JMP_ACTION_MISS";
	case INSTR_JMP_EQ: return "INSTR_JMP_EQ";
	case INSTR_JMP_EQ_MH: return "INSTR_JMP_EQ_MH";
	case INSTR_JMP_EQ_HM: return "INSTR_JMP_EQ_HM";
	case INSTR_JMP_EQ_HH: return "INSTR_JMP_EQ_HH";
	case INSTR_JMP_EQ_I: return "INSTR_JMP_EQ_I";
	case INSTR_JMP_NEQ: return "INSTR_JMP_NEQ";
	case INSTR_JMP_NEQ_MH: return "INSTR_JMP_NEQ_MH";
	case INSTR_JMP_NEQ_HM: return "INSTR_JMP_NEQ_HM";
	case INSTR_JMP_NEQ_HH: return "INSTR_JMP_NEQ_HH";
	case INSTR_JMP_NEQ_I: return "INSTR_JMP_NEQ_I";
	case INSTR_JMP_LT: return "INSTR_JMP_LT";
	case INSTR_JMP_LT_MH: return "INSTR_JMP_LT_MH";
	case INSTR_JMP_LT_HM: return "INSTR_JMP_LT_HM";
	case INSTR_JMP_LT_HH: return "INSTR_JMP_LT_HH";
	case INSTR_JMP_LT_MI: return "INSTR_JMP_LT_MI";
	case INSTR_JMP_LT_HI: return "INSTR_JMP_LT_HI";
	case INSTR_JMP_GT: return "INSTR_JMP_GT";
	case INSTR_JMP_GT_MH: return "INSTR_JMP_GT_MH";
	case INSTR_JMP_GT_HM: return "INSTR_JMP_GT_HM";
	case INSTR_JMP_GT_HH: return "INSTR_JMP_GT_HH";
	case INSTR_JMP_GT_MI: return "INSTR_JMP_GT_MI";
	case INSTR_JMP_GT_HI: return "INSTR_JMP_GT_HI";

	case INSTR_RETURN: return "INSTR_RETURN";

	default: return "INSTR_UNKNOWN";
	}
}

typedef void
(*instruction_export_t)(struct instruction *, FILE *);

static void
instr_io_export(struct instruction *instr, FILE *f)
{
	uint32_t n_io = 0, n_io_imm = 0, n_hdrs = 0, i;

	/* n_io, n_io_imm, n_hdrs. */
	if (instr->type == INSTR_RX ||
	    instr->type == INSTR_TX ||
	    instr->type == INSTR_HDR_EXTRACT_M ||
	    (instr->type >= INSTR_HDR_EMIT_TX && instr->type <= INSTR_HDR_EMIT8_TX))
		n_io = 1;

	if (instr->type == INSTR_TX_I)
		n_io_imm = 1;

	if (instr->type >= INSTR_HDR_EXTRACT && instr->type <= INSTR_HDR_EXTRACT8)
		n_hdrs = 1 + (instr->type - INSTR_HDR_EXTRACT);

	if (instr->type == INSTR_HDR_EXTRACT_M ||
	    instr->type == INSTR_HDR_LOOKAHEAD ||
	    instr->type == INSTR_HDR_EMIT)
		n_hdrs = 1;

	if (instr->type >= INSTR_HDR_EMIT_TX && instr->type <= INSTR_HDR_EMIT8_TX)
		n_hdrs = 1 + (instr->type - INSTR_HDR_EMIT_TX);

	/* instr. */
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n",
		instr_type_to_name(instr));

	/* instr.io. */
	if (n_io || n_io_imm || n_hdrs)
		fprintf(f,
			"\t\t.io = {\n");

	/* instr.io.io. */
	if (n_io)
		fprintf(f,
			"\t\t\t.io = {\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t},\n",
			instr->io.io.offset,
			instr->io.io.n_bits);

	if (n_io_imm)
		fprintf(f,
			"\t\t\t.io = {\n"
			"\t\t\t\t.val = %u,\n"
			"\t\t\t},\n",
			instr->io.io.val);

	/* instr.io.hdr. */
	if (n_hdrs) {
		fprintf(f,
			"\t\t.hdr = {\n");

		/* instr.io.hdr.header_id. */
		fprintf(f,
			"\t\t\t.header_id = {");

		for (i = 0; i < n_hdrs; i++)
			fprintf(f,
				"%u, ",
				instr->io.hdr.header_id[i]);

		fprintf(f,
			"},\n");

		/* instr.io.hdr.struct_id. */
		fprintf(f,
			"\t\t\t.struct_id = {");

		for (i = 0; i < n_hdrs; i++)
			fprintf(f,
				"%u, ",
				instr->io.hdr.struct_id[i]);

		fprintf(f,
			"},\n");

		/* instr.io.hdr.n_bytes. */
		fprintf(f,
			"\t\t\t.n_bytes = {");

		for (i = 0; i < n_hdrs; i++)
			fprintf(f,
				"%u, ",
				instr->io.hdr.n_bytes[i]);

		fprintf(f,
			"},\n");

		/* instr.io.hdr - closing curly brace. */
		fprintf(f,
			"\t\t\t}\n,");
	}

	/* instr.io - closing curly brace. */
	if (n_io || n_io_imm || n_hdrs)
		fprintf(f,
			"\t\t},\n");

	/* instr - closing curly brace. */
	fprintf(f,
		"\t},\n");
}

static void
instr_mirror_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.mirror = {\n"
		"\t\t\t.dst = {\n"
		"\t\t\t\t.struct_id = %u,\n"
		"\t\t\t\t.n_bits = %u,\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t}\n,"
		"\t\t\t.src = {\n"
		"\t\t\t\t.struct_id = %u,\n"
		"\t\t\t\t.n_bits = %u,\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t}\n,"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->mirror.dst.struct_id,
		instr->mirror.dst.n_bits,
		instr->mirror.dst.offset,
		instr->mirror.src.struct_id,
		instr->mirror.src.n_bits,
		instr->mirror.src.offset);
}

static void
instr_recirculate_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t},\n",
		instr_type_to_name(instr));
}

static void
instr_recircid_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.io = {\n"
		"\t\t\t.io = {\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t\t.n_bits = %u,\n"
		"\t\t\t},\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->io.io.offset,
		instr->io.io.n_bits);
}

static void
instr_hdr_validate_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.valid = {\n"
		"\t\t\t.header_id = %u,\n"
		"\t\t\t.struct_id = %u,\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->valid.header_id,
		instr->valid.struct_id);
}

static void
instr_mov_export(struct instruction *instr, FILE *f)
{
	if (instr->type != INSTR_MOV_I)
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.mov = {\n"
			"\t\t\t.dst = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n"
			"\t\t\t.src = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->mov.dst.struct_id,
			instr->mov.dst.n_bits,
			instr->mov.dst.offset,
			instr->mov.src.struct_id,
			instr->mov.src.n_bits,
			instr->mov.src.offset);
	else
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.mov = {\n"
			"\t\t\t.dst = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t}\n,"
			"\t\t\t.src_val = %" PRIu64 ",\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->mov.dst.struct_id,
			instr->mov.dst.n_bits,
			instr->mov.dst.offset,
			instr->mov.src_val);
}

static void
instr_dma_ht_export(struct instruction *instr, FILE *f)
{
	uint32_t n_dma = 0, i;

	/* n_dma. */
	n_dma = 1 + (instr->type - INSTR_DMA_HT);

	/* instr. */
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n",
		instr_type_to_name(instr));

	/* instr.dma. */
	fprintf(f,
		"\t\t.dma = {\n");

	/* instr.dma.dst. */
	fprintf(f,
		"\t\t\t.dst = {\n");

	/* instr.dma.dst.header_id. */
	fprintf(f,
		"\t\t\t\t.header_id = {");

	for (i = 0; i < n_dma; i++)
		fprintf(f,
			"%u, ",
			instr->dma.dst.header_id[i]);

	fprintf(f,
		"},\n");

	/* instr.dma.dst.struct_id. */
	fprintf(f,
		"\t\t\t\t.struct_id = {");

	for (i = 0; i < n_dma; i++)
		fprintf(f,
			"%u, ",
			instr->dma.dst.struct_id[i]);

	fprintf(f,
		"},\n");

	/* instr.dma.dst - closing curly brace. */
	fprintf(f,
		"\t\t\t},\n");

	/* instr.dma.src. */
	fprintf(f,
		"\t\t\t.src = {\n");

	/* instr.dma.src.offset. */
	fprintf(f,
		"\t\t\t\t.offset = {");

	for (i = 0; i < n_dma; i++)
		fprintf(f,
			"%u, ",
			instr->dma.src.offset[i]);

	fprintf(f,
		"},\n");

	/* instr.dma.src - closing curly brace. */
	fprintf(f,
		"\t\t\t},\n");

	/* instr.dma.n_bytes. */
	fprintf(f,
		"\t\t\t.n_bytes = {");

	for (i = 0; i < n_dma; i++)
		fprintf(f,
			"%u, ",
			instr->dma.n_bytes[i]);

	fprintf(f,
		"},\n");

	/* instr.dma - closing curly brace. */
	fprintf(f,
		"\t\t},\n");

	/* instr - closing curly brace. */
	fprintf(f,
		"\t},\n");
}

static void
instr_alu_export(struct instruction *instr, FILE *f)
{
	int imm = 0;

	if (instr->type == INSTR_ALU_ADD_MI ||
	    instr->type == INSTR_ALU_ADD_HI ||
	    instr->type == INSTR_ALU_SUB_MI ||
	    instr->type == INSTR_ALU_SUB_HI ||
	    instr->type == INSTR_ALU_SHL_MI ||
	    instr->type == INSTR_ALU_SHL_HI ||
	    instr->type == INSTR_ALU_SHR_MI ||
	    instr->type == INSTR_ALU_SHR_HI ||
	    instr->type == INSTR_ALU_AND_I ||
	    instr->type == INSTR_ALU_OR_I ||
	    instr->type == INSTR_ALU_XOR_I)
		imm = 1;

	if (!imm)
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.alu = {\n"
			"\t\t\t.dst = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n"
			"\t\t\t.src = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->alu.dst.struct_id,
			instr->alu.dst.n_bits,
			instr->alu.dst.offset,
			instr->alu.src.struct_id,
			instr->alu.src.n_bits,
			instr->alu.src.offset);
	else
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.alu = {\n"
			"\t\t\t.dst = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t}\n,"
			"\t\t\t.src_val = %" PRIu64 ",\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->alu.dst.struct_id,
			instr->alu.dst.n_bits,
			instr->alu.dst.offset,
			instr->alu.src_val);
}

static void
instr_hash_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.hash_func = {\n"
		"\t\t\t.hash_func_id = %u,\n"
		"\t\t\t.dst = {\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t\t.n_bits = %u,\n"
		"\t\t\t},\n"
		"\t\t\t.src = {\n"
		"\t\t\t\t.struct_id = %u,\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t\t.n_bytes = %u,\n"
		"\t\t\t},\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->hash_func.hash_func_id,
		instr->hash_func.dst.offset,
		instr->hash_func.dst.n_bits,
		instr->hash_func.src.struct_id,
		instr->hash_func.src.offset,
		instr->hash_func.src.n_bytes);
}

static void
instr_rss_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.rss = {\n"
		"\t\t\t.rss_obj_id = %u,\n"
		"\t\t\t.dst = {\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t\t.n_bits = %u,\n"
		"\t\t\t},\n"
		"\t\t\t.src = {\n"
		"\t\t\t\t.struct_id = %u,\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t\t.n_bytes = %u,\n"
		"\t\t\t},\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->rss.rss_obj_id,
		instr->rss.dst.offset,
		instr->rss.dst.n_bits,
		instr->rss.src.struct_id,
		instr->rss.src.offset,
		instr->rss.src.n_bytes);
}

static void
instr_reg_export(struct instruction *instr __rte_unused, FILE *f __rte_unused)
{
	int prefetch  = 0, idx_imm = 0, src_imm = 0;

	if (instr->type == INSTR_REGPREFETCH_RH ||
	    instr->type == INSTR_REGPREFETCH_RM ||
	    instr->type == INSTR_REGPREFETCH_RI)
		prefetch = 1;

	/* index is the 3rd operand for the regrd instruction and the 2nd
	 * operand for the regwr and regadd instructions.
	 */
	if (instr->type == INSTR_REGPREFETCH_RI ||
	    instr->type == INSTR_REGRD_HRI ||
	    instr->type == INSTR_REGRD_MRI ||
	    instr->type == INSTR_REGWR_RIH ||
	    instr->type == INSTR_REGWR_RIM ||
	    instr->type == INSTR_REGWR_RII ||
	    instr->type == INSTR_REGADD_RIH ||
	    instr->type == INSTR_REGADD_RIM ||
	    instr->type == INSTR_REGADD_RII)
		idx_imm = 1;

	/* src is the 3rd operand for the regwr and regadd instructions. */
	if (instr->type == INSTR_REGWR_RHI ||
	    instr->type == INSTR_REGWR_RMI ||
	    instr->type == INSTR_REGWR_RII ||
	    instr->type == INSTR_REGADD_RHI ||
	    instr->type == INSTR_REGADD_RMI ||
	    instr->type == INSTR_REGADD_RII)
		src_imm = 1;

	/* instr.regarray.regarray_id. */
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.regarray = {\n"
		"\t\t\t.regarray_id = %u,\n",
		instr_type_to_name(instr),
		instr->regarray.regarray_id);

	/* instr.regarray.idx / instr.regarray.idx_val. */
	if (!idx_imm)
		fprintf(f,
			"\t\t\t\t.idx = {\n"
			"\t\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t\t.offset = %u,\n"
			"\t\t\t\t},\n",
			instr->regarray.idx.struct_id,
			instr->regarray.idx.n_bits,
			instr->regarray.idx.offset);
	else
		fprintf(f,
			"\t\t\t\t.idx_val = %u,\n",
			instr->regarray.idx_val);

	/* instr.regarray.dstsrc / instr.regarray.dstsrc_val. */
	if (!prefetch) {
		if (!src_imm)
			fprintf(f,
				"\t\t\t\t.dstsrc = {\n"
				"\t\t\t\t\t.struct_id = %u,\n"
				"\t\t\t\t\t.n_bits = %u,\n"
				"\t\t\t\t\t.offset = %u,\n"
				"\t\t\t\t},\n",
				instr->regarray.dstsrc.struct_id,
				instr->regarray.dstsrc.n_bits,
				instr->regarray.dstsrc.offset);
		else
			fprintf(f,
				"\t\t\t\t.dstsrc_val = %" PRIu64 ",\n",
				instr->regarray.dstsrc_val);
	}

	/* instr.regarray and instr - closing curly braces. */
	fprintf(f,
		"\t\t},\n"
		"\t},\n");
}

static void
instr_meter_export(struct instruction *instr __rte_unused, FILE *f __rte_unused)
{
	int prefetch  = 0, idx_imm = 0, color_in_imm = 0;

	if (instr->type == INSTR_METPREFETCH_H ||
	    instr->type == INSTR_METPREFETCH_M ||
	    instr->type == INSTR_METPREFETCH_I)
		prefetch = 1;

	/* idx_imm. */
	if (instr->type == INSTR_METPREFETCH_I ||
	    instr->type == INSTR_METER_IHM ||
	    instr->type == INSTR_METER_IHI ||
	    instr->type == INSTR_METER_IMM ||
	    instr->type == INSTR_METER_IMI)
		idx_imm = 1;

	/* color_in_imm. */
	if (instr->type == INSTR_METER_HHI ||
	    instr->type == INSTR_METER_HMI ||
	    instr->type == INSTR_METER_MHI ||
	    instr->type == INSTR_METER_MMI ||
	    instr->type == INSTR_METER_IHI ||
	    instr->type == INSTR_METER_IMI)
		color_in_imm = 1;

	/* instr.meter.metarray_id. */
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.meter = {\n"
		"\t\t\t.metarray_id = %u,\n",
		instr_type_to_name(instr),
		instr->meter.metarray_id);

	/* instr.meter.idx / instr.meter.idx_val. */
	if (!idx_imm)
		fprintf(f,
			"\t\t\t.idx = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n",
			instr->meter.idx.struct_id,
			instr->meter.idx.n_bits,
			instr->meter.idx.offset);
	else
		fprintf(f,
			"\t\t\t.idx_val = %u,\n",
			instr->meter.idx_val);

	if (!prefetch) {
		/* instr.meter.length. */
		fprintf(f,
			"\t\t\t.length = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n",
			instr->meter.length.struct_id,
			instr->meter.length.n_bits,
			instr->meter.length.offset);

		/* instr.meter.color_in / instr.meter.color_in_val. */
		if (!color_in_imm)
			fprintf(f,
				"\t\t\t.color_in = {\n"
				"\t\t\t\t.struct_id = %u,\n"
				"\t\t\t\t.n_bits = %u,\n"
				"\t\t\t\t.offset = %u,\n"
				"\t\t\t},\n",
				instr->meter.color_in.struct_id,
				instr->meter.color_in.n_bits,
				instr->meter.color_in.offset);
		else
			fprintf(f,
				"\t\t\t.color_in_val = %u,\n",
				(uint32_t)instr->meter.color_in_val);

		/* instr.meter.color_out. */
		fprintf(f,
			"\t\t\t.color_out = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n",
			instr->meter.color_out.struct_id,
			instr->meter.color_out.n_bits,
			instr->meter.color_out.offset);
	}

	/* instr.meter and instr - closing curly braces. */
	fprintf(f,
		"\t\t},\n"
		"\t},\n");
}

static void
instr_table_export(struct instruction *instr,
		FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.table = {\n"
		"\t\t\t.table_id = %u,\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->table.table_id);
}

static void
instr_learn_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.learn = {\n"
		"\t\t\t.action_id = %u,\n"
		"\t\t\t.mf_first_arg_offset = %u,\n"
		"\t\t\t.mf_timeout_id_offset = %u,\n"
		"\t\t\t.mf_timeout_id_n_bits = %u,\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->learn.action_id,
		instr->learn.mf_first_arg_offset,
		instr->learn.mf_timeout_id_offset,
		instr->learn.mf_timeout_id_n_bits);
}

static void
instr_rearm_export(struct instruction *instr, FILE *f)
{
	if (instr->type == INSTR_LEARNER_REARM)
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t},\n",
			instr_type_to_name(instr));
	else
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.learn = {\n"
			"\t\t\t.mf_timeout_id_offset = %u,\n"
			"\t\t\t.mf_timeout_id_n_bits = %u,\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->learn.mf_timeout_id_offset,
			instr->learn.mf_timeout_id_n_bits);
}

static void
instr_forget_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t},\n",
		instr_type_to_name(instr));
}

static void
instr_entryid_export(struct instruction *instr, FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.mov = {\n"
		"\t\t\t.dst = {\n"
		"\t\t\t\t.n_bits = %u,\n"
		"\t\t\t\t.offset = %u,\n"
		"\t\t\t},\n"
		"\t\t},\n"
		"\t},\n",
		instr_type_to_name(instr),
		instr->mov.dst.n_bits,
		instr->mov.dst.offset);
}

static void
instr_extern_export(struct instruction *instr, FILE *f)
{
	if (instr->type == INSTR_EXTERN_OBJ)
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.ext_obj = {\n"
			"\t\t\t.ext_obj_id = %u,\n"
			"\t\t\t.func_id = %u,\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->ext_obj.ext_obj_id,
			instr->ext_obj.func_id);
	else
		fprintf(f,
			"\t{\n"
			"\t\t.type = %s,\n"
			"\t\t.ext_func = {\n"
			"\t\t\t.ext_func_id = %u,\n"
			"\t\t},\n"
			"\t},\n",
			instr_type_to_name(instr),
			instr->ext_func.ext_func_id);
}

static void
instr_jmp_export(struct instruction *instr, FILE *f __rte_unused)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n"
		"\t\t.jmp = {\n"
		"\t\t\t.ip = NULL,\n",
		instr_type_to_name(instr));

	switch (instr->type) {
	case INSTR_JMP_VALID:
	case INSTR_JMP_INVALID:
		fprintf(f,
			"\t\t\t.header_id = %u,\n",
			instr->jmp.header_id);
		break;

	case INSTR_JMP_ACTION_HIT:
	case INSTR_JMP_ACTION_MISS:
		fprintf(f,
			"\t\t\t.action_id = %u,\n",
			instr->jmp.action_id);
		break;

	case INSTR_JMP_EQ:
	case INSTR_JMP_EQ_MH:
	case INSTR_JMP_EQ_HM:
	case INSTR_JMP_EQ_HH:
	case INSTR_JMP_NEQ:
	case INSTR_JMP_NEQ_MH:
	case INSTR_JMP_NEQ_HM:
	case INSTR_JMP_NEQ_HH:
	case INSTR_JMP_LT:
	case INSTR_JMP_LT_MH:
	case INSTR_JMP_LT_HM:
	case INSTR_JMP_LT_HH:
	case INSTR_JMP_GT:
	case INSTR_JMP_GT_MH:
	case INSTR_JMP_GT_HM:
	case INSTR_JMP_GT_HH:
		fprintf(f,
			"\t\t\t.a = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n"
			"\t\t\t.b = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t},\n",
			instr->jmp.a.struct_id,
			instr->jmp.a.n_bits,
			instr->jmp.a.offset,
			instr->jmp.b.struct_id,
			instr->jmp.b.n_bits,
			instr->jmp.b.offset);
		break;

	case INSTR_JMP_EQ_I:
	case INSTR_JMP_NEQ_I:
	case INSTR_JMP_LT_MI:
	case INSTR_JMP_LT_HI:
	case INSTR_JMP_GT_MI:
	case INSTR_JMP_GT_HI:
		fprintf(f,
			"\t\t\t.a = {\n"
			"\t\t\t\t.struct_id = %u,\n"
			"\t\t\t\t.n_bits = %u,\n"
			"\t\t\t\t.offset = %u,\n"
			"\t\t\t}\n,"
			"\t\t\t.b_val = %" PRIu64 ",\n",
			instr->jmp.a.struct_id,
			instr->jmp.a.n_bits,
			instr->jmp.a.offset,
			instr->jmp.b_val);
		break;

	default:
		break;
	}

	fprintf(f,
		"\t\t},\n"
		"\t},\n");
}

static void
instr_return_export(struct instruction *instr,
		FILE *f)
{
	fprintf(f,
		"\t{\n"
		"\t\t.type = %s,\n",
		instr_type_to_name(instr));

	fprintf(f,
		"\t},\n");
}

static instruction_export_t export_table[] = {
	[INSTR_RX] = instr_io_export,

	[INSTR_TX] = instr_io_export,
	[INSTR_TX_I] = instr_io_export,
	[INSTR_DROP] = instr_io_export,
	[INSTR_MIRROR] = instr_mirror_export,
	[INSTR_RECIRCULATE] = instr_recirculate_export,
	[INSTR_RECIRCID] = instr_recircid_export,

	[INSTR_HDR_EXTRACT] = instr_io_export,
	[INSTR_HDR_EXTRACT2] = instr_io_export,
	[INSTR_HDR_EXTRACT3] = instr_io_export,
	[INSTR_HDR_EXTRACT4] = instr_io_export,
	[INSTR_HDR_EXTRACT5] = instr_io_export,
	[INSTR_HDR_EXTRACT6] = instr_io_export,
	[INSTR_HDR_EXTRACT7] = instr_io_export,
	[INSTR_HDR_EXTRACT8] = instr_io_export,

	[INSTR_HDR_EXTRACT_M] = instr_io_export,

	[INSTR_HDR_LOOKAHEAD] = instr_io_export,

	[INSTR_HDR_EMIT] = instr_io_export,
	[INSTR_HDR_EMIT_TX] = instr_io_export,
	[INSTR_HDR_EMIT2_TX] = instr_io_export,
	[INSTR_HDR_EMIT3_TX] = instr_io_export,
	[INSTR_HDR_EMIT4_TX] = instr_io_export,
	[INSTR_HDR_EMIT5_TX] = instr_io_export,
	[INSTR_HDR_EMIT6_TX] = instr_io_export,
	[INSTR_HDR_EMIT7_TX] = instr_io_export,
	[INSTR_HDR_EMIT8_TX] = instr_io_export,

	[INSTR_HDR_VALIDATE] = instr_hdr_validate_export,
	[INSTR_HDR_INVALIDATE] = instr_hdr_validate_export,

	[INSTR_MOV] = instr_mov_export,
	[INSTR_MOV_MH] = instr_mov_export,
	[INSTR_MOV_HM] = instr_mov_export,
	[INSTR_MOV_HH] = instr_mov_export,
	[INSTR_MOV_DMA] = instr_mov_export,
	[INSTR_MOV_128] = instr_mov_export,
	[INSTR_MOV_128_32] = instr_mov_export,
	[INSTR_MOV_I] = instr_mov_export,

	[INSTR_DMA_HT]  = instr_dma_ht_export,
	[INSTR_DMA_HT2] = instr_dma_ht_export,
	[INSTR_DMA_HT3] = instr_dma_ht_export,
	[INSTR_DMA_HT4] = instr_dma_ht_export,
	[INSTR_DMA_HT5] = instr_dma_ht_export,
	[INSTR_DMA_HT6] = instr_dma_ht_export,
	[INSTR_DMA_HT7] = instr_dma_ht_export,
	[INSTR_DMA_HT8] = instr_dma_ht_export,

	[INSTR_ALU_ADD] = instr_alu_export,
	[INSTR_ALU_ADD_MH] = instr_alu_export,
	[INSTR_ALU_ADD_HM] = instr_alu_export,
	[INSTR_ALU_ADD_HH] = instr_alu_export,
	[INSTR_ALU_ADD_MI] = instr_alu_export,
	[INSTR_ALU_ADD_HI] = instr_alu_export,

	[INSTR_ALU_SUB] = instr_alu_export,
	[INSTR_ALU_SUB_MH] = instr_alu_export,
	[INSTR_ALU_SUB_HM] = instr_alu_export,
	[INSTR_ALU_SUB_HH] = instr_alu_export,
	[INSTR_ALU_SUB_MI] = instr_alu_export,
	[INSTR_ALU_SUB_HI] = instr_alu_export,

	[INSTR_ALU_CKADD_FIELD] = instr_alu_export,
	[INSTR_ALU_CKADD_STRUCT] = instr_alu_export,
	[INSTR_ALU_CKADD_STRUCT20] = instr_alu_export,
	[INSTR_ALU_CKSUB_FIELD] = instr_alu_export,

	[INSTR_ALU_AND] = instr_alu_export,
	[INSTR_ALU_AND_MH] = instr_alu_export,
	[INSTR_ALU_AND_HM] = instr_alu_export,
	[INSTR_ALU_AND_HH] = instr_alu_export,
	[INSTR_ALU_AND_I] = instr_alu_export,

	[INSTR_ALU_OR] = instr_alu_export,
	[INSTR_ALU_OR_MH] = instr_alu_export,
	[INSTR_ALU_OR_HM] = instr_alu_export,
	[INSTR_ALU_OR_HH] = instr_alu_export,
	[INSTR_ALU_OR_I] = instr_alu_export,

	[INSTR_ALU_XOR] = instr_alu_export,
	[INSTR_ALU_XOR_MH] = instr_alu_export,
	[INSTR_ALU_XOR_HM] = instr_alu_export,
	[INSTR_ALU_XOR_HH] = instr_alu_export,
	[INSTR_ALU_XOR_I] = instr_alu_export,

	[INSTR_ALU_SHL] = instr_alu_export,
	[INSTR_ALU_SHL_MH] = instr_alu_export,
	[INSTR_ALU_SHL_HM] = instr_alu_export,
	[INSTR_ALU_SHL_HH] = instr_alu_export,
	[INSTR_ALU_SHL_MI] = instr_alu_export,
	[INSTR_ALU_SHL_HI] = instr_alu_export,

	[INSTR_ALU_SHR] = instr_alu_export,
	[INSTR_ALU_SHR_MH] = instr_alu_export,
	[INSTR_ALU_SHR_HM] = instr_alu_export,
	[INSTR_ALU_SHR_HH] = instr_alu_export,
	[INSTR_ALU_SHR_MI] = instr_alu_export,
	[INSTR_ALU_SHR_HI] = instr_alu_export,

	[INSTR_REGPREFETCH_RH] = instr_reg_export,
	[INSTR_REGPREFETCH_RM] = instr_reg_export,
	[INSTR_REGPREFETCH_RI] = instr_reg_export,

	[INSTR_REGRD_HRH] = instr_reg_export,
	[INSTR_REGRD_HRM] = instr_reg_export,
	[INSTR_REGRD_MRH] = instr_reg_export,
	[INSTR_REGRD_MRM] = instr_reg_export,
	[INSTR_REGRD_HRI] = instr_reg_export,
	[INSTR_REGRD_MRI] = instr_reg_export,

	[INSTR_REGWR_RHH] = instr_reg_export,
	[INSTR_REGWR_RHM] = instr_reg_export,
	[INSTR_REGWR_RMH] = instr_reg_export,
	[INSTR_REGWR_RMM] = instr_reg_export,
	[INSTR_REGWR_RHI] = instr_reg_export,
	[INSTR_REGWR_RMI] = instr_reg_export,
	[INSTR_REGWR_RIH] = instr_reg_export,
	[INSTR_REGWR_RIM] = instr_reg_export,
	[INSTR_REGWR_RII] = instr_reg_export,

	[INSTR_REGADD_RHH] = instr_reg_export,
	[INSTR_REGADD_RHM] = instr_reg_export,
	[INSTR_REGADD_RMH] = instr_reg_export,
	[INSTR_REGADD_RMM] = instr_reg_export,
	[INSTR_REGADD_RHI] = instr_reg_export,
	[INSTR_REGADD_RMI] = instr_reg_export,
	[INSTR_REGADD_RIH] = instr_reg_export,
	[INSTR_REGADD_RIM] = instr_reg_export,
	[INSTR_REGADD_RII] = instr_reg_export,

	[INSTR_METPREFETCH_H] = instr_meter_export,
	[INSTR_METPREFETCH_M] = instr_meter_export,
	[INSTR_METPREFETCH_I] = instr_meter_export,

	[INSTR_METER_HHM] = instr_meter_export,
	[INSTR_METER_HHI] = instr_meter_export,
	[INSTR_METER_HMM] = instr_meter_export,
	[INSTR_METER_HMI] = instr_meter_export,
	[INSTR_METER_MHM] = instr_meter_export,
	[INSTR_METER_MHI] = instr_meter_export,
	[INSTR_METER_MMM] = instr_meter_export,
	[INSTR_METER_MMI] = instr_meter_export,
	[INSTR_METER_IHM] = instr_meter_export,
	[INSTR_METER_IHI] = instr_meter_export,
	[INSTR_METER_IMM] = instr_meter_export,
	[INSTR_METER_IMI] = instr_meter_export,

	[INSTR_TABLE] = instr_table_export,
	[INSTR_TABLE_AF] = instr_table_export,
	[INSTR_SELECTOR] = instr_table_export,
	[INSTR_LEARNER] = instr_table_export,
	[INSTR_LEARNER_AF] = instr_table_export,

	[INSTR_LEARNER_LEARN] = instr_learn_export,
	[INSTR_LEARNER_REARM] = instr_rearm_export,
	[INSTR_LEARNER_REARM_NEW] = instr_rearm_export,
	[INSTR_LEARNER_FORGET] = instr_forget_export,
	[INSTR_ENTRYID] = instr_entryid_export,

	[INSTR_EXTERN_OBJ] = instr_extern_export,
	[INSTR_EXTERN_FUNC] = instr_extern_export,
	[INSTR_HASH_FUNC] = instr_hash_export,
	[INSTR_RSS] = instr_rss_export,

	[INSTR_JMP] = instr_jmp_export,
	[INSTR_JMP_VALID] = instr_jmp_export,
	[INSTR_JMP_INVALID] = instr_jmp_export,
	[INSTR_JMP_HIT] = instr_jmp_export,
	[INSTR_JMP_MISS] = instr_jmp_export,
	[INSTR_JMP_ACTION_HIT] = instr_jmp_export,
	[INSTR_JMP_ACTION_MISS] = instr_jmp_export,

	[INSTR_JMP_EQ] = instr_jmp_export,
	[INSTR_JMP_EQ_MH] = instr_jmp_export,
	[INSTR_JMP_EQ_HM] = instr_jmp_export,
	[INSTR_JMP_EQ_HH] = instr_jmp_export,
	[INSTR_JMP_EQ_I] = instr_jmp_export,

	[INSTR_JMP_NEQ] = instr_jmp_export,
	[INSTR_JMP_NEQ_MH] = instr_jmp_export,
	[INSTR_JMP_NEQ_HM] = instr_jmp_export,
	[INSTR_JMP_NEQ_HH] = instr_jmp_export,
	[INSTR_JMP_NEQ_I] = instr_jmp_export,

	[INSTR_JMP_LT] = instr_jmp_export,
	[INSTR_JMP_LT_MH] = instr_jmp_export,
	[INSTR_JMP_LT_HM] = instr_jmp_export,
	[INSTR_JMP_LT_HH] = instr_jmp_export,
	[INSTR_JMP_LT_MI] = instr_jmp_export,
	[INSTR_JMP_LT_HI] = instr_jmp_export,

	[INSTR_JMP_GT] = instr_jmp_export,
	[INSTR_JMP_GT_MH] = instr_jmp_export,
	[INSTR_JMP_GT_HM] = instr_jmp_export,
	[INSTR_JMP_GT_HH] = instr_jmp_export,
	[INSTR_JMP_GT_MI] = instr_jmp_export,
	[INSTR_JMP_GT_HI] = instr_jmp_export,

	[INSTR_RETURN] = instr_return_export,
};

static void
action_data_codegen(struct action *a, FILE *f)
{
	uint32_t i;

	fprintf(f,
		"static const struct instruction action_%s_instructions[] = {\n",
		a->name);

	for (i = 0; i < a->n_instructions; i++) {
		struct instruction *instr = &a->instructions[i];
		instruction_export_t func = export_table[instr->type];

		func(instr, f);
	}

	fprintf(f, "};\n");
}

static const char *
instr_type_to_func(struct instruction *instr)
{
	switch (instr->type) {
	case INSTR_RX: return NULL;

	case INSTR_TX: return "__instr_tx_exec";
	case INSTR_TX_I: return "__instr_tx_i_exec";
	case INSTR_DROP: return "__instr_drop_exec";
	case INSTR_MIRROR: return "__instr_mirror_exec";
	case INSTR_RECIRCULATE: return "__instr_recirculate_exec";
	case INSTR_RECIRCID: return "__instr_recircid_exec";

	case INSTR_HDR_EXTRACT: return "__instr_hdr_extract_exec";
	case INSTR_HDR_EXTRACT2: return "__instr_hdr_extract2_exec";
	case INSTR_HDR_EXTRACT3: return "__instr_hdr_extract3_exec";
	case INSTR_HDR_EXTRACT4: return "__instr_hdr_extract4_exec";
	case INSTR_HDR_EXTRACT5: return "__instr_hdr_extract5_exec";
	case INSTR_HDR_EXTRACT6: return "__instr_hdr_extract6_exec";
	case INSTR_HDR_EXTRACT7: return "__instr_hdr_extract7_exec";
	case INSTR_HDR_EXTRACT8: return "__instr_hdr_extract8_exec";

	case INSTR_HDR_EXTRACT_M: return "__instr_hdr_extract_m_exec";

	case INSTR_HDR_LOOKAHEAD: return "__instr_hdr_lookahead_exec";

	case INSTR_HDR_EMIT: return "__instr_hdr_emit_exec";
	case INSTR_HDR_EMIT_TX: return "__instr_hdr_emit_tx_exec";
	case INSTR_HDR_EMIT2_TX: return "__instr_hdr_emit2_tx_exec";
	case INSTR_HDR_EMIT3_TX: return "__instr_hdr_emit3_tx_exec";
	case INSTR_HDR_EMIT4_TX: return "__instr_hdr_emit4_tx_exec";
	case INSTR_HDR_EMIT5_TX: return "__instr_hdr_emit5_tx_exec";
	case INSTR_HDR_EMIT6_TX: return "__instr_hdr_emit6_tx_exec";
	case INSTR_HDR_EMIT7_TX: return "__instr_hdr_emit7_tx_exec";
	case INSTR_HDR_EMIT8_TX: return "__instr_hdr_emit8_tx_exec";

	case INSTR_HDR_VALIDATE: return "__instr_hdr_validate_exec";
	case INSTR_HDR_INVALIDATE: return "__instr_hdr_invalidate_exec";

	case INSTR_MOV: return "__instr_mov_exec";
	case INSTR_MOV_MH: return "__instr_mov_mh_exec";
	case INSTR_MOV_HM: return "__instr_mov_hm_exec";
	case INSTR_MOV_HH: return "__instr_mov_hh_exec";
	case INSTR_MOV_DMA: return "__instr_mov_dma_exec";
	case INSTR_MOV_128: return "__instr_mov_128_exec";
	case INSTR_MOV_128_32: return "__instr_mov_128_32_exec";
	case INSTR_MOV_I: return "__instr_mov_i_exec";

	case INSTR_DMA_HT: return "__instr_dma_ht_exec";
	case INSTR_DMA_HT2: return "__instr_dma_ht2_exec";
	case INSTR_DMA_HT3: return "__instr_dma_ht3_exec";
	case INSTR_DMA_HT4: return "__instr_dma_ht4_exec";
	case INSTR_DMA_HT5: return "__instr_dma_ht5_exec";
	case INSTR_DMA_HT6: return "__instr_dma_ht6_exec";
	case INSTR_DMA_HT7: return "__instr_dma_ht7_exec";
	case INSTR_DMA_HT8: return "__instr_dma_ht8_exec";

	case INSTR_ALU_ADD: return "__instr_alu_add_exec";
	case INSTR_ALU_ADD_MH: return "__instr_alu_add_mh_exec";
	case INSTR_ALU_ADD_HM: return "__instr_alu_add_hm_exec";
	case INSTR_ALU_ADD_HH: return "__instr_alu_add_hh_exec";
	case INSTR_ALU_ADD_MI: return "__instr_alu_add_mi_exec";
	case INSTR_ALU_ADD_HI: return "__instr_alu_add_hi_exec";

	case INSTR_ALU_SUB: return "__instr_alu_sub_exec";
	case INSTR_ALU_SUB_MH: return "__instr_alu_sub_mh_exec";
	case INSTR_ALU_SUB_HM: return "__instr_alu_sub_hm_exec";
	case INSTR_ALU_SUB_HH: return "__instr_alu_sub_hh_exec";
	case INSTR_ALU_SUB_MI: return "__instr_alu_sub_mi_exec";
	case INSTR_ALU_SUB_HI: return "__instr_alu_sub_hi_exec";

	case INSTR_ALU_CKADD_FIELD: return "__instr_alu_ckadd_field_exec";
	case INSTR_ALU_CKADD_STRUCT20: return "__instr_alu_ckadd_struct20_exec";
	case INSTR_ALU_CKADD_STRUCT: return "__instr_alu_ckadd_struct_exec";
	case INSTR_ALU_CKSUB_FIELD: return "__instr_alu_cksub_field_exec";

	case INSTR_ALU_AND: return "__instr_alu_and_exec";
	case INSTR_ALU_AND_MH: return "__instr_alu_and_mh_exec";
	case INSTR_ALU_AND_HM: return "__instr_alu_and_hm_exec";
	case INSTR_ALU_AND_HH: return "__instr_alu_and_hh_exec";
	case INSTR_ALU_AND_I: return "__instr_alu_and_i_exec";

	case INSTR_ALU_OR: return "__instr_alu_or_exec";
	case INSTR_ALU_OR_MH: return "__instr_alu_or_mh_exec";
	case INSTR_ALU_OR_HM: return "__instr_alu_or_hm_exec";
	case INSTR_ALU_OR_HH: return "__instr_alu_or_hh_exec";
	case INSTR_ALU_OR_I: return "__instr_alu_or_i_exec";

	case INSTR_ALU_XOR: return "__instr_alu_xor_exec";
	case INSTR_ALU_XOR_MH: return "__instr_alu_xor_mh_exec";
	case INSTR_ALU_XOR_HM: return "__instr_alu_xor_hm_exec";
	case INSTR_ALU_XOR_HH: return "__instr_alu_xor_hh_exec";
	case INSTR_ALU_XOR_I: return "__instr_alu_xor_i_exec";

	case INSTR_ALU_SHL: return "__instr_alu_shl_exec";
	case INSTR_ALU_SHL_MH: return "__instr_alu_shl_mh_exec";
	case INSTR_ALU_SHL_HM: return "__instr_alu_shl_hm_exec";
	case INSTR_ALU_SHL_HH: return "__instr_alu_shl_hh_exec";
	case INSTR_ALU_SHL_MI: return "__instr_alu_shl_mi_exec";
	case INSTR_ALU_SHL_HI: return "__instr_alu_shl_hi_exec";

	case INSTR_ALU_SHR: return "__instr_alu_shr_exec";
	case INSTR_ALU_SHR_MH: return "__instr_alu_shr_mh_exec";
	case INSTR_ALU_SHR_HM: return "__instr_alu_shr_hm_exec";
	case INSTR_ALU_SHR_HH: return "__instr_alu_shr_hh_exec";
	case INSTR_ALU_SHR_MI: return "__instr_alu_shr_mi_exec";
	case INSTR_ALU_SHR_HI: return "__instr_alu_shr_hi_exec";

	case INSTR_REGPREFETCH_RH: return "__instr_regprefetch_rh_exec";
	case INSTR_REGPREFETCH_RM: return "__instr_regprefetch_rm_exec";
	case INSTR_REGPREFETCH_RI: return "__instr_regprefetch_ri_exec";

	case INSTR_REGRD_HRH: return "__instr_regrd_hrh_exec";
	case INSTR_REGRD_HRM: return "__instr_regrd_hrm_exec";
	case INSTR_REGRD_HRI: return "__instr_regrd_hri_exec";
	case INSTR_REGRD_MRH: return "__instr_regrd_mrh_exec";
	case INSTR_REGRD_MRM: return "__instr_regrd_mrm_exec";
	case INSTR_REGRD_MRI: return "__instr_regrd_mri_exec";

	case INSTR_REGWR_RHH: return "__instr_regwr_rhh_exec";
	case INSTR_REGWR_RHM: return "__instr_regwr_rhm_exec";
	case INSTR_REGWR_RHI: return "__instr_regwr_rhi_exec";
	case INSTR_REGWR_RMH: return "__instr_regwr_rmh_exec";
	case INSTR_REGWR_RMM: return "__instr_regwr_rmm_exec";
	case INSTR_REGWR_RMI: return "__instr_regwr_rmi_exec";
	case INSTR_REGWR_RIH: return "__instr_regwr_rih_exec";
	case INSTR_REGWR_RIM: return "__instr_regwr_rim_exec";
	case INSTR_REGWR_RII: return "__instr_regwr_rii_exec";

	case INSTR_REGADD_RHH: return "__instr_regadd_rhh_exec";
	case INSTR_REGADD_RHM: return "__instr_regadd_rhm_exec";
	case INSTR_REGADD_RHI: return "__instr_regadd_rhi_exec";
	case INSTR_REGADD_RMH: return "__instr_regadd_rmh_exec";
	case INSTR_REGADD_RMM: return "__instr_regadd_rmm_exec";
	case INSTR_REGADD_RMI: return "__instr_regadd_rmi_exec";
	case INSTR_REGADD_RIH: return "__instr_regadd_rih_exec";
	case INSTR_REGADD_RIM: return "__instr_regadd_rim_exec";
	case INSTR_REGADD_RII: return "__instr_regadd_rii_exec";

	case INSTR_METPREFETCH_H: return "__instr_metprefetch_h_exec";
	case INSTR_METPREFETCH_M: return "__instr_metprefetch_m_exec";
	case INSTR_METPREFETCH_I: return "__instr_metprefetch_i_exec";

	case INSTR_METER_HHM: return "__instr_meter_hhm_exec";
	case INSTR_METER_HHI: return "__instr_meter_hhi_exec";
	case INSTR_METER_HMM: return "__instr_meter_hmm_exec";
	case INSTR_METER_HMI: return "__instr_meter_hmi_exec";
	case INSTR_METER_MHM: return "__instr_meter_mhm_exec";
	case INSTR_METER_MHI: return "__instr_meter_mhi_exec";
	case INSTR_METER_MMM: return "__instr_meter_mmm_exec";
	case INSTR_METER_MMI: return "__instr_meter_mmi_exec";
	case INSTR_METER_IHM: return "__instr_meter_ihm_exec";
	case INSTR_METER_IHI: return "__instr_meter_ihi_exec";
	case INSTR_METER_IMM: return "__instr_meter_imm_exec";
	case INSTR_METER_IMI: return "__instr_meter_imi_exec";

	case INSTR_TABLE: return NULL;
	case INSTR_TABLE_AF: return NULL;
	case INSTR_SELECTOR: return NULL;
	case INSTR_LEARNER: return NULL;
	case INSTR_LEARNER_AF: return NULL;

	case INSTR_LEARNER_LEARN: return "__instr_learn_exec";
	case INSTR_LEARNER_REARM: return "__instr_rearm_exec";
	case INSTR_LEARNER_REARM_NEW: return "__instr_rearm_new_exec";
	case INSTR_LEARNER_FORGET: return "__instr_forget_exec";
	case INSTR_ENTRYID: return "__instr_entryid_exec";

	case INSTR_EXTERN_OBJ: return NULL;
	case INSTR_EXTERN_FUNC: return NULL;
	case INSTR_HASH_FUNC: return "__instr_hash_func_exec";
	case INSTR_RSS: return "__instr_rss_exec";

	case INSTR_JMP: return NULL;
	case INSTR_JMP_VALID: return NULL;
	case INSTR_JMP_INVALID: return NULL;
	case INSTR_JMP_HIT: return NULL;
	case INSTR_JMP_MISS: return NULL;
	case INSTR_JMP_ACTION_HIT: return NULL;
	case INSTR_JMP_ACTION_MISS: return NULL;
	case INSTR_JMP_EQ: return NULL;
	case INSTR_JMP_EQ_MH: return NULL;
	case INSTR_JMP_EQ_HM: return NULL;
	case INSTR_JMP_EQ_HH: return NULL;
	case INSTR_JMP_EQ_I: return NULL;
	case INSTR_JMP_NEQ: return NULL;
	case INSTR_JMP_NEQ_MH: return NULL;
	case INSTR_JMP_NEQ_HM: return NULL;
	case INSTR_JMP_NEQ_HH: return NULL;
	case INSTR_JMP_NEQ_I: return NULL;
	case INSTR_JMP_LT: return NULL;
	case INSTR_JMP_LT_MH: return NULL;
	case INSTR_JMP_LT_HM: return NULL;
	case INSTR_JMP_LT_HH: return NULL;
	case INSTR_JMP_LT_MI: return NULL;
	case INSTR_JMP_LT_HI: return NULL;
	case INSTR_JMP_GT: return NULL;
	case INSTR_JMP_GT_MH: return NULL;
	case INSTR_JMP_GT_HM: return NULL;
	case INSTR_JMP_GT_HH: return NULL;
	case INSTR_JMP_GT_MI: return NULL;
	case INSTR_JMP_GT_HI: return NULL;

	case INSTR_RETURN: return NULL;

	default: return NULL;
	}
}

static void
action_instr_does_tx_codegen(struct action *a,
			uint32_t instr_pos,
			struct instruction *instr,
			FILE *f)
{
	fprintf(f,
		"%s(p, t, &action_%s_instructions[%u]);\n"
		"\tthread_ip_reset(p, t);\n"
		"\tinstr_rx_exec(p);\n"
		"\treturn;\n",
		instr_type_to_func(instr),
		a->name,
		instr_pos);
}

static void
action_instr_extern_obj_codegen(struct action *a,
				uint32_t instr_pos,
				FILE *f)
{
	fprintf(f,
		"while (!__instr_extern_obj_exec(p, t, &action_%s_instructions[%u]));\n",
		a->name,
		instr_pos);
}

static void
action_instr_extern_func_codegen(struct action *a,
				 uint32_t instr_pos,
				 FILE *f)
{
	fprintf(f,
		"while (!__instr_extern_func_exec(p, t, &action_%s_instructions[%u]));\n",
		a->name,
		instr_pos);
}

static void
action_instr_jmp_codegen(struct action *a,
			 uint32_t instr_pos,
			 struct instruction *instr,
			 struct instruction_data *data,
			 FILE *f)
{
	switch (instr->type) {
	case INSTR_JMP:
		fprintf(f,
			"goto %s;\n",
			data->jmp_label);
		return;

	case INSTR_JMP_VALID:
		fprintf(f,
			"if (HEADER_VALID(t, action_%s_instructions[%u].jmp.header_id))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_INVALID:
		fprintf(f,
			"if (!HEADER_VALID(t, action_%s_instructions[%u].jmp.header_id))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_HIT:
		fprintf(f,
			"if (t->hit)\n"
			"\t\tgoto %s;\n",
			data->jmp_label);
		return;

	case INSTR_JMP_MISS:
		fprintf(f,
			"if (!t->hit)\n"
			"\t\tgoto %s;\n",
			data->jmp_label);
		return;

	case INSTR_JMP_ACTION_HIT:
		fprintf(f,
			"if (t->action_id == action_%s_instructions[%u].jmp.action_id)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_ACTION_MISS:
		fprintf(f,
			"if (t->action_id != action_%s_instructions[%u].jmp.action_id)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_EQ:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) == "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_EQ_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) == "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_EQ_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) == "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_EQ_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) == "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_EQ_I:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) == "
			"action_%s_instructions[%u].jmp.b_val)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_NEQ:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) != "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_NEQ_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) != "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_NEQ_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) != "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_NEQ_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) != "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_NEQ_I:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) != "
			"action_%s_instructions[%u].jmp.b_val)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_LT:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) < "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_LT_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) < "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_LT_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) < "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_LT_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) < "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_LT_MI:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) < "
			"action_%s_instructions[%u].jmp.b_val)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_LT_HI:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) < "
			"action_%s_instructions[%u].jmp.b_val)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_GT:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) > "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_GT_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) > "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_GT_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) > "
			"instr_operand_hbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_GT_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) > "
			"instr_operand_nbo(t, &action_%s_instructions[%u].jmp.b))\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_GT_MI:
		fprintf(f,
			"if (instr_operand_hbo(t, &action_%s_instructions[%u].jmp.a) > "
			"action_%s_instructions[%u].jmp.b_val)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	case INSTR_JMP_GT_HI:
		fprintf(f,
			"if (instr_operand_nbo(t, &action_%s_instructions[%u].jmp.a) > "
			"action_%s_instructions[%u].jmp.b_val)\n"
			"\t\tgoto %s;\n",
			a->name,
			instr_pos,
			a->name,
			instr_pos,
			data->jmp_label);
		return;

	default:
		return;
	}
}

static void
action_instr_return_codegen(FILE *f)
{
	fprintf(f,
		"return;\n");
}

static void
action_instr_codegen(struct action *a, FILE *f)
{
	uint32_t i;

	fprintf(f,
		"void\n"
		"action_%s_run(struct rte_swx_pipeline *p)\n"
		"{\n"
		"\tstruct thread *t = &p->threads[p->thread_id];\n"
		"\n",
		a->name);

	for (i = 0; i < a->n_instructions; i++) {
		struct instruction *instr = &a->instructions[i];
		struct instruction_data *data = &a->instruction_data[i];

		/* Label, if present. */
		if (data->label[0])
			fprintf(f, "\n%s : ", data->label);
		else
			fprintf(f, "\n\t");

		/* TX instruction type. */
		if (instruction_does_tx(instr)) {
			action_instr_does_tx_codegen(a, i, instr, f);
			continue;
		}

		/* Extern object/function instruction type. */
		if (instr->type == INSTR_EXTERN_OBJ) {
			action_instr_extern_obj_codegen(a, i, f);
			continue;
		}

		if (instr->type == INSTR_EXTERN_FUNC) {
			action_instr_extern_func_codegen(a, i, f);
			continue;
		}

		/* Jump instruction type. */
		if (instruction_is_jmp(instr)) {
			action_instr_jmp_codegen(a, i, instr, data, f);
			continue;
		}

		/* Return instruction type. */
		if (instr->type == INSTR_RETURN) {
			action_instr_return_codegen(f);
			continue;
		}

		/* Any other instruction type. */
		fprintf(f,
			"%s(p, t, &action_%s_instructions[%u]);\n",
			instr_type_to_func(instr),
			a->name,
			i);
	}

	fprintf(f, "}\n\n");
}

struct instruction_group {
	TAILQ_ENTRY(instruction_group) node;

	uint32_t group_id;

	uint32_t first_instr_id;

	uint32_t last_instr_id;

	instr_exec_t func;
};

TAILQ_HEAD(instruction_group_list, instruction_group);

static struct instruction_group *
instruction_group_list_group_find(struct instruction_group_list *igl, uint32_t instruction_id)
{
	struct instruction_group *g;

	TAILQ_FOREACH(g, igl, node)
		if ((g->first_instr_id <= instruction_id) && (instruction_id <= g->last_instr_id))
			return g;

	return NULL;
}

static void
instruction_group_list_free(struct instruction_group_list *igl)
{
	if (!igl)
		return;

	for ( ; ; ) {
		struct instruction_group *g;

		g = TAILQ_FIRST(igl);
		if (!g)
			break;

		TAILQ_REMOVE(igl, g, node);
		free(g);
	}

	free(igl);
}

static struct instruction_group_list *
instruction_group_list_create(struct rte_swx_pipeline *p)
{
	struct instruction_group_list *igl = NULL;
	struct instruction_group *g = NULL;
	uint32_t n_groups = 0, i;

	if (!p || !p->instructions || !p->instruction_data || !p->n_instructions)
		goto error;

	/* List init. */
	igl = calloc(1, sizeof(struct instruction_group_list));
	if (!igl)
		goto error;

	TAILQ_INIT(igl);

	/* Allocate the first group. */
	g = calloc(1, sizeof(struct instruction_group));
	if (!g)
		goto error;

	/* Iteration 1: Separate the instructions into groups based on the thread yield
	 * instructions. Do not worry about the jump instructions at this point.
	 */
	for (i = 0; i < p->n_instructions; i++) {
		struct instruction *instr = &p->instructions[i];

		/* Check for thread yield instructions. */
		if (!instruction_does_thread_yield(instr))
			continue;

		/* If the current group contains at least one instruction, then finalize it (with
		 * the previous instruction), add it to the list and allocate a new group (that
		 * starts with the current instruction).
		 */
		if (i - g->first_instr_id) {
			/* Finalize the group. */
			g->last_instr_id = i - 1;

			/* Add the group to the list. Advance the number of groups. */
			TAILQ_INSERT_TAIL(igl, g, node);
			n_groups++;

			/* Allocate a new group. */
			g = calloc(1, sizeof(struct instruction_group));
			if (!g)
				goto error;

			/* Initialize the new group. */
			g->group_id = n_groups;
			g->first_instr_id = i;
		}

		/* Finalize the current group (with the current instruction, therefore this group
		 * contains just the current thread yield instruction), add it to the list and
		 * allocate a new group (that starts with the next instruction).
		 */

		/* Finalize the group. */
		g->last_instr_id = i;

		/* Add the group to the list. Advance the number of groups. */
		TAILQ_INSERT_TAIL(igl, g, node);
		n_groups++;

		/* Allocate a new group. */
		g = calloc(1, sizeof(struct instruction_group));
		if (!g)
			goto error;

		/* Initialize the new group. */
		g->group_id = n_groups;
		g->first_instr_id = i + 1;
	}

	/* Handle the last group. */
	if (i - g->first_instr_id) {
		/* Finalize the group. */
		g->last_instr_id = i - 1;

		/* Add the group to the list. Advance the number of groups. */
		TAILQ_INSERT_TAIL(igl, g, node);
		n_groups++;
	} else
		free(g);

	g = NULL;

	/* Iteration 2: Handle jumps. If the current group contains an instruction which represents
	 * the destination of a jump instruction located in a different group ("far jump"), then the
	 * current group has to be split, so that the instruction representing the far jump
	 * destination is at the start of its group.
	 */
	for ( ; ; ) {
		int is_modified = 0;

		for (i = 0; i < p->n_instructions; i++) {
			struct instruction_data *data = &p->instruction_data[i];
			struct instruction_group *g;
			uint32_t j;

			/* Continue when the current instruction is not a jump destination. */
			if (!data->n_users)
				continue;

			g = instruction_group_list_group_find(igl, i);
			if (!g)
				goto error;

			/* Find out all the jump instructions with this destination. */
			for (j = 0; j < p->n_instructions; j++) {
				struct instruction *jmp_instr = &p->instructions[j];
				struct instruction_data *jmp_data = &p->instruction_data[j];
				struct instruction_group *jmp_g, *new_g;

				/* Continue when not a jump instruction. Even when jump instruction,
				 * continue when the jump destination is not this instruction.
				 */
				if (!instruction_is_jmp(jmp_instr) ||
				    strcmp(jmp_data->jmp_label, data->label))
					continue;

				jmp_g = instruction_group_list_group_find(igl, j);
				if (!jmp_g)
					goto error;

				/* Continue when both the jump instruction and the jump destination
				 * instruction are in the same group. Even when in different groups,
				 * still continue if the jump destination instruction is already the
				 * first instruction of its group.
				 */
				if ((jmp_g->group_id == g->group_id) || (g->first_instr_id == i))
					continue;

				/* Split the group of the current jump destination instruction to
				 * make this instruction the first instruction of a new group.
				 */
				new_g = calloc(1, sizeof(struct instruction_group));
				if (!new_g)
					goto error;

				new_g->group_id = n_groups;
				new_g->first_instr_id = i;
				new_g->last_instr_id = g->last_instr_id;

				g->last_instr_id = i - 1;

				TAILQ_INSERT_AFTER(igl, g, new_g, node);
				n_groups++;
				is_modified = 1;

				/* The decision to split this group (to make the current instruction
				 * the first instruction of a new group) is already taken and fully
				 * implemented, so no need to search for more reasons to do it.
				 */
				break;
			}
		}

		/* Re-evaluate everything, as at least one group got split, so some jumps that were
		 * previously considered local (i.e. the jump destination is in the same group as
		 * the jump instruction) can now be "far jumps" (i.e. the jump destination is in a
		 * different group than the jump instruction). Wost case scenario: each instruction
		 * that is a jump destination ends up as the first instruction of its group.
		 */
		if (!is_modified)
			break;
	}

	/* Re-assign the group IDs to be in incremental order. */
	i = 0;
	TAILQ_FOREACH(g, igl, node) {
		g->group_id = i;

		i++;
	}

	return igl;

error:
	instruction_group_list_free(igl);

	free(g);

	return NULL;
}

static void
pipeline_instr_does_tx_codegen(struct rte_swx_pipeline *p __rte_unused,
			       uint32_t instr_pos,
			       struct instruction *instr,
			       FILE *f)
{
	fprintf(f,
		"%s(p, t, &pipeline_instructions[%u]);\n"
		"\tthread_ip_reset(p, t);\n"
		"\tinstr_rx_exec(p);\n"
		"\treturn;\n",
		instr_type_to_func(instr),
		instr_pos);
}

static int
pipeline_instr_jmp_codegen(struct rte_swx_pipeline *p,
			   struct instruction_group_list *igl,
			   uint32_t jmp_instr_id,
			   struct instruction *jmp_instr,
			   struct instruction_data *jmp_data,
			   FILE *f)
{
	struct instruction_group *jmp_g, *g;
	struct instruction_data *data;
	uint32_t instr_id;

	switch (jmp_instr->type) {
	case INSTR_JMP:
		break;

	case INSTR_JMP_VALID:
		fprintf(f,
			"if (HEADER_VALID(t, pipeline_instructions[%u].jmp.header_id))",
			jmp_instr_id);
		break;

	case INSTR_JMP_INVALID:
		fprintf(f,
			"if (!HEADER_VALID(t, pipeline_instructions[%u].jmp.header_id))",
			jmp_instr_id);
		break;

	case INSTR_JMP_HIT:
		fprintf(f,
			"if (t->hit)\n");
		break;

	case INSTR_JMP_MISS:
		fprintf(f,
			"if (!t->hit)\n");
		break;

	case INSTR_JMP_ACTION_HIT:
		fprintf(f,
			"if (t->action_id == pipeline_instructions[%u].jmp.action_id)",
			jmp_instr_id);
		break;

	case INSTR_JMP_ACTION_MISS:
		fprintf(f,
			"if (t->action_id != pipeline_instructions[%u].jmp.action_id)",
			jmp_instr_id);
		break;

	case INSTR_JMP_EQ:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) == "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_EQ_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) == "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_EQ_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) == "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_EQ_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) == "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_EQ_I:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) == "
			"pipeline_instructions[%u].jmp.b_val)",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_NEQ:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) != "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_NEQ_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) != "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_NEQ_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) != "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_NEQ_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) != "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_NEQ_I:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) != "
			"pipeline_instructions[%u].jmp.b_val)",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_LT:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) < "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_LT_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) < "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_LT_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) < "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_LT_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) < "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_LT_MI:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) < "
			"pipeline_instructions[%u].jmp.b_val)",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_LT_HI:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) < "
			"pipeline_instructions[%u].jmp.b_val)",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_GT:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) > "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_GT_MH:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) > "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_GT_HM:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) > "
			"instr_operand_hbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_GT_HH:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) > "
			"instr_operand_nbo(t, &pipeline_instructions[%u].jmp.b))",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_GT_MI:
		fprintf(f,
			"if (instr_operand_hbo(t, &pipeline_instructions[%u].jmp.a) > "
			"pipeline_instructions[%u].jmp.b_val)",
			jmp_instr_id,
			jmp_instr_id);
		break;

	case INSTR_JMP_GT_HI:
		fprintf(f,
			"if (instr_operand_nbo(t, &pipeline_instructions[%u].jmp.a) > "
			"pipeline_instructions[%u].jmp.b_val)",
			jmp_instr_id,
			jmp_instr_id);
		break;

	default:
		break;
	}

	/* Find the instruction group of the jump instruction. */
	jmp_g = instruction_group_list_group_find(igl, jmp_instr_id);
	if (!jmp_g)
		return -EINVAL;

	/* Find the instruction group of the jump destination instruction. */
	data = label_find(p->instruction_data, p->n_instructions, jmp_data->jmp_label);
	if (!data)
		return -EINVAL;

	instr_id = data - p->instruction_data;

	g = instruction_group_list_group_find(igl, instr_id);
	if (!g)
		return -EINVAL;

	/* Code generation for "near" jump (same instruction group) or "far" jump (different
	 * instruction group).
	 */
	if (g->group_id == jmp_g->group_id)
		fprintf(f,
			"\n\t\tgoto %s;\n",
			jmp_data->jmp_label);
	else
		fprintf(f,
			" {\n"
			"\t\tthread_ip_set(t, &p->instructions[%u]);\n"
			"\t\treturn;\n"
			"\t}\n\n",
			g->group_id);

	return 0;
}

static void
instruction_group_list_codegen(struct instruction_group_list *igl,
			       struct rte_swx_pipeline *p,
			       FILE *f)
{
	struct instruction_group *g;
	uint32_t i;
	int is_required = 0;

	/* Check if code generation is required. */
	TAILQ_FOREACH(g, igl, node)
		if (g->first_instr_id < g->last_instr_id)
			is_required = 1;

	if (!is_required)
		return;

	/* Generate the code for the pipeline instruction array. */
	fprintf(f,
		"static const struct instruction pipeline_instructions[] = {\n");

	for (i = 0; i < p->n_instructions; i++) {
		struct instruction *instr = &p->instructions[i];
		instruction_export_t func = export_table[instr->type];

		func(instr, f);
	}

	fprintf(f, "};\n\n");

	/* Generate the code for the pipeline functions: one function for each instruction group
	 * that contains more than one instruction.
	 */
	TAILQ_FOREACH(g, igl, node) {
		struct instruction *last_instr;
		uint32_t j;

		/* Skip if group contains a single instruction. */
		if (g->last_instr_id == g->first_instr_id)
			continue;

		/* Generate new pipeline function. */
		fprintf(f,
			"void\n"
			"pipeline_func_%u(struct rte_swx_pipeline *p)\n"
			"{\n"
			"\tstruct thread *t = &p->threads[p->thread_id];\n"
			"\n",
			g->group_id);

		/* Generate the code for each pipeline instruction. */
		for (j = g->first_instr_id; j <= g->last_instr_id; j++) {
			struct instruction *instr = &p->instructions[j];
			struct instruction_data *data = &p->instruction_data[j];

			/* Label, if present. */
			if (data->label[0])
				fprintf(f, "\n%s : ", data->label);
			else
				fprintf(f, "\n\t");

			/* TX instruction type. */
			if (instruction_does_tx(instr)) {
				pipeline_instr_does_tx_codegen(p, j, instr, f);
				continue;
			}

			/* Jump instruction type. */
			if (instruction_is_jmp(instr)) {
				pipeline_instr_jmp_codegen(p, igl, j, instr, data, f);
				continue;
			}

			/* Any other instruction type. */
			fprintf(f,
				"%s(p, t, &pipeline_instructions[%u]);\n",
				instr_type_to_func(instr),
				j);
		}

		/* Finalize the generated pipeline function. For some instructions such as TX,
		 * emit-many-and-TX and unconditional jump, the next instruction has been already
		 * decided unconditionally and the instruction pointer of the current thread set
		 * accordingly; for all the other instructions, the instruction pointer must be
		 * incremented now.
		 */
		last_instr = &p->instructions[g->last_instr_id];

		if (!instruction_does_tx(last_instr) && (last_instr->type != INSTR_JMP))
			fprintf(f,
				"thread_ip_inc(p);\n");

		fprintf(f,
			"}\n"
			"\n");
	}
}

static uint32_t
instruction_group_list_custom_instructions_count(struct instruction_group_list *igl)
{
	struct instruction_group *g;
	uint32_t n_custom_instr = 0;

	/* Groups with a single instruction: no function is generated for this group, the group
	 * keeps its current instruction. Groups with more than two instructions: one function and
	 * the associated custom instruction get generated for each such group.
	 */
	TAILQ_FOREACH(g, igl, node) {
		if (g->first_instr_id == g->last_instr_id)
			continue;

		n_custom_instr++;
	}

	return n_custom_instr;
}

static int
pipeline_adjust_check(struct rte_swx_pipeline *p __rte_unused,
		      struct instruction_group_list *igl)
{
	uint32_t n_custom_instr = instruction_group_list_custom_instructions_count(igl);

	/* Check that enough space is available within the pipeline instruction table to store all
	 * the custom instructions.
	 */
	if (INSTR_CUSTOM_0 + n_custom_instr > RTE_SWX_PIPELINE_INSTRUCTION_TABLE_SIZE_MAX)
		return -ENOSPC;

	return 0;
}

static void
pipeline_adjust(struct rte_swx_pipeline *p, struct instruction_group_list *igl)
{
	struct instruction_group *g;
	uint32_t i;

	/* Pipeline table instructions. */
	for (i = 0; i < p->n_instructions; i++) {
		struct instruction *instr = &p->instructions[i];

		if (instr->type == INSTR_TABLE)
			instr->type = INSTR_TABLE_AF;

		if (instr->type == INSTR_LEARNER)
			instr->type = INSTR_LEARNER_AF;
	}

	/* Pipeline custom instructions. */
	i = 0;
	TAILQ_FOREACH(g, igl, node) {
		struct instruction *instr = &p->instructions[g->first_instr_id];
		uint32_t j;

		if (g->first_instr_id == g->last_instr_id)
			continue;

		/* Install a new custom instruction. */
		p->instruction_table[INSTR_CUSTOM_0 + i] = g->func;

		/* First instruction of the group: change its type to the new custom instruction. */
		instr->type = INSTR_CUSTOM_0 + i;

		/* All the subsequent instructions of the group: invalidate. */
		for (j = g->first_instr_id + 1; j <= g->last_instr_id; j++) {
			struct instruction_data *data = &p->instruction_data[j];

			data->invalid = 1;
		}

		i++;
	}

	/* Remove the invalidated instructions. */
	p->n_instructions = instr_compact(p->instructions, p->instruction_data, p->n_instructions);

	/* Resolve the jump destination for any "standalone" jump instructions (i.e. those jump
	 * instructions that are the only instruction within their group, so they were left
	 * unmodified).
	 */
	instr_jmp_resolve(p->instructions, p->instruction_data, p->n_instructions);
}

int
rte_swx_pipeline_codegen(FILE *spec_file,
			 FILE *code_file,
			 uint32_t *err_line,
			 const char **err_msg)

{
	struct rte_swx_pipeline *p = NULL;
	struct pipeline_spec *s = NULL;
	struct instruction_group_list *igl = NULL;
	struct action *a;
	int status = 0;

	/* Check input arguments. */
	if (!spec_file || !code_file) {
		if (err_line)
			*err_line = 0;
		if (err_msg)
			*err_msg = "Invalid input argument.";
		status = -EINVAL;
		goto free;
	}

	/* Pipeline configuration. */
	s = pipeline_spec_parse(spec_file, err_line, err_msg);
	if (!s) {
		status = -EINVAL;
		goto free;
	}

	status = rte_swx_pipeline_config(&p, NULL, 0);
	if (status) {
		if (err_line)
			*err_line = 0;
		if (err_msg)
			*err_msg = "Pipeline configuration error.";
		goto free;
	}

	status = pipeline_spec_configure(p, s, err_msg);
	if (status) {
		if (err_line)
			*err_line = 0;
		goto free;
	}

	/*
	 * Pipeline code generation.
	 */

	/* Instruction Group List (IGL) computation: the pipeline configuration must be done first,
	 * but there is no need for the pipeline build to be done as well.
	 */
	igl = instruction_group_list_create(p);
	if (!igl) {
		if (err_line)
			*err_line = 0;
		if (err_msg)
			*err_msg = "Memory allocation failed.";
		status = -ENOMEM;
		goto free;
	}

	/* Header file inclusion. */
	fprintf(code_file, "#include \"rte_swx_pipeline_internal.h\"\n");
	fprintf(code_file, "#include \"rte_swx_pipeline_spec.h\"\n\n");

	/* Code generation for the pipeline specification. */
	pipeline_spec_codegen(code_file, s);
	fprintf(code_file, "\n");

	/* Code generation for the action instructions. */
	TAILQ_FOREACH(a, &p->actions, node) {
		fprintf(code_file, "/**\n * Action %s\n */\n\n", a->name);

		action_data_codegen(a, code_file);
		fprintf(code_file, "\n");

		action_instr_codegen(a, code_file);
		fprintf(code_file, "\n");
	}

	/* Code generation for the pipeline instructions. */
	instruction_group_list_codegen(igl, p, code_file);

free:
	instruction_group_list_free(igl);
	rte_swx_pipeline_free(p);
	pipeline_spec_free(s);

	return status;
}

int
rte_swx_pipeline_build_from_lib(struct rte_swx_pipeline **pipeline,
				const char *name,
				const char *lib_file_name,
				FILE *iospec_file,
				int numa_node)
{
	struct rte_swx_pipeline *p = NULL;
	void *lib = NULL;
	struct pipeline_iospec *sio = NULL;
	struct pipeline_spec *s = NULL;
	struct instruction_group_list *igl = NULL;
	struct action *a;
	struct instruction_group *g;
	int status = 0;

	/* Check input arguments. */
	if (!pipeline ||
	    !name ||
	    !name[0] ||
	    !lib_file_name ||
	    !lib_file_name[0] ||
	    !iospec_file) {
		status = -EINVAL;
		goto free;
	}

	/* Open the library. */
	lib = dlopen(lib_file_name, RTLD_LAZY);
	if (!lib) {
		status = -EIO;
		goto free;
	}

	/* Get the pipeline specification structures. */
	s = dlsym(lib, "pipeline_spec");
	if (!s) {
		status = -EINVAL;
		goto free;
	}

	sio = pipeline_iospec_parse(iospec_file, NULL, NULL);
	if (!sio) {
		status = -EINVAL;
		goto free;
	}

	/* Pipeline configuration based on the specification structures. */
	status = rte_swx_pipeline_config(&p, name, numa_node);
	if (status)
		goto free;

	status = pipeline_iospec_configure(p, sio, NULL);
	if (status)
		goto free;

	status = pipeline_spec_configure(p, s, NULL);
	if (status)
		goto free;

	/* Pipeline build. */
	status = rte_swx_pipeline_build(p);
	if (status)
		goto free;

	/* Action instructions. */
	TAILQ_FOREACH(a, &p->actions, node) {
		char name[RTE_SWX_NAME_SIZE * 2];

		snprintf(name, sizeof(name), "action_%s_run", a->name);

		p->action_funcs[a->id] = dlsym(lib, name);
		if (!p->action_funcs[a->id]) {
			status = -EINVAL;
			goto free;
		}
	}

	/* Pipeline instructions. */
	igl = instruction_group_list_create(p);
	if (!igl) {
		status = -ENOMEM;
		goto free;
	}

	TAILQ_FOREACH(g, igl, node) {
		char name[RTE_SWX_NAME_SIZE * 2];

		if (g->first_instr_id == g->last_instr_id)
			continue;

		snprintf(name, sizeof(name), "pipeline_func_%u", g->group_id);

		g->func = dlsym(lib, name);
		if (!g->func) {
			status = -EINVAL;
			goto free;
		}
	}

	status = pipeline_adjust_check(p, igl);
	if (status)
		goto free;

	pipeline_adjust(p, igl);

	p->lib = lib;

	*pipeline = p;

free:
	instruction_group_list_free(igl);

	pipeline_iospec_free(sio);

	if (status) {
		rte_swx_pipeline_free(p);

		if (lib)
			dlclose(lib);
	}

	return status;
}
