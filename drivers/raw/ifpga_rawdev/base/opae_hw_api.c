/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "opae_hw_api.h"
#include "opae_debug.h"
#include "ifpga_api.h"

/* OPAE Bridge Functions */

/**
 * opae_bridge_alloc - alloc opae_bridge data structure
 * @name: bridge name.
 * @ops: ops of this bridge.
 * @data: private data of this bridge.
 *
 * Return opae_bridge on success, otherwise NULL.
 */
struct opae_bridge *
opae_bridge_alloc(const char *name, struct opae_bridge_ops *ops, void *data)
{
	struct opae_bridge *br = opae_zmalloc(sizeof(*br));

	if (!br)
		return NULL;

	br->name = name;
	br->ops = ops;
	br->data = data;

	opae_log("%s %p\n", __func__, br);

	return br;
}

/**
 * opae_bridge_reset -  reset opae_bridge
 * @br: bridge to be reset.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_bridge_reset(struct opae_bridge *br)
{
	if (!br)
		return -EINVAL;

	if (br->ops && br->ops->reset)
		return br->ops->reset(br);

	opae_log("%s no ops\n", __func__);

	return -ENOENT;
}

/* Accelerator Functions */

/**
 * opae_accelerator_alloc - alloc opae_accelerator data structure
 * @name: accelerator name.
 * @ops: ops of this accelerator.
 * @data: private data of this accelerator.
 *
 * Return: opae_accelerator on success, otherwise NULL.
 */
struct opae_accelerator *
opae_accelerator_alloc(const char *name, struct opae_accelerator_ops *ops,
		       void *data)
{
	struct opae_accelerator *acc = opae_zmalloc(sizeof(*acc));

	if (!acc)
		return NULL;

	acc->name = name;
	acc->ops = ops;
	acc->data = data;

	opae_log("%s %p\n", __func__, acc);

	return acc;
}

/**
 * opae_acc_reg_read - read accelerator's register from its reg region.
 * @acc: accelerator to read.
 * @region_idx: reg region index.
 * @offset: reg offset.
 * @byte: read operation width, e.g 4 byte = 32bit read.
 * @data: data to store the value read from the register.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_acc_reg_read(struct opae_accelerator *acc, unsigned int region_idx,
		      u64 offset, unsigned int byte, void *data)
{
	if (!acc || !data)
		return -EINVAL;

	if (acc->ops && acc->ops->read)
		return acc->ops->read(acc, region_idx, offset, byte, data);

	return -ENOENT;
}

/**
 * opae_acc_reg_write - write to accelerator's register from its reg region.
 * @acc: accelerator to write.
 * @region_idx: reg region index.
 * @offset: reg offset.
 * @byte: write operation width, e.g 4 byte = 32bit write.
 * @data: data stored the value to write to the register.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_acc_reg_write(struct opae_accelerator *acc, unsigned int region_idx,
		       u64 offset, unsigned int byte, void *data)
{
	if (!acc || !data)
		return -EINVAL;

	if (acc->ops && acc->ops->write)
		return acc->ops->write(acc, region_idx, offset, byte, data);

	return -ENOENT;
}

/**
 * opae_acc_get_info - get information of an accelerator.
 * @acc: targeted accelerator
 * @info: accelerator info data structure to be filled.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_acc_get_info(struct opae_accelerator *acc, struct opae_acc_info *info)
{
	if (!acc || !info)
		return -EINVAL;

	if (acc->ops && acc->ops->get_info)
		return acc->ops->get_info(acc, info);

	return -ENOENT;
}

/**
 * opae_acc_get_region_info - get information of an accelerator register region.
 * @acc: targeted accelerator
 * @info: accelerator region info data structure to be filled.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_acc_get_region_info(struct opae_accelerator *acc,
			     struct opae_acc_region_info *info)
{
	if (!acc || !info)
		return -EINVAL;

	if (acc->ops && acc->ops->get_region_info)
		return acc->ops->get_region_info(acc, info);

	return -ENOENT;
}

/**
 * opae_acc_set_irq -  set an accelerator's irq.
 * @acc: targeted accelerator
 * @start: start vector number
 * @count: count of vectors to be set from the start vector
 * @evtfds: event fds to be notified when corresponding irqs happens
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_acc_set_irq(struct opae_accelerator *acc,
		     u32 start, u32 count, s32 evtfds[])
{
	if (!acc || !acc->data)
		return -EINVAL;

	if (start + count <= start)
		return -EINVAL;

	if (acc->ops && acc->ops->set_irq)
		return acc->ops->set_irq(acc, start, count, evtfds);

	return -ENOENT;
}

/**
 * opae_acc_get_uuid -  get accelerator's UUID.
 * @acc: targeted accelerator
 * @uuid: a pointer to UUID
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_acc_get_uuid(struct opae_accelerator *acc,
		      struct uuid *uuid)
{
	if (!acc || !uuid)
		return -EINVAL;

	if (acc->ops && acc->ops->get_uuid)
		return acc->ops->get_uuid(acc, uuid);

	return -ENOENT;
}

/* Manager Functions */

/**
 * opae_manager_alloc - alloc opae_manager data structure
 * @name: manager name.
 * @ops: ops of this manager.
 * @data: private data of this manager.
 *
 * Return: opae_manager on success, otherwise NULL.
 */
struct opae_manager *
opae_manager_alloc(const char *name, struct opae_manager_ops *ops, void *data)
{
	struct opae_manager *mgr = opae_zmalloc(sizeof(*mgr));

	if (!mgr)
		return NULL;

	mgr->name = name;
	mgr->ops = ops;
	mgr->data = data;

	opae_log("%s %p\n", __func__, mgr);

	return mgr;
}

/**
 * opae_manager_flash - flash a reconfiguration image via opae_manager
 * @mgr: opae_manager for flash.
 * @id: id of target region (accelerator).
 * @buf: image data buffer.
 * @size: buffer size.
 * @status: status to store flash result.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_manager_flash(struct opae_manager *mgr, int id, void *buf, u32 size,
		       u64 *status)
{
	if (!mgr)
		return -EINVAL;

	if (mgr && mgr->ops && mgr->ops->flash)
		return mgr->ops->flash(mgr, id, buf, size, status);

	return -ENOENT;
}

/* Adapter Functions */

/**
 * opae_adapter_data_alloc - alloc opae_adapter_data data structure
 * @type: opae_adapter_type.
 *
 * Return: opae_adapter_data on success, otherwise NULL.
 */
void *opae_adapter_data_alloc(enum opae_adapter_type type)
{
	struct opae_adapter_data *data;
	int size;

	switch (type) {
	case OPAE_FPGA_PCI:
		size = sizeof(struct opae_adapter_data_pci);
		break;
	case OPAE_FPGA_NET:
		size = sizeof(struct opae_adapter_data_net);
		break;
	default:
		size = sizeof(struct opae_adapter_data);
		break;
	}

	data = opae_zmalloc(size);
	if (!data)
		return NULL;

	data->type = type;

	return data;
}

static struct opae_adapter_ops *match_ops(struct opae_adapter *adapter)
{
	struct opae_adapter_data *data;

	if (!adapter || !adapter->data)
		return NULL;

	data = adapter->data;

	if (data->type == OPAE_FPGA_PCI)
		return &ifpga_adapter_ops;

	return NULL;
}

/**
 * opae_adapter_data_alloc - alloc opae_adapter_data data structure
 * @name: adapter name.
 * @data: private data of this adapter.
 *
 * Return: opae_adapter on success, otherwise NULL.
 */
struct opae_adapter *opae_adapter_alloc(const char *name, void *data)
{
	struct opae_adapter *adapter = opae_zmalloc(sizeof(*adapter));

	if (!adapter)
		return NULL;

	TAILQ_INIT(&adapter->acc_list);
	adapter->data = data;
	adapter->name = name;
	adapter->ops = match_ops(adapter);

	return adapter;
}

/**
 * opae_adapter_enumerate - enumerate this adapter
 * @adapter: adapter to enumerate.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_adapter_enumerate(struct opae_adapter *adapter)
{
	int ret = -ENOENT;

	if (!adapter)
		return -EINVAL;

	if (adapter->ops && adapter->ops->enumerate)
		ret = adapter->ops->enumerate(adapter);

	if (!ret)
		opae_adapter_dump(adapter, 1);

	return ret;
}

/**
 * opae_adapter_destroy - destroy this adapter
 * @adapter: adapter to destroy.
 *
 * destroy things allocated during adapter enumeration.
 */
void opae_adapter_destroy(struct opae_adapter *adapter)
{
	if (adapter && adapter->ops && adapter->ops->destroy)
		adapter->ops->destroy(adapter);
}

/**
 * opae_adapter_get_acc - find and return accelerator with matched id
 * @adapter: adapter to find the accelerator.
 * @acc_id: id (index) of the accelerator.
 *
 * destroy things allocated during adapter enumeration.
 */
struct opae_accelerator *
opae_adapter_get_acc(struct opae_adapter *adapter, int acc_id)
{
	struct opae_accelerator *acc = NULL;

	if (!adapter)
		return NULL;

	opae_adapter_for_each_acc(adapter, acc)
		if (acc->index == acc_id)
			return acc;

	return NULL;
}
