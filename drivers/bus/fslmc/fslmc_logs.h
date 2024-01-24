/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016 NXP
 *
 */

#ifndef _FSLMC_LOGS_H_
#define _FSLMC_LOGS_H_

extern int dpaa2_logtype_bus;

#define DPAA2_BUS_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa2_logtype_bus, "fslmc: " fmt "\n", \
		##args)

/* Debug logs are with Function names */
#define DPAA2_BUS_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa2_logtype_bus, "fslmc: %s(): " fmt "\n", \
		__func__, ##args)

#define DPAA2_BUS_INFO(fmt, args...) \
	DPAA2_BUS_LOG(INFO, fmt, ## args)
#define DPAA2_BUS_ERR(fmt, args...) \
	DPAA2_BUS_LOG(ERR, fmt, ## args)
#define DPAA2_BUS_WARN(fmt, args...) \
	DPAA2_BUS_LOG(WARNING, fmt, ## args)

#endif /* _FSLMC_LOGS_H_ */
