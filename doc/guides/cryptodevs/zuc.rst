..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

ZUC Crypto Poll Mode Driver
===========================

The ZUC PMD (**librte_pmd_zuc**) provides poll mode crypto driver
support for utilizing Intel Libsso library, which implements F8 and F9 functions
for ZUC EEA3 cipher and EIA3 hash algorithms.

Features
--------

ZUC PMD has support for:

Cipher algorithm:

* RTE_CRYPTO_CIPHER_ZUC_EEA3

Authentication algorithm:

* RTE_CRYPTO_AUTH_ZUC_EIA3

Limitations
-----------

* Chained mbufs are not supported.
* ZUC (EIA3) supported only if hash offset field is byte-aligned.
* ZUC (EEA3) supported only if cipher length, cipher offset fields are byte-aligned.
* ZUC PMD cannot be built as a shared library, due to limitations in
  in the underlying library.


Installation
------------

To build DPDK with the ZUC_PMD the user is required to download
the export controlled ``libsso_zuc`` library, by registering in
`Intel Resource & Design Center <https://www.intel.com/content/www/us/en/design/resource-design-center.html>`_.
Once approval has been granted, the user needs to search for
*ZUC 128-EAA3 and 128-EIA3 3GPP cryptographic algorithms Software Library* to download the
library or directly through this `link <https://cdrdv2.intel.com/v1/dl/getContent/575868>`_.
After downloading the library, the user needs to unpack and compile it
on their system before building DPDK::

   make

Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Export the environmental variable LIBSSO_ZUC_PATH with the path where
  the library was extracted (zuc folder).

* Export the environmental variable LD_LIBRARY_PATH with the path
  where the built libsso library is (LIBSSO_ZUC_PATH/build).

* Build the LIBSSO_ZUC library (explained in Installation section).

* Build DPDK as follows:

.. code-block:: console

	make config T=x86_64-native-linux-gcc
	sed -i 's,\(CONFIG_RTE_LIBRTE_PMD_ZUC\)=n,\1=y,' build/.config
	make

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_zuc") within the application.

* Use --vdev="crypto_zuc" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./l2fwd-crypto -l 1 -n 4 --vdev="crypto_zuc,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_ONLY --cipher_algo "zuc-eea3"
