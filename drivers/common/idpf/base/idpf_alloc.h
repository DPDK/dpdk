/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _IDPF_ALLOC_H_
#define _IDPF_ALLOC_H_

/* Memory types */
enum idpf_memset_type {
	IDPF_NONDMA_MEM = 0,
	IDPF_DMA_MEM
};

/* Memcpy types */
enum idpf_memcpy_type {
	IDPF_NONDMA_TO_NONDMA = 0,
	IDPF_NONDMA_TO_DMA,
	IDPF_DMA_TO_DMA,
	IDPF_DMA_TO_NONDMA
};

#endif /* _IDPF_ALLOC_H_ */
