/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_eventdev.h"
#include "cnxk_eventdev_dp.h"

struct cnxk_sso_xstats_name {
	const char name[RTE_EVENT_DEV_XSTATS_NAME_SIZE];
	const size_t offset;
	const uint64_t mask;
	const uint8_t shift;
	uint64_t reset_snap[CNXK_SSO_MAX_HWGRP];
};

static struct cnxk_sso_xstats_name sso_hws_xstats[] = {
	{
		"last_grp_serviced",
		offsetof(struct roc_sso_hws_stats, arbitration),
		0x3FF,
		0,
		{0},
	},
	{
		"affinity_arbitration_credits",
		offsetof(struct roc_sso_hws_stats, arbitration),
		0xF,
		16,
		{0},
	},
};

static struct cnxk_sso_xstats_name sso_hwgrp_xstats[] = {
	{
		"wrk_sched",
		offsetof(struct roc_sso_hwgrp_stats, ws_pc),
		~0x0,
		0,
		{0},
	},
	{
		"xaq_dram",
		offsetof(struct roc_sso_hwgrp_stats, ext_pc),
		~0x0,
		0,
		{0},
	},
	{
		"add_wrk",
		offsetof(struct roc_sso_hwgrp_stats, wa_pc),
		~0x0,
		0,
		{0},
	},
	{
		"tag_switch_req",
		offsetof(struct roc_sso_hwgrp_stats, ts_pc),
		~0x0,
		0,
		{0},
	},
	{
		"desched_req",
		offsetof(struct roc_sso_hwgrp_stats, ds_pc),
		~0x0,
		0,
		{0},
	},
	{
		"desched_wrk",
		offsetof(struct roc_sso_hwgrp_stats, dq_pc),
		~0x0,
		0,
		{0},
	},
	{
		"xaq_cached",
		offsetof(struct roc_sso_hwgrp_stats, aw_status),
		0x3,
		0,
		{0},
	},
	{
		"work_inflight",
		offsetof(struct roc_sso_hwgrp_stats, aw_status),
		0x3F,
		16,
		{0},
	},
	{
		"inuse_pages",
		offsetof(struct roc_sso_hwgrp_stats, page_cnt),
		0xFFFFFFFF,
		0,
		{0},
	},
};

#define CNXK_SSO_NUM_HWS_XSTATS RTE_DIM(sso_hws_xstats)
#define CNXK_SSO_NUM_GRP_XSTATS RTE_DIM(sso_hwgrp_xstats)

#define CNXK_SSO_NUM_XSTATS (CNXK_SSO_NUM_HWS_XSTATS + CNXK_SSO_NUM_GRP_XSTATS)

int
cnxk_sso_xstats_get(const struct rte_eventdev *event_dev,
		    enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		    const uint64_t ids[], uint64_t values[], unsigned int n)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_stats hwgrp_stats;
	struct cnxk_sso_xstats_name *xstats;
	struct cnxk_sso_xstats_name *xstat;
	struct roc_sso_hws_stats hws_stats;
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;
	unsigned int i;
	uint64_t value;
	void *rsp;
	int rc;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		return 0;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)dev->nb_event_ports)
			goto invalid_value;

		xstats_mode_count = CNXK_SSO_NUM_HWS_XSTATS;
		xstats = sso_hws_xstats;

		rc = roc_sso_hws_stats_get(&dev->sso, queue_port_id,
					   &hws_stats);
		if (rc < 0)
			goto invalid_value;
		rsp = &hws_stats;
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)dev->nb_event_queues)
			goto invalid_value;

		xstats_mode_count = CNXK_SSO_NUM_GRP_XSTATS;
		start_offset = CNXK_SSO_NUM_HWS_XSTATS;
		xstats = sso_hwgrp_xstats;

		rc = roc_sso_hwgrp_stats_get(&dev->sso, queue_port_id,
					     &hwgrp_stats);
		if (rc < 0)
			goto invalid_value;
		rsp = &hwgrp_stats;

		break;
	default:
		plt_err("Invalid mode received");
		goto invalid_value;
	};

	for (i = 0; i < n && i < xstats_mode_count; i++) {
		xstat = &xstats[ids[i] - start_offset];
		value = *(uint64_t *)((char *)rsp + xstat->offset);
		value = (value >> xstat->shift) & xstat->mask;

		values[i] = value;
		values[i] -= xstat->reset_snap[queue_port_id];
	}

	return i;
invalid_value:
	return -EINVAL;
}

int
cnxk_sso_xstats_reset(struct rte_eventdev *event_dev,
		      enum rte_event_dev_xstats_mode mode,
		      int16_t queue_port_id, const uint64_t ids[], uint32_t n)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_stats hwgrp_stats;
	struct cnxk_sso_xstats_name *xstats;
	struct cnxk_sso_xstats_name *xstat;
	struct roc_sso_hws_stats hws_stats;
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;
	unsigned int i;
	uint64_t value;
	void *rsp;
	int rc;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		return 0;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)dev->nb_event_ports)
			goto invalid_value;

		xstats_mode_count = CNXK_SSO_NUM_HWS_XSTATS;
		xstats = sso_hws_xstats;
		rc = roc_sso_hws_stats_get(&dev->sso, queue_port_id,
					   &hws_stats);
		if (rc < 0)
			goto invalid_value;
		rsp = &hws_stats;
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)dev->nb_event_queues)
			goto invalid_value;

		xstats_mode_count = CNXK_SSO_NUM_GRP_XSTATS;
		start_offset = CNXK_SSO_NUM_HWS_XSTATS;
		xstats = sso_hwgrp_xstats;

		rc = roc_sso_hwgrp_stats_get(&dev->sso, queue_port_id,
					     &hwgrp_stats);
		if (rc < 0)
			goto invalid_value;
		rsp = &hwgrp_stats;
		break;
	default:
		plt_err("Invalid mode received");
		goto invalid_value;
	};

	for (i = 0; i < n && i < xstats_mode_count; i++) {
		xstat = &xstats[ids[i] - start_offset];
		value = *(uint64_t *)((char *)rsp + xstat->offset);
		value = (value >> xstat->shift) & xstat->mask;

		xstat->reset_snap[queue_port_id] = value;
	}
	return i;
invalid_value:
	return -EINVAL;
}

int
cnxk_sso_xstats_get_names(const struct rte_eventdev *event_dev,
			  enum rte_event_dev_xstats_mode mode,
			  uint8_t queue_port_id,
			  struct rte_event_dev_xstats_name *xstats_names,
			  uint64_t *ids, unsigned int size)
{
	struct rte_event_dev_xstats_name xstats_names_copy[CNXK_SSO_NUM_XSTATS];
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;
	unsigned int xidx = 0;
	unsigned int i;

	for (i = 0; i < CNXK_SSO_NUM_HWS_XSTATS; i++) {
		snprintf(xstats_names_copy[i].name,
			 sizeof(xstats_names_copy[i].name), "%s",
			 sso_hws_xstats[i].name);
	}

	for (; i < CNXK_SSO_NUM_XSTATS; i++) {
		snprintf(xstats_names_copy[i].name,
			 sizeof(xstats_names_copy[i].name), "%s",
			 sso_hwgrp_xstats[i - CNXK_SSO_NUM_HWS_XSTATS].name);
	}

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)dev->nb_event_ports)
			break;
		xstats_mode_count = CNXK_SSO_NUM_HWS_XSTATS;
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)dev->nb_event_queues)
			break;
		xstats_mode_count = CNXK_SSO_NUM_GRP_XSTATS;
		start_offset = CNXK_SSO_NUM_HWS_XSTATS;
		break;
	default:
		plt_err("Invalid mode received");
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
