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
	return (struct opae_adapter *)rawdev->dev_private;
}

#define IFPGA_RAWDEV_MSIX_IRQ_NUM 7
#define IFPGA_RAWDEV_NUM 32
#define IFPGA_MAX_VDEV 4
#define IFPGA_MAX_IRQ 12

struct ifpga_rawdev {
	int dev_id;
	struct rte_rawdev *rawdev;
	int aer_enable;
	int intr_fd[IFPGA_RAWDEV_MSIX_IRQ_NUM+1];
	uint32_t aer_old[2];
	char fvl_bdf[8][16];
	char parent_bdf[16];
	/* 0 for FME interrupt, others are reserved for AFU irq */
	void *intr_handle[IFPGA_MAX_IRQ];
	/* enable monitor thread poll device's sensors or not */
	int poll_enabled;
	/* name of virtual devices created on raw device */
	char *vdev_name[IFPGA_MAX_VDEV];
};

struct ifpga_vdev_args {
	char bdf[PCI_PRI_STR_SIZE];
	int port;
};

struct ifpga_rawdev *
ifpga_rawdev_get(const struct rte_rawdev *rawdev);

enum ifpga_irq_type {
	IFPGA_FME_IRQ = 0,
	IFPGA_AFU_IRQ = 1,
};

int
ifpga_register_msix_irq(struct ifpga_rawdev *dev, int port_id,
		enum ifpga_irq_type type, int vec_start, int count,
		rte_intr_callback_fn handler, const char *name,
		void *arg);
int
ifpga_unregister_msix_irq(struct ifpga_rawdev *dev, enum ifpga_irq_type type,
		int vec_start, rte_intr_callback_fn handler, void *arg);

struct rte_pci_bus *ifpga_get_pci_bus(void);
int ifpga_rawdev_partial_reconfigure(struct rte_rawdev *dev, int port,
	const char *file);
void ifpga_rawdev_cleanup(void);

#endif /* _IFPGA_RAWDEV_H_ */
