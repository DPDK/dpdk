/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_H_
#define _CNXK_CRYPTODEV_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "roc_ae.h"
#include "roc_cpt.h"

#define CNXK_CPT_MAX_CAPS		 56
#define CNXK_SEC_IPSEC_CRYPTO_MAX_CAPS	 16
#define CNXK_SEC_TLS_1_3_CRYPTO_MAX_CAPS 3
#define CNXK_SEC_TLS_1_2_CRYPTO_MAX_CAPS 7
#define CNXK_SEC_MAX_CAPS		 19

/**
 * Device private data
 */
struct cnxk_cpt_vf {
	struct roc_cpt_lmtline rx_inj_lmtline;
	uint16_t rx_inj_sso_pf_func;
	uint16_t *rx_chan_base;
	struct roc_cpt cpt;
	struct rte_cryptodev_capabilities crypto_caps[CNXK_CPT_MAX_CAPS];
	struct rte_cryptodev_capabilities sec_ipsec_crypto_caps[CNXK_SEC_IPSEC_CRYPTO_MAX_CAPS];
	struct rte_cryptodev_capabilities sec_tls_1_3_crypto_caps[CNXK_SEC_TLS_1_3_CRYPTO_MAX_CAPS];
	struct rte_cryptodev_capabilities sec_tls_1_2_crypto_caps[CNXK_SEC_TLS_1_2_CRYPTO_MAX_CAPS];
	struct rte_cryptodev_capabilities
		sec_dtls_1_2_crypto_caps[CNXK_SEC_TLS_1_2_CRYPTO_MAX_CAPS];
	struct rte_security_capability sec_caps[CNXK_SEC_MAX_CAPS];
	uint64_t cnxk_fpm_iova[ROC_AE_EC_ID_PMAX];
	struct roc_ae_ec_group *ec_grp[ROC_AE_EC_ID_PMAX];
	uint16_t max_qps_limit;
	uint16_t rx_inject_qp;
	uint8_t ctx_ilen;
};

uint64_t cnxk_cpt_default_ff_get(void);
int cnxk_cpt_eng_grp_add(struct roc_cpt *roc_cpt);
int cnxk_cpt_parse_devargs(struct rte_devargs *devargs, struct cnxk_cpt_vf *vf);
void cnxk_cpt_int_misc_cb(struct roc_cpt_lf *lf, void *args);

#endif /* _CNXK_CRYPTODEV_H_ */
