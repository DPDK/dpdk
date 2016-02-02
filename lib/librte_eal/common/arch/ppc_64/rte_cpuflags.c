/*
 *   BSD LICENSE
 *
 *   Copyright (C) IBM Corporation 2014.
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
 *     * Neither the name of IBM Corporation nor the names of its
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

const struct feature_entry rte_cpu_feature_table[] = {
	FEAT_DEF(PPC_LE, 0x00000001, 0, REG_HWCAP,  0)
	FEAT_DEF(TRUE_LE, 0x00000001, 0, REG_HWCAP,  1)
	FEAT_DEF(PSERIES_PERFMON_COMPAT, 0x00000001, 0, REG_HWCAP,  6)
	FEAT_DEF(VSX, 0x00000001, 0, REG_HWCAP,  7)
	FEAT_DEF(ARCH_2_06, 0x00000001, 0, REG_HWCAP,  8)
	FEAT_DEF(POWER6_EXT, 0x00000001, 0, REG_HWCAP,  9)
	FEAT_DEF(DFP, 0x00000001, 0, REG_HWCAP,  10)
	FEAT_DEF(PA6T, 0x00000001, 0, REG_HWCAP,  11)
	FEAT_DEF(ARCH_2_05, 0x00000001, 0, REG_HWCAP,  12)
	FEAT_DEF(ICACHE_SNOOP, 0x00000001, 0, REG_HWCAP,  13)
	FEAT_DEF(SMT, 0x00000001, 0, REG_HWCAP,  14)
	FEAT_DEF(BOOKE, 0x00000001, 0, REG_HWCAP,  15)
	FEAT_DEF(CELLBE, 0x00000001, 0, REG_HWCAP,  16)
	FEAT_DEF(POWER5_PLUS, 0x00000001, 0, REG_HWCAP,  17)
	FEAT_DEF(POWER5, 0x00000001, 0, REG_HWCAP,  18)
	FEAT_DEF(POWER4, 0x00000001, 0, REG_HWCAP,  19)
	FEAT_DEF(NOTB, 0x00000001, 0, REG_HWCAP,  20)
	FEAT_DEF(EFP_DOUBLE, 0x00000001, 0, REG_HWCAP,  21)
	FEAT_DEF(EFP_SINGLE, 0x00000001, 0, REG_HWCAP,  22)
	FEAT_DEF(SPE, 0x00000001, 0, REG_HWCAP,  23)
	FEAT_DEF(UNIFIED_CACHE, 0x00000001, 0, REG_HWCAP,  24)
	FEAT_DEF(4xxMAC, 0x00000001, 0, REG_HWCAP,  25)
	FEAT_DEF(MMU, 0x00000001, 0, REG_HWCAP,  26)
	FEAT_DEF(FPU, 0x00000001, 0, REG_HWCAP,  27)
	FEAT_DEF(ALTIVEC, 0x00000001, 0, REG_HWCAP,  28)
	FEAT_DEF(PPC601, 0x00000001, 0, REG_HWCAP,  29)
	FEAT_DEF(PPC64, 0x00000001, 0, REG_HWCAP,  30)
	FEAT_DEF(PPC32, 0x00000001, 0, REG_HWCAP,  31)
	FEAT_DEF(TAR, 0x00000001, 0, REG_HWCAP2,  26)
	FEAT_DEF(LSEL, 0x00000001, 0, REG_HWCAP2,  27)
	FEAT_DEF(EBB, 0x00000001, 0, REG_HWCAP2,  28)
	FEAT_DEF(DSCR, 0x00000001, 0, REG_HWCAP2,  29)
	FEAT_DEF(HTM, 0x00000001, 0, REG_HWCAP2,  30)
	FEAT_DEF(ARCH_2_07, 0x00000001, 0, REG_HWCAP2,  31)
};

const char *
rte_cpu_get_flag_name(enum rte_cpu_flag_t feature)
{
	if (feature >= RTE_CPUFLAG_NUMFLAGS)
		return NULL;
	return rte_cpu_feature_table[feature].name;
}
