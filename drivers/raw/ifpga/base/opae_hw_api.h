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
#include "opae_intel_max10.h"
#include "opae_eth_group.h"
#include "ifpga_defines.h"

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
struct opae_manager_networking_ops;

/*
 * opae_manager has pointer to its parent adapter, as it could be able to manage
 * all components on this FPGA device (adapter). If not the case, don't set this
 * adapter, which limit opae_manager ops to manager itself.
 */
struct opae_manager {
	const char *name;
	struct opae_adapter *adapter;
	struct opae_manager_ops *ops;
	struct opae_manager_networking_ops *network_ops;
	struct opae_sensor_list *sensor_list;
	void *data;
};

/* FIXME: add more management ops, e.g power/thermal and etc */
struct opae_manager_ops {
	int (*flash)(struct opae_manager *mgr, int id, const char *buffer,
		     u32 size, u64 *status);
	int (*get_eth_group_region_info)(struct opae_manager *mgr,
			struct opae_eth_group_region_info *info);
	int (*get_sensor_value)(struct opae_manager *mgr,
			struct opae_sensor_info *sensor,
			unsigned int *value);
	int (*get_board_info)(struct opae_manager *mgr,
			struct opae_board_info **info);
	int (*get_uuid)(struct opae_manager *mgr, struct uuid *uuid);
	int (*update_flash)(struct opae_manager *mgr, const char *image,
			u64 *status);
	int (*stop_flash_update)(struct opae_manager *mgr, int force);
	int (*reload)(struct opae_manager *mgr, int type, int page);
	int (*read_flash)(struct opae_manager *mgr, u32 address, u32 size, void *buf);
};

/* networking management ops in FME */
struct opae_manager_networking_ops {
	int (*read_mac_rom)(struct opae_manager *mgr, int offset, void *buf,
			int size);
	int (*write_mac_rom)(struct opae_manager *mgr, int offset, void *buf,
			int size);
	int (*get_eth_group_nums)(struct opae_manager *mgr);
	int (*get_eth_group_info)(struct opae_manager *mgr,
			u8 group_id, struct opae_eth_group_info *info);
	int (*eth_group_reg_read)(struct opae_manager *mgr, u8 group_id,
			u8 type, u8 index, u16 addr, u32 *data);
	int (*eth_group_reg_write)(struct opae_manager *mgr, u8 group_id,
			u8 type, u8 index, u16 addr, u32 data);
	int (*get_retimer_info)(struct opae_manager *mgr,
			struct opae_retimer_info *info);
	int (*get_retimer_status)(struct opae_manager *mgr,
			struct opae_retimer_status *status);
};

#define opae_mgr_for_each_sensor(mgr, sensor) \
	TAILQ_FOREACH(sensor, mgr->sensor_list, node)

/* OPAE Manager APIs */
struct opae_manager *
opae_manager_alloc(const char *name, struct opae_manager_ops *ops,
		struct opae_manager_networking_ops *network_ops, void *data);
#define opae_manager_free(mgr) opae_free(mgr)
int opae_manager_flash(struct opae_manager *mgr, int acc_id, const char *buf,
		       u32 size, u64 *status);
int opae_manager_get_eth_group_region_info(struct opae_manager *mgr,
		u8 group_id, struct opae_eth_group_region_info *info);
int opae_mgr_get_sensor_list(struct opae_manager *mgr, char *buf, size_t size);
struct opae_sensor_info *opae_mgr_get_sensor_by_name(struct opae_manager *mgr,
		const char *name);
struct opae_sensor_info *opae_mgr_get_sensor_by_id(struct opae_manager *mgr,
		unsigned int id);
int opae_mgr_get_sensor_value_by_name(struct opae_manager *mgr,
		const char *name, unsigned int *value);
int opae_mgr_get_sensor_value_by_id(struct opae_manager *mgr,
		unsigned int id, unsigned int *value);
int opae_mgr_get_sensor_value(struct opae_manager *mgr,
		struct opae_sensor_info *sensor,
		unsigned int *value);

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
	struct opae_adapter *adapter;
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
#define AFU_REGION_SIZE  0x8000
	u64 len;
	u8 *addr;
};

struct opae_adapter_data_pci {
	enum opae_adapter_type type;
	u16 device_id;
	u16 vendor_id;
	u16 bus; /*Device bus for PCI */
	u16 devid; /* Device ID */
	u16 function; /* Device function */
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

#define SHM_PREFIX     "/IFPGA:"
#define SHM_BLK_SIZE   0x2000

typedef struct {
	union {
		u8 byte[SHM_BLK_SIZE];
		struct {
			pthread_mutex_t spi_mutex;
			pthread_mutex_t i2c_mutex;
			u32 ref_cnt;    /* reference count of shared memory */
			u32 dtb_size;   /* actual length of DTB data in byte */
			u32 rsu_ctrl;   /* used to control rsu */
			u32 rsu_stat;   /* used to report status for rsu */
		};
	};
	u8 dtb[SHM_BLK_SIZE];   /* DTB data */
} opae_share_data;

typedef struct  {
	int id;       /* shared memory id returned by shm_open */
	u32 size;     /* size of shared memory in byte */
	void *ptr;    /* start address of shared memory */
} opae_share_memory;

struct opae_adapter {
	const char *name;
	struct opae_manager *mgr;
	struct opae_accelerator_list acc_list;
	struct opae_adapter_ops *ops;
	void *data;
	pthread_mutex_t *lock;   /* multi-process mutex for IFPGA */
	opae_share_memory shm;
};

/* OPAE Adapter APIs */
void *opae_adapter_data_alloc(enum opae_adapter_type type);
#define opae_adapter_data_free(data) opae_free(data)

int opae_adapter_init(struct opae_adapter *adapter,
		const char *name, void *data);
#define opae_adapter_free(adapter) opae_free(adapter)
int opae_adapter_lock(struct opae_adapter *adapter, int timeout);
int opae_adapter_unlock(struct opae_adapter *adapter);
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

/* OPAE vBNG network datastruct */
#define OPAE_ETHER_ADDR_LEN 6

struct opae_ether_addr {
	unsigned char addr_bytes[OPAE_ETHER_ADDR_LEN];
} __rte_packed;

/* OPAE vBNG network API*/
int opae_manager_read_mac_rom(struct opae_manager *mgr, int port,
		struct opae_ether_addr *addr);
int opae_manager_write_mac_rom(struct opae_manager *mgr, int port,
		struct opae_ether_addr *addr);
int opae_manager_get_retimer_info(struct opae_manager *mgr,
		struct opae_retimer_info *info);
int opae_manager_get_retimer_status(struct opae_manager *mgr,
		struct opae_retimer_status *status);
int opae_manager_get_eth_group_nums(struct opae_manager *mgr);
int opae_manager_get_eth_group_info(struct opae_manager *mgr,
		u8 group_id, struct opae_eth_group_info *info);
int opae_manager_eth_group_write_reg(struct opae_manager *mgr, u8 group_id,
		u8 type, u8 index, u16 addr, u32 data);
int opae_manager_eth_group_read_reg(struct opae_manager *mgr, u8 group_id,
		u8 type, u8 index, u16 addr, u32 *data);
int opae_mgr_get_board_info(struct opae_manager *mgr,
		struct opae_board_info **info);
int opae_mgr_get_uuid(struct opae_manager *mgr, struct uuid *uuid);
int opae_mgr_update_flash(struct opae_manager *mgr, const char *image,
		uint64_t *status);
int opae_mgr_stop_flash_update(struct opae_manager *mgr, int force);
int opae_mgr_reload(struct opae_manager *mgr, int type, int page);
int opae_mgr_read_flash(struct opae_manager *mgr, u32 address, u32 size, void *buf);
#endif /* _OPAE_HW_API_H_*/
