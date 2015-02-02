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
 * new functionality, behavior, etc.  When that occurs, it is desireable to
 * allow for backwards compatibility for a time with older binaries that are
 * dynamically linked to the dpdk.  To support that, the __vsym and
 * VERSION_SYMBOL macros are created.  They, in conjunction with the
 * <library>_version.map file for a given library allow for multiple versions of
 * a symbol to exist in a shared library so that older binaries need not be
 * immediately recompiled. Their use is outlined in the following example:
 * Assumptions: DPDK 2.(X) contains a function int foo(char *string)
 *              DPDK 2.(X+1) needs to change foo to be int foo(int index)
 *
 * To accomplish this:
 * 1) Edit lib/<library>/library_version.map to add a DPDK_2.(X+1) node, in which
 * foo is exported as a global symbol.
 *
 * 2) rename the existing function int foo(char *string) to
 * 	int __vsym foo_v20(char *string)
 *
 * 3) Add this macro immediately below the function
 * 	VERSION_SYMBOL(foo, _v20, 2.0);
 *
 * 4) Implement a new version of foo.
 * 	char foo(int value, int otherval) { ...}
 *
 * 5) Mark the newest version as the default version
 * 	BIND_DEFAULT_SYMBOL(foo, 2.1);
 *
 */

/*
 * Macro Parameters:
 * b - function base name
 * e - function version extension, to be concatenated with base name
 * n - function symbol version string to be applied
 */

/*
 * VERSION_SYMBOL
 * Creates a symbol version table entry binding symbol <b>@DPDK_<n> to the internal
 * function name <b>_<e>
 */
#define VERSION_SYMBOL(b, e, n) __asm__(".symver " RTE_STR(b) RTE_STR(e) ", "RTE_STR(b)"@DPDK_"RTE_STR(n))

/*
 * BASE_SYMBOL
 * Creates a symbol version table entry binding unversioned symbol <b>
 * to the internal function <b>_<e>
 */
#define BASE_SYMBOL(b, e) __asm__(".symver " RTE_STR(b) RTE_STR(e) ", "RTE_STR(b)"@")

/*
 * BNID_DEFAULT_SYMBOL
 * Creates a symbol version entry instructing the linker to bind references to
 * symbol <b> to the internal symbol <b>_<e>
 */
#define BIND_DEFAULT_SYMBOL(b, e, n) __asm__(".symver " RTE_STR(b) RTE_STR(e) ", "RTE_STR(b)"@@DPDK_"RTE_STR(n))
#define __vsym __attribute__((used))

#else
/*
 * No symbol versioning in use
 */
#define VERSION_SYMBOL(b, e, v)
#define __vsym
#define BASE_SYMBOL(b, n)
#define BIND_DEFAULT_SYMBOL(b, v)

/*
 * RTE_BUILD_SHARED_LIB=n
 */
#endif


#endif /* _RTE_COMPAT_H_ */
