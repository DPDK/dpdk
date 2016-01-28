/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2015.
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

#include "rte_cpuflags.h"

#ifdef RTE_ARCH_64
const struct feature_entry rte_cpu_feature_table[] = {
	FEAT_DEF(FP,		0x00000001, 0, REG_HWCAP,  0)
	FEAT_DEF(NEON,		0x00000001, 0, REG_HWCAP,  1)
	FEAT_DEF(EVTSTRM,	0x00000001, 0, REG_HWCAP,  2)
	FEAT_DEF(AES,		0x00000001, 0, REG_HWCAP,  3)
	FEAT_DEF(PMULL,		0x00000001, 0, REG_HWCAP,  4)
	FEAT_DEF(SHA1,		0x00000001, 0, REG_HWCAP,  5)
	FEAT_DEF(SHA2,		0x00000001, 0, REG_HWCAP,  6)
	FEAT_DEF(CRC32,		0x00000001, 0, REG_HWCAP,  7)
	FEAT_DEF(AARCH64,	0x00000001, 0, REG_PLATFORM, 1)
};
#else
const struct feature_entry rte_cpu_feature_table[] = {
	FEAT_DEF(SWP,       0x00000001, 0, REG_HWCAP,  0)
	FEAT_DEF(HALF,      0x00000001, 0, REG_HWCAP,  1)
	FEAT_DEF(THUMB,     0x00000001, 0, REG_HWCAP,  2)
	FEAT_DEF(A26BIT,    0x00000001, 0, REG_HWCAP,  3)
	FEAT_DEF(FAST_MULT, 0x00000001, 0, REG_HWCAP,  4)
	FEAT_DEF(FPA,       0x00000001, 0, REG_HWCAP,  5)
	FEAT_DEF(VFP,       0x00000001, 0, REG_HWCAP,  6)
	FEAT_DEF(EDSP,      0x00000001, 0, REG_HWCAP,  7)
	FEAT_DEF(JAVA,      0x00000001, 0, REG_HWCAP,  8)
	FEAT_DEF(IWMMXT,    0x00000001, 0, REG_HWCAP,  9)
	FEAT_DEF(CRUNCH,    0x00000001, 0, REG_HWCAP,  10)
	FEAT_DEF(THUMBEE,   0x00000001, 0, REG_HWCAP,  11)
	FEAT_DEF(NEON,      0x00000001, 0, REG_HWCAP,  12)
	FEAT_DEF(VFPv3,     0x00000001, 0, REG_HWCAP,  13)
	FEAT_DEF(VFPv3D16,  0x00000001, 0, REG_HWCAP,  14)
	FEAT_DEF(TLS,       0x00000001, 0, REG_HWCAP,  15)
	FEAT_DEF(VFPv4,     0x00000001, 0, REG_HWCAP,  16)
	FEAT_DEF(IDIVA,     0x00000001, 0, REG_HWCAP,  17)
	FEAT_DEF(IDIVT,     0x00000001, 0, REG_HWCAP,  18)
	FEAT_DEF(VFPD32,    0x00000001, 0, REG_HWCAP,  19)
	FEAT_DEF(LPAE,      0x00000001, 0, REG_HWCAP,  20)
	FEAT_DEF(EVTSTRM,   0x00000001, 0, REG_HWCAP,  21)
	FEAT_DEF(AES,       0x00000001, 0, REG_HWCAP2,  0)
	FEAT_DEF(PMULL,     0x00000001, 0, REG_HWCAP2,  1)
	FEAT_DEF(SHA1,      0x00000001, 0, REG_HWCAP2,  2)
	FEAT_DEF(SHA2,      0x00000001, 0, REG_HWCAP2,  3)
	FEAT_DEF(CRC32,     0x00000001, 0, REG_HWCAP2,  4)
	FEAT_DEF(V7L,       0x00000001, 0, REG_PLATFORM, 0)
};
#endif

