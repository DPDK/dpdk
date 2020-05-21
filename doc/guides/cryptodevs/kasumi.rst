..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2019 Intel Corporation.

KASUMI Crypto Poll Mode Driver
===============================

The KASUMI PMD (**librte_pmd_kasumi**) provides poll mode crypto driver support for
utilizing `Intel IPSec Multi-buffer library <https://github.com/01org/intel-ipsec-mb>`_
which implements F8 and F9 functions for KASUMI UEA1 cipher and UIA1 hash algorithms.

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

To build DPDK with the KASUMI_PMD the user is required to download the multi-buffer
library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v0.54, which
can be downloaded from `<https://github.com/01org/intel-ipsec-mb/archive/v0.54.zip>`_.

After downloading the library, the user needs to unpack and compile it
on their system before building DPDK:

.. code-block:: console

    make
    make install

The library requires NASM to be built. Depending on the library version, it might
require a minimum NASM version (e.g. v0.54 requires at least NASM 2.14).

NASM is packaged for different OS. However, on some OS the version is too old,
so a manual installation is required. In that case, NASM can be downloaded from
`NASM website <https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D>`_.
Once it is downloaded, extract it and follow these steps:

.. code-block:: console

    ./configure
    make
    make install

.. note::

   Compilation of the Multi-Buffer library is broken when GCC < 5.0, if library <= v0.53.
   If a lower GCC version than 5.0, the workaround proposed by the following link
   should be used: `<https://github.com/intel/intel-ipsec-mb/issues/40>`_.

As a reference, the following table shows a mapping between the past DPDK versions
and the external crypto libraries supported by them:

.. _table_kasumi_versions:

.. table:: DPDK and external crypto library version compatibility

   =============  ================================
   DPDK version   Crypto library version
   =============  ================================
   16.11 - 19.11  LibSSO KASUMI
   20.02+         Multi-buffer library 0.53 - 0.54
   =============  ================================


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

* Build DPDK as follows:

.. code-block:: console

	make config T=x86_64-native-linux-gcc
	sed -i 's,\(CONFIG_RTE_LIBRTE_PMD_KASUMI\)=n,\1=y,' build/.config
	make


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
