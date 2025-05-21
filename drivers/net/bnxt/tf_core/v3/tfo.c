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
	bool ts_is_shared; /**< Table scope is shared */
	bool ts_is_bs_owner; /**< Backing store allocated by this instance (PF) */
	uint16_t ts_max_pools; /**< maximum pools per CPM instance */
	enum cfa_app_type ts_app; /**< application type TF/AFM */
	/** backing store memory config */
	struct tfc_ts_mem_cfg ts_mem[CFA_REGION_TYPE_MAX][CFA_DIR_MAX];
	/** pool info config */
	struct tfc_ts_pool_info ts_pool[CFA_DIR_MAX];
};

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
};

void tfo_open(void **tfo, bool is_pf)
{
	int rc;
	struct tfc_object *tfco = NULL;
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
		/* Allocate TIM */
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
		if (tfo_tim_get(*tfo, &tim))
			goto done;

		if (!tim)
			goto done;

		for (tsid = 0; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
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
		rte_free(tim);
		tfco->ts_tim = NULL;
done:
		rte_free(*tfo);
		*tfo = NULL;
	}
}

int tfo_mpcinfo_get(void *tfo, struct cfa_bld_mpcinfo **mpc_info)
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

	*mpc_info = &tfco->mpc_info;

	return 0;
}

int tfo_ts_validate(void *tfo, uint8_t ts_tsid, bool *ts_valid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
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
	tsid_db = &tfco->tsid_db[ts_tsid];

	if (ts_valid)
		*ts_valid = tsid_db->ts_valid;

	return 0;
}

int tfo_ts_set(void *tfo, uint8_t ts_tsid, bool ts_is_shared,
	       enum cfa_app_type ts_app, bool ts_valid, uint16_t ts_max_pools)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
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

	tsid_db = &tfco->tsid_db[ts_tsid];

	tsid_db->ts_valid = ts_valid;
	tsid_db->ts_is_shared = ts_is_shared;
	tsid_db->ts_app = ts_app;
	tsid_db->ts_max_pools = ts_max_pools;

	return 0;
}

int tfo_ts_get(void *tfo, uint8_t ts_tsid, bool *ts_is_shared,
	       enum cfa_app_type *ts_app, bool *ts_valid,
	       uint16_t *ts_max_pools)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
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

	tsid_db = &tfco->tsid_db[ts_tsid];

	if (ts_valid)
		*ts_valid = tsid_db->ts_valid;

	if (ts_is_shared)
		*ts_is_shared = tsid_db->ts_is_shared;

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

int tfo_tim_set(void *tfo, void *tim)
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
	if (tim == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tim pointer");
		return -EINVAL;
	}

	if (tfco->ts_tim != NULL &&
	    tfco->ts_tim != tim) {
		PMD_DRV_LOG_LINE(ERR,
				 "Cannot set TS TIM, TIM is already set");
		return -EINVAL;
	}

	tfco->ts_tim = tim;

	return 0;
}

int tfo_tim_get(void *tfo, void **tim)
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
	if (tim == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tim pointer to pointer");
		return -EINVAL;
	}
	if (tfco->ts_tim == NULL) {
		/* ts tim could be null, no need to log error message */
		return -ENODEV;
	}

	*tim = tfco->ts_tim;

	return 0;
}
