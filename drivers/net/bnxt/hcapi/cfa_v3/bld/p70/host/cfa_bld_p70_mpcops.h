/****************************************************************************
 * Copyright(c) 2022 Broadcom Corporation, all rights reserved
 * Proprietary and Confidential Information.
 *
 * This source file is the property of Broadcom Corporation, and
 * may not be copied or distributed in any isomorphic form without
 * the prior written consent of Broadcom Corporation.
 *
 * @file cfa_bld_p70_mpcops.h
 *
 * @brief CFA Phase 7.0 specific Builder library MPC ops api
 */

#ifndef _CFA_BLD_P70_MPCOPS_H_
#define _CFA_BLD_P70_MPCOPS_H_

#include "cfa_types.h"
#include "cfa_bld_mpcops.h"

int cfa_bld_p70_mpc_bind(enum cfa_ver hw_ver, struct cfa_bld_mpcinfo *mpcinfo);

#endif /* _CFA_BLD_P70_MPCOPS_H_ */
