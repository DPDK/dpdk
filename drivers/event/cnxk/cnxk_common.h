/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CNXK_COMMON_H__
#define __CNXK_COMMON_H__

#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

static uint32_t
cnxk_sso_hws_prf_wdata(struct cnxk_sso_evdev *dev)
{
	uint32_t wdata = 1;

	if (dev->deq_tmo_ns)
		wdata |= BIT(16);

	switch (dev->gw_mode) {
	case CNXK_GW_MODE_NONE:
	default:
		break;
	case CNXK_GW_MODE_PREF:
		wdata |= BIT(19);
		break;
	case CNXK_GW_MODE_PREF_WFE:
		wdata |= BIT(20) | BIT(19);
		break;
	}

	return wdata;
}

static uint8_t
cnxk_sso_hws_preschedule_get(uint8_t preschedule_type)
{
	uint8_t gw_mode = 0;

	switch (preschedule_type) {
	default:
	case RTE_EVENT_PRESCHEDULE_NONE:
		gw_mode = CNXK_GW_MODE_NONE;
		break;
	case RTE_EVENT_PRESCHEDULE:
		gw_mode = CNXK_GW_MODE_PREF;
		break;
	case RTE_EVENT_PRESCHEDULE_ADAPTIVE:
		gw_mode = CNXK_GW_MODE_PREF_WFE;
		break;
	}

	return gw_mode;
}

#endif /* __CNXK_COMMON_H__ */
