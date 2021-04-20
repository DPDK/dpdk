/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Neil Horman <nhorman@tuxdriver.com>.
 * All rights reserved.
 */

#ifndef _RTE_COMPAT_H_
#define _RTE_COMPAT_H_

#ifndef ALLOW_EXPERIMENTAL_API

#define __rte_experimental \
__attribute__((deprecated("Symbol is not yet part of stable ABI"), \
section(".text.experimental")))

#else

#define __rte_experimental \
__attribute__((section(".text.experimental")))

#endif

#ifndef __has_attribute
/* if no has_attribute assume no support for attribute too */
#define __has_attribute(x) 0
#endif

#if !defined ALLOW_INTERNAL_API && __has_attribute(error) /* For GCC */

#define __rte_internal \
__attribute__((error("Symbol is not public ABI"), \
section(".text.internal")))

#elif !defined ALLOW_INTERNAL_API && __has_attribute(diagnose_if) /* For clang */

#define __rte_internal \
__attribute__((diagnose_if(1, "Symbol is not public ABI", "error"), \
section(".text.internal")))

#else

#define __rte_internal \
__attribute__((section(".text.internal")))

#endif

#endif /* _RTE_COMPAT_H_ */
