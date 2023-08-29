/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_common.h>
#include <rte_pmd_cnxk_eventdev.h>
#include <rte_eventdev.h>

#include "roc_platform.h"
#include "roc_sso.h"
#include "roc_sso_dp.h"

struct pwords {
	uint64_t u[5];
};

void
rte_pmd_cnxk_eventdev_wait_head(uint8_t dev, uint8_t port)
{
	struct pwords *w = rte_event_fp_ops[dev].data[port];
	uint8_t vws;

	if (w->u[1] & 0x3) {
		roc_sso_hws_head_wait(w->u[0]);
	} else {
		/* Dual WS case */
		vws = (w->u[4] >> 8) & 0x1;
		roc_sso_hws_head_wait(w->u[vws]);
	}
}


uint8_t
rte_pmd_cnxk_eventdev_is_head(uint8_t dev, uint8_t port)
{
	struct pwords *w = rte_event_fp_ops[dev].data[port];
	uintptr_t base;
	uint8_t vws;

	if (w->u[1] & 0x3) {
		base = w->u[0];
	} else {
		/* Dual WS case */
		vws = (w->u[4] >> 8) & 0x1;
		base = w->u[vws];
	}

	return roc_sso_hws_is_head(base);
}
