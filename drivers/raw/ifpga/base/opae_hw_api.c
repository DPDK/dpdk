/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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
	if (!acc)
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
 * @network_ops: ops of network management.
 * @data: private data of this manager.
 *
 * Return: opae_manager on success, otherwise NULL.
 */
struct opae_manager *
opae_manager_alloc(const char *name, struct opae_manager_ops *ops,
		struct opae_manager_networking_ops *network_ops, void *data)
{
	struct opae_manager *mgr = opae_zmalloc(sizeof(*mgr));

	if (!mgr)
		return NULL;

	mgr->name = name;
	mgr->ops = ops;
	mgr->network_ops = network_ops;
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
int opae_manager_flash(struct opae_manager *mgr, int id, const char *buf,
		u32 size, u64 *status)
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

static void opae_mutex_init(pthread_mutex_t *mutex)
{
	pthread_mutexattr_t mattr;

	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
	pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
	pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(mutex, &mattr);
	pthread_mutexattr_destroy(&mattr);
}

static int opae_shm_open(char *shm_name, u32 size, int *new_shm)
{
	int shm_id;
	int ret;

	shm_id = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (shm_id == -1) {
		if (errno == EEXIST) {
			dev_info(NULL, "shared memory %s already exist\n",
					shm_name);
			shm_id = shm_open(shm_name, O_RDWR, 0666);
		} else {
			dev_err(NULL, "failed to create shared memory %s\n",
					shm_name);
			return -1;
		}
	} else {
		*new_shm = 1;
		ret = ftruncate(shm_id, size);
		if (ret == -1) {
			dev_err(NULL,
					"failed to set shared memory size to %u\n",
					size);
			ret = shm_unlink(shm_name);
			if (ret == -1) {
				dev_err(NULL,
						"failed to unlink shared memory %s\n",
						shm_name);
			}
			return -1;
		}
	}

	return shm_id;
}

static pthread_mutex_t *opae_adapter_mutex_open(struct opae_adapter *adapter)
{
	char shm_name[32];
	void *ptr;
	int shm_id;
	int new_shm = 0;

	if (!adapter->data)
		return NULL;
	adapter->lock = NULL;

	snprintf(shm_name, sizeof(shm_name), "/mutex.IFPGA:%s", adapter->name);
	shm_id = opae_shm_open(shm_name, sizeof(pthread_mutex_t), &new_shm);
	if (shm_id == -1) {
		dev_err(NULL, "failed to open shared memory %s\n", shm_name);
	} else {
		dev_info(NULL, "shared memory %s id is %d\n",
				shm_name, shm_id);
		ptr = mmap(NULL, sizeof(pthread_mutex_t),
				PROT_READ | PROT_WRITE, MAP_SHARED,
				shm_id, 0);
		adapter->lock = (pthread_mutex_t *)ptr;
		if (ptr != MAP_FAILED) {
			dev_info(NULL,
					"shared memory %s address is %p\n",
					shm_name, ptr);
			if (new_shm)
				opae_mutex_init(adapter->lock);
		} else {
			dev_err(NULL, "failed to map shared memory %s\n",
					shm_name);
		}
	}

	return adapter->lock;
}

static void opae_adapter_mutex_close(struct opae_adapter *adapter)
{
	char shm_name[32];
	int ret;

	if (!adapter->lock)
		return;

	snprintf(shm_name, sizeof(shm_name), "/mutex.IFPGA:%s", adapter->name);

	ret = munmap(adapter->lock, sizeof(pthread_mutex_t));
	if (ret == -1)
		dev_err(NULL, "failed to unmap shared memory %s\n", shm_name);
	else
		adapter->lock = NULL;
}

/**
 * opae_adapter_lock - lock this adapter
 * @adapter: adapter to lock.
 * @timeout: maximum time to wait for lock done
 *           -1  wait until the lock is available
 *           0   do not wait and return immediately
 *           t   positive time in second to wait
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_adapter_lock(struct opae_adapter *adapter, int timeout)
{
	struct timespec t;
	int ret = -EINVAL;

	if (adapter && adapter->lock) {
		if (timeout < 0) {
			ret = pthread_mutex_lock(adapter->lock);
		} else if (timeout == 0) {
			ret = pthread_mutex_trylock(adapter->lock);
		} else {
			clock_gettime(CLOCK_REALTIME, &t);
			t.tv_sec += timeout;
			ret = pthread_mutex_timedlock(adapter->lock, &t);
		}
	}
	return ret;
}

/**
 * opae_adapter_unlock - unlock this adapter
 * @adapter: adapter to unlock.
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_adapter_unlock(struct opae_adapter *adapter)
{
	int ret = -EINVAL;

	if (adapter && adapter->lock)
		ret = pthread_mutex_unlock(adapter->lock);

	return ret;
}

static void opae_adapter_shm_init(struct opae_adapter *adapter)
{
	opae_share_data *sd;

	if (!adapter->shm.ptr)
		return;

	sd = (opae_share_data *)adapter->shm.ptr;
	dev_info(NULL, "initialize shared memory\n");
	opae_mutex_init(&sd->spi_mutex);
	opae_mutex_init(&sd->i2c_mutex);
	sd->ref_cnt = 0;
	sd->dtb_size = SHM_BLK_SIZE;
	sd->rsu_ctrl = 0;
	sd->rsu_stat = 0;
}

static void *opae_adapter_shm_alloc(struct opae_adapter *adapter)
{
	char shm_name[32];
	opae_share_data *sd;
	u32 size = sizeof(opae_share_data);
	int shm_id;
	int new_shm = 0;

	if (!adapter->data)
		return NULL;

	snprintf(shm_name, sizeof(shm_name), "/IFPGA:%s", adapter->name);
	adapter->shm.ptr = NULL;

	opae_adapter_lock(adapter, -1);
	shm_id = opae_shm_open(shm_name, size, &new_shm);
	if (shm_id == -1) {
		dev_err(NULL, "failed to open shared memory %s\n", shm_name);
	} else {
		dev_info(NULL, "shared memory %s id is %d\n",
				shm_name, shm_id);
		adapter->shm.id = shm_id;
		adapter->shm.size = size;
		adapter->shm.ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
							MAP_SHARED, shm_id, 0);
		if (adapter->shm.ptr != MAP_FAILED) {
			dev_info(NULL,
					"shared memory %s address is %p\n",
					shm_name, adapter->shm.ptr);
			if (new_shm)
				opae_adapter_shm_init(adapter);
			sd = (opae_share_data *)adapter->shm.ptr;
			sd->ref_cnt++;
		} else {
			dev_err(NULL, "failed to map shared memory %s\n",
					shm_name);
		}
	}
	opae_adapter_unlock(adapter);

	return adapter->shm.ptr;
}

static void opae_adapter_shm_free(struct opae_adapter *adapter)
{
	char shm_name[32];
	opae_share_data *sd;
	u32 ref_cnt;
	int ret;

	if (!adapter->shm.ptr)
		return;

	sd = (opae_share_data *)adapter->shm.ptr;
	snprintf(shm_name, sizeof(shm_name), "/IFPGA:%s", adapter->name);

	opae_adapter_lock(adapter, -1);
	ref_cnt = --sd->ref_cnt;
	ret = munmap(adapter->shm.ptr, adapter->shm.size);
	if (ret == -1)
		dev_err(NULL, "failed to unmap shared memory %s\n", shm_name);
	else
		adapter->shm.ptr = NULL;

	if (ref_cnt == 0) {
		dev_info(NULL, "unlink shared memory %s\n", shm_name);
		ret = shm_unlink(shm_name);
		if (ret == -1) {
			dev_err(NULL, "failed to unlink shared memory %s\n",
					shm_name);
		}
	}
	opae_adapter_unlock(adapter);
}

/**
 * opae_adapter_init - init opae_adapter data structure
 * @adapter: pointer of opae_adapter data structure
 * @name: adapter name.
 * @data: private data of this adapter.
 *
 * Return: 0 on success.
 */
int opae_adapter_init(struct opae_adapter *adapter,
		const char *name, void *data)
{
	if (!adapter)
		return -ENOMEM;

	TAILQ_INIT(&adapter->acc_list);
	adapter->data = data;
	adapter->name = name;
	adapter->ops = match_ops(adapter);

	if (!opae_adapter_mutex_open(adapter))
		return -ENOMEM;

	if (!opae_adapter_shm_alloc(adapter))
		return -ENOMEM;

	return 0;
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
		opae_adapter_dump(adapter, 0);

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
	if (adapter) {
		if (adapter->ops && adapter->ops->destroy)
			adapter->ops->destroy(adapter);
		opae_adapter_shm_free(adapter);
		opae_adapter_mutex_close(adapter);
	}
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

/**
 * opae_manager_read_mac_rom - read the content of the MAC ROM
 * @mgr: opae_manager for MAC ROM
 * @port: the port number of retimer
 * @addr: buffer of the MAC address
 *
 * Return: return the bytes of read successfully
 */
int opae_manager_read_mac_rom(struct opae_manager *mgr, int port,
		struct opae_ether_addr *addr)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->read_mac_rom)
		return mgr->network_ops->read_mac_rom(mgr,
				port * sizeof(struct opae_ether_addr),
				addr, sizeof(struct opae_ether_addr));

	return -ENOENT;
}

/**
 * opae_manager_write_mac_rom - write data into MAC ROM
 * @mgr: opae_manager for MAC ROM
 * @port: the port number of the retimer
 * @addr: data of the MAC address
 *
 * Return: return written bytes
 */
int opae_manager_write_mac_rom(struct opae_manager *mgr, int port,
		struct opae_ether_addr *addr)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops && mgr->network_ops->write_mac_rom)
		return mgr->network_ops->write_mac_rom(mgr,
				port * sizeof(struct opae_ether_addr),
				addr, sizeof(struct opae_ether_addr));

	return -ENOENT;
}

/**
 * opae_manager_get_eth_group_nums - get eth group numbers
 * @mgr: opae_manager for eth group
 *
 * Return: the numbers of eth group
 */
int opae_manager_get_eth_group_nums(struct opae_manager *mgr)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->get_retimer_info)
		return mgr->network_ops->get_eth_group_nums(mgr);

	return -ENOENT;
}

/**
 * opae_manager_get_eth_group_info - get eth group info
 * @mgr: opae_manager for eth group
 * @group_id: id for eth group
 * @info: info return to caller
 *
 * Return: 0 on success, otherwise error code
 */
int opae_manager_get_eth_group_info(struct opae_manager *mgr,
	       u8 group_id, struct opae_eth_group_info *info)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->get_retimer_info)
		return mgr->network_ops->get_eth_group_info(mgr,
			group_id, info);

	return -ENOENT;
}

/**
 * opae_manager_get_eth_group_region_info
 * @mgr: opae_manager for flash.
 * @info: the memory region info for eth group
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_manager_get_eth_group_region_info(struct opae_manager *mgr,
		u8 group_id, struct opae_eth_group_region_info *info)
{
	if (!mgr)
		return -EINVAL;

	if (group_id >= MAX_ETH_GROUP_DEVICES)
		return -EINVAL;

	info->group_id = group_id;

	if (mgr && mgr->ops && mgr->ops->get_eth_group_region_info)
		return mgr->ops->get_eth_group_region_info(mgr, info);

	return -ENOENT;
}

/**
 * opae_manager_eth_group_read_reg - read ETH group register
 * @mgr: opae_manager for ETH Group
 * @group_id: ETH group id
 * @type: eth type
 * @index: port index in eth group device
 * @addr: register address of ETH Group
 * @data: read buffer
 *
 * Return: 0 on success, otherwise error code
 */
int opae_manager_eth_group_read_reg(struct opae_manager *mgr, u8 group_id,
		u8 type, u8 index, u16 addr, u32 *data)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->eth_group_reg_read)
		return mgr->network_ops->eth_group_reg_read(mgr, group_id,
				type, index, addr, data);

	return -ENOENT;
}

/**
 * opae_manager_eth_group_write_reg - write ETH group register
 * @mgr: opae_manager for ETH Group
 * @group_id: ETH group id
 * @type: eth type
 * @index: port index in eth group device
 * @addr: register address of ETH Group
 * @data: data will write to register
 *
 * Return: 0 on success, otherwise error code
 */
int opae_manager_eth_group_write_reg(struct opae_manager *mgr, u8 group_id,
		u8 type, u8 index, u16 addr, u32 data)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->eth_group_reg_write)
		return mgr->network_ops->eth_group_reg_write(mgr, group_id,
				type, index, addr, data);

	return -ENOENT;
}

/**
 * opae_manager_get_retimer_info - get retimer info like PKVL chip
 * @mgr: opae_manager for retimer
 * @info: info return to caller
 *
 * Return: 0 on success, otherwise error code
 */
int opae_manager_get_retimer_info(struct opae_manager *mgr,
	       struct opae_retimer_info *info)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->get_retimer_info)
		return mgr->network_ops->get_retimer_info(mgr, info);

	return -ENOENT;
}

/**
 * opae_manager_get_retimer_status - get retimer status
 * @mgr: opae_manager of retimer
 * @status: status of retimer
 *
 * Return: 0 on success, otherwise error code
 */
int opae_manager_get_retimer_status(struct opae_manager *mgr,
		struct opae_retimer_status *status)
{
	if (!mgr || !mgr->network_ops)
		return -EINVAL;

	if (mgr->network_ops->get_retimer_status)
		return mgr->network_ops->get_retimer_status(mgr,
				status);

	return -ENOENT;
}

/**
 * opae_manager_get_sensor_list - get sensor name list
 * @mgr: opae_manager of sensors
 * @buf: buffer to accommodate name list separated by semicolon
 * @size: size of buffer
 *
 * Return: the pointer of the opae_sensor_info
 */
int
opae_mgr_get_sensor_list(struct opae_manager *mgr, char *buf, size_t size)
{
	struct opae_sensor_info *sensor;
	uint32_t offset = 0;

	opae_mgr_for_each_sensor(mgr, sensor) {
		if (sensor->name) {
			if (buf && (offset < size))
				snprintf(buf + offset, size - offset, "%s;",
					sensor->name);
			offset += strlen(sensor->name) + 1;
		}
	}

	if (buf && (offset > 0) && (offset <= size))
		buf[offset-1] = 0;

	return offset;
}

/**
 * opae_manager_get_sensor_by_id - get sensor device
 * @id: the id of the sensor
 *
 * Return: the pointer of the opae_sensor_info
 */
struct opae_sensor_info *
opae_mgr_get_sensor_by_id(struct opae_manager *mgr,
		unsigned int id)
{
	struct opae_sensor_info *sensor;

	opae_mgr_for_each_sensor(mgr, sensor)
		if (sensor->id == id)
			return sensor;

	return NULL;
}

/**
 * opae_manager_get_sensor_by_name - get sensor device
 * @name: the name of the sensor
 *
 * Return: the pointer of the opae_sensor_info
 */
struct opae_sensor_info *
opae_mgr_get_sensor_by_name(struct opae_manager *mgr,
		const char *name)
{
	struct opae_sensor_info *sensor;

	opae_mgr_for_each_sensor(mgr, sensor)
		if (!strcmp(sensor->name, name))
			return sensor;

	return NULL;
}

/**
 * opae_manager_get_sensor_value_by_name - find the sensor by name and read out
 * the value
 * @mgr: opae_manager for sensor.
 * @name: the name of the sensor
 * @value: the readout sensor value
 *
 * Return: 0 on success, otherwise error code
 */
int
opae_mgr_get_sensor_value_by_name(struct opae_manager *mgr,
		const char *name, unsigned int *value)
{
	struct opae_sensor_info *sensor;

	if (!mgr)
		return -EINVAL;

	sensor = opae_mgr_get_sensor_by_name(mgr, name);
	if (!sensor)
		return -ENODEV;

	if (mgr->ops && mgr->ops->get_sensor_value)
		return mgr->ops->get_sensor_value(mgr, sensor, value);

	return -ENOENT;
}

/**
 * opae_manager_get_sensor_value_by_id - find the sensor by id and readout the
 * value
 * @mgr: opae_manager for sensor
 * @id: the id of the sensor
 * @value: the readout sensor value
 *
 * Return: 0 on success, otherwise error code
 */
int
opae_mgr_get_sensor_value_by_id(struct opae_manager *mgr,
		unsigned int id, unsigned int *value)
{
	struct opae_sensor_info *sensor;

	if (!mgr)
		return -EINVAL;

	sensor = opae_mgr_get_sensor_by_id(mgr, id);
	if (!sensor)
		return -ENODEV;

	if (mgr->ops && mgr->ops->get_sensor_value)
		return mgr->ops->get_sensor_value(mgr, sensor, value);

	return -ENOENT;
}

/**
 * opae_manager_get_sensor_value - get the current
 * sensor value
 * @mgr: opae_manager for sensor
 * @sensor: opae_sensor_info for sensor
 * @value: the readout sensor value
 *
 * Return: 0 on success, otherwise error code
 */
int
opae_mgr_get_sensor_value(struct opae_manager *mgr,
		struct opae_sensor_info *sensor,
		unsigned int *value)
{
	if (!mgr || !sensor)
		return -EINVAL;

	if (mgr->ops && mgr->ops->get_sensor_value)
		return mgr->ops->get_sensor_value(mgr, sensor, value);

	return -ENOENT;
}

/**
 * opae_manager_get_board_info - get board info
 * sensor value
 * @info: opae_board_info for the card
 *
 * Return: 0 on success, otherwise error code
 */
int
opae_mgr_get_board_info(struct opae_manager *mgr,
		struct opae_board_info **info)
{
	if (!mgr || !info)
		return -EINVAL;

	if (mgr->ops && mgr->ops->get_board_info)
		return mgr->ops->get_board_info(mgr, info);

	return -ENOENT;
}

/**
 * opae_mgr_get_uuid -  get manager's UUID.
 * @mgr: targeted manager
 * @uuid: a pointer to UUID
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_mgr_get_uuid(struct opae_manager *mgr, struct uuid *uuid)
{
	if (!mgr || !uuid)
		return -EINVAL;

	if (mgr->ops && mgr->ops->get_uuid)
		return mgr->ops->get_uuid(mgr, uuid);

	return -ENOENT;
}

/**
 * opae_mgr_update_flash -  update image in flash.
 * @mgr: targeted manager
 * @image: name of image file
 * @status: status of update
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_mgr_update_flash(struct opae_manager *mgr, const char *image,
	uint64_t *status)
{
	if (!mgr)
		return -EINVAL;

	if (mgr->ops && mgr->ops->update_flash)
		return mgr->ops->update_flash(mgr, image, status);

	return -ENOENT;
}

/**
 * opae_stop_flash_update -  stop flash update.
 * @mgr: targeted manager
 * @force: make sure the update process is stopped
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_mgr_stop_flash_update(struct opae_manager *mgr, int force)
{
	if (!mgr)
		return -EINVAL;

	if (mgr->ops && mgr->ops->stop_flash_update)
		return mgr->ops->stop_flash_update(mgr, force);

	return -ENOENT;
}

/**
 * opae_mgr_reload -  reload FPGA.
 * @mgr: targeted manager
 * @type: FPGA type
 * @page: reload from which page
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_mgr_reload(struct opae_manager *mgr, int type, int page)
{
	if (!mgr)
		return -EINVAL;

	if (mgr->ops && mgr->ops->reload)
		return mgr->ops->reload(mgr, type, page);

	return -ENOENT;
}
/**
 * opae_mgr_read_flash -  read flash content
 * @mgr: targeted manager
 * @address: the start address of flash
 * @size: the size of flash
 * @buf: the read buffer
 *
 * Return: 0 on success, otherwise error code.
 */
int opae_mgr_read_flash(struct opae_manager *mgr, u32 address,
		u32 size, void *buf)
{
	if (!mgr)
		return -EINVAL;

	if (mgr->ops && mgr->ops->read_flash)
		return mgr->ops->read_flash(mgr, address, size, buf);

	return -ENOENT;
}
