..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

NXP QorIQ DPAA2 Board Support Package
=====================================

This doc has information about steps to setup NXP QorIQ DPAA2 platform
and information about common offload hw block drivers of
**NXP QorIQ DPAA2** SoC family.

Supported DPAA2 SoCs
--------------------

- LX2160A
- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

More information about SoC can be found at `NXP Official Website
<https://www.nxp.com/products/processors-and-microcontrollers/arm-based-
processors-and-mcus/qoriq-layerscape-arm-processors:QORIQ-ARM>`_.


Common Offload HW Block Drivers
-------------------------------

1. **Nics Driver**

   See :doc:`../nics/dpaa2` for NXP dpaa2 nic driver information.

2. **Cryptodev Driver**

   See :doc:`../cryptodevs/dpaa2_sec` for NXP dpaa2 cryptodev driver information.

3. **Eventdev Driver**

   See :doc:`../eventdevs/dpaa2` for NXP dpaa2 eventdev driver information.

4. **Rawdev AIOP CMDIF Driver**

   See :doc:`../rawdevs/dpaa2_cmdif` for NXP dpaa2 AIOP command interface driver information.

5. **Rawdev QDMA Driver**

   See :doc:`../rawdevs/dpaa2_qdma` for NXP dpaa2 QDMA driver information.


Steps To Setup Platform
-----------------------

There are four main pre-requisites for executing DPAA2 PMD on a DPAA2
compatible board:

1. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/7.3-2018.05/aarch64-linux-gnu/gcc-linaro-7.3.1-2018.05-i686_aarch64-linux-gnu.tar.xz>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://source.codeaurora.org/external/qoriq/qoriq-components/linux>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 16.04 LTS (Xenial) or 18.04 (Bionic) userland which can be obtained
   from `here
   <http://cdimage.ubuntu.com/ubuntu-base/releases/18.04/release/ubuntu-base-18.04.1-base-arm64.tar.gz>`_.

4. **Resource Scripts**

   DPAA2 based resources can be configured easily with the help of ready scripts
   as provided in the DPDK Extra repository.

As an alternative method, DPAA2 PMD can also be executed using images provided
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


.. _extra_scripts:

- **DPDK Extra Scripts**

  DPAA2 based resources can be configured easily with the help of ready scripts
  as provided in the DPDK Extra repository.

  `DPDK Extras Scripts <https://source.codeaurora.org/external/qoriq/qoriq-components/dpdk-extras>`_.

Currently supported by DPDK:

- NXP SDK **2.0+** (preferred: LSDK 19.03).
- MC Firmware version **10.14.0** and higher.
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>`
  to setup the basic DPDK environment.
