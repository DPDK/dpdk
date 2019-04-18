/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2012 6WIND S.A.
 * Copyright 2012 Mellanox Technologies, Ltd
 */

/**
 * @file
 * mlx4 driver initialization.
 */

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_common.h>
#include <rte_config.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_interrupts.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "mlx4.h"
#include "mlx4_glue.h"
#include "mlx4_flow.h"
#include "mlx4_mr.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

static const char *MZ_MLX4_PMD_SHARED_DATA = "mlx4_pmd_shared_data";

/* Shared memory between primary and secondary processes. */
struct mlx4_shared_data *mlx4_shared_data;

/* Spinlock for mlx4_shared_data allocation. */
static rte_spinlock_t mlx4_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/* Process local data for secondary processes. */
static struct mlx4_local_data mlx4_local_data;

/** Configuration structure for device arguments. */
struct mlx4_conf {
	struct {
		uint32_t present; /**< Bit-field for existing ports. */
		uint32_t enabled; /**< Bit-field for user-enabled ports. */
	} ports;
	int mr_ext_memseg_en;
	/** Whether memseg should be extended for MR creation. */
};

/* Available parameters list. */
const char *pmd_mlx4_init_params[] = {
	MLX4_PMD_PORT_KVARG,
	MLX4_MR_EXT_MEMSEG_EN_KVARG,
	NULL,
};

static void mlx4_dev_stop(struct rte_eth_dev *dev);

/**
 * Initialize shared data between primary and secondary process.
 *
 * A memzone is reserved by primary process and secondary processes attach to
 * the memzone.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_init_shared_data(void)
{
	const struct rte_memzone *mz;
	int ret = 0;

	rte_spinlock_lock(&mlx4_shared_data_lock);
	if (mlx4_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate shared memory. */
			mz = rte_memzone_reserve(MZ_MLX4_PMD_SHARED_DATA,
						 sizeof(*mlx4_shared_data),
						 SOCKET_ID_ANY, 0);
			if (mz == NULL) {
				ERROR("Cannot allocate mlx4 shared data\n");
				ret = -rte_errno;
				goto error;
			}
			mlx4_shared_data = mz->addr;
			memset(mlx4_shared_data, 0, sizeof(*mlx4_shared_data));
			rte_spinlock_init(&mlx4_shared_data->lock);
		} else {
			/* Lookup allocated shared memory. */
			mz = rte_memzone_lookup(MZ_MLX4_PMD_SHARED_DATA);
			if (mz == NULL) {
				ERROR("Cannot attach mlx4 shared data\n");
				ret = -rte_errno;
				goto error;
			}
			mlx4_shared_data = mz->addr;
			memset(&mlx4_local_data, 0, sizeof(mlx4_local_data));
		}
	}
error:
	rte_spinlock_unlock(&mlx4_shared_data_lock);
	return ret;
}

#ifdef HAVE_IBV_MLX4_BUF_ALLOCATORS
/**
 * Verbs callback to allocate a memory. This function should allocate the space
 * according to the size provided residing inside a huge page.
 * Please note that all allocation must respect the alignment from libmlx4
 * (i.e. currently sysconf(_SC_PAGESIZE)).
 *
 * @param[in] size
 *   The size in bytes of the memory to allocate.
 * @param[in] data
 *   A pointer to the callback data.
 *
 * @return
 *   Allocated buffer, NULL otherwise and rte_errno is set.
 */
static void *
mlx4_alloc_verbs_buf(size_t size, void *data)
{
	struct mlx4_priv *priv = data;
	void *ret;
	size_t alignment = sysconf(_SC_PAGESIZE);
	unsigned int socket = SOCKET_ID_ANY;

	if (priv->verbs_alloc_ctx.type == MLX4_VERBS_ALLOC_TYPE_TX_QUEUE) {
		const struct txq *txq = priv->verbs_alloc_ctx.obj;

		socket = txq->socket;
	} else if (priv->verbs_alloc_ctx.type ==
		   MLX4_VERBS_ALLOC_TYPE_RX_QUEUE) {
		const struct rxq *rxq = priv->verbs_alloc_ctx.obj;

		socket = rxq->socket;
	}
	assert(data != NULL);
	ret = rte_malloc_socket(__func__, size, alignment, socket);
	if (!ret && size)
		rte_errno = ENOMEM;
	return ret;
}

/**
 * Verbs callback to free a memory.
 *
 * @param[in] ptr
 *   A pointer to the memory to free.
 * @param[in] data
 *   A pointer to the callback data.
 */
static void
mlx4_free_verbs_buf(void *ptr, void *data __rte_unused)
{
	assert(data != NULL);
	rte_free(ptr);
}
#endif

/**
 * Initialize process private data structure.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_proc_priv_init(struct rte_eth_dev *dev)
{
	struct mlx4_proc_priv *ppriv;
	size_t ppriv_size;

	/*
	 * UAR register table follows the process private structure. BlueFlame
	 * registers for Tx queues are stored in the table.
	 */
	ppriv_size = sizeof(struct mlx4_proc_priv) +
		     dev->data->nb_tx_queues * sizeof(void *);
	ppriv = rte_malloc_socket("mlx4_proc_priv", ppriv_size,
				  RTE_CACHE_LINE_SIZE, dev->device->numa_node);
	if (!ppriv) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	ppriv->uar_table_sz = ppriv_size;
	dev->process_private = ppriv;
	return 0;
}

/**
 * Un-initialize process private data structure.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_proc_priv_uninit(struct rte_eth_dev *dev)
{
	if (!dev->process_private)
		return;
	rte_free(dev->process_private);
	dev->process_private = NULL;
}

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_configure(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	int ret;

	/* Prepare internal flow rules. */
	ret = mlx4_flow_sync(priv, &error);
	if (ret) {
		ERROR("cannot set up internal flow rules (code %d, \"%s\"),"
		      " flow error type %d, cause %p, message: %s",
		      -ret, strerror(-ret), error.type, error.cause,
		      error.message ? error.message : "(unspecified)");
		goto exit;
	}
	ret = mlx4_intr_install(priv);
	if (ret) {
		ERROR("%p: interrupt handler installation failed",
		      (void *)dev);
		goto exit;
	}
	ret = mlx4_proc_priv_init(dev);
	if (ret) {
		ERROR("%p: process private data allocation failed",
		      (void *)dev);
		goto exit;
	}
exit:
	return ret;
}

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by initializing common RSS resources and attaching
 * all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_start(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	int ret;

	if (priv->started)
		return 0;
	DEBUG("%p: attaching configured flows to all RX queues", (void *)dev);
	priv->started = 1;
	ret = mlx4_rss_init(priv);
	if (ret) {
		ERROR("%p: cannot initialize RSS resources: %s",
		      (void *)dev, strerror(-ret));
		goto err;
	}
#ifndef NDEBUG
	mlx4_mr_dump_dev(dev);
#endif
	ret = mlx4_rxq_intr_enable(priv);
	if (ret) {
		ERROR("%p: interrupt handler installation failed",
		     (void *)dev);
		goto err;
	}
	ret = mlx4_flow_sync(priv, &error);
	if (ret) {
		ERROR("%p: cannot attach flow rules (code %d, \"%s\"),"
		      " flow error type %d, cause %p, message: %s",
		      (void *)dev,
		      -ret, strerror(-ret), error.type, error.cause,
		      error.message ? error.message : "(unspecified)");
		goto err;
	}
	rte_wmb();
	dev->tx_pkt_burst = mlx4_tx_burst;
	dev->rx_pkt_burst = mlx4_rx_burst;
	/* Enable datapath on secondary process. */
	mlx4_mp_req_start_rxtx(dev);
	return 0;
err:
	mlx4_dev_stop(dev);
	return ret;
}

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_stop(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;

	if (!priv->started)
		return;
	DEBUG("%p: detaching flows from all RX queues", (void *)dev);
	priv->started = 0;
	dev->tx_pkt_burst = mlx4_tx_burst_removed;
	dev->rx_pkt_burst = mlx4_rx_burst_removed;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx4_mp_req_stop_rxtx(dev);
	mlx4_flow_sync(priv, NULL);
	mlx4_rxq_intr_disable(priv);
	mlx4_rss_deinit(priv);
}

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_close(struct rte_eth_dev *dev)
{
	struct mlx4_priv *priv = dev->data->dev_private;
	unsigned int i;

	DEBUG("%p: closing device \"%s\"",
	      (void *)dev,
	      ((priv->ctx != NULL) ? priv->ctx->device->name : ""));
	dev->rx_pkt_burst = mlx4_rx_burst_removed;
	dev->tx_pkt_burst = mlx4_tx_burst_removed;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx4_mp_req_stop_rxtx(dev);
	mlx4_flow_clean(priv);
	mlx4_rss_deinit(priv);
	for (i = 0; i != dev->data->nb_rx_queues; ++i)
		mlx4_rx_queue_release(dev->data->rx_queues[i]);
	for (i = 0; i != dev->data->nb_tx_queues; ++i)
		mlx4_tx_queue_release(dev->data->tx_queues[i]);
	mlx4_proc_priv_uninit(dev);
	mlx4_mr_release(dev);
	if (priv->pd != NULL) {
		assert(priv->ctx != NULL);
		claim_zero(mlx4_glue->dealloc_pd(priv->pd));
		claim_zero(mlx4_glue->close_device(priv->ctx));
	} else
		assert(priv->ctx == NULL);
	mlx4_intr_uninstall(priv);
	memset(priv, 0, sizeof(*priv));
}

static const struct eth_dev_ops mlx4_dev_ops = {
	.dev_configure = mlx4_dev_configure,
	.dev_start = mlx4_dev_start,
	.dev_stop = mlx4_dev_stop,
	.dev_set_link_down = mlx4_dev_set_link_down,
	.dev_set_link_up = mlx4_dev_set_link_up,
	.dev_close = mlx4_dev_close,
	.link_update = mlx4_link_update,
	.promiscuous_enable = mlx4_promiscuous_enable,
	.promiscuous_disable = mlx4_promiscuous_disable,
	.allmulticast_enable = mlx4_allmulticast_enable,
	.allmulticast_disable = mlx4_allmulticast_disable,
	.mac_addr_remove = mlx4_mac_addr_remove,
	.mac_addr_add = mlx4_mac_addr_add,
	.mac_addr_set = mlx4_mac_addr_set,
	.set_mc_addr_list = mlx4_set_mc_addr_list,
	.stats_get = mlx4_stats_get,
	.stats_reset = mlx4_stats_reset,
	.fw_version_get = mlx4_fw_version_get,
	.dev_infos_get = mlx4_dev_infos_get,
	.dev_supported_ptypes_get = mlx4_dev_supported_ptypes_get,
	.vlan_filter_set = mlx4_vlan_filter_set,
	.rx_queue_setup = mlx4_rx_queue_setup,
	.tx_queue_setup = mlx4_tx_queue_setup,
	.rx_queue_release = mlx4_rx_queue_release,
	.tx_queue_release = mlx4_tx_queue_release,
	.flow_ctrl_get = mlx4_flow_ctrl_get,
	.flow_ctrl_set = mlx4_flow_ctrl_set,
	.mtu_set = mlx4_mtu_set,
	.filter_ctrl = mlx4_filter_ctrl,
	.rx_queue_intr_enable = mlx4_rx_intr_enable,
	.rx_queue_intr_disable = mlx4_rx_intr_disable,
	.is_removed = mlx4_is_removed,
};

/* Available operations from secondary process. */
static const struct eth_dev_ops mlx4_dev_sec_ops = {
	.stats_get = mlx4_stats_get,
	.stats_reset = mlx4_stats_reset,
	.fw_version_get = mlx4_fw_version_get,
	.dev_infos_get = mlx4_dev_infos_get,
};

/**
 * Get PCI information from struct ibv_device.
 *
 * @param device
 *   Pointer to Ethernet device structure.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_ibv_device_to_pci_addr(const struct ibv_device *device,
			    struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", device->ibdev_path);

	file = fopen(path, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);
		int ret;

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1))
			while (line[(len - 1)] != '\n') {
				ret = fgetc(file);
				if (ret == EOF)
					break;
				line[(len - 1)] = ret;
			}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			ret = 0;
			break;
		}
	}
	fclose(file);
	return 0;
}

/**
 * Verify and store value for device argument.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param[in, out] conf
 *   Shared configuration data.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_arg_parse(const char *key, const char *val, struct mlx4_conf *conf)
{
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		WARN("%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX4_PMD_PORT_KVARG, key) == 0) {
		uint32_t ports = rte_log2_u32(conf->ports.present + 1);

		if (tmp >= ports) {
			ERROR("port index %lu outside range [0,%" PRIu32 ")",
			      tmp, ports);
			return -EINVAL;
		}
		if (!(conf->ports.present & (1 << tmp))) {
			rte_errno = EINVAL;
			ERROR("invalid port index %lu", tmp);
			return -rte_errno;
		}
		conf->ports.enabled |= 1 << tmp;
	} else if (strcmp(MLX4_MR_EXT_MEMSEG_EN_KVARG, key) == 0) {
		conf->mr_ext_memseg_en = !!tmp;
	} else {
		rte_errno = EINVAL;
		WARN("%s: unknown parameter", key);
		return -rte_errno;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_args(struct rte_devargs *devargs, struct mlx4_conf *conf)
{
	struct rte_kvargs *kvlist;
	unsigned int arg_count;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, pmd_mlx4_init_params);
	if (kvlist == NULL) {
		rte_errno = EINVAL;
		ERROR("failed to parse kvargs");
		return -rte_errno;
	}
	/* Process parameters. */
	for (i = 0; pmd_mlx4_init_params[i]; ++i) {
		arg_count = rte_kvargs_count(kvlist, pmd_mlx4_init_params[i]);
		while (arg_count-- > 0) {
			ret = rte_kvargs_process(kvlist,
						 pmd_mlx4_init_params[i],
						 (int (*)(const char *,
							  const char *,
							  void *))
						 mlx4_arg_parse,
						 conf);
			if (ret != 0)
				goto free_kvlist;
		}
	}
free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

/**
 * Interpret RSS capabilities reported by device.
 *
 * This function returns the set of usable Verbs RSS hash fields, kernel
 * quirks taken into account.
 *
 * @param ctx
 *   Verbs context.
 * @param pd
 *   Verbs protection domain.
 * @param device_attr_ex
 *   Extended device attributes to interpret.
 *
 * @return
 *   Usable RSS hash fields mask in Verbs format.
 */
static uint64_t
mlx4_hw_rss_sup(struct ibv_context *ctx, struct ibv_pd *pd,
		struct ibv_device_attr_ex *device_attr_ex)
{
	uint64_t hw_rss_sup = device_attr_ex->rss_caps.rx_hash_fields_mask;
	struct ibv_cq *cq = NULL;
	struct ibv_wq *wq = NULL;
	struct ibv_rwq_ind_table *ind = NULL;
	struct ibv_qp *qp = NULL;

	if (!hw_rss_sup) {
		WARN("no RSS capabilities reported; disabling support for UDP"
		     " RSS and inner VXLAN RSS");
		return IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_DST_IPV4 |
			IBV_RX_HASH_SRC_IPV6 | IBV_RX_HASH_DST_IPV6 |
			IBV_RX_HASH_SRC_PORT_TCP | IBV_RX_HASH_DST_PORT_TCP;
	}
	if (!(hw_rss_sup & IBV_RX_HASH_INNER))
		return hw_rss_sup;
	/*
	 * Although reported as supported, missing code in some Linux
	 * versions (v4.15, v4.16) prevents the creation of hash QPs with
	 * inner capability.
	 *
	 * There is no choice but to attempt to instantiate a temporary RSS
	 * context in order to confirm its support.
	 */
	cq = mlx4_glue->create_cq(ctx, 1, NULL, NULL, 0);
	wq = cq ? mlx4_glue->create_wq
		(ctx,
		 &(struct ibv_wq_init_attr){
			.wq_type = IBV_WQT_RQ,
			.max_wr = 1,
			.max_sge = 1,
			.pd = pd,
			.cq = cq,
		 }) : NULL;
	ind = wq ? mlx4_glue->create_rwq_ind_table
		(ctx,
		 &(struct ibv_rwq_ind_table_init_attr){
			.log_ind_tbl_size = 0,
			.ind_tbl = &wq,
			.comp_mask = 0,
		 }) : NULL;
	qp = ind ? mlx4_glue->create_qp_ex
		(ctx,
		 &(struct ibv_qp_init_attr_ex){
			.comp_mask =
				(IBV_QP_INIT_ATTR_PD |
				 IBV_QP_INIT_ATTR_RX_HASH |
				 IBV_QP_INIT_ATTR_IND_TABLE),
			.qp_type = IBV_QPT_RAW_PACKET,
			.pd = pd,
			.rwq_ind_tbl = ind,
			.rx_hash_conf = {
				.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = MLX4_RSS_HASH_KEY_SIZE,
				.rx_hash_key = mlx4_rss_hash_key_default,
				.rx_hash_fields_mask = hw_rss_sup,
			},
		 }) : NULL;
	if (!qp) {
		WARN("disabling unusable inner RSS capability due to kernel"
		     " quirk");
		hw_rss_sup &= ~IBV_RX_HASH_INNER;
	} else {
		claim_zero(mlx4_glue->destroy_qp(qp));
	}
	if (ind)
		claim_zero(mlx4_glue->destroy_rwq_ind_table(ind));
	if (wq)
		claim_zero(mlx4_glue->destroy_wq(wq));
	if (cq)
		claim_zero(mlx4_glue->destroy_cq(cq));
	return hw_rss_sup;
}

static struct rte_pci_driver mlx4_driver;

/**
 * PMD global initialization.
 *
 * Independent from individual device, this function initializes global
 * per-PMD data structures distinguishing primary and secondary processes.
 * Hence, each initialization is called once per a process.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_init_once(void)
{
	struct mlx4_shared_data *sd;
	struct mlx4_local_data *ld = &mlx4_local_data;

	if (mlx4_init_shared_data())
		return -rte_errno;
	sd = mlx4_shared_data;
	assert(sd);
	rte_spinlock_lock(&sd->lock);
	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		if (sd->init_done)
			break;
		LIST_INIT(&sd->mem_event_cb_list);
		rte_rwlock_init(&sd->mem_event_rwlock);
		rte_mem_event_callback_register("MLX4_MEM_EVENT_CB",
						mlx4_mr_mem_event_cb, NULL);
		mlx4_mp_init_primary();
		sd->init_done = true;
		break;
	case RTE_PROC_SECONDARY:
		if (ld->init_done)
			break;
		mlx4_mp_init_secondary();
		++sd->secondary_cnt;
		ld->init_done = true;
		break;
	default:
		break;
	}
	rte_spinlock_unlock(&sd->lock);
	return 0;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function creates an Ethernet device for each port of a given
 * PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx4_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct ibv_device **list;
	struct ibv_device *ibv_dev;
	int err = 0;
	struct ibv_context *attr_ctx = NULL;
	struct ibv_device_attr device_attr;
	struct ibv_device_attr_ex device_attr_ex;
	struct mlx4_conf conf = {
		.ports.present = 0,
		.mr_ext_memseg_en = 1,
	};
	unsigned int vf;
	int i;

	(void)pci_drv;
	err = mlx4_init_once();
	if (err) {
		ERROR("unable to init PMD global data: %s",
		      strerror(rte_errno));
		return -rte_errno;
	}
	assert(pci_drv == &mlx4_driver);
	list = mlx4_glue->get_device_list(&i);
	if (list == NULL) {
		rte_errno = errno;
		assert(rte_errno);
		if (rte_errno == ENOSYS)
			ERROR("cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	assert(i >= 0);
	/*
	 * For each listed device, check related sysfs entry against
	 * the provided PCI ID.
	 */
	while (i != 0) {
		struct rte_pci_addr pci_addr;

		--i;
		DEBUG("checking device \"%s\"", list[i]->name);
		if (mlx4_ibv_device_to_pci_addr(list[i], &pci_addr))
			continue;
		if ((pci_dev->addr.domain != pci_addr.domain) ||
		    (pci_dev->addr.bus != pci_addr.bus) ||
		    (pci_dev->addr.devid != pci_addr.devid) ||
		    (pci_dev->addr.function != pci_addr.function))
			continue;
		vf = (pci_dev->id.device_id ==
		      PCI_DEVICE_ID_MELLANOX_CONNECTX3VF);
		INFO("PCI information matches, using device \"%s\" (VF: %s)",
		     list[i]->name, (vf ? "true" : "false"));
		attr_ctx = mlx4_glue->open_device(list[i]);
		err = errno;
		break;
	}
	if (attr_ctx == NULL) {
		mlx4_glue->free_device_list(list);
		switch (err) {
		case 0:
			rte_errno = ENODEV;
			ERROR("cannot access device, is mlx4_ib loaded?");
			return -rte_errno;
		case EINVAL:
			rte_errno = EINVAL;
			ERROR("cannot use device, are drivers up to date?");
			return -rte_errno;
		}
		assert(err > 0);
		rte_errno = err;
		return -rte_errno;
	}
	ibv_dev = list[i];
	DEBUG("device opened");
	if (mlx4_glue->query_device(attr_ctx, &device_attr)) {
		err = ENODEV;
		goto error;
	}
	INFO("%u port(s) detected", device_attr.phys_port_cnt);
	conf.ports.present |= (UINT64_C(1) << device_attr.phys_port_cnt) - 1;
	if (mlx4_args(pci_dev->device.devargs, &conf)) {
		ERROR("failed to process device arguments");
		err = EINVAL;
		goto error;
	}
	/* Use all ports when none are defined */
	if (!conf.ports.enabled)
		conf.ports.enabled = conf.ports.present;
	/* Retrieve extended device attributes. */
	if (mlx4_glue->query_device_ex(attr_ctx, NULL, &device_attr_ex)) {
		err = ENODEV;
		goto error;
	}
	assert(device_attr.max_sge >= MLX4_MAX_SGE);
	for (i = 0; i < device_attr.phys_port_cnt; i++) {
		uint32_t port = i + 1; /* ports are indexed from one */
		struct ibv_context *ctx = NULL;
		struct ibv_port_attr port_attr;
		struct ibv_pd *pd = NULL;
		struct mlx4_priv *priv = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		struct ether_addr mac;
		char name[RTE_ETH_NAME_MAX_LEN];

		/* If port is not enabled, skip. */
		if (!(conf.ports.enabled & (1 << i)))
			continue;
		DEBUG("using port %u", port);
		ctx = mlx4_glue->open_device(ibv_dev);
		if (ctx == NULL) {
			err = ENODEV;
			goto port_error;
		}
		snprintf(name, sizeof(name), "%s port %u",
			 mlx4_glue->get_device_name(ibv_dev), port);
		if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
			eth_dev = rte_eth_dev_attach_secondary(name);
			if (eth_dev == NULL) {
				ERROR("can not attach rte ethdev");
				rte_errno = ENOMEM;
				err = rte_errno;
				goto error;
			}
			priv = eth_dev->data->dev_private;
			if (!priv->verbs_alloc_ctx.enabled) {
				ERROR("secondary process is not supported"
				      " due to lack of external allocator"
				      " from Verbs");
				rte_errno = ENOTSUP;
				err = rte_errno;
				goto error;
			}
			eth_dev->device = &pci_dev->device;
			eth_dev->dev_ops = &mlx4_dev_sec_ops;
			err = mlx4_proc_priv_init(eth_dev);
			if (err)
				goto error;
			/* Receive command fd from primary process. */
			err = mlx4_mp_req_verbs_cmd_fd(eth_dev);
			if (err < 0) {
				err = rte_errno;
				goto error;
			}
			/* Remap UAR for Tx queues. */
			err = mlx4_tx_uar_init_secondary(eth_dev, err);
			if (err) {
				err = rte_errno;
				goto error;
			}
			/*
			 * Ethdev pointer is still required as input since
			 * the primary device is not accessible from the
			 * secondary process.
			 */
			eth_dev->tx_pkt_burst = mlx4_tx_burst;
			eth_dev->rx_pkt_burst = mlx4_rx_burst;
			claim_zero(mlx4_glue->close_device(ctx));
			rte_eth_copy_pci_info(eth_dev, pci_dev);
			rte_eth_dev_probing_finish(eth_dev);
			continue;
		}
		/* Check port status. */
		err = mlx4_glue->query_port(ctx, port, &port_attr);
		if (err) {
			err = ENODEV;
			ERROR("port query failed: %s", strerror(err));
			goto port_error;
		}
		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
			err = ENOTSUP;
			ERROR("port %d is not configured in Ethernet mode",
			      port);
			goto port_error;
		}
		if (port_attr.state != IBV_PORT_ACTIVE)
			DEBUG("port %d is not active: \"%s\" (%d)",
			      port, mlx4_glue->port_state_str(port_attr.state),
			      port_attr.state);
		/* Make asynchronous FD non-blocking to handle interrupts. */
		err = mlx4_fd_set_non_blocking(ctx->async_fd);
		if (err) {
			ERROR("cannot make asynchronous FD non-blocking: %s",
			      strerror(err));
			goto port_error;
		}
		/* Allocate protection domain. */
		pd = mlx4_glue->alloc_pd(ctx);
		if (pd == NULL) {
			err = ENOMEM;
			ERROR("PD allocation failure");
			goto port_error;
		}
		/* from rte_ethdev.c */
		priv = rte_zmalloc("ethdev private structure",
				   sizeof(*priv),
				   RTE_CACHE_LINE_SIZE);
		if (priv == NULL) {
			err = ENOMEM;
			ERROR("priv allocation failure");
			goto port_error;
		}
		priv->ctx = ctx;
		priv->device_attr = device_attr;
		priv->port = port;
		priv->pd = pd;
		priv->mtu = ETHER_MTU;
		priv->vf = vf;
		priv->hw_csum =	!!(device_attr.device_cap_flags &
				   IBV_DEVICE_RAW_IP_CSUM);
		DEBUG("checksum offloading is %ssupported",
		      (priv->hw_csum ? "" : "not "));
		/* Only ConnectX-3 Pro supports tunneling. */
		priv->hw_csum_l2tun =
			priv->hw_csum &&
			(device_attr.vendor_part_id ==
			 PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO);
		DEBUG("L2 tunnel checksum offloads are %ssupported",
		      priv->hw_csum_l2tun ? "" : "not ");
		priv->hw_rss_sup = mlx4_hw_rss_sup(priv->ctx, priv->pd,
						   &device_attr_ex);
		DEBUG("supported RSS hash fields mask: %016" PRIx64,
		      priv->hw_rss_sup);
		priv->hw_rss_max_qps =
			device_attr_ex.rss_caps.max_rwq_indirection_table_size;
		DEBUG("MAX RSS queues %d", priv->hw_rss_max_qps);
		priv->hw_fcs_strip = !!(device_attr_ex.raw_packet_caps &
					IBV_RAW_PACKET_CAP_SCATTER_FCS);
		DEBUG("FCS stripping toggling is %ssupported",
		      priv->hw_fcs_strip ? "" : "not ");
		priv->tso =
			((device_attr_ex.tso_caps.max_tso > 0) &&
			 (device_attr_ex.tso_caps.supported_qpts &
			  (1 << IBV_QPT_RAW_PACKET)));
		if (priv->tso)
			priv->tso_max_payload_sz =
					device_attr_ex.tso_caps.max_tso;
		DEBUG("TSO is %ssupported",
		      priv->tso ? "" : "not ");
		priv->mr_ext_memseg_en = conf.mr_ext_memseg_en;
		/* Configure the first MAC address by default. */
		err = mlx4_get_mac(priv, &mac.addr_bytes);
		if (err) {
			ERROR("cannot get MAC address, is mlx4_en loaded?"
			      " (error: %s)", strerror(err));
			goto port_error;
		}
		INFO("port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		     priv->port,
		     mac.addr_bytes[0], mac.addr_bytes[1],
		     mac.addr_bytes[2], mac.addr_bytes[3],
		     mac.addr_bytes[4], mac.addr_bytes[5]);
		/* Register MAC address. */
		priv->mac[0] = mac;
#ifndef NDEBUG
		{
			char ifname[IF_NAMESIZE];

			if (mlx4_get_ifname(priv, &ifname) == 0)
				DEBUG("port %u ifname is \"%s\"",
				      priv->port, ifname);
			else
				DEBUG("port %u ifname is unknown", priv->port);
		}
#endif
		/* Get actual MTU if possible. */
		mlx4_mtu_get(priv, &priv->mtu);
		DEBUG("port %u MTU is %u", priv->port, priv->mtu);
		eth_dev = rte_eth_dev_allocate(name);
		if (eth_dev == NULL) {
			err = ENOMEM;
			ERROR("can not allocate rte ethdev");
			goto port_error;
		}
		eth_dev->data->dev_private = priv;
		eth_dev->data->mac_addrs = priv->mac;
		eth_dev->device = &pci_dev->device;
		rte_eth_copy_pci_info(eth_dev, pci_dev);
		/* Initialize local interrupt handle for current port. */
		priv->intr_handle = (struct rte_intr_handle){
			.fd = -1,
			.type = RTE_INTR_HANDLE_EXT,
		};
		/*
		 * Override ethdev interrupt handle pointer with private
		 * handle instead of that of the parent PCI device used by
		 * default. This prevents it from being shared between all
		 * ports of the same PCI device since each of them is
		 * associated its own Verbs context.
		 *
		 * Rx interrupts in particular require this as the PMD has
		 * no control over the registration of queue interrupts
		 * besides setting up eth_dev->intr_handle, the rest is
		 * handled by rte_intr_rx_ctl().
		 */
		eth_dev->intr_handle = &priv->intr_handle;
		priv->dev_data = eth_dev->data;
		eth_dev->dev_ops = &mlx4_dev_ops;
#ifdef HAVE_IBV_MLX4_BUF_ALLOCATORS
		/* Hint libmlx4 to use PMD allocator for data plane resources */
		struct mlx4dv_ctx_allocators alctr = {
			.alloc = &mlx4_alloc_verbs_buf,
			.free = &mlx4_free_verbs_buf,
			.data = priv,
		};
		err = mlx4_glue->dv_set_context_attr
			(ctx, MLX4DV_SET_CTX_ATTR_BUF_ALLOCATORS,
			 (void *)((uintptr_t)&alctr));
		if (err)
			WARN("Verbs external allocator is not supported");
		else
			priv->verbs_alloc_ctx.enabled = 1;
#endif
		/* Bring Ethernet device up. */
		DEBUG("forcing Ethernet interface up");
		mlx4_dev_set_link_up(eth_dev);
		/* Update link status once if waiting for LSC. */
		if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
			mlx4_link_update(eth_dev, 0);
		/*
		 * Once the device is added to the list of memory event
		 * callback, its global MR cache table cannot be expanded
		 * on the fly because of deadlock. If it overflows, lookup
		 * should be done by searching MR list linearly, which is slow.
		 */
		err = mlx4_mr_btree_init(&priv->mr.cache,
					 MLX4_MR_BTREE_CACHE_N * 2,
					 eth_dev->device->numa_node);
		if (err) {
			/* rte_errno is already set. */
			goto port_error;
		}
		/* Add device to memory callback list. */
		rte_rwlock_write_lock(&mlx4_shared_data->mem_event_rwlock);
		LIST_INSERT_HEAD(&mlx4_shared_data->mem_event_cb_list,
				 priv, mem_event_cb);
		rte_rwlock_write_unlock(&mlx4_shared_data->mem_event_rwlock);
		rte_eth_dev_probing_finish(eth_dev);
		continue;
port_error:
		rte_free(priv);
		if (eth_dev != NULL)
			eth_dev->data->dev_private = NULL;
		if (pd)
			claim_zero(mlx4_glue->dealloc_pd(pd));
		if (ctx)
			claim_zero(mlx4_glue->close_device(ctx));
		if (eth_dev != NULL) {
			/* mac_addrs must not be freed because part of dev_private */
			eth_dev->data->mac_addrs = NULL;
			rte_eth_dev_release_port(eth_dev);
		}
		break;
	}
	/*
	 * XXX if something went wrong in the loop above, there is a resource
	 * leak (ctx, pd, priv, dpdk ethdev) but we can do nothing about it as
	 * long as the dpdk does not provide a way to deallocate a ethdev and a
	 * way to enumerate the registered ethdevs to free the previous ones.
	 */
error:
	if (attr_ctx)
		claim_zero(mlx4_glue->close_device(attr_ctx));
	if (list)
		mlx4_glue->free_device_list(list);
	if (err)
		rte_errno = err;
	return -err;
}

static const struct rte_pci_id mlx4_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3VF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx4_driver = {
	.driver = {
		.name = MLX4_DRIVER_NAME
	},
	.id_table = mlx4_pci_id_map,
	.probe = mlx4_pci_probe,
	.drv_flags = RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_INTR_RMV,
};

#ifdef RTE_IBVERBS_LINK_DLOPEN

/**
 * Suffix RTE_EAL_PMD_PATH with "-glue".
 *
 * This function performs a sanity check on RTE_EAL_PMD_PATH before
 * suffixing its last component.
 *
 * @param buf[out]
 *   Output buffer, should be large enough otherwise NULL is returned.
 * @param size
 *   Size of @p out.
 *
 * @return
 *   Pointer to @p buf or @p NULL in case suffix cannot be appended.
 */
static char *
mlx4_glue_path(char *buf, size_t size)
{
	static const char *const bad[] = { "/", ".", "..", NULL };
	const char *path = RTE_EAL_PMD_PATH;
	size_t len = strlen(path);
	size_t off;
	int i;

	while (len && path[len - 1] == '/')
		--len;
	for (off = len; off && path[off - 1] != '/'; --off)
		;
	for (i = 0; bad[i]; ++i)
		if (!strncmp(path + off, bad[i], (int)(len - off)))
			goto error;
	i = snprintf(buf, size, "%.*s-glue", (int)len, path);
	if (i == -1 || (size_t)i >= size)
		goto error;
	return buf;
error:
	ERROR("unable to append \"-glue\" to last component of"
	      " RTE_EAL_PMD_PATH (\"" RTE_EAL_PMD_PATH "\"),"
	      " please re-configure DPDK");
	return NULL;
}

/**
 * Initialization routine for run-time dependency on rdma-core.
 */
static int
mlx4_glue_init(void)
{
	char glue_path[sizeof(RTE_EAL_PMD_PATH) - 1 + sizeof("-glue")];
	const char *path[] = {
		/*
		 * A basic security check is necessary before trusting
		 * MLX4_GLUE_PATH, which may override RTE_EAL_PMD_PATH.
		 */
		(geteuid() == getuid() && getegid() == getgid() ?
		 getenv("MLX4_GLUE_PATH") : NULL),
		/*
		 * When RTE_EAL_PMD_PATH is set, use its glue-suffixed
		 * variant, otherwise let dlopen() look up libraries on its
		 * own.
		 */
		(*RTE_EAL_PMD_PATH ?
		 mlx4_glue_path(glue_path, sizeof(glue_path)) : ""),
	};
	unsigned int i = 0;
	void *handle = NULL;
	void **sym;
	const char *dlmsg;

	while (!handle && i != RTE_DIM(path)) {
		const char *end;
		size_t len;
		int ret;

		if (!path[i]) {
			++i;
			continue;
		}
		end = strpbrk(path[i], ":;");
		if (!end)
			end = path[i] + strlen(path[i]);
		len = end - path[i];
		ret = 0;
		do {
			char name[ret + 1];

			ret = snprintf(name, sizeof(name), "%.*s%s" MLX4_GLUE,
				       (int)len, path[i],
				       (!len || *(end - 1) == '/') ? "" : "/");
			if (ret == -1)
				break;
			if (sizeof(name) != (size_t)ret + 1)
				continue;
			DEBUG("looking for rdma-core glue as \"%s\"", name);
			handle = dlopen(name, RTLD_LAZY);
			break;
		} while (1);
		path[i] = end + 1;
		if (!*end)
			++i;
	}
	if (!handle) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			WARN("cannot load glue library: %s", dlmsg);
		goto glue_error;
	}
	sym = dlsym(handle, "mlx4_glue");
	if (!sym || !*sym) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			ERROR("cannot resolve glue symbol: %s", dlmsg);
		goto glue_error;
	}
	mlx4_glue = *sym;
	return 0;
glue_error:
	if (handle)
		dlclose(handle);
	WARN("cannot initialize PMD due to missing run-time"
	     " dependency on rdma-core libraries (libibverbs,"
	     " libmlx4)");
	return -rte_errno;
}

#endif

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx4_pmd_init)
{
	/*
	 * MLX4_DEVICE_FATAL_CLEANUP tells ibv_destroy functions we
	 * want to get success errno value in case of calling them
	 * when the device was removed.
	 */
	setenv("MLX4_DEVICE_FATAL_CLEANUP", "1", 1);
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
#ifdef RTE_IBVERBS_LINK_DLOPEN
	if (mlx4_glue_init())
		return;
	assert(mlx4_glue);
#endif
#ifndef NDEBUG
	/* Glue structure must not contain any NULL pointers. */
	{
		unsigned int i;

		for (i = 0; i != sizeof(*mlx4_glue) / sizeof(void *); ++i)
			assert(((const void *const *)mlx4_glue)[i]);
	}
#endif
	if (strcmp(mlx4_glue->version, MLX4_GLUE_VERSION)) {
		ERROR("rdma-core glue \"%s\" mismatch: \"%s\" is required",
		      mlx4_glue->version, MLX4_GLUE_VERSION);
		return;
	}
	mlx4_glue->fork_init();
	rte_pci_register(&mlx4_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx4, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx4, mlx4_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx4,
	"* ib_uverbs & mlx4_en & mlx4_core & mlx4_ib");
