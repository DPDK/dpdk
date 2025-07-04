/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef __CFA_TRACE_H_
#define __CFA_TRACE_H_

/*!
 * \file
 * \brief CFA logging macros and functions
 *  @{
 */

#ifndef COMP_ID
#error COMP_ID must be defined in the module including this file.
#endif

/* These must be defined before the platform specific header is included. */
#ifndef CFA_DEBUG_LEVEL_DBG
#define CFA_DEBUG_LEVEL_DBG 0x0
#define CFA_DEBUG_LEVEL_INFO 0x1
#define CFA_DEBUG_LEVEL_WARN 0x2
#define CFA_DEBUG_LEVEL_CRITICAL 0x3
#define CFA_DEBUG_LEVEL_FATAL 0x4
#endif

/* Include platform specific CFA logging api header */
#include "cfa_debug_defs.h"

/* #define CFA_DYNAMIC_TRACE_FILTERING */

/** @name Default Log Levels
 * Default log level for each component.  If not defined for a component, the
 * log level for that component defaults to 0 (CFA_DEBUG_LEVEL_DBG).
 * @{
 */
#define CFA_BLD_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_CMM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_GIM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_HOSTIF_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_IDM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_OIM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_RM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_SM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_TBM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_TCM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_TIM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_TPM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
#define CFA_TSM_DEBUG_LEVEL CFA_DEBUG_LEVEL_INFO
/** @} */

/* Do not use these macros directly */

/* \cond DO_NOT_DOCUMENT */
#define JOIN(a, b) a##b
#define JOIN3(a, b, c) a##b##c
#define CFA_COMP_NAME(ID) JOIN(CFA_COMP_, ID)
#define CFA_COMP_DBG_LEVEL(ID) JOIN3(CFA_, ID, _DEBUG_LEVEL)
#define CFA_TRACE_STRINGIFY(x) #x
#define CFA_TRACE_LINE_STRING(line) CFA_TRACE_STRINGIFY(line)
#define CFA_TRACE_LINE() CFA_TRACE_LINE_STRING(__LINE__)
#define CFA_LOG(level, format, ...)                                            \
	CFA_TRACE(level, "%s:" CFA_TRACE_LINE() ": " format, __func__,         \
		  ##__VA_ARGS__)

#define CFA_LOG_FL(function, line, level, format, ...)                         \
	CFA_TRACE(level, "%s: %s: " format, function, line, ##__VA_ARGS__)

#ifdef CFA_DYNAMIC_TRACE_FILTERING
#define CFA_TRACE_FILTERED(component, level, format, ...)                      \
	do {                                                                   \
		int local_level = level;                                       \
		if (cfa_trace_enabled(component, local_level))                 \
			CFA_LOG(local_level, format, ##__VA_ARGS__);           \
	} while (0)

#define CFA_TRACE_FILTERED_FL(function, line, component, level, format, ...)   \
	do {                                                                   \
		int local_level = level;                                       \
		if (cfa_trace_enabled(component, local_level))                 \
			CFA_LOG_FL(function, line, local_level, format,        \
				   ##__VA_ARGS__);                             \
	} while (0)

#else
/* Static log filtering */
#if CFA_COMP_DBG_LEVEL(COMP_ID) <= CFA_DEBUG_LEVEL_DBG
#define CFA_TRACE_DBG(format, ...)                                             \
	CFA_LOG(CFA_DEBUG_LEVEL_DBG, format, ##__VA_ARGS__)
#define CFA_TRACE_DBG_FL(function, line, format, ...)                          \
	CFA_LOG_FL(function, line, CFA_DEBUG_LEVEL_DBG, format, ##__VA_ARGS__)
#else
#define CFA_TRACE_DBG(format, ...)
#define CFA_TRACE_DBG_FL(format, ...)
#endif
#if CFA_COMP_DBG_LEVEL(COMP_ID) <= CFA_DEBUG_LEVEL_INFO
#define CFA_TRACE_INFO(format, ...)                                            \
	CFA_LOG(CFA_DEBUG_LEVEL_INFO, format, ##__VA_ARGS__)
#define CFA_TRACE_INFO_FL(function, line, format, ...)                         \
	CFA_LOG_FL(function, line, CFA_DEBUG_LEVEL_INFO, format, ##__VA_ARGS__)
#else
#define CFA_TRACE_INFO(format, ...)
#define CFA_TRACE_INFO_FL(function, line, format, ...)
#endif
#if CFA_COMP_DBG_LEVEL(COMP_ID) <= CFA_DEBUG_LEVEL_WARN
#define CFA_TRACE_WARN(format, ...)                                            \
	CFA_LOG(CFA_DEBUG_LEVEL_WARN, format, ##__VA_ARGS__)
#define CFA_TRACE_WARN_FL(function, line, format, ...)                         \
	CFA_LOG_FL(function, line, CFA_DEBUG_LEVEL_WARN, format, ##__VA_ARGS__)
#else
#define CFA_TRACE_WARN(format, ...)
#define CFA_TRACE_WARN_FL(function, line, format, ...)
#endif
#if CFA_COMP_DBG_LEVEL(COMP_ID) <= CFA_DEBUG_LEVEL_CRITICAL
#define CFA_TRACE_ERR(format, ...)                                             \
	CFA_LOG(CFA_DEBUG_LEVEL_CRITICAL, format, ##__VA_ARGS__)
#define CFA_TRACE_ERR_FL(function, line, format, ...)                          \
	CFA_LOG_FL(function, line, CFA_DEBUG_LEVEL_CRITICAL, format,           \
		   ##__VA_ARGS__)
#else
#define CFA_TRACE_ERR(format, ...)
#define CFA_TRACE_ERR_FL(function, line, format, ...)
#endif
#define CFA_TRACE_FATAL(format, ...)                                           \
	CFA_LOG(CFA_DEBUG_LEVEL_FATAL, format, ##__VA_ARGS__)

#endif
/* \endcond */

/** @name Logging Macros
 * These macros log with the function and line number of the location invoking
 * the macro.
 * @{
 */
#ifdef CFA_DYNAMIC_TRACE_FILTERING
#define CFA_LOG_DBG(format, ...)                                               \
	CFA_TRACE_FILTERED(CFA_COMP_NAME(COMP_ID), CFA_DEBUG_LEVEL_DBG,        \
			   format, ##__VA_ARGS__)
#define CFA_LOG_INFO(COMP_ID, format, ...)                                     \
	CFA_TRACE_FILTERED(CFA_COMP_NAME(COMP_ID), CFA_DEBUG_LEVEL_INFO,       \
			   format, ##__VA_ARGS__)
#define CFA_LOG_WARN(format, ...)                                              \
	CFA_TRACE_FILTERED(CFA_COMP_NAME(COMP_ID), CFA_DEBUG_LEVEL_WARN,       \
			   format, ##__VA_ARGS__)
#define CFA_LOG_ERR(format, ...)                                               \
	CFA_TRACE_FILTERED(CFA_COMP_NAME(COMP_ID), CFA_DEBUG_LEVEL_CRITICAL,   \
			   format, ##__VA_ARGS__)
#define CFA_LOG_FATAL(format, ...)                                             \
	CFA_TRACE_FILTERED(CFA_COMP_NAME(COMP_ID), CFA_DEBUG_LEVEL_FATAL,      \
			   format, ##__VA_ARGS__)
#else
#define CFA_LOG_DBG(format, ...) CFA_TRACE_DBG(format, ##__VA_ARGS__)
#define CFA_LOG_INFO(format, ...) CFA_TRACE_INFO(format, ##__VA_ARGS__)
#define CFA_LOG_WARN(format, ...) CFA_TRACE_WARN(format, ##__VA_ARGS__)
#define CFA_LOG_ERR(format, ...) CFA_TRACE_ERR(format, ##__VA_ARGS__)
#define CFA_LOG_FATAL(format, ...) CFA_TRACE_FATAL(format, ##__VA_ARGS__)
#endif
/** @} */

/** @name Logging Macros with Function
 * These macros log with the function and line number passed into
 * the macro.
 * @{
 */
#ifdef CFA_DYNAMIC_TRACE_FILTERING
#define CFA_LOG_DBG_FL(function, line, format, ...)                            \
	CFA_TRACE_FILTERED_FL(function, line, CFA_COMP_NAME(COMP_ID),          \
			      CFA_DEBUG_LEVEL_DBG, format, ##__VA_ARGS__)
#define CFA_LOG_INFO_FL(function, line, format, ...)                           \
	CFA_TRACE_FILTERED_FL(function, line, CFA_COMP_NAME(COMP_ID),          \
			      CFA_DEBUG_LEVEL_INFO, format, ##__VA_ARGS__)
#define CFA_LOG_WARN_FL(function, line, format, ...)                           \
	CFA_TRACE_FILTERED_FL(function, line, CFA_COMP_NAME(COMP_ID),          \
			      CFA_DEBUG_LEVEL_WARN, format, ##__VA_ARGS__)
#define CFA_LOG_ERR_FL(function, line, format, ...)                            \
	CFA_TRACE_FILTERED_FL(function, line, CFA_COMP_NAME(COMP_ID),          \
			      CFA_DEBUG_LEVEL_CRITICAL, format, ##__VA_ARGS__)
#define CFA_LOG_FATAL_FL(function, line, format, ...)                          \
	CFA_TRACE_FILTERED_FL(function, line, CFA_COMP_NAME(COMP_ID),          \
			      CFA_DEBUG_LEVEL_FATAL, format, ##__VA_ARGS__)
#else
#define CFA_LOG_DBG_FL(function, line, format, ...)                            \
	CFA_TRACE_DBG_FL(function, line, format, ##__VA_ARGS__)
#define CFA_LOG_INFO_FL(function, line, format, ...)                           \
	CFA_TRACE_INFO_FL(function, line, format, ##__VA_ARGS__)
#define CFA_LOG_WARN_FL(function, line, format, ...)                           \
	CFA_TRACE_WARN_FL(function, line, format, ##__VA_ARGS__)
#define CFA_LOG_ERR_FL(function, line, format, ...)                            \
	CFA_TRACE_ERR_FL(function, line, format, ##__VA_ARGS__)
#define CFA_LOG_FATAL_FL(function, line, format, ...)                          \
	CFA_TRACE_FATAL_FL(function, line, format, ##__VA_ARGS__)
#endif
/** @} */

/**
 * CFA components
 */
enum cfa_components {
	CFA_COMP_BLD = 0,
	CFA_COMP_FIRST = CFA_COMP_BLD,
	CFA_COMP_CMM,
	CFA_COMP_GIM,
	CFA_COMP_HOSTIF,
	CFA_COMP_IDM,
	CFA_COMP_OIM,
	CFA_COMP_RM,
	CFA_COMP_SM,
	CFA_COMP_TBM,
	CFA_COMP_TCM,
	CFA_COMP_TIM,
	CFA_COMP_TPM,
	CFA_COMP_TSM,
	CFA_COMP_MAX
};

#ifdef CFA_DYNAMIC_TRACE_FILTERING
/**
 * CFA logging system initialization
 *
 * This API initializes the CFA logging infrastructure
 *
 * @return
 *  0 for SUCCESS, negative error value for FAILURE
 */
int cfa_trace_init(void);

/**
 * CFA logging check if message permitted
 *
 * This API indicates if a log message for a component at a given level should
 * be issued
 *
 * @param[in] component
 *  The CFA component
 *
 * @param[in] level
 *  The logging level to check for the component
 *
 * @return
 *  0 if message not permitted, non-zero if the message is permitted
 */
int cfa_trace_enabled(enum cfa_components component, int level);

/**
 * CFA logging level set
 *
 * This API set the minimum level of log messages to be issued for a component
 *
 * @param[in] component
 *  The CFA component
 *
 * @param[in] level
 *  The logging level to set for the component
 *
 * @return
 *  0 for SUCCESS, negative error value for FAILURE
 */
int cfa_trace_level_set(enum cfa_components component, int level);

#endif /* CFA_DYNAMIC_TRACE_FILTERING */

/** @} */

#endif /* __CFA_TRACE_H_ */
