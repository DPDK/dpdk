..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 HiSilicon Limited.

HNS3 Poll Mode Driver
===============================

The hns3 PMD (**librte_net_hns3**) provides poll mode driver support
for the inbuilt HiSilicon Network Subsystem(HNS) network engine
found in the HiSilicon Kunpeng 920 SoC and Kunpeng 930 SoC .

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
- IEEE1588/802.1AS timestamping

Prerequisites
-------------
- Get the information about Kunpeng920 chip using
  `<https://www.hisilicon.com/en/products/Kunpeng>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.


Runtime Config Options
----------------------

- ``rx_func_hint`` (default ``none``)

  Used to select Rx burst function, supported value are ``vec``, ``sve``,
  ``simple``, ``common``.
  ``vec``, if supported use the ``vec`` Rx function which indicates the
  default vector algorithm, neon for Kunpeng Arm platform.
  ``sve``, if supported use the ``sve`` Rx function which indicates the
  sve algorithm.
  ``simple``, if supported use the ``simple`` Rx function which indicates
  the scalar simple algorithm.
  ``common``, if supported use the ``common`` Rx function which indicates
  the scalar scattered algorithm.

  When provided parameter is not supported, ``vec`` usage condition will
  be first checked, if meets, use the ``vec``. Then, ``simple``, at last
  ``common``.

- ``tx_func_hint`` (default ``none``)

  Used to select Tx burst function, supported value are ``vec``, ``sve``,
  ``simple``, ``common``.
  ``vec``, if supported use the ``vec`` Tx function which indicates the
  default vector algorithm, neon for Kunpeng Arm platform.
  ``sve``, if supported use the ``sve`` Tx function which indicates the
  sve algorithm.
  ``simple``, if supported use the ``simple`` Tx function which indicates
  the scalar simple algorithm.
  ``common``, if supported use the ``common`` Tx function which indicates
  the scalar algorithm.

  When provided parameter is not supported, ``vec`` usage condition will
  be first checked, if meets, use the ``vec``. Then, ``simple``, at last
  ``common``.

- ``dev_caps_mask`` (default ``0``)

  Used to mask the capability which queried from firmware.
  This args take hexadecimal bitmask where each bit represents whether mask
  corresponding capability. eg. If the capability is 0xFFFF queried from
  firmware, and the args value is 0xF which means the bit0~bit3 should be
  masked off, then the capability will be 0xFFF0.
  Its main purpose is to debug and avoid problems.

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
