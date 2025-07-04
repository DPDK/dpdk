/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef ADF_TRANSPORT_ACCESS_MACROS_GEN_LCE_H
#define ADF_TRANSPORT_ACCESS_MACROS_GEN_LCE_H

#include "adf_transport_access_macros.h"

#define ADF_RINGS_PER_INT_SRCSEL_GEN4 2
#define ADF_BANK_INT_SRC_SEL_MASK_GEN4 0x44UL
#define ADF_BANK_INT_FLAG_CLEAR_MASK_GEN4 0x3
#define ADF_RING_BUNDLE_SIZE_GEN_LCE 0x2000
#define ADF_RING_CSR_RING_CONFIG_GEN_LCE 0x1000
#define ADF_RING_CSR_RING_LBASE_GEN_LCE 0x1040
#define ADF_RING_CSR_RING_UBASE_GEN_LCE 0x1080

#define BUILD_RING_BASE_ADDR_GEN_LCE(addr, size) \
	((((addr) >> 6) & (0xFFFFFFFFFFFFFFFFULL << (size))) << 6)

#define WRITE_CSR_RING_BASE_GEN_LCE(csr_base_addr, bank, ring, value) \
do { \
	uint32_t l_base = 0, u_base = 0; \
	l_base = (uint32_t)(value & 0xFFFFFFFF); \
	u_base = (uint32_t)((value & 0xFFFFFFFF00000000ULL) >> 32);	\
	ADF_CSR_WR(csr_base_addr,	\
		(ADF_RING_BUNDLE_SIZE_GEN_LCE * bank) +			\
		ADF_RING_CSR_RING_LBASE_GEN_LCE + (ring << 2),		\
		l_base);						\
	ADF_CSR_WR(csr_base_addr,	\
		 (ADF_RING_BUNDLE_SIZE_GEN_LCE * bank) +			\
		ADF_RING_CSR_RING_UBASE_GEN_LCE + (ring << 2),		\
		u_base);	\
} while (0)

#define WRITE_CSR_RING_CONFIG_GEN_LCE(csr_base_addr, bank, ring, value) \
	ADF_CSR_WR(csr_base_addr,	\
		 (ADF_RING_BUNDLE_SIZE_GEN_LCE * bank) + \
		ADF_RING_CSR_RING_CONFIG_GEN_LCE + (ring << 2), value)

#define WRITE_CSR_RING_TAIL_GEN_LCE(csr_base_addr, bank, ring, value) \
	ADF_CSR_WR((u8 *)(csr_base_addr), \
		(ADF_RING_BUNDLE_SIZE_GEN_LCE * (bank)) + \
		ADF_RING_CSR_RING_TAIL + ((ring) << 2), value)

#define WRITE_CSR_RING_HEAD_GEN_LCE(csr_base_addr, bank, ring, value) \
	ADF_CSR_WR((u8 *)(csr_base_addr), \
		(ADF_RING_BUNDLE_SIZE_GEN_LCE * (bank)) + \
		ADF_RING_CSR_RING_HEAD + ((ring) << 2), value)

#endif
