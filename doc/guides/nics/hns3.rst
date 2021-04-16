..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 HiSilicon Limited.

HNS3 Poll Mode Driver
===============================

The hns3 PMD (**librte_net_hns3**) provides poll mode driver support
for the inbuilt HiSilicon Network Subsystem(HNS) network engine
found in the HiSilicon Kunpeng 920 SoC.

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
- Vector Poll mode driver
- Dump register
- SR-IOV VF
- Multi-process
- MAC/VLAN filter
- MTU update
- NUMA support
- Generic flow API

Prerequisites
-------------
- Get the information about Kunpeng920 chip using
  `<https://www.hisilicon.com/en/products/Kunpeng>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.


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
