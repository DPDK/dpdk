..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

ICE Poll Mode Driver
======================

The ice PMD (librte_pmd_ice) provides poll mode driver support for
10/25 Gbps Intel® Ethernet 810 Series Network Adapters based on
the Intel Ethernet Controller E810.


Prerequisites
-------------

- Identifying your adapter using `Intel Support
  <http://www.intel.com/support>`_ and get the latest NVM/FW images.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

- To get better performance on Intel platforms, please follow the "How to get best performance with NICs on Intel platforms"
  section of the :ref:`Getting Started Guide for Linux <linux_gsg>`.


Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_ICE_PMD`` (default ``y``)

  Toggle compilation of the ``librte_pmd_ice`` driver.

- ``CONFIG_RTE_LIBRTE_ICE_DEBUG_*`` (default ``n``)

  Toggle display of generic debugging messages.

Runtime Config Options
~~~~~~~~~~~~~~~~~~~~~~

- ``Maximum Number of Queue Pairs``

  The maximum number of queue pairs is decided by HW. If not configured, APP
  uses the number from HW. Users can check the number by calling the API
  ``rte_eth_dev_info_get``.
  If users want to limit the number of queues, they can set a smaller number
  using EAL parameter like ``max_queue_pair_num=n``.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


Limitations or Known issues
---------------------------

19.02 limitation
~~~~~~~~~~~~~~~~

Ice code released in 19.02 is for evaluation only.


Promiscuous mode not supported
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
As promiscuous mode is not supported as this stage, a port can only receive the
packets which destination MAC address is this port's own.


TX anti-spoofing cannot be disabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
TX anti-spoofing is enabled by default. At this stage it's not supported to
disable it. So any TX packet which source MAC address is not this port's own
will be dropped by HW. It means io-fwd is not supported now. Recommand to use
MAC-fwd for evaluation.
