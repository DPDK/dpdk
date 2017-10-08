/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
 *     * Neither the name of Cavium networks nor the names of its
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
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_alarm.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_devargs.h>
#include <rte_dev.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_vdev.h>

#include "octeontx_ethdev.h"
#include "octeontx_logs.h"

struct octeontx_vdev_init_params {
	uint8_t	nr_port;
};

/* Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = (int *)extra_args;

	*i = atoi(value);
	if (*i < 0) {
		octeontx_log_err("argument has to be positive.");
		return -1;
	}

	return 0;
}

static int
octeontx_parse_vdev_init_params(struct octeontx_vdev_init_params *params,
				struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	static const char * const octeontx_vdev_valid_params[] = {
		OCTEONTX_VDEV_NR_PORT_ARG,
		NULL
	};

	const char *input_args = rte_vdev_device_args(dev);
	if (params == NULL)
		return -EINVAL;


	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				octeontx_vdev_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
					OCTEONTX_VDEV_NR_PORT_ARG,
					&parse_integer_arg,
					&params->nr_port);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static inline void
devconf_set_default_sane_values(struct rte_event_dev_config *dev_conf,
				struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;

	dev_conf->nb_event_ports = info->max_event_ports;
	dev_conf->nb_event_queues = info->max_event_queues;

	dev_conf->nb_event_queue_flows = info->max_event_queue_flows;
	dev_conf->nb_event_port_dequeue_depth =
			info->max_event_port_dequeue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_events_limit =
			info->max_num_events;
}

/* Create Ethdev interface per BGX LMAC ports */
static int
octeontx_create(struct rte_vdev_device *dev, int port, uint8_t evdev,
			int socket_id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(port);
	RTE_SET_USED(evdev);
	RTE_SET_USED(socket_id);

	return -ENODEV;
}

/* Un initialize octeontx device */
static int
octeontx_remove(struct rte_vdev_device *dev)
{
	char octtx_name[OCTEONTX_MAX_NAME_LEN];
	struct rte_eth_dev *eth_dev = NULL;
	struct octeontx_nic *nic = NULL;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT; i++) {
		sprintf(octtx_name, "eth_octeontx_%d", i);

		/* reserve an ethdev entry */
		eth_dev = rte_eth_dev_allocated(octtx_name);
		if (eth_dev == NULL)
			return -ENODEV;

		nic = octeontx_pmd_priv(eth_dev);
		rte_event_dev_stop(nic->evdev);
		PMD_INIT_LOG(INFO, "Closing octeontx device %s", octtx_name);

		rte_free(eth_dev->data->mac_addrs);
		rte_free(eth_dev->data->dev_private);
		rte_free(eth_dev->data);
		rte_eth_dev_release_port(eth_dev);
		rte_event_dev_close(nic->evdev);
	}

	/* Free FC resource */
	octeontx_pko_fc_free();

	return 0;
}

/* Initialize octeontx device */
static int
octeontx_probe(struct rte_vdev_device *dev)
{
	const char *dev_name;
	static int probe_once;
	uint8_t socket_id, qlist;
	int tx_vfcnt, port_id, evdev, qnum, pnum, res, i;
	struct rte_event_dev_config dev_conf;
	const char *eventdev_name = "event_octeontx";
	struct rte_event_dev_info info;

	struct octeontx_vdev_init_params init_params = {
		OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT
	};

	dev_name = rte_vdev_device_name(dev);
	res = octeontx_parse_vdev_init_params(&init_params, dev);
	if (res < 0)
		return -EINVAL;

	if (init_params.nr_port > OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT) {
		octeontx_log_err("nr_port (%d) > max (%d)", init_params.nr_port,
				OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT);
		return -ENOTSUP;
	}

	PMD_INIT_LOG(DEBUG, "initializing %s pmd", dev_name);

	socket_id = rte_socket_id();

	tx_vfcnt = octeontx_pko_vf_count();

	if (tx_vfcnt < init_params.nr_port) {
		octeontx_log_err("not enough PKO (%d) for port number (%d)",
				tx_vfcnt, init_params.nr_port);
		return -EINVAL;
	}
	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		octeontx_log_err("eventdev %s not found", eventdev_name);
		return -ENODEV;
	}

	res = rte_event_dev_info_get(evdev, &info);
	if (res < 0) {
		octeontx_log_err("failed to eventdev info %d", res);
		return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "max_queue %d max_port %d",
			info.max_event_queues, info.max_event_ports);

	if (octeontx_pko_init_fc(tx_vfcnt))
		return -ENOMEM;

	devconf_set_default_sane_values(&dev_conf, &info);
	res = rte_event_dev_configure(evdev, &dev_conf);
	if (res < 0)
		goto parse_error;

	rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_PORT_COUNT,
			(uint32_t *)&pnum);
	rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			(uint32_t *)&qnum);
	if (pnum < qnum) {
		octeontx_log_err("too few event ports (%d) for event_q(%d)",
				pnum, qnum);
		res = -EINVAL;
		goto parse_error;
	}
	if (pnum > qnum) {
		/*
		 * We don't poll on event ports
		 * that do not have any queues assigned.
		 */
		pnum = qnum;
		PMD_INIT_LOG(INFO,
			"reducing number of active event ports to %d", pnum);
	}
	for (i = 0; i < qnum; i++) {
		res = rte_event_queue_setup(evdev, i, NULL);
		if (res < 0) {
			octeontx_log_err("failed to setup event_q(%d): res %d",
					i, res);
			goto parse_error;
		}
	}

	for (i = 0; i < pnum; i++) {
		res = rte_event_port_setup(evdev, i, NULL);
		if (res < 0) {
			res = -ENODEV;
			octeontx_log_err("failed to setup ev port(%d) res=%d",
						i, res);
			goto parse_error;
		}
		/* Link one queue to one event port */
		qlist = i;
		res = rte_event_port_link(evdev, i, &qlist, NULL, 1);
		if (res < 0) {
			res = -ENODEV;
			octeontx_log_err("failed to link port (%d): res=%d",
					i, res);
			goto parse_error;
		}
	}

	/* Create ethdev interface */
	for (i = 0; i < init_params.nr_port; i++) {
		port_id = octeontx_create(dev, i, evdev, socket_id);
		if (port_id < 0) {
			octeontx_log_err("failed to create device %s",
					dev_name);
			res = -ENODEV;
			goto parse_error;
		}

		PMD_INIT_LOG(INFO, "created ethdev %s for port %d", dev_name,
					port_id);
	}

	if (probe_once) {
		octeontx_log_err("interface %s not supported", dev_name);
		octeontx_remove(dev);
		res = -ENOTSUP;
		goto parse_error;
	}
	probe_once = 1;

	return 0;

parse_error:
	octeontx_pko_fc_free();
	return res;
}

static struct rte_vdev_driver octeontx_pmd_drv = {
	.probe = octeontx_probe,
	.remove = octeontx_remove,
};

RTE_PMD_REGISTER_VDEV(OCTEONTX_PMD, octeontx_pmd_drv);
RTE_PMD_REGISTER_ALIAS(OCTEONTX_PMD, eth_octeontx);
RTE_PMD_REGISTER_PARAM_STRING(OCTEONTX_PMD, "nr_port=<int> ");
