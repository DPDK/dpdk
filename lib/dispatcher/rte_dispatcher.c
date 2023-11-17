/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_random.h>
#include <rte_service_component.h>

#include "eventdev_pmd.h"

#include <rte_dispatcher.h>

#define EVD_MAX_PORTS_PER_LCORE 4
#define EVD_MAX_HANDLERS 32
#define EVD_MAX_FINALIZERS 16
#define EVD_AVG_PRIO_INTERVAL 2000
#define EVD_SERVICE_NAME "dispatcher"

struct rte_dispatcher_lcore_port {
	uint8_t port_id;
	uint16_t batch_size;
	uint64_t timeout;
};

struct rte_dispatcher_handler {
	int id;
	rte_dispatcher_match_t match_fun;
	void *match_data;
	rte_dispatcher_process_t process_fun;
	void *process_data;
};

struct rte_dispatcher_finalizer {
	int id;
	rte_dispatcher_finalize_t finalize_fun;
	void *finalize_data;
};

struct rte_dispatcher_lcore {
	uint8_t num_ports;
	uint16_t num_handlers;
	int32_t prio_count;
	struct rte_dispatcher_lcore_port ports[EVD_MAX_PORTS_PER_LCORE];
	struct rte_dispatcher_handler handlers[EVD_MAX_HANDLERS];
	struct rte_dispatcher_stats stats;
} __rte_cache_aligned;

struct rte_dispatcher {
	uint8_t event_dev_id;
	int socket_id;
	uint32_t service_id;
	struct rte_dispatcher_lcore lcores[RTE_MAX_LCORE];
	uint16_t num_finalizers;
	struct rte_dispatcher_finalizer finalizers[EVD_MAX_FINALIZERS];
};

static int
evd_lookup_handler_idx(struct rte_dispatcher_lcore *lcore,
	const struct rte_event *event)
{
	uint16_t i;

	for (i = 0; i < lcore->num_handlers; i++) {
		struct rte_dispatcher_handler *handler =
			&lcore->handlers[i];

		if (handler->match_fun(event, handler->match_data))
			return i;
	}

	return -1;
}

static void
evd_prioritize_handler(struct rte_dispatcher_lcore *lcore,
	int handler_idx)
{
	struct rte_dispatcher_handler tmp;

	if (handler_idx == 0)
		return;

	/* Let the lucky handler "bubble" up the list */

	tmp = lcore->handlers[handler_idx - 1];
	lcore->handlers[handler_idx - 1] = lcore->handlers[handler_idx];
	lcore->handlers[handler_idx] = tmp;
}

static inline void
evd_consider_prioritize_handler(struct rte_dispatcher_lcore *lcore,
	int handler_idx, uint16_t handler_events)
{
	lcore->prio_count -= handler_events;

	if (unlikely(lcore->prio_count <= 0)) {
		evd_prioritize_handler(lcore, handler_idx);

		/*
		 * Randomize the interval in the unlikely case
		 * the traffic follow some very strict pattern.
		 */
		lcore->prio_count =
			rte_rand_max(EVD_AVG_PRIO_INTERVAL) +
			EVD_AVG_PRIO_INTERVAL / 2;
	}
}

static inline void
evd_dispatch_events(struct rte_dispatcher *dispatcher,
	struct rte_dispatcher_lcore *lcore,
	struct rte_dispatcher_lcore_port *port,
	struct rte_event *events, uint16_t num_events)
{
	int i;
	struct rte_event bursts[EVD_MAX_HANDLERS][num_events];
	uint16_t burst_lens[EVD_MAX_HANDLERS] = { 0 };
	uint16_t drop_count = 0;
	uint16_t dispatch_count;
	uint16_t dispatched = 0;

	for (i = 0; i < num_events; i++) {
		struct rte_event *event = &events[i];
		int handler_idx;

		handler_idx = evd_lookup_handler_idx(lcore, event);

		if (unlikely(handler_idx < 0)) {
			drop_count++;
			continue;
		}

		bursts[handler_idx][burst_lens[handler_idx]] = *event;
		burst_lens[handler_idx]++;
	}

	dispatch_count = num_events - drop_count;

	for (i = 0; i < lcore->num_handlers &&
		 dispatched < dispatch_count; i++) {
		struct rte_dispatcher_handler *handler =
			&lcore->handlers[i];
		uint16_t len = burst_lens[i];

		if (len == 0)
			continue;

		handler->process_fun(dispatcher->event_dev_id, port->port_id,
				     bursts[i], len, handler->process_data);

		dispatched += len;

		/*
		 * Safe, since any reshuffling will only involve
		 * already-processed handlers.
		 */
		evd_consider_prioritize_handler(lcore, i, len);
	}

	lcore->stats.ev_batch_count++;
	lcore->stats.ev_dispatch_count += dispatch_count;
	lcore->stats.ev_drop_count += drop_count;

	for (i = 0; i < dispatcher->num_finalizers; i++) {
		struct rte_dispatcher_finalizer *finalizer =
			&dispatcher->finalizers[i];

		finalizer->finalize_fun(dispatcher->event_dev_id,
					port->port_id,
					finalizer->finalize_data);
	}
}

static __rte_always_inline uint16_t
evd_port_dequeue(struct rte_dispatcher *dispatcher,
	struct rte_dispatcher_lcore *lcore,
	struct rte_dispatcher_lcore_port *port)
{
	uint16_t batch_size = port->batch_size;
	struct rte_event events[batch_size];
	uint16_t n;

	n = rte_event_dequeue_burst(dispatcher->event_dev_id, port->port_id,
				    events, batch_size, port->timeout);

	if (likely(n > 0))
		evd_dispatch_events(dispatcher, lcore, port, events, n);

	lcore->stats.poll_count++;

	return n;
}

static __rte_always_inline uint16_t
evd_lcore_process(struct rte_dispatcher *dispatcher,
	struct rte_dispatcher_lcore *lcore)
{
	uint16_t i;
	uint16_t event_count = 0;

	for (i = 0; i < lcore->num_ports; i++) {
		struct rte_dispatcher_lcore_port *port =
			&lcore->ports[i];

		event_count += evd_port_dequeue(dispatcher, lcore, port);
	}

	return event_count;
}

static int32_t
evd_process(void *userdata)
{
	struct rte_dispatcher *dispatcher = userdata;
	unsigned int lcore_id = rte_lcore_id();
	struct rte_dispatcher_lcore *lcore =
		&dispatcher->lcores[lcore_id];
	uint64_t event_count;

	event_count = evd_lcore_process(dispatcher, lcore);

	if (unlikely(event_count == 0))
		return -EAGAIN;

	return 0;
}

static int
evd_service_register(struct rte_dispatcher *dispatcher)
{
	struct rte_service_spec service = {
		.callback = evd_process,
		.callback_userdata = dispatcher,
		.capabilities = RTE_SERVICE_CAP_MT_SAFE,
		.socket_id = dispatcher->socket_id
	};
	int rc;

	snprintf(service.name, sizeof(service.name), EVD_SERVICE_NAME);

	rc = rte_service_component_register(&service, &dispatcher->service_id);
	if (rc != 0)
		RTE_EDEV_LOG_ERR("Registration of dispatcher service "
				 "%s failed with error code %d",
				 service.name, rc);

	return rc;
}

static int
evd_service_unregister(struct rte_dispatcher *dispatcher)
{
	int rc;

	rc = rte_service_component_unregister(dispatcher->service_id);
	if (rc != 0)
		RTE_EDEV_LOG_ERR("Unregistration of dispatcher service "
				 "failed with error code %d", rc);

	return rc;
}

struct rte_dispatcher *
rte_dispatcher_create(uint8_t event_dev_id)
{
	int socket_id;
	struct rte_dispatcher *dispatcher;
	int rc;

	socket_id = rte_event_dev_socket_id(event_dev_id);

	dispatcher =
		rte_malloc_socket("dispatcher", sizeof(struct rte_dispatcher),
				  RTE_CACHE_LINE_SIZE, socket_id);

	if (dispatcher == NULL) {
		RTE_EDEV_LOG_ERR("Unable to allocate memory for dispatcher");
		rte_errno = ENOMEM;
		return NULL;
	}

	*dispatcher = (struct rte_dispatcher) {
		.event_dev_id = event_dev_id,
		.socket_id = socket_id
	};

	rc = evd_service_register(dispatcher);
	if (rc < 0) {
		rte_free(dispatcher);
		rte_errno = -rc;
		return NULL;
	}

	return dispatcher;
}

int
rte_dispatcher_free(struct rte_dispatcher *dispatcher)
{
	int rc;

	if (dispatcher == NULL)
		return 0;

	rc = evd_service_unregister(dispatcher);
	if (rc != 0)
		return rc;

	rte_free(dispatcher);

	return 0;
}

uint32_t
rte_dispatcher_service_id_get(const struct rte_dispatcher *dispatcher)
{
	return dispatcher->service_id;
}

static int
lcore_port_index(struct rte_dispatcher_lcore *lcore,
	uint8_t event_port_id)
{
	uint16_t i;

	for (i = 0; i < lcore->num_ports; i++) {
		struct rte_dispatcher_lcore_port *port =
			&lcore->ports[i];

		if (port->port_id == event_port_id)
			return i;
	}

	return -1;
}

int
rte_dispatcher_bind_port_to_lcore(struct rte_dispatcher *dispatcher,
	uint8_t event_port_id, uint16_t batch_size, uint64_t timeout,
	unsigned int lcore_id)
{
	struct rte_dispatcher_lcore *lcore;
	struct rte_dispatcher_lcore_port *port;

	lcore =	&dispatcher->lcores[lcore_id];

	if (lcore->num_ports == EVD_MAX_PORTS_PER_LCORE)
		return -ENOMEM;

	if (lcore_port_index(lcore, event_port_id) >= 0)
		return -EEXIST;

	port = &lcore->ports[lcore->num_ports];

	*port = (struct rte_dispatcher_lcore_port) {
		.port_id = event_port_id,
		.batch_size = batch_size,
		.timeout = timeout
	};

	lcore->num_ports++;

	return 0;
}

int
rte_dispatcher_unbind_port_from_lcore(struct rte_dispatcher *dispatcher,
	uint8_t event_port_id, unsigned int lcore_id)
{
	struct rte_dispatcher_lcore *lcore;
	int port_idx;
	struct rte_dispatcher_lcore_port *port;
	struct rte_dispatcher_lcore_port *last;

	lcore =	&dispatcher->lcores[lcore_id];

	port_idx = lcore_port_index(lcore, event_port_id);

	if (port_idx < 0)
		return -ENOENT;

	port = &lcore->ports[port_idx];
	last = &lcore->ports[lcore->num_ports - 1];

	if (port != last)
		*port = *last;

	lcore->num_ports--;

	return 0;
}

static struct rte_dispatcher_handler *
evd_lcore_get_handler_by_id(struct rte_dispatcher_lcore *lcore, int handler_id)
{
	uint16_t i;

	for (i = 0; i < lcore->num_handlers; i++) {
		struct rte_dispatcher_handler *handler =
			&lcore->handlers[i];

		if (handler->id == handler_id)
			return handler;
	}

	return NULL;
}

static int
evd_alloc_handler_id(struct rte_dispatcher *dispatcher)
{
	int handler_id = 0;
	struct rte_dispatcher_lcore *reference_lcore =
		&dispatcher->lcores[0];

	if (reference_lcore->num_handlers == EVD_MAX_HANDLERS)
		return -1;

	while (evd_lcore_get_handler_by_id(reference_lcore, handler_id) != NULL)
		handler_id++;

	return handler_id;
}

static void
evd_lcore_install_handler(struct rte_dispatcher_lcore *lcore,
	const struct rte_dispatcher_handler *handler)
{
	int handler_idx = lcore->num_handlers;

	lcore->handlers[handler_idx] = *handler;
	lcore->num_handlers++;
}

static void
evd_install_handler(struct rte_dispatcher *dispatcher,
	const struct rte_dispatcher_handler *handler)
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct rte_dispatcher_lcore *lcore =
			&dispatcher->lcores[i];
		evd_lcore_install_handler(lcore, handler);
	}
}

int
rte_dispatcher_register(struct rte_dispatcher *dispatcher,
	rte_dispatcher_match_t match_fun, void *match_data,
	rte_dispatcher_process_t process_fun, void *process_data)
{
	struct rte_dispatcher_handler handler = {
		.match_fun = match_fun,
		.match_data = match_data,
		.process_fun = process_fun,
		.process_data = process_data
	};

	handler.id = evd_alloc_handler_id(dispatcher);

	if (handler.id < 0)
		return -ENOMEM;

	evd_install_handler(dispatcher, &handler);

	return handler.id;
}

static int
evd_lcore_uninstall_handler(struct rte_dispatcher_lcore *lcore,
	int handler_id)
{
	struct rte_dispatcher_handler *unreg_handler;
	int handler_idx;
	uint16_t last_idx;

	unreg_handler = evd_lcore_get_handler_by_id(lcore, handler_id);

	if (unreg_handler == NULL) {
		RTE_EDEV_LOG_ERR("Invalid handler id %d", handler_id);
		return -EINVAL;
	}

	handler_idx = unreg_handler - &lcore->handlers[0];

	last_idx = lcore->num_handlers - 1;

	if (handler_idx != last_idx) {
		/* move all handlers to maintain handler order */
		int n = last_idx - handler_idx;
		memmove(unreg_handler, unreg_handler + 1,
			sizeof(struct rte_dispatcher_handler) * n);
	}

	lcore->num_handlers--;

	return 0;
}

static int
evd_uninstall_handler(struct rte_dispatcher *dispatcher, int handler_id)
{
	unsigned int lcore_id;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		struct rte_dispatcher_lcore *lcore =
			&dispatcher->lcores[lcore_id];
		int rc;

		rc = evd_lcore_uninstall_handler(lcore, handler_id);
		if (rc < 0)
			return rc;
	}

	return 0;
}

int
rte_dispatcher_unregister(struct rte_dispatcher *dispatcher, int handler_id)
{
	return evd_uninstall_handler(dispatcher, handler_id);
}

static struct rte_dispatcher_finalizer *
evd_get_finalizer_by_id(struct rte_dispatcher *dispatcher,
		       int handler_id)
{
	int i;

	for (i = 0; i < dispatcher->num_finalizers; i++) {
		struct rte_dispatcher_finalizer *finalizer =
			&dispatcher->finalizers[i];

		if (finalizer->id == handler_id)
			return finalizer;
	}

	return NULL;
}

static int
evd_alloc_finalizer_id(struct rte_dispatcher *dispatcher)
{
	int finalizer_id = 0;

	while (evd_get_finalizer_by_id(dispatcher, finalizer_id) != NULL)
		finalizer_id++;

	return finalizer_id;
}

static struct rte_dispatcher_finalizer *
evd_alloc_finalizer(struct rte_dispatcher *dispatcher)
{
	int finalizer_idx;
	struct rte_dispatcher_finalizer *finalizer;

	if (dispatcher->num_finalizers == EVD_MAX_FINALIZERS)
		return NULL;

	finalizer_idx = dispatcher->num_finalizers;
	finalizer = &dispatcher->finalizers[finalizer_idx];

	finalizer->id = evd_alloc_finalizer_id(dispatcher);

	dispatcher->num_finalizers++;

	return finalizer;
}

int
rte_dispatcher_finalize_register(struct rte_dispatcher *dispatcher,
	rte_dispatcher_finalize_t finalize_fun, void *finalize_data)
{
	struct rte_dispatcher_finalizer *finalizer;

	finalizer = evd_alloc_finalizer(dispatcher);

	if (finalizer == NULL)
		return -ENOMEM;

	finalizer->finalize_fun = finalize_fun;
	finalizer->finalize_data = finalize_data;

	return finalizer->id;
}

int
rte_dispatcher_finalize_unregister(struct rte_dispatcher *dispatcher,
	int finalizer_id)
{
	struct rte_dispatcher_finalizer *unreg_finalizer;
	int finalizer_idx;
	uint16_t last_idx;

	unreg_finalizer = evd_get_finalizer_by_id(dispatcher, finalizer_id);

	if (unreg_finalizer == NULL) {
		RTE_EDEV_LOG_ERR("Invalid finalizer id %d", finalizer_id);
		return -EINVAL;
	}

	finalizer_idx = unreg_finalizer - &dispatcher->finalizers[0];

	last_idx = dispatcher->num_finalizers - 1;

	if (finalizer_idx != last_idx) {
		/* move all finalizers to maintain order */
		int n = last_idx - finalizer_idx;
		memmove(unreg_finalizer, unreg_finalizer + 1,
			sizeof(struct rte_dispatcher_finalizer) * n);
	}

	dispatcher->num_finalizers--;

	return 0;
}

static void
evd_set_service_runstate(struct rte_dispatcher *dispatcher, int state)
{
	int rc;

	rc = rte_service_component_runstate_set(dispatcher->service_id,
						state);
	/*
	 * The only cause of a runstate_set() failure is an invalid
	 * service id, which in turns means the dispatcher instance's
	 * state is invalid.
	 */
	if (rc != 0)
		RTE_EDEV_LOG_ERR("Unexpected error %d occurred while setting "
				 "service component run state to %d", rc,
				 state);

	RTE_VERIFY(rc == 0);
}

void
rte_dispatcher_start(struct rte_dispatcher *dispatcher)
{
	evd_set_service_runstate(dispatcher, 1);
}

void
rte_dispatcher_stop(struct rte_dispatcher *dispatcher)
{
	evd_set_service_runstate(dispatcher, 0);
}

static void
evd_aggregate_stats(struct rte_dispatcher_stats *result,
	const struct rte_dispatcher_stats *part)
{
	result->poll_count += part->poll_count;
	result->ev_batch_count += part->ev_batch_count;
	result->ev_dispatch_count += part->ev_dispatch_count;
	result->ev_drop_count += part->ev_drop_count;
}

void
rte_dispatcher_stats_get(const struct rte_dispatcher *dispatcher,
	struct rte_dispatcher_stats *stats)
{
	unsigned int lcore_id;

	*stats = (struct rte_dispatcher_stats) {};

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		const struct rte_dispatcher_lcore *lcore =
			&dispatcher->lcores[lcore_id];

		evd_aggregate_stats(stats, &lcore->stats);
	}
}

void
rte_dispatcher_stats_reset(struct rte_dispatcher *dispatcher)
{
	unsigned int lcore_id;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		struct rte_dispatcher_lcore *lcore =
			&dispatcher->lcores[lcore_id];

		lcore->stats = (struct rte_dispatcher_stats) {};
	}
}
