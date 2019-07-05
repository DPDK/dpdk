/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_RAWDEV_H_
#define _IFPGA_RAWDEV_H_

extern int ifpga_rawdev_logtype;

#define IFPGA_RAWDEV_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ifpga_rawdev_logtype, "%s(): " fmt "\n", \
				__func__, ##args)

#define IFPGA_RAWDEV_PMD_FUNC_TRACE() IFPGA_RAWDEV_PMD_LOG(DEBUG, ">>")

#define IFPGA_RAWDEV_PMD_DEBUG(fmt, args...) \
	IFPGA_RAWDEV_PMD_LOG(DEBUG, fmt, ## args)
#define IFPGA_RAWDEV_PMD_INFO(fmt, args...) \
	IFPGA_RAWDEV_PMD_LOG(INFO, fmt, ## args)
#define IFPGA_RAWDEV_PMD_ERR(fmt, args...) \
	IFPGA_RAWDEV_PMD_LOG(ERR, fmt, ## args)
#define IFPGA_RAWDEV_PMD_WARN(fmt, args...) \
	IFPGA_RAWDEV_PMD_LOG(WARNING, fmt, ## args)

enum ifpga_rawdev_device_state {
	IFPGA_IDLE,
	IFPGA_READY,
	IFPGA_ERROR
};

/** Set a bit in the uint64 variable */
#define IFPGA_BIT_SET(var, pos) \
	((var) |= ((uint64_t)1 << ((pos))))

/** Reset the bit in the variable */
#define IFPGA_BIT_RESET(var, pos) \
	((var) &= ~((uint64_t)1 << ((pos))))

/** Check the bit is set in the variable */
#define IFPGA_BIT_ISSET(var, pos) \
	(((var) & ((uint64_t)1 << ((pos)))) ? 1 : 0)

static inline struct opae_adapter *
ifpga_rawdev_get_priv(const struct rte_rawdev *rawdev)
{
	return rawdev->dev_private;
}

#endif /* _IFPGA_RAWDEV_H_ */
