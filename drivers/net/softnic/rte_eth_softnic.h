/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
