/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_HW_ACCESS_H_
#define _OTX2_CRYPTODEV_HW_ACCESS_H_

#include <rte_cryptodev.h>

#include "otx2_dev.h"

/* Register offsets */

/* CPT LF registers */
#define OTX2_CPT_LF_MISC_INT		0xb0ull
#define OTX2_CPT_LF_MISC_INT_ENA_W1S	0xd0ull
#define OTX2_CPT_LF_MISC_INT_ENA_W1C	0xe0ull

#define OTX2_CPT_LF_BAR2(vf, q_id) \
		((vf)->otx2_dev.bar2 + \
		 ((RVU_BLOCK_ADDR_CPT0 << 20) | ((q_id) << 12)))

void otx2_cpt_err_intr_unregister(const struct rte_cryptodev *dev);

int otx2_cpt_err_intr_register(const struct rte_cryptodev *dev);

#endif /* _OTX2_CRYPTODEV_HW_ACCESS_H_ */
