/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Neil Horman <nhorman@tuxdriver.com>.
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

#ifndef _RTE_COMPAT_H_
#define _RTE_COMPAT_H_
#include <rte_common.h>

#ifdef RTE_BUILD_SHARED_LIB

/*
 * Provides backwards compatibility when updating exported functions.
 * When a symol is exported from a library to provide an API, it also provides a
 * calling convention (ABI) that is embodied in its name, return type,
 * arguments, etc.  On occasion that function may need to change to accommodate
 * new functionality, behavior, etc.  When that occurs, it is desirable to
 * allow for backwards compatibility for a time with older binaries that are
 * dynamically linked to the dpdk.  To support that, the __vsym and
 * VERSION_SYMBOL macros are created.  They, in conjunction with the
 * <library>_version.map file for a given library allow for multiple versions of
 * a symbol to exist in a shared library so that older binaries need not be
 * immediately recompiled.
 *
 * Refer to the guidelines document in the docs subdirectory for details on the
 * use of these macros
 */

/*
 * Macro Parameters:
 * b - function base name
 * e - function version extension, to be concatenated with base name
 * n - function symbol version string to be applied
 * f - function prototype
 * p - full function symbol name
 */

/*
 * VERSION_SYMBOL
 * Creates a symbol version table entry binding symbol <b>@DPDK_<n> to the internal
 * function name <b>_<e>
 */
#define VERSION_SYMBOL(b, e, n) __asm__(".symver " RTE_STR(b) RTE_STR(e) ", " RTE_STR(b) "@DPDK_" RTE_STR(n))

/*
 * BIND_DEFAULT_SYMBOL
 * Creates a symbol version entry instructing the linker to bind references to
 * symbol <b> to the internal symbol <b>_<e>
 */
#define BIND_DEFAULT_SYMBOL(b, e, n) __asm__(".symver " RTE_STR(b) RTE_STR(e) ", " RTE_STR(b) "@@DPDK_" RTE_STR(n))
#define __vsym __attribute__((used))

/*
 * MAP_STATIC_SYMBOL
 * If a function has been bifurcated into multiple versions, none of which
 * are defined as the exported symbol name in the map file, this macro can be
 * used to alias a specific version of the symbol to its exported name.  For
 * example, if you have 2 versions of a function foo_v1 and foo_v2, where the
 * former is mapped to foo@DPDK_1 and the latter is mapped to foo@DPDK_2 when
 * building a shared library, this macro can be used to map either foo_v1 or
 * foo_v2 to the symbol foo when building a static library, e.g.:
 * MAP_STATIC_SYMBOL(void foo(), foo_v2);
 */
#define MAP_STATIC_SYMBOL(f, p)

#else
/*
 * No symbol versioning in use
 */
#define VERSION_SYMBOL(b, e, n)
#define __vsym
#define BIND_DEFAULT_SYMBOL(b, e, n)
#define MAP_STATIC_SYMBOL(f, p) f __attribute__((alias(RTE_STR(p))))
/*
 * RTE_BUILD_SHARED_LIB=n
 */
#endif


#endif /* _RTE_COMPAT_H_ */
