/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_vdpa.h>
#include <rte_vfio.h>
#include <rte_vhost.h>

#include <vdpa_driver.h>

#include "efx.h"
#include "sfc_vdpa_ops.h"
#include "sfc_vdpa.h"

/* These protocol features are needed to enable notifier ctrl */
#define SFC_VDPA_PROTOCOL_FEATURES \
		((1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_BACKEND_REQ) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_MQ))

/*
 * Set of features which are enabled by default.
 * Protocol feature bit is needed to enable notification notifier ctrl.
 */
#define SFC_VDPA_DEFAULT_FEATURES \
		((1ULL << VHOST_USER_F_PROTOCOL_FEATURES) | \
		 (1ULL << VIRTIO_NET_F_MQ))

#define SFC_VDPA_MSIX_IRQ_SET_BUF_LEN \
		(sizeof(struct vfio_irq_set) + \
		sizeof(int) * (SFC_VDPA_MAX_QUEUE_PAIRS * 2 + 1))

/* It will be used for target VF when calling function is not PF */
#define SFC_VDPA_VF_NULL		0xFFFF

static int
sfc_vdpa_get_device_features(struct sfc_vdpa_ops_data *ops_data)
{
	int rc;
	uint64_t dev_features;
	efx_nic_t *nic;

	nic = sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle)->nic;

	rc = efx_virtio_get_features(nic, EFX_VIRTIO_DEVICE_TYPE_NET,
				     &dev_features);
	if (rc != 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "could not read device feature: %s",
			     rte_strerror(rc));
		return rc;
	}

	ops_data->dev_features = dev_features;

	sfc_vdpa_info(ops_data->dev_handle,
		      "device supported virtio features : 0x%" PRIx64,
		      ops_data->dev_features);

	return 0;
}

static uint64_t
hva_to_gpa(int vid, uint64_t hva)
{
	struct rte_vhost_memory *vhost_mem = NULL;
	struct rte_vhost_mem_region *mem_reg = NULL;
	uint32_t i;
	uint64_t gpa = 0;

	if (rte_vhost_get_mem_table(vid, &vhost_mem) < 0)
		goto error;

	for (i = 0; i < vhost_mem->nregions; i++) {
		mem_reg = &vhost_mem->regions[i];

		if (hva >= mem_reg->host_user_addr &&
				hva < mem_reg->host_user_addr + mem_reg->size) {
			gpa = (hva - mem_reg->host_user_addr) +
				mem_reg->guest_phys_addr;
			break;
		}
	}

error:
	free(vhost_mem);
	return gpa;
}

static int
sfc_vdpa_enable_vfio_intr(struct sfc_vdpa_ops_data *ops_data)
{
	int rc;
	int *irq_fd_ptr;
	int vfio_dev_fd;
	uint32_t i, num_vring;
	struct rte_vhost_vring vring;
	struct vfio_irq_set *irq_set;
	struct rte_pci_device *pci_dev;
	char irq_set_buf[SFC_VDPA_MSIX_IRQ_SET_BUF_LEN];
	void *dev;

	num_vring = rte_vhost_get_vring_num(ops_data->vid);
	dev = ops_data->dev_handle;
	vfio_dev_fd = sfc_vdpa_adapter_by_dev_handle(dev)->vfio_dev_fd;
	pci_dev = sfc_vdpa_adapter_by_dev_handle(dev)->pdev;

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = num_vring + 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			 VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;
	irq_fd_ptr = (int *)&irq_set->data;
	irq_fd_ptr[RTE_INTR_VEC_ZERO_OFFSET] =
		rte_intr_fd_get(pci_dev->intr_handle);

	for (i = 0; i < num_vring; i++) {
		rc = rte_vhost_get_vhost_vring(ops_data->vid, i, &vring);
		if (rc)
			return -1;

		irq_fd_ptr[RTE_INTR_VEC_RXTX_OFFSET + i] = vring.callfd;
	}

	rc = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (rc) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "error enabling MSI-X interrupts: %s",
			     strerror(errno));
		return -1;
	}

	return 0;
}

static int
sfc_vdpa_disable_vfio_intr(struct sfc_vdpa_ops_data *ops_data)
{
	int rc;
	int vfio_dev_fd;
	struct vfio_irq_set irq_set;
	void *dev;

	dev = ops_data->dev_handle;
	vfio_dev_fd = sfc_vdpa_adapter_by_dev_handle(dev)->vfio_dev_fd;

	irq_set.argsz = sizeof(irq_set);
	irq_set.count = 0;
	irq_set.flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set.index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set.start = 0;

	rc = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, &irq_set);
	if (rc) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "error disabling MSI-X interrupts: %s",
			     strerror(errno));
		return -1;
	}

	return 0;
}

static int
sfc_vdpa_get_vring_info(struct sfc_vdpa_ops_data *ops_data,
			int vq_num, struct sfc_vdpa_vring_info *vring)
{
	int rc;
	uint64_t gpa;
	struct rte_vhost_vring vq;

	rc = rte_vhost_get_vhost_vring(ops_data->vid, vq_num, &vq);
	if (rc < 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "get vhost vring failed: %s", rte_strerror(rc));
		return rc;
	}

	gpa = hva_to_gpa(ops_data->vid, (uint64_t)(uintptr_t)vq.desc);
	if (gpa == 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "fail to get GPA for descriptor ring.");
		return -1;
	}
	vring->desc = gpa;

	gpa = hva_to_gpa(ops_data->vid, (uint64_t)(uintptr_t)vq.avail);
	if (gpa == 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "fail to get GPA for available ring.");
		return -1;
	}
	vring->avail = gpa;

	gpa = hva_to_gpa(ops_data->vid, (uint64_t)(uintptr_t)vq.used);
	if (gpa == 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "fail to get GPA for used ring.");
		return -1;
	}
	vring->used = gpa;

	vring->size = vq.size;

	rc = rte_vhost_get_vring_base(ops_data->vid, vq_num,
				      &vring->last_avail_idx,
				      &vring->last_used_idx);

	return rc;
}

static int
sfc_vdpa_virtq_start(struct sfc_vdpa_ops_data *ops_data, int vq_num)
{
	int rc;
	uint32_t doorbell;
	efx_virtio_vq_t *vq;
	struct sfc_vdpa_vring_info vring;
	efx_virtio_vq_cfg_t vq_cfg;
	efx_virtio_vq_dyncfg_t vq_dyncfg;

	vq = ops_data->vq_cxt[vq_num].vq;
	if (vq == NULL)
		return -1;

	rc = sfc_vdpa_get_vring_info(ops_data, vq_num, &vring);
	if (rc < 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "get vring info failed: %s", rte_strerror(rc));
		goto fail_vring_info;
	}

	vq_cfg.evvc_target_vf = SFC_VDPA_VF_NULL;

	/* even virtqueue for RX and odd for TX */
	if (vq_num % 2) {
		vq_cfg.evvc_type = EFX_VIRTIO_VQ_TYPE_NET_TXQ;
		sfc_vdpa_info(ops_data->dev_handle,
			      "configure virtqueue # %d (TXQ)", vq_num);
	} else {
		vq_cfg.evvc_type = EFX_VIRTIO_VQ_TYPE_NET_RXQ;
		sfc_vdpa_info(ops_data->dev_handle,
			      "configure virtqueue # %d (RXQ)", vq_num);
	}

	vq_cfg.evvc_vq_num = vq_num;
	vq_cfg.evvc_desc_tbl_addr   = vring.desc;
	vq_cfg.evvc_avail_ring_addr = vring.avail;
	vq_cfg.evvc_used_ring_addr  = vring.used;
	vq_cfg.evvc_vq_size = vring.size;

	vq_dyncfg.evvd_vq_used_idx  = vring.last_used_idx;
	vq_dyncfg.evvd_vq_avail_idx = vring.last_avail_idx;

	/* MSI-X vector is function-relative */
	vq_cfg.evvc_msix_vector = RTE_INTR_VEC_RXTX_OFFSET + vq_num;
	if (ops_data->vdpa_context == SFC_VDPA_AS_VF)
		vq_cfg.evvc_pas_id = 0;
	vq_cfg.evcc_features = ops_data->dev_features &
			       ops_data->req_features;

	/* Start virtqueue */
	rc = efx_virtio_qstart(vq, &vq_cfg, &vq_dyncfg);
	if (rc != 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "virtqueue start failed: %s",
			     rte_strerror(rc));
		goto fail_virtio_qstart;
	}

	sfc_vdpa_info(ops_data->dev_handle,
		      "virtqueue started successfully for vq_num %d", vq_num);

	rc = efx_virtio_get_doorbell_offset(vq,	&doorbell);
	if (rc != 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "failed to get doorbell offset: %s",
			     rte_strerror(rc));
		goto fail_doorbell;
	}

	/*
	 * Cache the bar_offset here for each VQ here, it will come
	 * in handy when sfc_vdpa_get_notify_area() is invoked.
	 */
	ops_data->vq_cxt[vq_num].doorbell = (void *)(uintptr_t)doorbell;
	ops_data->vq_cxt[vq_num].enable = B_TRUE;

	return rc;

fail_doorbell:
fail_virtio_qstart:
	efx_virtio_qdestroy(vq);
fail_vring_info:
	return rc;
}

static int
sfc_vdpa_virtq_stop(struct sfc_vdpa_ops_data *ops_data, int vq_num)
{
	int rc;
	efx_virtio_vq_dyncfg_t vq_idx;
	efx_virtio_vq_t *vq;

	if (ops_data->vq_cxt[vq_num].enable != B_TRUE)
		return -1;

	vq = ops_data->vq_cxt[vq_num].vq;
	if (vq == NULL)
		return -1;

	/* stop the vq */
	rc = efx_virtio_qstop(vq, &vq_idx);
	if (rc == 0) {
		ops_data->vq_cxt[vq_num].cidx = vq_idx.evvd_vq_used_idx;
		ops_data->vq_cxt[vq_num].pidx = vq_idx.evvd_vq_avail_idx;
	}
	ops_data->vq_cxt[vq_num].enable = B_FALSE;

	return rc;
}

static int
sfc_vdpa_configure(struct sfc_vdpa_ops_data *ops_data)
{
	int rc, i;
	int nr_vring;
	int max_vring_cnt;
	efx_virtio_vq_t *vq;
	efx_nic_t *nic;
	void *dev;

	dev = ops_data->dev_handle;
	nic = sfc_vdpa_adapter_by_dev_handle(dev)->nic;

	SFC_EFX_ASSERT(ops_data->state == SFC_VDPA_STATE_INITIALIZED);

	ops_data->state = SFC_VDPA_STATE_CONFIGURING;

	nr_vring = rte_vhost_get_vring_num(ops_data->vid);
	max_vring_cnt =
		(sfc_vdpa_adapter_by_dev_handle(dev)->max_queue_count * 2);

	/* number of vring should not be more than supported max vq count */
	if (nr_vring > max_vring_cnt) {
		sfc_vdpa_err(dev,
			     "nr_vring (%d) is > max vring count (%d)",
			     nr_vring, max_vring_cnt);
		goto fail_vring_num;
	}

	rc = sfc_vdpa_dma_map(ops_data, true);
	if (rc) {
		sfc_vdpa_err(dev,
			     "DMA map failed: %s", rte_strerror(rc));
		goto fail_dma_map;
	}

	for (i = 0; i < nr_vring; i++) {
		rc = efx_virtio_qcreate(nic, &vq);
		if ((rc != 0) || (vq == NULL)) {
			sfc_vdpa_err(dev,
				     "virtqueue create failed: %s",
				     rte_strerror(rc));
			goto fail_vq_create;
		}

		/* store created virtqueue context */
		ops_data->vq_cxt[i].vq = vq;
	}

	ops_data->vq_count = i;

	ops_data->state = SFC_VDPA_STATE_CONFIGURED;

	return 0;

fail_vq_create:
	sfc_vdpa_dma_map(ops_data, false);

fail_dma_map:
fail_vring_num:
	ops_data->state = SFC_VDPA_STATE_INITIALIZED;

	return -1;
}

static void
sfc_vdpa_close(struct sfc_vdpa_ops_data *ops_data)
{
	int i;

	if (ops_data->state != SFC_VDPA_STATE_CONFIGURED)
		return;

	ops_data->state = SFC_VDPA_STATE_CLOSING;

	for (i = 0; i < ops_data->vq_count; i++) {
		if (ops_data->vq_cxt[i].vq == NULL)
			continue;

		efx_virtio_qdestroy(ops_data->vq_cxt[i].vq);
	}

	sfc_vdpa_dma_map(ops_data, false);

	ops_data->state = SFC_VDPA_STATE_INITIALIZED;
}

static void
sfc_vdpa_stop(struct sfc_vdpa_ops_data *ops_data)
{
	int i;
	int rc;

	if (ops_data->state != SFC_VDPA_STATE_STARTED)
		return;

	ops_data->state = SFC_VDPA_STATE_STOPPING;

	for (i = 0; i < ops_data->vq_count; i++) {
		rc = sfc_vdpa_virtq_stop(ops_data, i);
		if (rc != 0)
			continue;
	}

	sfc_vdpa_disable_vfio_intr(ops_data);

	sfc_vdpa_filter_remove(ops_data);

	ops_data->state = SFC_VDPA_STATE_CONFIGURED;
}

static int
sfc_vdpa_start(struct sfc_vdpa_ops_data *ops_data)
{
	int i, j;
	int rc;

	SFC_EFX_ASSERT(ops_data->state == SFC_VDPA_STATE_CONFIGURED);

	sfc_vdpa_log_init(ops_data->dev_handle, "entry");

	ops_data->state = SFC_VDPA_STATE_STARTING;

	sfc_vdpa_log_init(ops_data->dev_handle, "enable interrupts");
	rc = sfc_vdpa_enable_vfio_intr(ops_data);
	if (rc < 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "vfio intr allocation failed: %s",
			     rte_strerror(rc));
		goto fail_enable_vfio_intr;
	}

	rte_vhost_get_negotiated_features(ops_data->vid,
					  &ops_data->req_features);

	sfc_vdpa_info(ops_data->dev_handle,
		      "negotiated feature : 0x%" PRIx64,
		      ops_data->req_features);

	for (i = 0; i < ops_data->vq_count; i++) {
		sfc_vdpa_log_init(ops_data->dev_handle,
				  "starting vq# %d", i);
		rc = sfc_vdpa_virtq_start(ops_data, i);
		if (rc != 0)
			goto fail_vq_start;
	}

	ops_data->vq_count = i;

	sfc_vdpa_log_init(ops_data->dev_handle,
			  "configure MAC filters");
	rc = sfc_vdpa_filter_config(ops_data);
	if (rc != 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "MAC filter config failed: %s",
			     rte_strerror(rc));
		goto fail_filter_cfg;
	}

	ops_data->state = SFC_VDPA_STATE_STARTED;

	sfc_vdpa_log_init(ops_data->dev_handle, "done");

	return 0;

fail_filter_cfg:
	/* remove already created filters */
	sfc_vdpa_filter_remove(ops_data);
fail_vq_start:
	/* stop already started virtqueues */
	for (j = 0; j < i; j++)
		sfc_vdpa_virtq_stop(ops_data, j);
	sfc_vdpa_disable_vfio_intr(ops_data);

fail_enable_vfio_intr:
	ops_data->state = SFC_VDPA_STATE_CONFIGURED;

	return rc;
}

static int
sfc_vdpa_get_queue_num(struct rte_vdpa_device *vdpa_dev, uint32_t *queue_num)
{
	struct sfc_vdpa_ops_data *ops_data;
	void *dev;

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	dev = ops_data->dev_handle;
	*queue_num = sfc_vdpa_adapter_by_dev_handle(dev)->max_queue_count;

	sfc_vdpa_info(dev, "vDPA ops get_queue_num :: supported queue num : %u",
		      *queue_num);

	return 0;
}

static int
sfc_vdpa_get_features(struct rte_vdpa_device *vdpa_dev, uint64_t *features)
{
	struct sfc_vdpa_ops_data *ops_data;

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	*features = ops_data->drv_features;

	sfc_vdpa_info(ops_data->dev_handle,
		      "vDPA ops get_feature :: features : 0x%" PRIx64,
		      *features);

	return 0;
}

static int
sfc_vdpa_get_protocol_features(struct rte_vdpa_device *vdpa_dev,
			       uint64_t *features)
{
	struct sfc_vdpa_ops_data *ops_data;

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	*features = SFC_VDPA_PROTOCOL_FEATURES;

	sfc_vdpa_info(ops_data->dev_handle,
		      "vDPA ops get_protocol_feature :: features : 0x%" PRIx64,
		      *features);

	return 0;
}

static uint32_t
sfc_vdpa_notify_ctrl(void *arg)
{
	struct sfc_vdpa_ops_data *ops_data;
	int vid;

	ops_data = arg;
	if (ops_data == NULL)
		return 0;

	sfc_vdpa_adapter_lock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));

	vid = ops_data->vid;

	if (rte_vhost_host_notifier_ctrl(vid, RTE_VHOST_QUEUE_ALL, true) != 0)
		sfc_vdpa_info(ops_data->dev_handle,
			      "vDPA (%s): Notifier could not get configured",
			      ops_data->vdpa_dev->device->name);

	sfc_vdpa_adapter_unlock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));

	return 0;
}

static int
sfc_vdpa_setup_notify_ctrl(struct sfc_vdpa_ops_data *ops_data)
{
	int ret;

	ops_data->is_notify_thread_started = false;

	/*
	 * Use rte_vhost_host_notifier_ctrl in a thread to avoid
	 * dead lock scenario when multiple VFs are used in single vdpa
	 * application and multiple VFs are passed to a single VM.
	 */
	ret = rte_thread_create_internal_control(&ops_data->notify_tid,
			     "sfc-vdpa", sfc_vdpa_notify_ctrl, ops_data);
	if (ret != 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "failed to create notify_ctrl thread: %s",
			     rte_strerror(ret));
		return -1;
	}
	ops_data->is_notify_thread_started = true;

	return 0;
}

static int
sfc_vdpa_dev_config(int vid)
{
	struct rte_vdpa_device *vdpa_dev;
	int rc;
	struct sfc_vdpa_ops_data *ops_data;

	vdpa_dev = rte_vhost_get_vdpa_device(vid);

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL) {
		SFC_VDPA_GENERIC_LOG(ERR,
			     "invalid vDPA device : %p, vid : %d",
			     vdpa_dev, vid);
		return -1;
	}

	sfc_vdpa_log_init(ops_data->dev_handle, "entry");

	ops_data->vid = vid;

	sfc_vdpa_adapter_lock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));

	sfc_vdpa_log_init(ops_data->dev_handle, "configuring");
	rc = sfc_vdpa_configure(ops_data);
	if (rc != 0)
		goto fail_vdpa_config;

	sfc_vdpa_log_init(ops_data->dev_handle, "starting");
	rc = sfc_vdpa_start(ops_data);
	if (rc != 0)
		goto fail_vdpa_start;

	rc = sfc_vdpa_setup_notify_ctrl(ops_data);
	if (rc != 0)
		goto fail_vdpa_notify;

	sfc_vdpa_adapter_unlock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));

	sfc_vdpa_log_init(ops_data->dev_handle, "done");

	return 0;

fail_vdpa_notify:
	sfc_vdpa_stop(ops_data);

fail_vdpa_start:
	sfc_vdpa_close(ops_data);

fail_vdpa_config:
	sfc_vdpa_adapter_unlock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));

	return -1;
}

static int
sfc_vdpa_dev_close(int vid)
{
	int ret;
	struct rte_vdpa_device *vdpa_dev;
	struct sfc_vdpa_ops_data *ops_data;

	vdpa_dev = rte_vhost_get_vdpa_device(vid);

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL) {
		SFC_VDPA_GENERIC_LOG(ERR,
			     "invalid vDPA device : %p, vid : %d",
			     vdpa_dev, vid);
		return -1;
	}

	sfc_vdpa_adapter_lock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));
	if (ops_data->is_notify_thread_started == true) {
		ret = pthread_cancel((pthread_t)ops_data->notify_tid.opaque_id);
		if (ret != 0) {
			sfc_vdpa_err(ops_data->dev_handle,
				     "failed to cancel notify_ctrl thread: %s",
				     rte_strerror(ret));
		}

		ret = rte_thread_join(ops_data->notify_tid, NULL);
		if (ret != 0) {
			sfc_vdpa_err(ops_data->dev_handle,
				     "failed to join terminated notify_ctrl thread: %s",
				     rte_strerror(ret));
		}
	}
	ops_data->is_notify_thread_started = false;

	sfc_vdpa_stop(ops_data);
	sfc_vdpa_close(ops_data);

	sfc_vdpa_adapter_unlock(sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle));

	return 0;
}

static int
sfc_vdpa_set_vring_state(int vid, int vring, int state)
{
	struct sfc_vdpa_ops_data *ops_data;
	struct rte_vdpa_device *vdpa_dev;
	efx_rc_t rc;
	int vring_max;
	void *dev;

	vdpa_dev = rte_vhost_get_vdpa_device(vid);

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	dev = ops_data->dev_handle;

	sfc_vdpa_info(dev,
		      "vDPA ops set_vring_state: vid: %d, vring: %d, state:%d",
		      vid, vring, state);

	vring_max = (sfc_vdpa_adapter_by_dev_handle(dev)->max_queue_count * 2);

	if (vring < 0 || vring > vring_max) {
		sfc_vdpa_err(dev, "received invalid vring id : %d to set state",
			     vring);
		return -1;
	}

	/*
	 * Skip if device is not yet started. virtqueues state can be
	 * changed once it is created and other configurations are done.
	 */
	if (ops_data->state != SFC_VDPA_STATE_STARTED)
		return 0;

	if (ops_data->vq_cxt[vring].enable == state)
		return 0;

	if (state == 0) {
		rc = sfc_vdpa_virtq_stop(ops_data, vring);
		if (rc != 0) {
			sfc_vdpa_err(dev, "virtqueue stop failed: %s",
				     rte_strerror(rc));
		}
	} else {
		rc = sfc_vdpa_virtq_start(ops_data, vring);
		if (rc != 0) {
			sfc_vdpa_err(dev, "virtqueue start failed: %s",
				     rte_strerror(rc));
		}
	}

	return rc;
}

static int
sfc_vdpa_set_features(int vid)
{
	RTE_SET_USED(vid);

	return -1;
}

static int
sfc_vdpa_get_vfio_device_fd(int vid)
{
	struct rte_vdpa_device *vdpa_dev;
	struct sfc_vdpa_ops_data *ops_data;
	int vfio_dev_fd;
	void *dev;

	vdpa_dev = rte_vhost_get_vdpa_device(vid);

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	dev = ops_data->dev_handle;
	vfio_dev_fd = sfc_vdpa_adapter_by_dev_handle(dev)->vfio_dev_fd;

	sfc_vdpa_info(dev, "vDPA ops get_vfio_device_fd :: vfio fd : %d",
		      vfio_dev_fd);

	return vfio_dev_fd;
}

static int
sfc_vdpa_get_notify_area(int vid, int qid, uint64_t *offset, uint64_t *size)
{
	int ret;
	efx_nic_t *nic;
	int vfio_dev_fd;
	volatile void *doorbell;
	struct rte_pci_device *pci_dev;
	struct rte_vdpa_device *vdpa_dev;
	struct sfc_vdpa_ops_data *ops_data;
	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	const efx_nic_cfg_t *encp;
	int max_vring_cnt;
	int64_t len;
	void *dev;

	vdpa_dev = rte_vhost_get_vdpa_device(vid);

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	dev = ops_data->dev_handle;

	vfio_dev_fd = sfc_vdpa_adapter_by_dev_handle(dev)->vfio_dev_fd;
	max_vring_cnt =
		(sfc_vdpa_adapter_by_dev_handle(dev)->max_queue_count * 2);

	nic = sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle)->nic;
	encp = efx_nic_cfg_get(nic);

	if (qid >= max_vring_cnt) {
		sfc_vdpa_err(dev, "invalid qid : %d", qid);
		return -1;
	}

	reg.index = sfc_vdpa_adapter_by_dev_handle(dev)->mem_bar.esb_rid;
	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if (ret != 0) {
		sfc_vdpa_err(dev, "could not get device region info: %s",
			     strerror(errno));
		return ret;
	}

	/* Use bar_offset that was cached during sfc_vdpa_virtq_start() */
	*offset = reg.offset + (uint64_t)ops_data->vq_cxt[qid].doorbell;

	len = (1U << encp->enc_vi_window_shift) / 2;
	if (len >= sysconf(_SC_PAGESIZE)) {
		*size = sysconf(_SC_PAGESIZE);
	} else {
		sfc_vdpa_err(dev, "invalid VI window size : 0x%" PRIx64, len);
		return -1;
	}

	sfc_vdpa_info(dev, "vDPA ops get_notify_area :: offset : 0x%" PRIx64,
		      *offset);

	pci_dev = sfc_vdpa_adapter_by_dev_handle(dev)->pdev;
	doorbell = (uint8_t *)pci_dev->mem_resource[reg.index].addr + *offset;

	/*
	 * virtio-net driver in VM sends queue notifications before
	 * vDPA has a chance to setup the queues and notification area,
	 * and hence the HW misses these doorbell notifications.
	 * Since, it is safe to send duplicate doorbell, send another
	 * doorbell from vDPA driver as workaround for this timing issue.
	 */
	rte_write16(qid, doorbell);

	return 0;
}

static struct rte_vdpa_dev_ops sfc_vdpa_ops = {
	.get_queue_num = sfc_vdpa_get_queue_num,
	.get_features = sfc_vdpa_get_features,
	.get_protocol_features = sfc_vdpa_get_protocol_features,
	.dev_conf = sfc_vdpa_dev_config,
	.dev_close = sfc_vdpa_dev_close,
	.set_vring_state = sfc_vdpa_set_vring_state,
	.set_features = sfc_vdpa_set_features,
	.get_vfio_device_fd = sfc_vdpa_get_vfio_device_fd,
	.get_notify_area = sfc_vdpa_get_notify_area,
};

struct sfc_vdpa_ops_data *
sfc_vdpa_device_init(void *dev_handle, enum sfc_vdpa_context context)
{
	struct sfc_vdpa_ops_data *ops_data;
	struct rte_pci_device *pci_dev;
	int rc;

	/* Create vDPA ops context */
	ops_data = rte_zmalloc("vdpa", sizeof(struct sfc_vdpa_ops_data), 0);
	if (ops_data == NULL)
		return NULL;

	ops_data->vdpa_context = context;
	ops_data->dev_handle = dev_handle;

	pci_dev = sfc_vdpa_adapter_by_dev_handle(dev_handle)->pdev;

	/* Register vDPA Device */
	sfc_vdpa_log_init(dev_handle, "register vDPA device");
	ops_data->vdpa_dev =
		rte_vdpa_register_device(&pci_dev->device, &sfc_vdpa_ops);
	if (ops_data->vdpa_dev == NULL) {
		sfc_vdpa_err(dev_handle, "vDPA device registration failed");
		goto fail_register_device;
	}

	/* Read supported device features */
	sfc_vdpa_log_init(dev_handle, "get device feature");
	rc = sfc_vdpa_get_device_features(ops_data);
	if (rc != 0)
		goto fail_get_dev_feature;

	/* Driver features are superset of device supported feature
	 * and any additional features supported by the driver.
	 */
	ops_data->drv_features =
		ops_data->dev_features | SFC_VDPA_DEFAULT_FEATURES;

	ops_data->state = SFC_VDPA_STATE_INITIALIZED;

	return ops_data;

fail_get_dev_feature:
	rte_vdpa_unregister_device(ops_data->vdpa_dev);

fail_register_device:
	rte_free(ops_data);
	return NULL;
}

void
sfc_vdpa_device_fini(struct sfc_vdpa_ops_data *ops_data)
{
	rte_vdpa_unregister_device(ops_data->vdpa_dev);

	rte_free(ops_data);
}
