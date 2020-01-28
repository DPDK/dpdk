/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>

#include "l3fwd.h"
#include "l3fwd_event.h"

void
l3fwd_event_set_internal_port_ops(struct l3fwd_event_setup_ops *ops)
{
	RTE_SET_USED(ops);
}
