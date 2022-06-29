/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_class.h>
#include <rte_malloc.h>
#include <rte_eal_paging.h>

#include "mlx5_common.h"
#include "mlx5_common_os.h"
#include "mlx5_common_mp.h"
#include "mlx5_common_log.h"
#include "mlx5_common_defs.h"
#include "mlx5_common_private.h"

uint8_t haswell_broadwell_cpu;

/* In case this is an x86_64 intel processor to check if
 * we should use relaxed ordering.
 */
#ifdef RTE_ARCH_X86_64
/**
 * This function returns processor identification and feature information
 * into the registers.
 *
 * @param eax, ebx, ecx, edx
 *		Pointers to the registers that will hold cpu information.
 * @param level
 *		The main category of information returned.
 */
static inline void mlx5_cpu_id(unsigned int level,
				unsigned int *eax, unsigned int *ebx,
				unsigned int *ecx, unsigned int *edx)
{
	__asm__("cpuid\n\t"
		: "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "0" (level));
}
#endif

RTE_LOG_REGISTER_DEFAULT(mlx5_common_logtype, NOTICE)

/* Head of list of drivers. */
static TAILQ_HEAD(mlx5_drivers, mlx5_class_driver) drivers_list =
				TAILQ_HEAD_INITIALIZER(drivers_list);

/* Head of devices. */
static TAILQ_HEAD(mlx5_devices, mlx5_common_device) devices_list =
				TAILQ_HEAD_INITIALIZER(devices_list);
static pthread_mutex_t devices_list_lock;

static const struct {
	const char *name;
	unsigned int drv_class;
} mlx5_classes[] = {
	{ .name = "vdpa", .drv_class = MLX5_CLASS_VDPA },
	{ .name = "eth", .drv_class = MLX5_CLASS_ETH },
	/* Keep class "net" for backward compatibility. */
	{ .name = "net", .drv_class = MLX5_CLASS_ETH },
	{ .name = "regex", .drv_class = MLX5_CLASS_REGEX },
	{ .name = "compress", .drv_class = MLX5_CLASS_COMPRESS },
	{ .name = "crypto", .drv_class = MLX5_CLASS_CRYPTO },
};

static int
class_name_to_value(const char *class_name)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(mlx5_classes); i++) {
		if (strcmp(class_name, mlx5_classes[i].name) == 0)
			return mlx5_classes[i].drv_class;
	}
	return -EINVAL;
}

static struct mlx5_class_driver *
driver_get(uint32_t class)
{
	struct mlx5_class_driver *driver;

	TAILQ_FOREACH(driver, &drivers_list, next) {
		if ((uint32_t)driver->drv_class == class)
			return driver;
	}
	return NULL;
}

/**
 * Verify and store value for devargs.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param opaque
 *   User data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_common_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_common_dev_config *config = opaque;
	signed long tmp;

	if (val == NULL || *val == '\0') {
		DRV_LOG(ERR, "Key %s is missing value.", key);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	errno = 0;
	tmp = strtol(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		DRV_LOG(WARNING, "%s: \"%s\" is an invalid integer.", key, val);
		return -rte_errno;
	}
	if (strcmp(key, "tx_db_nc") == 0) {
		if (tmp != MLX5_TXDB_CACHED &&
		    tmp != MLX5_TXDB_NCACHED &&
		    tmp != MLX5_TXDB_HEURISTIC) {
			DRV_LOG(ERR, "Invalid Tx doorbell mapping parameter.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->dbnc = tmp;
	} else if (strcmp(key, "mr_ext_memseg_en") == 0) {
		config->mr_ext_memseg_en = !!tmp;
	} else if (strcmp(key, "mr_mempool_reg_en") == 0) {
		config->mr_mempool_reg_en = !!tmp;
	} else if (strcmp(key, "sys_mem_en") == 0) {
		config->sys_mem_en = !!tmp;
	}
	return 0;
}

/**
 * Parse common device parameters.
 *
 * @param devargs
 *   Device arguments structure.
 * @param config
 *   Pointer to device configuration structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_common_config_get(struct rte_devargs *devargs,
		       struct mlx5_common_dev_config *config)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	/* Set defaults. */
	config->mr_ext_memseg_en = 1;
	config->mr_mempool_reg_en = 1;
	config->sys_mem_en = 0;
	config->dbnc = MLX5_ARG_UNSET;
	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = rte_kvargs_process(kvlist, NULL, mlx5_common_args_check_handler,
				 config);
	if (ret)
		ret = -rte_errno;
	rte_kvargs_free(kvlist);
	DRV_LOG(DEBUG, "mr_ext_memseg_en is %u.", config->mr_ext_memseg_en);
	DRV_LOG(DEBUG, "mr_mempool_reg_en is %u.", config->mr_mempool_reg_en);
	DRV_LOG(DEBUG, "sys_mem_en is %u.", config->sys_mem_en);
	DRV_LOG(DEBUG, "Tx doorbell mapping parameter is %d.", config->dbnc);
	return ret;
}

static int
devargs_class_handler(__rte_unused const char *key,
		      const char *class_names, void *opaque)
{
	int *ret = opaque;
	int class_val;
	char *scratch;
	char *found;
	char *refstr = NULL;

	*ret = 0;
	scratch = strdup(class_names);
	if (scratch == NULL) {
		*ret = -ENOMEM;
		return *ret;
	}
	found = strtok_r(scratch, ":", &refstr);
	if (found == NULL)
		/* Empty string. */
		goto err;
	do {
		/* Extract each individual class name. Multiple
		 * classes can be supplied as class=net:regex:foo:bar.
		 */
		class_val = class_name_to_value(found);
		/* Check if its a valid class. */
		if (class_val < 0) {
			*ret = -EINVAL;
			goto err;
		}
		*ret |= class_val;
		found = strtok_r(NULL, ":", &refstr);
	} while (found != NULL);
err:
	free(scratch);
	if (*ret < 0)
		DRV_LOG(ERR, "Invalid mlx5 class options: %s.\n", class_names);
	return *ret;
}

static int
parse_class_options(const struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;
	if (devargs->cls != NULL && devargs->cls->name != NULL)
		/* Global syntax, only one class type. */
		return class_name_to_value(devargs->cls->name);
	/* Legacy devargs support multiple classes. */
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return 0;
	rte_kvargs_process(kvlist, RTE_DEVARGS_KEY_CLASS,
			   devargs_class_handler, &ret);
	rte_kvargs_free(kvlist);
	return ret;
}

static const unsigned int mlx5_class_invalid_combinations[] = {
	MLX5_CLASS_ETH | MLX5_CLASS_VDPA,
	/* New class combination should be added here. */
};

static int
is_valid_class_combination(uint32_t user_classes)
{
	unsigned int i;

	/* Verify if user specified unsupported combination. */
	for (i = 0; i < RTE_DIM(mlx5_class_invalid_combinations); i++) {
		if ((mlx5_class_invalid_combinations[i] & user_classes) ==
		    mlx5_class_invalid_combinations[i])
			return -EINVAL;
	}
	/* Not found any invalid class combination. */
	return 0;
}

static bool
mlx5_bus_match(const struct mlx5_class_driver *drv,
	       const struct rte_device *dev)
{
	if (mlx5_dev_is_pci(dev))
		return mlx5_dev_pci_match(drv, dev);
	return true;
}

static struct mlx5_common_device *
to_mlx5_device(const struct rte_device *rte_dev)
{
	struct mlx5_common_device *cdev;

	TAILQ_FOREACH(cdev, &devices_list, next) {
		if (rte_dev == cdev->dev)
			return cdev;
	}
	return NULL;
}

int
mlx5_dev_to_pci_str(const struct rte_device *dev, char *addr, size_t size)
{
	struct rte_pci_addr pci_addr = { 0 };
	int ret;

	if (mlx5_dev_is_pci(dev)) {
		/* Input might be <BDF>, format PCI address to <DBDF>. */
		ret = rte_pci_addr_parse(dev->name, &pci_addr);
		if (ret != 0)
			return -ENODEV;
		rte_pci_device_name(&pci_addr, addr, size);
		return 0;
	}
#ifdef RTE_EXEC_ENV_LINUX
	return mlx5_auxiliary_get_pci_str(RTE_DEV_TO_AUXILIARY_CONST(dev),
			addr, size);
#else
	rte_errno = ENODEV;
	return -rte_errno;
#endif
}

/**
 * Register the mempool for the protection domain.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
 * @param mp
 *   Mempool being registered.
 *
 * @return
 *   0 on success, (-1) on failure and rte_errno is set.
 */
static int
mlx5_dev_mempool_register(struct mlx5_common_device *cdev,
			  struct rte_mempool *mp, bool is_extmem)
{
	return mlx5_mr_mempool_register(cdev, mp, is_extmem);
}

/**
 * Unregister the mempool from the protection domain.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
 * @param mp
 *   Mempool being unregistered.
 */
void
mlx5_dev_mempool_unregister(struct mlx5_common_device *cdev,
			    struct rte_mempool *mp)
{
	if (mlx5_mr_mempool_unregister(cdev, mp) < 0)
		DRV_LOG(WARNING, "Failed to unregister mempool %s for PD %p: %s",
			mp->name, cdev->pd, rte_strerror(rte_errno));
}

/**
 * rte_mempool_walk() callback to register mempools for the protection domain.
 *
 * @param mp
 *   The mempool being walked.
 * @param arg
 *   Pointer to the device shared context.
 */
static void
mlx5_dev_mempool_register_cb(struct rte_mempool *mp, void *arg)
{
	struct mlx5_common_device *cdev = arg;
	int ret;

	ret = mlx5_dev_mempool_register(cdev, mp, false);
	if (ret < 0 && rte_errno != EEXIST)
		DRV_LOG(ERR,
			"Failed to register existing mempool %s for PD %p: %s",
			mp->name, cdev->pd, rte_strerror(rte_errno));
}

/**
 * rte_mempool_walk() callback to unregister mempools
 * from the protection domain.
 *
 * @param mp
 *   The mempool being walked.
 * @param arg
 *   Pointer to the device shared context.
 */
static void
mlx5_dev_mempool_unregister_cb(struct rte_mempool *mp, void *arg)
{
	mlx5_dev_mempool_unregister((struct mlx5_common_device *)arg, mp);
}

/**
 * Mempool life cycle callback for mlx5 common devices.
 *
 * @param event
 *   Mempool life cycle event.
 * @param mp
 *   Associated mempool.
 * @param arg
 *   Pointer to a device shared context.
 */
static void
mlx5_dev_mempool_event_cb(enum rte_mempool_event event, struct rte_mempool *mp,
			  void *arg)
{
	struct mlx5_common_device *cdev = arg;

	switch (event) {
	case RTE_MEMPOOL_EVENT_READY:
		if (mlx5_dev_mempool_register(cdev, mp, false) < 0)
			DRV_LOG(ERR,
				"Failed to register new mempool %s for PD %p: %s",
				mp->name, cdev->pd, rte_strerror(rte_errno));
		break;
	case RTE_MEMPOOL_EVENT_DESTROY:
		mlx5_dev_mempool_unregister(cdev, mp);
		break;
	}
}

int
mlx5_dev_mempool_subscribe(struct mlx5_common_device *cdev)
{
	int ret = 0;

	if (!cdev->config.mr_mempool_reg_en)
		return 0;
	rte_rwlock_write_lock(&cdev->mr_scache.mprwlock);
	if (cdev->mr_scache.mp_cb_registered)
		goto exit;
	/* Callback for this device may be already registered. */
	ret = rte_mempool_event_callback_register(mlx5_dev_mempool_event_cb,
						  cdev);
	if (ret != 0 && rte_errno != EEXIST)
		goto exit;
	/* Register mempools only once for this device. */
	if (ret == 0)
		rte_mempool_walk(mlx5_dev_mempool_register_cb, cdev);
	ret = 0;
	cdev->mr_scache.mp_cb_registered = 1;
exit:
	rte_rwlock_write_unlock(&cdev->mr_scache.mprwlock);
	return ret;
}

static void
mlx5_dev_mempool_unsubscribe(struct mlx5_common_device *cdev)
{
	int ret;

	if (!cdev->mr_scache.mp_cb_registered ||
	    !cdev->config.mr_mempool_reg_en)
		return;
	/* Stop watching for mempool events and unregister all mempools. */
	ret = rte_mempool_event_callback_unregister(mlx5_dev_mempool_event_cb,
						    cdev);
	if (ret == 0)
		rte_mempool_walk(mlx5_dev_mempool_unregister_cb, cdev);
}

/**
 * Callback for memory event.
 *
 * @param event_type
 *   Memory event type.
 * @param addr
 *   Address of memory.
 * @param len
 *   Size of memory.
 */
static void
mlx5_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
		     size_t len, void *arg __rte_unused)
{
	struct mlx5_common_device *cdev;

	/* Must be called from the primary process. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	switch (event_type) {
	case RTE_MEM_EVENT_FREE:
		pthread_mutex_lock(&devices_list_lock);
		/* Iterate all the existing mlx5 devices. */
		TAILQ_FOREACH(cdev, &devices_list, next)
			mlx5_free_mr_by_addr(&cdev->mr_scache,
					     mlx5_os_get_ctx_device_name
								    (cdev->ctx),
					     addr, len);
		pthread_mutex_unlock(&devices_list_lock);
		break;
	case RTE_MEM_EVENT_ALLOC:
	default:
		break;
	}
}

/**
 * Uninitialize all HW global of device context.
 *
 * @param cdev
 *   Pointer to mlx5 device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static void
mlx5_dev_hw_global_release(struct mlx5_common_device *cdev)
{
	if (cdev->pd != NULL) {
		claim_zero(mlx5_os_dealloc_pd(cdev->pd));
		cdev->pd = NULL;
	}
	if (cdev->ctx != NULL) {
		claim_zero(mlx5_glue->close_device(cdev->ctx));
		cdev->ctx = NULL;
	}
}

/**
 * Initialize all HW global of device context.
 *
 * @param cdev
 *   Pointer to mlx5 device structure.
 * @param classes
 *   Chosen classes come from user device arguments.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_dev_hw_global_prepare(struct mlx5_common_device *cdev, uint32_t classes)
{
	int ret;

	/* Create context device */
	ret = mlx5_os_open_device(cdev, classes);
	if (ret < 0)
		return ret;
	/* Allocate Protection Domain object and extract its pdn. */
	ret = mlx5_os_pd_create(cdev);
	if (ret)
		goto error;
	/* All actions taken below are relevant only when DevX is supported */
	if (cdev->config.devx == 0)
		return 0;
	/* Query HCA attributes. */
	ret = mlx5_devx_cmd_query_hca_attr(cdev->ctx, &cdev->config.hca_attr);
	if (ret) {
		DRV_LOG(ERR, "Unable to read HCA capabilities.");
		rte_errno = ENOTSUP;
		goto error;
	}
	return 0;
error:
	mlx5_dev_hw_global_release(cdev);
	return ret;
}

static void
mlx5_common_dev_release(struct mlx5_common_device *cdev)
{
	pthread_mutex_lock(&devices_list_lock);
	TAILQ_REMOVE(&devices_list, cdev, next);
	pthread_mutex_unlock(&devices_list_lock);
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (TAILQ_EMPTY(&devices_list))
			rte_mem_event_callback_unregister("MLX5_MEM_EVENT_CB",
							  NULL);
		mlx5_dev_mempool_unsubscribe(cdev);
		mlx5_mr_release_cache(&cdev->mr_scache);
		mlx5_dev_hw_global_release(cdev);
	}
	rte_free(cdev);
}

static struct mlx5_common_device *
mlx5_common_dev_create(struct rte_device *eal_dev, uint32_t classes)
{
	struct mlx5_common_device *cdev;
	int ret;

	cdev = rte_zmalloc("mlx5_common_device", sizeof(*cdev), 0);
	if (!cdev) {
		DRV_LOG(ERR, "Device allocation failure.");
		rte_errno = ENOMEM;
		return NULL;
	}
	cdev->dev = eal_dev;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto exit;
	/* Parse device parameters. */
	ret = mlx5_common_config_get(eal_dev->devargs, &cdev->config);
	if (ret < 0) {
		DRV_LOG(ERR, "Failed to process device arguments: %s",
			strerror(rte_errno));
		rte_free(cdev);
		return NULL;
	}
	mlx5_malloc_mem_select(cdev->config.sys_mem_en);
	/* Initialize all HW global of device context. */
	ret = mlx5_dev_hw_global_prepare(cdev, classes);
	if (ret) {
		DRV_LOG(ERR, "Failed to initialize device context.");
		rte_free(cdev);
		return NULL;
	}
	/* Initialize global MR cache resources and update its functions. */
	ret = mlx5_mr_create_cache(&cdev->mr_scache, eal_dev->numa_node);
	if (ret) {
		DRV_LOG(ERR, "Failed to initialize global MR share cache.");
		mlx5_dev_hw_global_release(cdev);
		rte_free(cdev);
		return NULL;
	}
	/* Register callback function for global shared MR cache management. */
	if (TAILQ_EMPTY(&devices_list))
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_mr_mem_event_cb, NULL);
exit:
	pthread_mutex_lock(&devices_list_lock);
	TAILQ_INSERT_HEAD(&devices_list, cdev, next);
	pthread_mutex_unlock(&devices_list_lock);
	return cdev;
}

static int
drivers_remove(struct mlx5_common_device *cdev, uint32_t enabled_classes)
{
	struct mlx5_class_driver *driver;
	int local_ret = -ENODEV;
	unsigned int i = 0;
	int ret = 0;

	while (enabled_classes) {
		driver = driver_get(RTE_BIT64(i));
		if (driver != NULL) {
			local_ret = driver->remove(cdev);
			if (local_ret == 0)
				cdev->classes_loaded &= ~RTE_BIT64(i);
			else if (ret == 0)
				ret = local_ret;
		}
		enabled_classes &= ~RTE_BIT64(i);
		i++;
	}
	if (local_ret != 0 && ret == 0)
		ret = local_ret;
	return ret;
}

static int
drivers_probe(struct mlx5_common_device *cdev, uint32_t user_classes)
{
	struct mlx5_class_driver *driver;
	uint32_t enabled_classes = 0;
	bool already_loaded;
	int ret = -EINVAL;

	TAILQ_FOREACH(driver, &drivers_list, next) {
		if ((driver->drv_class & user_classes) == 0)
			continue;
		if (!mlx5_bus_match(driver, cdev->dev))
			continue;
		already_loaded = cdev->classes_loaded & driver->drv_class;
		if (already_loaded && driver->probe_again == 0) {
			DRV_LOG(ERR, "Device %s is already probed",
				cdev->dev->name);
			ret = -EEXIST;
			goto probe_err;
		}
		ret = driver->probe(cdev);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to load driver %s",
				driver->name);
			goto probe_err;
		}
		enabled_classes |= driver->drv_class;
	}
	if (!ret) {
		cdev->classes_loaded |= enabled_classes;
		return 0;
	}
probe_err:
	/*
	 * Need to remove only drivers which were not probed before this probe
	 * instance, but have already been probed before this failure.
	 */
	enabled_classes &= ~cdev->classes_loaded;
	drivers_remove(cdev, enabled_classes);
	return ret;
}

int
mlx5_common_dev_probe(struct rte_device *eal_dev)
{
	struct mlx5_common_device *cdev;
	uint32_t classes = 0;
	bool new_device = false;
	int ret;

	DRV_LOG(INFO, "probe device \"%s\".", eal_dev->name);
	ret = parse_class_options(eal_dev->devargs);
	if (ret < 0) {
		DRV_LOG(ERR, "Unsupported mlx5 class type: %s",
			eal_dev->devargs->args);
		return ret;
	}
	classes = ret;
	if (classes == 0)
		/* Default to net class. */
		classes = MLX5_CLASS_ETH;
	cdev = to_mlx5_device(eal_dev);
	if (!cdev) {
		cdev = mlx5_common_dev_create(eal_dev, classes);
		if (!cdev)
			return -ENOMEM;
		new_device = true;
	}
	/*
	 * Validate combination here.
	 * For new device, the classes_loaded field is 0 and it check only
	 * the classes given as user device arguments.
	 */
	ret = is_valid_class_combination(classes | cdev->classes_loaded);
	if (ret != 0) {
		DRV_LOG(ERR, "Unsupported mlx5 classes combination.");
		goto class_err;
	}
	ret = drivers_probe(cdev, classes);
	if (ret)
		goto class_err;
	return 0;
class_err:
	if (new_device)
		mlx5_common_dev_release(cdev);
	return ret;
}

int
mlx5_common_dev_remove(struct rte_device *eal_dev)
{
	struct mlx5_common_device *cdev;
	int ret;

	cdev = to_mlx5_device(eal_dev);
	if (!cdev)
		return -ENODEV;
	/* Matching device found, cleanup and unload drivers. */
	ret = drivers_remove(cdev, cdev->classes_loaded);
	if (ret == 0)
		mlx5_common_dev_release(cdev);
	return ret;
}

/**
 * Callback to DMA map external memory to a device.
 *
 * @param rte_dev
 *   Pointer to the generic device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 *
 * @return
 *   0 on success, negative value on error.
 */
int
mlx5_common_dev_dma_map(struct rte_device *rte_dev, void *addr,
			uint64_t iova __rte_unused, size_t len)
{
	struct mlx5_common_device *dev;
	struct mlx5_mr_btree *bt;
	struct mlx5_mr *mr;

	dev = to_mlx5_device(rte_dev);
	if (!dev) {
		DRV_LOG(WARNING,
			"Unable to find matching mlx5 device to device %s",
			rte_dev->name);
		rte_errno = ENODEV;
		return -1;
	}
	mr = mlx5_create_mr_ext(dev->pd, (uintptr_t)addr, len,
				SOCKET_ID_ANY, dev->mr_scache.reg_mr_cb);
	if (!mr) {
		DRV_LOG(WARNING, "Device %s unable to DMA map", rte_dev->name);
		rte_errno = EINVAL;
		return -1;
	}
try_insert:
	rte_rwlock_write_lock(&dev->mr_scache.rwlock);
	bt = &dev->mr_scache.cache;
	if (bt->len == bt->size) {
		uint32_t size;
		int ret;

		size = bt->size + 1;
		MLX5_ASSERT(size > bt->size);
		/*
		 * Avoid deadlock (numbers show the sequence of events):
		 *    mlx5_mr_create_primary():
		 *        1) take EAL memory lock
		 *        3) take MR lock
		 *    this function:
		 *        2) take MR lock
		 *        4) take EAL memory lock while allocating the new cache
		 * Releasing the MR lock before step 4
		 * allows another thread to execute step 3.
		 */
		rte_rwlock_write_unlock(&dev->mr_scache.rwlock);
		ret = mlx5_mr_expand_cache(&dev->mr_scache, size,
					   rte_dev->numa_node);
		if (ret < 0) {
			mlx5_mr_free(mr, dev->mr_scache.dereg_mr_cb);
			rte_errno = ret;
			return -1;
		}
		goto try_insert;
	}
	LIST_INSERT_HEAD(&dev->mr_scache.mr_list, mr, mr);
	/* Insert to the global cache table. */
	mlx5_mr_insert_cache(&dev->mr_scache, mr);
	rte_rwlock_write_unlock(&dev->mr_scache.rwlock);
	return 0;
}

/**
 * Callback to DMA unmap external memory to a device.
 *
 * @param rte_dev
 *   Pointer to the generic device.
 * @param addr
 *   Starting virtual address of memory to be unmapped.
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 * @param len
 *   Length of memory segment being unmapped.
 *
 * @return
 *   0 on success, negative value on error.
 */
int
mlx5_common_dev_dma_unmap(struct rte_device *rte_dev, void *addr,
			  uint64_t iova __rte_unused, size_t len __rte_unused)
{
	struct mlx5_common_device *dev;
	struct mr_cache_entry entry;
	struct mlx5_mr *mr;

	dev = to_mlx5_device(rte_dev);
	if (!dev) {
		DRV_LOG(WARNING,
			"Unable to find matching mlx5 device to device %s.",
			rte_dev->name);
		rte_errno = ENODEV;
		return -1;
	}
	rte_rwlock_read_lock(&dev->mr_scache.rwlock);
	mr = mlx5_mr_lookup_list(&dev->mr_scache, &entry, (uintptr_t)addr);
	if (!mr) {
		rte_rwlock_read_unlock(&dev->mr_scache.rwlock);
		DRV_LOG(WARNING,
			"Address 0x%" PRIxPTR " wasn't registered to device %s",
			(uintptr_t)addr, rte_dev->name);
		rte_errno = EINVAL;
		return -1;
	}
	LIST_REMOVE(mr, mr);
	DRV_LOG(DEBUG, "MR(%p) is removed from list.", (void *)mr);
	mlx5_mr_free(mr, dev->mr_scache.dereg_mr_cb);
	mlx5_mr_rebuild_cache(&dev->mr_scache);
	/*
	 * No explicit wmb is needed after updating dev_gen due to
	 * store-release ordering in unlock that provides the
	 * implicit barrier at the software visible level.
	 */
	++dev->mr_scache.dev_gen;
	DRV_LOG(DEBUG, "Broadcasting local cache flush, gen=%d.",
		dev->mr_scache.dev_gen);
	rte_rwlock_read_unlock(&dev->mr_scache.rwlock);
	return 0;
}

void
mlx5_class_driver_register(struct mlx5_class_driver *driver)
{
	mlx5_common_driver_on_register_pci(driver);
	TAILQ_INSERT_TAIL(&drivers_list, driver, next);
}

static void mlx5_common_driver_init(void)
{
	mlx5_common_pci_init();
#ifdef RTE_EXEC_ENV_LINUX
	mlx5_common_auxiliary_init();
#endif
}

static bool mlx5_common_initialized;

/**
 * One time initialization routine for run-time dependency on glue library
 * for multiple PMDs. Each mlx5 PMD that depends on mlx5_common module,
 * must invoke in its constructor.
 */
void
mlx5_common_init(void)
{
	if (mlx5_common_initialized)
		return;

	pthread_mutex_init(&devices_list_lock, NULL);
	mlx5_glue_constructor();
	mlx5_common_driver_init();
	mlx5_common_initialized = true;
}

/**
 * This function is responsible of initializing the variable
 *  haswell_broadwell_cpu by checking if the cpu is intel
 *  and reading the data returned from mlx5_cpu_id().
 *  since haswell and broadwell cpus don't have improved performance
 *  when using relaxed ordering we want to check the cpu type before
 *  before deciding whether to enable RO or not.
 *  if the cpu is haswell or broadwell the variable will be set to 1
 *  otherwise it will be 0.
 */
RTE_INIT_PRIO(mlx5_is_haswell_broadwell_cpu, LOG)
{
#ifdef RTE_ARCH_X86_64
	unsigned int broadwell_models[4] = {0x3d, 0x47, 0x4F, 0x56};
	unsigned int haswell_models[4] = {0x3c, 0x3f, 0x45, 0x46};
	unsigned int i, model, family, brand_id, vendor;
	unsigned int signature_intel_ebx = 0x756e6547;
	unsigned int extended_model;
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	int max_level;

	mlx5_cpu_id(0, &eax, &ebx, &ecx, &edx);
	vendor = ebx;
	max_level = eax;
	if (max_level < 1) {
		haswell_broadwell_cpu = 0;
		return;
	}
	mlx5_cpu_id(1, &eax, &ebx, &ecx, &edx);
	model = (eax >> 4) & 0x0f;
	family = (eax >> 8) & 0x0f;
	brand_id = ebx & 0xff;
	extended_model = (eax >> 12) & 0xf0;
	/* Check if the processor is Haswell or Broadwell */
	if (vendor == signature_intel_ebx) {
		if (family == 0x06)
			model += extended_model;
		if (brand_id == 0 && family == 0x6) {
			for (i = 0; i < RTE_DIM(broadwell_models); i++)
				if (model == broadwell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
			for (i = 0; i < RTE_DIM(haswell_models); i++)
				if (model == haswell_models[i]) {
					haswell_broadwell_cpu = 1;
					return;
				}
		}
	}
#endif
	haswell_broadwell_cpu = 0;
}

/**
 * Allocate the User Access Region with DevX on specified device.
 * This routine handles the following UAR allocation issues:
 *
 *  - Try to allocate the UAR with the most appropriate memory mapping
 *    type from the ones supported by the host.
 *
 *  - Try to allocate the UAR with non-NULL base address OFED 5.0.x and
 *    Upstream rdma_core before v29 returned the NULL as UAR base address
 *    if UAR was not the first object in the UAR page.
 *    It caused the PMD failure and we should try to get another UAR till
 *    we get the first one with non-NULL base address returned.
 *
 * @param [in] cdev
 *   Pointer to mlx5 device structure to perform allocation on its context.
 *
 * @return
 *   UAR object pointer on success, NULL otherwise and rte_errno is set.
 */
static void *
mlx5_devx_alloc_uar(struct mlx5_common_device *cdev)
{
	void *uar;
	uint32_t retry, uar_mapping;
	void *base_addr;

	for (retry = 0; retry < MLX5_ALLOC_UAR_RETRY; ++retry) {
#ifdef MLX5DV_UAR_ALLOC_TYPE_NC
		/* Control the mapping type according to the settings. */
		uar_mapping = (cdev->config.dbnc == MLX5_TXDB_NCACHED) ?
			    MLX5DV_UAR_ALLOC_TYPE_NC : MLX5DV_UAR_ALLOC_TYPE_BF;
#else
		/*
		 * It seems we have no way to control the memory mapping type
		 * for the UAR, the default "Write-Combining" type is supposed.
		 */
		uar_mapping = 0;
#endif
		uar = mlx5_glue->devx_alloc_uar(cdev->ctx, uar_mapping);
#ifdef MLX5DV_UAR_ALLOC_TYPE_NC
		if (!uar && uar_mapping == MLX5DV_UAR_ALLOC_TYPE_BF) {
			/*
			 * In some environments like virtual machine the
			 * Write Combining mapped might be not supported and
			 * UAR allocation fails. We tried "Non-Cached" mapping
			 * for the case.
			 */
			DRV_LOG(DEBUG, "Failed to allocate DevX UAR (BF)");
			uar_mapping = MLX5DV_UAR_ALLOC_TYPE_NC;
			uar = mlx5_glue->devx_alloc_uar(cdev->ctx, uar_mapping);
		} else if (!uar && uar_mapping == MLX5DV_UAR_ALLOC_TYPE_NC) {
			/*
			 * If Verbs/kernel does not support "Non-Cached"
			 * try the "Write-Combining".
			 */
			DRV_LOG(DEBUG, "Failed to allocate DevX UAR (NC)");
			uar_mapping = MLX5DV_UAR_ALLOC_TYPE_BF;
			uar = mlx5_glue->devx_alloc_uar(cdev->ctx, uar_mapping);
		}
#endif
		if (!uar) {
			DRV_LOG(ERR, "Failed to allocate DevX UAR (BF/NC)");
			rte_errno = ENOMEM;
			goto exit;
		}
		base_addr = mlx5_os_get_devx_uar_base_addr(uar);
		if (base_addr)
			break;
		/*
		 * The UARs are allocated by rdma_core within the
		 * IB device context, on context closure all UARs
		 * will be freed, should be no memory/object leakage.
		 */
		DRV_LOG(DEBUG, "Retrying to allocate DevX UAR");
		uar = NULL;
	}
	/* Check whether we finally succeeded with valid UAR allocation. */
	if (!uar) {
		DRV_LOG(ERR, "Failed to allocate DevX UAR (NULL base)");
		rte_errno = ENOMEM;
	}
	/*
	 * Return void * instead of struct mlx5dv_devx_uar *
	 * is for compatibility with older rdma-core library headers.
	 */
exit:
	return uar;
}

void
mlx5_devx_uar_release(struct mlx5_uar *uar)
{
	if (uar->obj != NULL)
		mlx5_glue->devx_free_uar(uar->obj);
	memset(uar, 0, sizeof(*uar));
}

int
mlx5_devx_uar_prepare(struct mlx5_common_device *cdev, struct mlx5_uar *uar)
{
	off_t uar_mmap_offset;
	const size_t page_size = rte_mem_page_size();
	void *base_addr;
	void *uar_obj;

	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return -1;
	}
	uar_obj = mlx5_devx_alloc_uar(cdev);
	if (uar_obj == NULL || mlx5_os_get_devx_uar_reg_addr(uar_obj) == NULL) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to allocate UAR.");
		return -1;
	}
	uar->obj = uar_obj;
	uar_mmap_offset = mlx5_os_get_devx_uar_mmap_offset(uar_obj);
	base_addr = mlx5_os_get_devx_uar_base_addr(uar_obj);
	uar->dbnc = mlx5_db_map_type_get(uar_mmap_offset, page_size);
	uar->bf_db.db = mlx5_os_get_devx_uar_reg_addr(uar_obj);
	uar->cq_db.db = RTE_PTR_ADD(base_addr, MLX5_CQ_DOORBELL);
#ifndef RTE_ARCH_64
	rte_spinlock_init(&uar->bf_sl);
	rte_spinlock_init(&uar->cq_sl);
	uar->bf_db.sl_p = &uar->bf_sl;
	uar->cq_db.sl_p = &uar->cq_sl;
#endif /* RTE_ARCH_64 */
	return 0;
}

RTE_PMD_EXPORT_NAME(mlx5_common_driver, __COUNTER__);
