/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _ICE_ALLOC_H_
#define _ICE_ALLOC_H_

/* Memory types */
enum ice_memset_type {
	ICE_NONDMA_MEM = 0,
	ICE_DMA_MEM
};

/* Memcpy types */
enum ice_memcpy_type {
	ICE_NONDMA_TO_NONDMA = 0,
	ICE_NONDMA_TO_DMA,
	ICE_DMA_TO_DMA,
	ICE_DMA_TO_NONDMA
};

#endif /* _ICE_ALLOC_H_ */
