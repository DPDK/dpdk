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

See :doc:`../platform/dpaa` for setup information

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

.. note::

   Some part of dpaa bus code (qbman and fman - library) routines are
   dual licensed (BSD & GPLv2), however they are used as BSD in DPDK in userspace.

Compilation
-----------

For builds using ``meson`` and ``ninja``, the driver will be built when the
target platform is dpaa-based. No additional compilation steps are necessary.

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
