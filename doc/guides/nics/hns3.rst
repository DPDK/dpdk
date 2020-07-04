..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Hisilicon Limited.

HNS3 Poll Mode Driver
===============================

The hns3 PMD (librte_pmd_hns3) provides poll mode driver support
for the inbuilt Hisilicon Network Subsystem(HNS) network engine
found in the Hisilicon Kunpeng 920 SoC.

Features
--------

Features of the HNS3 PMD are:

- Multiple queues for TX and RX
- Receive Side Scaling (RSS)
- Packet type information
- Checksum offload
- TSO offload
- LRO offload
- Promiscuous mode
- Multicast mode
- Port hardware statistics
- Jumbo frames
- Link state information
- Interrupt mode for RX
- VLAN stripping and inserting
- QinQ inserting
- DCB
- Scattered and gather for TX and RX
- Flow director
- Dump register
- SR-IOV VF
- Multi-process
- MAC/VLAN filter
- MTU update
- NUMA support

Prerequisites
-------------
- Get the information about Kunpeng920 chip using
  `<http://www.hisilicon.com/en/Products/ProductList/Kunpeng>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_HNS3_PMD`` (default ``y``)

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Limitations or Known issues
---------------------------
Currently, we only support VF device is bound to vfio_pci or
igb_uio and then driven by DPDK driver when PF is driven by
kernel mode hns3 ethdev driver, VF is not supported when PF
is driven by DPDK driver.

Build with ICC is not supported yet.
X86-32, Power8, ARMv7 and BSD are not supported yet.
