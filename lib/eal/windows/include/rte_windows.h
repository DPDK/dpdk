/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _RTE_WINDOWS_H_
#define _RTE_WINDOWS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file Windows-specific facilities
 *
 * This file should be included by DPDK libraries and applications
 * that need access to Windows API. It includes platform SDK headers
 * in compatible order with proper options and defines error-handling macros.
 */

/* Disable excessive libraries. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* Override Windows SDK definition of _m_prefetchw to avoid conflicting types */
#ifdef RTE_TOOLCHAIN_CLANG
#undef _m_prefetchw
#define _m_prefetchw __m_prefetchw
#endif

/* Must come first. */
#include <windows.h>

#include <basetsd.h>
#include <psapi.h>
#include <setupapi.h>
#include <winioctl.h>

/* Have GUIDs defined. */
#ifndef INITGUID
#define INITGUID
#endif
#include <initguid.h>
#include <devguid.h>
#include <rte_log.h>

/**
 * Log GetLastError() with context, usually a Win32 API function and arguments.
 */
#define RTE_LOG_WIN32_ERR(...) \
	RTE_LOG(DEBUG, EAL, RTE_FMT("GetLastError()=%lu: " \
		RTE_FMT_HEAD(__VA_ARGS__,) "\n", GetLastError(), \
		RTE_FMT_TAIL(__VA_ARGS__,)))

#ifdef __cplusplus
}
#endif

#endif /* _RTE_WINDOWS_H_ */
