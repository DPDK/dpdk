..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.

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
- Generic flow API

Prerequisites
-------------

- Learning about Wangxun 10 Gigabit Ethernet NICs using
  `<https://www.net-swift.com/a/383.html>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Configuration
-------------

Compilation Options
~~~~~~~~~~~~~~~~~~~

The following build-time options may be enabled on build time using.

``-Dc_args=`` meson argument (e.g. ``-Dc_args=-DRTE_LIBRTE_TXGBE_DEBUG_RX``).

Please note that enabling debugging options may affect system performance.

- ``RTE_LIBRTE_TXGBE_DEBUG_RX`` (undefined by default)

  Toggle display of receive fast path run-time messages.

- ``RTE_LIBRTE_TXGBE_DEBUG_TX`` (undefined by default)

  Toggle display of transmit fast path run-time messages.

- ``RTE_LIBRTE_TXGBE_DEBUG_TX_FREE`` (undefined by default)

  Toggle display of transmit descriptor clean messages.

- ``RTE_LIBRTE_TXGBE_PF_DISABLE_STRIP_CRC`` (undefined by default)

  Decide to enable or disable HW CRC in VF PMD.

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

- ``pmd.net.txgbe.bp`` (default level is **notice**)

  Extra logging of auto-negotiation process for backplane NICs.
  Supply ``--log-level=pmd.net.txgbe.bp:debug`` to view messages.

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

The following ``devargs`` options can be enabled at runtime. They must
be passed as part of EAL arguments. For example,

.. code-block:: console

   dpdk-testpmd -a 01:00.0,auto_neg=1 -- -i

Please note that following ``devargs`` are only set for backplane NICs.

- ``auto_neg`` (default **1**)

  Toggle behavior to use auto-negotiation mode or force mode to
  link up backplane NICs.

- ``poll`` (default **0**)

  Toggle behavior to enable/disable polling mode to receive AN interrupt.

- ``present`` (default **0**)

  Toggle behavior to use present mode or init mode.

- ``sgmii`` (default **0**)

  Special treatment for KX SGMII cards.

- ``ffe_set`` (default **0**)

  Use to set PHY link mode and enable FFE parameters for user debugging.
  If disabled, the FFE parameters will not take effect. Otherwise, set 1
  for SFI mode, set 2 for KR mode, set 3 for KX4 mode, set 4 for KX mode.

- ``ffe_main`` (default **27**)

  PHY parameter used for user debugging. Setting other values to
  take effect requires setting the ``ffe_set``.

- ``ffe_pre`` (default **8**)

  PHY parameter used for user debugging. Setting other values to
  take effect requires setting the ``ffe_set``.

- ``ffe_post`` (default **44**)

  PHY parameter used for user debugging. Setting other values to
  take effect requires setting the ``ffe_set``.

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Sample Application Notes
------------------------

Generic flow API
~~~~~~~~~~~~~~~~

TXGBE PMD supports generic flow API which configures hardware to match specific
ingress or egress traffic, alter its fate and query related counters according
to any number of user-defined rules.

A flow rule is the combination of attributes with a matching pattern and a list of
actions. Theoretically one rule can match more than one filters, which named for
different patterns and actions. Like ethertype filter defines a rule in pattern:
the first not void item can be ETH, and the next not void item must be END.

For example, create a flow rule:

.. code-block:: console

	testpmd> flow create 0 ingress pattern eth type is 0x0806 / end actions queue index 2 / end

For a detailed usage description please refer to "Flow rules management" section in DPDK :doc:`Testpmd Runtime Functions <../testpmd_app_ug/testpmd_funcs>`.

Traffic Management API
~~~~~~~~~~~~~~~~~~~~~~

TXGBE PMD supports generic DPDK Traffic Management API which allows to
configure the following features: hierarchical scheduling, traffic shaping,
congestion management, packet marking.

For example, add shaper profile

.. code-block:: console

	testpmd> add port tm node shaper profile 0 0 0 0 25000000 0 0

For a detailed usage description please refer to "Traffic Management" section in DPDK :doc:`Testpmd Runtime Functions <../testpmd_app_ug/testpmd_funcs>`.

Limitations or Known issues
---------------------------

Build with ICC is not supported yet.
Power8, ARMv7 and BSD are not supported yet.
