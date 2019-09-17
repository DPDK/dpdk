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

- ``Safe Mode Support`` (default ``0``)

  If driver failed to load OS package, by default driver's initialization failed.
  But if user intend to use the device without OS package, user can take ``devargs``
  parameter ``safe-mode-support``, for example::

    -w 80:00.0,safe-mode-support=1

  Then the driver will be initialized successfully and the device will enter Safe Mode.
  NOTE: In Safe mode, only very limited features are available, features like RSS,
  checksum, fdir, tunneling ... are all disabled.

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
stage.

The default DDP package file name is ice.pkg. For a specific NIC, the
DDP package supposed to be loaded can have a filename: ice-xxxxxx.pkg,
where 'xxxxxx' is the 64-bit PCIe Device Serial Number of the NIC. For
example, if the NIC's device serial number is 00-CC-BB-FF-FF-AA-05-68,
the device-specific DDP package filename is ice-00ccbbffffaa0568.pkg
(in hex and all low case). During initialization, the driver searches
in the following paths in order: /lib/firmware/updates/intel/ice/ddp
and /lib/firmware/intel/ice/ddp. The corresponding device-specific DDP
package will be downloaded first if the file exists. If not, then the
driver tries to load the default package. The type of loaded package
is stored in ``ice_adapter->active_pkg_type``.

A symbolic link to the DDP package file is also ok. The same package
file is used by both the kernel driver and the DPDK PMD.

19.02 limitation
~~~~~~~~~~~~~~~~

Ice code released in 19.02 is for evaluation only.
