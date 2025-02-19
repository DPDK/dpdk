/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Red Hat, Inc.
 */

#ifndef EAL_EXPORT_H
#define EAL_EXPORT_H

/* Internal macros for exporting symbols, used by the build system.
 * For RTE_EXPORT_EXPERIMENTAL_SYMBOL, ver indicates the
 * version this symbol was introduced in.
 */
#define RTE_EXPORT_EXPERIMENTAL_SYMBOL(symbol, ver)
#define RTE_EXPORT_INTERNAL_SYMBOL(symbol)
#define RTE_EXPORT_SYMBOL(symbol)

#endif /* EAL_EXPORT_H */
