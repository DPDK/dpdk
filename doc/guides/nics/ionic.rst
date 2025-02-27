..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018-2022 Advanced Micro Devices, Inc.

IONIC Driver
============

The ionic driver provides support for AMD Pensando server adapters.
It currently supports the below models:

- DSC-25 dual-port 25G Distributed Services Card
  `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKurUAG>`__
- DSC-100 dual-port 100G Distributed Services Card
  `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKuwUAG>`__
- DSC2-200 dual-port 200G Distributed Services Card
  `(pdf) <https://www.amd.com/content/dam/amd/en/documents/pensando-technical-docs/product-briefs/pensando-elba-product-brief.pdf>`__
- DSC3-400 dual-port 400G Distributed Services Card
  `(pdf) <https://www.amd.com/content/dam/amd/en/documents/pensando-technical-docs/product-briefs/pensando-dsc3-product-brief.pdf>`__

Please visit the
`AMD Pensando Networking
<https://www.amd.com/en/products/accelerators/pensando.html>`_
web site for more information.

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

Runtime Configuration
---------------------

- ``Queue in CMB support`` (default ``0``)

  Queue memory can be allocated from the Controller Memory Buffer (CMB) using
  the ``ionic_cmb`` ``devargs`` parameter.

  For example::

    -a 0000:b5:00.0,ionic_cmb=1

Building DPDK
-------------

The ionic PMD supports UIO and VFIO. Please refer to the
:ref:`DPDK documentation that comes with the DPDK suite <linux_gsg>`
for instructions on how to build DPDK.
