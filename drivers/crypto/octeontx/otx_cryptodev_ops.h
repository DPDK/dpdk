/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_OPS_H_
#define _OTX_CRYPTODEV_OPS_H_

#define OTX_CPT_MIN_HEADROOM_REQ	(24)
#define OTX_CPT_MIN_TAILROOM_REQ	(8)
#define CPT_NUM_QS_PER_VF		(1)

void
cleanup_global_resources(void);

int
otx_cpt_dev_create(struct rte_cryptodev *c_dev);

#endif /* _OTX_CRYPTODEV_OPS_H_ */
