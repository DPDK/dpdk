/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_alarm.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_interrupts.h>

#include "otx2_common.h"
#include "otx2_irq.h"

#ifdef RTE_EAL_VFIO

#include <inttypes.h>
#include <linux/vfio.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_INTR_VEC_ID RTE_MAX_RXTX_INTR_VEC_ID
#define MSIX_IRQ_SET_BUF_LEN (sizeof(struct vfio_irq_set) + \
			      sizeof(int) * (MAX_INTR_VEC_ID))

static int
irq_get_info(struct rte_intr_handle *intr_handle)
{
	struct vfio_irq_info irq = { .argsz = sizeof(irq) };
	int rc, vfio_dev_fd;

	irq.index = VFIO_PCI_MSIX_IRQ_INDEX;

	vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);
	rc = ioctl(vfio_dev_fd, VFIO_DEVICE_GET_IRQ_INFO, &irq);
	if (rc < 0) {
		otx2_err("Failed to get IRQ info rc=%d errno=%d", rc, errno);
		return rc;
	}

	otx2_base_dbg("Flags=0x%x index=0x%x count=0x%x max_intr_vec_id=0x%x",
		      irq.flags, irq.index, irq.count, MAX_INTR_VEC_ID);

	if (irq.count > MAX_INTR_VEC_ID) {
		otx2_err("HW max=%d > MAX_INTR_VEC_ID: %d",
			 rte_intr_max_intr_get(intr_handle),
			 MAX_INTR_VEC_ID);
		if (rte_intr_max_intr_set(intr_handle, MAX_INTR_VEC_ID))
			return -1;
	} else {
		if (rte_intr_max_intr_set(intr_handle, irq.count))
			return -1;
	}

	return 0;
}

static int
irq_config(struct rte_intr_handle *intr_handle, unsigned int vec)
{
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int len, rc, vfio_dev_fd;
	int32_t *fd_ptr;

	if (vec > (uint32_t)rte_intr_max_intr_get(intr_handle)) {
		otx2_err("vector=%d greater than max_intr=%d", vec,
			 rte_intr_max_intr_get(intr_handle));
		return -EINVAL;
	}

	len = sizeof(struct vfio_irq_set) + sizeof(int32_t);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;

	irq_set->start = vec;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;

	/* Use vec fd to set interrupt vectors */
	fd_ptr = (int32_t *)&irq_set->data[0];
	fd_ptr[0] = rte_intr_efds_index_get(intr_handle, vec);

	vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);
	rc = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (rc)
		otx2_err("Failed to set_irqs vector=0x%x rc=%d", vec, rc);

	return rc;
}

static int
irq_init(struct rte_intr_handle *intr_handle)
{
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int len, rc, vfio_dev_fd;
	int32_t *fd_ptr;
	uint32_t i;

	if (rte_intr_max_intr_get(intr_handle) > MAX_INTR_VEC_ID) {
		otx2_err("Max_intr=%d greater than MAX_INTR_VEC_ID=%d",
			 rte_intr_max_intr_get(intr_handle),
			 MAX_INTR_VEC_ID);
		return -ERANGE;
	}

	len = sizeof(struct vfio_irq_set) +
		sizeof(int32_t) * rte_intr_max_intr_get(intr_handle);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	irq_set->start = 0;
	irq_set->count = rte_intr_max_intr_get(intr_handle);
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;

	fd_ptr = (int32_t *)&irq_set->data[0];
	for (i = 0; i < irq_set->count; i++)
		fd_ptr[i] = -1;

	vfio_dev_fd = rte_intr_dev_fd_get(intr_handle);
	rc = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (rc)
		otx2_err("Failed to set irqs vector rc=%d", rc);

	return rc;
}

/**
 * @internal
 * Disable IRQ
 */
int
otx2_disable_irqs(struct rte_intr_handle *intr_handle)
{
	/* Clear max_intr to indicate re-init next time */
	if (rte_intr_max_intr_set(intr_handle, 0))
		return -1;
	return rte_intr_disable(intr_handle);
}

/**
 * @internal
 * Register IRQ
 */
int
otx2_register_irq(struct rte_intr_handle *intr_handle,
		  rte_intr_callback_fn cb, void *data, unsigned int vec)
{
	struct rte_intr_handle *tmp_handle;
	uint32_t nb_efd, tmp_nb_efd;
	int rc, fd;

	/* If no max_intr read from VFIO */
	if (rte_intr_max_intr_get(intr_handle) == 0) {
		irq_get_info(intr_handle);
		irq_init(intr_handle);
	}

	if (vec > (uint32_t)rte_intr_max_intr_get(intr_handle)) {
		otx2_err("Vector=%d greater than max_intr=%d", vec,
			rte_intr_max_intr_get(intr_handle));
		return -EINVAL;
	}

	tmp_handle = intr_handle;
	/* Create new eventfd for interrupt vector */
	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd == -1)
		return -ENODEV;

	if (rte_intr_fd_set(tmp_handle, fd))
		return errno;

	/* Register vector interrupt callback */
	rc = rte_intr_callback_register(tmp_handle, cb, data);
	if (rc) {
		otx2_err("Failed to register vector:0x%x irq callback.", vec);
		return rc;
	}

	rte_intr_efds_index_set(intr_handle, vec, fd);
	nb_efd = (vec > (uint32_t)rte_intr_nb_efd_get(intr_handle)) ?
		vec : (uint32_t)rte_intr_nb_efd_get(intr_handle);
	rte_intr_nb_efd_set(intr_handle, nb_efd);

	tmp_nb_efd = rte_intr_nb_efd_get(intr_handle) + 1;
	if (tmp_nb_efd > (uint32_t)rte_intr_max_intr_get(intr_handle))
		rte_intr_max_intr_set(intr_handle, tmp_nb_efd);

	otx2_base_dbg("Enable vector:0x%x for vfio (efds: %d, max:%d)", vec,
		     rte_intr_nb_efd_get(intr_handle),
		     rte_intr_max_intr_get(intr_handle));

	/* Enable MSIX vectors to VFIO */
	return irq_config(intr_handle, vec);
}

/**
 * @internal
 * Unregister IRQ
 */
void
otx2_unregister_irq(struct rte_intr_handle *intr_handle,
		    rte_intr_callback_fn cb, void *data, unsigned int vec)
{
	struct rte_intr_handle *tmp_handle;
	uint8_t retries = 5; /* 5 ms */
	int rc, fd;

	if (vec > (uint32_t)rte_intr_max_intr_get(intr_handle)) {
		otx2_err("Error unregistering MSI-X interrupts vec:%d > %d",
			 vec, rte_intr_max_intr_get(intr_handle));
		return;
	}

	tmp_handle = intr_handle;
	fd = rte_intr_efds_index_get(intr_handle, vec);
	if (fd == -1)
		return;

	if (rte_intr_fd_set(tmp_handle, fd))
		return;

	do {
		/* Un-register callback func from platform lib */
		rc = rte_intr_callback_unregister(tmp_handle, cb, data);
		/* Retry only if -EAGAIN */
		if (rc != -EAGAIN)
			break;
		rte_delay_ms(1);
		retries--;
	} while (retries);

	if (rc < 0) {
		otx2_err("Error unregistering MSI-X vec %d cb, rc=%d", vec, rc);
		return;
	}

	otx2_base_dbg("Disable vector:0x%x for vfio (efds: %d, max:%d)", vec,
		     rte_intr_nb_efd_get(intr_handle),
		     rte_intr_max_intr_get(intr_handle));

	if (rte_intr_efds_index_get(intr_handle, vec) != -1)
		close(rte_intr_efds_index_get(intr_handle, vec));
	/* Disable MSIX vectors from VFIO */
	rte_intr_efds_index_set(intr_handle, vec, -1);
	irq_config(intr_handle, vec);
}

#else

/**
 * @internal
 * Register IRQ
 */
int otx2_register_irq(__rte_unused struct rte_intr_handle *intr_handle,
		      __rte_unused rte_intr_callback_fn cb,
		      __rte_unused void *data, __rte_unused unsigned int vec)
{
	return -ENOTSUP;
}


/**
 * @internal
 * Unregister IRQ
 */
void otx2_unregister_irq(__rte_unused struct rte_intr_handle *intr_handle,
			 __rte_unused rte_intr_callback_fn cb,
			 __rte_unused void *data, __rte_unused unsigned int vec)
{
}

/**
 * @internal
 * Disable IRQ
 */
int otx2_disable_irqs(__rte_unused struct rte_intr_handle *intr_handle)
{
	return -ENOTSUP;
}

#endif /* RTE_EAL_VFIO */
