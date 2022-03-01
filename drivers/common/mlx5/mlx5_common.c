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

/* Driver type key for new device global syntax. */
#define MLX5_DRIVER_KEY "driver"

/* Device parameter to get file descriptor for import device. */
#define MLX5_DEVICE_FD "cmd_fd"

/* Device parameter to get PD number for import Protection Domain. */
#define MLX5_PD_HANDLE "pd_handle"

/* Enable extending memsegs when creating a MR. */
#define MLX5_MR_EXT_MEMSEG_EN "mr_ext_memseg_en"

/* Device parameter to configure implicit registration of mempool memory. */
#define MLX5_MR_MEMPOOL_REG_EN "mr_mempool_reg_en"

/* The default memory allocator used in PMD. */
#define MLX5_SYS_MEM_EN "sys_mem_en"

/*
 * Device parameter to force doorbell register mapping
 * to non-cached region eliminating the extra write memory barrier.
 * Deprecated, ignored (Name changed to sq_db_nc).
 */
#define MLX5_TX_DB_NC "tx_db_nc"

/*
 * Device parameter to force doorbell register mapping
 * to non-cached region eliminating the extra write memory barrier.
 */
#define MLX5_SQ_DB_NC "sq_db_nc"

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

int
mlx5_kvargs_process(struct mlx5_kvargs_ctrl *mkvlist, const char *const keys[],
		    arg_handler_t handler, void *opaque_arg)
{
	const struct rte_kvargs_pair *pair;
	uint32_t i, j;

	MLX5_ASSERT(mkvlist && mkvlist->kvlist);
	/* Process parameters. */
	for (i = 0; i < mkvlist->kvlist->count; i++) {
		pair = &mkvlist->kvlist->pairs[i];
		for (j = 0; keys[j] != NULL; ++j) {
			if (strcmp(pair->key, keys[j]) != 0)
				continue;
			if ((*handler)(pair->key, pair->value, opaque_arg) < 0)
				return -1;
			mkvlist->is_used[i] = true;
			break;
		}
	}
	return 0;
}

/**
 * Prepare a mlx5 kvargs control.
 *
 * @param[out] mkvlist
 *   Pointer to mlx5 kvargs control.
 * @param[in] devargs
 *   The input string containing the key/value associations.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_kvargs_prepare(struct mlx5_kvargs_ctrl *mkvlist,
		    const struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	uint32_t i;

	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/*
	 * rte_kvargs_parse enable key without value, in mlx5 PMDs we disable
	 * this syntax.
	 */
	for (i = 0; i < kvlist->count; i++) {
		const struct rte_kvargs_pair *pair = &kvlist->pairs[i];
		if (pair->value == NULL || *(pair->value) == '\0') {
			DRV_LOG(ERR, "Key %s is missing value.", pair->key);
			rte_kvargs_free(kvlist);
			rte_errno = EINVAL;
			return -rte_errno;
		}
	}
	/* Makes sure all devargs used array is false. */
	memset(mkvlist, 0, sizeof(*mkvlist));
	mkvlist->kvlist = kvlist;
	DRV_LOG(DEBUG, "Parse successfully %u devargs.",
		mkvlist->kvlist->count);
	return 0;
}

/**
 * Release a mlx5 kvargs control.
 *
 * @param[out] mkvlist
 *   Pointer to mlx5 kvargs control.
 */
static void
mlx5_kvargs_release(struct mlx5_kvargs_ctrl *mkvlist)
{
	if (mkvlist == NULL)
		return;
	rte_kvargs_free(mkvlist->kvlist);
	memset(mkvlist, 0, sizeof(*mkvlist));
}

/**
 * Validate device arguments list.
 * It report about the first unknown parameter.
 *
 * @param[in] mkvlist
 *   Pointer to mlx5 kvargs control.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_kvargs_validate(struct mlx5_kvargs_ctrl *mkvlist)
{
	uint32_t i;

	/* Secondary process should not handle devargs. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	if (mkvlist == NULL)
		return 0;
	for (i = 0; i < mkvlist->kvlist->count; i++) {
		if (mkvlist->is_used[i] == 0) {
			DRV_LOG(ERR, "Key \"%s\" "
				"is unknown for the provided classes.",
				mkvlist->kvlist->pairs[i].key);
			rte_errno = EINVAL;
			return -rte_errno;
		}
	}
	return 0;
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

	if (strcmp(MLX5_DRIVER_KEY, key) == 0 ||
	    strcmp(RTE_DEVARGS_KEY_CLASS, key) == 0)
		return 0;
	errno = 0;
	tmp = strtol(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		DRV_LOG(WARNING, "%s: \"%s\" is an invalid integer.", key, val);
		return -rte_errno;
	}
	if (strcmp(key, MLX5_TX_DB_NC) == 0)
		DRV_LOG(WARNING,
			"%s: deprecated parameter, converted to queue_db_nc",
			key);
	if (strcmp(key, MLX5_SQ_DB_NC) == 0 ||
	    strcmp(key, MLX5_TX_DB_NC) == 0) {
		if (tmp != MLX5_SQ_DB_CACHED &&
		    tmp != MLX5_SQ_DB_NCACHED &&
		    tmp != MLX5_SQ_DB_HEURISTIC) {
			DRV_LOG(ERR,
				"Invalid Send Queue doorbell mapping parameter.");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		config->dbnc = tmp;
	} else if (strcmp(key, MLX5_MR_EXT_MEMSEG_EN) == 0) {
		config->mr_ext_memseg_en = !!tmp;
	} else if (strcmp(key, MLX5_MR_MEMPOOL_REG_EN) == 0) {
		config->mr_mempool_reg_en = !!tmp;
	} else if (strcmp(key, MLX5_SYS_MEM_EN) == 0) {
		config->sys_mem_en = !!tmp;
	} else if (strcmp(key, MLX5_DEVICE_FD) == 0) {
		config->device_fd = tmp;
	} else if (strcmp(key, MLX5_PD_HANDLE) == 0) {
		config->pd_handle = tmp;
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
mlx5_common_config_get(struct mlx5_kvargs_ctrl *mkvlist,
		       struct mlx5_common_dev_config *config)
{
	const char **params = (const char *[]){
		RTE_DEVARGS_KEY_CLASS,
		MLX5_DRIVER_KEY,
		MLX5_TX_DB_NC,
		MLX5_SQ_DB_NC,
		MLX5_MR_EXT_MEMSEG_EN,
		MLX5_SYS_MEM_EN,
		MLX5_MR_MEMPOOL_REG_EN,
		MLX5_DEVICE_FD,
		MLX5_PD_HANDLE,
		NULL,
	};
	int ret = 0;

	/* Set defaults. */
	config->mr_ext_memseg_en = 1;
	config->mr_mempool_reg_en = 1;
	config->sys_mem_en = 0;
	config->dbnc = MLX5_ARG_UNSET;
	config->device_fd = MLX5_ARG_UNSET;
	config->pd_handle = MLX5_ARG_UNSET;
	if (mkvlist == NULL)
		return 0;
	/* Process common parameters. */
	ret = mlx5_kvargs_process(mkvlist, params,
				  mlx5_common_args_check_handler, config);
	if (ret) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/* Validate user arguments for remote PD and CTX if it is given. */
	ret = mlx5_os_remote_pd_and_ctx_validate(config);
	if (ret)
		return ret;
	DRV_LOG(DEBUG, "mr_ext_memseg_en is %u.", config->mr_ext_memseg_en);
	DRV_LOG(DEBUG, "mr_mempool_reg_en is %u.", config->mr_mempool_reg_en);
	DRV_LOG(DEBUG, "sys_mem_en is %u.", config->sys_mem_en);
	DRV_LOG(DEBUG, "Send Queue doorbell mapping parameter is %d.",
		config->dbnc);
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
parse_class_options(const struct rte_devargs *devargs,
		    struct mlx5_kvargs_ctrl *mkvlist)
{
	int ret = 0;

	if (devargs == NULL)
		return 0;
	if (devargs->cls != NULL && devargs->cls->name != NULL)
		/* Global syntax, only one class type. */
		return class_name_to_value(devargs->cls->name);
	/* Legacy devargs support multiple classes. */
	rte_kvargs_process(mkvlist->kvlist, RTE_DEVARGS_KEY_CLASS,
			   devargs_class_handler, &ret);
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
		claim_zero(mlx5_os_pd_release(cdev));
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
	/*
	 * When CTX is created by Verbs, query HCA attribute is unsupported.
	 * When CTX is imported, we cannot know if it is created by DevX or
	 * Verbs. So, we use query HCA attribute function to check it.
	 */
	if (cdev->config.devx || cdev->config.device_fd != MLX5_ARG_UNSET) {
		/* Query HCA attributes. */
		ret = mlx5_devx_cmd_query_hca_attr(cdev->ctx,
						   &cdev->config.hca_attr);
		if (ret) {
			DRV_LOG(ERR, "Unable to read HCA caps in DevX mode.");
			rte_errno = ENOTSUP;
			goto error;
		}
		cdev->config.devx = 1;
	}
	DRV_LOG(DEBUG, "DevX is %ssupported.", cdev->config.devx ? "" : "NOT ");
	/* Prepare Protection Domain object and extract its pdn. */
	ret = mlx5_os_pd_prepare(cdev);
	if (ret)
		goto error;
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
mlx5_common_dev_create(struct rte_device *eal_dev, uint32_t classes,
		       struct mlx5_kvargs_ctrl *mkvlist)
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
	ret = mlx5_common_config_get(mkvlist, &cdev->config);
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

/**
 * Validate common devargs when probing again.
 *
 * When common device probing again, it cannot change its configurations.
 * If user ask non compatible configurations in devargs, it is error.
 * This function checks the match between:
 *  - Common device configurations requested by probe again devargs.
 *  - Existing common device configurations.
 *
 * @param cdev
 *   Pointer to mlx5 device structure.
 * @param mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_common_probe_again_args_validate(struct mlx5_common_device *cdev,
				      struct mlx5_kvargs_ctrl *mkvlist)
{
	struct mlx5_common_dev_config *config;
	int ret;

	/* Secondary process should not handle devargs. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	/* Probe again doesn't have to generate devargs. */
	if (mkvlist == NULL)
		return 0;
	config = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
			     sizeof(struct mlx5_common_dev_config),
			     RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (config == NULL) {
		rte_errno = -ENOMEM;
		return -rte_errno;
	}
	/*
	 * Creates a temporary common configure structure according to new
	 * devargs attached in probing again.
	 */
	ret = mlx5_common_config_get(mkvlist, config);
	if (ret) {
		DRV_LOG(ERR, "Failed to process device configure: %s",
			strerror(rte_errno));
		mlx5_free(config);
		return ret;
	}
	/*
	 * Checks the match between the temporary structure and the existing
	 * common device structure.
	 */
	if (cdev->config.mr_ext_memseg_en != config->mr_ext_memseg_en) {
		DRV_LOG(ERR, "\"" MLX5_MR_EXT_MEMSEG_EN "\" "
			"configuration mismatch for device %s.",
			cdev->dev->name);
		goto error;
	}
	if (cdev->config.mr_mempool_reg_en != config->mr_mempool_reg_en) {
		DRV_LOG(ERR, "\"" MLX5_MR_MEMPOOL_REG_EN "\" "
			"configuration mismatch for device %s.",
			cdev->dev->name);
		goto error;
	}
	if (cdev->config.device_fd != config->device_fd) {
		DRV_LOG(ERR, "\"" MLX5_DEVICE_FD "\" "
			"configuration mismatch for device %s.",
			cdev->dev->name);
		goto error;
	}
	if (cdev->config.pd_handle != config->pd_handle) {
		DRV_LOG(ERR, "\"" MLX5_PD_HANDLE "\" "
			"configuration mismatch for device %s.",
			cdev->dev->name);
		goto error;
	}
	if (cdev->config.sys_mem_en != config->sys_mem_en) {
		DRV_LOG(ERR, "\"" MLX5_SYS_MEM_EN "\" "
			"configuration mismatch for device %s.",
			cdev->dev->name);
		goto error;
	}
	if (cdev->config.dbnc != config->dbnc) {
		DRV_LOG(ERR, "\"" MLX5_SQ_DB_NC "\" "
			"configuration mismatch for device %s.",
			cdev->dev->name);
		goto error;
	}
	mlx5_free(config);
	return 0;
error:
	mlx5_free(config);
	rte_errno = EINVAL;
	return -rte_errno;
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
drivers_probe(struct mlx5_common_device *cdev, uint32_t user_classes,
	      struct mlx5_kvargs_ctrl *mkvlist)
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
		ret = driver->probe(cdev, mkvlist);
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
	struct mlx5_kvargs_ctrl mkvlist;
	struct mlx5_kvargs_ctrl *mkvlist_p = NULL;
	uint32_t classes = 0;
	bool new_device = false;
	int ret;

	DRV_LOG(INFO, "probe device \"%s\".", eal_dev->name);
	if (eal_dev->devargs != NULL)
		mkvlist_p = &mkvlist;
	ret = mlx5_kvargs_prepare(mkvlist_p, eal_dev->devargs);
	if (ret < 0) {
		DRV_LOG(ERR, "Unsupported device arguments: %s",
			eal_dev->devargs->args);
		return ret;
	}
	ret = parse_class_options(eal_dev->devargs, mkvlist_p);
	if (ret < 0) {
		DRV_LOG(ERR, "Unsupported mlx5 class type: %s",
			eal_dev->devargs->args);
		goto class_err;
	}
	classes = ret;
	if (classes == 0)
		/* Default to net class. */
		classes = MLX5_CLASS_ETH;
	/*
	 * MLX5 common driver supports probing again in two scenarios:
	 * - Add new driver under existing common device (regardless of the
	 *   driver's own support in probing again).
	 * - Transfer the probing again support of the drivers themselves.
	 *
	 * In both scenarios it uses in the existing device. here it looks for
	 * device that match to rte device, if it exists, the request classes
	 * were probed with this device.
	 */
	cdev = to_mlx5_device(eal_dev);
	if (!cdev) {
		/* It isn't probing again, creates a new device. */
		cdev = mlx5_common_dev_create(eal_dev, classes, mkvlist_p);
		if (!cdev) {
			ret = -ENOMEM;
			goto class_err;
		}
		new_device = true;
	} else {
		/* It is probing again, validate common devargs match. */
		ret = mlx5_common_probe_again_args_validate(cdev, mkvlist_p);
		if (ret) {
			DRV_LOG(ERR,
				"Probe again parameters aren't compatible : %s",
				strerror(rte_errno));
			goto class_err;
		}
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
	ret = drivers_probe(cdev, classes, mkvlist_p);
	if (ret)
		goto class_err;
	/*
	 * Validate that all devargs have been used, unused key -> unknown Key.
	 * When probe again validate is failed, the added drivers aren't removed
	 * here but when device is released.
	 */
	ret = mlx5_kvargs_validate(mkvlist_p);
	if (ret)
		goto class_err;
	mlx5_kvargs_release(mkvlist_p);
	return 0;
class_err:
	if (new_device) {
		/*
		 * For new device, classes_loaded is always 0 before
		 * drivers_probe function.
		 */
		if (cdev->classes_loaded)
			drivers_remove(cdev, cdev->classes_loaded);
		mlx5_common_dev_release(cdev);
	}
	mlx5_kvargs_release(mkvlist_p);
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
	rte_rwlock_write_lock(&dev->mr_scache.rwlock);
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
		uar_mapping = (cdev->config.dbnc == MLX5_SQ_DB_NCACHED) ?
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
