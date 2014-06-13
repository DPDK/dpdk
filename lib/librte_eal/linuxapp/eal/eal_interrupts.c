/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/ioctl.h>

#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_pci.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_spinlock.h>

#include "eal_private.h"
#include "eal_vfio.h"

#define EAL_INTR_EPOLL_WAIT_FOREVER (-1)

/**
 * union for pipe fds.
 */
union intr_pipefds{
	struct {
		int pipefd[2];
	};
	struct {
		int readfd;
		int writefd;
	};
};

/**
 * union buffer for reading on different devices
 */
union rte_intr_read_buffer {
	int uio_intr_count;              /* for uio device */
#ifdef VFIO_PRESENT
	uint64_t vfio_intr_count;        /* for vfio device */
#endif
	uint64_t timerfd_num;            /* for timerfd */
	char charbuf[16];                /* for others */
};

TAILQ_HEAD(rte_intr_cb_list, rte_intr_callback);
TAILQ_HEAD(rte_intr_source_list, rte_intr_source);

struct rte_intr_callback {
	TAILQ_ENTRY(rte_intr_callback) next;
	rte_intr_callback_fn cb_fn;  /**< callback address */
	void *cb_arg;                /**< parameter for callback */
};

struct rte_intr_source {
	TAILQ_ENTRY(rte_intr_source) next;
	struct rte_intr_handle intr_handle; /**< interrupt handle */
	struct rte_intr_cb_list callbacks;  /**< user callbacks */
	uint32_t active;
};

/* global spinlock for interrupt data operation */
static rte_spinlock_t intr_lock = RTE_SPINLOCK_INITIALIZER;

/* union buffer for pipe read/write */
static union intr_pipefds intr_pipe;

/* interrupt sources list */
static struct rte_intr_source_list intr_sources;

/* interrupt handling thread */
static pthread_t intr_thread;

/* VFIO interrupts */
#ifdef VFIO_PRESENT

#define IRQ_SET_BUF_LEN  (sizeof(struct vfio_irq_set) + sizeof(int))

/* enable legacy (INTx) interrupts */
static int
vfio_enable_intx(struct rte_intr_handle *intr_handle) {
	struct vfio_irq_set *irq_set;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	int len, ret;
	int *fd_ptr;

	len = sizeof(irq_set_buf);

	/* enable INTx */
	irq_set = (struct vfio_irq_set *) irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_INTX_IRQ_INDEX;
	irq_set->start = 0;
	fd_ptr = (int *) &irq_set->data;
	*fd_ptr = intr_handle->fd;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error enabling INTx interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}

	/* unmask INTx after enabling */
	memset(irq_set, 0, len);
	len = sizeof(struct vfio_irq_set);
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_UNMASK;
	irq_set->index = VFIO_PCI_INTX_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error unmasking INTx interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}
	return 0;
}

/* disable legacy (INTx) interrupts */
static int
vfio_disable_intx(struct rte_intr_handle *intr_handle) {
	struct vfio_irq_set *irq_set;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	int len, ret;

	len = sizeof(struct vfio_irq_set);

	/* mask interrupts before disabling */
	irq_set = (struct vfio_irq_set *) irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_UNMASK;
	irq_set->index = VFIO_PCI_INTX_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error unmasking INTx interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}

	/* disable INTx*/
	memset(irq_set, 0, len);
	irq_set->argsz = len;
	irq_set->count = 0;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_INTX_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL,
			"Error disabling INTx interrupts for fd %d\n", intr_handle->fd);
		return -1;
	}
	return 0;
}

/* enable MSI-X interrupts */
static int
vfio_enable_msi(struct rte_intr_handle *intr_handle) {
	int len, ret;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int *fd_ptr;

	len = sizeof(irq_set_buf);

	irq_set = (struct vfio_irq_set *) irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSI_IRQ_INDEX;
	irq_set->start = 0;
	fd_ptr = (int *) &irq_set->data;
	*fd_ptr = intr_handle->fd;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error enabling MSI interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}

	/* manually trigger interrupt to enable it */
	memset(irq_set, 0, len);
	len = sizeof(struct vfio_irq_set);
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSI_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error triggering MSI interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}
	return 0;
}

/* disable MSI-X interrupts */
static int
vfio_disable_msi(struct rte_intr_handle *intr_handle) {
	struct vfio_irq_set *irq_set;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	int len, ret;

	len = sizeof(struct vfio_irq_set);

	irq_set = (struct vfio_irq_set *) irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 0;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSI_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret)
		RTE_LOG(ERR, EAL,
			"Error disabling MSI interrupts for fd %d\n", intr_handle->fd);

	return ret;
}

/* enable MSI-X interrupts */
static int
vfio_enable_msix(struct rte_intr_handle *intr_handle) {
	int len, ret;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int *fd_ptr;

	len = sizeof(irq_set_buf);

	irq_set = (struct vfio_irq_set *) irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;
	fd_ptr = (int *) &irq_set->data;
	*fd_ptr = intr_handle->fd;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error enabling MSI-X interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}

	/* manually trigger interrupt to enable it */
	memset(irq_set, 0, len);
	len = sizeof(struct vfio_irq_set);
	irq_set->argsz = len;
	irq_set->count = 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret) {
		RTE_LOG(ERR, EAL, "Error triggering MSI-X interrupts for fd %d\n",
						intr_handle->fd);
		return -1;
	}
	return 0;
}

/* disable MSI-X interrupts */
static int
vfio_disable_msix(struct rte_intr_handle *intr_handle) {
	struct vfio_irq_set *irq_set;
	char irq_set_buf[IRQ_SET_BUF_LEN];
	int len, ret;

	len = sizeof(struct vfio_irq_set);

	irq_set = (struct vfio_irq_set *) irq_set_buf;
	irq_set->argsz = len;
	irq_set->count = 0;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

	ret = ioctl(intr_handle->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);

	if (ret)
		RTE_LOG(ERR, EAL,
			"Error disabling MSI-X interrupts for fd %d\n", intr_handle->fd);

	return ret;
}
#endif

int
rte_intr_callback_register(struct rte_intr_handle *intr_handle,
			rte_intr_callback_fn cb, void *cb_arg)
{
	int ret, wake_thread;
	struct rte_intr_source *src;
	struct rte_intr_callback *callback;

	wake_thread = 0;

	/* first do parameter checking */
	if (intr_handle == NULL || intr_handle->fd < 0 || cb == NULL) {
		RTE_LOG(ERR, EAL,
			"Registering with invalid input parameter\n");
		return -EINVAL;
	}

	/* allocate a new interrupt callback entity */
	callback = rte_zmalloc("interrupt callback list",
				sizeof(*callback), 0);
	if (callback == NULL) {
		RTE_LOG(ERR, EAL, "Can not allocate memory\n");
		return -ENOMEM;
	}
	callback->cb_fn = cb;
	callback->cb_arg = cb_arg;

	rte_spinlock_lock(&intr_lock);

	/* check if there is at least one callback registered for the fd */
	TAILQ_FOREACH(src, &intr_sources, next) {
		if (src->intr_handle.fd == intr_handle->fd) {
			/* we had no interrupts for this */
			if TAILQ_EMPTY(&src->callbacks)
				wake_thread = 1;

			TAILQ_INSERT_TAIL(&(src->callbacks), callback, next);
			ret = 0;
			break;
		}
	}

	/* no existing callbacks for this - add new source */
	if (src == NULL) {
		if ((src = rte_zmalloc("interrupt source list",
				sizeof(*src), 0)) == NULL) {
			RTE_LOG(ERR, EAL, "Can not allocate memory\n");
			rte_free(callback);
			ret = -ENOMEM;
		} else {
			src->intr_handle = *intr_handle;
			TAILQ_INIT(&src->callbacks);
			TAILQ_INSERT_TAIL(&(src->callbacks), callback, next);
			TAILQ_INSERT_TAIL(&intr_sources, src, next);
			wake_thread = 1;
			ret = 0;
		}
	}

	rte_spinlock_unlock(&intr_lock);

	/**
	 * check if need to notify the pipe fd waited by epoll_wait to
	 * rebuild the wait list.
	 */
	if (wake_thread)
		if (write(intr_pipe.writefd, "1", 1) < 0)
			return -EPIPE;

	return (ret);
}

int
rte_intr_callback_unregister(struct rte_intr_handle *intr_handle,
			rte_intr_callback_fn cb_fn, void *cb_arg)
{
	int ret;
	struct rte_intr_source *src;
	struct rte_intr_callback *cb, *next;

	/* do parameter checking first */
	if (intr_handle == NULL || intr_handle->fd < 0) {
		RTE_LOG(ERR, EAL,
		"Unregistering with invalid input parameter\n");
		return -EINVAL;
	}

	rte_spinlock_lock(&intr_lock);

	/* check if the insterrupt source for the fd is existent */
	TAILQ_FOREACH(src, &intr_sources, next)
		if (src->intr_handle.fd == intr_handle->fd)
			break;

	/* No interrupt source registered for the fd */
	if (src == NULL) {
		ret = -ENOENT;

	/* interrupt source has some active callbacks right now. */
	} else if (src->active != 0) {
		ret = -EAGAIN;

	/* ok to remove. */
	} else {
		ret = 0;

		/*walk through the callbacks and remove all that match. */
		for (cb = TAILQ_FIRST(&src->callbacks); cb != NULL; cb = next) {

			next = TAILQ_NEXT(cb, next);

			if (cb->cb_fn == cb_fn && (cb_arg == (void *)-1 ||
					cb->cb_arg == cb_arg)) {
				TAILQ_REMOVE(&src->callbacks, cb, next);
				rte_free(cb);
				ret++;
			}
		}

		/* all callbacks for that source are removed. */
		if (TAILQ_EMPTY(&src->callbacks)) {
			TAILQ_REMOVE(&intr_sources, src, next);
			rte_free(src);
		}
	}

	rte_spinlock_unlock(&intr_lock);

	/* notify the pipe fd waited by epoll_wait to rebuild the wait list */
	if (ret >= 0 && write(intr_pipe.writefd, "1", 1) < 0) {
		ret = -EPIPE;
	}

	return (ret);
}

int
rte_intr_enable(struct rte_intr_handle *intr_handle)
{
	const int value = 1;

	if (!intr_handle || intr_handle->fd < 0)
		return -1;

	switch (intr_handle->type){
	/* write to the uio fd to enable the interrupt */
	case RTE_INTR_HANDLE_UIO:
		if (write(intr_handle->fd, &value, sizeof(value)) < 0) {
			RTE_LOG(ERR, EAL,
				"Error enabling interrupts for fd %d\n",
							intr_handle->fd);
			return -1;
		}
		break;
	/* not used at this moment */
	case RTE_INTR_HANDLE_ALARM:
		return -1;
#ifdef VFIO_PRESENT
	case RTE_INTR_HANDLE_VFIO_MSIX:
		if (vfio_enable_msix(intr_handle))
			return -1;
		break;
	case RTE_INTR_HANDLE_VFIO_MSI:
		if (vfio_enable_msi(intr_handle))
			return -1;
		break;
	case RTE_INTR_HANDLE_VFIO_LEGACY:
		if (vfio_enable_intx(intr_handle))
			return -1;
		break;
#endif
	/* unknown handle type */
	default:
		RTE_LOG(ERR, EAL,
			"Unknown handle type of fd %d\n",
					intr_handle->fd);
		return -1;
	}

	return 0;
}

int
rte_intr_disable(struct rte_intr_handle *intr_handle)
{
	const int value = 0;

	if (!intr_handle || intr_handle->fd < 0)
		return -1;

	switch (intr_handle->type){
	/* write to the uio fd to disable the interrupt */
	case RTE_INTR_HANDLE_UIO:
		if (write(intr_handle->fd, &value, sizeof(value)) < 0){
			RTE_LOG(ERR, EAL,
				"Error disabling interrupts for fd %d\n",
							intr_handle->fd);
			return -1;
		}
		break;
	/* not used at this moment */
	case RTE_INTR_HANDLE_ALARM:
		return -1;
#ifdef VFIO_PRESENT
	case RTE_INTR_HANDLE_VFIO_MSIX:
		if (vfio_disable_msix(intr_handle))
			return -1;
		break;
	case RTE_INTR_HANDLE_VFIO_MSI:
		if (vfio_disable_msi(intr_handle))
			return -1;
		break;
	case RTE_INTR_HANDLE_VFIO_LEGACY:
		if (vfio_disable_intx(intr_handle))
			return -1;
		break;
#endif
	/* unknown handle type */
	default:
		RTE_LOG(ERR, EAL,
			"Unknown handle type of fd %d\n",
					intr_handle->fd);
		return -1;
	}

	return 0;
}

static int
eal_intr_process_interrupts(struct epoll_event *events, int nfds)
{
	int n, bytes_read;
	struct rte_intr_source *src;
	struct rte_intr_callback *cb;
	union rte_intr_read_buffer buf;
	struct rte_intr_callback active_cb;

	for (n = 0; n < nfds; n++) {

		/**
		 * if the pipe fd is ready to read, return out to
		 * rebuild the wait list.
		 */
		if (events[n].data.fd == intr_pipe.readfd){
			int r = read(intr_pipe.readfd, buf.charbuf,
					sizeof(buf.charbuf));
			RTE_SET_USED(r);
			return -1;
		}
		rte_spinlock_lock(&intr_lock);
		TAILQ_FOREACH(src, &intr_sources, next)
			if (src->intr_handle.fd ==
					events[n].data.fd)
				break;
		if (src == NULL){
			rte_spinlock_unlock(&intr_lock);
			continue;
		}

		/* mark this interrupt source as active and release the lock. */
		src->active = 1;
		rte_spinlock_unlock(&intr_lock);

		/* set the length to be read dor different handle type */
		switch (src->intr_handle.type) {
		case RTE_INTR_HANDLE_UIO:
			bytes_read = sizeof(buf.uio_intr_count);
			break;
		case RTE_INTR_HANDLE_ALARM:
			bytes_read = sizeof(buf.timerfd_num);
			break;
#ifdef VFIO_PRESENT
		case RTE_INTR_HANDLE_VFIO_MSIX:
		case RTE_INTR_HANDLE_VFIO_MSI:
		case RTE_INTR_HANDLE_VFIO_LEGACY:
			bytes_read = sizeof(buf.vfio_intr_count);
			break;
#endif
		default:
			bytes_read = 1;
			break;
		}

		/**
		 * read out to clear the ready-to-be-read flag
		 * for epoll_wait.
		 */
		bytes_read = read(events[n].data.fd, &buf, bytes_read);

		if (bytes_read < 0)
			RTE_LOG(ERR, EAL, "Error reading from file "
				"descriptor %d: %s\n", events[n].data.fd,
							strerror(errno));
		else if (bytes_read == 0)
			RTE_LOG(ERR, EAL, "Read nothing from file "
				"descriptor %d\n", events[n].data.fd);

		/* grab a lock, again to call callbacks and update status. */
		rte_spinlock_lock(&intr_lock);

		if (bytes_read > 0) {

			/* Finally, call all callbacks. */
			TAILQ_FOREACH(cb, &src->callbacks, next) {

				/* make a copy and unlock. */
				active_cb = *cb;
				rte_spinlock_unlock(&intr_lock);

				/* call the actual callback */
				active_cb.cb_fn(&src->intr_handle,
					active_cb.cb_arg);

				/*get the lock back. */
				rte_spinlock_lock(&intr_lock);
			}
		}

		/* we done with that interrupt source, release it. */
		src->active = 0;
		rte_spinlock_unlock(&intr_lock);
	}

	return 0;
}

/**
 * It handles all the interrupts.
 *
 * @param pfd
 *  epoll file descriptor.
 * @param totalfds
 *  The number of file descriptors added in epoll.
 *
 * @return
 *  void
 */
static void
eal_intr_handle_interrupts(int pfd, unsigned totalfds)
{
	struct epoll_event events[totalfds];
	int nfds = 0;

	for(;;) {
		nfds = epoll_wait(pfd, events, totalfds,
			EAL_INTR_EPOLL_WAIT_FOREVER);
		/* epoll_wait fail */
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			RTE_LOG(ERR, EAL,
				"epoll_wait returns with fail\n");
			return;
		}
		/* epoll_wait timeout, will never happens here */
		else if (nfds == 0)
			continue;
		/* epoll_wait has at least one fd ready to read */
		if (eal_intr_process_interrupts(events, nfds) < 0)
			return;
	}
}

/**
 * It builds/rebuilds up the epoll file descriptor with all the
 * file descriptors being waited on. Then handles the interrupts.
 *
 * @param arg
 *  pointer. (unused)
 *
 * @return
 *  never return;
 */
static __attribute__((noreturn)) void *
eal_intr_thread_main(__rte_unused void *arg)
{
	struct epoll_event ev;

	/* host thread, never break out */
	for (;;) {
		/* build up the epoll fd with all descriptors we are to
		 * wait on then pass it to the handle_interrupts function
		 */
		static struct epoll_event pipe_event = {
			.events = EPOLLIN | EPOLLPRI,
		};
		struct rte_intr_source *src;
		unsigned numfds = 0;

		/* create epoll fd */
		int pfd = epoll_create(1);
		if (pfd < 0)
			rte_panic("Cannot create epoll instance\n");

		pipe_event.data.fd = intr_pipe.readfd;
		/**
		 * add pipe fd into wait list, this pipe is used to
		 * rebuild the wait list.
		 */
		if (epoll_ctl(pfd, EPOLL_CTL_ADD, intr_pipe.readfd,
						&pipe_event) < 0) {
			rte_panic("Error adding fd to %d epoll_ctl, %s\n",
					intr_pipe.readfd, strerror(errno));
		}
		numfds++;

		rte_spinlock_lock(&intr_lock);

		TAILQ_FOREACH(src, &intr_sources, next) {
			if (src->callbacks.tqh_first == NULL)
				continue; /* skip those with no callbacks */
			ev.events = EPOLLIN | EPOLLPRI;
			ev.data.fd = src->intr_handle.fd;

			/**
			 * add all the uio device file descriptor
			 * into wait list.
			 */
			if (epoll_ctl(pfd, EPOLL_CTL_ADD,
					src->intr_handle.fd, &ev) < 0){
				rte_panic("Error adding fd %d epoll_ctl, %s\n",
					src->intr_handle.fd, strerror(errno));
			}
			else
				numfds++;
		}
		rte_spinlock_unlock(&intr_lock);
		/* serve the interrupt */
		eal_intr_handle_interrupts(pfd, numfds);

		/**
		 * when we return, we need to rebuild the
		 * list of fds to monitor.
		 */
		close(pfd);
	}
}

int
rte_eal_intr_init(void)
{
	int ret = 0;

	/* init the global interrupt source head */
	TAILQ_INIT(&intr_sources);

	/**
	 * create a pipe which will be waited by epoll and notified to
	 * rebuild the wait list of epoll.
	 */
	if (pipe(intr_pipe.pipefd) < 0)
		return -1;

	/* create the host thread to wait/handle the interrupt */
	ret = pthread_create(&intr_thread, NULL,
			eal_intr_thread_main, NULL);
	if (ret != 0)
		RTE_LOG(ERR, EAL,
			"Failed to create thread for interrupt handling\n");

	return -ret;
}

