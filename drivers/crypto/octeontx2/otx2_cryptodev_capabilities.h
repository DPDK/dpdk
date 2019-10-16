/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_CAPABILITIES_H_
#define _OTX2_CRYPTODEV_CAPABILITIES_H_

#include <rte_cryptodev.h>

/*
 * Get capabilities list for the device
 *
 */
const struct rte_cryptodev_capabilities *otx2_cpt_capabilities_get(void);

#endif /* _OTX2_CRYPTODEV_CAPABILITIES_H_ */
