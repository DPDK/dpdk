/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_INTERNAL_VER_H
#define SXE2_INTERNAL_VER_H

#define SXE2_VER_MAJOR_OFFSET (16)
#define SXE2_MK_VER(major, minor) \
	(major << SXE2_VER_MAJOR_OFFSET | minor)
#define SXE2_MK_VER_MAJOR(ver) (((ver) >> SXE2_VER_MAJOR_OFFSET) & 0xff)
#define SXE2_MK_VER_MINOR(ver) ((ver) & 0xff)

#define SXE2_ITR_VER_MAJOR_V100    1
#define SXE2_ITR_VER_MAJOR_V200    2

#define SXE2_ITR_VER_MAJOR      1
#define SXE2_ITR_VER_MINOR      1
#define SXE2_ITR_VER SXE2_MK_VER(SXE2_ITR_VER_MAJOR, SXE2_ITR_VER_MINOR)

#define SXE2_CTRL_VER_IS_V100(ver)  (SXE2_MK_VER_MAJOR(ver) == SXE2_ITR_VER_MAJOR_V100)
#define SXE2_CTRL_VER_IS_V200(ver)  (SXE2_MK_VER_MAJOR(ver) == SXE2_ITR_VER_MAJOR_V200)

#define SXE2LIB_ITR_VER_MAJOR      1
#define SXE2LIB_ITR_VER_MINOR      1
#define SXE2LIB_ITR_VER     SXE2_MK_VER(SXE2LIB_ITR_VER_MAJOR, SXE2LIB_ITR_VER_MINOR)

#define SXE2_DRV_CLI_VER_MAJOR      1
#define SXE2_DRV_CLI_VER_MINOR      1
#define SXE2_DRV_CLI_VER \
	SXE2_MK_VER(SXE2_DRV_CLI_VER_MAJOR, SXE2_DRV_CLI_VER_MINOR)

#endif /* SXE2_INTERNAL_VER_H */
