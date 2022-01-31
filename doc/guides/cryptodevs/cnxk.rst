.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2021 Marvell.

Marvell cnxk Crypto Poll Mode Driver
====================================

The cnxk crypto poll mode driver provides support for offloading
cryptographic operations to cryptographic accelerator units on the
**Marvell OCTEON cnxk** SoC family.

The cnxk crypto PMD code is organized into different sets of files.
The file names starting with cn9k and cn10k provides support for CN9XX
and CN10XX respectively. The common code between the SoCs is present
in file names starting with cnxk.

More information about OCTEON cnxk SoCs may be obtained from `<https://www.marvell.com>`_

Supported OCTEON cnxk SoCs
--------------------------

- CN9XX
- CN10XX

Features
--------

The OCTEON cnxk crypto PMD has support for:

Symmetric Crypto Algorithms
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_NULL``
* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_3DES_ECB``
* ``RTE_CRYPTO_CIPHER_AES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_CTR``
* ``RTE_CRYPTO_CIPHER_AES_XTS``
* ``RTE_CRYPTO_CIPHER_DES_CBC``
* ``RTE_CRYPTO_CIPHER_KASUMI_F8``
* ``RTE_CRYPTO_CIPHER_SNOW3G_UEA2``
* ``RTE_CRYPTO_CIPHER_ZUC_EEA3``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_NULL``
* ``RTE_CRYPTO_AUTH_AES_GMAC``
* ``RTE_CRYPTO_AUTH_KASUMI_F9``
* ``RTE_CRYPTO_AUTH_MD5``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_SHA1``
* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_SNOW3G_UIA2``
* ``RTE_CRYPTO_AUTH_ZUC_EIA3``
* ``RTE_CRYPTO_AUTH_AES_CMAC``

AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``
* ``RTE_CRYPTO_AEAD_CHACHA20_POLY1305``

Asymmetric Crypto Algorithms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``RTE_CRYPTO_ASYM_XFORM_RSA``
* ``RTE_CRYPTO_ASYM_XFORM_MODEX``

Installation
------------

The OCTEON cnxk crypto PMD may be compiled natively on an OCTEON cnxk platform
or cross-compiled on an x86 platform.

Refer to :doc:`../platform/cnxk` for instructions to build your DPDK
application.

.. note::

   The OCTEON cnxk crypto PMD uses services from the kernel mode OCTEON cnxk
   crypto PF driver in linux. This driver is included in the OCTEON TX SDK.

Initialization
--------------

``CN9K Initialization``

List the CPT PF devices available on cn9k platform:

.. code-block:: console

    lspci -d:a0fd

``a0fd`` is the CPT PF device id. You should see output similar to:

.. code-block:: console

    0002:10:00.0 Class 1080: Device 177d:a0fd

Set ``sriov_numvfs`` on the CPT PF device, to create a VF:

.. code-block:: console

    echo 1 > /sys/bus/pci/devices/0002:10:00.0/sriov_numvfs

Bind the CPT VF device to the vfio_pci driver:

.. code-block:: console

    cd <dpdk directory>
    ./usertools/dpdk-devbind.py -u 0002:10:00.1
    ./usertools/dpdk-devbind.py -b vfio-pci 0002:10.00.1

.. note::

    * For CN98xx SoC, it is recommended to use even and odd DBDF VFs to achieve
      higher performance as even VF uses one crypto engine and odd one uses
      another crypto engine.

    * Ensure that sufficient huge pages are available for your application::

         dpdk-hugepages.py --setup 4G --pagesize 512M

      Refer to :ref:`linux_gsg_hugepages` for more details.

``CN10K Initialization``

List the CPT PF devices available on cn10k platform:

.. code-block:: console

    lspci -d:a0f2

``a0f2`` is the CPT PF device id. You should see output similar to:

.. code-block:: console

    0002:20:00.0 Class 1080: Device 177d:a0f2

Set ``sriov_numvfs`` on the CPT PF device, to create a VF:

.. code-block:: console

    echo 1 > /sys/bus/pci/devices/0002:20:00.0/sriov_numvfs

Bind the CPT VF device to the vfio_pci driver:

.. code-block:: console

    cd <dpdk directory>
    ./usertools/dpdk-devbind.py -u 0002:20:00.1
    ./usertools/dpdk-devbind.py -b vfio-pci 0002:20:00.1

Runtime Config Options
----------------------

- ``Maximum queue pairs limit`` (default ``63``)

   The number of maximum queue pairs supported by the device, can be limited
   during runtime by using ``max_qps_limit`` ``devargs`` parameter.

   For example::

      -a 0002:20:00.1,max_qps_limit=4

   With the above configuration, the number of maximum queue pairs supported
   by the device is limited to 4.

Debugging Options
-----------------

.. _table_octeon_cnxk_crypto_debug_options:

.. table:: OCTEON cnxk crypto PMD debug options

    +---+------------+-------------------------------------------------------+
    | # | Component  | EAL log command                                       |
    +===+============+=======================================================+
    | 1 | CPT        | --log-level='pmd\.crypto\.cnxk,8'                     |
    +---+------------+-------------------------------------------------------+

Testing
-------

The symmetric crypto operations on OCTEON cnxk crypto PMD may be verified by
running the test application:

``CN9K``

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_cn9k_autotest

``CN10K``

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_cn10k_autotest

The asymmetric crypto operations on OCTEON cnxk crypto PMD may be verified by
running the test application:

``CN9K``

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_cn9k_asym_autotest

``CN10K``

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_cn10k_asym_autotest

Lookaside IPsec Support
-----------------------

The OCTEON cnxk SoCs can accelerate IPsec traffic in lookaside protocol mode,
with its **cryptographic accelerator (CPT)**. ``OCTEON cnxk crypto PMD`` implements
this as an ``RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL`` offload.

Refer to :doc:`../prog_guide/rte_security` for more details on protocol offloads.

This feature can be tested with ipsec-secgw sample application.

Supported OCTEON cnxk SoCs
~~~~~~~~~~~~~~~~~~~~~~~~~~

- CN9XX
- CN10XX

CN9XX Features supported
~~~~~~~~~~~~~~~~~~~~~~~~

* IPv4
* IPv6
* ESP
* ESN
* Anti-replay
* Tunnel mode
* Transport mode(IPv4)
* UDP Encapsulation

AEAD algorithms
+++++++++++++++

* AES-128/192/256-GCM

Cipher algorithms
+++++++++++++++++

* NULL
* AES-128/192/256-CBC
* AES-128/192/256-CTR

Auth algorithms
+++++++++++++++

* SHA1-HMAC
* SHA256-128-HMAC
* SHA384-192-HMAC
* SHA512-256-HMAC
* AES-XCBC-96

CN10XX Features supported
~~~~~~~~~~~~~~~~~~~~~~~~~

* IPv4
* ESP
* ESN
* Anti-replay
* Tunnel mode
* Transport mode
* UDP Encapsulation

AEAD algorithms
+++++++++++++++

* AES-128/192/256-GCM

Cipher algorithms
+++++++++++++++++

* NULL
* AES-128/192/256-CBC
* AES-128/192/256-CTR

Auth algorithms
+++++++++++++++

* NULL
* SHA1-HMAC
* SHA256-128-HMAC
* SHA384-192-HMAC
* SHA512-256-HMAC
* AES-XCBC-96
