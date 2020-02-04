/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_SEC_IDEV_H_
#define _OTX2_SEC_IDEV_H_

#include <rte_ethdev.h>

uint8_t otx2_eth_dev_is_sec_capable(struct rte_eth_dev *eth_dev);

#endif /* _OTX2_SEC_IDEV_H_ */
