/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/param.h>
#include <rte_common.h>
#include <rte_errno.h>
#include "hsi_struct_def_dpdk.h"

#include "tf_core.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tfp.h"
#include "hwrm_tf.h"
#include "bnxt.h"
#include "tf_resources.h"
#include "tf_rm.h"

#define PTU_PTE_VALID          0x1UL
#define PTU_PTE_LAST           0x2UL
#define PTU_PTE_NEXT_TO_LAST   0x4UL

/* Number of pointers per page_size */
#define	MAX_PAGE_PTRS(page_size)  ((page_size) / sizeof(void *))

/* API defined in tf_tbl.h */
void
tf_init_tbl_pool(struct tf_session *session)
{
	enum tf_dir dir;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		session->ext_pool_2_scope[dir][TF_EXT_POOL_0] =
			TF_TBL_SCOPE_INVALID;
	}
}
