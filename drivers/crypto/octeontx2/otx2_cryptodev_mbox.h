/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_MBOX_H_
#define _OTX2_CRYPTODEV_MBOX_H_

#include <rte_cryptodev.h>

int otx2_cpt_available_queues_get(const struct rte_cryptodev *dev,
				  uint16_t *nb_queues);

int otx2_cpt_queues_attach(const struct rte_cryptodev *dev, uint8_t nb_queues);

int otx2_cpt_queues_detach(const struct rte_cryptodev *dev);

int otx2_cpt_msix_offsets_get(const struct rte_cryptodev *dev);

int otx2_cpt_af_reg_read(const struct rte_cryptodev *dev, uint64_t reg,
			 uint64_t *val);

int otx2_cpt_af_reg_write(const struct rte_cryptodev *dev, uint64_t reg,
			  uint64_t val);

#endif /* _OTX2_CRYPTODEV_MBOX_H_ */
