/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#ifndef _RTE_MRVL_COMPAT_H_
#define _RTE_MRVL_COMPAT_H_

/* Unluckily, container_of is defined by both DPDK and MUSDK,
 * we'll declare only one version.
 *
 * Note that it is not used in this PMD anyway.
 */
#ifdef container_of
#undef container_of
#endif
#include "env/mv_autogen_comp_flags.h"
#include "drivers/mv_sam.h"
#include "drivers/mv_sam_cio.h"
#include "drivers/mv_sam_session.h"

#endif /* _RTE_MRVL_COMPAT_H_ */
