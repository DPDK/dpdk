/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Kevin Bowling
 */

#ifndef NIC_UIO_H
#define NIC_UIO_H

#include <sys/ioccom.h>

/*
 * Optional single-vector MSI-X interface. INIT reserves vector zero for this
 * open file description; other opens may still mmap the BARs. Repeating INIT
 * on the owner is harmless. Closing the owner releases the interrupt.
 *
 * ENABLE installs the handler and unmasks the vector; DISABLE removes the
 * handler and masks the vector. Neither operation acknowledges device-specific
 * interrupt causes: that remains the PMD's responsibility.
 *
 * EVFILT_READ reports unread interrupts. read() returns a native uint32_t
 * cumulative event count, or EAGAIN when there is no unread event. Counts wrap.
 * An older module rejects INIT with ENOTTY and continues to support BAR mmap.
 * All interrupt-control ioctls require a writable descriptor.
 *
 * The first writable open enables bus mastering; the last writable close
 * disables it, including process exit. dup() and fork() retain that open file
 * description. A BAR mapping alone does not keep DMA enabled: applications
 * must keep a writable descriptor open while using the device for DMA.
 */
#define NIC_UIO_IRQ_INIT		_IO('N', 1)
#define NIC_UIO_IRQ_ENABLE	_IO('N', 2)
#define NIC_UIO_IRQ_DISABLE	_IO('N', 3)

#endif /* NIC_UIO_H */
