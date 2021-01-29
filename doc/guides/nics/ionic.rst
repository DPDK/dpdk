..  SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
    Copyright(c) 2018-2020 Pensando Systems, Inc. All rights reserved.

IONIC Driver
============

The ionic driver provides support for Pensando server adapters.
It currently supports the below models:

- DSC-25 dual-port 25G Distributed Services Card `(pdf) <https://pensando.io/wp-content/uploads/2020/03/Pensando-DSC-25-Product-Brief.pdf>`__
- DSC-100 dual-port 100G Distributed Services Card `(pdf) <https://pensando.io/wp-content/uploads/2020/03/Pensando-DSC-100-Product-Brief.pdf>`__

Please visit the Pensando web site at https://pensando.io for more information.
The `Documents <https://pensando.io/documents/>`_ page contains Product Briefs and other product information.

Identifying the Adapter
-----------------------

To determine if one or more Pensando DSC Ethernet devices are installed
on the host, check for the PCI devices:

   .. code-block:: console

      lspci -d 1dd8:
      b5:00.0 Ethernet controller: Device 1dd8:1002
      b6:00.0 Ethernet controller: Device 1dd8:1002

Firmware Support
----------------

The ionic PMD requires firmware which supports 16 segment transmit SGLs.
This support was added prior to version 1.0. For help upgrading older versions,
please contact Pensando support.

Building DPDK
-------------

The ionic PMD supports UIO and VFIO. Please refer to the
:ref:`DPDK documentation that comes with the DPDK suite <linux_gsg>`
for instructions on how to build DPDK.
