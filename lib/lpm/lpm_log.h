/* SPDX-License-Identifier: BSD-3-Clause */

extern int lpm_logtype;
#define RTE_LOGTYPE_LPM lpm_logtype
#define LPM_LOG(level, ...) \
	RTE_LOG_LINE(level, LPM, "" __VA_ARGS__)
