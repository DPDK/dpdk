..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2021 NXP

NXP DPAA DMA Driver
===================

The DPAA DMA is an implementation of the dmadev APIs,
that provide means to initiate a DMA transaction from CPU.
The initiated DMA is performed without CPU being involved
in the actual DMA transaction.
This is achieved via using the QDMA controller of DPAA SoC.

The QDMA controller transfers blocks of data
between one source and one destination.
The blocks of data transferred can be represented in memory
as contiguous or noncontiguous using scatter/gather table(s).

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Supported DPAA SoCs
-------------------

- LS1046A
- LS1043A

Prerequisites
-------------

See :doc:`/platform/dpaa` for setup information

- Follow the DPDK :doc:`/linux_gsg/index` to setup the basic DPDK environment.

.. note::

   Some part of dpaa bus code (qbman and fman - library) routines are
   dual licensed (BSD & GPLv2), however they are used as BSD in DPDK in userspace.

Compilation
-----------

For builds using ``meson`` and ``ninja``, the driver will be built when the
target platform is dpaa-based. No additional compilation steps are necessary.

``RTE_DMA_DPAA_ERRATA_ERR050757``
  Enable software workaround for Errata-A050757
``RTE_DMA_DPAA_ERRATA_ERR050265``
  Enable software workaround for Errata-A050265

Initialization
--------------

On EAL initialization, DPAA DMA devices will be detected on DPAA bus and
will be probed and populated into their device list.

Features
--------

The DPAA DMA implements following features in the dmadev API:

- Supports 1 virtual channel.
- Supports all 4 DMA transfers: MEM_TO_MEM, MEM_TO_DEV,
  DEV_TO_MEM, DEV_TO_DEV.
- Supports DMA silent mode.
- Supports issuing DMA of data within memory without hogging CPU while
  performing DMA operation.
- Supports statistics.

Platform Requirement
--------------------

DPAA DMA driver for DPDK can only work on NXP SoCs
as listed in the `Supported DPAA SoCs`_.

Device Arguments
----------------

``dpaa_dma_err_check=1``
  Enable DMA error checking at driver level. When set, the driver reads
  the hardware error detect register after each dequeue and increments
  the error statistics counter on any reported error.
  Usage example: ``-a dpaa_bus:dpaa_qdma-1,dpaa_dma_err_check=1``

``dpaa_dma_sg_disable=1``
  Disable scatter-gather (SG) batching. By default the driver groups
  multiple pending descriptors into a single SG command to reduce
  doorbell overhead. Setting this devarg forces each descriptor to be
  submitted as an individual single-entry transfer. Useful for latency
  benchmarking or debugging.
  Usage example: ``-a dpaa_bus:dpaa_qdma-1,dpaa_dma_sg_disable=1``

``dpaa_dma_data_validation=1``
  Enable post-DMA data validation. After each completed transfer the
  driver reads back source and destination buffers and compares them
  byte by byte, logging any mismatch. This has significant performance
  impact and is intended for debugging only.
  Usage example: ``-a dpaa_bus:dpaa_qdma-1,dpaa_dma_data_validation=1``

``dpaa_dma_pci_read_disable=1``
  Disable the ERR050757 workaround programming. The workaround sets the
  read transaction type on the source frame descriptor and, for transfers
  longer than the errata safe length, programs the source stride so that
  read transactions stay within that limit. Only available when the driver
  is built with ``RTE_DMA_DPAA_ERRATA_ERR050757`` enabled. Disabling it may
  improve throughput on platforms not affected by the erratum.
  Usage example: ``-a dpaa_bus:dpaa_qdma-1,dpaa_dma_pci_read_disable=1``
