/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_evdev.h"

void
sso_updt_xae_cnt(struct otx2_sso_evdev *dev, void *data, uint32_t event_type)
{
	switch (event_type) {
	case RTE_EVENT_TYPE_TIMER:
	{
		dev->adptr_xae_cnt += (*(uint64_t *)data);
		break;
	}
	default:
		break;
	}
}
