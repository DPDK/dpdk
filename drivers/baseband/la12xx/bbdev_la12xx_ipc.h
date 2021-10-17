/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */
#ifndef __BBDEV_LA12XX_IPC_H__
#define __BBDEV_LA12XX_IPC_H__

/** No. of max channel per instance */
#define IPC_MAX_DEPTH	(16)

/* This shared memory would be on the host side which have copy of some
 * of the parameters which are also part of Shared BD ring. Read access
 * of these parameters from the host side would not be over PCI.
 */
typedef struct host_ipc_params {
	volatile uint32_t pi;
	volatile uint32_t ci;
	volatile uint32_t modem_ptr[IPC_MAX_DEPTH];
} __rte_packed host_ipc_params_t;

#endif
