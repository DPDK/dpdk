/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */
#ifndef _ROC_FEATURES_H_
#define _ROC_FEATURES_H_

static inline bool
roc_feature_sso_has_stash(void)
{
	return (roc_model_is_cn103xx() | roc_model_is_cn10ka_b0()) ? true : false;
}

static inline bool
roc_feature_nix_has_inl_ipsec_mseg(void)
{
	return (roc_model_is_cn10kb() || roc_model_is_cn10ka_b0());
}

static inline bool
roc_feature_nix_has_inl_rq_mask(void)
{
	return (roc_model_is_cn10kb() || roc_model_is_cn10ka_b0());
}

static inline bool
roc_feature_nix_has_own_meta_aura(void)
{
	return (roc_model_is_cn10kb() || roc_model_is_cn10ka_b0());
}

static inline bool
roc_feature_nix_has_late_bp(void)
{
	return (roc_model_is_cn10kb() || roc_model_is_cn10ka_b0());
}

static inline bool
roc_feature_nix_has_reass(void)
{
	return roc_model_is_cn10ka();
}

#endif
