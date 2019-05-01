/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/mman.h>
#include <linux/rtnetlink.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_eal_memconfig.h>
#include <rte_kvargs.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_glue.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"

/* Device parameter to enable RX completion queue compression. */
#define MLX5_RXQ_CQE_COMP_EN "rxq_cqe_comp_en"

/* Device parameter to enable RX completion entry padding to 128B. */
#define MLX5_RXQ_CQE_PAD_EN "rxq_cqe_pad_en"

/* Device parameter to enable padding Rx packet to cacheline size. */
#define MLX5_RXQ_PKT_PAD_EN "rxq_pkt_pad_en"

/* Device parameter to enable Multi-Packet Rx queue. */
#define MLX5_RX_MPRQ_EN "mprq_en"

/* Device parameter to configure log 2 of the number of strides for MPRQ. */
#define MLX5_RX_MPRQ_LOG_STRIDE_NUM "mprq_log_stride_num"

/* Device parameter to limit the size of memcpy'd packet for MPRQ. */
#define MLX5_RX_MPRQ_MAX_MEMCPY_LEN "mprq_max_memcpy_len"

/* Device parameter to set the minimum number of Rx queues to enable MPRQ. */
#define MLX5_RXQS_MIN_MPRQ "rxqs_min_mprq"

/* Device parameter to configure inline send. */
#define MLX5_TXQ_INLINE "txq_inline"

/*
 * Device parameter to configure the number of TX queues threshold for
 * enabling inline send.
 */
#define MLX5_TXQS_MIN_INLINE "txqs_min_inline"

/*
 * Device parameter to configure the number of TX queues threshold for
 * enabling vectorized Tx.
 */
#define MLX5_TXQS_MAX_VEC "txqs_max_vec"

/* Device parameter to enable multi-packet send WQEs. */
#define MLX5_TXQ_MPW_EN "txq_mpw_en"

/* Device parameter to include 2 dsegs in the title WQEBB. */
#define MLX5_TXQ_MPW_HDR_DSEG_EN "txq_mpw_hdr_dseg_en"

/* Device parameter to limit the size of inlining packet. */
#define MLX5_TXQ_MAX_INLINE_LEN "txq_max_inline_len"

/* Device parameter to enable hardware Tx vector. */
#define MLX5_TX_VEC_EN "tx_vec_en"

/* Device parameter to enable hardware Rx vector. */
#define MLX5_RX_VEC_EN "rx_vec_en"

/* Allow L3 VXLAN flow creation. */
#define MLX5_L3_VXLAN_EN "l3_vxlan_en"

/* Activate DV E-Switch flow steering. */
#define MLX5_DV_ESW_EN "dv_esw_en"

/* Activate DV flow steering. */
#define MLX5_DV_FLOW_EN "dv_flow_en"

/* Activate Netlink support in VF mode. */
#define MLX5_VF_NL_EN "vf_nl_en"

/* Enable extending memsegs when creating a MR. */
#define MLX5_MR_EXT_MEMSEG_EN "mr_ext_memseg_en"

/* Select port representors to instantiate. */
#define MLX5_REPRESENTOR "representor"

#ifndef HAVE_IBV_MLX5_MOD_MPW
#define MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED (1 << 2)
#define MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW (1 << 3)
#endif

#ifndef HAVE_IBV_MLX5_MOD_CQE_128B_COMP
#define MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP (1 << 4)
#endif

static const char *MZ_MLX5_PMD_SHARED_DATA = "mlx5_pmd_shared_data";

/* Shared memory between primary and secondary processes. */
struct mlx5_shared_data *mlx5_shared_data;

/* Spinlock for mlx5_shared_data allocation. */
static rte_spinlock_t mlx5_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/* Process local data for secondary processes. */
static struct mlx5_local_data mlx5_local_data;

/** Driver-specific log messages type. */
int mlx5_logtype;

/** Data associated with devices to spawn. */
struct mlx5_dev_spawn_data {
	uint32_t ifindex; /**< Network interface index. */
	uint32_t max_port; /**< IB device maximal port index. */
	uint32_t ibv_port; /**< IB device physical port index. */
	struct mlx5_switch_info info; /**< Switch information. */
	struct ibv_device *ibv_dev; /**< Associated IB device. */
	struct rte_eth_dev *eth_dev; /**< Associated Ethernet device. */
	struct rte_pci_device *pci_dev; /**< Backend PCI device. */
};

static LIST_HEAD(, mlx5_ibv_shared) mlx5_ibv_list = LIST_HEAD_INITIALIZER();
static pthread_mutex_t mlx5_ibv_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Allocate shared IB device context. If there is multiport device the
 * master and representors will share this context, if there is single
 * port dedicated IB device, the context will be used by only given
 * port due to unification.
 *
 * Routine first searches the context for the specified IB device name,
 * if found the shared context assumed and reference counter is incremented.
 * If no context found the new one is created and initialized with specified
 * IB device context and parameters.
 *
 * @param[in] spawn
 *   Pointer to the IB device attributes (name, port, etc).
 *
 * @return
 *   Pointer to mlx5_ibv_shared object on success,
 *   otherwise NULL and rte_errno is set.
 */
static struct mlx5_ibv_shared *
mlx5_alloc_shared_ibctx(const struct mlx5_dev_spawn_data *spawn)
{
	struct mlx5_ibv_shared *sh;
	int err = 0;
	uint32_t i;

	assert(spawn);
	/* Secondary process should not create the shared context. */
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	pthread_mutex_lock(&mlx5_ibv_list_mutex);
	/* Search for IB context by device name. */
	LIST_FOREACH(sh, &mlx5_ibv_list, next) {
		if (!strcmp(sh->ibdev_name, spawn->ibv_dev->name)) {
			sh->refcnt++;
			goto exit;
		}
	}
	/* No device found, we have to create new shared context. */
	assert(spawn->max_port);
	sh = rte_zmalloc("ethdev shared ib context",
			 sizeof(struct mlx5_ibv_shared) +
			 spawn->max_port *
			 sizeof(struct mlx5_ibv_shared_port),
			 RTE_CACHE_LINE_SIZE);
	if (!sh) {
		DRV_LOG(ERR, "shared context allocation failure");
		rte_errno  = ENOMEM;
		goto exit;
	}
	/* Try to open IB device with DV first, then usual Verbs. */
	errno = 0;
	sh->ctx = mlx5_glue->dv_open_device(spawn->ibv_dev);
	if (sh->ctx) {
		sh->devx = 1;
		DRV_LOG(DEBUG, "DevX is supported");
	} else {
		sh->ctx = mlx5_glue->open_device(spawn->ibv_dev);
		if (!sh->ctx) {
			err = errno ? errno : ENODEV;
			goto error;
		}
		DRV_LOG(DEBUG, "DevX is NOT supported");
	}
	err = mlx5_glue->query_device_ex(sh->ctx, NULL, &sh->device_attr);
	if (err) {
		DRV_LOG(DEBUG, "ibv_query_device_ex() failed");
		goto error;
	}
	sh->refcnt = 1;
	sh->max_port = spawn->max_port;
	strncpy(sh->ibdev_name, sh->ctx->device->name,
		sizeof(sh->ibdev_name));
	strncpy(sh->ibdev_path, sh->ctx->device->ibdev_path,
		sizeof(sh->ibdev_path));
	sh->pci_dev = spawn->pci_dev;
	pthread_mutex_init(&sh->intr_mutex, NULL);
	/*
	 * Setting port_id to max unallowed value means
	 * there is no interrupt subhandler installed for
	 * the given port index i.
	 */
	for (i = 0; i < sh->max_port; i++)
		sh->port[i].ih_port_id = RTE_MAX_ETHPORTS;
	sh->pd = mlx5_glue->alloc_pd(sh->ctx);
	if (sh->pd == NULL) {
		DRV_LOG(ERR, "PD allocation failure");
		err = ENOMEM;
		goto error;
	}
	/*
	 * Once the device is added to the list of memory event
	 * callback, its global MR cache table cannot be expanded
	 * on the fly because of deadlock. If it overflows, lookup
	 * should be done by searching MR list linearly, which is slow.
	 *
	 * At this point the device is not added to the memory
	 * event list yet, context is just being created.
	 */
	err = mlx5_mr_btree_init(&sh->mr.cache,
				 MLX5_MR_BTREE_CACHE_N * 2,
				 sh->pci_dev->device.numa_node);
	if (err) {
		err = rte_errno;
		goto error;
	}
	LIST_INSERT_HEAD(&mlx5_ibv_list, sh, next);
exit:
	pthread_mutex_unlock(&mlx5_ibv_list_mutex);
	return sh;
error:
	pthread_mutex_unlock(&mlx5_ibv_list_mutex);
	assert(sh);
	if (sh->pd)
		claim_zero(mlx5_glue->dealloc_pd(sh->pd));
	if (sh->ctx)
		claim_zero(mlx5_glue->close_device(sh->ctx));
	rte_free(sh);
	assert(err > 0);
	rte_errno = err;
	return NULL;
}

/**
 * Free shared IB device context. Decrement counter and if zero free
 * all allocated resources and close handles.
 *
 * @param[in] sh
 *   Pointer to mlx5_ibv_shared object to free
 */
static void
mlx5_free_shared_ibctx(struct mlx5_ibv_shared *sh)
{
	pthread_mutex_lock(&mlx5_ibv_list_mutex);
#ifndef NDEBUG
	/* Check the object presence in the list. */
	struct mlx5_ibv_shared *lctx;

	LIST_FOREACH(lctx, &mlx5_ibv_list, next)
		if (lctx == sh)
			break;
	assert(lctx);
	if (lctx != sh) {
		DRV_LOG(ERR, "Freeing non-existing shared IB context");
		goto exit;
	}
#endif
	assert(sh);
	assert(sh->refcnt);
	/* Secondary process should not free the shared context. */
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (--sh->refcnt)
		goto exit;
	/* Release created Memory Regions. */
	mlx5_mr_release(sh);
	LIST_REMOVE(sh, next);
	/*
	 *  Ensure there is no async event handler installed.
	 *  Only primary process handles async device events.
	 **/
	assert(!sh->intr_cnt);
	if (sh->intr_cnt)
		rte_intr_callback_unregister
			(&sh->intr_handle, mlx5_dev_interrupt_handler, sh);
	pthread_mutex_destroy(&sh->intr_mutex);
	if (sh->pd)
		claim_zero(mlx5_glue->dealloc_pd(sh->pd));
	if (sh->ctx)
		claim_zero(mlx5_glue->close_device(sh->ctx));
	rte_free(sh);
exit:
	pthread_mutex_unlock(&mlx5_ibv_list_mutex);
}

/**
 * Initialize DR related data within private structure.
 * Routine checks the reference counter and does actual
 * resources creation/initialization only if counter is zero.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 *
 * @return
 *   Zero on success, positive error code otherwise.
 */
static int
mlx5_alloc_shared_dr(struct mlx5_priv *priv)
{
#ifdef HAVE_MLX5DV_DR
	struct mlx5_ibv_shared *sh = priv->sh;
	int err = 0;
	void *domain;

	assert(sh);
	if (sh->dv_refcnt) {
		/* Shared DV/DR structures is already initialized. */
		sh->dv_refcnt++;
		priv->dr_shared = 1;
		return 0;
	}
	/* Reference counter is zero, we should initialize structures. */
	domain = mlx5_glue->dr_create_domain(sh->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
	if (!domain) {
		DRV_LOG(ERR, "ingress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	sh->rx_domain = domain;
	domain = mlx5_glue->dr_create_domain(sh->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_TX);
	if (!domain) {
		DRV_LOG(ERR, "egress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	pthread_mutex_init(&sh->dv_mutex, NULL);
	sh->tx_domain = domain;
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (priv->config.dv_esw_en) {
		domain  = mlx5_glue->dr_create_domain
			(sh->ctx, MLX5DV_DR_DOMAIN_TYPE_FDB);
		if (!domain) {
			DRV_LOG(ERR, "FDB mlx5dv_dr_create_domain failed");
			err = errno;
			goto error;
		}
		sh->fdb_domain = domain;
		sh->esw_drop_action = mlx5_glue->dr_create_flow_action_drop();
	}
#endif
	sh->dv_refcnt++;
	priv->dr_shared = 1;
	return 0;

error:
       /* Rollback the created objects. */
	if (sh->rx_domain) {
		mlx5_glue->dr_destroy_domain(sh->rx_domain);
		sh->rx_domain = NULL;
	}
	if (sh->tx_domain) {
		mlx5_glue->dr_destroy_domain(sh->tx_domain);
		sh->tx_domain = NULL;
	}
	if (sh->fdb_domain) {
		mlx5_glue->dr_destroy_domain(sh->fdb_domain);
		sh->fdb_domain = NULL;
	}
	if (sh->esw_drop_action) {
		mlx5_glue->destroy_flow_action(sh->esw_drop_action);
		sh->esw_drop_action = NULL;
	}
	return err;
#else
	(void)priv;
	return 0;
#endif
}

/**
 * Destroy DR related data within private structure.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
static void
mlx5_free_shared_dr(struct mlx5_priv *priv)
{
#ifdef HAVE_MLX5DV_DR
	struct mlx5_ibv_shared *sh;

	if (!priv->dr_shared)
		return;
	priv->dr_shared = 0;
	sh = priv->sh;
	assert(sh);
	assert(sh->dv_refcnt);
	if (sh->dv_refcnt && --sh->dv_refcnt)
		return;
	if (sh->rx_domain) {
		mlx5_glue->dr_destroy_domain(sh->rx_domain);
		sh->rx_domain = NULL;
	}
	if (sh->tx_domain) {
		mlx5_glue->dr_destroy_domain(sh->tx_domain);
		sh->tx_domain = NULL;
	}
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (sh->fdb_domain) {
		mlx5_glue->dr_destroy_domain(sh->fdb_domain);
		sh->fdb_domain = NULL;
	}
	if (sh->esw_drop_action) {
		mlx5_glue->destroy_flow_action(sh->esw_drop_action);
		sh->esw_drop_action = NULL;
	}
#endif
	pthread_mutex_destroy(&sh->dv_mutex);
#else
	(void)priv;
#endif
}

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
mlx5_init_shared_data(void)
{
	const struct rte_memzone *mz;
	int ret = 0;

	rte_spinlock_lock(&mlx5_shared_data_lock);
	if (mlx5_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate shared memory. */
			mz = rte_memzone_reserve(MZ_MLX5_PMD_SHARED_DATA,
						 sizeof(*mlx5_shared_data),
						 SOCKET_ID_ANY, 0);
			if (mz == NULL) {
				DRV_LOG(ERR,
					"Cannot allocate mlx5 shared data\n");
				ret = -rte_errno;
				goto error;
			}
			mlx5_shared_data = mz->addr;
			memset(mlx5_shared_data, 0, sizeof(*mlx5_shared_data));
			rte_spinlock_init(&mlx5_shared_data->lock);
		} else {
			/* Lookup allocated shared memory. */
			mz = rte_memzone_lookup(MZ_MLX5_PMD_SHARED_DATA);
			if (mz == NULL) {
				DRV_LOG(ERR,
					"Cannot attach mlx5 shared data\n");
				ret = -rte_errno;
				goto error;
			}
			mlx5_shared_data = mz->addr;
			memset(&mlx5_local_data, 0, sizeof(mlx5_local_data));
		}
	}
error:
	rte_spinlock_unlock(&mlx5_shared_data_lock);
	return ret;
}

/**
 * Retrieve integer value from environment variable.
 *
 * @param[in] name
 *   Environment variable name.
 *
 * @return
 *   Integer value, 0 if the variable is not set.
 */
int
mlx5_getenv_int(const char *name)
{
	const char *val = getenv(name);

	if (val == NULL)
		return 0;
	return atoi(val);
}

/**
 * Verbs callback to allocate a memory. This function should allocate the space
 * according to the size provided residing inside a huge page.
 * Please note that all allocation must respect the alignment from libmlx5
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
mlx5_alloc_verbs_buf(size_t size, void *data)
{
	struct mlx5_priv *priv = data;
	void *ret;
	size_t alignment = sysconf(_SC_PAGESIZE);
	unsigned int socket = SOCKET_ID_ANY;

	if (priv->verbs_alloc_ctx.type == MLX5_VERBS_ALLOC_TYPE_TX_QUEUE) {
		const struct mlx5_txq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	} else if (priv->verbs_alloc_ctx.type ==
		   MLX5_VERBS_ALLOC_TYPE_RX_QUEUE) {
		const struct mlx5_rxq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
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
mlx5_free_verbs_buf(void *ptr, void *data __rte_unused)
{
	assert(data != NULL);
	rte_free(ptr);
}

/**
 * Initialize process private data structure.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_proc_priv_init(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_proc_priv *ppriv;
	size_t ppriv_size;

	/*
	 * UAR register table follows the process private structure. BlueFlame
	 * registers for Tx queues are stored in the table.
	 */
	ppriv_size =
		sizeof(struct mlx5_proc_priv) + priv->txqs_n * sizeof(void *);
	ppriv = rte_malloc_socket("mlx5_proc_priv", ppriv_size,
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
mlx5_proc_priv_uninit(struct rte_eth_dev *dev)
{
	if (!dev->process_private)
		return;
	rte_free(dev->process_private);
	dev->process_private = NULL;
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
mlx5_dev_close(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	DRV_LOG(DEBUG, "port %u closing device \"%s\"",
		dev->data->port_id,
		((priv->sh->ctx != NULL) ? priv->sh->ctx->device->name : ""));
	/* In case mlx5_dev_stop() has not been called. */
	mlx5_dev_interrupt_handler_uninstall(dev);
	mlx5_traffic_disable(dev);
	mlx5_flow_flush(dev, NULL);
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx5_mp_req_stop_rxtx(dev);
	if (priv->rxqs != NULL) {
		/* XXX race condition if mlx5_rx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->rxqs_n); ++i)
			mlx5_rxq_release(dev, i);
		priv->rxqs_n = 0;
		priv->rxqs = NULL;
	}
	if (priv->txqs != NULL) {
		/* XXX race condition if mlx5_tx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->txqs_n); ++i)
			mlx5_txq_release(dev, i);
		priv->txqs_n = 0;
		priv->txqs = NULL;
	}
	mlx5_proc_priv_uninit(dev);
	mlx5_mprq_free_mp(dev);
	/* Remove from memory callback device list. */
	rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
	assert(priv->sh);
	LIST_REMOVE(priv->sh, mem_event_cb);
	rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
	mlx5_free_shared_dr(priv);
	if (priv->rss_conf.rss_key != NULL)
		rte_free(priv->rss_conf.rss_key);
	if (priv->reta_idx != NULL)
		rte_free(priv->reta_idx);
	if (priv->config.vf)
		mlx5_nl_mac_addr_flush(dev);
	if (priv->nl_socket_route >= 0)
		close(priv->nl_socket_route);
	if (priv->nl_socket_rdma >= 0)
		close(priv->nl_socket_rdma);
	if (priv->tcf_context)
		mlx5_flow_tcf_context_destroy(priv->tcf_context);
	if (priv->sh) {
		/*
		 * Free the shared context in last turn, because the cleanup
		 * routines above may use some shared fields, like
		 * mlx5_nl_mac_addr_flush() uses ibdev_path for retrieveing
		 * ifindex if Netlink fails.
		 */
		mlx5_free_shared_ibctx(priv->sh);
		priv->sh = NULL;
	}
	ret = mlx5_hrxq_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some hash Rx queue still remain",
			dev->data->port_id);
	ret = mlx5_ind_table_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some indirection table still remain",
			dev->data->port_id);
	ret = mlx5_rxq_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Verbs Rx queue still remain",
			dev->data->port_id);
	ret = mlx5_rxq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Rx queues still remain",
			dev->data->port_id);
	ret = mlx5_txq_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Verbs Tx queue still remain",
			dev->data->port_id);
	ret = mlx5_txq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Tx queues still remain",
			dev->data->port_id);
	ret = mlx5_flow_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some flows still remain",
			dev->data->port_id);
	if (priv->domain_id != RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		unsigned int c = 0;
		uint16_t port_id;

		RTE_ETH_FOREACH_DEV_OF(port_id, dev->device) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;

			if (!opriv ||
			    opriv->domain_id != priv->domain_id ||
			    &rte_eth_devices[port_id] == dev)
				continue;
			++c;
		}
		if (!c)
			claim_zero(rte_eth_switch_domain_free(priv->domain_id));
	}
	memset(priv, 0, sizeof(*priv));
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	/*
	 * Reset mac_addrs to NULL such that it is not freed as part of
	 * rte_eth_dev_release_port(). mac_addrs is part of dev_private so
	 * it is freed when dev_private is freed.
	 */
	dev->data->mac_addrs = NULL;
}

const struct eth_dev_ops mlx5_dev_ops = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.dev_close = mlx5_dev_close,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.reta_update = mlx5_dev_rss_reta_update,
	.reta_query = mlx5_dev_rss_reta_query,
	.rss_hash_update = mlx5_rss_hash_update,
	.rss_hash_conf_get = mlx5_rss_hash_conf_get,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rx_queue_count = mlx5_rx_queue_count,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
};

/* Available operations from secondary process. */
static const struct eth_dev_ops mlx5_dev_sec_ops = {
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
};

/* Available operations in flow isolated mode. */
const struct eth_dev_ops mlx5_dev_ops_isolate = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.dev_close = mlx5_dev_close,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
};

/**
 * Verify and store value for device argument.
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
mlx5_args_check(const char *key, const char *val, void *opaque)
{
	struct mlx5_dev_config *config = opaque;
	unsigned long tmp;

	/* No-op, port representors are processed in mlx5_dev_spawn(). */
	if (!strcmp(MLX5_REPRESENTOR, key))
		return 0;
	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		DRV_LOG(WARNING, "%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX5_RXQ_CQE_COMP_EN, key) == 0) {
		config->cqe_comp = !!tmp;
	} else if (strcmp(MLX5_RXQ_CQE_PAD_EN, key) == 0) {
		config->cqe_pad = !!tmp;
	} else if (strcmp(MLX5_RXQ_PKT_PAD_EN, key) == 0) {
		config->hw_padding = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_EN, key) == 0) {
		config->mprq.enabled = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_LOG_STRIDE_NUM, key) == 0) {
		config->mprq.stride_num_n = tmp;
	} else if (strcmp(MLX5_RX_MPRQ_MAX_MEMCPY_LEN, key) == 0) {
		config->mprq.max_memcpy_len = tmp;
	} else if (strcmp(MLX5_RXQS_MIN_MPRQ, key) == 0) {
		config->mprq.min_rxqs_num = tmp;
	} else if (strcmp(MLX5_TXQ_INLINE, key) == 0) {
		config->txq_inline = tmp;
	} else if (strcmp(MLX5_TXQS_MIN_INLINE, key) == 0) {
		config->txqs_inline = tmp;
	} else if (strcmp(MLX5_TXQS_MAX_VEC, key) == 0) {
		config->txqs_vec = tmp;
	} else if (strcmp(MLX5_TXQ_MPW_EN, key) == 0) {
		config->mps = !!tmp;
	} else if (strcmp(MLX5_TXQ_MPW_HDR_DSEG_EN, key) == 0) {
		config->mpw_hdr_dseg = !!tmp;
	} else if (strcmp(MLX5_TXQ_MAX_INLINE_LEN, key) == 0) {
		config->inline_max_packet_sz = tmp;
	} else if (strcmp(MLX5_TX_VEC_EN, key) == 0) {
		config->tx_vec_en = !!tmp;
	} else if (strcmp(MLX5_RX_VEC_EN, key) == 0) {
		config->rx_vec_en = !!tmp;
	} else if (strcmp(MLX5_L3_VXLAN_EN, key) == 0) {
		config->l3_vxlan_en = !!tmp;
	} else if (strcmp(MLX5_VF_NL_EN, key) == 0) {
		config->vf_nl_en = !!tmp;
	} else if (strcmp(MLX5_DV_ESW_EN, key) == 0) {
		config->dv_esw_en = !!tmp;
	} else if (strcmp(MLX5_DV_FLOW_EN, key) == 0) {
		config->dv_flow_en = !!tmp;
	} else if (strcmp(MLX5_MR_EXT_MEMSEG_EN, key) == 0) {
		config->mr_ext_memseg_en = !!tmp;
	} else {
		DRV_LOG(WARNING, "%s: unknown parameter", key);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param config
 *   Pointer to device configuration structure.
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_args(struct mlx5_dev_config *config, struct rte_devargs *devargs)
{
	const char **params = (const char *[]){
		MLX5_RXQ_CQE_COMP_EN,
		MLX5_RXQ_CQE_PAD_EN,
		MLX5_RXQ_PKT_PAD_EN,
		MLX5_RX_MPRQ_EN,
		MLX5_RX_MPRQ_LOG_STRIDE_NUM,
		MLX5_RX_MPRQ_MAX_MEMCPY_LEN,
		MLX5_RXQS_MIN_MPRQ,
		MLX5_TXQ_INLINE,
		MLX5_TXQS_MIN_INLINE,
		MLX5_TXQS_MAX_VEC,
		MLX5_TXQ_MPW_EN,
		MLX5_TXQ_MPW_HDR_DSEG_EN,
		MLX5_TXQ_MAX_INLINE_LEN,
		MLX5_TX_VEC_EN,
		MLX5_RX_VEC_EN,
		MLX5_L3_VXLAN_EN,
		MLX5_VF_NL_EN,
		MLX5_DV_ESW_EN,
		MLX5_DV_FLOW_EN,
		MLX5_MR_EXT_MEMSEG_EN,
		MLX5_REPRESENTOR,
		NULL,
	};
	struct rte_kvargs *kvlist;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	/* Following UGLY cast is done to pass checkpatch. */
	kvlist = rte_kvargs_parse(devargs->args, params);
	if (kvlist == NULL)
		return 0;
	/* Process parameters. */
	for (i = 0; (params[i] != NULL); ++i) {
		if (rte_kvargs_count(kvlist, params[i])) {
			ret = rte_kvargs_process(kvlist, params[i],
						 mlx5_args_check, config);
			if (ret) {
				rte_errno = EINVAL;
				rte_kvargs_free(kvlist);
				return -rte_errno;
			}
		}
	}
	rte_kvargs_free(kvlist);
	return 0;
}

static struct rte_pci_driver mlx5_driver;

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
mlx5_init_once(void)
{
	struct mlx5_shared_data *sd;
	struct mlx5_local_data *ld = &mlx5_local_data;

	if (mlx5_init_shared_data())
		return -rte_errno;
	sd = mlx5_shared_data;
	assert(sd);
	rte_spinlock_lock(&sd->lock);
	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		if (sd->init_done)
			break;
		LIST_INIT(&sd->mem_event_cb_list);
		rte_rwlock_init(&sd->mem_event_rwlock);
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_mr_mem_event_cb, NULL);
		mlx5_mp_init_primary();
		sd->init_done = true;
		break;
	case RTE_PROC_SECONDARY:
		if (ld->init_done)
			break;
		mlx5_mp_init_secondary();
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
 * Spawn an Ethernet device from Verbs information.
 *
 * @param dpdk_dev
 *   Backing DPDK device.
 * @param spawn
 *   Verbs device parameters (name, port, switch_info) to spawn.
 * @param config
 *   Device configuration parameters.
 *
 * @return
 *   A valid Ethernet device object on success, NULL otherwise and rte_errno
 *   is set. The following errors are defined:
 *
 *   EBUSY: device is not supposed to be spawned.
 *   EEXIST: device is already spawned
 */
static struct rte_eth_dev *
mlx5_dev_spawn(struct rte_device *dpdk_dev,
	       struct mlx5_dev_spawn_data *spawn,
	       struct mlx5_dev_config config)
{
	const struct mlx5_switch_info *switch_info = &spawn->info;
	struct mlx5_ibv_shared *sh = NULL;
	struct ibv_port_attr port_attr;
	struct mlx5dv_context dv_attr = { .comp_mask = 0 };
	struct rte_eth_dev *eth_dev = NULL;
	struct mlx5_priv *priv = NULL;
	int err = 0;
	unsigned int hw_padding = 0;
	unsigned int mps;
	unsigned int cqe_comp;
	unsigned int cqe_pad = 0;
	unsigned int tunnel_en = 0;
	unsigned int mpls_en = 0;
	unsigned int swp = 0;
	unsigned int mprq = 0;
	unsigned int mprq_min_stride_size_n = 0;
	unsigned int mprq_max_stride_size_n = 0;
	unsigned int mprq_min_stride_num_n = 0;
	unsigned int mprq_max_stride_num_n = 0;
	struct ether_addr mac;
	char name[RTE_ETH_NAME_MAX_LEN];
	int own_domain_id = 0;
	uint16_t port_id;
	unsigned int i;

	/* Determine if this port representor is supposed to be spawned. */
	if (switch_info->representor && dpdk_dev->devargs) {
		struct rte_eth_devargs eth_da;

		err = rte_eth_devargs_parse(dpdk_dev->devargs->args, &eth_da);
		if (err) {
			rte_errno = -err;
			DRV_LOG(ERR, "failed to process device arguments: %s",
				strerror(rte_errno));
			return NULL;
		}
		for (i = 0; i < eth_da.nb_representor_ports; ++i)
			if (eth_da.representor_ports[i] ==
			    (uint16_t)switch_info->port_name)
				break;
		if (i == eth_da.nb_representor_ports) {
			rte_errno = EBUSY;
			return NULL;
		}
	}
	/* Build device name. */
	if (!switch_info->representor)
		strlcpy(name, dpdk_dev->name, sizeof(name));
	else
		snprintf(name, sizeof(name), "%s_representor_%u",
			 dpdk_dev->name, switch_info->port_name);
	/* check if the device is already spawned */
	if (rte_eth_dev_get_port_by_name(name, &port_id) == 0) {
		rte_errno = EEXIST;
		return NULL;
	}
	DRV_LOG(DEBUG, "naming Ethernet device \"%s\"", name);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (eth_dev == NULL) {
			DRV_LOG(ERR, "can not attach rte ethdev");
			rte_errno = ENOMEM;
			return NULL;
		}
		eth_dev->device = dpdk_dev;
		eth_dev->dev_ops = &mlx5_dev_sec_ops;
		err = mlx5_proc_priv_init(eth_dev);
		if (err)
			return NULL;
		/* Receive command fd from primary process */
		err = mlx5_mp_req_verbs_cmd_fd(eth_dev);
		if (err < 0)
			return NULL;
		/* Remap UAR for Tx queues. */
		err = mlx5_tx_uar_init_secondary(eth_dev, err);
		if (err)
			return NULL;
		/*
		 * Ethdev pointer is still required as input since
		 * the primary device is not accessible from the
		 * secondary process.
		 */
		eth_dev->rx_pkt_burst = mlx5_select_rx_function(eth_dev);
		eth_dev->tx_pkt_burst = mlx5_select_tx_function(eth_dev);
		return eth_dev;
	}
	sh = mlx5_alloc_shared_ibctx(spawn);
	if (!sh)
		return NULL;
	config.devx = sh->devx;
#ifdef HAVE_IBV_MLX5_MOD_SWP
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_SWP;
#endif
	/*
	 * Multi-packet send is supported by ConnectX-4 Lx PF as well
	 * as all ConnectX-5 devices.
	 */
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS;
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_STRIDING_RQ;
#endif
	mlx5_glue->dv_query_device(sh->ctx, &dv_attr);
	if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED) {
		if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW) {
			DRV_LOG(DEBUG, "enhanced MPW is supported");
			mps = MLX5_MPW_ENHANCED;
		} else {
			DRV_LOG(DEBUG, "MPW is supported");
			mps = MLX5_MPW;
		}
	} else {
		DRV_LOG(DEBUG, "MPW isn't supported");
		mps = MLX5_MPW_DISABLED;
	}
#ifdef HAVE_IBV_MLX5_MOD_SWP
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_SWP)
		swp = dv_attr.sw_parsing_caps.sw_parsing_offloads;
	DRV_LOG(DEBUG, "SWP support: %u", swp);
#endif
	config.swp = !!swp;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_STRIDING_RQ) {
		struct mlx5dv_striding_rq_caps mprq_caps =
			dv_attr.striding_rq_caps;

		DRV_LOG(DEBUG, "\tmin_single_stride_log_num_of_bytes: %d",
			mprq_caps.min_single_stride_log_num_of_bytes);
		DRV_LOG(DEBUG, "\tmax_single_stride_log_num_of_bytes: %d",
			mprq_caps.max_single_stride_log_num_of_bytes);
		DRV_LOG(DEBUG, "\tmin_single_wqe_log_num_of_strides: %d",
			mprq_caps.min_single_wqe_log_num_of_strides);
		DRV_LOG(DEBUG, "\tmax_single_wqe_log_num_of_strides: %d",
			mprq_caps.max_single_wqe_log_num_of_strides);
		DRV_LOG(DEBUG, "\tsupported_qpts: %d",
			mprq_caps.supported_qpts);
		DRV_LOG(DEBUG, "device supports Multi-Packet RQ");
		mprq = 1;
		mprq_min_stride_size_n =
			mprq_caps.min_single_stride_log_num_of_bytes;
		mprq_max_stride_size_n =
			mprq_caps.max_single_stride_log_num_of_bytes;
		mprq_min_stride_num_n =
			mprq_caps.min_single_wqe_log_num_of_strides;
		mprq_max_stride_num_n =
			mprq_caps.max_single_wqe_log_num_of_strides;
		config.mprq.stride_num_n = RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
						   mprq_min_stride_num_n);
	}
#endif
	if (RTE_CACHE_LINE_SIZE == 128 &&
	    !(dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP))
		cqe_comp = 0;
	else
		cqe_comp = 1;
	config.cqe_comp = cqe_comp;
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	/* Whether device supports 128B Rx CQE padding. */
	cqe_pad = RTE_CACHE_LINE_SIZE == 128 &&
		  (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_PAD);
#endif
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS) {
		tunnel_en = ((dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_VXLAN) &&
			     (dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GRE));
	}
	DRV_LOG(DEBUG, "tunnel offloading is %ssupported",
		tunnel_en ? "" : "not ");
#else
	DRV_LOG(WARNING,
		"tunnel offloading disabled due to old OFED/rdma-core version");
#endif
	config.tunnel_en = tunnel_en;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	mpls_en = ((dv_attr.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_GRE) &&
		   (dv_attr.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_UDP));
	DRV_LOG(DEBUG, "MPLS over GRE/UDP tunnel offloading is %ssupported",
		mpls_en ? "" : "not ");
#else
	DRV_LOG(WARNING, "MPLS over GRE/UDP tunnel offloading disabled due to"
		" old OFED/rdma-core version or firmware configuration");
#endif
	config.mpls_en = mpls_en;
	/* Check port status. */
	err = mlx5_glue->query_port(sh->ctx, spawn->ibv_port, &port_attr);
	if (err) {
		DRV_LOG(ERR, "port query failed: %s", strerror(err));
		goto error;
	}
	if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
		DRV_LOG(ERR, "port is not configured in Ethernet mode");
		err = EINVAL;
		goto error;
	}
	if (port_attr.state != IBV_PORT_ACTIVE)
		DRV_LOG(DEBUG, "port is not active: \"%s\" (%d)",
			mlx5_glue->port_state_str(port_attr.state),
			port_attr.state);
	/* Allocate private eth device data. */
	priv = rte_zmalloc("ethdev private structure",
			   sizeof(*priv),
			   RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		DRV_LOG(ERR, "priv allocation failure");
		err = ENOMEM;
		goto error;
	}
	priv->sh = sh;
	priv->ibv_port = spawn->ibv_port;
	priv->mtu = ETHER_MTU;
#ifndef RTE_ARCH_64
	/* Initialize UAR access locks for 32bit implementations. */
	rte_spinlock_init(&priv->uar_lock_cq);
	for (i = 0; i < MLX5_UAR_PAGE_NUM_MAX; i++)
		rte_spinlock_init(&priv->uar_lock[i]);
#endif
	/* Some internal functions rely on Netlink sockets, open them now. */
	priv->nl_socket_rdma = mlx5_nl_init(NETLINK_RDMA);
	priv->nl_socket_route =	mlx5_nl_init(NETLINK_ROUTE);
	priv->nl_sn = 0;
	priv->representor = !!switch_info->representor;
	priv->master = !!switch_info->master;
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	/*
	 * Currently we support single E-Switch per PF configurations
	 * only and vport_id field contains the vport index for
	 * associated VF, which is deduced from representor port name.
	 * For example, let's have the IB device port 10, it has
	 * attached network device eth0, which has port name attribute
	 * pf0vf2, we can deduce the VF number as 2, and set vport index
	 * as 3 (2+1). This assigning schema should be changed if the
	 * multiple E-Switch instances per PF configurations or/and PCI
	 * subfunctions are added.
	 */
	priv->vport_id = switch_info->representor ?
			 switch_info->port_name + 1 : -1;
	/* representor_id field keeps the unmodified port/VF index. */
	priv->representor_id = switch_info->representor ?
			       switch_info->port_name : -1;
	/*
	 * Look for sibling devices in order to reuse their switch domain
	 * if any, otherwise allocate one.
	 */
	RTE_ETH_FOREACH_DEV_OF(port_id, dpdk_dev) {
		const struct mlx5_priv *opriv =
			rte_eth_devices[port_id].data->dev_private;

		if (!opriv ||
			opriv->domain_id ==
			RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID)
			continue;
		priv->domain_id = opriv->domain_id;
		break;
	}
	if (priv->domain_id == RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		err = rte_eth_switch_domain_alloc(&priv->domain_id);
		if (err) {
			err = rte_errno;
			DRV_LOG(ERR, "unable to allocate switch domain: %s",
				strerror(rte_errno));
			goto error;
		}
		own_domain_id = 1;
	}
	err = mlx5_args(&config, dpdk_dev->devargs);
	if (err) {
		err = rte_errno;
		DRV_LOG(ERR, "failed to process device arguments: %s",
			strerror(rte_errno));
		goto error;
	}
	config.hw_csum = !!(sh->device_attr.device_cap_flags_ex &
			    IBV_DEVICE_RAW_IP_CSUM);
	DRV_LOG(DEBUG, "checksum offloading is %ssupported",
		(config.hw_csum ? "" : "not "));
#if !defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) && \
	!defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	DRV_LOG(DEBUG, "counters are not supported");
#endif
#ifndef HAVE_IBV_FLOW_DV_SUPPORT
	if (config.dv_flow_en) {
		DRV_LOG(WARNING, "DV flow is not supported");
		config.dv_flow_en = 0;
	}
#endif
	config.ind_table_max_size =
		sh->device_attr.rss_caps.max_rwq_indirection_table_size;
	/*
	 * Remove this check once DPDK supports larger/variable
	 * indirection tables.
	 */
	if (config.ind_table_max_size > (unsigned int)ETH_RSS_RETA_SIZE_512)
		config.ind_table_max_size = ETH_RSS_RETA_SIZE_512;
	DRV_LOG(DEBUG, "maximum Rx indirection table size is %u",
		config.ind_table_max_size);
	config.hw_vlan_strip = !!(sh->device_attr.raw_packet_caps &
				  IBV_RAW_PACKET_CAP_CVLAN_STRIPPING);
	DRV_LOG(DEBUG, "VLAN stripping is %ssupported",
		(config.hw_vlan_strip ? "" : "not "));
	config.hw_fcs_strip = !!(sh->device_attr.raw_packet_caps &
				 IBV_RAW_PACKET_CAP_SCATTER_FCS);
	DRV_LOG(DEBUG, "FCS stripping configuration is %ssupported",
		(config.hw_fcs_strip ? "" : "not "));
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
	hw_padding = !!sh->device_attr.rx_pad_end_addr_align;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
	hw_padding = !!(sh->device_attr.device_cap_flags_ex &
			IBV_DEVICE_PCI_WRITE_END_PADDING);
#endif
	if (config.hw_padding && !hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding isn't supported");
		config.hw_padding = 0;
	} else if (config.hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding is enabled");
	}
	config.tso = (sh->device_attr.tso_caps.max_tso > 0 &&
		      (sh->device_attr.tso_caps.supported_qpts &
		       (1 << IBV_QPT_RAW_PACKET)));
	if (config.tso)
		config.tso_max_payload_sz = sh->device_attr.tso_caps.max_tso;
	/*
	 * MPW is disabled by default, while the Enhanced MPW is enabled
	 * by default.
	 */
	if (config.mps == MLX5_ARG_UNSET)
		config.mps = (mps == MLX5_MPW_ENHANCED) ? MLX5_MPW_ENHANCED :
							  MLX5_MPW_DISABLED;
	else
		config.mps = config.mps ? mps : MLX5_MPW_DISABLED;
	DRV_LOG(INFO, "%sMPS is %s",
		config.mps == MLX5_MPW_ENHANCED ? "enhanced " : "",
		config.mps != MLX5_MPW_DISABLED ? "enabled" : "disabled");
	if (config.cqe_comp && !cqe_comp) {
		DRV_LOG(WARNING, "Rx CQE compression isn't supported");
		config.cqe_comp = 0;
	}
	if (config.cqe_pad && !cqe_pad) {
		DRV_LOG(WARNING, "Rx CQE padding isn't supported");
		config.cqe_pad = 0;
	} else if (config.cqe_pad) {
		DRV_LOG(INFO, "Rx CQE padding is enabled");
	}
	if (config.mprq.enabled && mprq) {
		if (config.mprq.stride_num_n > mprq_max_stride_num_n ||
		    config.mprq.stride_num_n < mprq_min_stride_num_n) {
			config.mprq.stride_num_n =
				RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
					mprq_min_stride_num_n);
			DRV_LOG(WARNING,
				"the number of strides"
				" for Multi-Packet RQ is out of range,"
				" setting default value (%u)",
				1 << config.mprq.stride_num_n);
		}
		config.mprq.min_stride_size_n = mprq_min_stride_size_n;
		config.mprq.max_stride_size_n = mprq_max_stride_size_n;
	} else if (config.mprq.enabled && !mprq) {
		DRV_LOG(WARNING, "Multi-Packet RQ isn't supported");
		config.mprq.enabled = 0;
	}
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL) {
		DRV_LOG(ERR, "can not allocate rte ethdev");
		err = ENOMEM;
		goto error;
	}
	/* Flag to call rte_eth_dev_release_port() in rte_eth_dev_close(). */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;
	if (priv->representor) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
		eth_dev->data->representor_id = priv->representor_id;
	}
	eth_dev->data->dev_private = priv;
	priv->dev_data = eth_dev->data;
	eth_dev->data->mac_addrs = priv->mac;
	eth_dev->device = dpdk_dev;
	/* Configure the first MAC address by default. */
	if (mlx5_get_mac(eth_dev, &mac.addr_bytes)) {
		DRV_LOG(ERR,
			"port %u cannot get MAC address, is mlx5_en"
			" loaded? (errno: %s)",
			eth_dev->data->port_id, strerror(rte_errno));
		err = ENODEV;
		goto error;
	}
	DRV_LOG(INFO,
		"port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		eth_dev->data->port_id,
		mac.addr_bytes[0], mac.addr_bytes[1],
		mac.addr_bytes[2], mac.addr_bytes[3],
		mac.addr_bytes[4], mac.addr_bytes[5]);
#ifndef NDEBUG
	{
		char ifname[IF_NAMESIZE];

		if (mlx5_get_ifname(eth_dev, &ifname) == 0)
			DRV_LOG(DEBUG, "port %u ifname is \"%s\"",
				eth_dev->data->port_id, ifname);
		else
			DRV_LOG(DEBUG, "port %u ifname is unknown",
				eth_dev->data->port_id);
	}
#endif
	/* Get actual MTU if possible. */
	err = mlx5_get_mtu(eth_dev, &priv->mtu);
	if (err) {
		err = rte_errno;
		goto error;
	}
	DRV_LOG(DEBUG, "port %u MTU is %u", eth_dev->data->port_id,
		priv->mtu);
	/* Initialize burst functions to prevent crashes before link-up. */
	eth_dev->rx_pkt_burst = removed_rx_burst;
	eth_dev->tx_pkt_burst = removed_tx_burst;
	eth_dev->dev_ops = &mlx5_dev_ops;
	/* Register MAC address. */
	claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
	if (config.vf && config.vf_nl_en)
		mlx5_nl_mac_addr_sync(eth_dev);
	priv->tcf_context = mlx5_flow_tcf_context_create();
	if (!priv->tcf_context) {
		err = -rte_errno;
		DRV_LOG(WARNING,
			"flow rules relying on switch offloads will not be"
			" supported: cannot open libmnl socket: %s",
			strerror(rte_errno));
	} else {
		struct rte_flow_error error;
		unsigned int ifindex = mlx5_ifindex(eth_dev);

		if (!ifindex) {
			err = -rte_errno;
			error.message =
				"cannot retrieve network interface index";
		} else {
			err = mlx5_flow_tcf_init(priv->tcf_context,
						 ifindex, &error);
		}
		if (err) {
			DRV_LOG(WARNING,
				"flow rules relying on switch offloads will"
				" not be supported: %s: %s",
				error.message, strerror(rte_errno));
			mlx5_flow_tcf_context_destroy(priv->tcf_context);
			priv->tcf_context = NULL;
		}
	}
	TAILQ_INIT(&priv->flows);
	TAILQ_INIT(&priv->ctrl_flows);
	/* Hint libmlx5 to use PMD allocator for data plane resources */
	struct mlx5dv_ctx_allocators alctr = {
		.alloc = &mlx5_alloc_verbs_buf,
		.free = &mlx5_free_verbs_buf,
		.data = priv,
	};
	mlx5_glue->dv_set_context_attr(sh->ctx,
				       MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
				       (void *)((uintptr_t)&alctr));
	/* Bring Ethernet device up. */
	DRV_LOG(DEBUG, "port %u forcing Ethernet interface up",
		eth_dev->data->port_id);
	mlx5_set_link_up(eth_dev);
	/*
	 * Even though the interrupt handler is not installed yet,
	 * interrupts will still trigger on the async_fd from
	 * Verbs context returned by ibv_open_device().
	 */
	mlx5_link_update(eth_dev, 0);
#ifdef HAVE_IBV_DEVX_OBJ
	if (config.devx) {
		err = mlx5_devx_cmd_query_hca_attr(sh->ctx, &config.hca_attr);
		if (err) {
			err = -err;
			goto error;
		}
	}
#endif
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (!(config.hca_attr.eswitch_manager && config.dv_flow_en &&
	      (switch_info->representor || switch_info->master)))
		config.dv_esw_en = 0;
#else
	config.dv_esw_en = 0;
#endif
	/* Store device configuration on private structure. */
	priv->config = config;
	if (config.dv_flow_en) {
		err = mlx5_alloc_shared_dr(priv);
		if (err)
			goto error;
	}
	/* Supported Verbs flow priority number detection. */
	err = mlx5_flow_discover_priorities(eth_dev);
	if (err < 0) {
		err = -err;
		goto error;
	}
	priv->config.flow_prio = err;
	/* Add device to memory callback list. */
	rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
	LIST_INSERT_HEAD(&mlx5_shared_data->mem_event_cb_list,
			 sh, mem_event_cb);
	rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
	return eth_dev;
error:
	if (priv) {
		if (priv->sh)
			mlx5_free_shared_dr(priv);
		if (priv->nl_socket_route >= 0)
			close(priv->nl_socket_route);
		if (priv->nl_socket_rdma >= 0)
			close(priv->nl_socket_rdma);
		if (priv->tcf_context)
			mlx5_flow_tcf_context_destroy(priv->tcf_context);
		if (own_domain_id)
			claim_zero(rte_eth_switch_domain_free(priv->domain_id));
		rte_free(priv);
		if (eth_dev != NULL)
			eth_dev->data->dev_private = NULL;
	}
	if (eth_dev != NULL) {
		/* mac_addrs must not be freed alone because part of dev_private */
		eth_dev->data->mac_addrs = NULL;
		rte_eth_dev_release_port(eth_dev);
	}
	if (sh)
		mlx5_free_shared_ibctx(sh);
	assert(err > 0);
	rte_errno = err;
	return NULL;
}

/**
 * Comparison callback to sort device data.
 *
 * This is meant to be used with qsort().
 *
 * @param a[in]
 *   Pointer to pointer to first data object.
 * @param b[in]
 *   Pointer to pointer to second data object.
 *
 * @return
 *   0 if both objects are equal, less than 0 if the first argument is less
 *   than the second, greater than 0 otherwise.
 */
static int
mlx5_dev_spawn_data_cmp(const void *a, const void *b)
{
	const struct mlx5_switch_info *si_a =
		&((const struct mlx5_dev_spawn_data *)a)->info;
	const struct mlx5_switch_info *si_b =
		&((const struct mlx5_dev_spawn_data *)b)->info;
	int ret;

	/* Master device first. */
	ret = si_b->master - si_a->master;
	if (ret)
		return ret;
	/* Then representor devices. */
	ret = si_b->representor - si_a->representor;
	if (ret)
		return ret;
	/* Unidentified devices come last in no specific order. */
	if (!si_a->representor)
		return 0;
	/* Order representors by name. */
	return si_a->port_name - si_b->port_name;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns Ethernet devices out of a given PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx5_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct ibv_device **ibv_list;
	/*
	 * Number of found IB Devices matching with requested PCI BDF.
	 * nd != 1 means there are multiple IB devices over the same
	 * PCI device and we have representors and master.
	 */
	unsigned int nd = 0;
	/*
	 * Number of found IB device Ports. nd = 1 and np = 1..n means
	 * we have the single multiport IB device, and there may be
	 * representors attached to some of found ports.
	 */
	unsigned int np = 0;
	/*
	 * Number of DPDK ethernet devices to Spawn - either over
	 * multiple IB devices or multiple ports of single IB device.
	 * Actually this is the number of iterations to spawn.
	 */
	unsigned int ns = 0;
	struct mlx5_dev_config dev_config;
	int ret;

	ret = mlx5_init_once();
	if (ret) {
		DRV_LOG(ERR, "unable to init PMD global data: %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	assert(pci_drv == &mlx5_driver);
	errno = 0;
	ibv_list = mlx5_glue->get_device_list(&ret);
	if (!ibv_list) {
		rte_errno = errno ? errno : ENOSYS;
		DRV_LOG(ERR, "cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	/*
	 * First scan the list of all Infiniband devices to find
	 * matching ones, gathering into the list.
	 */
	struct ibv_device *ibv_match[ret + 1];
	int nl_route = -1;
	int nl_rdma = -1;
	unsigned int i;

	while (ret-- > 0) {
		struct rte_pci_addr pci_addr;

		DRV_LOG(DEBUG, "checking device \"%s\"", ibv_list[ret]->name);
		if (mlx5_ibv_device_to_pci_addr(ibv_list[ret], &pci_addr))
			continue;
		if (pci_dev->addr.domain != pci_addr.domain ||
		    pci_dev->addr.bus != pci_addr.bus ||
		    pci_dev->addr.devid != pci_addr.devid ||
		    pci_dev->addr.function != pci_addr.function)
			continue;
		DRV_LOG(INFO, "PCI information matches for device \"%s\"",
			ibv_list[ret]->name);
		ibv_match[nd++] = ibv_list[ret];
	}
	ibv_match[nd] = NULL;
	if (!nd) {
		/* No device matches, just complain and bail out. */
		mlx5_glue->free_device_list(ibv_list);
		DRV_LOG(WARNING,
			"no Verbs device matches PCI device " PCI_PRI_FMT ","
			" are kernel drivers loaded?",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
		rte_errno = ENOENT;
		ret = -rte_errno;
		return ret;
	}
	nl_route = mlx5_nl_init(NETLINK_ROUTE);
	nl_rdma = mlx5_nl_init(NETLINK_RDMA);
	if (nd == 1) {
		/*
		 * Found single matching device may have multiple ports.
		 * Each port may be representor, we have to check the port
		 * number and check the representors existence.
		 */
		if (nl_rdma >= 0)
			np = mlx5_nl_portnum(nl_rdma, ibv_match[0]->name);
		if (!np)
			DRV_LOG(WARNING, "can not get IB device \"%s\""
					 " ports number", ibv_match[0]->name);
	}
	/*
	 * Now we can determine the maximal
	 * amount of devices to be spawned.
	 */
	struct mlx5_dev_spawn_data list[np ? np : nd];

	if (np > 1) {
		/*
		 * Single IB device with multiple ports found,
		 * it may be E-Switch master device and representors.
		 * We have to perform identification trough the ports.
		 */
		assert(nl_rdma >= 0);
		assert(ns == 0);
		assert(nd == 1);
		for (i = 1; i <= np; ++i) {
			list[ns].max_port = np;
			list[ns].ibv_port = i;
			list[ns].ibv_dev = ibv_match[0];
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].ifindex = mlx5_nl_ifindex
					(nl_rdma, list[ns].ibv_dev->name, i);
			if (!list[ns].ifindex) {
				/*
				 * No network interface index found for the
				 * specified port, it means there is no
				 * representor on this port. It's OK,
				 * there can be disabled ports, for example
				 * if sriov_numvfs < sriov_totalvfs.
				 */
				continue;
			}
			ret = -1;
			if (nl_route >= 0)
				ret = mlx5_nl_switch_info
					       (nl_route,
						list[ns].ifindex,
						&list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret =  mlx5_sysfs_switch_info
						(list[ns].ifindex,
						 &list[ns].info);
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master))
				ns++;
		}
		if (!ns) {
			DRV_LOG(ERR,
				"unable to recognize master/representors"
				" on the IB device with multiple ports");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	} else {
		/*
		 * The existence of several matching entries (nd > 1) means
		 * port representors have been instantiated. No existing Verbs
		 * call nor sysfs entries can tell them apart, this can only
		 * be done through Netlink calls assuming kernel drivers are
		 * recent enough to support them.
		 *
		 * In the event of identification failure through Netlink,
		 * try again through sysfs, then:
		 *
		 * 1. A single IB device matches (nd == 1) with single
		 *    port (np=0/1) and is not a representor, assume
		 *    no switch support.
		 *
		 * 2. Otherwise no safe assumptions can be made;
		 *    complain louder and bail out.
		 */
		np = 1;
		for (i = 0; i != nd; ++i) {
			memset(&list[ns].info, 0, sizeof(list[ns].info));
			list[ns].max_port = 1;
			list[ns].ibv_port = 1;
			list[ns].ibv_dev = ibv_match[i];
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].ifindex = 0;
			if (nl_rdma >= 0)
				list[ns].ifindex = mlx5_nl_ifindex
					(nl_rdma, list[ns].ibv_dev->name, 1);
			if (!list[ns].ifindex) {
				char ifname[IF_NAMESIZE];

				/*
				 * Netlink failed, it may happen with old
				 * ib_core kernel driver (before 4.16).
				 * We can assume there is old driver because
				 * here we are processing single ports IB
				 * devices. Let's try sysfs to retrieve
				 * the ifindex. The method works for
				 * master device only.
				 */
				if (nd > 1) {
					/*
					 * Multiple devices found, assume
					 * representors, can not distinguish
					 * master/representor and retrieve
					 * ifindex via sysfs.
					 */
					continue;
				}
				ret = mlx5_get_master_ifname
					(ibv_match[i]->ibdev_path, &ifname);
				if (!ret)
					list[ns].ifindex =
						if_nametoindex(ifname);
				if (!list[ns].ifindex) {
					/*
					 * No network interface index found
					 * for the specified device, it means
					 * there it is neither representor
					 * nor master.
					 */
					continue;
				}
			}
			ret = -1;
			if (nl_route >= 0)
				ret = mlx5_nl_switch_info
					       (nl_route,
						list[ns].ifindex,
						&list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret =  mlx5_sysfs_switch_info
						(list[ns].ifindex,
						 &list[ns].info);
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master)) {
				ns++;
			} else if ((nd == 1) &&
				   !list[ns].info.representor &&
				   !list[ns].info.master) {
				/*
				 * Single IB device with
				 * one physical port and
				 * attached network device.
				 * May be SRIOV is not enabled
				 * or there is no representors.
				 */
				DRV_LOG(INFO, "no E-Switch support detected");
				ns++;
				break;
			}
		}
		if (!ns) {
			DRV_LOG(ERR,
				"unable to recognize master/representors"
				" on the multiple IB devices");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	}
	assert(ns);
	/*
	 * Sort list to probe devices in natural order for users convenience
	 * (i.e. master first, then representors from lowest to highest ID).
	 */
	qsort(list, ns, sizeof(*list), mlx5_dev_spawn_data_cmp);
	/* Default configuration. */
	dev_config = (struct mlx5_dev_config){
		.hw_padding = 0,
		.mps = MLX5_ARG_UNSET,
		.tx_vec_en = 1,
		.rx_vec_en = 1,
		.txq_inline = MLX5_ARG_UNSET,
		.txqs_inline = MLX5_ARG_UNSET,
		.txqs_vec = MLX5_ARG_UNSET,
		.inline_max_packet_sz = MLX5_ARG_UNSET,
		.vf_nl_en = 1,
		.mr_ext_memseg_en = 1,
		.mprq = {
			.enabled = 0, /* Disabled by default. */
			.stride_num_n = MLX5_MPRQ_STRIDE_NUM_N,
			.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN,
			.min_rxqs_num = MLX5_MPRQ_MIN_RXQS,
		},
		.dv_esw_en = 1,
	};
	/* Device specific configuration. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5BF:
		dev_config.txqs_vec = MLX5_VPMD_MAX_TXQS_BLUEFIELD;
		break;
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF:
		dev_config.vf = 1;
		break;
	default:
		break;
	}
	/* Set architecture-dependent default value if unset. */
	if (dev_config.txqs_vec == MLX5_ARG_UNSET)
		dev_config.txqs_vec = MLX5_VPMD_MAX_TXQS;
	for (i = 0; i != ns; ++i) {
		uint32_t restore;

		list[i].eth_dev = mlx5_dev_spawn(&pci_dev->device,
						 &list[i],
						 dev_config);
		if (!list[i].eth_dev) {
			if (rte_errno != EBUSY && rte_errno != EEXIST)
				break;
			/* Device is disabled or already spawned. Ignore it. */
			continue;
		}
		restore = list[i].eth_dev->data->dev_flags;
		rte_eth_copy_pci_info(list[i].eth_dev, pci_dev);
		/* Restore non-PCI flags cleared by the above call. */
		list[i].eth_dev->data->dev_flags |= restore;
		rte_eth_dev_probing_finish(list[i].eth_dev);
	}
	if (i != ns) {
		DRV_LOG(ERR,
			"probe of PCI device " PCI_PRI_FMT " aborted after"
			" encountering an error: %s",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function,
			strerror(rte_errno));
		ret = -rte_errno;
		/* Roll back. */
		while (i--) {
			if (!list[i].eth_dev)
				continue;
			mlx5_dev_close(list[i].eth_dev);
			/* mac_addrs must not be freed because in dev_private */
			list[i].eth_dev->data->mac_addrs = NULL;
			claim_zero(rte_eth_dev_release_port(list[i].eth_dev));
		}
		/* Restore original error. */
		rte_errno = -ret;
	} else {
		ret = 0;
	}
exit:
	/*
	 * Do the routine cleanup:
	 * - close opened Netlink sockets
	 * - free the Infiniband device list
	 */
	if (nl_rdma >= 0)
		close(nl_rdma);
	if (nl_route >= 0)
		close(nl_route);
	assert(ibv_list);
	mlx5_glue->free_device_list(ibv_list);
	return ret;
}

/**
 * DPDK callback to remove a PCI device.
 *
 * This function removes all Ethernet devices belong to a given PCI device.
 *
 * @param[in] pci_dev
 *   Pointer to the PCI device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
static int
mlx5_pci_remove(struct rte_pci_device *pci_dev)
{
	uint16_t port_id;

	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		rte_eth_dev_close(port_id);
	return 0;
}

static const struct rte_pci_id mlx5_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4LX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5EX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6VF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx5_driver = {
	.driver = {
		.name = MLX5_DRIVER_NAME
	},
	.id_table = mlx5_pci_id_map,
	.probe = mlx5_pci_probe,
	.remove = mlx5_pci_remove,
	.dma_map = mlx5_dma_map,
	.dma_unmap = mlx5_dma_unmap,
	.drv_flags = (RTE_PCI_DRV_INTR_LSC | RTE_PCI_DRV_INTR_RMV |
		      RTE_PCI_DRV_PROBE_AGAIN),
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
mlx5_glue_path(char *buf, size_t size)
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
	DRV_LOG(ERR,
		"unable to append \"-glue\" to last component of"
		" RTE_EAL_PMD_PATH (\"" RTE_EAL_PMD_PATH "\"),"
		" please re-configure DPDK");
	return NULL;
}

/**
 * Initialization routine for run-time dependency on rdma-core.
 */
static int
mlx5_glue_init(void)
{
	char glue_path[sizeof(RTE_EAL_PMD_PATH) - 1 + sizeof("-glue")];
	const char *path[] = {
		/*
		 * A basic security check is necessary before trusting
		 * MLX5_GLUE_PATH, which may override RTE_EAL_PMD_PATH.
		 */
		(geteuid() == getuid() && getegid() == getgid() ?
		 getenv("MLX5_GLUE_PATH") : NULL),
		/*
		 * When RTE_EAL_PMD_PATH is set, use its glue-suffixed
		 * variant, otherwise let dlopen() look up libraries on its
		 * own.
		 */
		(*RTE_EAL_PMD_PATH ?
		 mlx5_glue_path(glue_path, sizeof(glue_path)) : ""),
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

			ret = snprintf(name, sizeof(name), "%.*s%s" MLX5_GLUE,
				       (int)len, path[i],
				       (!len || *(end - 1) == '/') ? "" : "/");
			if (ret == -1)
				break;
			if (sizeof(name) != (size_t)ret + 1)
				continue;
			DRV_LOG(DEBUG, "looking for rdma-core glue as \"%s\"",
				name);
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
			DRV_LOG(WARNING, "cannot load glue library: %s", dlmsg);
		goto glue_error;
	}
	sym = dlsym(handle, "mlx5_glue");
	if (!sym || !*sym) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			DRV_LOG(ERR, "cannot resolve glue symbol: %s", dlmsg);
		goto glue_error;
	}
	mlx5_glue = *sym;
	return 0;
glue_error:
	if (handle)
		dlclose(handle);
	DRV_LOG(WARNING,
		"cannot initialize PMD due to missing run-time dependency on"
		" rdma-core libraries (libibverbs, libmlx5)");
	return -rte_errno;
}

#endif

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_pmd_init)
{
	/* Initialize driver log type. */
	mlx5_logtype = rte_log_register("pmd.net.mlx5");
	if (mlx5_logtype >= 0)
		rte_log_set_level(mlx5_logtype, RTE_LOG_NOTICE);

	/* Build the static tables for Verbs conversion. */
	mlx5_set_ptype_table();
	mlx5_set_cksum_table();
	mlx5_set_swp_types_table();
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
	/* Match the size of Rx completion entry to the size of a cacheline. */
	if (RTE_CACHE_LINE_SIZE == 128)
		setenv("MLX5_CQE_SIZE", "128", 0);
	/*
	 * MLX5_DEVICE_FATAL_CLEANUP tells ibv_destroy functions to
	 * cleanup all the Verbs resources even when the device was removed.
	 */
	setenv("MLX5_DEVICE_FATAL_CLEANUP", "1", 1);
#ifdef RTE_IBVERBS_LINK_DLOPEN
	if (mlx5_glue_init())
		return;
	assert(mlx5_glue);
#endif
#ifndef NDEBUG
	/* Glue structure must not contain any NULL pointers. */
	{
		unsigned int i;

		for (i = 0; i != sizeof(*mlx5_glue) / sizeof(void *); ++i)
			assert(((const void *const *)mlx5_glue)[i]);
	}
#endif
	if (strcmp(mlx5_glue->version, MLX5_GLUE_VERSION)) {
		DRV_LOG(ERR,
			"rdma-core glue \"%s\" mismatch: \"%s\" is required",
			mlx5_glue->version, MLX5_GLUE_VERSION);
		return;
	}
	mlx5_glue->fork_init();
	rte_pci_register(&mlx5_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx5, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx5, mlx5_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx5, "* ib_uverbs & mlx5_core & mlx5_ib");
