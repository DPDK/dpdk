/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <rte_mempool.h>
#include <rte_bus_pci.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include "mlx5_devx_cmds.h"
#include "../mlx5_common_log.h"
#include "mlx5_common.h"
#include "mlx5_common_os.h"
#include "mlx5_malloc.h"

/**
 * Initialization routine for run-time dependency on external lib.
 */
void
mlx5_glue_constructor(void)
{
}

/**
 * Release PD. Releases a given mlx5_pd object
 *
 * @param[in] pd
 *   Pointer to mlx5_pd.
 *
 * @return
 *   Zero if pd is released successfully, negative number otherwise.
 */
int
mlx5_os_dealloc_pd(void *pd)
{
	if (!pd)
		return -EINVAL;
	mlx5_devx_cmd_destroy(((struct mlx5_pd *)pd)->obj);
	mlx5_free(pd);
	return 0;
}

/**
 * Allocate Protection Domain object and extract its pdn using DV API.
 *
 * @param[out] dev
 *   Pointer to the mlx5 device.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_os_pd_create(struct mlx5_common_device *cdev)
{
	struct mlx5_pd *pd;

	pd = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pd), 0, SOCKET_ID_ANY);
	if (!pd)
		return -1;
	struct mlx5_devx_obj *obj = mlx5_devx_cmd_alloc_pd(cdev->ctx);
	if (!obj) {
		mlx5_free(pd);
		return -1;
	}
	pd->obj = obj;
	pd->pdn = obj->id;
	pd->devx_ctx = cdev->ctx;
	cdev->pd = pd;
	cdev->pdn = pd->pdn;
	return 0;
}

/**
 * Detect if a devx_device_bdf object has identical DBDF values to the
 * rte_pci_addr found in bus/pci probing.
 *
 * @param[in] devx_bdf
 *   Pointer to the devx_device_bdf structure.
 * @param[in] addr
 *   Pointer to the rte_pci_addr structure.
 *
 * @return
 *   1 on Device match, 0 on mismatch.
 */
static int
mlx5_match_devx_bdf_to_addr(struct devx_device_bdf *devx_bdf,
			    struct rte_pci_addr *addr)
{
	if (addr->domain != (devx_bdf->bus_id >> 8) ||
	    addr->bus != (devx_bdf->bus_id & 0xff) ||
	    addr->devid != devx_bdf->dev_id ||
	    addr->function != devx_bdf->fnc_id) {
		return 0;
	}
	return 1;
}

/**
 * Detect if a devx_device_bdf object matches the rte_pci_addr
 * found in bus/pci probing
 * Compare both the Native/PF BDF and the raw_bdf representing a VF BDF.
 *
 * @param[in] devx_bdf
 *   Pointer to the devx_device_bdf structure.
 * @param[in] addr
 *   Pointer to the rte_pci_addr structure.
 *
 * @return
 *   1 on Device match, 0 on mismatch, rte_errno code on failure.
 */
static int
mlx5_match_devx_devices_to_addr(struct devx_device_bdf *devx_bdf,
				struct rte_pci_addr *addr)
{
	int err;
	struct devx_device mlx5_dev;

	if (mlx5_match_devx_bdf_to_addr(devx_bdf, addr))
		return 1;
	/*
	 * Didn't match on Native/PF BDF, could still match a VF BDF,
	 * check it next.
	 */
	err = mlx5_glue->query_device(devx_bdf, &mlx5_dev);
	if (err) {
		DRV_LOG(ERR, "query_device failed");
		rte_errno = err;
		return rte_errno;
	}
	if (mlx5_match_devx_bdf_to_addr(&mlx5_dev.raw_bdf, addr))
		return 1;
	return 0;
}

/**
 * Look for DevX device that match to given rte_device.
 *
 * @param dev
 *   Pointer to the generic device.
 * @param devx_list
 *   Pointer to head of DevX devices list.
 * @param n
 *   Number of devices in given DevX devices list.
 *
 * @return
 *   A device match on success, NULL otherwise and rte_errno is set.
 */
static struct devx_device_bdf *
mlx5_os_get_devx_device(struct rte_device *dev,
			struct devx_device_bdf *devx_list, int n)
{
	struct devx_device_bdf *devx_match = NULL;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev);
	struct rte_pci_addr *addr = &pci_dev->addr;

	while (n-- > 0) {
		int ret = mlx5_match_devx_devices_to_addr(devx_list, addr);
		if (!ret) {
			devx_list++;
			continue;
		}
		if (ret != 1) {
			rte_errno = ret;
			return NULL;
		}
		devx_match = devx_list;
		break;
	}
	if (devx_match == NULL) {
		/* No device matches, just complain and bail out. */
		DRV_LOG(WARNING,
			"No DevX device matches PCI device " PCI_PRI_FMT ","
			" is DevX Configured?",
			addr->domain, addr->bus, addr->devid, addr->function);
		rte_errno = ENOENT;
	}
	return devx_match;
}

/**
 * Function API open device under Windows.
 *
 * This function calls the Windows glue APIs to open a device.
 *
 * @param cdev
 *   Pointer to mlx5 device structure.
 * @param classes
 *   Chosen classes come from user device arguments.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_open_device(struct mlx5_common_device *cdev, uint32_t classes)
{
	struct devx_device_bdf *devx_bdf_dev = NULL;
	struct devx_device_bdf *devx_list;
	struct mlx5_context *mlx5_ctx = NULL;
	int n;

	if (classes != MLX5_CLASS_ETH && classes != MLX5_CLASS_CRYPTO) {
		DRV_LOG(ERR,
			"The chosen classes are not supported on Windows.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	errno = 0;
	devx_list = mlx5_glue->get_device_list(&n);
	if (devx_list == NULL) {
		rte_errno = errno ? errno : ENOSYS;
		DRV_LOG(ERR, "Cannot list devices, is DevX enabled?");
		return -rte_errno;
	}
	devx_bdf_dev = mlx5_os_get_devx_device(cdev->dev, devx_list, n);
	if (devx_bdf_dev == NULL)
		goto error;
	/* Try to open DevX device with DV. */
	mlx5_ctx = mlx5_glue->open_device(devx_bdf_dev);
	if (mlx5_ctx == NULL) {
		DRV_LOG(ERR, "Failed to open DevX device.");
		rte_errno = errno;
		goto error;
	}
	if (mlx5_glue->query_device(devx_bdf_dev, &mlx5_ctx->mlx5_dev)) {
		DRV_LOG(ERR, "Failed to query device context fields.");
		rte_errno = errno;
		goto error;
	}
	cdev->config.devx = 1;
	cdev->ctx = mlx5_ctx;
	mlx5_glue->free_device_list(devx_list);
	return 0;
error:
	if (mlx5_ctx != NULL)
		claim_zero(mlx5_glue->close_device(mlx5_ctx));
	mlx5_glue->free_device_list(devx_list);
	return -rte_errno;
}

/**
 * Register umem.
 *
 * @param[in] ctx
 *   Pointer to context.
 * @param[in] addr
 *   Pointer to memory start address.
 * @param[in] size
 *   Size of the memory to register.
 * @param[out] access
 *   UMEM access type
 *
 * @return
 *   umem on successful registration, NULL and errno otherwise
 */
void *
mlx5_os_umem_reg(void *ctx, void *addr, size_t size, uint32_t access)
{
	struct mlx5_devx_umem *umem;

	umem = mlx5_malloc(MLX5_MEM_ZERO,
		(sizeof(*umem)), 0, SOCKET_ID_ANY);
	if (!umem) {
		errno = ENOMEM;
		return NULL;
	}
	umem->umem_hdl = mlx5_glue->devx_umem_reg(ctx, addr, size, access,
		&umem->umem_id);
	if (!umem->umem_hdl) {
		mlx5_free(umem);
		return NULL;
	}
	umem->addr = addr;
	return umem;
}

/**
 * Deregister umem.
 *
 * @param[in] pumem
 *   Pointer to umem.
 *
 * @return
 *   0 on successful release, negative number otherwise
 */
int
mlx5_os_umem_dereg(void *pumem)
{
	struct mlx5_devx_umem *umem;
	int err = 0;

	if (!pumem)
		return err;
	umem = pumem;
	if (umem->umem_hdl)
		err = mlx5_glue->devx_umem_dereg(umem->umem_hdl);
	mlx5_free(umem);
	return err;
}

/**
 * Register mr. Given protection domain pointer, pointer to addr and length
 * register the memory region.
 *
 * @param[in] pd
 *   Pointer to protection domain context (type mlx5_pd).
 * @param[in] addr
 *   Pointer to memory start address (type devx_device_ctx).
 * @param[in] length
 *   Length of the memory to register.
 * @param[out] pmd_mr
 *   pmd_mr struct set with lkey, address, length, pointer to mr object, mkey
 *
 * @return
 *   0 on successful registration, -1 otherwise
 */
static int
mlx5_os_reg_mr(void *pd,
	       void *addr, size_t length, struct mlx5_pmd_mr *pmd_mr)
{
	struct mlx5_devx_mkey_attr mkey_attr;
	struct mlx5_pd *mlx5_pd = (struct mlx5_pd *)pd;
	struct mlx5_hca_attr attr;
	struct mlx5_devx_obj *mkey;
	void *obj;

	if (!pd || !addr) {
		rte_errno = EINVAL;
		return -1;
	}
	if (mlx5_devx_cmd_query_hca_attr(mlx5_pd->devx_ctx, &attr))
		return -1;
	obj = mlx5_os_umem_reg(mlx5_pd->devx_ctx, addr, length,
			       IBV_ACCESS_LOCAL_WRITE);
	if (!obj)
		return -1;
	memset(&mkey_attr, 0, sizeof(mkey_attr));
	mkey_attr.addr = (uintptr_t)addr;
	mkey_attr.size = length;
	mkey_attr.umem_id = ((struct mlx5_devx_umem *)(obj))->umem_id;
	mkey_attr.pd = mlx5_pd->pdn;
	if (!haswell_broadwell_cpu) {
		mkey_attr.relaxed_ordering_write = attr.relaxed_ordering_write;
		mkey_attr.relaxed_ordering_read = attr.relaxed_ordering_read;
	}
	mkey = mlx5_devx_cmd_mkey_create(mlx5_pd->devx_ctx, &mkey_attr);
	if (!mkey) {
		claim_zero(mlx5_os_umem_dereg(obj));
		return -1;
	}
	pmd_mr->addr = addr;
	pmd_mr->len = length;
	pmd_mr->obj = obj;
	pmd_mr->mkey = mkey;
	pmd_mr->lkey = pmd_mr->mkey->id;
	return 0;
}

/**
 * De-register mr.
 *
 * @param[in] pmd_mr
 *  Pointer to PMD mr object
 */
static void
mlx5_os_dereg_mr(struct mlx5_pmd_mr *pmd_mr)
{
	if (!pmd_mr)
		return;
	if (pmd_mr->mkey)
		claim_zero(mlx5_devx_cmd_destroy(pmd_mr->mkey));
	if (pmd_mr->obj)
		claim_zero(mlx5_os_umem_dereg(pmd_mr->obj));
	memset(pmd_mr, 0, sizeof(*pmd_mr));
}

/**
 * Set the reg_mr and dereg_mr callbacks.
 *
 * @param[out] reg_mr_cb
 *   Pointer to reg_mr func
 * @param[out] dereg_mr_cb
 *   Pointer to dereg_mr func
 *
 */
void
mlx5_os_set_reg_mr_cb(mlx5_reg_mr_t *reg_mr_cb, mlx5_dereg_mr_t *dereg_mr_cb)
{
	*reg_mr_cb = mlx5_os_reg_mr;
	*dereg_mr_cb = mlx5_os_dereg_mr;
}

/*
 * In Windows, no need to wrap the MR, no known issue for it in kernel.
 * Use the regular function to create direct MR.
 */
int
mlx5_os_wrapped_mkey_create(void *ctx, void *pd, uint32_t pdn, void *addr,
			    size_t length, struct mlx5_pmd_wrapped_mr *wpmd_mr)
{
	struct mlx5_pmd_mr pmd_mr = {0};
	int ret = mlx5_os_reg_mr(pd, addr, length, &pmd_mr);

	(void)pdn;
	(void)ctx;
	if (ret != 0)
		return -1;
	wpmd_mr->addr = addr;
	wpmd_mr->len = length;
	wpmd_mr->obj = pmd_mr.obj;
	wpmd_mr->imkey = pmd_mr.mkey;
	wpmd_mr->lkey = pmd_mr.mkey->id;
	return 0;
}

void
mlx5_os_wrapped_mkey_destroy(struct mlx5_pmd_wrapped_mr *wpmd_mr)
{
	struct mlx5_pmd_mr pmd_mr;

	if (!wpmd_mr)
		return;
	pmd_mr.addr = wpmd_mr->addr;
	pmd_mr.len = wpmd_mr->len;
	pmd_mr.obj = wpmd_mr->obj;
	pmd_mr.mkey = wpmd_mr->imkey;
	pmd_mr.lkey = wpmd_mr->lkey;
	mlx5_os_dereg_mr(&pmd_mr);
	memset(wpmd_mr, 0, sizeof(*wpmd_mr));
}
