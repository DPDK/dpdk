/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ACL_OSDEP_H_
#define _RTE_ACL_OSDEP_H_

/**
 * @file
 *
 * RTE ACL DPDK/OS dependent file.
 */

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/queue.h>

/*
 * Common defines.
 */

#define DIM(x) RTE_DIM(x)

#include <rte_common.h>
#include <rte_vect.h>
#include <rte_memory.h>
#include <rte_log.h>
#include <rte_memcpy.h>
#include <rte_prefetch.h>
#include <rte_byteorder.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_debug.h>

#endif /* _RTE_ACL_OSDEP_H_ */
