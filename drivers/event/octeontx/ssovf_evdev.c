/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2017.
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

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_vdev.h>

#include "ssovf_evdev.h"

/* SSOPF Mailbox messages */

struct ssovf_mbox_dev_info {
	uint64_t min_deq_timeout_ns;
	uint64_t max_deq_timeout_ns;
	uint32_t max_num_events;
};

static int
ssovf_mbox_dev_info(struct ssovf_mbox_dev_info *info)
{
	struct octeontx_mbox_hdr hdr = {0};
	uint16_t len = sizeof(struct ssovf_mbox_dev_info);

	hdr.coproc = SSO_COPROC;
	hdr.msg = SSO_GET_DEV_INFO;
	hdr.vfid = 0;

	memset(info, 0, len);
	return octeontx_ssovf_mbox_send(&hdr, NULL, 0, info, len);
}

static int
ssovf_vdev_probe(const char *name, const char *params)
{
	struct octeontx_ssovf_info oinfo;
	struct ssovf_mbox_dev_info info;
	struct ssovf_evdev *edev;
	struct rte_eventdev *eventdev;
	static int ssovf_init_once;
	int ret;

	RTE_SET_USED(params);

	/* More than one instance is not supported */
	if (ssovf_init_once) {
		ssovf_log_err("Request to create >1 %s instance", name);
		return -EINVAL;
	}

	eventdev = rte_event_pmd_vdev_init(name, sizeof(struct ssovf_evdev),
				rte_socket_id());
	if (eventdev == NULL) {
		ssovf_log_err("Failed to create eventdev vdev %s", name);
		return -ENOMEM;
	}
	eventdev->dev_ops = NULL;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = octeontx_ssovf_info(&oinfo);
	if (ret) {
		ssovf_log_err("Failed to probe and validate ssovfs %d", ret);
		goto error;
	}

	edev = ssovf_pmd_priv(eventdev);
	edev->max_event_ports = oinfo.total_ssowvfs;
	edev->max_event_queues = oinfo.total_ssovfs;
	edev->is_timeout_deq = 0;

	ret = ssovf_mbox_dev_info(&info);
	if (ret < 0 || ret != sizeof(struct ssovf_mbox_dev_info)) {
		ssovf_log_err("Failed to get mbox devinfo %d", ret);
		goto error;
	}

	edev->min_deq_timeout_ns = info.min_deq_timeout_ns;
	edev->max_deq_timeout_ns = info.max_deq_timeout_ns;
	edev->max_num_events =  info.max_num_events;
	ssovf_log_dbg("min_deq_tmo=%"PRId64" max_deq_tmo=%"PRId64" max_evts=%d",
			info.min_deq_timeout_ns, info.max_deq_timeout_ns,
			info.max_num_events);

	if (!edev->max_event_ports || !edev->max_event_queues) {
		ssovf_log_err("Not enough eventdev resource queues=%d ports=%d",
			edev->max_event_queues, edev->max_event_ports);
		ret = -ENODEV;
		goto error;
	}

	ssovf_log_info("Initializing %s domain=%d max_queues=%d max_ports=%d",
			name, oinfo.domain, edev->max_event_queues,
			edev->max_event_ports);

	ssovf_init_once = 1;
	return 0;

error:
	rte_event_pmd_vdev_uninit(name);
	return ret;
}

static int
ssovf_vdev_remove(const char *name)
{
	ssovf_log_info("Closing %s", name);
	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_ssovf_pmd = {
	.probe = ssovf_vdev_probe,
	.remove = ssovf_vdev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_OCTEONTX_PMD, vdev_ssovf_pmd);
