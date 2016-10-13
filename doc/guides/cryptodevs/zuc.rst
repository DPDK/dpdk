..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
the export controlled ``libsso_zuc`` library, by requesting it from
`<https://networkbuilders.intel.com/network-technologies/dpdk>`_.
Once approval has been granted, the user needs to log in
`<https://networkbuilders.intel.com/dpdklogin>`_
and click on "ZUC Library" link, to download the library.
After downloading the library, the user needs to unpack and compile it
on their system before building DPDK::

   make

Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Export the environmental variable LIBSSO_ZUC_PATH with the path where
  the library was extracted (zuc folder).

* Build the LIBSSO_ZUC library (explained in Installation section).

* Build DPDK as follows:

.. code-block:: console

	make config T=x86_64-native-linuxapp-gcc
	sed -i 's,\(CONFIG_RTE_LIBRTE_PMD_ZUC\)=n,\1=y,' build/.config
	make

To use the PMD in an application, user must:

* Call rte_eal_vdev_init("crypto_zuc") within the application.

* Use --vdev="crypto_zuc" in the EAL options, which will call rte_eal_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./l2fwd-crypto -c 40 -n 4 --vdev="crypto_zuc,socket_id=1,max_nb_sessions=128"
