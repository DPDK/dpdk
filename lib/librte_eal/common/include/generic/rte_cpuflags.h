/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _RTE_CPUFLAGS_H_
#define _RTE_CPUFLAGS_H_

/**
 * @file
 * Architecture specific API to determine available CPU features at runtime.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

/**
 * Enumeration of all CPU features supported
 */
enum rte_cpu_flag_t;

/**
 * Enumeration of CPU registers
 */
enum cpu_register_t;

typedef uint32_t cpuid_registers_t[4];

#define CPU_FLAG_NAME_MAX_LEN 64

/**
 * Struct to hold a processor feature entry
 */
struct feature_entry {
	uint32_t leaf;				/**< cpuid leaf */
	uint32_t subleaf;			/**< cpuid subleaf */
	uint32_t reg;				/**< cpuid register */
	uint32_t bit;				/**< cpuid register bit */
	char name[CPU_FLAG_NAME_MAX_LEN];       /**< String for printing */
};

#define FEAT_DEF(name, leaf, subleaf, reg, bit) \
	[RTE_CPUFLAG_##name] = {leaf, subleaf, reg, bit, #name },

/**
 * An array that holds feature entries
 */
static const struct feature_entry cpu_feature_table[];

/**
 * Execute CPUID instruction and get contents of a specific register
 *
 * This function, when compiled with GCC, will generate architecture-neutral
 * code, as per GCC manual.
 */
static inline void
rte_cpu_get_features(uint32_t leaf, uint32_t subleaf, cpuid_registers_t out);

/**
 * Function for checking a CPU flag availability
 *
 * @param flag
 *     CPU flag to query CPU for
 * @return
 *     1 if flag is available
 *     0 if flag is not available
 *     -ENOENT if flag is invalid
 */
static inline int
rte_cpu_get_flag_enabled(enum rte_cpu_flag_t feature);

/**
 * This function checks that the currently used CPU supports the CPU features
 * that were specified at compile time. It is called automatically within the
 * EAL, so does not need to be used by applications.
 */
void
rte_cpu_check_supported(void);

#endif /* _RTE_CPUFLAGS_H_ */
