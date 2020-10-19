..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2020.

TXGBE Poll Mode Driver
======================

The TXGBE PMD (librte_pmd_txgbe) provides poll mode driver support
for Wangxun 10 Gigabit Ethernet NICs.

Features
--------

- Multiple queues for TX and RX
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
- Interrupt mode for RX
- Scattered and gather for TX and RX
- DCB
- IEEE 1588
- FW version
- LRO

Prerequisites
-------------

- Learning about Wangxun 10 Gigabit Ethernet NICs using
  `<https://www.net-swift.com/a/383.html>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Build Options
~~~~~~~~~~~~~

The following build-time options may be enabled on build time using.

``-Dc_args=`` meson argument (e.g. ``-Dc_args=-DRTE_LIBRTE_TXGBE_DEBUG_RX``).

Please note that enabling debugging options may affect system performance.

- ``RTE_LIBRTE_TXGBE_DEBUG_RX`` (undefined by default)

  Toggle display of receive fast path run-time messages.

- ``RTE_LIBRTE_TXGBE_DEBUG_TX`` (undefined by default)

  Toggle display of transmit fast path run-time messages.

- ``RTE_LIBRTE_TXGBE_DEBUG_TX_FREE`` (undefined by default)

  Toggle display of transmit descriptor clean messages.

Dynamic Logging Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~

One may leverage EAL option "--log-level" to change default levels
for the log types supported by the driver. The option is used with
an argument typically consisting of two parts separated by a colon.

TXGBE PMD provides the following log types available for control:

- ``pmd.net.txgbe.driver`` (default level is **notice**)

  Affects driver-wide messages unrelated to any particular devices.

- ``pmd.net.txgbe.init`` (default level is **notice**)

  Extra logging of the messages during PMD initialization.

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Limitations or Known issues
---------------------------

Build with ICC is not supported yet.
Power8, ARMv7 and BSD are not supported yet.
