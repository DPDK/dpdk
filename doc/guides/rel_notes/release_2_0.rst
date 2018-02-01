..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

DPDK Release 2.0
================


New Features
------------

*   Poll-mode driver support for an early release of the PCIE host interface of the Intel(R) Ethernet Switch FM10000.

    *   Basic Rx/Tx functions for PF/VF

    *   Interrupt handling support for PF/VF

    *   Per queue start/stop functions for PF/VF

    *   Support Mailbox handling between PF/VF and PF/Switch Manager

    *   Receive Side Scaling (RSS) for PF/VF

    *   Scatter receive function for PF/VF

    *   Reta update/query for PF/VF

    *   VLAN filter set for PF

    *   Link status query for PF/VF

.. note:: The software is intended to run on pre-release hardware and may contain unknown or unresolved defects or
          issues related to functionality and performance.
          The poll mode driver is also pre-release and will be updated to a released version post hardware and base driver release.
          Should the official hardware release be made between DPDK releases an updated poll-mode driver will be made available.

*   Link Bonding

    *   Support for adaptive load balancing (mode 6) to the link bonding library.

    *   Support for registration of link status change callbacks with link bonding devices.

    *   Support for slaves devices which do not support link status change interrupts in the link bonding library via a link status polling mechanism.

*   PCI Hotplug with NULL PMD sample application

*   ABI versioning

*   x32 ABI

*   Non-EAL Thread Support

*   Multi-pthread Support

*   Re-order Library

*   ACL for AVX2

*   Architecture Independent CRC Hash

*   uio_pci_generic Support

*   KNI Optimizations

*   Vhost-user support

*   Virtio (link, vlan, mac, port IO, perf)

*   IXGBE-VF RSS

*   RX/TX Callbacks

*   Unified Flow Types

*   Indirect Attached MBUF Flag

*   Use default port configuration in TestPMD

*   Tunnel offloading in TestPMD

*   Poll Mode Driver - 40 GbE Controllers (librte_pmd_i40e)

    *   Support for Flow Director

    *   Support for ethertype filter

    *   Support RSS in VF

    *   Support configuring redirection table with different size from 1GbE and 10 GbE

       -   128/512 entries of 40GbE PF

       -   64 entries of 40GbE VF

    *   Support configuring hash functions

    *   Support for VXLAN packet on Intel® 40GbE Controllers

*   Poll Mode Driver for Mellanox ConnectX-3 EN adapters (mlx4)

.. note:: This PMD is only available for Linux and is disabled by default
          due to external dependencies (libibverbs and libmlx4). Please
          refer to the NIC drivers guide for more information.

*   Packet Distributor Sample Application

*   Job Stats library and Sample Application.

*   Enhanced Jenkins hash (jhash) library

.. note:: The hash values returned by the new jhash library are different
          from the ones returned by the previous library.
