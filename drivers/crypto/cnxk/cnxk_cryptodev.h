/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_H_
#define _CNXK_CRYPTODEV_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "roc_cpt.h"

#define CNXK_CPT_MAX_CAPS	 34
#define CNXK_SEC_CRYPTO_MAX_CAPS 4
#define CNXK_SEC_MAX_CAPS	 5
#define CNXK_AE_EC_ID_MAX	 8
/**
 * Device private data
 */
struct cnxk_cpt_vf {
	struct roc_cpt cpt;
	struct rte_cryptodev_capabilities crypto_caps[CNXK_CPT_MAX_CAPS];
	struct rte_cryptodev_capabilities
		sec_crypto_caps[CNXK_SEC_CRYPTO_MAX_CAPS];
	struct rte_security_capability sec_caps[CNXK_SEC_MAX_CAPS];
	uint64_t cnxk_fpm_iova[CNXK_AE_EC_ID_MAX];
	struct roc_ae_ec_group *ec_grp[CNXK_AE_EC_ID_MAX];
};

uint64_t cnxk_cpt_default_ff_get(void);
int cnxk_cpt_eng_grp_add(struct roc_cpt *roc_cpt);

#endif /* _CNXK_CRYPTODEV_H_ */
