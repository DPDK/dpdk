/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_mbox.h"

/**
 * @internal
 * Set default NPA configuration.
 */
void
otx2_npa_set_defaults(struct otx2_idev_cfg *idev)
{
	idev->npa_pf_func = 0;
	rte_atomic16_set(&idev->npa_refcnt, 0);
}

/**
 * @internal
 * Get intra device config structure.
 */
struct otx2_idev_cfg *
otx2_intra_dev_get_cfg(void)
{
	const char name[] = "octeontx2_intra_device_conf";
	const struct rte_memzone *mz;
	struct otx2_idev_cfg *idev;

	mz = rte_memzone_lookup(name);
	if (mz != NULL)
		return mz->addr;

	/* Request for the first time */
	mz = rte_memzone_reserve_aligned(name, sizeof(struct otx2_idev_cfg),
					 SOCKET_ID_ANY, 0, OTX2_ALIGN);
	if (mz != NULL) {
		idev = mz->addr;
		idev->sso_pf_func = 0;
		idev->npa_lf = NULL;
		otx2_npa_set_defaults(idev);
		return idev;
	}
	return NULL;
}

/**
 * @internal
 * Get SSO PF_FUNC.
 */
uint16_t
otx2_sso_pf_func_get(void)
{
	struct otx2_idev_cfg *idev;
	uint16_t sso_pf_func;

	sso_pf_func = 0;
	idev = otx2_intra_dev_get_cfg();

	if (idev != NULL)
		sso_pf_func = idev->sso_pf_func;

	return sso_pf_func;
}

/**
 * @internal
 * Set SSO PF_FUNC.
 */
void
otx2_sso_pf_func_set(uint16_t sso_pf_func)
{
	struct otx2_idev_cfg *idev;

	idev = otx2_intra_dev_get_cfg();

	if (idev != NULL) {
		idev->sso_pf_func = sso_pf_func;
		rte_smp_wmb();
	}
}

/**
 * @internal
 * Get NPA PF_FUNC.
 */
uint16_t
otx2_npa_pf_func_get(void)
{
	struct otx2_idev_cfg *idev;
	uint16_t npa_pf_func;

	npa_pf_func = 0;
	idev = otx2_intra_dev_get_cfg();

	if (idev != NULL)
		npa_pf_func = idev->npa_pf_func;

	return npa_pf_func;
}

/**
 * @internal
 * Get NPA lf object.
 */
struct otx2_npa_lf *
otx2_npa_lf_obj_get(void)
{
	struct otx2_idev_cfg *idev;

	idev = otx2_intra_dev_get_cfg();

	if (idev != NULL && rte_atomic16_read(&idev->npa_refcnt))
		return idev->npa_lf;

	return NULL;
}

/**
 * @internal
 * Is NPA lf active for the given device?.
 */
int
otx2_npa_lf_active(void *otx2_dev)
{
	struct otx2_dev *dev = otx2_dev;
	struct otx2_idev_cfg *idev;

	/* Check if npalf is actively used on this dev */
	idev = otx2_intra_dev_get_cfg();
	if (!idev || !idev->npa_lf || idev->npa_lf->mbox != dev->mbox)
		return 0;

	return rte_atomic16_read(&idev->npa_refcnt);
}

/*
 * @internal
 * Gets reference only to existing NPA LF object.
 */
int otx2_npa_lf_obj_ref(void)
{
	struct otx2_idev_cfg *idev;
	uint16_t cnt;
	int rc;

	idev = otx2_intra_dev_get_cfg();

	/* Check if ref not possible */
	if (idev == NULL)
		return -EINVAL;


	/* Get ref only if > 0 */
	cnt = rte_atomic16_read(&idev->npa_refcnt);
	while (cnt != 0) {
		rc = rte_atomic16_cmpset(&idev->npa_refcnt_u16, cnt, cnt + 1);
		if (rc)
			break;

		cnt = rte_atomic16_read(&idev->npa_refcnt);
	}

	return cnt ? 0 : -EINVAL;
}

/**
 * @internal
 */
int otx2_logtype_base;
/**
 * @internal
 */
int otx2_logtype_mbox;
/**
 * @internal
 */
int otx2_logtype_npa;
/**
 * @internal
 */
int otx2_logtype_nix;
/**
 * @internal
 */
int otx2_logtype_npc;
/**
 * @internal
 */
int otx2_logtype_tm;
/**
 * @internal
 */
int otx2_logtype_sso;
/**
 * @internal
 */
int otx2_logtype_tim;
/**
 * @internal
 */
int otx2_logtype_dpi;

RTE_INIT(otx2_log_init);
static void
otx2_log_init(void)
{
	otx2_logtype_base = rte_log_register("pmd.octeontx2.base");
	if (otx2_logtype_base >= 0)
		rte_log_set_level(otx2_logtype_base, RTE_LOG_NOTICE);

	otx2_logtype_mbox = rte_log_register("pmd.octeontx2.mbox");
	if (otx2_logtype_mbox >= 0)
		rte_log_set_level(otx2_logtype_mbox, RTE_LOG_NOTICE);

	otx2_logtype_npa = rte_log_register("pmd.mempool.octeontx2");
	if (otx2_logtype_npa >= 0)
		rte_log_set_level(otx2_logtype_npa, RTE_LOG_NOTICE);

	otx2_logtype_nix = rte_log_register("pmd.net.octeontx2");
	if (otx2_logtype_nix >= 0)
		rte_log_set_level(otx2_logtype_nix, RTE_LOG_NOTICE);

	otx2_logtype_npc = rte_log_register("pmd.net.octeontx2.flow");
	if (otx2_logtype_npc >= 0)
		rte_log_set_level(otx2_logtype_npc, RTE_LOG_NOTICE);

	otx2_logtype_tm = rte_log_register("pmd.net.octeontx2.tm");
	if (otx2_logtype_tm >= 0)
		rte_log_set_level(otx2_logtype_tm, RTE_LOG_NOTICE);

	otx2_logtype_sso = rte_log_register("pmd.event.octeontx2");
	if (otx2_logtype_sso >= 0)
		rte_log_set_level(otx2_logtype_sso, RTE_LOG_NOTICE);

	otx2_logtype_tim = rte_log_register("pmd.event.octeontx2.timer");
	if (otx2_logtype_tim >= 0)
		rte_log_set_level(otx2_logtype_tim, RTE_LOG_NOTICE);

	otx2_logtype_dpi = rte_log_register("pmd.raw.octeontx2.dpi");
	if (otx2_logtype_dpi >= 0)
		rte_log_set_level(otx2_logtype_dpi, RTE_LOG_NOTICE);
}
