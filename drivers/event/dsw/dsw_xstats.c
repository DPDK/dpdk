/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#include "dsw_evdev.h"

#include <stdbool.h>
#include <string.h>

#include <rte_debug.h>

/* The high bits in the xstats id is used to store an additional
 * parameter (beyond the queue or port id already in the xstats
 * interface).
 */
#define DSW_XSTATS_ID_PARAM_BITS (8)
#define DSW_XSTATS_ID_STAT_BITS					\
	(sizeof(unsigned int)*CHAR_BIT - DSW_XSTATS_ID_PARAM_BITS)
#define DSW_XSTATS_ID_STAT_MASK ((1 << DSW_XSTATS_ID_STAT_BITS) - 1)

#define DSW_XSTATS_ID_GET_PARAM(id)		\
	((id)>>DSW_XSTATS_ID_STAT_BITS)

#define DSW_XSTATS_ID_GET_STAT(id)		\
	((id) & DSW_XSTATS_ID_STAT_MASK)

#define DSW_XSTATS_ID_CREATE(id, param_value)			\
	(((param_value) << DSW_XSTATS_ID_STAT_BITS) | id)

typedef
uint64_t (*dsw_xstats_dev_get_value_fn)(struct dsw_evdev *dsw);

struct dsw_xstat_dev {
	const char *name;
	dsw_xstats_dev_get_value_fn get_value_fn;
};

typedef
uint64_t (*dsw_xstats_port_get_value_fn)(struct dsw_evdev *dsw,
					 uint8_t port_id, uint8_t queue_id);

struct dsw_xstats_port {
	const char *name_fmt;
	dsw_xstats_port_get_value_fn get_value_fn;
	bool per_queue;
};

static uint64_t
dsw_xstats_dev_credits_on_loan(struct dsw_evdev *dsw)
{
	return rte_atomic32_read(&dsw->credits_on_loan);
}

static struct dsw_xstat_dev dsw_dev_xstats[] = {
	{ "dev_credits_on_loan", dsw_xstats_dev_credits_on_loan }
};

#define DSW_GEN_PORT_ACCESS_FN(_variable)				\
	static uint64_t							\
	dsw_xstats_port_get_ ## _variable(struct dsw_evdev *dsw,	\
					  uint8_t port_id,		\
					  uint8_t queue_id __rte_unused) \
	{								\
		return dsw->ports[port_id]._variable;			\
	}

DSW_GEN_PORT_ACCESS_FN(new_enqueued)
DSW_GEN_PORT_ACCESS_FN(forward_enqueued)
DSW_GEN_PORT_ACCESS_FN(release_enqueued)

static uint64_t
dsw_xstats_port_get_queue_enqueued(struct dsw_evdev *dsw, uint8_t port_id,
				   uint8_t queue_id)
{
	return dsw->ports[port_id].queue_enqueued[queue_id];
}

DSW_GEN_PORT_ACCESS_FN(dequeued)

static uint64_t
dsw_xstats_port_get_queue_dequeued(struct dsw_evdev *dsw, uint8_t port_id,
				   uint8_t queue_id)
{
	return dsw->ports[port_id].queue_dequeued[queue_id];
}

DSW_GEN_PORT_ACCESS_FN(migrations)

static uint64_t
dsw_xstats_port_get_migration_latency(struct dsw_evdev *dsw, uint8_t port_id,
				      uint8_t queue_id __rte_unused)
{
	uint64_t total_latency = dsw->ports[port_id].migration_latency;
	uint64_t num_migrations = dsw->ports[port_id].migrations;

	return num_migrations > 0 ? total_latency / num_migrations : 0;
}

static uint64_t
dsw_xstats_port_get_event_proc_latency(struct dsw_evdev *dsw, uint8_t port_id,
				       uint8_t queue_id __rte_unused)
{
	uint64_t total_busy_cycles =
		dsw->ports[port_id].total_busy_cycles;
	uint64_t dequeued =
		dsw->ports[port_id].dequeued;

	return dequeued > 0 ? total_busy_cycles / dequeued : 0;
}

DSW_GEN_PORT_ACCESS_FN(inflight_credits)

static uint64_t
dsw_xstats_port_get_load(struct dsw_evdev *dsw, uint8_t port_id,
			 uint8_t queue_id __rte_unused)
{
	int16_t load;

	load = rte_atomic16_read(&dsw->ports[port_id].load);

	return DSW_LOAD_TO_PERCENT(load);
}

DSW_GEN_PORT_ACCESS_FN(last_bg)

static struct dsw_xstats_port dsw_port_xstats[] = {
	{ "port_%u_new_enqueued", dsw_xstats_port_get_new_enqueued,
	  false },
	{ "port_%u_forward_enqueued", dsw_xstats_port_get_forward_enqueued,
	  false },
	{ "port_%u_release_enqueued", dsw_xstats_port_get_release_enqueued,
	  false },
	{ "port_%u_queue_%u_enqueued", dsw_xstats_port_get_queue_enqueued,
	  true },
	{ "port_%u_dequeued", dsw_xstats_port_get_dequeued,
	  false },
	{ "port_%u_queue_%u_dequeued", dsw_xstats_port_get_queue_dequeued,
	  true },
	{ "port_%u_migrations", dsw_xstats_port_get_migrations,
	  false },
	{ "port_%u_migration_latency", dsw_xstats_port_get_migration_latency,
	  false },
	{ "port_%u_event_proc_latency", dsw_xstats_port_get_event_proc_latency,
	  false },
	{ "port_%u_inflight_credits", dsw_xstats_port_get_inflight_credits,
	  false },
	{ "port_%u_load", dsw_xstats_port_get_load,
	  false },
	{ "port_%u_last_bg", dsw_xstats_port_get_last_bg,
	  false }
};

static int
dsw_xstats_dev_get_names(struct rte_event_dev_xstats_name *xstats_names,
			 unsigned int *ids, unsigned int size)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(dsw_dev_xstats) && i < size; i++) {
		ids[i] = i;
		strcpy(xstats_names[i].name, dsw_dev_xstats[i].name);
	}

	return i;
}

static int
dsw_xstats_port_get_names(struct dsw_evdev *dsw, uint8_t port_id,
			  struct rte_event_dev_xstats_name *xstats_names,
			  unsigned int *ids, unsigned int size)
{
	uint8_t queue_id = 0;
	unsigned int id_idx;
	unsigned int stat_idx;

	for (id_idx = 0, stat_idx = 0;
	     id_idx < size && stat_idx < RTE_DIM(dsw_port_xstats);
	     id_idx++) {
		struct dsw_xstats_port *xstat = &dsw_port_xstats[stat_idx];

		if (xstat->per_queue) {
			ids[id_idx] = DSW_XSTATS_ID_CREATE(stat_idx, queue_id);
			snprintf(xstats_names[id_idx].name,
				 RTE_EVENT_DEV_XSTATS_NAME_SIZE,
				 dsw_port_xstats[stat_idx].name_fmt, port_id,
				 queue_id);
			queue_id++;
		} else {
			ids[id_idx] = stat_idx;
			snprintf(xstats_names[id_idx].name,
				 RTE_EVENT_DEV_XSTATS_NAME_SIZE,
				 dsw_port_xstats[stat_idx].name_fmt, port_id);
		}

		if (!(xstat->per_queue && queue_id < dsw->num_queues)) {
			stat_idx++;
			queue_id = 0;
		}
	}
	return id_idx;
}

int
dsw_xstats_get_names(const struct rte_eventdev *dev,
		     enum rte_event_dev_xstats_mode mode,
		     uint8_t queue_port_id,
		     struct rte_event_dev_xstats_name *xstats_names,
		     unsigned int *ids, unsigned int size)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		return dsw_xstats_dev_get_names(xstats_names, ids, size);
	case RTE_EVENT_DEV_XSTATS_PORT:
		return dsw_xstats_port_get_names(dsw, queue_port_id,
						 xstats_names, ids, size);
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		return 0;
	default:
		RTE_ASSERT(false);
		return -1;
	}
}

static int
dsw_xstats_dev_get(const struct rte_eventdev *dev,
		   const unsigned int ids[], uint64_t values[], unsigned int n)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int id = ids[i];
		struct dsw_xstat_dev *xstat = &dsw_dev_xstats[id];
		values[i] = xstat->get_value_fn(dsw);
	}
	return n;
}

static int
dsw_xstats_port_get(const struct rte_eventdev *dev, uint8_t port_id,
		    const unsigned int ids[], uint64_t values[], unsigned int n)
{
	struct dsw_evdev *dsw = dsw_pmd_priv(dev);
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int id = ids[i];
		unsigned int stat_idx = DSW_XSTATS_ID_GET_STAT(id);
		struct dsw_xstats_port *xstat = &dsw_port_xstats[stat_idx];
		uint8_t queue_id = 0;

		if (xstat->per_queue)
			queue_id = DSW_XSTATS_ID_GET_PARAM(id);

		values[i] = xstat->get_value_fn(dsw, port_id, queue_id);
	}
	return n;
}

int
dsw_xstats_get(const struct rte_eventdev *dev,
	       enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
	       const unsigned int ids[], uint64_t values[], unsigned int n)
{
	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		return dsw_xstats_dev_get(dev, ids, values, n);
	case RTE_EVENT_DEV_XSTATS_PORT:
		return dsw_xstats_port_get(dev, queue_port_id, ids, values, n);
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		return 0;
	default:
		RTE_ASSERT(false);
		return -1;
	}
	return 0;
}

uint64_t dsw_xstats_get_by_name(const struct rte_eventdev *dev,
				const char *name, unsigned int *id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(name);
	RTE_SET_USED(id);
	return 0;
}
