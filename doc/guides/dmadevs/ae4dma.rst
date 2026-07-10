..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2026 Advanced Micro Devices, Inc.

.. include:: <isonum.txt>

AMD AE4DMA DMA Device Driver
============================

The ``ae4dma`` dmadev driver is a poll-mode driver (PMD) for the AMD AE4DMA hardware DMA engine.
The engine exposes 16 independent hardware command queues, each with a ring of 32 descriptors.
The PMD maps each hardware command queue to a separate DPDK dmadev with a single virtual channel,
so a single PCI function appears as 16 dmadevs named ``<pci-bdf>-ch0`` through ``<pci-bdf>-ch15``.

The driver supports memory-to-memory copy operations only.

Hardware Requirements
---------------------

The ``dpdk-devbind.py`` script can be used to list AE4DMA devices on the system::

   $ dpdk-devbind.py --status-dev dma

AE4DMA devices appear with vendor ID ``0x1022`` and device ID ``0x149b``.

Compilation
-----------

The driver is built as part of the standard DPDK build on x86 platforms using ``meson``
and ``ninja``; no extra configuration is required.

Device Setup
------------

The AE4DMA device must be bound to a DPDK-compatible kernel module such as ``vfio-pci``
before it can be used::

   $ dpdk-devbind.py -b vfio-pci <pci-bdf>

Initialization
~~~~~~~~~~~~~~

On probe the PMD performs the following steps for each PCI function:

* Reads BAR0 and programs the common configuration register
  with the number of hardware queues to enable (16).
* For each hardware queue it allocates a 32-entry descriptor ring in IOVA-contiguous memory,
  programs the queue base address and ring depth into the per-queue registers,
  and enables the queue.
* Interrupts are masked; completion is polled by the application.
