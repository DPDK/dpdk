/*-
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <inttypes.h>
#include <string.h>

#include <rte_bus_vdev.h>
#include <rte_lcore.h>
#include <rte_memzone.h>
#include <rte_kvargs.h>
#include <rte_errno.h>
#include <rte_cycles.h>

#include "opdl_evdev.h"
#include "opdl_ring.h"
#include "opdl_log.h"

#define EVENTDEV_NAME_OPDL_PMD event_opdl
#define NUMA_NODE_ARG "numa_node"
#define DO_VALIDATION_ARG "do_validation"
#define DO_TEST_ARG "self_test"


static void
opdl_info_get(struct rte_eventdev *dev, struct rte_event_dev_info *info);




static int
opdl_dev_configure(const struct rte_eventdev *dev)
{
	struct opdl_evdev *opdl = opdl_pmd_priv(dev);
	const struct rte_eventdev_data *data = dev->data;
	const struct rte_event_dev_config *conf = &data->dev_conf;

	opdl->max_queue_nb = conf->nb_event_queues;
	opdl->max_port_nb = conf->nb_event_ports;
	opdl->nb_events_limit = conf->nb_events_limit;

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT) {
		PMD_DRV_LOG(ERR, "DEV_ID:[%02d] : "
			     "DEQUEUE_TIMEOUT not supported\n",
			     dev->data->dev_id);
		return -ENOTSUP;
	}

	return 0;
}

static void
opdl_info_get(struct rte_eventdev *dev, struct rte_event_dev_info *info)
{
	RTE_SET_USED(dev);

	static const struct rte_event_dev_info evdev_opdl_info = {
		.driver_name = OPDL_PMD_NAME,
		.max_event_queues = RTE_EVENT_MAX_QUEUES_PER_DEV,
		.max_event_queue_flows = OPDL_QID_NUM_FIDS,
		.max_event_queue_priority_levels = OPDL_Q_PRIORITY_MAX,
		.max_event_priority_levels = OPDL_IQS_MAX,
		.max_event_ports = OPDL_PORTS_MAX,
		.max_event_port_dequeue_depth = MAX_OPDL_CONS_Q_DEPTH,
		.max_event_port_enqueue_depth = MAX_OPDL_CONS_Q_DEPTH,
		.max_num_events = OPDL_INFLIGHT_EVENTS_TOTAL,
		.event_dev_cap = RTE_EVENT_DEV_CAP_BURST_MODE,
	};

	*info = evdev_opdl_info;
}

static void
opdl_dump(struct rte_eventdev *dev, FILE *f)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	if (!device->do_validation)
		return;

	fprintf(f,
		"\n\n -- RING STATISTICS --\n");

	for (uint32_t i = 0; i < device->nb_opdls; i++)
		opdl_ring_dump(device->opdl[i], f);

	fprintf(f,
		"\n\n -- PORT STATISTICS --\n"
		"Type Port Index  Port Id  Queue Id     Av. Req Size  "
		"Av. Grant Size     Av. Cycles PP"
		"      Empty DEQs   Non Empty DEQs   Pkts Processed\n");

	for (uint32_t i = 0; i < device->max_port_nb; i++) {
		char queue_id[64];
		char total_cyc[64];
		const char *p_type;

		uint64_t cne, cpg;
		struct opdl_port *port = &device->ports[i];

		if (port->initialized) {
			cne = port->port_stat[claim_non_empty];
			cpg = port->port_stat[claim_pkts_granted];
			if (port->p_type == OPDL_REGULAR_PORT)
				p_type = "REG";
			else if (port->p_type == OPDL_PURE_RX_PORT)
				p_type = "  RX";
			else if (port->p_type == OPDL_PURE_TX_PORT)
				p_type = "  TX";
			else if (port->p_type == OPDL_ASYNC_PORT)
				p_type = "SYNC";
			else
				p_type = "????";

			sprintf(queue_id, "%02u", port->external_qid);
			if (port->p_type == OPDL_REGULAR_PORT ||
					port->p_type == OPDL_ASYNC_PORT)
				sprintf(total_cyc,
					" %'16"PRIu64"",
					(cpg != 0 ?
					 port->port_stat[total_cycles] / cpg
					 : 0));
			else
				sprintf(total_cyc,
					"             ----");
			fprintf(f,
				"%4s %10u %8u %9s %'16"PRIu64" %'16"PRIu64" %s "
				"%'16"PRIu64" %'16"PRIu64" %'16"PRIu64"\n",
				p_type,
				i,
				port->id,
				(port->external_qid == OPDL_INVALID_QID ? "---"
				 : queue_id),
				(cne != 0 ?
				 port->port_stat[claim_pkts_requested] / cne
				 : 0),
				(cne != 0 ?
				 port->port_stat[claim_pkts_granted] / cne
				 : 0),
				total_cyc,
				port->port_stat[claim_empty],
				port->port_stat[claim_non_empty],
				port->port_stat[claim_pkts_granted]);
		}
	}
	fprintf(f, "\n");
}


static void
opdl_stop(struct rte_eventdev *dev)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);

	opdl_xstats_uninit(dev);

	destroy_queues_and_rings(dev);


	device->started = 0;

	rte_smp_wmb();
}

static int
opdl_start(struct rte_eventdev *dev)
{
	int err = 0;

	if (!err)
		err = create_queues_and_rings(dev);


	if (!err)
		err = assign_internal_queue_ids(dev);


	if (!err)
		err = initialise_queue_zero_ports(dev);


	if (!err)
		err = initialise_all_other_ports(dev);


	if (!err)
		err = check_queues_linked(dev);


	if (!err)
		err = opdl_add_event_handlers(dev);


	if (!err)
		err = build_all_dependencies(dev);

	if (!err) {
		opdl_xstats_init(dev);

		struct opdl_evdev *device = opdl_pmd_priv(dev);

		PMD_DRV_LOG(INFO, "DEV_ID:[%02d] : "
			      "SUCCESS : Created %u total queues (%u ex, %u in),"
			      " %u opdls, %u event_dev ports, %u input ports",
			      opdl_pmd_dev_id(device),
			      device->nb_queues,
			      (device->nb_queues - device->nb_opdls),
			      device->nb_opdls,
			      device->nb_opdls,
			      device->nb_ports,
			      device->queue[0].nb_ports);
	} else
		opdl_stop(dev);

	return err;
}

static int
opdl_close(struct rte_eventdev *dev)
{
	struct opdl_evdev *device = opdl_pmd_priv(dev);
	uint32_t i;

	for (i = 0; i < device->max_port_nb; i++) {
		memset(&device->ports[i],
		       0,
		       sizeof(struct opdl_port));
	}

	memset(&device->s_md,
			0x0,
			sizeof(struct opdl_stage_meta_data)*OPDL_PORTS_MAX);

	memset(&device->q_md,
			0xFF,
			sizeof(struct opdl_queue_meta_data)*OPDL_MAX_QUEUES);


	memset(device->q_map_ex_to_in,
			0,
			sizeof(uint8_t)*OPDL_INVALID_QID);

	opdl_xstats_uninit(dev);

	device->max_port_nb = 0;

	device->max_queue_nb = 0;

	device->nb_opdls = 0;

	device->nb_queues   = 0;

	device->nb_ports    = 0;

	device->nb_q_md     = 0;

	dev->data->nb_queues = 0;

	dev->data->nb_ports = 0;


	return 0;
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
set_do_validation(const char *key __rte_unused, const char *value, void *opaque)
{
	int *do_val = opaque;
	*do_val = atoi(value);
	if (*do_val != 0)
		*do_val = 1;

	return 0;
}
static int
set_do_test(const char *key __rte_unused, const char *value, void *opaque)
{
	int *do_test = opaque;

	*do_test = atoi(value);

	if (*do_test != 0)
		*do_test = 1;
	return 0;
}

static int
opdl_probe(struct rte_vdev_device *vdev)
{
	static const struct rte_eventdev_ops evdev_opdl_ops = {
		.dev_configure = opdl_dev_configure,
		.dev_infos_get = opdl_info_get,
		.dev_close = opdl_close,
		.dev_start = opdl_start,
		.dev_stop = opdl_stop,
		.dump = opdl_dump,

		.xstats_get = opdl_xstats_get,
		.xstats_get_names = opdl_xstats_get_names,
		.xstats_get_by_name = opdl_xstats_get_by_name,
		.xstats_reset = opdl_xstats_reset,
	};

	static const char *const args[] = {
		NUMA_NODE_ARG,
		DO_VALIDATION_ARG,
		DO_TEST_ARG,
		NULL
	};
	const char *name;
	const char *params;
	struct rte_eventdev *dev;
	struct opdl_evdev *opdl;
	int socket_id = rte_socket_id();
	int do_validation = 0;
	int do_test = 0;
	int str_len;
	int test_result = 0;

	name = rte_vdev_device_name(vdev);
	params = rte_vdev_device_args(vdev);
	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (!kvlist) {
			PMD_DRV_LOG(INFO,
					"Ignoring unsupported parameters when creating device '%s'\n",
					name);
		} else {
			int ret = rte_kvargs_process(kvlist, NUMA_NODE_ARG,
					assign_numa_node, &socket_id);
			if (ret != 0) {
				PMD_DRV_LOG(ERR,
						"%s: Error parsing numa node parameter",
						name);

				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DO_VALIDATION_ARG,
					set_do_validation, &do_validation);
			if (ret != 0) {
				PMD_DRV_LOG(ERR,
					"%s: Error parsing do validation parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DO_TEST_ARG,
					set_do_test, &do_test);
			if (ret != 0) {
				PMD_DRV_LOG(ERR,
					"%s: Error parsing do test parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			rte_kvargs_free(kvlist);
		}
	}
	dev = rte_event_pmd_vdev_init(name,
			sizeof(struct opdl_evdev), socket_id);

	if (dev == NULL) {
		PMD_DRV_LOG(ERR, "eventdev vdev init() failed");
		return -EFAULT;
	}

	PMD_DRV_LOG(INFO, "DEV_ID:[%02d] : "
		      "Success - creating eventdev device %s, numa_node:[%d], do_valdation:[%s]"
			  " , self_test:[%s]\n",
		      dev->data->dev_id,
		      name,
		      socket_id,
		      (do_validation ? "true" : "false"),
			  (do_test ? "true" : "false"));

	dev->dev_ops = &evdev_opdl_ops;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	opdl = dev->data->dev_private;
	opdl->data = dev->data;
	opdl->socket = socket_id;
	opdl->do_validation = do_validation;
	opdl->do_test = do_test;
	str_len = strlen(name);
	memcpy(opdl->service_name, name, str_len);

	return test_result;
}

static int
opdl_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	PMD_DRV_LOG(INFO, "Closing eventdev opdl device %s\n", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver evdev_opdl_pmd_drv = {
	.probe = opdl_probe,
	.remove = opdl_remove
};

RTE_INIT(opdl_init_log);

static void
opdl_init_log(void)
{
	opdl_logtype_driver = rte_log_register("eventdev.opdl.driver");
	if (opdl_logtype_driver >= 0)
		rte_log_set_level(opdl_logtype_driver, RTE_LOG_INFO);
}


RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_OPDL_PMD, evdev_opdl_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(event_opdl, NUMA_NODE_ARG "=<int>"
			      DO_VALIDATION_ARG "=<int>" DO_TEST_ARG "=<int>");
