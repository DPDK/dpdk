/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_MUTEX_H__
#define __NFP_MUTEX_H__

#include "nfp_cpp.h"

struct nfp_cpp_mutex;

int nfp_cpp_mutex_init(struct nfp_cpp *cpp, int target,
		uint64_t address, uint32_t key_id);

struct nfp_cpp_mutex *nfp_cpp_mutex_alloc(struct nfp_cpp *cpp, int target,
		uint64_t address, uint32_t key_id);

void nfp_cpp_mutex_free(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_lock(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_unlock(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_trylock(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_reclaim(struct nfp_cpp *cpp, int target, uint64_t address);

#endif /* __NFP_MUTEX_H__ */
