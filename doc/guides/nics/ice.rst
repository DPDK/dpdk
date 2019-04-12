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

- ``CONFIG_RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC`` (default ``y``)

  Toggle bulk allocation for RX.

- ``CONFIG_RTE_LIBRTE_ICE_16BYTE_RX_DESC`` (default ``n``)

  Toggle to use a 16-byte RX descriptor, by default the RX descriptor is 32 byte.

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

Features
--------

Vector PMD
~~~~~~~~~~

Vector PMD for RX and TX path are selected automatically. The paths
are chosen based on 2 conditions.

- ``CPU``
  On the X86 platform, the driver checks if the CPU supports AVX2.
  If it's supported, AVX2 paths will be chosen. If not, SSE is chosen.

- ``Offload features``
  The supported HW offload features are described in the document ice_vec.ini.
  If any not supported features are used, ICE vector PMD is disabled and the
  normal paths are chosen.

Malicious driver detection (MDD)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It's not appropriate to send a packet, if this packet's destination MAC address
is just this port's MAC address. If SW tries to send such packets, HW will
report a MDD event and drop the packets.

The APPs based on DPDK should avoid providing such packets.

Sample Application Notes
------------------------

Vlan filter
~~~~~~~~~~~

Vlan filter only works when Promiscuous mode is off.

To start ``testpmd``, and add vlan 10 to port 0:

.. code-block:: console

    ./app/testpmd -l 0-15 -n 4 -- -i
    ...

    testpmd> rx_vlan add 10 0

Limitations or Known issues
---------------------------

The Intel E810 requires a programmable pipeline package be downloaded
by the driver to support normal operations. The E810 has a limited
functionality built in to allow PXE boot and other use cases, but the
driver must download a package file during the driver initialization
stage. The file must be in the /lib/firmware/intel/ice/ddp directory
and it must be named ice.pkg. A symbolic link to this file is also ok.
The same package file is used by both the kernel driver and the DPDK PMD.


19.02 limitation
~~~~~~~~~~~~~~~~

Ice code released in 19.02 is for evaluation only.
