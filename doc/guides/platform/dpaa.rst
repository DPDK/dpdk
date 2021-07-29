..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

NXP QorIQ DPAA Board Support Package
====================================

This doc has information about steps to setup QorIQ dpaa
based layerscape platform and information about common offload
hw block drivers of **NXP QorIQ DPAA** SoC family.

Supported DPAA SoCs
--------------------

* LS1046A/LS1026A
* LS1043A/LS1023A

More information about SoC can be found at `NXP Official Website
<https://www.nxp.com/products/processors-and-microcontrollers/arm-based-
processors-and-mcus/qoriq-layerscape-arm-processors:QORIQ-ARM>`_.


Common Offload HW Block Drivers
-------------------------------

1. **Nics Driver**

   See :doc:`../nics/dpaa` for NXP dpaa nic driver information.

2. **Cryptodev Driver**

   See :doc:`../cryptodevs/dpaa_sec` for NXP dpaa cryptodev driver information.

3. **Eventdev Driver**

   See :doc:`../eventdevs/dpaa` for NXP dpaa eventdev driver information.


Steps To Setup Platform
-----------------------

There are four main pre-requisites for executing DPAA PMD on a DPAA
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

4. **FMC Tool**

   If one is planning to use more than 1 Recv queue and hardware capability to
   parse, classify and distribute the packets, the Frame Manager Configuration
   Tool (FMC) need to be executed to set the configurations of the queues before
   running the DPAA based DPDK application. This setting is persistent, the
   configuration will remain in the hardware till it is re-configured. This
   includes the queue state, RSS and other policies.
   This tool can be obtained from `NXP (Freescale) Public Git Repository <https://source.codeaurora.org/external/qoriq/qoriq-components/fmc>`_.

   This tool needs configuration files which are available in the
   :ref:`DPDK Extra Scripts <extra_scripts>`, described below for DPDK usages.

   Note that DPAA PMD can also be executed using images provided
   as part of SDK from NXP. The SDK includes all the above prerequisites
   necessary (i.e. fmc tool) to bring up a DPAA board.

   As an alternate method, DPAA PMDs starting from DPDK 20.11 also support the
   fmlib library integration. The driver will detect about any existing FMC
   based config (if /tmp/fmc.bin is present). DPAA FMD will be used only if no
   previous fmc config is existing.

   Note that fmlib based integration rely on underlying fmd driver in kernel,
   which is available as part of NXP kernel or NXP SDK.

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

  DPAA based resources can be configured easily with the help of ready scripts
  as provided in the DPDK Extra repository.

  `DPDK Extras Scripts <https://source.codeaurora.org/external/qoriq/qoriq-components/dpdk-extras>`_.

Currently supported by DPDK:

- NXP SDK **2.0+** (preferred: LSDK 18.09).
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>`
  to setup the basic DPDK environment.
