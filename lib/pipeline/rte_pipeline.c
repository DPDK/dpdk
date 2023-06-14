/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "rte_pipeline.h"

#define RTE_TABLE_INVALID                                 UINT32_MAX

#ifdef RTE_PIPELINE_STATS_COLLECT

#define RTE_PIPELINE_STATS_AH_DROP_WRITE(p, mask)			\
	({ (p)->n_pkts_ah_drop = __builtin_popcountll(mask); })

#define RTE_PIPELINE_STATS_AH_DROP_READ(p, counter)			\
	({ (counter) += (p)->n_pkts_ah_drop; (p)->n_pkts_ah_drop = 0; })

#define RTE_PIPELINE_STATS_TABLE_DROP0(p)				\
	({ (p)->pkts_drop_mask = (p)->action_mask0[RTE_PIPELINE_ACTION_DROP]; })

#define RTE_PIPELINE_STATS_TABLE_DROP1(p, counter)			\
({									\
	uint64_t mask = (p)->action_mask0[RTE_PIPELINE_ACTION_DROP];	\
	mask ^= (p)->pkts_drop_mask;					\
	(counter) += __builtin_popcountll(mask);			\
})

#else

#define RTE_PIPELINE_STATS_AH_DROP_WRITE(p, mask)
#define RTE_PIPELINE_STATS_AH_DROP_READ(p, counter)
#define RTE_PIPELINE_STATS_TABLE_DROP0(p)
#define RTE_PIPELINE_STATS_TABLE_DROP1(p, counter)

#endif

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

	/* Statistics */
	uint64_t n_pkts_dropped_by_ah;
};

struct rte_port_out {
	/* Input parameters */
	struct rte_port_out_ops ops;
	rte_pipeline_port_out_action_handler f_action;
	void *arg_ah;

	/* Handle to low-level port */
	void *h_port;

	/* Statistics */
	uint64_t n_pkts_dropped_by_ah;
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

	/* Statistics */
	uint64_t n_pkts_dropped_by_lkp_hit_ah;
	uint64_t n_pkts_dropped_by_lkp_miss_ah;
	uint64_t n_pkts_dropped_lkp_hit;
	uint64_t n_pkts_dropped_lkp_miss;
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
	struct rte_port_in *port_in_next;

	/* Pipeline run structures */
	struct rte_mbuf *pkts[RTE_PORT_IN_BURST_SIZE_MAX];
	struct rte_pipeline_table_entry *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t action_mask0[RTE_PIPELINE_ACTIONS];
	uint64_t action_mask1[RTE_PIPELINE_ACTIONS];
	uint64_t pkts_mask;
	uint64_t n_pkts_ah_drop;
	uint64_t pkts_drop_mask;
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
	if (params->socket_id < 0) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Incorrect value for parameter socket_id\n",
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
			RTE_CACHE_LINE_SIZE, params->socket_id);

	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: Pipeline memory allocation failed\n", __func__);
		return NULL;
	}

	/* Save input parameters */
	strlcpy(p->name, params->name, RTE_PIPELINE_MAX_NAME_SZ);
	p->socket_id = params->socket_id;
	p->offset_port_id = params->offset_port_id;

	/* Initialize pipeline internal data structure */
	p->num_ports_in = 0;
	p->num_ports_out = 0;
	p->num_tables = 0;
	p->enabled_port_in_mask = 0;
	p->port_in_next = NULL;
	p->pkts_mask = 0;
	p->n_pkts_ah_drop = 0;

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
	default_entry = rte_zmalloc_socket(
		"PIPELINE", entry_size, RTE_CACHE_LINE_SIZE, p->socket_id);
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

int rte_pipeline_table_entry_add_bulk(struct rte_pipeline *p,
	uint32_t table_id,
	void **keys,
	struct rte_pipeline_table_entry **entries,
	uint32_t n_keys,
	int *key_found,
	struct rte_pipeline_table_entry **entries_ptr)
{
	struct rte_table *table;
	uint32_t i;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (keys == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: keys parameter is NULL\n", __func__);
		return -EINVAL;
	}

	if (entries == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: entries parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
			"%s: table_id %d out of range\n", __func__, table_id);
		return -EINVAL;
	}

	table = &p->tables[table_id];

	if (table->ops.f_add_bulk == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: f_add_bulk function pointer NULL\n",
			__func__);
		return -EINVAL;
	}

	for (i = 0; i < n_keys; i++) {
		if ((entries[i]->action == RTE_PIPELINE_ACTION_TABLE) &&
			table->table_next_id_valid &&
			(entries[i]->table_id != table->table_next_id)) {
			RTE_LOG(ERR, PIPELINE,
				"%s: Tree-like topologies not allowed\n", __func__);
			return -EINVAL;
		}
	}

	/* Add entry */
	for (i = 0; i < n_keys; i++) {
		if ((entries[i]->action == RTE_PIPELINE_ACTION_TABLE) &&
			(table->table_next_id_valid == 0)) {
			table->table_next_id = entries[i]->table_id;
			table->table_next_id_valid = 1;
		}
	}

	return (table->ops.f_add_bulk)(table->h_table, keys, (void **) entries,
		n_keys, key_found, (void **) entries_ptr);
}

int rte_pipeline_table_entry_delete_bulk(struct rte_pipeline *p,
	uint32_t table_id,
	void **keys,
	uint32_t n_keys,
	int *key_found,
	struct rte_pipeline_table_entry **entries)
{
	struct rte_table *table;

	/* Check input arguments */
	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (keys == NULL) {
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

	if (table->ops.f_delete_bulk == NULL) {
		RTE_LOG(ERR, PIPELINE,
			"%s: f_delete function pointer NULL\n", __func__);
		return -EINVAL;
	}

	return (table->ops.f_delete_bulk)(table->h_table, keys, n_keys, key_found,
			(void **) entries);
}

/*
 * Port
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
	uint64_t port_mask;
	uint32_t port_prev_id, port_next_id;

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

	port = &p->ports_in[port_id];

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

	port_prev->next = port;
	port->next = port_next;

	/* Check if list of enabled ports was previously empty */
	if (p->enabled_port_in_mask == port_mask)
		p->port_in_next = port;

	return 0;
}

int
rte_pipeline_port_in_disable(struct rte_pipeline *p, uint32_t port_id)
{
	struct rte_port_in *port, *port_prev, *port_next;
	uint64_t port_mask;
	uint32_t port_prev_id, port_next_id;

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

	port = &p->ports_in[port_id];

	/* Return if current input port is already disabled */
	port_mask = 1LLU << port_id;
	if ((p->enabled_port_in_mask & port_mask) == 0)
		return 0;

	p->enabled_port_in_mask &= ~port_mask;

	/* Return if no other enabled ports */
	if (p->enabled_port_in_mask == 0) {
		p->port_in_next = NULL;

		return 0;
	}

	/* Add current input port to the pipeline chain of enabled ports */
	port_prev_id = rte_mask_get_prev(p->enabled_port_in_mask, port_id);
	port_next_id = rte_mask_get_next(p->enabled_port_in_mask, port_id);

	port_prev = &p->ports_in[port_prev_id];
	port_next = &p->ports_in[port_next_id];

	port_prev->next = port_next;

	/* Check if the port which has just been disabled is next to serve */
	if (port == p->port_in_next)
		p->port_in_next = port_next;

	return 0;
}

/*
 * Pipeline run-time
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
	p->action_mask1[RTE_PIPELINE_ACTION_PORT_META] = 0;
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

	p->pkts_mask = pkts_mask;

	/* Output port user actions */
	if (port_out->f_action != NULL) {
		port_out->f_action(p, p->pkts, pkts_mask, port_out->arg_ah);

		RTE_PIPELINE_STATS_AH_DROP_READ(p,
			port_out->n_pkts_dropped_by_ah);
	}

	/* Output port TX */
	if (p->pkts_mask != 0)
		port_out->ops.f_tx_bulk(port_out->h_port,
			p->pkts,
			p->pkts_mask);
}

static inline void
rte_pipeline_action_handler_port(struct rte_pipeline *p, uint64_t pkts_mask)
{
	p->pkts_mask = pkts_mask;

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
				uint64_t pkt_mask = 1LLU << i;

				port_out->f_action(p,
					p->pkts,
					pkt_mask,
					port_out->arg_ah);

				RTE_PIPELINE_STATS_AH_DROP_READ(p,
					port_out->n_pkts_dropped_by_ah);

				/* Output port TX */
				if (pkt_mask & p->pkts_mask)
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
				port_out->f_action(p,
					p->pkts,
					pkt_mask,
					port_out->arg_ah);

				RTE_PIPELINE_STATS_AH_DROP_READ(p,
					port_out->n_pkts_dropped_by_ah);

				/* Output port TX */
				if (pkt_mask & p->pkts_mask)
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
	p->pkts_mask = pkts_mask;

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
				uint64_t pkt_mask = 1LLU << i;

				port_out->f_action(p,
					p->pkts,
					pkt_mask,
					port_out->arg_ah);

				RTE_PIPELINE_STATS_AH_DROP_READ(p,
					port_out->n_pkts_dropped_by_ah);

				/* Output port TX */
				if (pkt_mask & p->pkts_mask)
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
				port_out->f_action(p,
					p->pkts,
					pkt_mask,
					port_out->arg_ah);

				RTE_PIPELINE_STATS_AH_DROP_READ(p,
					port_out->n_pkts_dropped_by_ah);

				/* Output port TX */
				if (pkt_mask & p->pkts_mask)
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
	struct rte_port_in *port_in = p->port_in_next;
	uint32_t n_pkts, table_id;

	if (port_in == NULL)
		return 0;

	/* Input port RX */
	n_pkts = port_in->ops.f_rx(port_in->h_port, p->pkts,
		port_in->burst_size);
	if (n_pkts == 0) {
		p->port_in_next = port_in->next;
		return 0;
	}

	p->pkts_mask = RTE_LEN2MASK(n_pkts, uint64_t);
	p->action_mask0[RTE_PIPELINE_ACTION_DROP] = 0;
	p->action_mask0[RTE_PIPELINE_ACTION_PORT] = 0;
	p->action_mask0[RTE_PIPELINE_ACTION_PORT_META] = 0;
	p->action_mask0[RTE_PIPELINE_ACTION_TABLE] = 0;

	/* Input port user actions */
	if (port_in->f_action != NULL) {
		port_in->f_action(p, p->pkts, n_pkts, port_in->arg_ah);

		RTE_PIPELINE_STATS_AH_DROP_READ(p,
			port_in->n_pkts_dropped_by_ah);
	}

	/* Table */
	for (table_id = port_in->table_id; p->pkts_mask != 0; ) {
		struct rte_table *table;
		uint64_t lookup_hit_mask, lookup_miss_mask;

		/* Lookup */
		table = &p->tables[table_id];
		table->ops.f_lookup(table->h_table, p->pkts, p->pkts_mask,
			&lookup_hit_mask, (void **) p->entries);
		lookup_miss_mask = p->pkts_mask & (~lookup_hit_mask);

		/* Lookup miss */
		if (lookup_miss_mask != 0) {
			struct rte_pipeline_table_entry *default_entry =
				table->default_entry;

			p->pkts_mask = lookup_miss_mask;

			/* Table user actions */
			if (table->f_action_miss != NULL) {
				table->f_action_miss(p,
					p->pkts,
					lookup_miss_mask,
					default_entry,
					table->arg_ah);

				RTE_PIPELINE_STATS_AH_DROP_READ(p,
					table->n_pkts_dropped_by_lkp_miss_ah);
			}

			/* Table reserved actions */
			if ((default_entry->action == RTE_PIPELINE_ACTION_PORT) &&
				(p->pkts_mask != 0))
				rte_pipeline_action_handler_port_bulk(p,
					p->pkts_mask,
					default_entry->port_id);
			else {
				uint32_t pos = default_entry->action;

				RTE_PIPELINE_STATS_TABLE_DROP0(p);

				p->action_mask0[pos] |= p->pkts_mask;

				RTE_PIPELINE_STATS_TABLE_DROP1(p,
					table->n_pkts_dropped_lkp_miss);
			}
		}

		/* Lookup hit */
		if (lookup_hit_mask != 0) {
			p->pkts_mask = lookup_hit_mask;

			/* Table user actions */
			if (table->f_action_hit != NULL) {
				table->f_action_hit(p,
					p->pkts,
					lookup_hit_mask,
					p->entries,
					table->arg_ah);

				RTE_PIPELINE_STATS_AH_DROP_READ(p,
					table->n_pkts_dropped_by_lkp_hit_ah);
			}

			/* Table reserved actions */
			RTE_PIPELINE_STATS_TABLE_DROP0(p);
			rte_pipeline_compute_masks(p, p->pkts_mask);
			p->action_mask0[RTE_PIPELINE_ACTION_DROP] |=
				p->action_mask1[
					RTE_PIPELINE_ACTION_DROP];
			p->action_mask0[RTE_PIPELINE_ACTION_PORT] |=
				p->action_mask1[
					RTE_PIPELINE_ACTION_PORT];
			p->action_mask0[RTE_PIPELINE_ACTION_PORT_META] |=
				p->action_mask1[
					RTE_PIPELINE_ACTION_PORT_META];
			p->action_mask0[RTE_PIPELINE_ACTION_TABLE] |=
				p->action_mask1[
					RTE_PIPELINE_ACTION_TABLE];

			RTE_PIPELINE_STATS_TABLE_DROP1(p,
				table->n_pkts_dropped_lkp_hit);
		}

		/* Prepare for next iteration */
		p->pkts_mask = p->action_mask0[RTE_PIPELINE_ACTION_TABLE];
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

	/* Pick candidate for next port IN to serve */
	p->port_in_next = port_in->next;

	return (int) n_pkts;
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

	port_out->ops.f_tx(port_out->h_port, pkt); /* Output port TX */

	return 0;
}

int rte_pipeline_ah_packet_hijack(struct rte_pipeline *p,
	uint64_t pkts_mask)
{
	pkts_mask &= p->pkts_mask;
	p->pkts_mask &= ~pkts_mask;

	return 0;
}

int rte_pipeline_ah_packet_drop(struct rte_pipeline *p,
	uint64_t pkts_mask)
{
	pkts_mask &= p->pkts_mask;
	p->pkts_mask &= ~pkts_mask;
	p->action_mask0[RTE_PIPELINE_ACTION_DROP] |= pkts_mask;

	RTE_PIPELINE_STATS_AH_DROP_WRITE(p, pkts_mask);
	return 0;
}

int rte_pipeline_port_in_stats_read(struct rte_pipeline *p, uint32_t port_id,
	struct rte_pipeline_port_in_stats *stats, int clear)
{
	struct rte_port_in *port;
	int retval;

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

	port = &p->ports_in[port_id];

	if (port->ops.f_stats != NULL) {
		retval = port->ops.f_stats(port->h_port, &stats->stats, clear);
		if (retval)
			return retval;
	} else if (stats != NULL)
		memset(&stats->stats, 0, sizeof(stats->stats));

	if (stats != NULL)
		stats->n_pkts_dropped_by_ah = port->n_pkts_dropped_by_ah;

	if (clear != 0)
		port->n_pkts_dropped_by_ah = 0;

	return 0;
}

int rte_pipeline_port_out_stats_read(struct rte_pipeline *p, uint32_t port_id,
	struct rte_pipeline_port_out_stats *stats, int clear)
{
	struct rte_port_out *port;
	int retval;

	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n", __func__);
		return -EINVAL;
	}

	if (port_id >= p->num_ports_out) {
		RTE_LOG(ERR, PIPELINE,
			"%s: port OUT ID %u is out of range\n", __func__, port_id);
		return -EINVAL;
	}

	port = &p->ports_out[port_id];
	if (port->ops.f_stats != NULL) {
		retval = port->ops.f_stats(port->h_port, &stats->stats, clear);
		if (retval != 0)
			return retval;
	} else if (stats != NULL)
		memset(&stats->stats, 0, sizeof(stats->stats));

	if (stats != NULL)
		stats->n_pkts_dropped_by_ah = port->n_pkts_dropped_by_ah;

	if (clear != 0)
		port->n_pkts_dropped_by_ah = 0;

	return 0;
}

int rte_pipeline_table_stats_read(struct rte_pipeline *p, uint32_t table_id,
	struct rte_pipeline_table_stats *stats, int clear)
{
	struct rte_table *table;
	int retval;

	if (p == NULL) {
		RTE_LOG(ERR, PIPELINE, "%s: pipeline parameter NULL\n",
			__func__);
		return -EINVAL;
	}

	if (table_id >= p->num_tables) {
		RTE_LOG(ERR, PIPELINE,
				"%s: table %u is out of range\n", __func__, table_id);
		return -EINVAL;
	}

	table = &p->tables[table_id];
	if (table->ops.f_stats != NULL) {
		retval = table->ops.f_stats(table->h_table, &stats->stats, clear);
		if (retval != 0)
			return retval;
	} else if (stats != NULL)
		memset(&stats->stats, 0, sizeof(stats->stats));

	if (stats != NULL) {
		stats->n_pkts_dropped_by_lkp_hit_ah =
			table->n_pkts_dropped_by_lkp_hit_ah;
		stats->n_pkts_dropped_by_lkp_miss_ah =
			table->n_pkts_dropped_by_lkp_miss_ah;
		stats->n_pkts_dropped_lkp_hit = table->n_pkts_dropped_lkp_hit;
		stats->n_pkts_dropped_lkp_miss = table->n_pkts_dropped_lkp_miss;
	}

	if (clear != 0) {
		table->n_pkts_dropped_by_lkp_hit_ah = 0;
		table->n_pkts_dropped_by_lkp_miss_ah = 0;
		table->n_pkts_dropped_lkp_hit = 0;
		table->n_pkts_dropped_lkp_miss = 0;
	}

	return 0;
}
