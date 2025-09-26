..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2025 Huawei Technologies Co., Ltd

HINIC3 Poll Mode Driver
=======================

The hinic3 PMD (**librte_net_hinic3**) provides poll mode driver support
for 25Gbps/100Gbps/200Gbps Huawei SPx series Network Adapters.

Features
--------

- Multi arch support: x86_64, ARMv8.
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- Flow filtering
- Checksum offload
- TSO offload
- Promiscuous mode
- Port hardware statistics
- Link state information
- Link flow control
- Scattered and gather for TX and RX
- Allmulticast mode
- MTU update
- Multicast MAC filter
- Flow API
- Set Link down or up
- VLAN filter and VLAN offload
- SR-IOV - Partially supported at this point, VFIO only
- FW version
- LRO

Prerequisites
-------------

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Limitations or Known issues
---------------------------
X86-32, Windows, and BSD are not supported yet.
