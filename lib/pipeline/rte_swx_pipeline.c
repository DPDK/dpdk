/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

#include "rte_swx_pipeline_internal.h"

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
		int var_size = ((i == n_fields - 1) && last_field_has_variable_size) ? 1 : 0;
		uint32_t j;

		CHECK_NAME(f->name, EINVAL);
		CHECK(f->n_bits, EINVAL);
		CHECK((f->n_bits <= 64) || var_size, EINVAL);
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
instr_drop_translate(struct rte_swx_pipeline *p,
		     struct action *action __rte_unused,
		     char **tokens __rte_unused,
		     int n_tokens,
		     struct instruction *instr,
		     struct instruction_data *data __rte_unused)
{
	CHECK(n_tokens == 1, EINVAL);

	/* TX_I. */
	instr->type = INSTR_TX_I;
	instr->io.io.val = p->n_ports_out - 1;
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
		CHECK(!mf->var_size, EINVAL);

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
	int done, hit;

	/* Table. */
	done = table->func(ts->obj,
			   table->mailbox,
			   table->key,
			   &action_id,
			   &action_data,
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
	n_pkts_hit = stats->n_pkts_hit[hit];
	n_pkts_action = stats->n_pkts_action[action_id];

	TRACE("[Thread %2u] table %u (%s, action %u)\n",
	      p->thread_id,
	      table_id,
	      hit ? "hit" : "miss",
	      (uint32_t)action_id);

	t->action_id = action_id;
	t->structs[0] = action_data;
	t->hit = hit;
	stats->n_pkts_hit[hit] = n_pkts_hit + 1;
	stats->n_pkts_action[action_id] = n_pkts_action + 1;

	/* Thread. */
	thread_ip_action_call(p, t, action_id);
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
	int done, hit;

	/* Table. */
	time = rte_get_tsc_cycles();

	done = rte_swx_table_learner_lookup(ts->obj,
					    l->mailbox,
					    time,
					    l->key,
					    &action_id,
					    &action_data,
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
	n_pkts_hit = stats->n_pkts_hit[hit];
	n_pkts_action = stats->n_pkts_action[action_id];

	TRACE("[Thread %2u] learner %u (%s, action %u)\n",
	      p->thread_id,
	      learner_id,
	      hit ? "hit" : "miss",
	      (uint32_t)action_id);

	t->action_id = action_id;
	t->structs[0] = action_data;
	t->hit = hit;
	t->learner_id = learner_id;
	t->time = time;
	stats->n_pkts_hit[hit] = n_pkts_hit + 1;
	stats->n_pkts_action[action_id] = n_pkts_action + 1;

	/* Thread. */
	thread_ip_action_call(p, t, action_id);
}

/*
 * learn.
 */
static struct action *
action_find(struct rte_swx_pipeline *p, const char *name);

static int
action_has_nbo_args(struct action *a);

static int
instr_learn_translate(struct rte_swx_pipeline *p,
		      struct action *action,
		      char **tokens,
		      int n_tokens,
		      struct instruction *instr,
		      struct instruction_data *data __rte_unused)
{
	struct action *a;

	CHECK(action, EINVAL);
	CHECK(n_tokens == 2, EINVAL);

	a = action_find(p, tokens[1]);
	CHECK(a, EINVAL);
	CHECK(!action_has_nbo_args(a), EINVAL);

	instr->type = INSTR_LEARNER_LEARN;
	instr->learn.action_id = a->id;

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

	/* MOV, MOV_MH, MOV_HM or MOV_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

		instr->type = INSTR_MOV;
		if (dst[0] != 'h' && src[0] == 'h')
			instr->type = INSTR_MOV_MH;
		if (dst[0] == 'h' && src[0] != 'h')
			instr->type = INSTR_MOV_HM;
		if (dst[0] == 'h' && src[0] == 'h')
			instr->type = INSTR_MOV_HH;

		instr->mov.dst.struct_id = (uint8_t)dst_struct_id;
		instr->mov.dst.n_bits = fdst->n_bits;
		instr->mov.dst.offset = fdst->offset / 8;
		instr->mov.src.struct_id = (uint8_t)src_struct_id;
		instr->mov.src.n_bits = fsrc->n_bits;
		instr->mov.src.offset = fsrc->offset / 8;
		return 0;
	}

	/* MOV_I. */
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
	CHECK(!fdst->var_size, EINVAL);

	/* ADD, ADD_HM, ADD_MH, ADD_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* SUB, SUB_HM, SUB_MH, SUB_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(fdst && (fdst->n_bits == 16), EINVAL);
	CHECK(!fdst->var_size, EINVAL);

	/* CKADD_FIELD. */
	fsrc = header_field_parse(p, src, &hsrc);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!hsrc->st->var_size, EINVAL);

	instr->type = INSTR_ALU_CKADD_STRUCT;
	if ((hsrc->st->n_bits / 8) == 20)
		instr->type = INSTR_ALU_CKADD_STRUCT20;

	instr->alu.dst.struct_id = (uint8_t)hdst->struct_id;
	instr->alu.dst.n_bits = fdst->n_bits;
	instr->alu.dst.offset = fdst->offset / 8;
	instr->alu.src.struct_id = (uint8_t)hsrc->struct_id;
	instr->alu.src.n_bits = hsrc->st->n_bits;
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
	CHECK(fdst && (fdst->n_bits == 16), EINVAL);
	CHECK(!fdst->var_size, EINVAL);

	fsrc = header_field_parse(p, src, &hsrc);
	CHECK(fsrc, EINVAL);
	CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* SHL, SHL_HM, SHL_MH, SHL_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* SHR, SHR_HM, SHR_MH, SHR_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* AND, AND_MH, AND_HM, AND_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* OR, OR_MH, OR_HM, OR_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* XOR, XOR_MH, XOR_HM, XOR_HH. */
	fsrc = struct_field_parse(p, action, src, &src_struct_id);
	if (fsrc) {
		CHECK(!fsrc->var_size, EINVAL);

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

	TRACE("[Thread %2u] add\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_add_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] add (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, +);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_sub_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] sub (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, -);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shl_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shl (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, <<);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shr\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, >>);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shr (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, >>);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shr (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM(t, ip, >>);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shr (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH(t, ip, >>);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_mi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shr (mi)\n", p->thread_id);

	/* Structs. */
	ALU_MI(t, ip, >>);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_shr_hi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] shr (hi)\n", p->thread_id);

	/* Structs. */
	ALU_HI(t, ip, >>);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM_FAST(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH_FAST(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_and_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] and (i)\n", p->thread_id);

	/* Structs. */
	ALU_I(t, ip, &);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM_FAST(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH_FAST(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_or_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] or (i)\n", p->thread_id);

	/* Structs. */
	ALU_I(t, ip, |);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor\n", p->thread_id);

	/* Structs. */
	ALU(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_mh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor (mh)\n", p->thread_id);

	/* Structs. */
	ALU_MH(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_hm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor (hm)\n", p->thread_id);

	/* Structs. */
	ALU_HM_FAST(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_hh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor (hh)\n", p->thread_id);

	/* Structs. */
	ALU_HH_FAST(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_xor_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;

	TRACE("[Thread %2u] xor (i)\n", p->thread_id);

	/* Structs. */
	ALU_I(t, ip, ^);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_field_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr, dst;
	uint64_t *src64_ptr, src64, src64_mask, src;
	uint64_t r;

	TRACE("[Thread %2u] ckadd (field)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];
	dst = *dst16_ptr;

	src_struct = t->structs[ip->alu.src.struct_id];
	src64_ptr = (uint64_t *)&src_struct[ip->alu.src.offset];
	src64 = *src64_ptr;
	src64_mask = UINT64_MAX >> (64 - ip->alu.src.n_bits);
	src = src64 & src64_mask;

	r = dst;
	r = ~r & 0xFFFF;

	/* The first input (r) is a 16-bit number. The second and the third
	 * inputs are 32-bit numbers. In the worst case scenario, the sum of the
	 * three numbers (output r) is a 34-bit number.
	 */
	r += (src >> 32) + (src & 0xFFFFFFFF);

	/* The first input is a 16-bit number. The second input is an 18-bit
	 * number. In the worst case scenario, the sum of the two numbers is a
	 * 19-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 3-bit number (0 .. 7). Their sum is a 17-bit number (0 .. 0x10006).
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x10006), the output r is (0 .. 7). So no carry bit can be generated,
	 * therefore the output r is always a 16-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	r = ~r & 0xFFFF;
	r = r ? r : 0xFFFF;

	*dst16_ptr = (uint16_t)r;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_cksub_field_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr, dst;
	uint64_t *src64_ptr, src64, src64_mask, src;
	uint64_t r;

	TRACE("[Thread %2u] cksub (field)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];
	dst = *dst16_ptr;

	src_struct = t->structs[ip->alu.src.struct_id];
	src64_ptr = (uint64_t *)&src_struct[ip->alu.src.offset];
	src64 = *src64_ptr;
	src64_mask = UINT64_MAX >> (64 - ip->alu.src.n_bits);
	src = src64 & src64_mask;

	r = dst;
	r = ~r & 0xFFFF;

	/* Subtraction in 1's complement arithmetic (i.e. a '- b) is the same as
	 * the following sequence of operations in 2's complement arithmetic:
	 *    a '- b = (a - b) % 0xFFFF.
	 *
	 * In order to prevent an underflow for the below subtraction, in which
	 * a 33-bit number (the subtrahend) is taken out of a 16-bit number (the
	 * minuend), we first add a multiple of the 0xFFFF modulus to the
	 * minuend. The number we add to the minuend needs to be a 34-bit number
	 * or higher, so for readability reasons we picked the 36-bit multiple.
	 * We are effectively turning the 16-bit minuend into a 36-bit number:
	 *    (a - b) % 0xFFFF = (a + 0xFFFF00000 - b) % 0xFFFF.
	 */
	r += 0xFFFF00000ULL; /* The output r is a 36-bit number. */

	/* A 33-bit number is subtracted from a 36-bit number (the input r). The
	 * result (the output r) is a 36-bit number.
	 */
	r -= (src >> 32) + (src & 0xFFFFFFFF);

	/* The first input is a 16-bit number. The second input is a 20-bit
	 * number. Their sum is a 21-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 5-bit number (0 .. 31). The sum is a 17-bit number (0 .. 0x1001E).
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x1001E), the output r is (0 .. 31). So no carry bit can be
	 * generated, therefore the output r is always a 16-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	r = ~r & 0xFFFF;
	r = r ? r : 0xFFFF;

	*dst16_ptr = (uint16_t)r;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_struct20_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr;
	uint32_t *src32_ptr;
	uint64_t r0, r1;

	TRACE("[Thread %2u] ckadd (struct of 20 bytes)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];

	src_struct = t->structs[ip->alu.src.struct_id];
	src32_ptr = (uint32_t *)&src_struct[0];

	r0 = src32_ptr[0]; /* r0 is a 32-bit number. */
	r1 = src32_ptr[1]; /* r1 is a 32-bit number. */
	r0 += src32_ptr[2]; /* The output r0 is a 33-bit number. */
	r1 += src32_ptr[3]; /* The output r1 is a 33-bit number. */
	r0 += r1 + src32_ptr[4]; /* The output r0 is a 35-bit number. */

	/* The first input is a 16-bit number. The second input is a 19-bit
	 * number. Their sum is a 20-bit number.
	 */
	r0 = (r0 & 0xFFFF) + (r0 >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 4-bit number (0 .. 15). The sum is a 17-bit number (0 .. 0x1000E).
	 */
	r0 = (r0 & 0xFFFF) + (r0 >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x1000E), the output r is (0 .. 15). So no carry bit can be
	 * generated, therefore the output r is always a 16-bit number.
	 */
	r0 = (r0 & 0xFFFF) + (r0 >> 16);

	r0 = ~r0 & 0xFFFF;
	r0 = r0 ? r0 : 0xFFFF;

	*dst16_ptr = (uint16_t)r0;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_alu_ckadd_struct_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint8_t *dst_struct, *src_struct;
	uint16_t *dst16_ptr;
	uint32_t *src32_ptr;
	uint64_t r = 0;
	uint32_t i;

	TRACE("[Thread %2u] ckadd (struct)\n", p->thread_id);

	/* Structs. */
	dst_struct = t->structs[ip->alu.dst.struct_id];
	dst16_ptr = (uint16_t *)&dst_struct[ip->alu.dst.offset];

	src_struct = t->structs[ip->alu.src.struct_id];
	src32_ptr = (uint32_t *)&src_struct[0];

	/* The max number of 32-bit words in a 256-byte header is 8 = 2^3.
	 * Therefore, in the worst case scenario, a 35-bit number is added to a
	 * 16-bit number (the input r), so the output r is 36-bit number.
	 */
	for (i = 0; i < ip->alu.src.n_bits / 32; i++, src32_ptr++)
		r += *src32_ptr;

	/* The first input is a 16-bit number. The second input is a 20-bit
	 * number. Their sum is a 21-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* The first input is a 16-bit number (0 .. 0xFFFF). The second input is
	 * a 5-bit number (0 .. 31). The sum is a 17-bit number (0 .. 0x1000E).
	 */
	r = (r & 0xFFFF) + (r >> 16);

	/* When the input r is (0 .. 0xFFFF), the output r is equal to the input
	 * r, so the output is (0 .. 0xFFFF). When the input r is (0x10000 ..
	 * 0x1001E), the output r is (0 .. 31). So no carry bit can be
	 * generated, therefore the output r is always a 16-bit number.
	 */
	r = (r & 0xFFFF) + (r >> 16);

	r = ~r & 0xFFFF;
	r = r ? r : 0xFFFF;

	*dst16_ptr = (uint16_t)r;

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
		CHECK(!fidx->var_size, EINVAL);

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
	CHECK(!fdst->var_size, EINVAL);

	/* REGRD_HRH, REGRD_HRM, REGRD_MRH, REGRD_MRM. */
	fidx = struct_field_parse(p, action, idx, &idx_struct_id);
	if (fidx) {
		CHECK(!fidx->var_size, EINVAL);

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
		CHECK(!fidx->var_size, EINVAL);
		CHECK(!fsrc->var_size, EINVAL);

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
		CHECK(!fidx->var_size, EINVAL);

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

		CHECK(!fsrc->var_size, EINVAL);

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
		CHECK(!fidx->var_size, EINVAL);
		CHECK(!fsrc->var_size, EINVAL);

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
		CHECK(!fidx->var_size, EINVAL);

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

		CHECK(!fsrc->var_size, EINVAL);

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

static inline uint64_t *
instr_regarray_regarray(struct rte_swx_pipeline *p, struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];
	return r->regarray;
}

static inline uint64_t
instr_regarray_idx_hbo(struct rte_swx_pipeline *p, struct thread *t, struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];

	uint8_t *idx_struct = t->structs[ip->regarray.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->regarray.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx64_mask = UINT64_MAX >> (64 - ip->regarray.idx.n_bits);
	uint64_t idx = idx64 & idx64_mask & r->size_mask;

	return idx;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint64_t
instr_regarray_idx_nbo(struct rte_swx_pipeline *p, struct thread *t, struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];

	uint8_t *idx_struct = t->structs[ip->regarray.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->regarray.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx = (ntoh64(idx64) >> (64 - ip->regarray.idx.n_bits)) & r->size_mask;

	return idx;
}

#else

#define instr_regarray_idx_nbo instr_regarray_idx_hbo

#endif

static inline uint64_t
instr_regarray_idx_imm(struct rte_swx_pipeline *p, struct instruction *ip)
{
	struct regarray_runtime *r = &p->regarray_runtime[ip->regarray.regarray_id];

	uint64_t idx = ip->regarray.idx_val & r->size_mask;

	return idx;
}

static inline uint64_t
instr_regarray_src_hbo(struct thread *t, struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->regarray.dstsrc.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src64_mask = UINT64_MAX >> (64 - ip->regarray.dstsrc.n_bits);
	uint64_t src = src64 & src64_mask;

	return src;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint64_t
instr_regarray_src_nbo(struct thread *t, struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->regarray.dstsrc.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src = ntoh64(src64) >> (64 - ip->regarray.dstsrc.n_bits);

	return src;
}

#else

#define instr_regarray_src_nbo instr_regarray_src_hbo

#endif

static inline void
instr_regarray_dst_hbo_src_hbo_set(struct thread *t, struct instruction *ip, uint64_t src)
{
	uint8_t *dst_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[ip->regarray.dstsrc.offset];
	uint64_t dst64 = *dst64_ptr;
	uint64_t dst64_mask = UINT64_MAX >> (64 - ip->regarray.dstsrc.n_bits);

	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);

}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline void
instr_regarray_dst_nbo_src_hbo_set(struct thread *t, struct instruction *ip, uint64_t src)
{
	uint8_t *dst_struct = t->structs[ip->regarray.dstsrc.struct_id];
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[ip->regarray.dstsrc.offset];
	uint64_t dst64 = *dst64_ptr;
	uint64_t dst64_mask = UINT64_MAX >> (64 - ip->regarray.dstsrc.n_bits);

	src = hton64(src) >> (64 - ip->regarray.dstsrc.n_bits);
	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);
}

#else

#define instr_regarray_dst_nbo_src_hbo_set instr_regarray_dst_hbo_src_hbo_set

#endif

static inline void
instr_regprefetch_rh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regprefetch (r[h])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	rte_prefetch0(&regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regprefetch_rm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regprefetch (r[m])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	rte_prefetch0(&regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regprefetch_ri_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regprefetch (r[i])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	rte_prefetch0(&regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_hrh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (h = r[h])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	instr_regarray_dst_nbo_src_hbo_set(t, ip, regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_hrm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (h = r[m])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	instr_regarray_dst_nbo_src_hbo_set(t, ip, regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_mrh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (m = r[h])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	instr_regarray_dst_hbo_src_hbo_set(t, ip, regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_mrm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	instr_regarray_dst_hbo_src_hbo_set(t, ip, regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_hri_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (h = r[i])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	instr_regarray_dst_nbo_src_hbo_set(t, ip, regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regrd_mri_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx;

	TRACE("[Thread %2u] regrd (m = r[i])\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	instr_regarray_dst_hbo_src_hbo_set(t, ip, regarray[idx]);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rhh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[h] = h)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[h] = m)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rmh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[m] = h)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[m] = m)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[h] = i)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[m] = i)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rih_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[i] = h)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rim_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[i] = m)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regwr_rii_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regwr (r[i] = i)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] = src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rhh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[h] += h)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[h] += m)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rmh_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[m] += h)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[m] += m)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[h] += i)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_nbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[m] += i)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_hbo(p, t, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rih_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[i] += h)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_nbo(t, ip);
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rim_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[i] += m)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = instr_regarray_src_hbo(t, ip);
	regarray[idx] += src;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_regadd_rii_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	uint64_t *regarray, idx, src;

	TRACE("[Thread %2u] regadd (r[i] += i)\n", p->thread_id);

	/* Structs. */
	regarray = instr_regarray_regarray(p, ip);
	idx = instr_regarray_idx_imm(p, ip);
	src = ip->regarray.dstsrc_val;
	regarray[idx] += src;

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
		CHECK(!fidx->var_size, EINVAL);

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
	CHECK(!flength->var_size, EINVAL);

	fcin = struct_field_parse(p, action, color_in, &color_in_struct_id);

	fcout = struct_field_parse(p, NULL, color_out, &color_out_struct_id);
	CHECK(fcout, EINVAL);
	CHECK(!fcout->var_size, EINVAL);

	/* index = HMEFT, length = HMEFT, color_in = MEFT, color_out = MEF. */
	if (fidx && fcin) {
		CHECK(!fidx->var_size, EINVAL);
		CHECK(!fcin->var_size, EINVAL);

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

		return 0;
	}

	/* index = HMEFT, length = HMEFT, color_in = I, color_out = MEF. */
	if (fidx && !fcin) {
		uint32_t color_in_val;

		CHECK(!fidx->var_size, EINVAL);

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

		return 0;
	}

	/* index = I, length = HMEFT, color_in = MEFT, color_out = MEF. */
	if (!fidx && fcin) {
		uint32_t idx_val;

		idx_val = strtoul(idx, &idx, 0);
		CHECK(!idx[0], EINVAL);

		CHECK(!fcin->var_size, EINVAL);

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

		return 0;
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

		return 0;
	}

	CHECK(0, EINVAL);
}

static inline struct meter *
instr_meter_idx_hbo(struct rte_swx_pipeline *p, struct thread *t, struct instruction *ip)
{
	struct metarray_runtime *r = &p->metarray_runtime[ip->meter.metarray_id];

	uint8_t *idx_struct = t->structs[ip->meter.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->meter.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx64_mask = UINT64_MAX >> (64 - (ip)->meter.idx.n_bits);
	uint64_t idx = idx64 & idx64_mask & r->size_mask;

	return &r->metarray[idx];
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline struct meter *
instr_meter_idx_nbo(struct rte_swx_pipeline *p, struct thread *t, struct instruction *ip)
{
	struct metarray_runtime *r = &p->metarray_runtime[ip->meter.metarray_id];

	uint8_t *idx_struct = t->structs[ip->meter.idx.struct_id];
	uint64_t *idx64_ptr = (uint64_t *)&idx_struct[ip->meter.idx.offset];
	uint64_t idx64 = *idx64_ptr;
	uint64_t idx = (ntoh64(idx64) >> (64 - ip->meter.idx.n_bits)) & r->size_mask;

	return &r->metarray[idx];
}

#else

#define instr_meter_idx_nbo instr_meter_idx_hbo

#endif

static inline struct meter *
instr_meter_idx_imm(struct rte_swx_pipeline *p, struct instruction *ip)
{
	struct metarray_runtime *r = &p->metarray_runtime[ip->meter.metarray_id];

	uint64_t idx =  ip->meter.idx_val & r->size_mask;

	return &r->metarray[idx];
}

static inline uint32_t
instr_meter_length_hbo(struct thread *t, struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->meter.length.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->meter.length.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src64_mask = UINT64_MAX >> (64 - (ip)->meter.length.n_bits);
	uint64_t src = src64 & src64_mask;

	return (uint32_t)src;
}

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

static inline uint32_t
instr_meter_length_nbo(struct thread *t, struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->meter.length.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->meter.length.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src = ntoh64(src64) >> (64 - ip->meter.length.n_bits);

	return (uint32_t)src;
}

#else

#define instr_meter_length_nbo instr_meter_length_hbo

#endif

static inline enum rte_color
instr_meter_color_in_hbo(struct thread *t, struct instruction *ip)
{
	uint8_t *src_struct = t->structs[ip->meter.color_in.struct_id];
	uint64_t *src64_ptr = (uint64_t *)&src_struct[ip->meter.color_in.offset];
	uint64_t src64 = *src64_ptr;
	uint64_t src64_mask = UINT64_MAX >> (64 - ip->meter.color_in.n_bits);
	uint64_t src = src64 & src64_mask;

	return (enum rte_color)src;
}

static inline void
instr_meter_color_out_hbo_set(struct thread *t, struct instruction *ip, enum rte_color color_out)
{
	uint8_t *dst_struct = t->structs[ip->meter.color_out.struct_id];
	uint64_t *dst64_ptr = (uint64_t *)&dst_struct[ip->meter.color_out.offset];
	uint64_t dst64 = *dst64_ptr;
	uint64_t dst64_mask = UINT64_MAX >> (64 - ip->meter.color_out.n_bits);

	uint64_t src = (uint64_t)color_out;

	*dst64_ptr = (dst64 & ~dst64_mask) | (src & dst64_mask);
}

static inline void
instr_metprefetch_h_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;

	TRACE("[Thread %2u] metprefetch (h)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_metprefetch_m_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;

	TRACE("[Thread %2u] metprefetch (m)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_metprefetch_i_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;

	TRACE("[Thread %2u] metprefetch (i)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m);

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hhm)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hhi)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_hmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hmm)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}
static inline void
instr_meter_hmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (hmi)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_nbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mhm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mhm)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mhi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mhi)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mmm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mmm)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_mmi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (mmi)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_hbo(p, t, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_ihm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (ihm)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_ihi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (ihi)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_nbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}

static inline void
instr_meter_imm_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (imm)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = instr_meter_color_in_hbo(t, ip);

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

	/* Thread. */
	thread_ip_inc(p);
}
static inline void
instr_meter_imi_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	struct meter *m;
	uint64_t time, n_pkts, n_bytes;
	uint32_t length;
	enum rte_color color_in, color_out;

	TRACE("[Thread %2u] meter (imi)\n", p->thread_id);

	/* Structs. */
	m = instr_meter_idx_imm(p, ip);
	rte_prefetch0(m->n_pkts);
	time = rte_get_tsc_cycles();
	length = instr_meter_length_hbo(t, ip);
	color_in = (enum rte_color)ip->meter.color_in_val;

	color_out = rte_meter_trtcm_color_aware_check(&m->m,
		&m->profile->profile,
		time,
		length,
		color_in);

	color_out &= m->color_mask;

	n_pkts = m->n_pkts[color_out];
	n_bytes = m->n_bytes[color_out];

	instr_meter_color_out_hbo_set(t, ip, color_out);

	m->n_pkts[color_out] = n_pkts + 1;
	m->n_bytes[color_out] = n_bytes + length;

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
	CHECK(!fa->var_size, EINVAL);

	/* JMP_EQ, JMP_EQ_MH, JMP_EQ_HM, JMP_EQ_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size, EINVAL);

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
	CHECK(!fa->var_size, EINVAL);

	/* JMP_NEQ, JMP_NEQ_MH, JMP_NEQ_HM, JMP_NEQ_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size, EINVAL);

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
	CHECK(!fa->var_size, EINVAL);

	/* JMP_LT, JMP_LT_MH, JMP_LT_HM, JMP_LT_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size, EINVAL);

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
	CHECK(!fa->var_size, EINVAL);

	/* JMP_GT, JMP_GT_MH, JMP_GT_HM, JMP_GT_HH. */
	fb = struct_field_parse(p, action, b, &b_struct_id);
	if (fb) {
		CHECK(!fb->var_size, EINVAL);

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

	if (!strcmp(tokens[tpos], "forget"))
		return instr_forget_translate(p,
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

	CHECK(0, EINVAL);
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
			CHECK(strcmp(label, data[j].label), EINVAL);
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

	if (!instruction_is_tx(instr[i].type))
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
instr_pattern_mov_all_validate_search(struct rte_swx_pipeline *p,
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

	/* First instruction: MOV_HM. */
	if (data[0].invalid || (instr[0].type != INSTR_MOV_HM))
		return 0;

	h = header_find_by_struct_id(p, instr[0].mov.dst.struct_id);
	if (!h || h->st->var_size)
		return 0;

	for (src_field_id = 0; src_field_id < a->st->n_fields; src_field_id++)
		if (instr[0].mov.src.offset == a->st->fields[src_field_id].offset / 8)
			break;

	if (src_field_id == a->st->n_fields)
		return 0;

	if (instr[0].mov.dst.offset ||
	    (instr[0].mov.dst.n_bits != h->st->fields[0].n_bits) ||
	    instr[0].mov.src.struct_id ||
	    (instr[0].mov.src.n_bits != a->st->fields[src_field_id].n_bits) ||
	    (instr[0].mov.dst.n_bits != instr[0].mov.src.n_bits))
		return 0;

	if ((n_instr < h->st->n_fields + 1) ||
	     (a->st->n_fields < src_field_id + h->st->n_fields + 1))
		return 0;

	/* Subsequent instructions: MOV_HM. */
	for (i = 1; i < h->st->n_fields; i++)
		if (data[i].invalid ||
		    data[i].n_users ||
		    (instr[i].type != INSTR_MOV_HM) ||
		    (instr[i].mov.dst.struct_id != h->struct_id) ||
		    (instr[i].mov.dst.offset != h->st->fields[i].offset / 8) ||
		    (instr[i].mov.dst.n_bits != h->st->fields[i].n_bits) ||
		    instr[i].mov.src.struct_id ||
		    (instr[i].mov.src.offset != a->st->fields[src_field_id + i].offset / 8) ||
		    (instr[i].mov.src.n_bits != a->st->fields[src_field_id + i].n_bits) ||
		    (instr[i].mov.dst.n_bits != instr[i].mov.src.n_bits))
			return 0;

	/* Last instruction: HDR_VALIDATE. */
	if ((instr[i].type != INSTR_HDR_VALIDATE) ||
	    (instr[i].valid.header_id != h->id))
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

	*n_pattern_instr = 1 + i;
	return 1;
}

static void
instr_pattern_mov_all_validate_replace(struct rte_swx_pipeline *p,
				       struct action *a,
				       struct instruction *instr,
				       struct instruction_data *data,
				       uint32_t n_instr)
{
	struct header *h;
	uint32_t src_field_id, src_offset, i;

	/* Read from the instructions before they are modified. */
	h = header_find_by_struct_id(p, instr[0].mov.dst.struct_id);
	if (!h)
		return;

	for (src_field_id = 0; src_field_id < a->st->n_fields; src_field_id++)
		if (instr[0].mov.src.offset == a->st->fields[src_field_id].offset / 8)
			break;

	if (src_field_id == a->st->n_fields)
		return;

	src_offset = instr[0].mov.src.offset;

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
instr_pattern_mov_all_validate_optimize(struct rte_swx_pipeline *p,
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

		/* Mov all + validate. */
		detected = instr_pattern_mov_all_validate_search(p,
								 a,
								 instr,
								 data,
								 n_instructions - i,
								 instructions,
								 instruction_data,
								 n_instructions,
								 &n_instr);
		if (detected) {
			instr_pattern_mov_all_validate_replace(p, a, instr, data, n_instr);
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

	/* Mov all + validate. */
	n_instructions = instr_pattern_mov_all_validate_optimize(p,
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
		a->n_instructions = n_instructions;
	} else {
		p->instructions = instr;
		p->n_instructions = n_instructions;
	}

	free(data);
	return 0;

error:
	free(data);
	free(instr);
	return err;
}

typedef void (*instr_exec_t)(struct rte_swx_pipeline *);

static instr_exec_t instruction_table[] = {
	[INSTR_RX] = instr_rx_exec,
	[INSTR_TX] = instr_tx_exec,
	[INSTR_TX_I] = instr_tx_i_exec,

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
	[INSTR_SELECTOR] = instr_selector_exec,
	[INSTR_LEARNER] = instr_learner_exec,
	[INSTR_LEARNER_LEARN] = instr_learn_exec,
	[INSTR_LEARNER_FORGET] = instr_forget_exec,
	[INSTR_EXTERN_OBJ] = instr_extern_obj_exec,
	[INSTR_EXTERN_FUNC] = instr_extern_func_exec,

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

static inline void
instr_exec(struct rte_swx_pipeline *p)
{
	struct thread *t = &p->threads[p->thread_id];
	struct instruction *ip = t->ip;
	instr_exec_t instr = instruction_table[ip->type];

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
	struct action *a;
	int err;

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
	CHECK(a, ENOMEM);
	if (args_struct_type) {
		a->args_endianness = calloc(args_struct_type->n_fields, sizeof(int));
		if (!a->args_endianness) {
			free(a);
			CHECK(0, ENOMEM);
		}
	}

	/* Node initialization. */
	strcpy(a->name, name);
	a->st = args_struct_type;
	a->id = p->n_actions;

	/* Instruction translation. */
	err = instruction_config(p, a, instructions, n_instructions);
	if (err) {
		free(a->args_endianness);
		free(a);
		return err;
	}

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->actions, a, node);
	p->n_actions++;

	return 0;
}

static int
action_build(struct rte_swx_pipeline *p)
{
	struct action *action;

	p->action_instructions = calloc(p->n_actions,
					sizeof(struct instruction *));
	CHECK(p->action_instructions, ENOMEM);

	TAILQ_FOREACH(action, &p->actions, node)
		p->action_instructions[action->id] = action->instructions;

	return 0;
}

static void
action_build_free(struct rte_swx_pipeline *p)
{
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
		free(action->instructions);
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

	*match_type = (n_fields_em == n_fields) ?
		       RTE_SWX_TABLE_MATCH_EXACT :
		       RTE_SWX_TABLE_MATCH_WILDCARD;

	return 0;
}

static int
table_match_fields_check(struct rte_swx_pipeline *p,
			 struct rte_swx_pipeline_table_params *params,
			 struct header **header)
{
	struct header *h0 = NULL;
	struct field *hf, *mf;
	uint32_t *offset = NULL, i;
	int status = 0;

	/* Return if no match fields. */
	if (!params->n_fields) {
		if (params->fields) {
			status = -EINVAL;
			goto end;
		}

		if (header)
			*header = NULL;

		return 0;
	}

	/* Memory allocation. */
	offset = calloc(params->n_fields, sizeof(uint32_t));
	if (!offset) {
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

	for (i = 1; i < params->n_fields; i++)
		if (h0) {
			struct header *h;

			hf = header_field_parse(p, params->fields[i].name, &h);
			if (!hf || (h->id != h0->id) || hf->var_size) {
				status = -EINVAL;
				goto end;
			}

			offset[i] = hf->offset;
		} else {
			mf = metadata_field_parse(p, params->fields[i].name);
			if (!mf) {
				status = -EINVAL;
				goto end;
			}

			offset[i] = mf->offset;
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

	/* Return. */
	if (header)
		*header = h0;

end:
	free(offset);
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
	struct table_type *type;
	struct table *t;
	struct action *default_action;
	struct header *header = NULL;
	uint32_t action_data_size_max = 0, i;
	int status = 0;

	CHECK(p, EINVAL);

	CHECK_NAME(name, EINVAL);
	CHECK(!table_find(p, name), EEXIST);
	CHECK(!selector_find(p, name), EEXIST);
	CHECK(!learner_find(p, name), EEXIST);

	CHECK(params, EINVAL);

	/* Match checks. */
	status = table_match_fields_check(p, params, &header);
	if (status)
		return status;

	/* Action checks. */
	CHECK(params->n_actions, EINVAL);
	CHECK(params->action_names, EINVAL);
	for (i = 0; i < params->n_actions; i++) {
		const char *action_name = params->action_names[i];
		struct action *a;
		uint32_t action_data_size;

		CHECK_NAME(action_name, EINVAL);

		a = action_find(p, action_name);
		CHECK(a, EINVAL);
		CHECK(!action_does_learning(a), EINVAL);

		action_data_size = a->st ? a->st->n_bits / 8 : 0;
		if (action_data_size > action_data_size_max)
			action_data_size_max = action_data_size;
	}

	CHECK_NAME(params->default_action_name, EINVAL);
	for (i = 0; i < p->n_actions; i++)
		if (!strcmp(params->action_names[i],
			    params->default_action_name))
			break;
	CHECK(i < params->n_actions, EINVAL);
	default_action = action_find(p, params->default_action_name);
	CHECK((default_action->st && params->default_action_data) ||
	      !params->default_action_data, EINVAL);

	/* Table type checks. */
	if (recommended_table_type_name)
		CHECK_NAME(recommended_table_type_name, EINVAL);

	if (params->n_fields) {
		enum rte_swx_table_match_type match_type;

		status = table_match_type_resolve(params->fields, params->n_fields, &match_type);
		if (status)
			return status;

		type = table_type_resolve(p, recommended_table_type_name, match_type);
		CHECK(type, EINVAL);
	} else {
		type = NULL;
	}

	/* Memory allocation. */
	t = calloc(1, sizeof(struct table));
	CHECK(t, ENOMEM);

	t->fields = calloc(params->n_fields, sizeof(struct match_field));
	if (!t->fields) {
		free(t);
		CHECK(0, ENOMEM);
	}

	t->actions = calloc(params->n_actions, sizeof(struct action *));
	if (!t->actions) {
		free(t->fields);
		free(t);
		CHECK(0, ENOMEM);
	}

	if (action_data_size_max) {
		t->default_action_data = calloc(1, action_data_size_max);
		if (!t->default_action_data) {
			free(t->actions);
			free(t->fields);
			free(t);
			CHECK(0, ENOMEM);
		}
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

	for (i = 0; i < params->n_actions; i++)
		t->actions[i] = action_find(p, params->action_names[i]);
	t->default_action = default_action;
	if (default_action->st)
		memcpy(t->default_action_data,
		       params->default_action_data,
		       default_action->st->n_bits / 8);
	t->n_actions = params->n_actions;
	t->default_action_is_const = params->default_action_is_const;
	t->action_data_size_max = action_data_size_max;

	t->size = size;
	t->id = p->n_tables;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->tables, t, node);
	p->n_tables++;

	return 0;
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
	uint32_t i;

	/* Return if no match fields. */
	if (!params->n_fields || !params->field_names)
		return -EINVAL;

	/* Check that all the match fields either belong to the same header
	 * or are all meta-data fields.
	 */
	hf = header_field_parse(p, params->field_names[0], &h0);
	mf = metadata_field_parse(p, params->field_names[0]);
	if (!hf && !mf)
		return -EINVAL;

	for (i = 1; i < params->n_fields; i++)
		if (h0) {
			struct header *h;

			hf = header_field_parse(p, params->field_names[i], &h);
			if (!hf || (h->id != h0->id))
				return -EINVAL;
		} else {
			mf = metadata_field_parse(p, params->field_names[i]);
			if (!mf)
				return -EINVAL;
		}

	/* Check that there are no duplicated match fields. */
	for (i = 0; i < params->n_fields; i++) {
		const char *field_name = params->field_names[i];
		uint32_t j;

		for (j = i + 1; j < params->n_fields; j++)
			if (!strcmp(params->field_names[j], field_name))
				return -EINVAL;
	}

	/* Return. */
	if (header)
		*header = h0;

	return 0;
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
			      uint32_t timeout)
{
	struct learner *l = NULL;
	struct action *default_action;
	struct header *header = NULL;
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
		const char *action_field_name = params->action_field_names[i];
		struct action *a;
		uint32_t action_data_size;

		CHECK_NAME(action_name, EINVAL);

		a = action_find(p, action_name);
		CHECK(a, EINVAL);

		status = learner_action_args_check(p, a, action_field_name);
		if (status)
			return status;

		status = learner_action_learning_check(p,
						       a,
						       params->action_names,
						       params->n_actions);
		if (status)
			return status;

		action_data_size = a->st ? a->st->n_bits / 8 : 0;
		if (action_data_size > action_data_size_max)
			action_data_size_max = action_data_size;
	}

	CHECK_NAME(params->default_action_name, EINVAL);
	for (i = 0; i < p->n_actions; i++)
		if (!strcmp(params->action_names[i],
			    params->default_action_name))
			break;
	CHECK(i < params->n_actions, EINVAL);

	default_action = action_find(p, params->default_action_name);
	CHECK((default_action->st && params->default_action_data) ||
	      !params->default_action_data, EINVAL);

	/* Any other checks. */
	CHECK(size, EINVAL);
	CHECK(timeout, EINVAL);

	/* Memory allocation. */
	l = calloc(1, sizeof(struct learner));
	if (!l)
		goto nomem;

	l->fields = calloc(params->n_fields, sizeof(struct field *));
	if (!l->fields)
		goto nomem;

	l->actions = calloc(params->n_actions, sizeof(struct action *));
	if (!l->actions)
		goto nomem;

	l->action_arg = calloc(params->n_actions, sizeof(struct field *));
	if (!l->action_arg)
		goto nomem;

	if (action_data_size_max) {
		l->default_action_data = calloc(1, action_data_size_max);
		if (!l->default_action_data)
			goto nomem;
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
		const char *mf_name = params->action_field_names[i];

		l->actions[i] = action_find(p, params->action_names[i]);

		l->action_arg[i] = mf_name ? metadata_field_parse(p, mf_name) : NULL;
	}

	l->default_action = default_action;

	if (default_action->st)
		memcpy(l->default_action_data,
		       params->default_action_data,
		       default_action->st->n_bits / 8);

	l->n_actions = params->n_actions;

	l->default_action_is_const = params->default_action_is_const;

	l->action_data_size_max = action_data_size_max;

	l->size = size;

	l->timeout = timeout;

	l->id = p->n_learners;

	/* Node add to tailq. */
	TAILQ_INSERT_TAIL(&p->learners, l, node);
	p->n_learners++;

	return 0;

nomem:
	if (!l)
		return -ENOMEM;

	free(l->action_arg);
	free(l->actions);
	free(l->fields);
	free(l);

	return -ENOMEM;
}

static void
learner_params_free(struct rte_swx_table_learner_params *params)
{
	if (!params)
		return;

	free(params->key_mask0);

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

	/* Maximum number of keys. */
	params->n_keys_max = l->size;

	/* Timeout. */
	params->key_timeout = l->timeout;

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
			free(r->action_data);
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
			uint32_t j;

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

			/* r->action_data. */
			r->action_data = calloc(p->n_actions, sizeof(uint8_t *));
			if (!r->action_data) {
				status = -ENOMEM;
				goto error;
			}

			for (j = 0; j < l->n_actions; j++) {
				struct action *a = l->actions[j];
				struct field *mf = l->action_arg[j];
				uint8_t *m = t->structs[p->metadata_struct_id];

				r->action_data[a->id] = mf ? &m[mf->offset / 8] : NULL;
			}
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
		free(l->action_arg);
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

	p->table_state = calloc(p->n_tables + p->n_selectors,
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
		if (ts->obj)
			rte_swx_table_selector_free(ts->obj);
	}

	for (i = 0; i < p->n_learners; i++) {
		struct rte_swx_table_state *ts = &p->table_state[p->n_tables + p->n_selectors + i];

		/* ts->obj. */
		if (ts->obj)
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
	TAILQ_INIT(&pipeline->extern_types);
	TAILQ_INIT(&pipeline->extern_objs);
	TAILQ_INIT(&pipeline->extern_funcs);
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
	pipeline->numa_node = numa_node;

	*p = pipeline;
	return 0;
}

void
rte_swx_pipeline_free(struct rte_swx_pipeline *p)
{
	if (!p)
		return;

	free(p->instructions);

	metarray_free(p);
	regarray_free(p);
	table_state_free(p);
	learner_free(p);
	selector_free(p);
	table_free(p);
	action_free(p);
	metadata_free(p);
	header_free(p);
	extern_func_free(p);
	extern_obj_free(p);
	port_out_free(p);
	port_in_free(p);
	struct_free(p);

	free(p);
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

	status = extern_obj_build(p);
	if (status)
		goto error;

	status = extern_func_build(p);
	if (status)
		goto error;

	status = header_build(p);
	if (status)
		goto error;

	status = metadata_build(p);
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
	metadata_build_free(p);
	header_build_free(p);
	extern_func_build_free(p);
	extern_obj_build_free(p);
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

	pipeline->n_ports_in = p->n_ports_in;
	pipeline->n_ports_out = p->n_ports_out;
	pipeline->n_actions = n_actions;
	pipeline->n_tables = n_tables;
	pipeline->n_selectors = p->n_selectors;
	pipeline->n_learners = p->n_learners;
	pipeline->n_regarrays = p->n_regarrays;
	pipeline->n_metarrays = p->n_metarrays;

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
