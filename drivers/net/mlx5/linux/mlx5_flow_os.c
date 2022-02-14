/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include "mlx5_flow_os.h"

#include <rte_thread.h>

/* Key of thread specific flow workspace data. */
static rte_thread_key key_workspace;

int
mlx5_flow_os_init_workspace_once(void)
{
	if (rte_thread_key_create(&key_workspace, flow_release_workspace)) {
		DRV_LOG(ERR, "Can't create flow workspace data thread key.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	return 0;
}

void *
mlx5_flow_os_get_specific_workspace(void)
{
	return rte_thread_value_get(key_workspace);
}

int
mlx5_flow_os_set_specific_workspace(struct mlx5_flow_workspace *data)
{
	return rte_thread_value_set(key_workspace, data);
}

void
mlx5_flow_os_release_workspace(void)
{
	rte_thread_key_delete(key_workspace);
}
