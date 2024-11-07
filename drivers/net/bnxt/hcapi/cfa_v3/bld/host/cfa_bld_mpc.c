/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#include <errno.h>
#include "cfa_bld.h"
#include "host/cfa_bld_mpcops.h"

#if SUPPORT_CFA_HW_P70
#include "cfa_bld_p70_mpcops.h"
#endif

int cfa_bld_mpc_bind(enum cfa_ver hw_ver, struct cfa_bld_mpcinfo *mpcinfo)
{
	if (!mpcinfo)
		return -EINVAL;

	switch (hw_ver) {
	case CFA_P40:
	case CFA_P45:
	case CFA_P58:
	case CFA_P59:
		return -ENOTSUP;
	case CFA_P70:
#if SUPPORT_CFA_HW_P70
		return cfa_bld_p70_mpc_bind(hw_ver, mpcinfo);
#else
		return -ENOTSUP;
#endif
	default:
		return -EINVAL;
	}
}
