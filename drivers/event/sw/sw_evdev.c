/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#include <rte_vdev.h>
#include <rte_memzone.h>
#include <rte_kvargs.h>
#include <rte_ring.h>

#include "sw_evdev.h"

#define EVENTDEV_NAME_SW_PMD event_sw
#define NUMA_NODE_ARG "numa_node"
#define SCHED_QUANTA_ARG "sched_quanta"
#define CREDIT_QUANTA_ARG "credit_quanta"

static void
sw_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
				 struct rte_event_queue_conf *conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	static const struct rte_event_queue_conf default_conf = {
		.nb_atomic_flows = 4096,
		.nb_atomic_order_sequences = 1,
		.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY,
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
	};

	*conf = default_conf;
}

static void
sw_port_def_conf(struct rte_eventdev *dev, uint8_t port_id,
		 struct rte_event_port_conf *port_conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(port_id);

	port_conf->new_event_threshold = 1024;
	port_conf->dequeue_depth = 16;
	port_conf->enqueue_depth = 16;
}

static int
sw_dev_configure(const struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	const struct rte_eventdev_data *data = dev->data;
	const struct rte_event_dev_config *conf = &data->dev_conf;

	sw->qid_count = conf->nb_event_queues;
	sw->port_count = conf->nb_event_ports;
	sw->nb_events_limit = conf->nb_events_limit;

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		return -ENOTSUP;

	return 0;
}

static void
sw_info_get(struct rte_eventdev *dev, struct rte_event_dev_info *info)
{
	RTE_SET_USED(dev);

	static const struct rte_event_dev_info evdev_sw_info = {
			.driver_name = SW_PMD_NAME,
			.max_event_queues = RTE_EVENT_MAX_QUEUES_PER_DEV,
			.max_event_queue_flows = SW_QID_NUM_FIDS,
			.max_event_queue_priority_levels = SW_Q_PRIORITY_MAX,
			.max_event_priority_levels = SW_IQS_MAX,
			.max_event_ports = SW_PORTS_MAX,
			.max_event_port_dequeue_depth = MAX_SW_CONS_Q_DEPTH,
			.max_event_port_enqueue_depth = MAX_SW_PROD_Q_DEPTH,
			.max_num_events = SW_INFLIGHT_EVENTS_TOTAL,
			.event_dev_cap = (RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_EVENT_QOS),
	};

	*info = evdev_sw_info;
}

static int
assign_numa_node(const char *key __rte_unused, const char *value, void *opaque)
{
	int *socket_id = opaque;
	*socket_id = atoi(value);
	if (*socket_id >= RTE_MAX_NUMA_NODES)
		return -1;
	return 0;
}

static int
set_sched_quanta(const char *key __rte_unused, const char *value, void *opaque)
{
	int *quanta = opaque;
	*quanta = atoi(value);
	if (*quanta < 0 || *quanta >= 4096)
		return -1;
	return 0;
}

static int
set_credit_quanta(const char *key __rte_unused, const char *value, void *opaque)
{
	int *credit = opaque;
	*credit = atoi(value);
	if (*credit < 0 || *credit >= 128)
		return -1;
	return 0;
}

static int
sw_probe(const char *name, const char *params)
{
	static const struct rte_eventdev_ops evdev_sw_ops = {
			.dev_configure = sw_dev_configure,
			.dev_infos_get = sw_info_get,

			.queue_def_conf = sw_queue_def_conf,
			.port_def_conf = sw_port_def_conf,
	};

	static const char *const args[] = {
		NUMA_NODE_ARG,
		SCHED_QUANTA_ARG,
		CREDIT_QUANTA_ARG,
		NULL
	};
	struct rte_eventdev *dev;
	struct sw_evdev *sw;
	int socket_id = rte_socket_id();
	int sched_quanta  = SW_DEFAULT_SCHED_QUANTA;
	int credit_quanta = SW_DEFAULT_CREDIT_QUANTA;

	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (!kvlist) {
			SW_LOG_INFO(
				"Ignoring unsupported parameters when creating device '%s'\n",
				name);
		} else {
			int ret = rte_kvargs_process(kvlist, NUMA_NODE_ARG,
					assign_numa_node, &socket_id);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing numa node parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, SCHED_QUANTA_ARG,
					set_sched_quanta, &sched_quanta);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing sched quanta parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, CREDIT_QUANTA_ARG,
					set_credit_quanta, &credit_quanta);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing credit quanta parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			rte_kvargs_free(kvlist);
		}
	}

	SW_LOG_INFO(
			"Creating eventdev sw device %s, numa_node=%d, sched_quanta=%d, credit_quanta=%d\n",
			name, socket_id, sched_quanta, credit_quanta);

	dev = rte_event_pmd_vdev_init(name,
			sizeof(struct sw_evdev), socket_id);
	if (dev == NULL) {
		SW_LOG_ERR("eventdev vdev init() failed");
		return -EFAULT;
	}
	dev->dev_ops = &evdev_sw_ops;

	sw = dev->data->dev_private;
	sw->data = dev->data;

	/* copy values passed from vdev command line to instance */
	sw->credit_update_quanta = credit_quanta;
	sw->sched_quanta = sched_quanta;

	return 0;
}

static int
sw_remove(const char *name)
{
	if (name == NULL)
		return -EINVAL;

	SW_LOG_INFO("Closing eventdev sw device %s\n", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver evdev_sw_pmd_drv = {
	.probe = sw_probe,
	.remove = sw_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_SW_PMD, evdev_sw_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(event_sw, NUMA_NODE_ARG "=<int> "
		SCHED_QUANTA_ARG "=<int>" CREDIT_QUANTA_ARG "=<int>");
