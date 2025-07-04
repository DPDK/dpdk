/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef RTE_CPUID_H
#define RTE_CPUID_H

#ifndef RTE_TOOLCHAIN_MSVC
#include <cpuid.h>
#endif

enum cpu_register_t {
	RTE_REG_EAX = 0,
	RTE_REG_EBX,
	RTE_REG_ECX,
	RTE_REG_EDX,
};

typedef uint32_t cpuid_registers_t[4];

#ifdef RTE_TOOLCHAIN_MSVC
int
__get_cpuid_max(unsigned int e, unsigned int *s);
#endif

#endif /* RTE_CPUID_H */
