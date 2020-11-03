.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2018 NXP

ENETC Poll Mode Driver
======================

The ENETC NIC PMD (**librte_net_enetc**) provides poll mode driver
support for the inbuilt NIC found in the **NXP LS1028** SoC.

More information can be found at `NXP Official Website
<https://www.nxp.com/products/processors-and-microcontrollers/arm-based-processors-and-mcus/qoriq-layerscape-arm-processors/qoriq-layerscape-1028a-industrial-applications-processor:LS1028A>`_.

ENETC
-----

This section provides an overview of the NXP ENETC
and how it is integrated into the DPDK.

Contents summary

- ENETC overview
- ENETC features
- PCI bus driver
- NIC driver
- Supported ENETC SoCs
- Prerequisites
- Driver compilation and testing

ENETC Overview
~~~~~~~~~~~~~~

ENETC is a PCI Integrated End Point(IEP). IEP implements
peripheral devices in an SoC such that software sees them as PCIe device.
ENETC is an evolution of BDR(Buffer Descriptor Ring) based networking
IPs.

This infrastructure simplifies adding support for IEP and facilitates in following:

- Device discovery and location
- Resource requirement discovery and allocation (e.g. interrupt assignment,
  device register address)
- Event reporting

ENETC Features
~~~~~~~~~~~~~~

- Link Status
- Packet type information
- Basic stats
- Promiscuous
- Multicast
- Jumbo packets
- Queue Start/Stop
- Deferred Queue Start
- CRC offload

NIC Driver (PMD)
~~~~~~~~~~~~~~~~

ENETC PMD is traditional DPDK PMD which provides necessary interface between
RTE framework and ENETC internal drivers.

- Driver registers the device vendor table in PCI subsystem.
- RTE framework scans the PCI bus for connected devices.
- This scanning will invoke the probe function of ENETC driver.
- The probe function will set the basic device registers and also setups BD rings.
- On packet Rx the respective BD Ring status bit is set which is then used for
  packet processing.
- Then Tx is done first followed by Rx.

Supported ENETC SoCs
~~~~~~~~~~~~~~~~~~~~

- LS1028

Prerequisites
~~~~~~~~~~~~~

There are three main pre-requisites for executing ENETC PMD on a ENETC
compatible board:

1. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/7.3-2018.05/aarch64-linux-gnu/gcc-linaro-7.3.1-2018.05-i686_aarch64-linux-gnu.tar.xz>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://source.codeaurora.org/external/qoriq/qoriq-components/linux>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 16.04 LTS (Xenial) or 18.04 (Bionic) userland which can be obtained
   from `here <http://cdimage.ubuntu.com/ubuntu-base/releases/18.04/release/ubuntu-base-18.04.1-base-arm64.tar.gz>`_.

The following dependencies are not part of DPDK and must be installed
separately:

- **NXP Linux LSDK**

  NXP Layerscape software development kit (LSDK) includes support for family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  LSDK and related information can be obtained from:  `LSDK <https://www.nxp.com/support/developer-resources/run-time-software/linux-software-and-development-tools/layerscape-software-development-kit:LAYERSCAPE-SDK>`_

Driver compilation and testing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Follow instructions available in the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
to launch **testpmd**
