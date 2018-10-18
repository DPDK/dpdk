..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

AXGBE Poll Mode Driver
======================

The AXGBE poll mode driver library (**librte_pmd_axgbe**) implements support
for AMD 10 Gbps family of adapters. It is compiled and tested in standard linux distro like Ubuntu.

Detailed information about SoCs that use these devices can be found here:

- `AMD EPYC™ EMBEDDED 3000 family <https://www.amd.com/en/products/embedded-epyc-3000-series>`_.


Supported Features
------------------

AXGBE PMD has support for:

- Base L2 features
- TSS (Transmit Side Scaling)
- Promiscuous mode
- Port statistics
- Multicast mode
- RSS (Receive Side Scaling)
- Checksum offload
- Jumbo Frame up to 9K


Configuration Information
-------------------------

The following options can be modified in the ``.config`` file. Please note that
enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_AXGBE_PMD`` (default **y**)

  Toggle compilation of axgbe PMD.

- ``CONFIG_RTE_LIBRTE_AXGBE_PMD_DEBUG`` (default **n**)

  Toggle display for PMD debug related messages.


Building DPDK
-------------

See the :ref:`DPDK Getting Started Guide for Linux <linux_gsg>` for
instructions on how to build DPDK.

By default the AXGBE PMD library will be built into the DPDK library.

For configuring and using UIO frameworks, please also refer :ref:`the
documentation that comes with DPDK suite <linux_gsg>`.


Prerequisites and Pre-conditions
--------------------------------
- Prepare the system as recommended by DPDK suite.

- Bind the intended AMD device to ``igb_uio`` or ``vfio-pci`` module.

Now system is ready to run DPDK application.


Usage Example
-------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Example output:

.. code-block:: console

   [...]
   EAL: PCI device 0000:02:00.4 on NUMA socket 0
   EAL:   probe driver: 1022:1458 net_axgbe
   Interactive-mode selected
   USER1: create a new mbuf pool <mbuf_pool_socket_0>: n=171456, size=2176, socket=0
   USER1: create a new mbuf pool <mbuf_pool_socket_1>: n=171456, size=2176, socket=1
   USER1: create a new mbuf pool <mbuf_pool_socket_2>: n=171456, size=2176, socket=2
   USER1: create a new mbuf pool <mbuf_pool_socket_3>: n=171456, size=2176, socket=3
   Configuring Port 0 (socket 0)
   Port 0: 00:00:1A:1C:6A:17
   Checking link statuses...
   Port 0 Link Up - speed 10000 Mbps - full-duplex
   Done
   testpmd>
