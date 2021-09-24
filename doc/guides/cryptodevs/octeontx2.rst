..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.


Marvell OCTEON TX2 Crypto Poll Mode Driver
==========================================

The OCTEON TX2 crypto poll mode driver provides support for offloading
cryptographic operations to cryptographic accelerator units on the
**OCTEON TX2** :sup:`®` family of processors (CN9XXX).

More information about OCTEON TX2 SoCs may be obtained from `<https://www.marvell.com>`_

Features
--------

The OCTEON TX2 crypto PMD has support for:

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

AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``
* ``RTE_CRYPTO_AEAD_CHACHA20_POLY1305``

Asymmetric Crypto Algorithms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``RTE_CRYPTO_ASYM_XFORM_RSA``
* ``RTE_CRYPTO_ASYM_XFORM_MODEX``


Installation
------------

The OCTEON TX2 crypto PMD may be compiled natively on an OCTEON TX2 platform or
cross-compiled on an x86 platform.

Refer to :doc:`../platform/octeontx2` for instructions to build your DPDK
application.

.. note::

   The OCTEON TX2 crypto PMD uses services from the kernel mode OCTEON TX2
   crypto PF driver in linux. This driver is included in the OCTEON TX SDK.

Initialization
--------------

List the CPT PF devices available on your OCTEON TX2 platform:

.. code-block:: console

    lspci -d:a0fd

``a0fd`` is the CPT PF device id. You should see output similar to:

.. code-block:: console

    0002:10:00.0 Class 1080: Device 177d:a0fd

Set ``sriov_numvfs`` on the CPT PF device, to create a VF:

.. code-block:: console

    echo 1 > /sys/bus/pci/drivers/octeontx2-cpt/0002:10:00.0/sriov_numvfs

Bind the CPT VF device to the vfio_pci driver:

.. code-block:: console

    echo '177d a0fe' > /sys/bus/pci/drivers/vfio-pci/new_id
    echo 0002:10:00.1 > /sys/bus/pci/devices/0002:10:00.1/driver/unbind
    echo 0002:10:00.1 > /sys/bus/pci/drivers/vfio-pci/bind

Another way to bind the VF would be to use the ``dpdk-devbind.py`` script:

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

Debugging Options
-----------------

.. _table_octeontx2_crypto_debug_options:

.. table:: OCTEON TX2 crypto PMD debug options

    +---+------------+-------------------------------------------------------+
    | # | Component  | EAL log command                                       |
    +===+============+=======================================================+
    | 1 | CPT        | --log-level='pmd\.crypto\.octeontx2,8'                |
    +---+------------+-------------------------------------------------------+

Testing
-------

The symmetric crypto operations on OCTEON TX2 crypto PMD may be verified by running the test
application:

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_octeontx2_autotest

The asymmetric crypto operations on OCTEON TX2 crypto PMD may be verified by running the test
application:

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_octeontx2_asym_autotest


Lookaside IPsec Support
-----------------------

The OCTEON TX2 SoC can accelerate IPsec traffic in lookaside protocol mode,
with its **cryptographic accelerator (CPT)**. ``OCTEON TX2 crypto PMD`` implements
this as an ``RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL`` offload.

Refer to :doc:`../prog_guide/rte_security` for more details on protocol offloads.

This feature can be tested with ipsec-secgw sample application.


Features supported
~~~~~~~~~~~~~~~~~~

* IPv4
* IPv6
* ESP
* Tunnel mode
* Transport mode(IPv4)
* ESN
* Anti-replay
* UDP Encapsulation
* AES-128/192/256-GCM
* AES-128/192/256-CBC-SHA1-HMAC
* AES-128/192/256-CBC-SHA256-128-HMAC
