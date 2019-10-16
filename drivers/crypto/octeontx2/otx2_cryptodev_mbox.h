/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_MBOX_H_
#define _OTX2_CRYPTODEV_MBOX_H_

#include <rte_cryptodev.h>

int otx2_cpt_available_queues_get(const struct rte_cryptodev *dev,
				  uint16_t *nb_queues);

#endif /* _OTX2_CRYPTODEV_MBOX_H_ */
