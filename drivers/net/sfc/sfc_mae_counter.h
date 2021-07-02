/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#ifndef _SFC_MAE_COUNTER_H
#define _SFC_MAE_COUNTER_H

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default values for a user of counter RxQ */
#define SFC_MAE_COUNTER_RX_BURST 32
#define SFC_COUNTER_RXQ_RX_DESC_COUNT 256

/*
 * The refill level is chosen based on requirement to keep number
 * of give credits operations low.
 */
#define SFC_COUNTER_RXQ_REFILL_LEVEL (SFC_COUNTER_RXQ_RX_DESC_COUNT / 4)

/*
 * SF-122415-TC states that the packetiser that generates packets for
 * counter stream must support 9k frames. Set it to the maximum supported
 * size since in case of huge flow of counters, having fewer packets in counter
 * updates is better.
 */
#define SFC_MAE_COUNTER_STREAM_PACKET_SIZE 9216

bool sfc_mae_counter_rxq_required(struct sfc_adapter *sa);

int sfc_mae_counter_rxq_attach(struct sfc_adapter *sa);
void sfc_mae_counter_rxq_detach(struct sfc_adapter *sa);

int sfc_mae_counter_rxq_init(struct sfc_adapter *sa);
void sfc_mae_counter_rxq_fini(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_MAE_COUNTER_H */
