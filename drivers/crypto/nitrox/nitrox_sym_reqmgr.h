/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_SYM_REQMGR_H_
#define _NITROX_SYM_REQMGR_H_

#include "nitrox_sym_ctx.h"

struct nitrox_qp;
struct nitrox_softreq;

int nitrox_process_se_req(uint16_t qno, struct rte_crypto_op *op,
			  struct nitrox_crypto_ctx *ctx,
			  struct nitrox_softreq *sr);
int nitrox_check_se_req(struct nitrox_softreq *sr, struct rte_crypto_op **op);
void *nitrox_sym_instr_addr(struct nitrox_softreq *sr);
struct rte_mempool *nitrox_sym_req_pool_create(struct rte_cryptodev *cdev,
					       uint32_t nobjs, uint16_t qp_id,
					       int socket_id);
void nitrox_sym_req_pool_free(struct rte_mempool *mp);

#endif /* _NITROX_SYM_REQMGR_H_ */
