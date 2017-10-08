/*
 *   BSD LICENSE
 *
 *   Copyright (C) 2017 Cavium Inc. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__OCTEONTX_FPAVF_H__
#define	__OCTEONTX_FPAVF_H__

#include <rte_debug.h>

#ifdef RTE_LIBRTE_OCTEONTX_MEMPOOL_DEBUG
#define fpavf_log_info(fmt, args...) \
	RTE_LOG(INFO, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)
#define fpavf_log_dbg(fmt, args...) \
	RTE_LOG(DEBUG, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)
#else
#define fpavf_log_info(fmt, args...)
#define fpavf_log_dbg(fmt, args...)
#endif

#define fpavf_func_trace fpavf_log_dbg
#define fpavf_log_err(fmt, args...) \
	RTE_LOG(ERR, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)

/* fpa pool Vendor ID and Device ID */
#define PCI_VENDOR_ID_CAVIUM		0x177D
#define PCI_DEVICE_ID_OCTEONTX_FPA_VF	0xA053

#define	FPA_VF_MAX			32

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

/* FPA VF register offsets from VF_BAR4, size 2 MByte */
#define	FPA_VF_MSIX_VEC_ADDR		0x00000
#define	FPA_VF_MSIX_VEC_CTL		0x00008
#define	FPA_VF_MSIX_PBA			0xF0000

#define	FPA_VF0_APERTURE_SHIFT		22
#define FPA_AURA_SET_SIZE		16

#endif	/* __OCTEONTX_FPAVF_H__ */
