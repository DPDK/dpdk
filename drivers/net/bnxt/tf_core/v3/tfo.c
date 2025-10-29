/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "tfo.h"
#include "cfa_types.h"
#include "cfa_tim.h"
#include "bnxt.h"

/** Table scope stored configuration
 */
struct tfc_tsid_db {
	bool ts_valid; /**< Table scope is valid */
	enum cfa_scope_type scope_type; /**< non-shared, shared-app, global */
	bool ts_is_bs_owner; /**< Backing store alloced by this instance (PF) */
	uint16_t ts_max_pools; /**< maximum pools per CPM instance */
	enum cfa_app_type ts_app; /**< application type TF/AFM */
	/** backing store memory config */
	struct tfc_ts_mem_cfg ts_mem[CFA_REGION_TYPE_MAX][CFA_DIR_MAX];
	/** pool info config */
	struct tfc_ts_pool_info ts_pool[CFA_DIR_MAX];
};

/* Only a single global scope is allowed
 */
#define TFC_GLOBAL_SCOPE_MAX 1

/* TFC Global Object
 * The global object is not per port, it is global.  It is only
 * used when a global table scope is created.
 */
struct tfc_global_object {
	uint8_t gtsid;
	struct tfc_tsid_db gtsid_db;
	void *gts_tim;
};

struct tfc_global_object tfc_global;

/** TFC Object Signature
 * This signature identifies the tfc object database and
 * is used for pointer validation
 */
#define TFC_OBJ_SIGNATURE 0xABACABAF

/** TFC Object
 * This data structure contains all data stored per bnxt port
 * Access is restricted through set/get APIs.
 *
 * If a module (e.g. tbl_scope needs to store data, it should
 * be added here and accessor functions created.
 */
struct tfc_object {
	uint32_t signature; /**< TF object signature */
	uint16_t sid; /**< Session ID */
	bool is_pf; /**< port is a PF */
	struct cfa_bld_mpcinfo mpc_info; /**< MPC ops handle */
	struct tfc_tsid_db tsid_db[TFC_TBL_SCOPE_MAX]; /**< tsid database */
	/** TIM instance pointer (PF) - this is where the 4 instances
	 *  of the TPM (rx/tx_lkup, rx/tx_act) will be stored per shared
	 *  table scope.  Only valid on a PF.
	 */
	void *ts_tim;
	struct tfc_global_object *tfgo; /**< pointer to global */
};

void tfo_open(void **tfo, bool is_pf)
{
	int rc;
	struct tfc_object *tfco = NULL;
	struct tfc_global_object *tfgo;
	uint32_t tim_db_size;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return;
	}

	tfco = (struct tfc_object *)rte_zmalloc("tf", sizeof(*tfco), 0);
	if  (tfco == NULL)
		return;

	tfco->signature = TFC_OBJ_SIGNATURE;
	tfco->is_pf = is_pf;
	tfco->sid = INVALID_SID;
	tfco->ts_tim = NULL;

	/* Bind to the MPC builder */
	rc = cfa_bld_mpc_bind(CFA_P70, &tfco->mpc_info);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "MPC bind failed");
		goto cleanup;
	}
	if (is_pf) {
		/* Allocate per bp TIM database */
		rc = cfa_tim_query(TFC_TBL_SCOPE_MAX, CFA_REGION_TYPE_MAX,
				   &tim_db_size);
		if (rc)
			goto cleanup;

		tfco->ts_tim = rte_zmalloc("TIM", tim_db_size, 0);
		if (tfco->ts_tim == NULL)
			goto cleanup;

		rc = cfa_tim_open(tfco->ts_tim,
				  tim_db_size,
				  TFC_TBL_SCOPE_MAX,
				  CFA_REGION_TYPE_MAX);
		if (rc) {
			rte_free(tfco->ts_tim);
			tfco->ts_tim = NULL;
			goto cleanup;
		}
	}
	tfco->tfgo = &tfc_global;
	tfgo = tfco->tfgo;

	if (is_pf && !tfgo->gts_tim) {
		/* Allocate global scope TIM database */
		rc = cfa_tim_query(TFC_GLOBAL_SCOPE_MAX + 1, CFA_REGION_TYPE_MAX,
				   &tim_db_size);
		if (rc)
			goto cleanup;

		tfgo->gts_tim = rte_zmalloc("GTIM", tim_db_size, 0);
		if (!tfgo->gts_tim)
			goto cleanup;

		rc = cfa_tim_open(tfgo->gts_tim,
				  tim_db_size,
				  TFC_GLOBAL_SCOPE_MAX + 1,
				  CFA_REGION_TYPE_MAX);
		if (rc) {
			rte_free(tfgo->gts_tim);
			tfgo->gts_tim = NULL;
			goto cleanup;
		}
	}
	tfgo->gtsid = INVALID_TSID;
	*tfo = tfco;
	return;

 cleanup:
	rte_free(tfco);
	*tfo = NULL;
}

void tfo_close(void **tfo)
{
	struct tfc_object *tfco = (struct tfc_object *)(*tfo);
	enum cfa_region_type region;
	int dir;
	int tsid;
	void *tim;
	void *tpm;

	if (*tfo && tfco->signature == TFC_OBJ_SIGNATURE) {
		/*  If TIM is setup free it and any TPMs */
		for (tsid = 0; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
			if (tfo_tim_get(*tfo, &tim, tsid))
				continue;
			if (!tim)
				continue;
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (dir = 0; dir < CFA_DIR_MAX; dir++) {
					tpm = NULL;
					cfa_tim_tpm_inst_get(tim,
							     tsid,
							     region,
							     dir,
							     &tpm);
					if (tpm) {
						cfa_tim_tpm_inst_set(tim,
								     tsid,
								     region,
								     dir,
								     NULL);
						rte_free(tpm);
					}
				}
			}
		}
		if (tim)
			rte_free(tim);
		tfco->ts_tim = NULL;
		tfco->tfgo = NULL;

		if (*tfo)
			rte_free(*tfo);
		*tfo = NULL;
	}
}

int tfo_mpcinfo_get(void *tfo, struct cfa_bld_mpcinfo **mpc_info)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;

	*mpc_info = NULL;
	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}

	*mpc_info = &tfco->mpc_info;

	return 0;
}

int tfo_ts_validate(void *tfo, uint8_t ts_tsid, bool *ts_valid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}
	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	if (ts_valid)
		*ts_valid = tsid_db->ts_valid;

	return 0;
}

int tfo_ts_set(void *tfo, uint8_t ts_tsid, enum cfa_scope_type scope_type,
	       enum cfa_app_type ts_app, bool ts_valid, uint16_t ts_max_pools)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (scope_type == CFA_SCOPE_TYPE_GLOBAL) {
		tsid_db = &tfgo->gtsid_db;
		tfgo->gtsid = ts_tsid;
	} else if (scope_type == CFA_SCOPE_TYPE_INVALID && tfgo &&
		   ts_tsid == tfgo->gtsid) {
		tfgo->gtsid = INVALID_TSID;
		tsid_db = &tfgo->gtsid_db;
	} else {
		tsid_db = &tfco->tsid_db[ts_tsid];
	}

	tsid_db->ts_valid = ts_valid;
	tsid_db->scope_type = scope_type;
	tsid_db->ts_app = ts_app;
	tsid_db->ts_max_pools = ts_max_pools;

	return 0;
}

int tfo_ts_get(void *tfo, uint8_t ts_tsid, enum cfa_scope_type *scope_type,
	       enum cfa_app_type *ts_app, bool *ts_valid,
	       uint16_t *ts_max_pools)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (ts_tsid == tfgo->gtsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	if (ts_valid)
		*ts_valid = tsid_db->ts_valid;

	if (scope_type)
		*scope_type = tsid_db->scope_type;

	if (ts_app)
		*ts_app = tsid_db->ts_app;

	if (ts_max_pools)
		*ts_max_pools = tsid_db->ts_max_pools;

	return 0;
}

/** Set the table scope memory configuration for this direction
 */
int tfo_ts_set_mem_cfg(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	int rc = 0;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (mem_cfg == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid mem_cfg pointer");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	tsid_db->ts_mem[region][dir] = *mem_cfg;
	tsid_db->ts_is_bs_owner = is_bs_owner;

	return rc;
}

/** Get the table scope memory configuration for this direction
 */
int tfo_ts_get_mem_cfg(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool *is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	int rc = 0;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (mem_cfg == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid mem_cfg pointer");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	*mem_cfg = tsid_db->ts_mem[region][dir];
	if (is_bs_owner)
		*is_bs_owner = tsid_db->ts_is_bs_owner;

	return rc;
}

/** Get the Pool Manager instance
 */
int tfo_ts_get_cpm_inst(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			struct tfc_cpm **cpm_lkup, struct tfc_cpm **cpm_act)
{
	int rc = 0;
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (cpm_lkup == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid cpm_lkup pointer");
		return -EINVAL;
	}
	if (cpm_act == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid cpm_act pointer");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	*cpm_lkup = tsid_db->ts_pool[dir].lkup_cpm;
	*cpm_act = tsid_db->ts_pool[dir].act_cpm;

	return rc;
}
/** Set the Pool Manager instance
 */
int tfo_ts_set_cpm_inst(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			struct tfc_cpm *cpm_lkup, struct tfc_cpm *cpm_act)
{
	int rc = 0;
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}
	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	tsid_db->ts_pool[dir].lkup_cpm = cpm_lkup;
	tsid_db->ts_pool[dir].act_cpm = cpm_act;

	return rc;
}
/** Set the table scope pool memory configuration for this direction
 */
int tfo_ts_set_pool_info(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			 struct tfc_ts_pool_info *ts_pool)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	int rc = 0;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (ts_pool == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid ts_pool pointer");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	tsid_db->ts_pool[dir] = *ts_pool;

	return rc;
}

/** Get the table scope pool memory configuration for this direction
 */
int tfo_ts_get_pool_info(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			 struct tfc_ts_pool_info *ts_pool)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	int rc = 0;
	struct tfc_tsid_db *tsid_db;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (ts_pool == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid ts_pool pointer");
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid %d", ts_tsid);
		return -EINVAL;
	}
	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		tsid_db = &tfgo->gtsid_db;
	else
		tsid_db = &tfco->tsid_db[ts_tsid];

	*ts_pool = tsid_db->ts_pool[dir];

	return rc;
}

int tfo_sid_set(void *tfo, uint16_t sid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;

	if (tfo == NULL)  {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (tfco->sid != INVALID_SID && sid != INVALID_SID &&
	    tfco->sid != sid) {
		PMD_DRV_LOG_LINE(ERR,
				 "Cannot set SID %u, current session is %u",
				 sid, tfco->sid);
		return -EINVAL;
	}

	tfco->sid = sid;

	return 0;
}

int tfo_sid_get(void *tfo, uint16_t *sid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (sid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid sid pointer");
		return -EINVAL;
	}

	if (tfco->sid == INVALID_SID) {
		/* Session has not been created */
		return -ENODEV;
	}

	*sid = tfco->sid;

	return 0;
}

int tfo_tim_get(void *tfo, void **tim, uint8_t ts_tsid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo pointer");
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfo object");
		return -EINVAL;
	}
	if (tim == NULL) {
		PMD_DRV_LOG_LINE(ERR, "%s: Invalid tim pointer to pointer",
				 __func__);
		return -EINVAL;
	}

	*tim = NULL;
	tfgo = tfco->tfgo;

	if (ts_tsid == tfgo->gtsid) {
		if (!tfgo->gts_tim)
		/* ts tim could be null, no need to log error message */
			return -ENOENT;
		*tim = tfgo->gts_tim;
	} else {
		if (!tfco->ts_tim)
			/* ts tim could be null, no need to log error message */
			return -ENOENT;
		*tim = tfco->ts_tim;
	}

	return 0;
}

int tfo_tsid_get(void *tfo, uint8_t *tsid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;
	uint8_t i;

	if (tfo == NULL) {
		PMD_DRV_LOG_LINE(ERR, "%s: Invalid tfo pointer",
				 __func__);
		return -EINVAL;
	}
	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		PMD_DRV_LOG_LINE(ERR, "%s: Invalid tfo object",
				 __func__);
		return -EINVAL;
	}
	if (tsid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "%s: Invalid tsid pointer",
				 __func__);
		return -EINVAL;
	}

	tfgo = tfco->tfgo;
	if (tfgo) {
		tsid_db = &tfgo->gtsid_db;
		if (tsid_db->ts_valid && tfgo->gtsid != INVALID_TSID) {
			*tsid = tfgo->gtsid;
			return 0;
		}
	}
	for (i = 1; i < TFC_TBL_SCOPE_MAX; i++) {
		tsid_db = &tfco->tsid_db[i];

		if (tsid_db->ts_valid) {
			*tsid = i;
			return 0;
		}
	}

	return -1;
}
