.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2019 NXP

PFE Poll Mode Driver
======================

The PFE NIC PMD (**librte_net_pfe**) provides poll mode driver
support for the inbuilt NIC found in the **NXP LS1012** SoC.

More information can be found at `NXP Official Website
<https://nxp.com/ls1012a>`_.

This section provides an overview of the NXP PFE
and how it is integrated into the DPDK.

Contents summary

- PFE overview
- Supported PFE SoCs
- PFE features
- Prerequisites
- Driver compilation and testing
- Limitations

PFE Overview
------------

PFE is a hardware programmable packet forwarding engine to provide
high performance Ethernet interfaces. The diagram below shows a
system level overview of PFE:

.. code-block:: console

   ====================================================+===============
   US   +-----------------------------------------+    | Kernel Space
        |                                         |    |
        |           PFE Ethernet Driver           |    |
        +-----------------------------------------+    |
                  ^   |          ^     |               |
   PFE         RXQ|   |TXQ    RXQ|     |TXQ            |
   PMD            |   |          |     |               |
                  |   v          |     v               |   +----------+
               +---------+     +----------+            |   | pfe.ko   |
               | net_pfe0|     | net_pfe1 |            |   +----------+
               +---------+     +----------+            |
                  ^   |          ^     |               |
               TXQ|   |RXQ    TXQ|     |RXQ            |
                  |   |          |     |               |
                  |   v          |     v               |
                 +------------------------+            |
                 |                        |            |
                 |      PFE HIF driver    |            |
                 +------------------------+            |
                       ^         |                     |
                    RX |      TX |                     |
                   RING|     RING|                     |
                       |         v                     |
                     +--------------+                  |
                     |              |                  |
   ==================|    HIF       |==================+===============
         +-----------+              +--------------+
         |           |              |              |        HW
         |  PFE      +--------------+              |
         |       +-----+                +-----+    |
         |       | MAC |                | MAC |    |
         |       |     |                |     |    |
         +-------+-----+----------------+-----+----+
                 | PHY |                | PHY |
                 +-----+                +-----+


The HIF, PFE, MAC and PHY are the hardware blocks, the pfe.ko is a kernel
module, the PFE HIF driver and the PFE ethernet driver combined represent
as DPDK PFE poll mode driver are running in the userspace.

The PFE hardware supports one HIF (host interface) RX ring and one TX ring
to send and receive packets through packet forwarding engine. Both network
interface traffic is multiplexed and send over HIF queue.

net_pfe0 and net_pfe1 are logical ethernet interfaces, created by HIF client
driver. HIF driver is responsible for send and receive packets between
host interface and these logical interfaces. PFE ethernet driver is a
hardware independent and register with the HIF client driver to transmit and
receive packets from HIF via logical interfaces.

pfe.ko is required for PHY initialisation and also responsible for creating
the character device "pfe_us_cdev" which will be used for interacting with
the kernel layer for link status.

Supported PFE SoCs
------------------

- LS1012

PFE Features
------------

- L3/L4 checksum offload
- Packet type parsing
- Basic stats
- MTU update
- Promiscuous mode
- Allmulticast mode
- Link status
- ARMv8

Prerequisites
-------------

Below are some pre-requisites for executing PFE PMD on a PFE
compatible board:

#. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/7.3-2018.05/aarch64-linux-gnu/gcc-linaro-7.3.1-2018.05-i686_aarch64-linux-gnu.tar.xz>`_.

#. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://source.codeaurora.org/external/qoriq/qoriq-components/linux>`_.

#. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 16.04 LTS (Xenial) or 18.04 (Bionic) userland which can be obtained
   from `here <http://cdimage.ubuntu.com/ubuntu-base/releases/18.04/release/ubuntu-base-18.04.1-base-arm64.tar.gz>`_.

#. The ethernet device will be registered as virtual device, so pfe has dependency on
   **rte_bus_vdev** library and it is mandatory to use `--vdev` with value `net_pfe` to
   run DPDK application.

The following dependencies are not part of DPDK and must be installed
separately:

- **NXP Linux LSDK**

  NXP Layerscape software development kit (LSDK) includes support for family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  LSDK and related information can be obtained from:  `LSDK <https://www.nxp.com/support/developer-resources/run-time-software/linux-software-and-development-tools/layerscape-software-development-kit:LAYERSCAPE-SDK>`_

- **pfe kernel module**

  pfe kernel module can be obtained from NXP Layerscape software development kit at
  location `/lib/modules/<kernel version>/kernel/drivers/staging/fsl_ppfe` in rootfs.
  Module should be loaded using below command:

  .. code-block:: console

     insmod pfe.ko us=1


Driver compilation and testing
------------------------------

Follow instructions available in the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
to launch **testpmd**

Additionally, PFE driver needs `--vdev` as an input with value `net_pfe`
to execute DPDK application. There is an optional parameter `intf` available
to specify port ID. PFE driver supports only two interfaces, so valid values
for `intf` are 0 and 1.
see the command below:

 .. code-block:: console

    <dpdk app> <EAL args> --vdev="net_pfe0,intf=0" --vdev="net_pfe1,intf=1" -- ...


Limitations
-----------

- Multi buffer pool cannot be supported.
