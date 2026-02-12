.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021-2025 Advanced Micro Devices, Inc.

IONIC Crypto Driver
===================

The ionic crypto driver provides support for offloading cryptographic operations
to hardware cryptographic blocks on AMD Pensando server adapters.
It currently supports the below models:

- DSC-25 dual-port 25G Distributed Services Card
  `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKurUAG>`__
- DSC-100 dual-port 100G Distributed Services Card
  `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKuwUAG>`__
- DSC2-200 dual-port 200G Distributed Services Card
  `(pdf) <https://www.amd.com/content/dam/amd/en/documents/pensando-technical-docs/product-briefs/pensando-elba-product-brief.pdf>`__
- DSC3-400 dual-port 400G Distributed Services Card
  `(pdf) <https://www.amd.com/content/dam/amd/en/documents/pensando-technical-docs/product-briefs/pensando-dsc3-product-brief.pdf>`__
- Pollara 400 single-port 400G AI NIC
  `(pdf) <https://www.amd.com/content/dam/amd/en/documents/pensando-technical-docs/product-briefs/pollara-product-brief.pdf>`__

Please visit the
`AMD Pensando Networking
<https://www.amd.com/en/products/accelerators/pensando.html>`_
web site for more information.

Device Support
--------------

The ionic crypto driver currently supports
running directly on the device's embedded processors.
For help running the driver, please contact AMD Pensando support.

Limitations
-----------

- Host-side access via PCI is not yet supported
- Multiprocess applications are not yet supported
- Sessionless APIs are not yet supported

Runtime Configuration
---------------------

None

Features
--------

The ionic crypto PMD has support for:

Symmetric Crypto Algorithms
~~~~~~~~~~~~~~~~~~~~~~~~~~~

AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``
