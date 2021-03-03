/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_HW_H_
#define _IFPGA_HW_H_

#include "ifpga_defines.h"
#include "opae_ifpga_hw_api.h"
#include "opae_eth_group.h"

/** List of private feateues */
TAILQ_HEAD(ifpga_feature_list, ifpga_feature);

enum ifpga_feature_state {
	IFPGA_FEATURE_UNUSED = 0,
	IFPGA_FEATURE_ATTACHED,
};

enum feature_type {
	FEATURE_FME_TYPE = 0,
	FEATURE_PORT_TYPE,
};

struct feature_irq_ctx {
	int eventfd;
	int idx;
};

struct ifpga_feature {
	TAILQ_ENTRY(ifpga_feature)next;
	enum ifpga_feature_state state;
	enum feature_type type;
	const char *name;
	u64 id;
	u8 *addr;
	uint64_t phys_addr;
	u32 size;
	int revision;
	u64 cap;
	int vfio_dev_fd;
	struct feature_irq_ctx *ctx;
	unsigned int ctx_num;

	void *parent;		/* to parent hw data structure */

	struct ifpga_feature_ops *ops;/* callback to this private feature */
	unsigned int vec_start;
	unsigned int vec_cnt;
};

struct ifpga_feature_ops {
	int (*init)(struct ifpga_feature *feature);
	void (*uinit)(struct ifpga_feature *feature);
	int (*get_prop)(struct ifpga_feature *feature,
			struct feature_prop *prop);
	int (*set_prop)(struct ifpga_feature *feature,
			struct feature_prop *prop);
	int (*set_irq)(struct ifpga_feature *feature, void *irq_set);
};

enum ifpga_fme_state {
	IFPGA_FME_UNUSED = 0,
	IFPGA_FME_IMPLEMENTED,
};

struct ifpga_fme_hw {
	enum ifpga_fme_state state;

	struct ifpga_feature_list feature_list;
	spinlock_t lock;	/* protect hardware access */

	void *parent;		/* pointer to ifpga_hw */

	/* provied by HEADER feature */
	u32 port_num;
	struct uuid bitstream_id;
	u64 bitstream_md;
	size_t pr_bandwidth;
	u32 socket_id;
	u32 fabric_version_id;
	u32 cache_size;

	u32 capability;

	void *max10_dev; /* MAX10 device */
	void *i2c_master; /* I2C Master device */
	void *eth_dev[MAX_ETH_GROUP_DEVICES];
	struct opae_reg_region
		eth_group_region[MAX_ETH_GROUP_DEVICES];
	struct opae_board_info board_info;
	int nums_eth_dev;
	unsigned int nums_acc_region;
	void *sec_mgr;
};

enum ifpga_port_state {
	IFPGA_PORT_UNUSED = 0,
	IFPGA_PORT_ATTACHED,
	IFPGA_PORT_DETACHED,
};

struct ifpga_port_hw {
	enum ifpga_port_state state;

	struct ifpga_feature_list feature_list;
	spinlock_t lock;	/* protect access to hw */

	void *parent;		/* pointer to ifpga_hw */

	int port_id;		/* provied by HEADER feature */
	struct uuid afu_id;	/* provied by User AFU feature */

	unsigned int disable_count;

	u32 capability;
	u32 num_umsgs;	/* The number of allocated umsgs */
	u32 num_uafu_irqs;	/* The number of uafu interrupts */
	u8 *stp_addr;
	u32 stp_size;
};

#define AFU_MAX_REGION 1

struct ifpga_afu_info {
	struct opae_reg_region region[AFU_MAX_REGION];
	unsigned int num_regions;
	unsigned int num_irqs;
};

struct ifpga_hw {
	struct opae_adapter *adapter;
	struct opae_adapter_data_pci *pci_data;

	struct ifpga_fme_hw fme;
	struct ifpga_port_hw port[MAX_FPGA_PORT_NUM];
};

static inline bool is_ifpga_hw_pf(struct ifpga_hw *hw)
{
	return hw->fme.state != IFPGA_FME_UNUSED;
}

static inline bool is_valid_port_id(struct ifpga_hw *hw, u32 port_id)
{
	if (port_id >= MAX_FPGA_PORT_NUM ||
	    hw->port[port_id].state != IFPGA_PORT_ATTACHED)
		return false;

	return true;
}
#endif /* _IFPGA_HW_H_ */
