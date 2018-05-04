..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

NXP DPAA2 CMDIF Driver
======================

The DPAA2 CMDIF is an implementation of the rawdev API, that provides
communication between the GPP and AIOP (Firmware). This is achieved
via using the DPCI devices exposed by MC for GPP <--> AIOP interaction.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA2 CMDIF implements following features in the rawdev API;

- Getting the object ID of the device (DPCI) using attributes
- I/O to and from the AIOP device using DPCI

Supported DPAA2 SoCs
--------------------

- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

Prerequisites
-------------

There are three main pre-requisities for executing DPAA2 CMDIF on a DPAA2
compatible board:

1. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/6.3-2017.02/aarch64-linux-gnu>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://github.com/qoriq-open-source/linux>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `here <http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

As an alternative method, DPAA2 CMDIF can also be executed using images provided
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

- NXP SDK **2.0+**.
- MC Firmware version **10.0.0** and higher.
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

- ``CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV`` (default ``y``)

  Toggle compilation of the ``lrte_pmd_dpaa2_cmdif`` driver.

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_cmdif_application <EAL args> --log-level=pmd.raw.dpaa2.cmdif,<level>

Using ``pmd.raw.dpaa2.cmdif`` as log matching criteria, all Event PMD logs can be
enabled which are lower than logging ``level``.

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the DPAA2 CMDIF PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa2-linuxapp-gcc install

Initialization
--------------

The DPAA2 CMDIF is exposed as a vdev device which consists of dpci devices.
On EAL initialization, dpci devices will be probed and then vdev device
can be created from the application code by

* Invoking ``rte_vdev_init("dpaa2_dpci")`` from the application

* Using ``--vdev="dpaa2_dpci"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_cmdif_application <EAL args> --vdev="dpaa2_dpci"

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.
