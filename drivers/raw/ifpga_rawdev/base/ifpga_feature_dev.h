/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_FEATURE_DEV_H_
#define _IFPGA_FEATURE_DEV_H_

#include "ifpga_hw.h"

static inline struct ifpga_port_hw *
get_port(struct ifpga_hw *hw, u32 port_id)
{
	if (!is_valid_port_id(hw, port_id))
		return NULL;

	return &hw->port[port_id];
}

#define ifpga_for_each_fme_feature(hw, feature)		\
	for ((feature) = (hw)->sub_feature;			\
	   (feature) < (hw)->sub_feature + (FME_FEATURE_ID_MAX); (feature)++)

#define ifpga_for_each_port_feature(hw, feature)		\
	for ((feature) = (hw)->sub_feature;			\
	   (feature) < (hw)->sub_feature + (PORT_FEATURE_ID_MAX); (feature)++)

static inline struct feature *
get_fme_feature_by_id(struct ifpga_fme_hw *fme, u64 id)
{
	struct feature *feature;

	ifpga_for_each_fme_feature(fme, feature) {
		if (feature->id == id)
			return feature;
	}

	return NULL;
}

static inline struct feature *
get_port_feature_by_id(struct ifpga_port_hw *port, u64 id)
{
	struct feature *feature;

	ifpga_for_each_port_feature(port, feature) {
		if (feature->id == id)
			return feature;
	}

	return NULL;
}

static inline void  *
get_fme_feature_ioaddr_by_index(struct ifpga_fme_hw *fme, int index)
{
	return fme->sub_feature[index].addr;
}

static inline void  *
get_port_feature_ioaddr_by_index(struct ifpga_port_hw *port, int index)
{
	return port->sub_feature[index].addr;
}

static inline bool
is_fme_feature_present(struct ifpga_fme_hw *fme, int index)
{
	return !!get_fme_feature_ioaddr_by_index(fme, index);
}

static inline bool
is_port_feature_present(struct ifpga_port_hw *port, int index)
{
	return !!get_port_feature_ioaddr_by_index(port, index);
}

int fpga_get_afu_uuid(struct ifpga_port_hw *port, struct uuid *uuid);

int __fpga_port_disable(struct ifpga_port_hw *port);
void __fpga_port_enable(struct ifpga_port_hw *port);

static inline int fpga_port_disable(struct ifpga_port_hw *port)
{
	int ret;

	spinlock_lock(&port->lock);
	ret = __fpga_port_disable(port);
	spinlock_unlock(&port->lock);
	return ret;
}

static inline int fpga_port_enable(struct ifpga_port_hw *port)
{
	spinlock_lock(&port->lock);
	__fpga_port_enable(port);
	spinlock_unlock(&port->lock);

	return 0;
}

static inline int __fpga_port_reset(struct ifpga_port_hw *port)
{
	int ret;

	ret = __fpga_port_disable(port);
	if (ret)
		return ret;

	__fpga_port_enable(port);

	return 0;
}

static inline int fpga_port_reset(struct ifpga_port_hw *port)
{
	int ret;

	spinlock_lock(&port->lock);
	ret = __fpga_port_reset(port);
	spinlock_unlock(&port->lock);
	return ret;
}

int do_pr(struct ifpga_hw *hw, u32 port_id, void *buffer, u32 size,
	  u64 *status);

int fme_get_prop(struct ifpga_fme_hw *fme, struct feature_prop *prop);
int fme_set_prop(struct ifpga_fme_hw *fme, struct feature_prop *prop);
int fme_set_irq(struct ifpga_fme_hw *fme, u32 feature_id, void *irq_set);

int fme_hw_init(struct ifpga_fme_hw *fme);
void fme_hw_uinit(struct ifpga_fme_hw *fme);
void port_hw_uinit(struct ifpga_port_hw *port);
int port_hw_init(struct ifpga_port_hw *port);
int port_clear_error(struct ifpga_port_hw *port);
void port_err_mask(struct ifpga_port_hw *port, bool mask);
int port_err_clear(struct ifpga_port_hw *port, u64 err);

extern struct feature_ops fme_hdr_ops;
extern struct feature_ops fme_thermal_mgmt_ops;
extern struct feature_ops fme_power_mgmt_ops;
extern struct feature_ops fme_global_err_ops;
extern struct feature_ops fme_pr_mgmt_ops;
extern struct feature_ops fme_global_iperf_ops;
extern struct feature_ops fme_global_dperf_ops;

int port_get_prop(struct ifpga_port_hw *port, struct feature_prop *prop);
int port_set_prop(struct ifpga_port_hw *port, struct feature_prop *prop);

/* This struct is used when parsing uafu irq_set */
struct fpga_uafu_irq_set {
	u32 start;
	u32 count;
	s32 *evtfds;
};

int port_set_irq(struct ifpga_port_hw *port, u32 feature_id, void *irq_set);

extern struct feature_ops port_hdr_ops;
extern struct feature_ops port_error_ops;
extern struct feature_ops port_stp_ops;
extern struct feature_ops port_uint_ops;

/* help functions for feature ops */
int fpga_msix_set_block(struct feature *feature, unsigned int start,
			unsigned int count, s32 *fds);

#endif /* _IFPGA_FEATURE_DEV_H_ */
