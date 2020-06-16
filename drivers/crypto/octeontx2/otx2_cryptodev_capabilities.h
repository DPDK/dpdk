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
 * Get capabilities list for the device
 *
 */
const struct rte_cryptodev_capabilities *
otx2_cpt_capabilities_get(union cpt_eng_caps *hw_caps);

#endif /* _OTX2_CRYPTODEV_CAPABILITIES_H_ */
