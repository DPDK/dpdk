/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Red Hat, Inc.
 */

#ifndef EAL_EXPORT_H
#define EAL_EXPORT_H

#include <rte_common.h>

/* Internal macros for exporting symbols, used by the build system.
 * For RTE_EXPORT_EXPERIMENTAL_SYMBOL, ver indicates the
 * version this symbol was introduced in.
 */
#define RTE_EXPORT_EXPERIMENTAL_SYMBOL(symbol, ver)
#define RTE_EXPORT_INTERNAL_SYMBOL(symbol)
#define RTE_EXPORT_SYMBOL(symbol)

#if !defined(RTE_USE_FUNCTION_VERSIONING) && (defined(RTE_CC_GCC) || defined(RTE_CC_CLANG))
#define VERSIONING_WARN RTE_PRAGMA_WARNING(Use of function versioning disabled. \
	Is "use_function_versioning=true" in meson.build?)
#else
#define VERSIONING_WARN
#endif

/*
 * The macros below provide backwards compatibility when updating exported functions.
 * When a symbol is exported from a library to provide an API, it also provides a
 * calling convention (ABI) that is embodied in its name, return type,
 * arguments, etc.  On occasion that function may need to change to accommodate
 * new functionality, behavior, etc.  When that occurs, it is desirable to
 * allow for backwards compatibility for a time with older binaries that are
 * dynamically linked to the dpdk.
 */

#ifdef RTE_BUILD_SHARED_LIB

/*
 * Create a symbol version table entry binding symbol <name>@DPDK_<ver> to the internal
 * function name <name>_v<ver>.
 */
#define RTE_VERSION_SYMBOL(ver, type, name, args) VERSIONING_WARN \
__asm__(".symver " RTE_STR(name) "_v" RTE_STR(ver) ", " RTE_STR(name) "@DPDK_" RTE_STR(ver)); \
__rte_used type name ## _v ## ver args; \
type name ## _v ## ver args

/*
 * Similar to RTE_VERSION_SYMBOL but for experimental API symbols.
 * This is mainly used for keeping compatibility for symbols that get promoted to stable ABI.
 */
#define RTE_VERSION_EXPERIMENTAL_SYMBOL(type, name, args) VERSIONING_WARN \
__asm__(".symver " RTE_STR(name) "_exp, " RTE_STR(name) "@EXPERIMENTAL") \
__rte_used type name ## _exp args; \
type name ## _exp args

/*
 * Create a symbol version entry instructing the linker to bind references to
 * symbol <name> to the internal symbol <name>_v<ver>.
 */
#define RTE_DEFAULT_SYMBOL(ver, type, name, args) VERSIONING_WARN \
__asm__(".symver " RTE_STR(name) "_v" RTE_STR(ver) ", " RTE_STR(name) "@@DPDK_" RTE_STR(ver)); \
__rte_used type name ## _v ## ver args; \
type name ## _v ## ver args

#else /* !RTE_BUILD_SHARED_LIB */

#define RTE_VERSION_SYMBOL(ver, type, name, args) VERSIONING_WARN \
type name ## _v ## ver args; \
type name ## _v ## ver args

#define RTE_VERSION_EXPERIMENTAL_SYMBOL(type, name, args) VERSIONING_WARN \
type name ## _exp args; \
type name ## _exp args

#define RTE_DEFAULT_SYMBOL(ver, type, name, args) VERSIONING_WARN \
type name args

#endif /* RTE_BUILD_SHARED_LIB */

#endif /* EAL_EXPORT_H */
