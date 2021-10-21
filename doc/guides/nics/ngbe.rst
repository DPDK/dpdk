..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.

NGBE Poll Mode Driver
=====================

The NGBE PMD (librte_pmd_ngbe) provides poll mode driver support
for Wangxun 1 Gigabit Ethernet NICs.


Features
--------

- Multiple queues for Tx and Rx
- Receiver Side Scaling (RSS)
- MAC/VLAN filtering
- Packet type information
- Checksum offload
- VLAN/QinQ stripping and inserting
- TSO offload
- Promiscuous mode
- Multicast mode
- Port hardware statistics
- Jumbo frames
- Link state information
- Link flow control
- Scattered and gather for TX and RX
- IEEE 1588
- FW version


Prerequisites
-------------

- Learning about Wangxun 1 Gigabit Ethernet NICs using
  `<https://www.net-swift.com/a/386.html>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.


Pre-Installation Configuration
------------------------------

Dynamic Logging Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~

One may leverage EAL option "--log-level" to change default levels
for the log types supported by the driver. The option is used with
an argument typically consisting of two parts separated by a colon.

NGBE PMD provides the following log types available for control:

- ``pmd.net.ngbe.driver`` (default level is **notice**)

  Affects driver-wide messages unrelated to any particular devices.

- ``pmd.net.ngbe.init`` (default level is **notice**)

  Extra logging of the messages during PMD initialization.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


Limitations or Known issues
---------------------------

Build with ICC is not supported yet.
Power8, ARMv7 and BSD are not supported yet.
