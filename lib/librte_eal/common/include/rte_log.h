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

#ifndef _RTE_LOG_H_
#define _RTE_LOG_H_

/**
 * @file
 *
 * RTE Logs API
 *
 * This file provides a log API to RTE applications.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

/** The rte_log structure. */
struct rte_logs {
	uint32_t type;  /**< Bitfield with enabled logs. */
	uint32_t level; /**< Log level. */
	FILE *file;     /**< Pointer to current FILE* for logs. */
};

/** Global log informations */
extern struct rte_logs rte_logs;

/* SDK log type */
#define RTE_LOGTYPE_EAL     0x00000001 /**< Log related to eal. */
#define RTE_LOGTYPE_MALLOC  0x00000002 /**< Log related to malloc. */
#define RTE_LOGTYPE_RING    0x00000004 /**< Log related to ring. */
#define RTE_LOGTYPE_MEMPOOL 0x00000008 /**< Log related to mempool. */
#define RTE_LOGTYPE_TIMER   0x00000010 /**< Log related to timers. */
#define RTE_LOGTYPE_PMD     0x00000020 /**< Log related to poll mode driver. */
#define RTE_LOGTYPE_HASH    0x00000040 /**< Log related to hash table. */
#define RTE_LOGTYPE_LPM     0x00000080 /**< Log related to LPM. */
#define RTE_LOGTYPE_KNI     0x00000100 /**< Log related to KNI. */
#define RTE_LOGTYPE_ACL     0x00000200 /**< Log related to ACL. */
#define RTE_LOGTYPE_POWER   0x00000400 /**< Log related to power. */
#define RTE_LOGTYPE_METER   0x00000800 /**< Log related to QoS meter. */
#define RTE_LOGTYPE_SCHED   0x00001000 /**< Log related to QoS port scheduler. */
#define RTE_LOGTYPE_PORT    0x00002000 /**< Log related to port. */
#define RTE_LOGTYPE_TABLE   0x00004000 /**< Log related to table. */
#define RTE_LOGTYPE_PIPELINE 0x00008000 /**< Log related to pipeline. */

/* these log types can be used in an application */
#define RTE_LOGTYPE_USER1   0x01000000 /**< User-defined log type 1. */
#define RTE_LOGTYPE_USER2   0x02000000 /**< User-defined log type 2. */
#define RTE_LOGTYPE_USER3   0x04000000 /**< User-defined log type 3. */
#define RTE_LOGTYPE_USER4   0x08000000 /**< User-defined log type 4. */
#define RTE_LOGTYPE_USER5   0x10000000 /**< User-defined log type 5. */
#define RTE_LOGTYPE_USER6   0x20000000 /**< User-defined log type 6. */
#define RTE_LOGTYPE_USER7   0x40000000 /**< User-defined log type 7. */
#define RTE_LOGTYPE_USER8   0x80000000 /**< User-defined log type 8. */

/* Can't use 0, as it gives compiler warnings */
#define RTE_LOG_EMERG    1U  /**< System is unusable.               */
#define RTE_LOG_ALERT    2U  /**< Action must be taken immediately. */
#define RTE_LOG_CRIT     3U  /**< Critical conditions.              */
#define RTE_LOG_ERR      4U  /**< Error conditions.                 */
#define RTE_LOG_WARNING  5U  /**< Warning conditions.               */
#define RTE_LOG_NOTICE   6U  /**< Normal but significant condition. */
#define RTE_LOG_INFO     7U  /**< Informational.                    */
#define RTE_LOG_DEBUG    8U  /**< Debug-level messages.             */

/** The default log stream. */
extern FILE *eal_default_log_stream;

/**
 * Change the stream that will be used by the logging system.
 *
 * This can be done at any time. The f argument represents the stream
 * to be used to send the logs. If f is NULL, the default output is
 * used (stderr).
 *
 * @param f
 *   Pointer to the stream.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int rte_openlog_stream(FILE *f);

/**
 * Set the global log level.
 *
 * After this call, all logs that are lower or equal than level and
 * lower or equal than the RTE_LOG_LEVEL configuration option will be
 * displayed.
 *
 * @param level
 *   Log level. A value between RTE_LOG_EMERG (1) and RTE_LOG_DEBUG (8).
 */
void rte_set_log_level(uint32_t level);

/**
 * Get the global log level.
 */
uint32_t rte_get_log_level(void);

/**
 * Enable or disable the log type.
 *
 * @param type
 *   Log type, for example, RTE_LOGTYPE_EAL.
 * @param enable
 *   True for enable; false for disable.
 */
void rte_set_log_type(uint32_t type, int enable);

/**
 * Get the global log type.
 */
uint32_t rte_get_log_type(void);

/**
 * Get the current loglevel for the message being processed.
 *
 * Before calling the user-defined stream for logging, the log
 * subsystem sets a per-lcore variable containing the loglevel and the
 * logtype of the message being processed. This information can be
 * accessed by the user-defined log output function through this
 * function.
 *
 * @return
 *   The loglevel of the message being processed.
 */
int rte_log_cur_msg_loglevel(void);

/**
 * Get the current logtype for the message being processed.
 *
 * Before calling the user-defined stream for logging, the log
 * subsystem sets a per-lcore variable containing the loglevel and the
 * logtype of the message being processed. This information can be
 * accessed by the user-defined log output function through this
 * function.
 *
 * @return
 *   The logtype of the message being processed.
 */
int rte_log_cur_msg_logtype(void);

/**
 * Enable or disable the history (enabled by default)
 *
 * @param enable
 *   true to enable, or 0 to disable history.
 */
void rte_log_set_history(int enable);

/**
 * Dump the log history to a file
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_log_dump_history(FILE *f);

/**
 * Add a log message to the history.
 *
 * This function can be called from a user-defined log stream. It adds
 * the given message in the history that can be dumped using
 * rte_log_dump_history().
 *
 * @param buf
 *   A data buffer containing the message to be saved in the history.
 * @param size
 *   The length of the data buffer.
 * @return
 *   - 0: Success.
 *   - (-ENOBUFS) if there is no room to store the message.
 */
int rte_log_add_in_history(const char *buf, size_t size);

/**
 * Generates a log message.
 *
 * The message will be sent in the stream defined by the previous call
 * to rte_openlog_stream().
 *
 * The level argument determines if the log should be displayed or
 * not, depending on the global rte_logs variable.
 *
 * The preferred alternative is the RTE_LOG() function because debug logs may
 * be removed at compilation time if optimization is enabled. Moreover,
 * logs are automatically prefixed by type when using the macro.
 *
 * @param level
 *   Log level. A value between RTE_LOG_EMERG (1) and RTE_LOG_DEBUG (8).
 * @param logtype
 *   The log type, for example, RTE_LOGTYPE_EAL.
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
int rte_log(uint32_t level, uint32_t logtype, const char *format, ...)
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
	__attribute__((cold))
#endif
#endif
	__attribute__((format(printf, 3, 4)));

/**
 * Generates a log message.
 *
 * The message will be sent in the stream defined by the previous call
 * to rte_openlog_stream().
 *
 * The level argument determines if the log should be displayed or
 * not, depending on the global rte_logs variable. A trailing
 * newline may be added if needed.
 *
 * The preferred alternative is the RTE_LOG() because debug logs may be
 * removed at compilation time.
 *
 * @param level
 *   Log level. A value between RTE_LOG_EMERG (1) and RTE_LOG_DEBUG (8).
 * @param logtype
 *   The log type, for example, RTE_LOGTYPE_EAL.
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @param ap
 *   The va_list of the variable arguments required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
int rte_vlog(uint32_t level, uint32_t logtype, const char *format, va_list ap)
	__attribute__((format(printf,3,0)));

/**
 * Generates a log message.
 *
 * The RTE_LOG() is equivalent to rte_log() with two differences:

 * - RTE_LOG() can be used to remove debug logs at compilation time,
 *   depending on RTE_LOG_LEVEL configuration option, and compilation
 *   optimization level. If optimization is enabled, the tests
 *   involving constants only are pre-computed. If compilation is done
 *   with -O0, these tests will be done at run time.
 * - The log level and log type names are smaller, for example:
 *   RTE_LOG(INFO, EAL, "this is a %s", "log");
 *
 * @param l
 *   Log level. A value between EMERG (1) and DEBUG (8). The short name is
 *   expanded by the macro, so it cannot be an integer value.
 * @param t
 *   The log type, for example, EAL. The short name is expanded by the
 *   macro, so it cannot be an integer value.
 * @param fmt
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @param args
 *   The variable list of arguments according to the format string.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
#define RTE_LOG(l, t, ...)					\
	(void)(((RTE_LOG_ ## l <= RTE_LOG_LEVEL) &&			\
	  (RTE_LOG_ ## l <= rte_logs.level) &&			\
	  (RTE_LOGTYPE_ ## t & rte_logs.type)) ?		\
	 rte_log(RTE_LOG_ ## l,					\
		 RTE_LOGTYPE_ ## t, # t ": " __VA_ARGS__) :	\
	 0)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LOG_H_ */
