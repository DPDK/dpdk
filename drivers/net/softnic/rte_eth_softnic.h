/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_ETH_SOFTNIC_H__
#define __INCLUDE_RTE_ETH_SOFTNIC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Firmware. */
#ifndef SOFTNIC_FIRMWARE
#define SOFTNIC_FIRMWARE                                   "firmware.cli"
#endif

/** NUMA node ID. */
#ifndef SOFTNIC_CPU_ID
#define SOFTNIC_CPU_ID                                     0
#endif

/**
 * Soft NIC run.
 *
 * @param port_id
 *    Port ID of the Soft NIC device.
 * @return
 *    Zero on success, error code otherwise.
 */

int
rte_pmd_softnic_run(uint16_t port_id);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_H__ */
