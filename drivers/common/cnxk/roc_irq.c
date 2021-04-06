/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#if defined(__linux__)

#include <inttypes.h>
#include <linux/vfio.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MSIX_IRQ_SET_BUF_LEN                                                   \
	(sizeof(struct vfio_irq_set) + sizeof(int) * (PLT_MAX_RXTX_INTR_VEC_ID))

static int
irq_get_info(struct plt_intr_handle *intr_handle)
{
	struct vfio_irq_info irq = {.argsz = sizeof(irq)};
	int rc;

	irq.index = VFIO_PCI_MSIX_IRQ_INDEX;

	rc = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_GET_IRQ_INFO, &irq);
	if (rc < 0) {
		plt_err("Failed to get IRQ info rc=%d errno=%d", rc, errno);
		return rc;
	}

	plt_base_dbg("Flags=0x%x index=0x%x count=0x%x max_intr_vec_id=0x%x",
		     irq.flags, irq.index, irq.count, PLT_MAX_RXTX_INTR_VEC_ID);

	if (irq.count > PLT_MAX_RXTX_INTR_VEC_ID) {
		plt_err("HW max=%d > PLT_MAX_RXTX_INTR_VEC_ID: %d", irq.count,
			PLT_MAX_RXTX_INTR_VEC_ID);
		intr_handle->max_intr = PLT_MAX_RXTX_INTR_VEC_ID;
	} else {
		intr_handle->max_intr = irq.count;
	}

	return 0;
}

static int
irq_config(struct plt_intr_handle *intr_handle, unsigned int vec)
{
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int32_t *fd_ptr;
	int len, rc;

	if (vec > intr_handle->max_intr) {
		plt_err("vector=%d greater than max_intr=%d", vec,
			intr_handle->max_intr);
		return -EINVAL;
	}

	len = sizeof(struct vfio_irq_set) + sizeof(int32_t);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;

	irq_set->start = vec;
	irq_set->count = 1;
	irq_set->flags =
		VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;

	/* Use vec fd to set interrupt vectors */
	fd_ptr = (int32_t *)&irq_set->data[0];
	fd_ptr[0] = intr_handle->efds[vec];

	rc = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (rc)
		plt_err("Failed to set_irqs vector=0x%x rc=%d", vec, rc);

	return rc;
}

static int
irq_init(struct plt_intr_handle *intr_handle)
{
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int32_t *fd_ptr;
	int len, rc;
	uint32_t i;

	if (intr_handle->max_intr > PLT_MAX_RXTX_INTR_VEC_ID) {
		plt_err("Max_intr=%d greater than PLT_MAX_RXTX_INTR_VEC_ID=%d",
			intr_handle->max_intr, PLT_MAX_RXTX_INTR_VEC_ID);
		return -ERANGE;
	}

	len = sizeof(struct vfio_irq_set) +
	      sizeof(int32_t) * intr_handle->max_intr;

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	irq_set->start = 0;
	irq_set->count = intr_handle->max_intr;
	irq_set->flags =
		VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;

	fd_ptr = (int32_t *)&irq_set->data[0];
	for (i = 0; i < irq_set->count; i++)
		fd_ptr[i] = -1;

	rc = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (rc)
		plt_err("Failed to set irqs vector rc=%d", rc);

	return rc;
}

int
dev_irqs_disable(struct plt_intr_handle *intr_handle)
{
	/* Clear max_intr to indicate re-init next time */
	intr_handle->max_intr = 0;
	return plt_intr_disable(intr_handle);
}

int
dev_irq_register(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb,
		 void *data, unsigned int vec)
{
	struct plt_intr_handle tmp_handle;
	int rc;

	/* If no max_intr read from VFIO */
	if (intr_handle->max_intr == 0) {
		irq_get_info(intr_handle);
		irq_init(intr_handle);
	}

	if (vec > intr_handle->max_intr) {
		plt_err("Vector=%d greater than max_intr=%d", vec,
			intr_handle->max_intr);
		return -EINVAL;
	}

	tmp_handle = *intr_handle;
	/* Create new eventfd for interrupt vector */
	tmp_handle.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (tmp_handle.fd == -1)
		return -ENODEV;

	/* Register vector interrupt callback */
	rc = plt_intr_callback_register(&tmp_handle, cb, data);
	if (rc) {
		plt_err("Failed to register vector:0x%x irq callback.", vec);
		return rc;
	}

	intr_handle->efds[vec] = tmp_handle.fd;
	intr_handle->nb_efd =
		(vec > intr_handle->nb_efd) ? vec : intr_handle->nb_efd;
	if ((intr_handle->nb_efd + 1) > intr_handle->max_intr)
		intr_handle->max_intr = intr_handle->nb_efd + 1;

	plt_base_dbg("Enable vector:0x%x for vfio (efds: %d, max:%d)", vec,
		     intr_handle->nb_efd, intr_handle->max_intr);

	/* Enable MSIX vectors to VFIO */
	return irq_config(intr_handle, vec);
}

void
dev_irq_unregister(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb,
		   void *data, unsigned int vec)
{
	struct plt_intr_handle tmp_handle;
	uint8_t retries = 5; /* 5 ms */
	int rc;

	if (vec > intr_handle->max_intr) {
		plt_err("Error unregistering MSI-X interrupts vec:%d > %d", vec,
			intr_handle->max_intr);
		return;
	}

	tmp_handle = *intr_handle;
	tmp_handle.fd = intr_handle->efds[vec];
	if (tmp_handle.fd == -1)
		return;

	do {
		/* Un-register callback func from platform lib */
		rc = plt_intr_callback_unregister(&tmp_handle, cb, data);
		/* Retry only if -EAGAIN */
		if (rc != -EAGAIN)
			break;
		plt_delay_ms(1);
		retries--;
	} while (retries);

	if (rc < 0) {
		plt_err("Error unregistering MSI-X vec %d cb, rc=%d", vec, rc);
		return;
	}

	plt_base_dbg("Disable vector:0x%x for vfio (efds: %d, max:%d)", vec,
		     intr_handle->nb_efd, intr_handle->max_intr);

	if (intr_handle->efds[vec] != -1)
		close(intr_handle->efds[vec]);
	/* Disable MSIX vectors from VFIO */
	intr_handle->efds[vec] = -1;
	irq_config(intr_handle, vec);
}

#else

int
dev_irq_register(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb,
		 void *data, unsigned int vec)
{
	PLT_SET_USED(intr_handle);
	PLT_SET_USED(cb);
	PLT_SET_USED(data);
	PLT_SET_USED(vec);

	return -ENOTSUP;
}

void
dev_irq_unregister(struct plt_intr_handle *intr_handle, plt_intr_callback_fn cb,
		   void *data, unsigned int vec)
{
	PLT_SET_USED(intr_handle);
	PLT_SET_USED(cb);
	PLT_SET_USED(data);
	PLT_SET_USED(vec);
}

int
dev_irqs_disable(struct plt_intr_handle *intr_handle)
{
	PLT_SET_USED(intr_handle);

	return -ENOTSUP;
}

#endif /* __linux__ */
