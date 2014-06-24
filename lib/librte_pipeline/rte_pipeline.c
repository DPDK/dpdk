/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "rte_pipeline.h"

#define RTE_TABLE_INVALID                                 UINT32_MAX

struct rte_port_in {
	/* Input parameters */
	struct rte_port_in_ops ops;
	rte_pipeline_port_in_action_handler f_action;
	void *arg_ah;
	uint32_t burst_size;

	/* The table to which this port is connected */
	uint32_t table_id;

	/* Handle to low-level port */
	void *h_port;

	/* List of enabled ports */
	struct rte_port_in *next;
};

struct rte_port_out {
	/* Input parameters */
	struct rte_port_out_ops ops;
	rte_pipeline_port_out_action_handler f_action;
	rte_pipeline_port_out_action_handler_bulk f_action_bulk;
	void *arg_ah;

	/* Handle to low-level port */
	void *h_port;
};

struct rte_table {
	/* Input parameters */
	struct rte_table_ops ops;
	rte_pipeline_table_action_handler_hit f_action_hit;
	rte_pipeline_table_action_handler_miss f_action_miss;
	void *arg_ah;
	struct rte_pipeline_table_entry *default_entry;
	uint32_t entry_size;

	uint32_t table_next_id;
	uint32_t table_next_id_valid;

	/* Handle to the low-level table object */
	void *h_table;
};

#define RTE_PIPELINE_MAX_NAME_SZ                           124

struct rte_pipeline {
	/* Input parameters */
	char name[RTE_PIPELINE_MAX_NAME_SZ];
	int socket_id;
	uint32_t offset_port_id;

	/* Internal tables */
	struct rte_port_in ports_in[RTE_PIPELINE_PORT_IN_MAX];
	struct rte_port_out ports_out[RTE_PIPELINE_PORT_OUT_MAX];
	struct rte_table tables[RTE_PIPELINE_TABLE_MAX];

	/* Occupancy of internal tables */
	uint32_t num_ports_in;
	uint32_t num_ports_out;
	uint32_t num_tables;

	/* List of enabled ports */
	uint64_t enabled_port_in_mask;
	struct rte_port_in *port_in_first;

	/* Pipeline run structures */
	struct rte_mbuf *pkts[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_pipeline_table_entry *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t action_mask0[RTE_PIPELINE_ACTIONS];
	uint64_t action_mask1[RTE_PIPELINE_ACTIONS];
} __rte_cache_aligned;

static inline uint32_t
rte_mask_get_next(uint64_t mask, uint32_t pos)
{
	uint64_t mask_rot = (mask << ((63 - pos) & 0x3F)) |
			(mask >> ((pos + 1) & 0x3F));
	return (__builtin_ctzll(mask_rot) - (63 - pos)) & 0x3F;
}

static inline uint32_t
rte_mask_get_prev(uint64_t mask, uint32_t pos)
{
	uint64_t mask_rot = (mask >> (pos & 0x3F)) |
			(mask << ((64 - pos) & 0x3F));
	return ((63 - __builtin_clzll(mask_rot)) + pos) & 0x3F;
}

static void
rte_pipeline_table_free(struct rte_table *table);

static void
rte_pipeline_port_in_free(struct rte_port_in *port);

static void
rte_pipeline_port_out_free(struct rte_port_out *port);

/*
 * Pipeline
 *
 */
static int
rte_pipeline_check_params(struct rte_pipeline_params *params)
{
	if (params == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Incorrect value for parameter params\n", __func__);
		return -EINVAL;
	}

	/* name */
	if (params->name == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Incorrect value for parameter name\n", __func__);
		return -EINVAL;
	}

	/* socket */
	if ((params->socket_id < 0) ||
	    (params->socket_id >= RTE_MAX_NUMA_NODES)) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Incorrect value for parameter socket_id\n",
			__func__);
		return -EINVAL;
	}

	/* offset_port_id */
	if (params->offset_port_id & 0x3) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Incorrect value for parameter offset_port_id\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

struct rte_pipeline *
rte_pipeline_create(struct rte_pipeline_params *params)
{
	struct rte_pipeline *p;
	int status;

	/* Check input parameters */
	status = rte_pipeline_check_params(params);
	if (status != 0) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Pipeline params check failed (%d)\n",
			__func__, status);
		return NULL;
	}

	/* Allocate memory for the pipeline on requested socket */
	p = rte_zmalloc_socket("PIPELINE", sizeof(struct rte_pipeline),
			CACHE_LINE_SIZE, params->socket_id);

	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Pipeline memory allocation failed\n", __func__);
		return NULL;
	}

	/* Save input parameters */
	snprintf(p->name, RTE_PIPELINE_MAX_NAME_SZ, "%s", params->name);
	p->socket_id = params->socket_id;
	p->offset_port_id = params->offset_port_id;

	/* Initialize pipeline internal data structure */
	p->num_ports_in = 0;
	p->num_ports_out = 0;
	p->num_tables = 0;
	p->enabled_port_in_mask = 0;
	p->port_in_first = NULL;

	return p;
}

int
rte_pipeline_free(struct rte_pipeline *p)
{
	uint32_t i;

	/* Check input parameters */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: rte_pipeline parameter is NULL\n", __func__);
		return -EINVAL;
	}

	/* Free input ports */
	for (i = 0; i < p->num_ports_in; i++) {
		struct rte_port_in *port = &p->ports_in[i];

		rte_pipeline_port_in_free(port);
	}

	/* Free tables */
	for (i = 0; i < p->num_tables; i++) {
		struct rte_table *table = &p->tables[i];

		rte_pipeline_table_free(table);
	}

	/* Free output ports */
	for (i = 0; i < p->num_ports_out; i++) {
		struct rte_port_out *port = &p->ports_out[i];

		rte_pipeline_port_out_free(port);
	}

	/* Free pipeline memory */
	rte_free(p);

	return 0;
}

/*
 * Table
 *
 */
static int
rte_table_check_params(struct rte_pipeline *p,
		struct rte_pipeline_table_params *params,
		uint32_t *table_id)
{
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (params == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: params parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (table_id == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: table_id parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	/* ops */
	if (params->ops == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: params->ops is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (params->ops->f_create == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_create function pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (params->ops->f_lookup == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_lookup function pointer is NULL\n", __func__);
		return -EINVAL;
	}

	/* De we have room for one more table? */
	if (p->num_tables == RTE_PIPELINE_TABLE_MAX) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Incorrect value for num_tables parameter\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

int
rte_pipeline_table_create(struct rte_pipeline *p,
		struct rte_pipeline_table_params *params,
		uint32_t *table_id)
{
	struct rte_table *table;
	struct rte_pipeline_table_entry *default_entry;
	void *h_table;
	uint32_t entry_size, id;
	int status;

	/* Check input arguments */
	status = rte_table_check_params(p, params, table_id);
	if (status != 0)
		return status;

	id = p->num_tables;
	table = &p->tables[id];

	/* Allocate space for the default table entry */
	entry_size = sizeof(struct rte_pipeline_table_entry) +
		params->action_data_size;
	default_entry = (struct rte_pipeline_table_entry *) rte_zmalloc_socket(
		"PIPELINE", entry_size, CACHE_LINE_SIZE, p->socket_id);
	if (default_entry == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Failed to allocate default entry\n", __func__);
		return -EINVAL;
	}

	/* Create the table */
	h_table = params->ops->f_create(params->arg_create, p->socket_id,
		entry_size);
	if (h_table == NULL) {
		rte_free(default_entry);
		RTE_LOG(ERR, PIPELINE, "%s: Table creation failed\n", __func__);
		return -EINVAL;
	}

	/* Commit current table to the pipeline */
	p->num_tables++;
	*table_id = id;

	/* Save input parameters */
	memcpy(&table->ops, params->ops, sizeof(struct rte_table_ops));
	table->f_action_hit = params->f_action_hit;
	table->f_action_miss = params->f_action_miss;
	table->arg_ah = params->arg_ah;
	table->entry_size = entry_size;

	/* Clear the lookup miss actions (to be set later through API) */
	table->default_entry = default_entry;
	table->default_entry->action = RTE_PIPELINE_ACTION_DROP;

	/* Initialize table internal data structure */
	table->h_table = h_table;
	table->table_next_id = 0;
	table->table_next_id_valid = 0;

	return 0;
}

void
rte_pipeline_table_free(struct rte_table *table)
{
	if (table->ops.f_free != NULL)
		table->ops.f_free(table->h_table);

	rte_free(table->default_entry);
}

int
rte_pipeline_table_default_entry_add(struct rte_pipeline *p,
	uint32_t table_id,
	struct rte_pipeline_table_entry *default_entry,
	struct rte_pipeline_table_entry **default_entry_ptr)
{
	struct rte_table *table;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (default_entry == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: default_entry parameter is NULL\n", __func__);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
			"%s: table_id %d out of range\n", __func__, table_id);
		return -EINVAL;
	}

	table = &p->tables[table_id];

	if ((default_entry->action == RTE_PIPELINE_ACTION_TABLE) &&
		table->table_next_id_valid &&
		(default_entry->table_id != table->table_next_id)) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Tree-like topologies not allowed\n", __func__);
		return -EINVAL;
	}

	/* Set the lookup miss actions */
	if ((default_entry->action == RTE_PIPELINE_ACTION_TABLE) &&
		(table->table_next_id_valid == 0)) {
		table->table_next_id = default_entry->table_id;
		table->table_next_id_valid = 1;
	}

	memcpy(table->default_entry, default_entry, table->entry_size);

	*default_entry_ptr = table->default_entry;
	return 0;
}

int
rte_pipeline_table_default_entry_delete(struct rte_pipeline *p,
		uint32_t table_id,
		struct rte_pipeline_table_entry *entry)
{
	struct rte_table *table;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: pipeline parameter is NULL\n", __func__);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
			"%s: table_id %d out of range\n", __func__, table_id);
		return -EINVAL;
	}

	table = &p->tables[table_id];

	/* Save the current contents of the default entry */
	if (entry)
		memcpy(entry, table->default_entry, table->entry_size);

	/* Clear the lookup miss actions */
	memset(table->default_entry, 0, table->entry_size);
	table->default_entry->action = RTE_PIPELINE_ACTION_DROP;

	return 0;
}

int
rte_pipeline_table_entry_add(struct rte_pipeline *p,
		uint32_t table_id,
		void *key,
		struct rte_pipeline_table_entry *entry,
		int *key_found,
		struct rte_pipeline_table_entry **entry_ptr)
{
	struct rte_table *table;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (key == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: key parameter is NULL\n", __func__);
		return -EINVAL;
	}

	if (entry == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: entry parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
			"%s: table_id %d out of range\n", __func__, table_id);
		return -EINVAL;
	}

	table = &p->tables[table_id];

	if (table->ops.f_add == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: f_add function pointer NULL\n",
			__func__);
		return -EINVAL;
	}

	if ((entry->action == RTE_PIPELINE_ACTION_TABLE) &&
		table->table_next_id_valid &&
		(entry->table_id != table->table_next_id)) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Tree-like topologies not allowed\n", __func__);
		return -EINVAL;
	}

	/* Add entry */
	if ((entry->action == RTE_PIPELINE_ACTION_TABLE) &&
		(table->table_next_id_valid == 0)) {
		table->table_next_id = entry->table_id;
		table->table_next_id_valid = 1;
	}

	return (table->ops.f_add)(table->h_table, key, (void *) entry,
		key_found, (void **) entry_ptr);
}

int
rte_pipeline_table_entry_delete(struct rte_pipeline *p,
		uint32_t table_id,
		void *key,
		int *key_found,
		struct rte_pipeline_table_entry *entry)
{
	struct rte_table *table;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (key == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: key parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
			"%s: table_id %d out of range\n", __func__, table_id);
		return -EINVAL;
	}

	table = &p->tables[table_id];

	if (table->ops.f_delete == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_delete function pointer NULL\n", __func__);
		return -EINVAL;
	}

	return (table->ops.f_delete)(table->h_table, key, key_found, entry);
}

/*
 * Port
 *
 */
static int
rte_pipeline_port_in_check_params(struct rte_pipeline *p,
		struct rte_pipeline_port_in_params *params,
		uint32_t *port_id)
{
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}
	if (params == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: params parameter NULL\n", __func__);
		return -EINVAL;
	}
	if (port_id == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: port_id parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	/* ops */
	if (params->ops == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: params->ops parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (params->ops->f_create == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_create function pointer NULL\n", __func__);
		return -EINVAL;
	}

	if (params->ops->f_rx == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: f_rx function pointer NULL\n",
			__func__);
		return -EINVAL;
	}

	/* burst_size */
	if ((params->burst_size == 0) ||
		(params->burst_size > RTE_PORT_IN_BURST_SIZE_MAX)) {
		RTE_LOG(ERR, PIPELINE, "%s: invalid value for burst_size\n",
			__func__);
		return -EINVAL;
	}

	/* Do we have room for one more port? */
	if (p->num_ports_in == RTE_PIPELINE_PORT_IN_MAX) {
		RTE_LOG(ERR, PIPELINE,
			"%s: invalid value for num_ports_in\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int
rte_pipeline_port_out_check_params(struct rte_pipeline *p,
		struct rte_pipeline_port_out_params *params,
		uint32_t *port_id)
{
	rte_pipeline_port_out_action_handler f_ah;
	rte_pipeline_port_out_action_handler_bulk f_ah_bulk;

	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (params == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: params parameter NULL\n", __func__);
		return -EINVAL;
	}

	if (port_id == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: port_id parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	/* ops */
	if (params->ops == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: params->ops parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (params->ops->f_create == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_create function pointer NULL\n", __func__);
		return -EINVAL;
	}

	if (params->ops->f_tx == NULL) {
		RTE_LOG(ERR, PIPELINE,
				"%s: f_tx function pointer NULL\n", __func__);
		return -EINVAL;
	}

	if (params->ops->f_tx_bulk == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_tx_bulk function pointer NULL\n", __func__);
		return -EINVAL;
	}

	f_ah = params->f_action;
	f_ah_bulk = params->f_action_bulk;
	if (((f_ah != NULL) && (f_ah_bulk == NULL)) ||
	    ((f_ah == NULL) && (f_ah_bulk != NULL))) {
		RTE_LOG(ERR, PIPELINE, "%s: Action handlers have to be either"
			"both enabled or both disabled\n", __func__);
		return -EINVAL;
	}

	/* Do we have room for one more port? */
	if (p->num_ports_out == RTE_PIPELINE_PORT_OUT_MAX) {
		RTE_LOG(ERR, PIPELINE,
			"%s: invalid value for num_ports_out\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int
rte_pipeline_port_in_create(struct rte_pipeline *p,
		struct rte_pipeline_port_in_params *params,
		uint32_t *port_id)
{
	struct rte_port_in *port;
	void *h_port;
	uint32_t id;
	int status;

	/* Check input arguments */
	status = rte_pipeline_port_in_check_params(p, params, port_id);
	if (status != 0)
		return status;

	id = p->num_ports_in;
	port = &p->ports_in[id];

	/* Create the port */
	h_port = params->ops->f_create(params->arg_create, p->socket_id);
	if (h_port == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: Port creation failed\n", __func__);
		return -EINVAL;
	}

	/* Commit current table to the pipeline */
	p->num_ports_in++;
	*port_id = id;

	/* Save input parameters */
	memcpy(&port->ops, params->ops, sizeof(struct rte_port_in_ops));
	port->f_action = params->f_action;
	port->arg_ah = params->arg_ah;
	port->burst_size = params->burst_size;

	/* Initialize port internal data structure */
	port->table_id = RTE_TABLE_INVALID;
	port->h_port = h_port;
	port->next = NULL;

	return 0;
}

void
rte_pipeline_port_in_free(struct rte_port_in *port)
{
	if (port->ops.f_free != NULL)
		port->ops.f_free(port->h_port);
}

int
rte_pipeline_port_out_create(struct rte_pipeline *p,
		struct rte_pipeline_port_out_params *params,
		uint32_t *port_id)
{
	struct rte_port_out *port;
	void *h_port;
	uint32_t id;
	int status;

	/* Check input arguments */
	status = rte_pipeline_port_out_check_params(p, params, port_id);
	if (status != 0)
		return status;

	id = p->num_ports_out;
	port = &p->ports_out[id];

	/* Create the port */
	h_port = params->ops->f_create(params->arg_create, p->socket_id);
	if (h_port == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: Port creation failed\n", __func__);
		return -EINVAL;
	}

	/* Commit current table to the pipeline */
	p->num_ports_out++;
	*port_id = id;

	/* Save input parameters */
	memcpy(&port->ops, params->ops, sizeof(struct rte_port_out_ops));
	port->f_action = params->f_action;
	port->f_action_bulk = params->f_action_bulk;
	port->arg_ah = params->arg_ah;

	/* Initialize port internal data structure */
	port->h_port = h_port;

	return 0;
}

void
rte_pipeline_port_out_free(struct rte_port_out *port)
{
	if (port->ops.f_free != NULL)
		port->ops.f_free(port->h_port);
}

int
rte_pipeline_port_in_connect_to_table(struct rte_pipeline *p,
		uint32_t port_id,
		uint32_t table_id)
{
	struct rte_port_in *port;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (port_id >= p->num_ports_in) {
		RTE_LOG(ERR, PIPELINE,
			"%s: port IN ID %u is out of range\n",
			__func__, port_id);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Table ID %u is out of range\n",
			__func__, table_id);
		return -EINVAL;
	}

	port = &p->ports_in[port_id];
	port->table_id = table_id;

	return 0;
}

int
rte_pipeline_port_in_enable(struct rte_pipeline *p, uint32_t port_id)
{
	struct rte_port_in *port, *port_prev, *port_next;
	struct rte_port_in *port_first, *port_last;
	uint64_t port_mask;
	uint32_t port_prev_id, port_next_id, port_first_id, port_last_id;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (port_id >= p->num_ports_in) {
		RTE_LOG(ERR, PIPELINE,
			"%s: port IN ID %u is out of range\n",
			__func__, port_id);
		return -EINVAL;
	}

	/* Return if current input port is already enabled */
	port_mask = 1LLU << port_id;
	if (p->enabled_port_in_mask & port_mask)
		return 0;

	p->enabled_port_in_mask |= port_mask;

	/* Add current input port to the pipeline chain of enabled ports */
	port_prev_id = rte_mask_get_prev(p->enabled_port_in_mask, port_id);
	port_next_id = rte_mask_get_next(p->enabled_port_in_mask, port_id);

	port_prev = &p->ports_in[port_prev_id];
	port_next = &p->ports_in[port_next_id];
	port = &p->ports_in[port_id];

	port_prev->next = port;
	port->next = port_next;

	/* Update the first and last input ports in the chain */
	port_first_id = __builtin_ctzll(p->enabled_port_in_mask);
	port_last_id = 63 - __builtin_clzll(p->enabled_port_in_mask);

	port_first = &p->ports_in[port_first_id];
	port_last = &p->ports_in[port_last_id];

	p->port_in_first = port_first;
	port_last->next = NULL;

	return 0;
}

int
rte_pipeline_port_in_disable(struct rte_pipeline *p, uint32_t port_id)
{
	struct rte_port_in *port_prev, *port_next, *port_first, *port_last;
	uint64_t port_mask;
	uint32_t port_prev_id, port_next_id, port_first_id, port_last_id;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
		__func__);
		return -EINVAL;
	}

	if (port_id >= p->num_ports_in) {
		RTE_LOG(ERR, PIPELINE, "%s: port IN ID %u is out of range\n",
			__func__, port_id);
		return -EINVAL;
	}

	/* Return if current input port is already disabled */
	port_mask = 1LLU << port_id;
	if ((p->enabled_port_in_mask & port_mask) == 0)
		return 0;

	/* Return if no other enabled ports */
	if (__builtin_popcountll(p->enabled_port_in_mask) == 1) {
		p->enabled_port_in_mask &= ~port_mask;
		p->port_in_first = NULL;

		return 0;
	}

	/* Add current input port to the pipeline chain of enabled ports */
	port_prev_id = rte_mask_get_prev(p->enabled_port_in_mask, port_id);
	port_next_id = rte_mask_get_next(p->enabled_port_in_mask, port_id);

	port_prev = &p->ports_in[port_prev_id];
	port_next = &p->ports_in[port_next_id];

	port_prev->next = port_next;
	p->enabled_port_in_mask &= ~port_mask;

	/* Update the first and last input ports in the chain */
	port_first_id = __builtin_ctzll(p->enabled_port_in_mask);
	port_last_id = 63 - __builtin_clzll(p->enabled_port_in_mask);

	port_first = &p->ports_in[port_first_id];
	port_last = &p->ports_in[port_last_id];

	p->port_in_first = port_first;
	port_last->next = NULL;

	return 0;
}

/*
 * Pipeline run-time
 *
 */
int
rte_pipeline_check(struct rte_pipeline *p)
{
	uint32_t port_in_id;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	/* Check that pipeline has at least one input port, one table and one
	output port */
	if (p->num_ports_in == 0) {
		RTE_LOG(ERR, PIPELINE, "%s: must have at least 1 input port\n",
			__func__);
		return -EINVAL;
	}
	if (p->num_tables == 0) {
		RTE_LOG(ERR, PIPELINE, "%s: must have at least 1 table\n",
			__func__);
		return -EINVAL;
	}
	if (p->num_ports_out == 0) {
		RTE_LOG(ERR, PIPELINE, "%s: must have at least 1 output port\n",
			__func__);
		return -EINVAL;
	}

	/* Check that all input ports are connected */
	for (port_in_id = 0; port_in_id < p->num_ports_in; port_in_id++) {
		struct rte_port_in *port_in = &p->ports_in[port_in_id];

		if (port_in->table_id == RTE_TABLE_INVALID) {
			RTE_LOG(ERR, PIPELINE,
				"%s: Port IN ID %u is not connected\n",
				__func__, port_in_id);
			return -EINVAL;
		}
	}

	return 0;
}

static inline void
rte_pipeline_compute_masks(struct rte_pipeline *p, uint64_t pkts_mask)
{
	p->action_mask1[RTE_PIPELINE_ACTION_DROP] = 0;
	p->action_mask1[RTE_PIPELINE_ACTION_PORT] = 0;
	p->action_mask1[RTE_PIPELINE_ACTION_TABLE] = 0;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			uint64_t pkt_mask = 1LLU << i;
			uint32_t pos = p->entries[i]->action;

			p->action_mask1[pos] |= pkt_mask;
		}
	} else {
		uint32_t i;

		for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++) {
			uint64_t pkt_mask = 1LLU << i;
			uint32_t pos;

			if ((pkt_mask & pkts_mask) == 0)
				continue;

			pos = p->entries[i]->action;
			p->action_mask1[pos] |= pkt_mask;
		}
	}
}

static inline void
rte_pipeline_action_handler_port_bulk(struct rte_pipeline *p,
		uint64_t pkts_mask, uint32_t port_id)
{
	struct rte_port_out *port_out = &p->ports_out[port_id];

	/* Output port user actions */
	if (port_out->f_action_bulk != NULL) {
		uint64_t mask = pkts_mask;

		port_out->f_action_bulk(p->pkts, &pkts_mask, port_out->arg_ah);
		p->action_mask0[RTE_PIPELINE_ACTION_DROP] |= pkts_mask ^  mask;
	}

	/* Output port TX */
	if (pkts_mask != 0)
		port_out->ops.f_tx_bulk(port_out->h_port, p->pkts, pkts_mask);
}

static inline void
rte_pipeline_action_handler_port(struct rte_pipeline *p, uint64_t pkts_mask)
{
	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = p->pkts[i];
			uint32_t port_out_id = p->entries[i]->port_id;
			struct rte_port_out *port_out =
				&p->ports_out[port_out_id];

			/* Output port user actions */
			if (port_out->f_action == NULL) /* Output port TX */
				port_out->ops.f_tx(port_out->h_port, pkt);
			else {
				uint64_t pkt_mask = 1LLU;

				port_out->f_action(pkt, &pkt_mask,
					port_out->arg_ah);
				p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
					(pkt_mask ^ 1LLU) << i;

				/* Output port TX */
				if (pkt_mask != 0)
					port_out->ops.f_tx(port_out->h_port,
						pkt);
			}
		}
	} else {
		uint32_t i;

		for (i = 0;  i < RTE_PORT_IN_BURST_SIZE_MAX; i++) {
			uint64_t pkt_mask = 1LLU << i;
			struct rte_mbuf *pkt;
			struct rte_port_out *port_out;
			uint32_t port_out_id;

			if ((pkt_mask & pkts_mask) == 0)
				continue;

			pkt = p->pkts[i];
			port_out_id = p->entries[i]->port_id;
			port_out = &p->ports_out[port_out_id];

			/* Output port user actions */
			if (port_out->f_action == NULL) /* Output port TX */
				port_out->ops.f_tx(port_out->h_port, pkt);
			else {
				pkt_mask = 1LLU;

				port_out->f_action(pkt, &pkt_mask,
					port_out->arg_ah);
				p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
					(pkt_mask ^ 1LLU) << i;

				/* Output port TX */
				if (pkt_mask != 0)
					port_out->ops.f_tx(port_out->h_port,
						pkt);
			}
		}
	}
}

static inline void
rte_pipeline_action_handler_port_meta(struct rte_pipeline *p,
	uint64_t pkts_mask)
{
	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = p->pkts[i];
			uint32_t port_out_id =
				RTE_MBUF_METADATA_UINT32(pkt,
					p->offset_port_id);
			struct rte_port_out *port_out = &p->ports_out[
				port_out_id];

			/* Output port user actions */
			if (port_out->f_action == NULL) /* Output port TX */
				port_out->ops.f_tx(port_out->h_port, pkt);
			else {
				uint64_t pkt_mask = 1LLU;

				port_out->f_action(pkt, &pkt_mask,
					port_out->arg_ah);
				p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
					(pkt_mask ^ 1LLU) << i;

				/* Output port TX */
				if (pkt_mask != 0)
					port_out->ops.f_tx(port_out->h_port,
						pkt);
			}
		}
	} else {
		uint32_t i;

		for (i = 0;  i < RTE_PORT_IN_BURST_SIZE_MAX; i++) {
			uint64_t pkt_mask = 1LLU << i;
			struct rte_mbuf *pkt;
			struct rte_port_out *port_out;
			uint32_t port_out_id;

			if ((pkt_mask & pkts_mask) == 0)
				continue;

			pkt = p->pkts[i];
			port_out_id = RTE_MBUF_METADATA_UINT32(pkt,
				p->offset_port_id);
			port_out = &p->ports_out[port_out_id];

			/* Output port user actions */
			if (port_out->f_action == NULL) /* Output port TX */
				port_out->ops.f_tx(port_out->h_port, pkt);
			else {
				pkt_mask = 1LLU;

				port_out->f_action(pkt, &pkt_mask,
					port_out->arg_ah);
				p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
					(pkt_mask ^ 1LLU) << i;

				/* Output port TX */
				if (pkt_mask != 0)
					port_out->ops.f_tx(port_out->h_port,
						pkt);
			}
		}
	}
}

static inline void
rte_pipeline_action_handler_drop(struct rte_pipeline *p, uint64_t pkts_mask)
{
	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++)
			rte_pktmbuf_free(p->pkts[i]);
	} else {
		uint32_t i;

		for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++) {
			uint64_t pkt_mask = 1LLU << i;

			if ((pkt_mask & pkts_mask) == 0)
				continue;

			rte_pktmbuf_free(p->pkts[i]);
		}
	}
}

int
rte_pipeline_run(struct rte_pipeline *p)
{
	struct rte_port_in *port_in;

	for (port_in = p->port_in_first; port_in != NULL;
		port_in = port_in->next) {
		uint64_t pkts_mask;
		uint32_t n_pkts, table_id;

		/* Input port RX */
		n_pkts = port_in->ops.f_rx(port_in->h_port, p->pkts,
			port_in->burst_size);
		if (n_pkts == 0)
			continue;

		pkts_mask = RTE_LEN2MASK(n_pkts, uint64_t);
		p->action_mask0[RTE_PIPELINE_ACTION_DROP] = 0;
		p->action_mask0[RTE_PIPELINE_ACTION_PORT] = 0;
		p->action_mask0[RTE_PIPELINE_ACTION_TABLE] = 0;

		/* Input port user actions */
		if (port_in->f_action != NULL) {
			uint64_t mask = pkts_mask;

			port_in->f_action(p->pkts, n_pkts, &pkts_mask,
				port_in->arg_ah);
			p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
				pkts_mask ^ mask;
		}

		/* Table */
		for (table_id = port_in->table_id; pkts_mask != 0; ) {
			struct rte_table *table;
			uint64_t lookup_hit_mask, lookup_miss_mask;

			/* Lookup */
			table = &p->tables[table_id];
			table->ops.f_lookup(table->h_table, p->pkts, pkts_mask,
					&lookup_hit_mask, (void **) p->entries);
			lookup_miss_mask = pkts_mask & (~lookup_hit_mask);

			/* Lookup miss */
			if (lookup_miss_mask != 0) {
				struct rte_pipeline_table_entry *default_entry =
					table->default_entry;

				/* Table user actions */
				if (table->f_action_miss != NULL) {
					uint64_t mask = lookup_miss_mask;

					table->f_action_miss(p->pkts,
						&lookup_miss_mask,
						default_entry, table->arg_ah);
					p->action_mask0[
						RTE_PIPELINE_ACTION_DROP] |=
						lookup_miss_mask ^ mask;
				}

				/* Table reserved actions */
				if ((default_entry->action ==
					RTE_PIPELINE_ACTION_PORT) &&
					(lookup_miss_mask != 0))
					rte_pipeline_action_handler_port_bulk(p,
						lookup_miss_mask,
						default_entry->port_id);
				else {
					uint32_t pos = default_entry->action;

					p->action_mask0[pos] = lookup_miss_mask;
				}
			}

			/* Lookup hit */
			if (lookup_hit_mask != 0) {
				/* Table user actions */
				if (table->f_action_hit != NULL) {
					uint64_t mask = lookup_hit_mask;

					table->f_action_hit(p->pkts,
						&lookup_hit_mask,
						p->entries, table->arg_ah);
					p->action_mask0[
						RTE_PIPELINE_ACTION_DROP] |=
						lookup_hit_mask ^ mask;
				}

				/* Table reserved actions */
				rte_pipeline_compute_masks(p, lookup_hit_mask);
				p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
					p->action_mask1[
						RTE_PIPELINE_ACTION_DROP];
				p->action_mask0[RTE_PIPELINE_ACTION_PORT] |=
					p->action_mask1[
						RTE_PIPELINE_ACTION_PORT];
				p->action_mask0[RTE_PIPELINE_ACTION_TABLE] |=
					p->action_mask1[
						RTE_PIPELINE_ACTION_TABLE];
			}

			/* Prepare for next iteration */
			pkts_mask = p->action_mask0[RTE_PIPELINE_ACTION_TABLE];
			table_id = table->table_next_id;
			p->action_mask0[RTE_PIPELINE_ACTION_TABLE] = 0;
		}

		/* Table reserved action PORT */
		rte_pipeline_action_handler_port(p,
				p->action_mask0[RTE_PIPELINE_ACTION_PORT]);

		/* Table reserved action PORT META */
		rte_pipeline_action_handler_port_meta(p,
				p->action_mask0[RTE_PIPELINE_ACTION_PORT_META]);

		/* Table reserved action DROP */
		rte_pipeline_action_handler_drop(p,
				p->action_mask0[RTE_PIPELINE_ACTION_DROP]);
	}

	return 0;
}

int
rte_pipeline_flush(struct rte_pipeline *p)
{
	uint32_t port_id;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	for (port_id = 0; port_id < p->num_ports_out; port_id++) {
		struct rte_port_out *port = &p->ports_out[port_id];

		if (port->ops.f_flush != NULL)
			port->ops.f_flush(port->h_port);
	}

	return 0;
}

int
rte_pipeline_port_out_packet_insert(struct rte_pipeline *p,
		uint32_t port_id, struct rte_mbuf *pkt)
{
	struct rte_port_out *port_out = &p->ports_out[port_id];

	/* Output port user actions */
	if (port_out->f_action == NULL)
		port_out->ops.f_tx(port_out->h_port, pkt); /* Output port TX */
	else {
		uint64_t pkt_mask = 1LLU;

		port_out->f_action(pkt, &pkt_mask, port_out->arg_ah);

		if (pkt_mask != 0) /* Output port TX */
			port_out->ops.f_tx(port_out->h_port, pkt);
		else
			rte_pktmbuf_free(pkt);
	}

	return 0;
}
