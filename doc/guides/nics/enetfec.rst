.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 NXP

ENETFEC Poll Mode Driver
========================

The ENETFEC NIC PMD (**librte_net_enetfec**) provides poll mode driver
support for the inbuilt NIC found in the ** NXP i.MX 8M Mini** SoC.

More information can be found at NXP Official Website
<https://www.nxp.com/products/processors-and-microcontrollers/arm-processors/i-mx-applications-processors/i-mx-8-processors/i-mx-8m-mini-arm-cortex-a53-cortex-m4-audio-voice-video:i.MX8MMINI>

This section provides an overview of the NXP ENETFEC
and how it is integrated into the DPDK.
Driver is taken as **experimental**
as driver depends on a Linux kernel module 'enetfec-uio',
which is not upstreamed yet.

Contents summary

- ENETFEC overview
- Supported ENETFEC SoCs
- ENETFEC features
- Prerequisites
- Driver compilation and testing
- Limitations

ENETFEC Overview
----------------

The i.MX 8M Mini Media Applications Processor is built
to achieve both high performance and low power consumption.
ENETFEC PMD is a hardware programmable packet forwarding engine
to provide high performance Ethernet interface.
It has only 1 GB Ethernet interface with RJ45 connector.

The diagram below shows a system level overview of ENETFEC:

  .. code-block:: console

   =====================================================
   Userspace
        +-----------------------------------------+
        |             ENETFEC Driver              |
        |        +-------------------------+      |
        |        | virtual ethernet device |      |
        +-----------------------------------------+
                          ^   |
                          |   |
                          |   |
                     RXQ  |   |	TXQ
                          |   |
                          |   v
   =====================================================
   Kernel Space
                       +---------+
                       | fec-uio |
   ====================+=========+======================
   Hardware
        +-----------------------------------------+
        |           i.MX 8M MINI EVK              |
        |               +-----+                   |
        |               | MAC |                   |
        +---------------+-----+-------------------+
                        | PHY |
                        +-----+

ENETFEC Ethernet driver is traditional DPDK PMD running in userspace.
'fec-uio' is the kernel driver.
The MAC and PHY are the hardware blocks.
ENETFEC PMD uses standard UIO interface to access kernel
for PHY initialisation and for mapping the allocated memory
of register & buffer descriptor with DPDK
which gives access to non-cacheable memory for buffer descriptor.
net_enetfec is logical Ethernet interface, created by ENETFEC driver.

- ENETFEC driver registers the device in virtual device driver.
- RTE framework scans and will invoke the probe function of ENETFEC driver.
- The probe function will set the basic device registers and also setups BD rings.
- On packet Rx the respective BD Ring status bit is set which is then used for
  packet processing.
- Then Tx is done first followed by Rx via logical interfaces.

Supported ENETFEC SoCs
----------------------

- i.MX 8M Mini

ENETFEC Features
----------------

- Basic stats
- Promiscuous
- VLAN offload
- L3/L4 checksum offload
- Linux
- ARMv8

Prerequisites
-------------

There are three main pre-requisites for executing ENETFEC PMD on a i.MX 8M Mini
compatible board:

1. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain
   <https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/aarch64-linux-gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting
   <https://source.codeaurora.org/external/qoriq/qoriq-components/linux>`_.

.. note::

   Branch is 'lf-5.10.y'

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used.
   For example, Ubuntu 18.04 LTS (Bionic) or 20.04 LTS(Focal) userland
   which can be obtained from `here
   <http://cdimage.ubuntu.com/ubuntu-base/releases/18.04/release/ubuntu-base-18.04.1-base-arm64.tar.gz>`_.

4. The Ethernet device will be registered as virtual device,
   so ENETFEC has dependency on **rte_bus_vdev** library
   and it is mandatory to use `--vdev` with value `net_enetfec`
   to run DPDK application.

Driver compilation and testing
------------------------------

Follow instructions available in the document :doc:`build_and_test`
to launch **dpdk-testpmd**.

Limitations
-----------

- Multi queue is not supported.
