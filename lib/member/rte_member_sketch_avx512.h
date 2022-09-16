/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef RTE_MEMBER_SKETCH_AVX512_H
#define RTE_MEMBER_SKETCH_AVX512_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_vect.h>
#include "rte_member.h"
#include "rte_member_sketch.h"

#define NUM_ROW_VEC 8

void
sketch_update_avx512(const struct rte_member_setsum *ss,
		     const void *key,
		     uint32_t count);

uint64_t
sketch_lookup_avx512(const struct rte_member_setsum *ss,
		     const void *key);

void
sketch_delete_avx512(const struct rte_member_setsum *ss,
		     const void *key);

#ifdef __cplusplus
}
#endif

#endif /* RTE_MEMBER_SKETCH_AVX512_H */
