..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

NXP DPAA2 QDMA Driver
=====================

The DPAA2 QDMA is an implementation of the rawdev API, that provide means
to initiate a DMA transaction from CPU. The initiated DMA is performed
without CPU being involved in the actual DMA transaction. This is achieved
via using the DPDMAI device exposed by MC.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA2 QDMA implements following features in the rawdev API;

- Supports issuing DMA of data within memory without hogging CPU while
  performing DMA operation.
- Supports configuring to optionally get status of the DMA translation on
  per DMA operation basis.

Supported DPAA2 SoCs
--------------------

- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

Prerequisites
-------------

There are three main pre-requisities for executing DPAA2 QDMA on a DPAA2
compatible board:

1. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/6.3-2017.02/aarch64-linux-gnu>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://github.com/qoriq-open-source/linux>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `here <http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

As an alternative method, DPAA2 QDMA can also be executed using images provided
as part of SDK from NXP. The SDK includes all the above prerequisites necessary
to bring up a DPAA2 board.

The following dependencies are not part of DPDK and must be installed
separately:

- **NXP Linux SDK**

  NXP Linux software development kit (SDK) includes support for family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  SDK and related information can be obtained from:  `NXP QorIQ SDK  <http://www.nxp.com/products/software-and-tools/run-time-software/linux-sdk/linux-sdk-for-qoriq-processors:SDKLINUX>`_.

- **DPDK Extra Scripts**

  DPAA2 based resources can be configured easily with the help of ready scripts
  as provided in the DPDK Extra repository.

  `DPDK Extras Scripts <https://github.com/qoriq-open-source/dpdk-extras>`_.

Currently supported by DPDK:

- NXP LSDK **17.12+**.
- MC Firmware version **10.3.0** and higher.
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

.. note::

   Some part of fslmc bus code (mc flib - object library) routines are
   dual licensed (BSD & GPLv2).

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV`` (default ``y``)

  Toggle compilation of the ``lrte_pmd_dpaa2_qdma`` driver.

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_qdma_application <EAL args> --log-level=pmd.raw.dpaa2.qdma,<level>

Using ``pmd.raw.dpaa2.qdma`` as log matching criteria, all Event PMD logs can be
enabled which are lower than logging ``level``.

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the DPAA2 QDMA PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa2-linuxapp-gcc install

Initialization
--------------

The DPAA2 QDMA is exposed as a vdev device which consists of dpdmai devices.
On EAL initialization, dpdmai devices will be probed and populated into the
rawdevices. The rawdev ID of the device can be obtained using

* Invoking ``rte_rawdev_get_dev_id("dpdmai.x")`` from the application
  where x is the object ID of the DPDMAI object created by MC. Use can
  use this index for further rawdev function calls.

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.
