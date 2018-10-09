/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_CAPABILITIES_H_
#define _OTX_CRYPTODEV_CAPABILITIES_H_

#include <rte_cryptodev.h>

/*
 * Get capabilities list for the device
 *
 */
const struct rte_cryptodev_capabilities *
otx_get_capabilities(void);

#endif /* _OTX_CRYPTODEV_CAPABILITIES_H_ */
