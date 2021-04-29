/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_RAWDEV_H_
#define _IFPGA_RAWDEV_H_

extern int ifpga_rawdev_logtype;

#define IFPGA_RAWDEV_NAME_FMT "IFPGA:%02x:%02x.%x"

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

#define IFPGA_RAWDEV_MSIX_IRQ_NUM 7
#define IFPGA_RAWDEV_NUM 32

struct ifpga_rawdev {
	int dev_id;
	struct rte_rawdev *rawdev;
	int aer_enable;
	int intr_fd[IFPGA_RAWDEV_MSIX_IRQ_NUM+1];
	uint32_t aer_old[2];
	char fvl_bdf[8][16];
	char parent_bdf[16];
};

struct ifpga_rawdev *
ifpga_rawdev_get(const struct rte_rawdev *rawdev);

enum ifpga_irq_type {
	IFPGA_FME_IRQ = 0,
	IFPGA_AFU_IRQ = 1,
};

int
ifpga_register_msix_irq(struct rte_rawdev *dev, int port_id,
		enum ifpga_irq_type type, int vec_start, int count,
		rte_intr_callback_fn handler, const char *name,
		void *arg);
int
ifpga_unregister_msix_irq(enum ifpga_irq_type type,
		int vec_start, rte_intr_callback_fn handler, void *arg);

#endif /* _IFPGA_RAWDEV_H_ */
