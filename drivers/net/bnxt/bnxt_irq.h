/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_IRQ_H_
#define _BNXT_IRQ_H_

#define BNXT_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define BNXT_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

struct bnxt_irq {
	rte_intr_callback_fn	handler;
	unsigned int		vector;
	uint8_t			requested;
	char			name[RTE_ETH_NAME_MAX_LEN + 2];
};

struct bnxt;
void bnxt_free_int(struct bnxt *bp);
void bnxt_disable_int(struct bnxt *bp);
void bnxt_enable_int(struct bnxt *bp);
int bnxt_setup_int(struct bnxt *bp);
int bnxt_request_int(struct bnxt *bp);

#endif
