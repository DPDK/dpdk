/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_FPAVF_H__
#define	__OCTEONTX_FPAVF_H__

#include <rte_io.h>
#include "octeontx_pool_logs.h"

/* fpa pool Vendor ID and Device ID */
#define PCI_VENDOR_ID_CAVIUM		0x177D
#define PCI_DEVICE_ID_OCTEONTX_FPA_VF	0xA053

#define	FPA_VF_MAX			32
#define FPA_GPOOL_MASK			(FPA_VF_MAX-1)
#define FPA_GAURA_SHIFT			4

/* FPA VF register offsets */
#define FPA_VF_INT(x)			(0x200ULL | ((x) << 22))
#define FPA_VF_INT_W1S(x)		(0x210ULL | ((x) << 22))
#define FPA_VF_INT_ENA_W1S(x)		(0x220ULL | ((x) << 22))
#define FPA_VF_INT_ENA_W1C(x)		(0x230ULL | ((x) << 22))

#define	FPA_VF_VHPOOL_AVAILABLE(vhpool)		(0x04150 | ((vhpool)&0x0))
#define	FPA_VF_VHPOOL_THRESHOLD(vhpool)		(0x04160 | ((vhpool)&0x0))
#define	FPA_VF_VHPOOL_START_ADDR(vhpool)	(0x04200 | ((vhpool)&0x0))
#define	FPA_VF_VHPOOL_END_ADDR(vhpool)		(0x04210 | ((vhpool)&0x0))

#define	FPA_VF_VHAURA_CNT(vaura)		(0x20120 | ((vaura)&0xf)<<18)
#define	FPA_VF_VHAURA_CNT_ADD(vaura)		(0x20128 | ((vaura)&0xf)<<18)
#define	FPA_VF_VHAURA_CNT_LIMIT(vaura)		(0x20130 | ((vaura)&0xf)<<18)
#define	FPA_VF_VHAURA_CNT_THRESHOLD(vaura)	(0x20140 | ((vaura)&0xf)<<18)
#define	FPA_VF_VHAURA_OP_ALLOC(vaura)		(0x30000 | ((vaura)&0xf)<<18)
#define	FPA_VF_VHAURA_OP_FREE(vaura)		(0x38000 | ((vaura)&0xf)<<18)

#define FPA_VF_FREE_ADDRS_S(x, y, z)	\
	((x) | (((y) & 0x1ff) << 3) | ((((z) & 1)) << 14))

#define FPA_AURA_IDX(gpool)			(gpool << FPA_GAURA_SHIFT)
/* FPA VF register offsets from VF_BAR4, size 2 MByte */
#define	FPA_VF_MSIX_VEC_ADDR		0x00000
#define	FPA_VF_MSIX_VEC_CTL		0x00008
#define	FPA_VF_MSIX_PBA			0xF0000

#define	FPA_VF0_APERTURE_SHIFT		22
#define FPA_AURA_SET_SIZE		16

#define FPA_MAX_OBJ_SIZE		(128 * 1024)
#define OCTEONTX_FPAVF_BUF_OFFSET	128

/*
 * In Cavium OCTEON TX SoC, all accesses to the device registers are
 * implicitly strongly ordered. So, the relaxed version of IO operation is
 * safe to use with out any IO memory barriers.
 */
#define fpavf_read64 rte_read64_relaxed
#define fpavf_write64 rte_write64_relaxed

/* ARM64 specific functions */
#if defined(RTE_ARCH_ARM64)
#define fpavf_load_pair(val0, val1, addr) ({		\
			asm volatile(			\
			"ldp %x[x0], %x[x1], [%x[p1]]"	\
			:[x0]"=r"(val0), [x1]"=r"(val1) \
			:[p1]"r"(addr)			\
			); })

#define fpavf_store_pair(val0, val1, addr) ({		\
			asm volatile(			\
			"stp %x[x0], %x[x1], [%x[p1]]"	\
			::[x0]"r"(val0), [x1]"r"(val1), [p1]"r"(addr) \
			); })
#else /* Un optimized functions for building on non arm64 arch */

#define fpavf_load_pair(val0, val1, addr)		\
do {							\
	val0 = rte_read64(addr);			\
	val1 = rte_read64(((uint8_t *)addr) + 8);	\
} while (0)

#define fpavf_store_pair(val0, val1, addr)		\
do {							\
	rte_write64(val0, addr);			\
	rte_write64(val1, (((uint8_t *)addr) + 8));	\
} while (0)
#endif

uintptr_t
octeontx_fpa_bufpool_create(unsigned int object_size, unsigned int object_count,
				unsigned int buf_offset, int node);
int
octeontx_fpavf_pool_set_range(uintptr_t handle, unsigned long memsz,
			  void *memva, uint16_t gpool);
int
octeontx_fpa_bufpool_destroy(uintptr_t handle, int node);
int
octeontx_fpa_bufpool_block_size(uintptr_t handle);
int
octeontx_fpa_bufpool_free_count(uintptr_t handle);

static __rte_always_inline uint8_t
octeontx_fpa_bufpool_gpool(uintptr_t handle)
{
	return (uint8_t)handle & FPA_GPOOL_MASK;
}

static __rte_always_inline uint16_t
octeontx_fpa_bufpool_gaura(uintptr_t handle)
{
	return octeontx_fpa_bufpool_gpool(handle) << FPA_GAURA_SHIFT;
}

#endif	/* __OCTEONTX_FPAVF_H__ */
