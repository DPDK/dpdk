..  SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
    Copyright 2018-2022 Advanced Micro Devices, Inc.

IONIC Driver
============

The ionic driver provides support for AMD Pensando server adapters.
It currently supports the below models:

- DSC-25 dual-port 25G Distributed Services Card `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKurUAG>`__
- DSC-100 dual-port 100G Distributed Services Card `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKuwUAG>`__

Please visit the AMD Pensando web site at https://www.amd.com/en/accelerators/pensando for more information.

Identifying the Adapter
-----------------------

To determine if one or more AMD Pensando DSC Ethernet devices are installed
on the host, check for the PCI devices:

   .. code-block:: console

      lspci -d 1dd8:
      b5:00.0 Ethernet controller: Device 1dd8:1002
      b6:00.0 Ethernet controller: Device 1dd8:1002

Firmware Support
----------------

The ionic PMD requires firmware which supports 16 segment transmit SGLs.
This support was added prior to version 1.0. For help upgrading older versions,
please contact AMD Pensando support.

Building DPDK
-------------

The ionic PMD supports UIO and VFIO. Please refer to the
:ref:`DPDK documentation that comes with the DPDK suite <linux_gsg>`
for instructions on how to build DPDK.
