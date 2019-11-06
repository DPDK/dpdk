/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019 Cavium Inc.
 *
 * All rights reserved.
 * www.cavium.com
 */

#ifndef BNX2X_OSAL_H
#define BNX2X_OSAL_H

#ifdef RTE_EXEC_ENV_FREEBSD
#include <sys/stat.h>
#else
#include <linux/types.h>
#endif

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN RTE_LITTLE_ENDIAN
#endif
#undef __BIG_ENDIAN
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN    RTE_BIG_ENDIAN
#endif
#undef __LITTLE_ENDIAN
#endif

#ifdef RTE_EXEC_ENV_FREEBSD
#define __le16		uint16_t
#define __le32		uint32_t
#define __le64		uint64_t
#endif

#endif /* BNX2X_OSAL_H */
