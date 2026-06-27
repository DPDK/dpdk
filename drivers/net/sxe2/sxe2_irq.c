/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <ethdev_pci.h>
#include <ethdev_driver.h>
#include <rte_alarm.h>
#include <fcntl.h>
#include <rte_stdatomic.h>

#include "sxe2_ethdev.h"
#include "sxe2_irq.h"
#include "sxe2_common_log.h"
#include "sxe2_queue.h"
#include "sxe2_drv_cmd.h"
#include "sxe2_ioctl_chnl_func.h"
#include "sxe2vf_regs.h"
#include "sxe2_host_regs.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_switchdev.h"

#define SXE2_INT_EVENT_OICR_ALL (SXE2_PF_INT_OICR_SWINT | \
					SXE2_PF_INT_OICR_LAN_TX_ERR | \
					SXE2_PF_INT_OICR_LAN_RX_ERR | \
					SXE2_PF_INT_OICR_FW)

#define MAX_EVENT_PENDING (16)

struct sxe2_event_element {
	TAILQ_ENTRY(sxe2_event_element) next;
	struct rte_eth_dev *dev;
};

struct sxe2_event_handler {
	RTE_ATOMIC(uint16_t)ndev;
	rte_thread_t tid;
	int32_t fd[2];
	rte_spinlock_t  lock;
	TAILQ_HEAD(event_list, sxe2_event_element) pending;
};
static struct sxe2_event_handler event_handler = {
	.fd = {-1, -1},
};

static RTE_ATOMIC(uint32_t)event_thread_run;


static void sxe2_event_irq_common_handler(struct sxe2_adapter *adapter, uint64_t oicr)
{
	struct rte_eth_dev *dev = &rte_eth_devices[adapter->dev_info.dev_data->port_id];

	if (oicr & RTE_BIT32(SXE2_COM_EC_LINK_CHG)) {
		PMD_DEV_LOG_INFO(adapter, DRV, "OICR=%" PRIu64, oicr);
		(void)sxe2_drv_mac_link_status_get(adapter);
		if (rte_eal_process_type() == RTE_PROC_PRIMARY)
			rte_eth_dev_callback_process(dev,
						     RTE_ETH_EVENT_INTR_LSC,
						     NULL);
	}
	if (oicr & RTE_BIT32(SXE2_COM_SW_MODE_SWITCHDEV)) {
		PMD_DEV_LOG_INFO(adapter, DRV, "event notify switchdev");
		(void)sxe2_switchdev_notify_callback(adapter, true);
	}
	if (oicr & RTE_BIT32(SXE2_COM_SW_MODE_LEGACY)) {
		PMD_DEV_LOG_INFO(adapter, DRV, "event notify legacy");
		(void)sxe2_switchdev_notify_callback(adapter, false);
	}
}

static uint32_t sxe2_event_intr_handle(void *param __rte_unused)
{
	struct sxe2_event_handler *handler = &event_handler;
	struct sxe2_adapter *adapter;
	struct sxe2_event_element *pos;
	struct sxe2_event_element *tmp;
	int32_t ret = 0;
	uint64_t oicr = 0;
	TAILQ_HEAD(event_list, sxe2_event_element) pending;
	int8_t unused[MAX_EVENT_PENDING];
	ssize_t nr;

	while (rte_atomic_load_explicit(&event_thread_run, rte_memory_order_relaxed)) {
		nr = read(handler->fd[0], &unused, sizeof(unused));
		if (nr <= 0)
			break;

		rte_spinlock_lock(&handler->lock);
		TAILQ_INIT(&pending);
		TAILQ_CONCAT(&pending, &handler->pending, next);
		rte_spinlock_unlock(&handler->lock);

		TAILQ_FOREACH_SAFE(pos, &pending, next, tmp) {
			TAILQ_REMOVE(&pending, pos, next);
			adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(pos->dev);

			ret = sxe2_drv_dev_other_irq_get(adapter->cdev, &oicr);
			sxe2_event_irq_common_handler(adapter, oicr);

			rte_free(pos);
		}
	}

	return ret;
}

static int32_t sxe2_event_intr_handler_init(void)
{
	struct sxe2_event_handler *handler = &event_handler;
	int32_t ret = 0;
	int err = 0;

	PMD_INIT_FUNC_TRACE();
	if (rte_atomic_fetch_add_explicit(&handler->ndev, 1, rte_memory_order_acq_rel) >= 1) {
		ret = 0;
		PMD_LOG_DEBUG(INIT, "%s: ndev > 1", __func__);
		goto l_end;
	}

#if defined(RTE_EXEC_ENV_IS_WINDOWS) && RTE_EXEC_ENV_IS_WINDOWS != 0
	err = _pipe(handler->fd, MAX_EVENT_PENDING, O_BINARY);
#else
	err = pipe(handler->fd);
#endif
	if (err != 0) {
		ret = -ECHILD;
		rte_atomic_fetch_sub_explicit(&handler->ndev, 1, rte_memory_order_release);
		PMD_LOG_ERR(INIT, "%s: pipe failed", __func__);
		goto l_end;
	}

	rte_atomic_store_explicit(&event_thread_run, 1, rte_memory_order_relaxed);

	TAILQ_INIT(&handler->pending);
	rte_spinlock_init(&handler->lock);

	if (rte_thread_create_internal_control(&handler->tid, "sxe2-event",
				sxe2_event_intr_handle, NULL)) {
		PMD_LOG_ERR(INIT, "%s: thread create failed", __func__);
		rte_atomic_fetch_sub_explicit(&handler->ndev, 1, rte_memory_order_release);
		ret = -ECHILD;
		goto l_end;
	}
	ret = 0;
l_end:
	return ret;
}

static void sxe2_event_intr_handler_uinit(void)
{
	struct sxe2_event_handler *handler = &event_handler;
	struct sxe2_event_element *pos;
	struct sxe2_event_element *tmp;
	ssize_t nw = 0;
	int8_t notify_byte = 0;

	PMD_INIT_FUNC_TRACE();
	if (rte_atomic_fetch_sub_explicit(&handler->ndev, 1, rte_memory_order_acq_rel) > 1) {
		PMD_LOG_DEBUG(INIT, "event handler uinit, ndev > 0");
		return;
	}

	rte_atomic_store_explicit(&event_thread_run, 0, rte_memory_order_relaxed);
	nw = write(handler->fd[1], &notify_byte, 1);
	RTE_SET_USED(nw);

	(void)rte_thread_join(handler->tid, NULL);

	if (handler->fd[0] != -1) {
		close(handler->fd[0]);
		handler->fd[0] = -1;
	}
	if (handler->fd[1] != -1) {
		close(handler->fd[1]);
		handler->fd[1] = -1;
	}

	rte_spinlock_lock(&handler->lock);
	TAILQ_FOREACH_SAFE(pos, &handler->pending, next, tmp) {
		TAILQ_REMOVE(&handler->pending, pos, next);
		rte_free(pos);
	}
	rte_spinlock_unlock(&handler->lock);
}

static void sxe2_event_intr_post(struct rte_eth_dev *dev)
{
	struct sxe2_event_handler *handler = &event_handler;
	struct sxe2_event_element *elem = rte_malloc(NULL, sizeof(struct sxe2_event_element), 0);
	int8_t notify_byte = 0;
	ssize_t nw = 0;

	if (!elem)
		goto l_end;

	elem->dev = dev;

	rte_spinlock_lock(&handler->lock);
	TAILQ_INSERT_TAIL(&handler->pending, elem, next);
	rte_spinlock_unlock(&handler->lock);

	nw = write(handler->fd[1], &notify_byte, 1);
	RTE_SET_USED(nw);

l_end:
	return;
}

static void sxe2_interrupt_handler_other(void *arg)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)arg;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t eventfd = adapter->irq_ctxt.other_event_fd;
	int32_t ret = 0;
	uint64_t buf = 0;

	ret = read(eventfd, &buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "read eventfd[%d] failed, ret:%d, errno:%d.",
				eventfd, ret, errno);
	}

	sxe2_event_intr_post(dev);
}

static void sxe2_interrupt_handler_reset(void *arg)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)arg;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t resetfd = adapter->irq_ctxt.reset_event_fd;
	int32_t ret = 0;
	uint64_t buf = 0;

	ret = read(resetfd, &buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "read resetfd[%d] failed, ret:%d, errno:%d.",
				resetfd, ret, errno);
	}

	sxe2_drv_cmd_close(adapter->cdev);

	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RMV, NULL);
}

int32_t sxe2_sw_irq_ctxt_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	int32_t ret = 0;

	irq_ctxt->rxq_avail_cnt = irq_ctxt->max_cnt_hw;
	irq_ctxt->rxq_base_idx_in_pf = irq_ctxt->base_idx_in_func;
	irq_ctxt->reset_event_fd = -1;
	irq_ctxt->other_event_fd = -1;

	return ret;
}

void sxe2_sw_irq_ctxt_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	memset(&adapter->irq_ctxt, 0, sizeof(adapter->irq_ctxt));
	adapter->irq_ctxt.reset_event_fd = -1;
	adapter->irq_ctxt.other_event_fd = -1;
}

void sxe2_sw_irq_ctx_hw_cap_set(struct sxe2_adapter *adapter,
				struct sxe2_drv_msix_caps *msix_caps)
{
	adapter->irq_ctxt.max_cnt_hw = msix_caps->msix_vectors_cnt;
	adapter->irq_ctxt.base_idx_in_func = msix_caps->base_idx_in_func;
}

static int32_t sxe2_intr_handler_cfg(struct rte_intr_handle *handle,
		int32_t fd, rte_intr_callback_fn cb, void *cb_arg)
{
	int32_t ret = 0;

	ret = rte_intr_fd_set(handle, fd);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to set intr_handle->fd, error %i (%s)",
			errno, strerror(errno));
		goto err;
	}

	ret = rte_intr_type_set(handle, RTE_INTR_HANDLE_EXT);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to set intr_handle->type, error %i (%s)",
			errno, strerror(errno));
		goto err;
	}

	ret = rte_intr_callback_register(handle, cb, cb_arg);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize register intr callback, ret=%d", ret);
		goto err;
	}
err:
	return ret;
}

static struct rte_intr_handle *
sxe2_intr_handler_create(int32_t fd, rte_intr_callback_fn cb, void *cb_arg)
{
	struct rte_intr_handle *tmp_intr_handle = NULL;
	int32_t ret = 0;

	tmp_intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (!tmp_intr_handle) {
		PMD_LOG_ERR(INIT, "Failed to alloc memory for intr_handler, error %i (%s)",
			errno, strerror(errno));
		goto err;
	}

	ret = rte_intr_fd_set(tmp_intr_handle, fd);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to set intr_handle->fd, error %i (%s)",
				errno, strerror(errno));
		goto err;
	}

	ret = rte_intr_type_set(tmp_intr_handle, RTE_INTR_HANDLE_EXT);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to set intr_handle->type, error %i (%s)",
				errno, strerror(errno));
		goto err;
	}

	ret = rte_intr_callback_register(tmp_intr_handle, cb, cb_arg);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to initialize register intr callback, ret=%d", ret);
		goto err;
	}

	return tmp_intr_handle;
err:
	rte_intr_instance_free(tmp_intr_handle);
	return NULL;
}

static void sxe2_intr_handler_destroy(struct rte_intr_handle *intr_handle,
				  rte_intr_callback_fn cb, void *cb_arg)
{
	if (!intr_handle)
		return;

	if (rte_intr_fd_get(intr_handle) >= 0)
		(void)rte_intr_callback_unregister(intr_handle, cb, cb_arg);
	rte_intr_instance_free(intr_handle);
}

static int32_t sxe2_event_intr_fd_create(void)
{
	int32_t fd = 0;

	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd < 0) {
		PMD_LOG_ERR(INIT, "Can't setup eventfd, error %i (%s)",
			errno, strerror(errno));
		goto err;
	}

	return fd;
err:
	return -EBADF;
}

static void sxe2_event_intr_fd_destroy(int32_t fd)
{
	if (fd >= 0)
		close(fd);
}

static int32_t sxe2_other_intr_register(struct rte_eth_dev *dev, int32_t fd)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	uint64_t event = RTE_BIT32(SXE2_COM_EC_LINK_CHG) |
				RTE_BIT32(SXE2_COM_SW_MODE_LEGACY) |
				RTE_BIT32(SXE2_COM_SW_MODE_SWITCHDEV) |
				RTE_BIT32(SXE2_COM_FC_ST_CHANGE);

	ret = sxe2_drv_dev_other_irq_set(adapter->cdev, fd, event);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set other irq, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static void sxe2_other_intr_unregister(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_drv_dev_other_irq_set(adapter->cdev, -1, 0);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set other irq, ret=%d", ret);
}

static int32_t sxe2_reset_intr_register(struct rte_eth_dev *dev, int32_t fd)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_drv_dev_reset_irq_set(adapter->cdev, fd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set reset irq, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static void sxe2_reset_intr_unregister(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_drv_dev_reset_irq_set(adapter->cdev, -1);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set reset irq, ret=%d", ret);
}

int32_t sxe2_intr_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_pci_device *pci_dev = container_of(dev->device, struct rte_pci_device, device);
	struct rte_intr_handle *reset_handle = NULL;
	int32_t ofd = -1;
	int32_t rfd = -1;
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	ofd = sxe2_event_intr_fd_create();
	if (ofd < 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to create event intr fd");
		ret = -EBADF;
		goto l_end;
	}

	ret = sxe2_intr_handler_cfg(pci_dev->intr_handle,
			ofd, sxe2_interrupt_handler_other, dev);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to create intr handler");
		goto l_err_create_other_handler;
	}

	ret = sxe2_event_intr_handler_init();
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Event handler init failed, ret=%d", ret);
		goto l_err_event_intr_handler_init;
	}

	ret = sxe2_other_intr_register(dev, ofd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to register other intr, ret=%d", ret);
		goto l_err_register_other_intr;
	}
	adapter->irq_ctxt.other_event_fd = ofd;

	rfd = sxe2_event_intr_fd_create();
	if (rfd < 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to create event intr fd");
		ret = -EBADF;
		goto l_err_create_reset_fd;
	}

	reset_handle = sxe2_intr_handler_create(rfd, sxe2_interrupt_handler_reset, dev);
	if (!reset_handle) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to create intr handler");
		ret = -ENOMEM;
		goto l_err_create_reset_handler;
	}
	adapter->irq_ctxt.reset_handle = reset_handle;

	ret = sxe2_reset_intr_register(dev, rfd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to register reset intr, ret=%d", ret);
		goto l_err_register_reset_intr;
	}
	adapter->irq_ctxt.reset_event_fd = rfd;

	goto l_end;
l_err_register_reset_intr:
	sxe2_intr_handler_destroy(reset_handle, sxe2_interrupt_handler_reset, dev);
	adapter->irq_ctxt.reset_handle = NULL;
l_err_create_reset_handler:
	sxe2_event_intr_fd_destroy(rfd);
l_err_create_reset_fd:
	sxe2_other_intr_unregister(dev);
	adapter->irq_ctxt.other_event_fd = -1;
l_err_register_other_intr:
	sxe2_event_intr_handler_uinit();
l_err_event_intr_handler_init:
	sxe2_intr_handler_destroy(pci_dev->intr_handle,
			sxe2_interrupt_handler_other, dev);
	pci_dev->intr_handle = NULL;
l_err_create_other_handler:
	sxe2_event_intr_fd_destroy(ofd);
l_end:
	return ret;
}

void sxe2_intr_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_pci_device *pci_dev = container_of(dev->device, struct rte_pci_device, device);

	sxe2_reset_intr_unregister(dev);
	sxe2_intr_handler_destroy(adapter->irq_ctxt.reset_handle,
				sxe2_interrupt_handler_reset, dev);
	sxe2_event_intr_fd_destroy(adapter->irq_ctxt.reset_event_fd);
	sxe2_other_intr_unregister(dev);
	sxe2_event_intr_handler_uinit();
	sxe2_intr_handler_destroy(pci_dev->intr_handle,
				sxe2_interrupt_handler_other, dev);
	sxe2_event_intr_fd_destroy(adapter->irq_ctxt.other_event_fd);

	adapter->irq_ctxt.other_event_fd = -1;
	adapter->irq_ctxt.reset_event_fd = -1;
	pci_dev->intr_handle = NULL;
	adapter->irq_ctxt.reset_handle = NULL;
}

static int32_t sxe2_rxq_intr_efd_alloc(struct rte_eth_dev *dev, int32_t *efd)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	int32_t fd = 0;

	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd < 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Can't setup eventfd, error %i (%s)",
			errno, strerror(errno));
		ret = -EBADF;
		goto l_end;
	}

	*efd = fd;

l_end:
	return ret;
}

static void sxe2_rxq_intr_efd_free(int32_t efd)
{
	close(efd);
}

static void sxe2_pci_hw_int_itr_set(struct sxe2_adapter *adapter, uint16_t msix_idx, uint16_t itr)
{
	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_ITR, msix_idx, itr);
}

static void sxe2_pci_hw_irq_disable(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	uint32_t value = (SXE2_ITR_IDX_NONE << SXE2_VF_DYN_CTL_ITR_IDX_S);

	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_DYN, irq_idx, value);
}

static void sxe2_pci_hw_irq_enable(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	uint32_t value = SXE2_VF_DYN_CTL_INTENABLE |
		SXE2_VF_DYN_CTL_CLEARPBA |
			(SXE2_ITR_IDX_NONE << SXE2_VF_DYN_CTL_ITR_IDX_S);

	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_DYN, irq_idx, value);
}

static uint32_t sxe2_pci_hw_irq_dyn_ctl_read(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	return sxe2_pci_map_read_reg(adapter, SXE2_PCI_MAP_RES_IRQ_DYN, irq_idx);
}

static void sxe2_pci_hw_msix_disable(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_MSIX,
			irq_idx, SXE2VF_BAR4_MSIX_DISABLE);
}

static void sxe2_pci_hw_msix_enable(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_MSIX,
			irq_idx, SXE2VF_BAR4_MSIX_ENABLE);
}

static void sxe2_pci_hw_irq_trigger(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_DYN, irq_idx,
			(SXE2VF_ITR_IDX_NONE << SXE2VF_DYN_CTL_ITR_IDX_SHIFT) |
			SXE2VF_DYN_CTL_SWINT_TRIG | SXE2VF_DYN_CTL_INTENABLE_MSK);
}

static void sxe2_pci_hw_irq_clear_pba(struct sxe2_adapter *adapter, uint16_t irq_idx)
{
	sxe2_pci_map_write_reg(adapter, SXE2_PCI_MAP_RES_IRQ_DYN, irq_idx,
		(SXE2VF_ITR_IDX_NONE << SXE2VF_DYN_CTL_ITR_IDX_SHIFT) |
		SXE2VF_DYN_CTL_CLEARPBA | SXE2VF_DYN_CTL_INTENABLE_MSK);
}

static void sxe2_rxq_msix_cfg_unmap(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	uint16_t rxq_cnt = adapter->q_ctxt.qp_cnt_assign;
	uint16_t i = 0;

	for (i = 0; i < irq_ctxt->rxq_irq_cnt; i++)
		sxe2_pci_hw_irq_disable(adapter, irq_ctxt->rxq_msix_idx[i]);

	for (i = 0; i < rxq_cnt; i++)
		(void)sxe2_drv_rxq_unbind_irq(adapter, i);
}

static int32_t sxe2_rxq_msix_cfg_map(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	int32_t ret = 0;
	uint32_t val;
	uint16_t rxq_cnt = dev->data->nb_rx_queues;
	uint16_t i = 0;
	uint8_t rx_low_latency = adapter->devargs.rx_low_latency;

	for (i = 0; i < irq_ctxt->rxq_irq_cnt; i++) {
		sxe2_pci_hw_irq_disable(adapter, irq_ctxt->rxq_msix_idx[i]);
		if (rx_low_latency) {
			sxe2_pci_hw_int_itr_set(adapter, irq_ctxt->rxq_msix_idx[i],
					SXE2_ITR_INTERVAL_LOW);
		} else {
			sxe2_pci_hw_int_itr_set(adapter, irq_ctxt->rxq_msix_idx[i],
					SXE2_ITR_INTERVAL_NORMAL);
		}
	}

	for (i = 0; i < rxq_cnt; i++) {
		ret = sxe2_drv_rxq_bind_irq(adapter, i, irq_ctxt->rxq_msix_idx[i]);
		if (ret != 0) {
			PMD_DEV_LOG_ERR(adapter, INIT, "RXQ[%u] bind IRQ[%u] failed.",
				i, irq_ctxt->rxq_msix_idx[i]);
			goto l_end;
		}
	}

	for (i = 0; i < irq_ctxt->rxq_irq_cnt; i++)
		sxe2_pci_hw_irq_enable(adapter, irq_ctxt->rxq_msix_idx[i]);

	for (i = 0; i < irq_ctxt->rxq_irq_cnt; i++) {
		val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, i);
		if ((val & SXE2VF_DYN_CTL_INTENABLE) == 0)
			continue;

		sxe2_pci_hw_msix_disable(adapter, i);
		sxe2_pci_hw_irq_trigger(adapter, i);
		val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, i);
		sxe2_pci_hw_irq_clear_pba(adapter, i);
		val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, i);
		sxe2_pci_hw_msix_enable(adapter, i);
	}

l_end:
	if (ret != 0)
		sxe2_rxq_msix_cfg_unmap(dev);
	return ret;
}

static int32_t sxe2_rxq_map_msix_intr(struct rte_eth_dev *dev,
		uint16_t msix_base __rte_unused, uint16_t nb_msix,
		uint16_t base_queue, uint16_t nb_queue)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	uint32_t *msix_tbl = NULL;
	int32_t ret = 0;
	uint16_t i;

	if (!nb_queue || !nb_msix || nb_queue < nb_msix) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Queue num[%u] or msix num[%u] is invalid.",
				nb_queue, nb_msix);
		ret = -EINVAL;
		goto l_end;
	}

	msix_tbl = rte_zmalloc(NULL, sizeof(uint32_t) * nb_queue, 0);
	if (!msix_tbl) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to alloc msix_tbl memory.");
		ret = -ENOMEM;
		goto l_end;
	}
	for (i = 0; i < nb_queue; i++) {
		msix_tbl[i] = i % nb_msix;
		PMD_DEV_LOG_INFO(adapter, INIT, "Queue %u is binding to vect %u",
				base_queue + i, msix_tbl[i]);
	}

	irq_ctxt->rxq_irq_cnt = nb_msix;
	irq_ctxt->rxq_msix_idx = msix_tbl;

l_end:
	return ret;
}

static void sxe2_rxq_unmap_msix_intr(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;

	rte_free(irq_ctxt->rxq_msix_idx);
	irq_ctxt->rxq_msix_idx = NULL;
	irq_ctxt->rxq_irq_cnt = 0;
}

static int32_t sxe2_rxq_intr_register(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	int32_t *efd_tbl = NULL;
	uint16_t rxq_cnt = dev->data->nb_rx_queues;
	uint16_t nb_msix = irq_ctxt->rxq_irq_cnt;
	uint16_t i;
	int32_t ret = 0;

	if (rte_intr_type_set(intr_handle, RTE_INTR_HANDLE_EXT)) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set intr_handle->type, error %i (%s)",
				errno, strerror(errno));
		ret = -EPERM;
		goto l_end;
	}

	efd_tbl = rte_zmalloc(NULL, sizeof(int32_t) * nb_msix, 0);
	if (!efd_tbl) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to alloc efd_tbl memory.");
		ret = -ENOMEM;
		goto l_end;
	}

	for (i = 0; i < nb_msix; i++) {
		ret = sxe2_rxq_intr_efd_alloc(dev, &efd_tbl[i]);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Failed to alloc efd_tbl, ret=%d", ret);
			goto l_free_efd_tbl;
		}
	}

	if (rte_intr_vec_list_alloc(intr_handle, "sxe2 rxq int", rxq_cnt)) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to allocate %d rx_queues intr_vec",
				rxq_cnt);
		ret = -ENOMEM;
		goto l_free_efd_tbl;
	}

	for	(i = 0; i < rxq_cnt; i++) {
		ret = rte_intr_vec_list_index_set(intr_handle, i,
				irq_ctxt->rxq_msix_idx[i] + RTE_INTR_VEC_RXTX_OFFSET);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set msix_tbl, ret=%d", ret);
			goto l_free_efd_tbl;
		}
	}

	for (i = 0; i < irq_ctxt->rxq_irq_cnt; i++) {
		ret = rte_intr_efds_index_set(intr_handle, i, efd_tbl[i]);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set efd_tbl, ret=%d", ret);
			goto l_free_efd_tbl;
		}
	}

	if (rte_intr_nb_efd_set(intr_handle, rxq_cnt)) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Set intr nb efd failed, error %i (%s)",
			errno, strerror(errno));
		ret = -EPERM;
		goto l_free_efd_tbl;
	}

	ret = sxe2_drv_dev_rxq_irq_set(adapter->cdev, 0, efd_tbl, nb_msix);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to set rxq irq, ret=%d", ret);
		goto l_free_efd_tbl;
	}
	irq_ctxt->rxq_event_fd = efd_tbl;

	goto l_end;

l_free_efd_tbl:
	if (efd_tbl) {
		for (i = 0; i < nb_msix; i++)
			if (efd_tbl[i] >= 0)
				sxe2_rxq_intr_efd_free(efd_tbl[i]);
		rte_free(efd_tbl);
	}
	irq_ctxt->rxq_event_fd = NULL;

	rte_intr_vec_list_free(intr_handle);
l_end:
	return ret;
}

static void sxe2_rxq_intr_unregister(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	int32_t efd = -1;
	uint16_t msix_cnt = irq_ctxt->rxq_irq_cnt;
	uint16_t i;

	if (irq_ctxt->rxq_event_fd) {
		for (i = 0; i < msix_cnt; i++) {
			(void)sxe2_drv_dev_rxq_irq_set(adapter->cdev, i, &efd, 1);
			sxe2_rxq_intr_efd_free(irq_ctxt->rxq_event_fd[i]);
		}
	}
	rte_free(irq_ctxt->rxq_event_fd);
	irq_ctxt->rxq_event_fd = NULL;

	rte_intr_vec_list_free(intr_handle);

	rte_intr_nb_efd_set(intr_handle, 0);
	rte_intr_max_intr_set(intr_handle, 0);
}

int32_t sxe2_rxq_intr_enable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint16_t msix_vect = adapter->irq_ctxt.rxq_base_idx_in_pf;
	uint16_t msix_cnt = adapter->irq_ctxt.rxq_avail_cnt;
	uint16_t rxq_cnt = dev->data->nb_rx_queues;
	uint16_t rxq_base = adapter->q_ctxt.base_idx_in_pf;
	int32_t ret = 0;

	if (!rxq_cnt)
		goto l_end;

	msix_cnt = RTE_MIN(msix_cnt, rxq_cnt);

	ret = sxe2_rxq_map_msix_intr(dev, msix_vect, msix_cnt, rxq_base, rxq_cnt);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to rxq[%d] map msix[%d] intr, cnt=%d, ret=%d",
					rxq_base, msix_vect, rxq_cnt, ret);
		goto l_end;
	}

	if (dev->data->dev_conf.intr_conf.rxq) {
		ret = sxe2_rxq_intr_register(dev);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, INIT, "Failed to register rxq[%d] intr, ret=%d",
					rxq_base, ret);
			goto l_err_unmap;
		}
	}

	ret = sxe2_rxq_msix_cfg_map(dev);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, INIT, "Failed to rxq[%d] map msix[%d] intr, ret=%d",
					rxq_base, msix_vect, ret);
		goto l_err_unregister;
	}

	goto l_end;
l_err_unregister:
	if (dev->data->dev_conf.intr_conf.rxq)
		sxe2_rxq_intr_unregister(dev);
l_err_unmap:
	sxe2_rxq_unmap_msix_intr(dev);
l_end:
	return ret;
}

int32_t sxe2_repr_rxq_intr_enable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint16_t rxq_cnt = dev->data->nb_rx_queues;
	int32_t ret = 0;
	uint16_t qid = adapter->repr_priv_data->repr_q_id;
	uint32_t val;

	if (!rxq_cnt)
		goto l_end;

	sxe2_pci_hw_irq_disable(adapter, qid);
	sxe2_pci_hw_int_itr_set(adapter, qid, SXE2_ITR_INTERVAL_NORMAL);
	ret = sxe2_drv_rxq_bind_irq(adapter, qid, qid);
	if (ret != 0) {
		PMD_DEV_LOG_ERR(adapter, INIT, "RXQ[%u] bind IRQ[%u] failed.",
				qid, qid);
		goto l_end;
	}
	sxe2_pci_hw_irq_enable(adapter, qid);

	val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, qid);
	if ((val & SXE2VF_DYN_CTL_INTENABLE) == 0)
		goto l_end;

	sxe2_pci_hw_msix_disable(adapter, qid);
	sxe2_pci_hw_irq_trigger(adapter, qid);
	val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, qid);
	sxe2_pci_hw_irq_clear_pba(adapter, qid);
	val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, qid);
	sxe2_pci_hw_msix_enable(adapter, qid);

l_end:
	return ret;
}

void sxe2_rxq_intr_disable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;

	if (!irq_ctxt->rxq_irq_cnt)
		goto l_end;

	sxe2_rxq_msix_cfg_unmap(dev);

	if (dev->data->dev_conf.intr_conf.rxq)
		sxe2_rxq_intr_unregister(dev);

	sxe2_rxq_unmap_msix_intr(dev);

l_end:
	return;
}

void sxe2_repr_rxq_intr_disable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint16_t qid = adapter->repr_priv_data->repr_q_id;

	sxe2_pci_hw_irq_disable(adapter, qid);
	(void)sxe2_drv_rxq_unbind_irq(adapter, qid);
}

int32_t sxe2_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_irq_context *irq_ctxt = &adapter->irq_ctxt;
	uint64_t buf;
	uint16_t irq_idx = irq_ctxt->rxq_msix_idx[queue_id];
	size_t read_ret;

	read_ret = read(irq_ctxt->rxq_event_fd[irq_idx], &buf, sizeof(buf));
	(void)read_ret;
	sxe2_pci_hw_irq_enable(adapter, irq_idx);
	return 0;
}

int32_t sxe2_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct sxe2_adapter *adapter =
		SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint32_t val;
	int32_t ret = 0;
	uint16_t irq_idx = adapter->irq_ctxt.rxq_msix_idx[queue_id];

	val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, irq_idx);
	if ((val & SXE2VF_DYN_CTL_INTENABLE) == 0) {
		PMD_DEV_LOG_DEBUG(adapter, INIT, "rxq [%d] interrupt is disabled.", queue_id);
		goto l_end;
	}

	sxe2_pci_hw_msix_disable(adapter, irq_idx);
	sxe2_pci_hw_irq_trigger(adapter, irq_idx);
	val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, irq_idx);
	sxe2_pci_hw_irq_clear_pba(adapter, irq_idx);
	val = sxe2_pci_hw_irq_dyn_ctl_read(adapter, irq_idx);
	sxe2_pci_hw_msix_enable(adapter, irq_idx);
l_end:
	return ret;
}
