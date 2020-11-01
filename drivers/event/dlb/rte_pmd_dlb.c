/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "rte_eventdev.h"
#include "rte_eventdev_pmd.h"
#include "rte_pmd_dlb.h"
#include "dlb_priv.h"
#include "dlb_inline_fns.h"

int
rte_pmd_dlb_set_token_pop_mode(uint8_t dev_id,
			       uint8_t port_id,
			       enum dlb_token_pop_mode mode)
{
	struct dlb_eventdev *dlb;
	struct rte_eventdev *dev;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_eventdevs[dev_id];

	dlb = dlb_pmd_priv(dev);

	if (mode >= NUM_TOKEN_POP_MODES)
		return -EINVAL;

	/* The event device must be configured, but not yet started */
	if (!dlb->configured || dlb->run_state != DLB_RUN_STATE_STOPPED)
		return -EINVAL;

	/* The token pop mode must be set before configuring the port */
	if (port_id >= dlb->num_ports || dlb->ev_ports[port_id].setup_done)
		return -EINVAL;

	dlb->ev_ports[port_id].qm_port.token_pop_mode = mode;

	return 0;
}
