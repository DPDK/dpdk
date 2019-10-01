/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_SYM_REQMGR_H_
#define _NITROX_SYM_REQMGR_H_

struct rte_mempool *nitrox_sym_req_pool_create(struct rte_cryptodev *cdev,
					       uint32_t nobjs, uint16_t qp_id,
					       int socket_id);
void nitrox_sym_req_pool_free(struct rte_mempool *mp);

#endif /* _NITROX_SYM_REQMGR_H_ */
