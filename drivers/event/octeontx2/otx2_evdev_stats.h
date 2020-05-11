/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_EVDEV_STATS_H__
#define __OTX2_EVDEV_STATS_H__

#include "otx2_evdev.h"

struct otx2_sso_xstats_name {
	const char name[RTE_EVENT_DEV_XSTATS_NAME_SIZE];
	const size_t offset;
	const uint64_t mask;
	const uint8_t shift;
	uint64_t reset_snap[OTX2_SSO_MAX_VHGRP];
};

static struct otx2_sso_xstats_name sso_hws_xstats[] = {
	{"last_grp_serviced",	offsetof(struct sso_hws_stats, arbitration),
				0x3FF, 0, {0} },
	{"affinity_arbitration_credits",
				offsetof(struct sso_hws_stats, arbitration),
				0xF, 16, {0} },
};

static struct otx2_sso_xstats_name sso_grp_xstats[] = {
	{"wrk_sched",		offsetof(struct sso_grp_stats, ws_pc), ~0x0, 0,
				{0} },
	{"xaq_dram",		offsetof(struct sso_grp_stats, ext_pc), ~0x0,
				0, {0} },
	{"add_wrk",		offsetof(struct sso_grp_stats, wa_pc), ~0x0, 0,
				{0} },
	{"tag_switch_req",	offsetof(struct sso_grp_stats, ts_pc), ~0x0, 0,
				{0} },
	{"desched_req",		offsetof(struct sso_grp_stats, ds_pc), ~0x0, 0,
				{0} },
	{"desched_wrk",		offsetof(struct sso_grp_stats, dq_pc), ~0x0, 0,
				{0} },
	{"xaq_cached",		offsetof(struct sso_grp_stats, aw_status), 0x3,
				0, {0} },
	{"work_inflight",	offsetof(struct sso_grp_stats, aw_status), 0x3F,
				16, {0} },
	{"inuse_pages",		offsetof(struct sso_grp_stats, page_cnt),
				0xFFFFFFFF, 0, {0} },
};

#define OTX2_SSO_NUM_HWS_XSTATS RTE_DIM(sso_hws_xstats)
#define OTX2_SSO_NUM_GRP_XSTATS RTE_DIM(sso_grp_xstats)

#define OTX2_SSO_NUM_XSTATS (OTX2_SSO_NUM_HWS_XSTATS + OTX2_SSO_NUM_GRP_XSTATS)

static int
otx2_sso_xstats_get(const struct rte_eventdev *event_dev,
		    enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		    const unsigned int ids[], uint64_t values[], unsigned int n)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_sso_xstats_name *xstats;
	struct otx2_sso_xstats_name *xstat;
	struct otx2_mbox *mbox = dev->mbox;
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;
	unsigned int i;
	uint64_t value;
	void *req_rsp;
	int rc;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		return 0;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)dev->nb_event_ports)
			goto invalid_value;

		xstats_mode_count = OTX2_SSO_NUM_HWS_XSTATS;
		xstats = sso_hws_xstats;

		req_rsp = otx2_mbox_alloc_msg_sso_hws_get_stats(mbox);
			((struct sso_info_req *)req_rsp)->hws = dev->dual_ws ?
					2 * queue_port_id : queue_port_id;
		rc = otx2_mbox_process_msg(mbox, (void **)&req_rsp);
		if (rc < 0)
			goto invalid_value;

		if (dev->dual_ws) {
			for (i = 0; i < n && i < xstats_mode_count; i++) {
				xstat = &xstats[ids[i] - start_offset];
				values[i] = *(uint64_t *)
					((char *)req_rsp + xstat->offset);
				values[i] = (values[i] >> xstat->shift) &
					xstat->mask;
			}

			req_rsp = otx2_mbox_alloc_msg_sso_hws_get_stats(mbox);
			((struct sso_info_req *)req_rsp)->hws =
					(2 * queue_port_id) + 1;
			rc = otx2_mbox_process_msg(mbox, (void **)&req_rsp);
			if (rc < 0)
				goto invalid_value;
		}

		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)dev->nb_event_queues)
			goto invalid_value;

		xstats_mode_count = OTX2_SSO_NUM_GRP_XSTATS;
		start_offset = OTX2_SSO_NUM_HWS_XSTATS;
		xstats = sso_grp_xstats;

		req_rsp = otx2_mbox_alloc_msg_sso_grp_get_stats(mbox);
			((struct sso_info_req *)req_rsp)->grp = queue_port_id;
		rc = otx2_mbox_process_msg(mbox, (void **)&req_rsp);
		if (rc < 0)
			goto invalid_value;

		break;
	default:
		otx2_err("Invalid mode received");
		goto invalid_value;
	};

	for (i = 0; i < n && i < xstats_mode_count; i++) {
		xstat = &xstats[ids[i] - start_offset];
		value = *(uint64_t *)((char *)req_rsp + xstat->offset);
		value = (value >> xstat->shift) & xstat->mask;

		if ((mode == RTE_EVENT_DEV_XSTATS_PORT) && dev->dual_ws)
			values[i] += value;
		else
			values[i] = value;

		values[i] -= xstat->reset_snap[queue_port_id];
	}

	return i;
invalid_value:
	return -EINVAL;
}

static int
otx2_sso_xstats_reset(struct rte_eventdev *event_dev,
		      enum rte_event_dev_xstats_mode mode,
		      int16_t queue_port_id, const uint32_t ids[], uint32_t n)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_sso_xstats_name *xstats;
	struct otx2_sso_xstats_name *xstat;
	struct otx2_mbox *mbox = dev->mbox;
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;
	unsigned int i;
	uint64_t value;
	void *req_rsp;
	int rc;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		return 0;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)dev->nb_event_ports)
			goto invalid_value;

		xstats_mode_count = OTX2_SSO_NUM_HWS_XSTATS;
		xstats = sso_hws_xstats;

		req_rsp = otx2_mbox_alloc_msg_sso_hws_get_stats(mbox);
		((struct sso_info_req *)req_rsp)->hws = dev->dual_ws ?
			2 * queue_port_id : queue_port_id;
		rc = otx2_mbox_process_msg(mbox, (void **)&req_rsp);
		if (rc < 0)
			goto invalid_value;

		if (dev->dual_ws) {
			for (i = 0; i < n && i < xstats_mode_count; i++) {
				xstat = &xstats[ids[i] - start_offset];
				xstat->reset_snap[queue_port_id] = *(uint64_t *)
					((char *)req_rsp + xstat->offset);
				xstat->reset_snap[queue_port_id] =
					(xstat->reset_snap[queue_port_id] >>
						xstat->shift) & xstat->mask;
			}

			req_rsp = otx2_mbox_alloc_msg_sso_hws_get_stats(mbox);
			((struct sso_info_req *)req_rsp)->hws =
					(2 * queue_port_id) + 1;
			rc = otx2_mbox_process_msg(mbox, (void **)&req_rsp);
			if (rc < 0)
				goto invalid_value;
		}

		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)dev->nb_event_queues)
			goto invalid_value;

		xstats_mode_count = OTX2_SSO_NUM_GRP_XSTATS;
		start_offset = OTX2_SSO_NUM_HWS_XSTATS;
		xstats = sso_grp_xstats;

		req_rsp = otx2_mbox_alloc_msg_sso_grp_get_stats(mbox);
			((struct sso_info_req *)req_rsp)->grp = queue_port_id;
		rc = otx2_mbox_process_msg(mbox, (void *)&req_rsp);
		if (rc < 0)
			goto invalid_value;

		break;
	default:
		otx2_err("Invalid mode received");
		goto invalid_value;
	};

	for (i = 0; i < n && i < xstats_mode_count; i++) {
		xstat = &xstats[ids[i] - start_offset];
		value = *(uint64_t *)((char *)req_rsp + xstat->offset);
		value = (value >> xstat->shift) & xstat->mask;

		if ((mode == RTE_EVENT_DEV_XSTATS_PORT) && dev->dual_ws)
			xstat->reset_snap[queue_port_id] += value;
		else
			xstat->reset_snap[queue_port_id] =  value;
	}
	return i;
invalid_value:
	return -EINVAL;
}

static int
otx2_sso_xstats_get_names(const struct rte_eventdev *event_dev,
			  enum rte_event_dev_xstats_mode mode,
			  uint8_t queue_port_id,
			  struct rte_event_dev_xstats_name *xstats_names,
			  unsigned int *ids, unsigned int size)
{
	struct rte_event_dev_xstats_name xstats_names_copy[OTX2_SSO_NUM_XSTATS];
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;
	unsigned int xidx = 0;
	unsigned int i;

	for (i = 0; i < OTX2_SSO_NUM_HWS_XSTATS; i++) {
		snprintf(xstats_names_copy[i].name,
			 sizeof(xstats_names_copy[i].name), "%s",
			 sso_hws_xstats[i].name);
	}

	for (; i < OTX2_SSO_NUM_XSTATS; i++) {
		snprintf(xstats_names_copy[i].name,
			 sizeof(xstats_names_copy[i].name), "%s",
			 sso_grp_xstats[i - OTX2_SSO_NUM_HWS_XSTATS].name);
	}

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)dev->nb_event_ports)
			break;
		xstats_mode_count = OTX2_SSO_NUM_HWS_XSTATS;
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)dev->nb_event_queues)
			break;
		xstats_mode_count = OTX2_SSO_NUM_GRP_XSTATS;
		start_offset = OTX2_SSO_NUM_HWS_XSTATS;
		break;
	default:
		otx2_err("Invalid mode received");
		return -EINVAL;
	};

	if (xstats_mode_count > size || !ids || !xstats_names)
		return xstats_mode_count;

	for (i = 0; i < xstats_mode_count; i++) {
		xidx = i + start_offset;
		strncpy(xstats_names[i].name, xstats_names_copy[xidx].name,
			sizeof(xstats_names[i].name));
		ids[i] = xidx;
	}

	return i;
}

#endif
