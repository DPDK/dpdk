/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_PLATFORM_H__
#define __NFP_PLATFORM_H__

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#ifndef BIT_ULL
#define BIT(x) (1 << (x))
#define BIT_ULL(x) (1ULL << (x))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) RTE_DIM(x)
#endif

#define NFP_ERRNO(err) (errno = (err), -1)
#define NFP_ERRNO_RET(err, ret) (errno = (err), (ret))
#define NFP_NOERR(errv) (errno)
#define NFP_ERRPTR(err) (errno = (err), NULL)
#define NFP_PTRERR(errv) (errno)

#endif /* __NFP_PLATFORM_H__ */
