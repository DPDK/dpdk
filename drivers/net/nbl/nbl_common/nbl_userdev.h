/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_USERDEV_H_
#define _NBL_USERDEV_H_

#include "nbl_ethdev.h"
#include "nbl_common.h"

#define NBL_USERDEV_INIT_COMMON(common) do {		\
	typeof(common) _comm = (common);		\
	_comm->devfd = -1;				\
	_comm->eventfd = -1;				\
	_comm->specific_dma = false;			\
	_comm->dma_set_msb = false;			\
	_comm->ifindex = -1;				\
	_comm->nl_socket_route = -1;			\
} while (0)

#endif
