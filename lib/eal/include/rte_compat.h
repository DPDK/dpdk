/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Neil Horman <nhorman@tuxdriver.com>.
 * All rights reserved.
 */

#ifndef _RTE_COMPAT_H_
#define _RTE_COMPAT_H_

#ifdef RTE_TOOLCHAIN_MSVC

#define __rte_experimental
#define __rte_internal

#else /* !RTE_TOOLCHAIN_MSVC */

#ifndef ALLOW_EXPERIMENTAL_API

#define __rte_experimental \
__attribute__((deprecated("Symbol is not yet part of stable ABI"), \
section(".text.experimental")))

#endif /* !ALLOW_EXPERIMENTAL_API */

#ifndef __rte_experimental
#define __rte_experimental \
__attribute__((section(".text.experimental")))
#endif

#ifndef ALLOW_INTERNAL_API

#ifndef __has_attribute
/* if no has_attribute assume no support for attribute too */
#define __has_attribute(x) 0
#endif

#if __has_attribute(diagnose_if) /* For Clang */

#define __rte_internal \
_Pragma("GCC diagnostic push") \
_Pragma("GCC diagnostic ignored \"-Wgcc-compat\"") \
__attribute__((diagnose_if(1, "Symbol is not public ABI", "error"), \
section(".text.internal"))) \
_Pragma("GCC diagnostic pop")

#elif __has_attribute(error) /* For GCC */

#define __rte_internal \
__attribute__((error("Symbol is not public ABI"), \
section(".text.internal")))

#endif

#endif /* !ALLOW_INTERNAL_API */

#ifndef __rte_internal
#define __rte_internal \
__attribute__((section(".text.internal")))
#endif

#endif /* !RTE_TOOLCHAIN_MSVC */

#endif /* _RTE_COMPAT_H_ */
