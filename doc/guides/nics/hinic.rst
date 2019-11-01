..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Huawei Technologies Co., Ltd


HINIC Poll Mode Driver
======================

The hinic PMD (librte_pmd_hinic) provides poll mode driver support
for 25Gbps Huawei Intelligent PCIE Network Adapters based on the
Huawei Ethernet Controller Hi1822.


Features
--------

- Multi arch support: x86_64, ARMv8.
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- MAC/VLAN filtering
- Checksum offload
- TSO offload
- Promiscuous mode
- Port hardware statistics
- Link state information
- Link flow control
- Scattered and gather for TX and RX
- SR-IOV - Partially supported at this point, VFIO only
- VLAN filter and VLAN offload
- Allmulticast mode
- MTU update
- Unicast MAC filter
- Multicast MAC filter
- Flow API
- Set Link down or up
- FW version
- LRO

Prerequisites
-------------

- Learning about Huawei Hi1822 IN200 Series Intelligent NICs using
  `<https://e.huawei.com/en/products/cloud-computing-dc/servers/pcie-ssd/in-card>`_.

- Getting the latest product documents and software supports using
  `<https://support.huawei.com/enterprise/en/intelligent-accelerator-components/in500-solution-pid-23507369>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_HINIC_PMD`` (default ``y``)

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Limitations or Known issues
---------------------------
Build with ICC is not supported yet.
X86-32, Power8, ARMv7 and BSD are not supported yet.
