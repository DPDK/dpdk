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

Usage
-----

Once a dmadev has been started, copies are submitted with ``rte_dma_copy()`` and
completions are reaped with ``rte_dma_completed()`` or ``rte_dma_completed_status()``.
See the :ref:`Enqueue / Dequeue API <dmadev_enqueue_dequeue>` section of
the dmadev library documentation for details.

Limitations
-----------

* Only memory-to-memory copies are supported. Fill, scatter-gather and
  any other operation types are not advertised in ``rte_dma_info::dev_capa``.
* The maximum number of descriptors per virtual channel is fixed by hardware at 32,
  and the engine reserves one slot to distinguish a full ring from an empty one,
  so at most 31 operations can be outstanding (``rte_dma_burst_capacity()`` returns at most 31).
  The PMD rounds the requested ring size up to a power of two and clamps it to 32.
* Only a single virtual channel per dmadev is supported;
  use the 16 per-PCI-function dmadevs to obtain channel-level parallelism.
* Interrupt-driven completion is not supported.
