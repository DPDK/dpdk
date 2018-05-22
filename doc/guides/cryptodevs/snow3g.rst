..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

SNOW 3G Crypto Poll Mode Driver
===============================

The SNOW 3G PMD (**librte_pmd_snow3g**) provides poll mode crypto driver
support for utilizing Intel Libsso library, which implements F8 and F9 functions
for SNOW 3G UEA2 cipher and UIA2 hash algorithms.

Features
--------

SNOW 3G PMD has support for:

Cipher algorithm:

* RTE_CRYPTO_CIPHER_SNOW3G_UEA2

Authentication algorithm:

* RTE_CRYPTO_AUTH_SNOW3G_UIA2

Limitations
-----------

* Chained mbufs are not supported.
* SNOW 3G (UIA2) supported only if hash offset field is byte-aligned.
* In-place bit-level operations for SNOW 3G (UEA2) are not supported
  (if length and/or offset of data to be ciphered is not byte-aligned).

Installation
------------

To build DPDK with the SNOW3G_PMD the user is required to download
the export controlled ``libsso_snow3g`` library, by registering in
`Intel Resource & Design Center <https://www.intel.com/content/www/us/en/design/resource-design-center.html>`_.
Once approval has been granted, the user needs to search for
*Snow3G F8 F9 3GPP cryptographic algorithms Software Library* to download the
library or directly through this `link <https://cdrdv2.intel.com/v1/dl/getContent/575867>`_.
After downloading the library, the user needs to unpack and compile it
on their system before building DPDK::

   make snow3G

**Note**: When encrypting with SNOW3G UEA2, by default the library
encrypts blocks of 4 bytes, regardless the number of bytes to
be encrypted provided (which leads to a possible buffer overflow).
To avoid this situation, it is necessary not to pass
3GPP_SAFE_BUFFERS as a compilation flag.
For this, in the Makefile of the library, make sure that this flag
is commented out.::

  #EXTRA_CFLAGS  += -D_3GPP_SAFE_BUFFERS


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Export the environmental variable LIBSSO_SNOW3G_PATH with the path where
  the library was extracted (snow3g folder).

* Build the LIBSSO_SNOW3G library (explained in Installation section).

* Set CONFIG_RTE_LIBRTE_PMD_SNOW3G=y in config/common_base.

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_snow3g") within the application.

* Use --vdev="crypto_snow3g" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./l2fwd-crypto -l 1 -n 4 --vdev="crypto_snow3g,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_ONLY --cipher_algo "snow3g-uea2"
