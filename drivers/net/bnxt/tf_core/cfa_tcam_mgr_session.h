/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_SESSION_H
#define CFA_TCAM_MGR_SESSION_H

#include <inttypes.h>
#include "cfa_tcam_mgr.h"

int
cfa_tcam_mgr_session_init(int sess_idx, enum cfa_tcam_mgr_device_type type);

int
cfa_tcam_mgr_get_session_from_context(struct cfa_tcam_mgr_context *context,
				      uint32_t *session_id);

int
cfa_tcam_mgr_session_find(unsigned int session_id);

int
cfa_tcam_mgr_session_add(unsigned int session_id);

int
cfa_tcam_mgr_session_free(unsigned int session_id,
		struct cfa_tcam_mgr_context *context);

int
cfa_tcam_mgr_session_cfg(unsigned int session_id,
			 uint16_t tcam_cnt[][CFA_TCAM_MGR_TBL_TYPE_MAX]);

int
cfa_tcam_mgr_session_entry_alloc(unsigned int session_id,
				 enum tf_dir dir,
				 enum cfa_tcam_mgr_tbl_type type);
int
cfa_tcam_mgr_session_entry_free(unsigned int session_id,
				unsigned int entry_id,
				enum tf_dir dir,
				enum cfa_tcam_mgr_tbl_type type);

void
cfa_tcam_mgr_sessions_dump(void);
void
cfa_tcam_mgr_entry_sessions_dump(int sess_idx, uint16_t id);
void
cfa_tcam_mgr_session_entries_dump(int sess_idx);

void
cfa_tcam_mgr_mv_session_used_entries_cnt(int sess_idx, enum tf_dir dir,
					 enum cfa_tcam_mgr_tbl_type dst_type,
					 enum cfa_tcam_mgr_tbl_type src_type);
#endif  /* CFA_TCAM_MGR_SESSION_H */
