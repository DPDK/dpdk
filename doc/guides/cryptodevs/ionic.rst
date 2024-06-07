.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021-2024 Advanced Micro Devices, Inc.

IONIC Crypto Driver
===================

The ionic crypto driver provides support for offloading cryptographic operations
to hardware cryptographic blocks on AMD Pensando server adapters.
It currently supports the below models:

- DSC-25 dual-port 25G Distributed Services Card
  `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKurUAG>`__
- DSC-100 dual-port 100G Distributed Services Card
  `(pdf) <https://pensandoio.secure.force.com/DownloadFile?id=a0L4T000004IKuwUAG>`__
- DSC-200 dual-port 200G Distributed Services Card
  `(pdf) <https://www.amd.com/system/files/documents/pensando-dsc-200-product-brief.pdf>`__

Please visit the AMD Pensando web site at
https://www.amd.com/en/accelerators/pensando for more information.

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
