/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_ETH_SOFTNIC_H__
#define __INCLUDE_RTE_ETH_SOFTNIC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOFTNIC_SOFT_TM_NB_QUEUES
#define SOFTNIC_SOFT_TM_NB_QUEUES			65536
#endif

#ifndef SOFTNIC_SOFT_TM_QUEUE_SIZE
#define SOFTNIC_SOFT_TM_QUEUE_SIZE			64
#endif

#ifndef SOFTNIC_SOFT_TM_ENQ_BSZ
#define SOFTNIC_SOFT_TM_ENQ_BSZ				32
#endif

#ifndef SOFTNIC_SOFT_TM_DEQ_BSZ
#define SOFTNIC_SOFT_TM_DEQ_BSZ				24
#endif

#ifndef SOFTNIC_HARD_TX_QUEUE_ID
#define SOFTNIC_HARD_TX_QUEUE_ID			0
#endif

/**
 * Run the traffic management function on the softnic device
 *
 * This function read the packets from the softnic input queues, insert into
 * QoS scheduler queues based on mbuf sched field value and transmit the
 * scheduled packets out through the hard device interface.
 *
 * @param portid
 *    port id of the soft device.
 * @return
 *    zero.
 */

int
rte_pmd_softnic_run(uint16_t port_id);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_ETH_SOFTNIC_H__ */
