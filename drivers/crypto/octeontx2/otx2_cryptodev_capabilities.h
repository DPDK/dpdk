/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_CAPABILITIES_H_
#define _OTX2_CRYPTODEV_CAPABILITIES_H_

#include <rte_cryptodev.h>

#include "otx2_mbox.h"

enum otx2_cpt_egrp {
	OTX2_CPT_EGRP_SE = 0,
	OTX2_CPT_EGRP_SE_IE = 1,
	OTX2_CPT_EGRP_AE = 2,
	OTX2_CPT_EGRP_MAX,
};

/*
 * Initialize crypto capabilities for the device
 *
 */
void otx2_crypto_capabilities_init(union cpt_eng_caps *hw_caps);

/*
 * Get capabilities list for the device
 *
 */
const struct rte_cryptodev_capabilities *
otx2_cpt_capabilities_get(void);

/*
 * Initialize security capabilities for the device
 *
 */
void otx2_crypto_sec_capabilities_init(union cpt_eng_caps *hw_caps);

/*
 * Get security capabilities list for the device
 *
 */
const struct rte_security_capability *
otx2_crypto_sec_capabilities_get(void *device __rte_unused);

#endif /* _OTX2_CRYPTODEV_CAPABILITIES_H_ */
