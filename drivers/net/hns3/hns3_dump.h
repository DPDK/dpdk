/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 HiSilicon Limited
 */

#ifndef _HNS3_DUMP_H_
#define _HNS3_DUMP_H_

#include <stdio.h>

#include <ethdev_driver.h>

int hns3_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file);
#endif /* _HNS3_DUMP_H_ */
