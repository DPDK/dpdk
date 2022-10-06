/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 HiSilicon Limited
 */

#ifndef HNS3_DUMP_H
#define HNS3_DUMP_H

#include <stdio.h>

#include <ethdev_driver.h>

int hns3_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file);

int hns3_rx_descriptor_dump(const struct rte_eth_dev *dev, uint16_t queue_id,
			    uint16_t offset, uint16_t num, FILE *file);
int hns3_tx_descriptor_dump(const struct rte_eth_dev *dev, uint16_t queue_id,
			    uint16_t offset, uint16_t num, FILE *file);
#endif /* HNS3_DUMP_H */
