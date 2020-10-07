/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_SYM_REQ_H_
#define _BCMFS_SYM_REQ_H_

#include "bcmfs_dev_msg.h"

/*
 * This structure hold the supportive data required to process a
 * rte_crypto_op
 */
struct bcmfs_sym_request {
	/* bcmfs qp message for h/w queues to process */
	struct bcmfs_qp_message msgs;
	/* crypto op */
	struct rte_crypto_op *op;
};

#endif /* _BCMFS_SYM_REQ_H_ */
