/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <inttypes.h>
#include <string.h>

#include <bus_vdev_driver.h>
#include <rte_errno.h>
#include <rte_cycles.h>
#include <rte_memzone.h>

#include "opdl_evdev.h"
#include "opdl_ring.h"
#include "opdl_log.h"


static __rte_always_inline uint32_t
enqueue_check(struct opdl_port *p,
		const struct rte_event ev[],
		uint16_t num,
		uint16_t num_events)
{
	uint16_t i;

	if (p->opdl->do_validation) {

		for (i = 0; i < num; i++) {
			if (ev[i].queue_id != p->next_external_qid) {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "ERROR - port:[%u] - event wants"
					     " to enq to q_id[%u],"
					     " but should be [%u]",
					     opdl_pmd_dev_id(p->opdl),
					     p->id,
					     ev[i].queue_id,
					     p->next_external_qid);
				rte_errno = EINVAL;
				return 0;
			}
		}

		/* Stats */
		if (p->p_type == OPDL_PURE_RX_PORT ||
				p->p_type == OPDL_ASYNC_PORT) {
			/* Stats */
			if (num_events) {
				p->port_stat[claim_pkts_requested] += num;
				p->port_stat[claim_pkts_granted] += num_events;
				p->port_stat[claim_non_empty]++;
				p->start_cycles = rte_rdtsc();
			} else {
				p->port_stat[claim_empty]++;
				p->start_cycles = 0;
			}
		} else {
			if (p->start_cycles) {
				uint64_t end_cycles = rte_rdtsc();
				p->port_stat[total_cycles] +=
					end_cycles - p->start_cycles;
			}
		}
	} else {
		if (num > 0 &&
				ev[0].queue_id != p->next_external_qid) {
			rte_errno = EINVAL;
			return 0;
		}
	}

	return num;
}

static __rte_always_inline void
update_on_dequeue(struct opdl_port *p,
		struct rte_event ev[],
		uint16_t num,
		uint16_t num_events)
{
	if (p->opdl->do_validation) {
		int16_t i;
		for (i = 0; i < num; i++)
			ev[i].queue_id =
				p->opdl->queue[p->queue_id].external_qid;

		/* Stats */
		if (num_events) {
			p->port_stat[claim_pkts_requested] += num;
			p->port_stat[claim_pkts_granted] += num_events;
			p->port_stat[claim_non_empty]++;
			p->start_cycles = rte_rdtsc();
		} else {
			p->port_stat[claim_empty]++;
			p->start_cycles = 0;
		}
	} else {
		if (num > 0)
			ev[0].queue_id =
				p->opdl->queue[p->queue_id].external_qid;
	}
}


/*
 * Error RX enqueue:
 *
 *
 */

static uint16_t
opdl_rx_error_enqueue(struct opdl_port *p,
		const struct rte_event ev[],
		uint16_t num)
{
	RTE_SET_USED(p);
	RTE_SET_USED(ev);
	RTE_SET_USED(num);

	rte_errno = ENOSPC;

	return 0;
}

/*
 * RX enqueue:
 *
 * This function handles enqueue for a single input stage_inst with
 *	threadsafe disabled or enabled. eg 1 thread using a stage_inst or
 *	multiple threads sharing a stage_inst
 */

static uint16_t
opdl_rx_enqueue(struct opdl_port *p,
		const struct rte_event ev[],
		uint16_t num)
{
	uint16_t enqueued = 0;

	enqueued = opdl_ring_input(opdl_stage_get_opdl_ring(p->enq_stage_inst),
				   ev,
				   num,
				   false);
	if (!enqueue_check(p, ev, num, enqueued))
		return 0;


	if (enqueued < num)
		rte_errno = ENOSPC;

	return enqueued;
}

/*
 * Error TX handler
 *
 */

static uint16_t
opdl_tx_error_dequeue(struct opdl_port *p,
		struct rte_event ev[],
		uint16_t num)
{
	RTE_SET_USED(p);
	RTE_SET_USED(ev);
	RTE_SET_USED(num);

	rte_errno = ENOSPC;

	return 0;
}

/*
 * TX single threaded claim
 *
 * This function handles dequeue for a single worker stage_inst with
 *	threadsafe disabled. eg 1 thread using an stage_inst
 */

static uint16_t
opdl_tx_dequeue_single_thread(struct opdl_port *p,
			struct rte_event ev[],
			uint16_t num)
{
	uint16_t returned;

	struct opdl_ring  *ring;

	ring = opdl_stage_get_opdl_ring(p->deq_stage_inst);

	returned = opdl_ring_copy_to_burst(ring,
					   p->deq_stage_inst,
					   ev,
					   num,
					   false);

	update_on_dequeue(p, ev, num, returned);

	return returned;
}

/*
 * TX multi threaded claim
 *
 * This function handles dequeue for multiple worker stage_inst with
 *	threadsafe disabled. eg multiple stage_inst each with its own instance
 */

static uint16_t
opdl_tx_dequeue_multi_inst(struct opdl_port *p,
			struct rte_event ev[],
			uint16_t num)
{
	uint32_t num_events = 0;

	num_events = opdl_stage_claim(p->deq_stage_inst,
				    (void *)ev,
				    num,
				    NULL,
				    false,
				    false);

	update_on_dequeue(p, ev, num, num_events);

	return opdl_stage_disclaim(p->deq_stage_inst, num_events, false);
}


/*
 * Worker thread claim
 *
 */

static uint16_t
opdl_claim(struct opdl_port *p, struct rte_event ev[], uint16_t num)
{
	uint32_t num_events = 0;

	if (unlikely(num > MAX_OPDL_CONS_Q_DEPTH)) {
		PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
			     "Attempt to dequeue num of events larger than port (%d) max",
			     opdl_pmd_dev_id(p->opdl),
			     p->id);
		rte_errno = EINVAL;
		return 0;
	}


	num_events = opdl_stage_claim(p->deq_stage_inst,
			(void *)ev,
			num,
			NULL,
			false,
			p->atomic_claim);


	update_on_dequeue(p, ev, num, num_events);

	return num_events;
}

/*
 * Worker thread disclaim
 */

static uint16_t
opdl_disclaim(struct opdl_port *p, const struct rte_event ev[], uint16_t num)
{
	uint16_t enqueued = 0;

	uint32_t i = 0;

	for (i = 0; i < num; i++)
		opdl_ring_cas_slot(p->enq_stage_inst, &ev[i],
				i, p->atomic_claim);

	enqueued = opdl_stage_disclaim(p->enq_stage_inst,
				       num,
				       false);

	return enqueue_check(p, ev, num, enqueued);
}

static __rte_always_inline struct opdl_stage *
stage_for_port(struct opdl_queue *q, unsigned int i)
{
	if (q->q_pos == OPDL_Q_POS_START || q->q_pos == OPDL_Q_POS_MIDDLE)
		return q->ports[i]->enq_stage_inst;
	else
		return q->ports[i]->deq_stage_inst;
}

static int opdl_add_deps(struct opdl_evdev *device,
			 int q_id,
			 int deps_q_id)
{
	unsigned int i, j;
	int status;
	struct opdl_ring  *ring;
	struct opdl_queue *queue = &device->queue[q_id];
	struct opdl_queue *queue_deps = &device->queue[deps_q_id];
	struct opdl_stage *dep_stages[OPDL_PORTS_MAX];

	/* sanity check that all stages are for same opdl ring */
	for (i = 0; i < queue->nb_ports; i++) {
		struct opdl_ring *r =
			opdl_stage_get_opdl_ring(stage_for_port(queue, i));
		for (j = 0; j < queue_deps->nb_ports; j++) {
			struct opdl_ring *rj =
				opdl_stage_get_opdl_ring(
						stage_for_port(queue_deps, j));
			if (r != rj) {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "Stages and dependents"
					     " are not for same opdl ring",
					     opdl_pmd_dev_id(device));
				uint32_t k;
				for (k = 0; k < device->nb_opdls; k++) {
					opdl_ring_dump(device->opdl[k],
							stdout);
				}
				return -EINVAL;
			}
		}
	}

	/* Gather all stages instance in deps */
	for (i = 0; i < queue_deps->nb_ports; i++)
		dep_stages[i] = stage_for_port(queue_deps, i);


	/* Add all deps for each port->stage_inst in this queue */
	for (i = 0; i < queue->nb_ports; i++) {

		ring = opdl_stage_get_opdl_ring(stage_for_port(queue, i));

		status = opdl_stage_deps_add(ring,
				stage_for_port(queue, i),
				queue->ports[i]->num_instance,
				queue->ports[i]->instance_id,
				dep_stages,
				queue_deps->nb_ports);
		if (status < 0)
			return -EINVAL;
	}

	return 0;
}

int
opdl_add_event_handlers(struct rte_eventdev *dev)
{
	int err = 0;

	struct opdl_evdev *device = opdl_pmd_priv(dev);
	unsigned int i;

	for (i = 0; i < device->max_port_nb; i++) {

		struct opdl_port *port = &device->ports[i];

		if (port->configured) {
			if (port->p_type == OPDL_PURE_RX_PORT) {
				port->enq = opdl_rx_enqueue;
				port->deq = opdl_tx_error_dequeue;

			} else if (port->p_type == OPDL_PURE_TX_PORT) {

				port->enq = opdl_rx_error_enqueue;

				if (port->num_instance == 1)
					port->deq =
						opdl_tx_dequeue_single_thread;
				else
					port->deq = opdl_tx_dequeue_multi_inst;

			} else if (port->p_type == OPDL_REGULAR_PORT) {

				port->enq = opdl_disclaim;
				port->deq = opdl_claim;

			} else if (port->p_type == OPDL_ASYNC_PORT) {

				port->enq = opdl_rx_enqueue;

				/* Always single instance */
				port->deq = opdl_tx_dequeue_single_thread;
			} else {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "port:[%u] has invalid port type - ",
					     opdl_pmd_dev_id(port->opdl),
					     port->id);
				err = -EINVAL;
				break;
			}
			port->initialized = 1;
		}
	}

	if (!err)
		fprintf(stdout, "Success - enqueue/dequeue handler(s) added\n");
	return err;
}

int
build_all_dependencies(struct rte_eventdev *dev)
{

	int err = 0;
	unsigned int i;
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	uint8_t start_qid = 0;

	for (i = 0; i < RTE_EVENT_MAX_QUEUES_PER_DEV; i++) {
		struct opdl_queue *queue = &device->queue[i];
		if (!queue->initialized)
			break;

		if (queue->q_pos == OPDL_Q_POS_START) {
			start_qid = i;
			continue;
		}

		if (queue->q_pos == OPDL_Q_POS_MIDDLE) {
			err = opdl_add_deps(device, i, i-1);
			if (err < 0) {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "dependency addition for queue:[%u] - FAILED",
					     dev->data->dev_id,
					     queue->external_qid);
				break;
			}
		}

		if (queue->q_pos == OPDL_Q_POS_END) {
			/* Add this dependency */
			err = opdl_add_deps(device, i, i-1);
			if (err < 0) {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "dependency addition for queue:[%u] - FAILED",
					     dev->data->dev_id,
					     queue->external_qid);
				break;
			}
			/* Add dependency for rx on tx */
			err = opdl_add_deps(device, start_qid, i);
			if (err < 0) {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "dependency addition for queue:[%u] - FAILED",
					     dev->data->dev_id,
					     queue->external_qid);
				break;
			}
		}
	}

	if (!err)
		fprintf(stdout, "Success - dependencies built\n");

	return err;
}
int
check_queues_linked(struct rte_eventdev *dev)
{

	int err = 0;
	unsigned int i;
	struct opdl_evdev *device = opdl_pmd_priv(dev);
	uint32_t nb_iq = 0;

	for (i = 0; i < RTE_EVENT_MAX_QUEUES_PER_DEV; i++) {
		struct opdl_queue *queue = &device->queue[i];

		if (!queue->initialized)
			break;

		if (queue->external_qid == OPDL_INVALID_QID)
			nb_iq++;

		if (queue->nb_ports == 0) {
			PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
				     "queue:[%u] has no associated ports",
				     dev->data->dev_id,
				     i);
			err = -EINVAL;
			break;
		}
	}
	if (!err) {
		if ((i - nb_iq) != device->max_queue_nb) {
			PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
				     "%u queues counted but should be %u",
				     dev->data->dev_id,
				     i - nb_iq,
				     device->max_queue_nb);
			err = -1;
		}

	}
	return err;
}

void
destroy_queues_and_rings(struct rte_eventdev *dev)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);
	uint32_t i;

	for (i = 0; i < device->nb_opdls; i++) {
		if (device->opdl[i])
			opdl_ring_free(device->opdl[i]);
	}

	memset(&device->queue,
			0,
			sizeof(struct opdl_queue)
			* RTE_EVENT_MAX_QUEUES_PER_DEV);
}

#define OPDL_ID(d)(d->nb_opdls - 1)

static __rte_always_inline void
initialise_queue(struct opdl_evdev *device,
		enum queue_pos pos,
		int32_t i)
{
	struct opdl_queue *queue = &device->queue[device->nb_queues];

	if (i == -1) {
		queue->q_type = OPDL_Q_TYPE_ORDERED;
		queue->external_qid = OPDL_INVALID_QID;
	} else {
		queue->q_type = device->q_md[i].type;
		queue->external_qid = device->q_md[i].ext_id;
		/* Add ex->in for queues setup */
		device->q_map_ex_to_in[queue->external_qid] = device->nb_queues;
	}
	queue->opdl_id = OPDL_ID(device);
	queue->q_pos = pos;
	queue->nb_ports = 0;
	queue->configured = 1;

	device->nb_queues++;
}


static __rte_always_inline int
create_opdl(struct opdl_evdev *device)
{
	int err = 0;

	char name[RTE_MEMZONE_NAMESIZE];

	snprintf(name, RTE_MEMZONE_NAMESIZE,
			"%s_%u", device->service_name, device->nb_opdls);

	device->opdl[device->nb_opdls] =
		opdl_ring_create(name,
				device->nb_events_limit,
				sizeof(struct rte_event),
				device->max_port_nb * 2,
				device->socket);

	if (!device->opdl[device->nb_opdls]) {
		PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
			     "opdl ring %u creation - FAILED",
			     opdl_pmd_dev_id(device),
			     device->nb_opdls);
		err = -EINVAL;
	} else {
		device->nb_opdls++;
	}
	return err;
}

static __rte_always_inline int
create_link_opdl(struct opdl_evdev *device, uint32_t index)
{

	int err = 0;

	if (device->q_md[index + 1].type !=
			OPDL_Q_TYPE_SINGLE_LINK) {

		/* async queue with regular
		 * queue following it
		 */

		/* create a new opdl ring */
		err = create_opdl(device);
		if (!err) {
			/* create an initial
			 * dummy queue for new opdl
			 */
			initialise_queue(device,
					OPDL_Q_POS_START,
					-1);
		} else {
			err = -EINVAL;
		}
	} else {
		PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
			     "queue %u, two consecutive"
			     " SINGLE_LINK queues, not allowed",
			     opdl_pmd_dev_id(device),
			     index);
		err = -EINVAL;
	}

	return err;
}

int
create_queues_and_rings(struct rte_eventdev *dev)
{
	int err = 0;

	struct opdl_evdev *device = opdl_pmd_priv(dev);

	device->nb_queues = 0;

	if (device->nb_ports != device->max_port_nb) {
		PMD_DRV_LOG(ERR, "Number ports setup:%u NOT EQUAL to max port"
				" number:%u for this device",
				device->nb_ports,
				device->max_port_nb);
		err = -1;
	}

	if (!err) {
		/* We will have at least one opdl so create it now */
		err = create_opdl(device);
	}

	if (!err) {

		/* Create 1st "dummy" queue */
		initialise_queue(device,
				 OPDL_Q_POS_START,
				 -1);

		uint32_t i;
		for (i = 0; i < device->nb_q_md; i++) {

			/* Check */
			if (!device->q_md[i].setup) {

				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "queue meta data slot %u"
					     " not setup - FAILING",
					     dev->data->dev_id,
					     i);
				err = -EINVAL;
				break;
			} else if (device->q_md[i].type !=
					OPDL_Q_TYPE_SINGLE_LINK) {

				if (!device->q_md[i + 1].setup) {
					/* Create a simple ORDERED/ATOMIC
					 * queue at the end
					 */
					initialise_queue(device,
							OPDL_Q_POS_END,
							i);

				} else {
					/* Create a simple ORDERED/ATOMIC
					 * queue in the middle
					 */
					initialise_queue(device,
							OPDL_Q_POS_MIDDLE,
							i);
				}
			} else if (device->q_md[i].type ==
					OPDL_Q_TYPE_SINGLE_LINK) {

				/* create last queue for this opdl */
				initialise_queue(device,
						OPDL_Q_POS_END,
						i);

				err = create_link_opdl(device, i);

				if (err)
					break;


			}
		}
	}
	if (err)
		destroy_queues_and_rings(dev);

	return err;
}


int
initialise_all_other_ports(struct rte_eventdev *dev)
{
	int err = 0;
	struct opdl_stage *stage_inst = NULL;

	struct opdl_evdev *device = opdl_pmd_priv(dev);

	uint32_t i;
	for (i = 0; i < device->nb_ports; i++) {
		struct opdl_port *port = &device->ports[i];
		struct opdl_queue *queue = &device->queue[port->queue_id];

		if (port->queue_id == 0) {
			continue;
		} else if (queue->q_type != OPDL_Q_TYPE_SINGLE_LINK) {

			if (queue->q_pos == OPDL_Q_POS_MIDDLE) {

				/* Regular port with claim/disclaim */
				stage_inst = opdl_stage_add(
					device->opdl[queue->opdl_id],
						false,
						false);
				port->deq_stage_inst = stage_inst;
				port->enq_stage_inst = stage_inst;

				if (queue->q_type == OPDL_Q_TYPE_ATOMIC)
					port->atomic_claim = true;
				else
					port->atomic_claim = false;

				port->p_type =  OPDL_REGULAR_PORT;

				/* Add the port to the queue array of ports */
				queue->ports[queue->nb_ports] = port;
				port->instance_id = queue->nb_ports;
				queue->nb_ports++;
				opdl_stage_set_queue_id(stage_inst,
						port->queue_id);

			} else if (queue->q_pos == OPDL_Q_POS_END) {

				/* tx port  */
				stage_inst = opdl_stage_add(
					device->opdl[queue->opdl_id],
						false,
						false);
				port->deq_stage_inst = stage_inst;
				port->enq_stage_inst = NULL;
				port->p_type = OPDL_PURE_TX_PORT;

				/* Add the port to the queue array of ports */
				queue->ports[queue->nb_ports] = port;
				port->instance_id = queue->nb_ports;
				queue->nb_ports++;
			} else {

				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "port %u:, linked incorrectly"
					     " to a q_pos START/INVALID %u",
					     opdl_pmd_dev_id(port->opdl),
					     port->id,
					     queue->q_pos);
				err = -EINVAL;
				break;
			}

		} else if (queue->q_type == OPDL_Q_TYPE_SINGLE_LINK) {

			port->p_type = OPDL_ASYNC_PORT;

			/* -- tx -- */
			stage_inst = opdl_stage_add(
				device->opdl[queue->opdl_id],
					false,
					false); /* First stage */
			port->deq_stage_inst = stage_inst;

			/* Add the port to the queue array of ports */
			queue->ports[queue->nb_ports] = port;
			port->instance_id = queue->nb_ports;
			queue->nb_ports++;

			if (queue->nb_ports > 1) {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "queue %u:, setup as SINGLE_LINK"
					     " but has more than one port linked",
					     opdl_pmd_dev_id(port->opdl),
					     queue->external_qid);
				err = -EINVAL;
				break;
			}

			/* -- single instance rx for next opdl -- */
			uint8_t next_qid =
				device->q_map_ex_to_in[queue->external_qid] + 1;
			if (next_qid < RTE_EVENT_MAX_QUEUES_PER_DEV &&
					device->queue[next_qid].configured) {

				/* Remap the queue */
				queue = &device->queue[next_qid];

				stage_inst = opdl_stage_add(
					device->opdl[queue->opdl_id],
						false,
						true);
				port->enq_stage_inst = stage_inst;

				/* Add the port to the queue array of ports */
				queue->ports[queue->nb_ports] = port;
				port->instance_id = queue->nb_ports;
				queue->nb_ports++;
				if (queue->nb_ports > 1) {
					PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
						"dummy queue %u: for "
						"port %u, "
						"SINGLE_LINK but has more "
						"than one port linked",
						opdl_pmd_dev_id(port->opdl),
						next_qid,
						port->id);
					err = -EINVAL;
					break;
				}
				/* Set this queue to initialized as it is never
				 * referenced by any ports
				 */
				queue->initialized = 1;
			}
		}
	}

	/* Now that all ports are initialised we need to
	 * setup the last bit of stage md
	 */
	if (!err) {
		for (i = 0; i < device->nb_ports; i++) {
			struct opdl_port *port = &device->ports[i];
			struct opdl_queue *queue =
				&device->queue[port->queue_id];

			if (port->configured &&
					(port->queue_id != OPDL_INVALID_QID)) {
				if (queue->nb_ports == 0) {
					PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
						"queue:[%u] has no ports"
						" linked to it",
						opdl_pmd_dev_id(port->opdl),
						port->id);
					err = -EINVAL;
					break;
				}

				port->num_instance = queue->nb_ports;
				port->initialized = 1;
				queue->initialized = 1;
			} else {
				PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
					     "Port:[%u] not configured  invalid"
					     " queue configuration",
					     opdl_pmd_dev_id(port->opdl),
					     port->id);
				err = -EINVAL;
				break;
			}
		}
	}
	return err;
}

int
initialise_queue_zero_ports(struct rte_eventdev *dev)
{
	int err = 0;
	uint8_t mt_rx = 0;
	struct opdl_stage *stage_inst = NULL;
	struct opdl_queue *queue = NULL;

	struct opdl_evdev *device = opdl_pmd_priv(dev);

	/* Assign queue zero and figure out how many Q0 ports we have */
	uint32_t i;
	for (i = 0; i < device->nb_ports; i++) {
		struct opdl_port *port = &device->ports[i];
		if (port->queue_id == OPDL_INVALID_QID) {
			port->queue_id = 0;
			port->external_qid = OPDL_INVALID_QID;
			port->p_type = OPDL_PURE_RX_PORT;
			mt_rx++;
		}
	}

	/* Create the stage */
	stage_inst = opdl_stage_add(device->opdl[0],
			(mt_rx > 1 ? true : false),
			true);
	if (stage_inst) {

		/* Assign the new created input stage to all relevant ports */
		for (i = 0; i < device->nb_ports; i++) {
			struct opdl_port *port = &device->ports[i];
			if (port->queue_id == 0) {
				queue = &device->queue[port->queue_id];
				port->enq_stage_inst = stage_inst;
				port->deq_stage_inst = NULL;
				port->configured = 1;
				port->initialized = 1;

				queue->ports[queue->nb_ports] = port;
				port->instance_id = queue->nb_ports;
				queue->nb_ports++;
			}
		}
	} else {
		err = -1;
	}
	return err;
}

int
assign_internal_queue_ids(struct rte_eventdev *dev)
{
	int err = 0;
	struct opdl_evdev *device = opdl_pmd_priv(dev);
	uint32_t i;

	for (i = 0; i < device->nb_ports; i++) {
		struct opdl_port *port = &device->ports[i];
		if (port->external_qid != OPDL_INVALID_QID) {
			port->queue_id =
				device->q_map_ex_to_in[port->external_qid];

			/* Now do the external_qid of the next queue */
			struct opdl_queue *queue =
				&device->queue[port->queue_id];
			if (queue->q_pos == OPDL_Q_POS_END)
				port->next_external_qid =
				device->queue[port->queue_id + 2].external_qid;
			else
				port->next_external_qid =
				device->queue[port->queue_id + 1].external_qid;
		}
	}
	return err;
}
