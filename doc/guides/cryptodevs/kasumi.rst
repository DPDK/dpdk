..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

KASUMI Crypto Poll Mode Driver
===============================

The KASUMI PMD (**librte_pmd_kasumi**) provides poll mode crypto driver
support for utilizing Intel Libsso library, which implements F8 and F9 functions
for KASUMI UEA1 cipher and UIA1 hash algorithms.

Features
--------

KASUMI PMD has support for:

Cipher algorithm:

* RTE_CRYPTO_CIPHER_KASUMI_F8

Authentication algorithm:

* RTE_CRYPTO_AUTH_KASUMI_F9

Limitations
-----------

* Chained mbufs are not supported.
* KASUMI(F9) supported only if hash offset and length field is byte-aligned.
* In-place bit-level operations for KASUMI(F8) are not supported
  (if length and/or offset of data to be ciphered is not byte-aligned).


Installation
------------

To build DPDK with the KASUMI_PMD the user is required to download
the export controlled ``libsso_kasumi`` library, by registering in
`Intel Resource & Design Center <https://www.intel.com/content/www/us/en/design/resource-design-center.html>`_.
Once approval has been granted, the user needs to search for
*Kasumi F8 F9 3GPP cryptographic algorithms Software Library* to download the
library or directly through this `link <https://cdrdv2.intel.com/v1/dl/getContent/575866>`_.
After downloading the library, the user needs to unpack and compile it
on their system before building DPDK::

   make

**Note**: When encrypting with KASUMI F8, by default the library
encrypts full blocks of 8 bytes, regardless the number of bytes to
be encrypted provided (which leads to a possible buffer overflow).
To avoid this situation, it is necessary not to pass
3GPP_SAFE_BUFFERS as a compilation flag.
Also, this is required when using chained operations
(cipher-then-auth/auth-then-cipher).
For this, in the Makefile of the library, make sure that this flag
is commented out::

  #EXTRA_CFLAGS  += -D_3GPP_SAFE_BUFFERS

**Note**: To build the PMD as a shared library, the libsso_kasumi
library must be built as follows::

  make KASUMI_CFLAGS=-DKASUMI_C


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Export the environmental variable LIBSSO_KASUMI_PATH with the path where
  the library was extracted (kasumi folder).

* Build the LIBSSO library (explained in Installation section).

* Set CONFIG_RTE_LIBRTE_PMD_KASUMI=y in config/common_base.

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_kasumi") within the application.

* Use --vdev="crypto_kasumi" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./l2fwd-crypto -l 1 -n 4 --vdev="crypto_kasumi,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_ONLY --cipher_algo "kasumi-f8"

Extra notes on KASUMI F9
------------------------

When using KASUMI F9 authentication algorithm, the input buffer must be
constructed according to the 3GPP KASUMI specifications (section 4.4, page 13):
`<http://cryptome.org/3gpp/35201-900.pdf>`_.
Input buffer has to have COUNT (4 bytes), FRESH (4 bytes), MESSAGE and DIRECTION (1 bit)
concatenated. After the DIRECTION bit, a single '1' bit is appended, followed by
between 0 and 7 '0' bits, so that the total length of the buffer is multiple of 8 bits.
Note that the actual message can be any length, specified in bits.

Once this buffer is passed this way, when creating the crypto operation,
length of data to authenticate (op.sym.auth.data.length) must be the length
of all the items described above, including the padding at the end.
Also, offset of data to authenticate (op.sym.auth.data.offset)
must be such that points at the start of the COUNT bytes.
