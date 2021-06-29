/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_CAPABILITIES_H_
#define _CNXK_CRYPTODEV_CAPABILITIES_H_

#include <rte_cryptodev.h>

#include "cnxk_cryptodev.h"

/*
 * Initialize crypto and IPsec capabilities for the device
 *
 */
void cnxk_cpt_caps_populate(struct cnxk_cpt_vf *vf);

/*
 * Get crypto capabilities list for the device
 *
 */
const struct rte_cryptodev_capabilities *
cnxk_crypto_capabilities_get(struct cnxk_cpt_vf *vf);

/*
 * Get security capabilities list for the device
 *
 */
const struct rte_security_capability *
cnxk_crypto_sec_capabilities_get(void *device);

#endif /* _CNXK_CRYPTODEV_CAPABILITIES_H_ */
