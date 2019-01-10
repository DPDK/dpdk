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

/** TCP connection port (0 = no connectivity). */
#ifndef SOFTNIC_CONN_PORT
#define SOFTNIC_CONN_PORT                                  0
#endif

/** NUMA node ID. */
#ifndef SOFTNIC_CPU_ID
#define SOFTNIC_CPU_ID                                     0
#endif

/**
 * Service cores:
 *
 * 0 = The current device is run explicitly by the application. The firmware
 *     creates one or several pipelines for the current device and maps them to
 *     CPU cores that should not be service cores. The application is required
 *     to call rte_pmd_softnic_run() for the current device on each of these CPU
 *     cores in order to make the current device work.
 *
 * 1 = The current device is run on the service cores transparently to the
 *     application. The firmware creates one or several pipelines for the
 *     current device and maps them to CPU cores that should be service cores.
 *     Each of these service cores is calling rte_pmd_softnic_run() for the
 *     current device in order to make the current device work. The application
 *     is not allowed to call rte_pmd_softnic_run() for the current device.
 */
#ifndef SOFTNIC_SC
#define SOFTNIC_SC                                         1
#endif

/** Traffic Manager: Number of scheduler queues. */
#ifndef SOFTNIC_TM_N_QUEUES
#define SOFTNIC_TM_N_QUEUES                                (64 * 1024)
#endif

/** Traffic Manager: Scheduler queue size (per traffic class). */
#ifndef SOFTNIC_TM_QUEUE_SIZE
#define SOFTNIC_TM_QUEUE_SIZE                              64
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

/**
 * Soft NIC manage.
 *
 * @param port_id
 *    Port ID of the Soft NIC device.
 * @return
 *    Zero on success, error code otherwise.
 */
int __rte_experimental
rte_pmd_softnic_manage(uint16_t port_id);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_H__ */
