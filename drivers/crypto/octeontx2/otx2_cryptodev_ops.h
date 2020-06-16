/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_OPS_H_
#define _OTX2_CRYPTODEV_OPS_H_

#include <rte_cryptodev_pmd.h>

#define OTX2_CPT_MIN_HEADROOM_REQ	24
#define OTX2_CPT_MIN_TAILROOM_REQ	8

extern struct rte_cryptodev_ops otx2_cpt_ops;

#endif /* _OTX2_CRYPTODEV_OPS_H_ */
