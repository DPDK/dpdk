/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include "hcapi_cfa_defs.h"
#include "tf_util.h"
#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_session.h"
#include "cfa_tcam_mgr_sbmp.h"
#include "tfp.h"
#include "cfa_tcam_mgr_p58.h"
#include "cfa_tcam_mgr_p4.h"

struct cfa_tcam_mgr_session_data {
	uint32_t session_id;
	/* The following are per-session values */
	uint16_t max_entries[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	uint16_t used_entries[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
};

static struct cfa_tcam_mgr_session_data session_data[TF_TCAM_MAX_SESSIONS];

static uint16_t last_entry_id;

static struct sbmp *session_bmp[TF_TCAM_MAX_SESSIONS];

int
cfa_tcam_mgr_session_init(int sess_idx, enum cfa_tcam_mgr_device_type type)
{
	int rc;

	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_P4:
	case CFA_TCAM_MGR_DEVICE_TYPE_SR:
		rc = cfa_tcam_mgr_sess_table_get_p4(sess_idx, &session_bmp[sess_idx]);
		break;
	case CFA_TCAM_MGR_DEVICE_TYPE_P5:
		rc = cfa_tcam_mgr_sess_table_get_p58(sess_idx, &session_bmp[sess_idx]);
		break;
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device %d\n", type);
		rc = -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	return rc;
}

int
cfa_tcam_mgr_get_session_from_context(struct cfa_tcam_mgr_context *context,
				      uint32_t *session_id)
{
	if (context == NULL) {
		CFA_TCAM_MGR_LOG_0(ERR, "context passed as NULL pointer.\n");
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	*session_id = context->tfp->session->session_id.id;
	return 0;
}

int
cfa_tcam_mgr_session_find(unsigned int session_id)
{
	unsigned int sess_idx;

	for (sess_idx = 0; sess_idx < ARRAY_SIZE(session_data); sess_idx++) {
		if (session_data[sess_idx].session_id == session_id)
			return sess_idx;
	}

	return -CFA_TCAM_MGR_ERR_CODE(INVAL);
}

int
cfa_tcam_mgr_session_add(unsigned int session_id)
{
	int sess_idx;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx >= 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Session is already bound.\n");
		return -CFA_TCAM_MGR_ERR_CODE(BUSY);
	}

	/* Session not found in table, find first empty entry. */
	for (sess_idx = 0;
	     sess_idx < (signed int)ARRAY_SIZE(session_data);
	     sess_idx++) {
		if (session_data[sess_idx].session_id == 0)
			break;
	}

	if (sess_idx >= (signed int)ARRAY_SIZE(session_data)) {
		/* No room in the session table */
		CFA_TCAM_MGR_LOG_0(ERR, "Session table is full.\n");
		return -CFA_TCAM_MGR_ERR_CODE(NOMEM);
	}

	session_data[sess_idx].session_id = session_id;

	return sess_idx;
}

int
cfa_tcam_mgr_session_free(unsigned int session_id,
		struct cfa_tcam_mgr_context *context)
{
	struct cfa_tcam_mgr_free_parms free_parms;
	int entry_id;
	int sess_idx = cfa_tcam_mgr_session_find(session_id);

	if (sess_idx < 0)
		return sess_idx;

	memset(&free_parms, 0, sizeof(free_parms));
	/* Since we are freeing all pending TCAM entries (which is typically
	 * done during tcam_unbind), we don't know the type of each entry.
	 * So we set the type to MAX as a hint to cfa_tcam_mgr_free() to
	 * figure out the actual type. We need to set it through each
	 * iteration in the loop below; otherwise, the type determined for
	 * the first entry would be used for subsequent entries that may or
	 * may not be of the same type, resulting in errors.
	 */
	for (entry_id = 0; entry_id < cfa_tcam_mgr_max_entries[sess_idx]; entry_id++) {
		if (SBMP_MEMBER(session_bmp[sess_idx][entry_id], sess_idx)) {
			SBMP_SESSION_REMOVE(session_bmp[sess_idx][entry_id], sess_idx);

			free_parms.id = entry_id;
			free_parms.type = CFA_TCAM_MGR_TBL_TYPE_MAX;
			cfa_tcam_mgr_free(context, &free_parms);
		}
	}

	memset(&session_data[sess_idx], 0, sizeof(session_data[sess_idx]));
	return 0;
}

int
cfa_tcam_mgr_session_cfg(unsigned int session_id,
			 uint16_t tcam_cnt[][CFA_TCAM_MGR_TBL_TYPE_MAX])
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_session_data *session_entry;
	unsigned int dir, type;
	int sess_idx = cfa_tcam_mgr_session_find(session_id);
	uint16_t requested_cnt;

	if (sess_idx < 0)
		return sess_idx;

	session_entry = &session_data[sess_idx];

	/* Validate session request */
	for (dir = 0; dir < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx]); dir++) {
		for (type = 0;
		     type < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir]);
		     type++) {
			table_data = &cfa_tcam_mgr_tables[sess_idx][dir][type];
			requested_cnt = tcam_cnt[dir][type];
			/*
			 * Only check if table supported (max_entries > 0).
			 */
			if (table_data->max_entries > 0 &&
			    requested_cnt > table_data->max_entries) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
						"Requested %d, available %d.\n",
						requested_cnt,
						table_data->max_entries);
				return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
			}
		}
	}

	memcpy(session_entry->max_entries, tcam_cnt,
	       sizeof(session_entry->max_entries));
	return 0;
}

void
cfa_tcam_mgr_mv_session_used_entries_cnt(int sess_idx, enum tf_dir dir,
					 enum cfa_tcam_mgr_tbl_type dst_type,
					 enum cfa_tcam_mgr_tbl_type src_type)
{
	session_data[sess_idx].used_entries[dir][dst_type]++;
	session_data[sess_idx].used_entries[dir][src_type]--;
}

int
cfa_tcam_mgr_session_entry_alloc(unsigned int session_id,
				 enum tf_dir dir,
				 enum cfa_tcam_mgr_tbl_type type)
{
	int sess_idx;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Session not found.\n");
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	if (session_data[sess_idx].used_entries[dir][type] >=
	    session_data[sess_idx].max_entries[dir][type]) {
		CFA_TCAM_MGR_LOG_0(ERR, "Table full (session).\n");
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	do {
		last_entry_id++;
		if (cfa_tcam_mgr_max_entries[sess_idx] <= last_entry_id)
			last_entry_id = 0;
	} while (!SBMP_IS_NULL(session_bmp[sess_idx][last_entry_id]));

	SBMP_SESSION_ADD(session_bmp[sess_idx][last_entry_id], sess_idx);

	session_data[sess_idx].used_entries[dir][type] += 1;

	return last_entry_id;
}

int
cfa_tcam_mgr_session_entry_free(unsigned int session_id,
				unsigned int entry_id,
				enum tf_dir dir,
				enum cfa_tcam_mgr_tbl_type type)
{
	int sess_idx;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Session not found.\n");
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	SBMP_SESSION_REMOVE(session_bmp[sess_idx][entry_id], sess_idx);
	session_data[sess_idx].used_entries[dir][type] -= 1;

	return 0;
}

#if SBMP_WORD_WIDTH == 16
#define SBMP_FORMAT PRIX16
#define SBMP_PRECISION "4"
#elif SBMP_WORD_WIDTH == 32
#define SBMP_FORMAT PRIX32
#define SBMP_PRECISION "8"
#elif SBMP_WORD_WIDTH == 64
#define SBMP_FORMAT PRIX64
#define SBMP_PRECISION "16"
#else
#error "Invalid value for SBMP_WORD_WIDTH."
#endif

static void
cfa_tcam_mgr_session_bitmap_print(struct sbmp *session_bmp)
{
	unsigned int i;

	printf("0x");
	for (i = 0;
	     i < ARRAY_SIZE(session_bmp->bits);
	     i++) {
		printf("%0" SBMP_PRECISION SBMP_FORMAT,
		       session_bmp->bits[i]);
	}
}

#define SESSION_DUMP_HEADER_1 "                             RX          TX\n"
#define SESSION_DUMP_HEADER_2 \
	"                         Max   Used  Max   Used\n"

static void
cfa_tcam_mgr_session_printf(struct cfa_tcam_mgr_session_data *session,
			    enum cfa_tcam_mgr_tbl_type tbl_type)
{
	printf("%-22s: %5u %5u %5u %5u\n",
	       cfa_tcam_mgr_tbl_2_str(tbl_type),
	       session->max_entries[TF_DIR_RX][tbl_type],
	       session->used_entries[TF_DIR_RX][tbl_type],
	       session->max_entries[TF_DIR_TX][tbl_type],
	       session->used_entries[TF_DIR_TX][tbl_type]);
}

void
cfa_tcam_mgr_sessions_dump(void)
{
	struct cfa_tcam_mgr_session_data *session;
	unsigned int sess_idx;
	bool sess_found = false;
	enum cfa_tcam_mgr_tbl_type tbl_type;

	printf("\nTCAM Sessions Table:\n");
	for (sess_idx = 0; sess_idx < ARRAY_SIZE(session_data); sess_idx++) {
		if (session_data[sess_idx].session_id != 0) {
			session = &session_data[sess_idx];
			if (!sess_found) {
				printf(SESSION_DUMP_HEADER_1);
				printf(SESSION_DUMP_HEADER_2);
			}
			printf("Session 0x%08x:\n",
			       session->session_id);
			for (tbl_type = CFA_TCAM_MGR_TBL_TYPE_START;
			     tbl_type < CFA_TCAM_MGR_TBL_TYPE_MAX;
			     tbl_type++) {
				cfa_tcam_mgr_session_printf(session, tbl_type);
			}
			sess_found = true;
		}
	}

	if (!sess_found)
		printf("No sessions found.\n");
}

/* This dumps all the sessions using an entry */
void
cfa_tcam_mgr_entry_sessions_dump(int sess_idx, uint16_t id)
{
	bool session_found = false;

	if (id >= cfa_tcam_mgr_max_entries[sess_idx]) {
		printf("Entry ID %u out of range for sess_idx %d.  Max ID %u.\n",
		       id, sess_idx, cfa_tcam_mgr_max_entries[sess_idx] - 1);
		return;
	}

	if (!SBMP_IS_NULL(session_bmp[sess_idx][id])) {
		printf("Sessions using entry ID %u:\n", id);
		for (sess_idx = 0; sess_idx < SBMP_SESSION_MAX; sess_idx++)
			if (SBMP_MEMBER(session_bmp[sess_idx][id], (sess_idx))) {
				if (session_data[sess_idx].session_id != 0) {
					printf("0x%08x (index %d)\n",
					  session_data[sess_idx].session_id,
					  sess_idx);
					session_found = true;
				} else {
					printf("Error! Entry ID %u used by "
					       "session index %d which is not "
					       "in use.\n",
					       id, sess_idx);
				}
			}
		if (!session_found)
			printf("No sessions using entry ID %u.\n", id);
	} else {
		printf("Entry ID %u not in use.\n",
		       id);
		return;
	}
}

/* This dumps all the entries in use by any session */
void
cfa_tcam_mgr_session_entries_dump(int sess_idx)
{
	bool entry_found = false;
	uint16_t id;

	printf("\nGlobal Maximum Entries for sess_idx %d: %d\n\n",
	       sess_idx, cfa_tcam_mgr_max_entries[sess_idx]);
	printf("TCAM Session Entry Table:\n");
	for (id = 0; id < cfa_tcam_mgr_max_entries[sess_idx]; id++) {
		if (!SBMP_IS_NULL(session_bmp[sess_idx][id])) {
			if (!entry_found)
				printf("  EID Session bitmap\n");
			printf("%5u ", id);
			cfa_tcam_mgr_session_bitmap_print(&session_bmp[sess_idx][id]);
			printf("\n");
			entry_found = true;
		}
	}

	if (!entry_found)
		printf("No entries found.\n");
}
