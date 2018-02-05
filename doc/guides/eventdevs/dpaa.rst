.. SPDX-License-Identifier:        BSD-3-Clause
   Copyright 2017 NXP

NXP DPAA Eventdev Driver
=========================

The dpaa eventdev is an implementation of the eventdev API, that provides a
wide range of the eventdev features. The eventdev relies on a dpaa based
platform to perform event scheduling.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA EVENTDEV implements many features in the eventdev API;

- Hardware based event scheduler
- 4 event ports
- 4 event queues
- Parallel flows
- Atomic flows

Supported DPAA SoCs
--------------------

- LS1046A
- LS1043A

Prerequisites
-------------

There are following pre-requisites for executing EVENTDEV on a DPAA compatible
platform:

1. **ARM 64 Tool Chain**

  For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/6.4-2017.08/aarch64-linux-gnu/>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://github.com/qoriq-open-source/linux>`_.

3. **Rootfile System**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `here <http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

As an alternative method, DPAA EVENTDEV can also be executed using images provided
as part of SDK from NXP. The SDK includes all the above prerequisites necessary
to bring up a DPAA board.

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

  DPAA based resources can be configured easily with the help of ready to use
  xml files as provided in the DPDK Extra repository.

  `DPDK Extras Scripts <https://github.com/qoriq-open-source/dpdk-extras>`_.

Currently supported by DPDK:

- NXP SDK **2.0+** or LSDK **17.09+**
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_PMD_DPAA_EVENTDEV`` (default ``y``)

  Toggle compilation of the ``librte_pmd_dpaa_event`` driver.

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the DPAA EVENTDEV PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa-linuxapp-gcc install

Initialization
--------------

The dpaa eventdev is exposed as a vdev device which consists of a set of channels
and queues. On EAL initialization, dpaa components will be
probed and then vdev device can be created from the application code by

* Invoking ``rte_vdev_init("event_dpaa1")`` from the application

* Using ``--vdev="event_dpaa1"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_eventdev_application --vdev="event_dpaa1"

Limitations
-----------

1. DPAA eventdev can not work with DPAA PUSH mode queues configured for ethdev.
   Please configure export DPAA_NUM_PUSH_QUEUES=0

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA SoCs``.

Port-core Binding
~~~~~~~~~~~~~~~~~

DPAA EVENTDEV driver requires event port 'x' to be used on core 'x'.
