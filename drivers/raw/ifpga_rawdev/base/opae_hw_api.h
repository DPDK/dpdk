/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _OPAE_HW_API_H_
#define _OPAE_HW_API_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

#include "opae_osdep.h"

#ifndef PCI_MAX_RESOURCE
#define PCI_MAX_RESOURCE 6
#endif

struct opae_adapter;

enum opae_adapter_type {
	OPAE_FPGA_PCI,
	OPAE_FPGA_NET,
};

/* OPAE Manager Data Structure */
struct opae_manager_ops;

/*
 * opae_manager has pointer to its parent adapter, as it could be able to manage
 * all components on this FPGA device (adapter). If not the case, don't set this
 * adapter, which limit opae_manager ops to manager itself.
 */
struct opae_manager {
	const char *name;
	struct opae_adapter *adapter;
	struct opae_manager_ops *ops;
	void *data;
};

/* FIXME: add more management ops, e.g power/thermal and etc */
struct opae_manager_ops {
	int (*flash)(struct opae_manager *mgr, int id, void *buffer,
		     u32 size, u64 *status);
};

/* OPAE Manager APIs */
struct opae_manager *
opae_manager_alloc(const char *name, struct opae_manager_ops *ops, void *data);
#define opae_manager_free(mgr) opae_free(mgr)
int opae_manager_flash(struct opae_manager *mgr, int acc_id, void *buf,
		       u32 size, u64 *status);

/* OPAE Bridge Data Structure */
struct opae_bridge_ops;

/*
 * opae_bridge only has pointer to its downstream accelerator.
 */
struct opae_bridge {
	const char *name;
	int id;
	struct opae_accelerator *acc;
	struct opae_bridge_ops *ops;
	void *data;
};

struct opae_bridge_ops {
	int (*reset)(struct opae_bridge *br);
};

/* OPAE Bridge APIs */
struct opae_bridge *
opae_bridge_alloc(const char *name, struct opae_bridge_ops *ops, void *data);
int opae_bridge_reset(struct opae_bridge *br);
#define opae_bridge_free(br) opae_free(br)

/* OPAE Acceleraotr Data Structure */
struct opae_accelerator_ops;

/*
 * opae_accelerator has pointer to its upstream bridge(port).
 * In some cases, if we allow same user to do PR on its own accelerator, then
 * set the manager pointer during the enumeration. But in other cases, the PR
 * functions only could be done via manager in another module / thread / service
 * / application for better protection.
 */
struct opae_accelerator {
	TAILQ_ENTRY(opae_accelerator) node;
	const char *name;
	int index;
	struct opae_bridge *br;
	struct opae_manager *mgr;
	struct opae_accelerator_ops *ops;
	void *data;
};

struct opae_acc_info {
	unsigned int num_regions;
	unsigned int num_irqs;
};

struct opae_acc_region_info {
	u32 flags;
#define ACC_REGION_READ		(1 << 0)
#define ACC_REGION_WRITE	(1 << 1)
#define ACC_REGION_MMIO		(1 << 2)
	u32 index;
	u64 phys_addr;
	u64 len;
	u8 *addr;
};

struct opae_accelerator_ops {
	int (*read)(struct opae_accelerator *acc, unsigned int region_idx,
		    u64 offset, unsigned int byte, void *data);
	int (*write)(struct opae_accelerator *acc, unsigned int region_idx,
		     u64 offset, unsigned int byte, void *data);
	int (*get_info)(struct opae_accelerator *acc,
			struct opae_acc_info *info);
	int (*get_region_info)(struct opae_accelerator *acc,
			       struct opae_acc_region_info *info);
	int (*set_irq)(struct opae_accelerator *acc,
		       u32 start, u32 count, s32 evtfds[]);
	int (*get_uuid)(struct opae_accelerator *acc,
			struct uuid *uuid);
};

/* OPAE accelerator APIs */
struct opae_accelerator *
opae_accelerator_alloc(const char *name, struct opae_accelerator_ops *ops,
		       void *data);
#define opae_accelerator_free(acc) opae_free(acc)
int opae_acc_get_info(struct opae_accelerator *acc, struct opae_acc_info *info);
int opae_acc_get_region_info(struct opae_accelerator *acc,
			     struct opae_acc_region_info *info);
int opae_acc_set_irq(struct opae_accelerator *acc,
		     u32 start, u32 count, s32 evtfds[]);
int opae_acc_get_uuid(struct opae_accelerator *acc,
		      struct uuid *uuid);

static inline struct opae_bridge *
opae_acc_get_br(struct opae_accelerator *acc)
{
	return acc ? acc->br : NULL;
}

static inline struct opae_manager *
opae_acc_get_mgr(struct opae_accelerator *acc)
{
	return acc ? acc->mgr : NULL;
}

int opae_acc_reg_read(struct opae_accelerator *acc, unsigned int region_idx,
		      u64 offset, unsigned int byte, void *data);
int opae_acc_reg_write(struct opae_accelerator *acc, unsigned int region_idx,
		       u64 offset, unsigned int byte, void *data);

#define opae_acc_reg_read64(acc, region, offset, data) \
	opae_acc_reg_read(acc, region, offset, 8, data)
#define opae_acc_reg_write64(acc, region, offset, data) \
	opae_acc_reg_write(acc, region, offset, 8, data)
#define opae_acc_reg_read32(acc, region, offset, data) \
	opae_acc_reg_read(acc, region, offset, 4, data)
#define opae_acc_reg_write32(acc, region, offset, data) \
	opae_acc_reg_write(acc, region, offset, 4, data)
#define opae_acc_reg_read16(acc, region, offset, data) \
	opae_acc_reg_read(acc, region, offset, 2, data)
#define opae_acc_reg_write16(acc, region, offset, data) \
	opae_acc_reg_write(acc, region, offset, 2, data)
#define opae_acc_reg_read8(acc, region, offset, data) \
	opae_acc_reg_read(acc, region, offset, 1, data)
#define opae_acc_reg_write8(acc, region, offset, data) \
	opae_acc_reg_write(acc, region, offset, 1, data)

/*for data stream read/write*/
int opae_acc_data_read(struct opae_accelerator *acc, unsigned int flags,
		       u64 offset, unsigned int byte, void *data);
int opae_acc_data_write(struct opae_accelerator *acc, unsigned int flags,
			u64 offset, unsigned int byte, void *data);

/* OPAE Adapter Data Structure */
struct opae_adapter_data {
	enum opae_adapter_type type;
};

struct opae_reg_region {
	u64 phys_addr;
	u64 len;
	u8 *addr;
};

struct opae_adapter_data_pci {
	enum opae_adapter_type type;
	u16 device_id;
	u16 vendor_id;
	struct opae_reg_region region[PCI_MAX_RESOURCE];
	int vfio_dev_fd;  /* VFIO device file descriptor */
};

/* FIXME: OPAE_FPGA_NET type */
struct opae_adapter_data_net {
	enum opae_adapter_type type;
};

struct opae_adapter_ops {
	int (*enumerate)(struct opae_adapter *adapter);
	void (*destroy)(struct opae_adapter *adapter);
};

TAILQ_HEAD(opae_accelerator_list, opae_accelerator);

#define opae_adapter_for_each_acc(adatper, acc) \
	TAILQ_FOREACH(acc, &adapter->acc_list, node)

struct opae_adapter {
	const char *name;
	struct opae_manager *mgr;
	struct opae_accelerator_list acc_list;
	struct opae_adapter_ops *ops;
	void *data;
};

/* OPAE Adapter APIs */
void *opae_adapter_data_alloc(enum opae_adapter_type type);
#define opae_adapter_data_free(data) opae_free(data)

struct opae_adapter *opae_adapter_alloc(const char *name, void *data);
#define opae_adapter_free(adapter) opae_free(adapter)

int opae_adapter_enumerate(struct opae_adapter *adapter);
void opae_adapter_destroy(struct opae_adapter *adapter);
static inline struct opae_manager *
opae_adapter_get_mgr(struct opae_adapter *adapter)
{
	return adapter ? adapter->mgr : NULL;
}

struct opae_accelerator *
opae_adapter_get_acc(struct opae_adapter *adapter, int acc_id);

static inline void opae_adapter_add_acc(struct opae_adapter *adapter,
					struct opae_accelerator *acc)
{
	TAILQ_INSERT_TAIL(&adapter->acc_list, acc, node);
}

static inline void opae_adapter_remove_acc(struct opae_adapter *adapter,
					   struct opae_accelerator *acc)
{
	TAILQ_REMOVE(&adapter->acc_list, acc, node);
}
#endif /* _OPAE_HW_API_H_*/
