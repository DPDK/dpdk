.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2018 Cavium, Inc

Cavium OCTEON TX Crypto Poll Mode Driver
========================================

The OCTEON TX crypto poll mode driver provides support for offloading
cryptographic operations to cryptographic accelerator units on
**OCTEON TX** :sup:`®` family of processors (CN8XXX). The OCTEON TX crypto
poll mode driver enqueues the crypto request to this accelerator and dequeues
the response once the operation is completed.

Supported Symmetric Crypto Algorithms
-------------------------------------

Cipher Algorithms
~~~~~~~~~~~~~~~~~

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

Hash Algorithms
~~~~~~~~~~~~~~~

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

AEAD Algorithms
~~~~~~~~~~~~~~~

* ``RTE_CRYPTO_AEAD_AES_GCM``

Supported Asymmetric Crypto Algorithms
--------------------------------------

* ``RTE_CRYPTO_ASYM_XFORM_RSA``
* ``RTE_CRYPTO_ASYM_XFORM_MODEX``

Config flags
------------

For compiling the OCTEON TX crypto poll mode driver, please check if the
CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO setting is set to `y` in
config/common_base file.

* ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO=y``

Compilation
-----------

The OCTEON TX crypto poll mode driver can be compiled either natively on
**OCTEON TX** :sup:`®` board or cross-compiled on an x86 based platform.

Refer :doc:`../platform/octeontx` for details about setting up the platform
and building DPDK applications.

.. note::

   OCTEON TX crypto PF driver needs microcode to be available at `/lib/firmware/` directory.
   Refer SDK documents for further information.

SDK and related information can be obtained from: `Cavium support site <https://support.cavium.com/>`_.

Execution
---------

The number of crypto VFs to be enabled can be controlled by setting sysfs entry,
`sriov_numvfs`, for the corresponding PF driver.

.. code-block:: console

        echo <num_vfs> > /sys/bus/pci/devices/<dev_bus_id>/sriov_numvfs

The device bus ID, `dev_bus_id`, to be used in the above step can be found out
by using dpdk-devbind.py script. The OCTEON TX crypto PF device need to be
identified and the corresponding device number can be used to tune various PF
properties.


Once the required VFs are enabled, dpdk-devbind.py script can be used to
identify the VFs. To be accessible from DPDK, VFs need to be bound to vfio-pci
driver:

.. code-block:: console

        cd <dpdk directory>
        ./usertools/dpdk-devbind.py -u <vf device no>
        ./usertools/dpdk-devbind.py -b vfio-pci <vf device no>

Appropriate huge page need to be setup in order to run the DPDK example
applications.

.. code-block:: console

        echo 8 > /sys/kernel/mm/hugepages/hugepages-524288kB/nr_hugepages
        mkdir /mnt/huge
        mount -t hugetlbfs nodev /mnt/huge

Example applications can now be executed with crypto operations offloaded to
OCTEON TX crypto PMD.

.. code-block:: console

        ./build/ipsec-secgw --log-level=8 -c 0xff -- -P -p 0x3 -u 0x2 --config
        "(1,0,0),(0,0,0)" -f ep1.cfg

Testing
-------

The symmetric crypto operations on OCTEON TX crypto PMD may be verified by running the test
application:

.. code-block:: console

        ./test
        RTE>>cryptodev_octeontx_autotest

The asymmetric crypto operations on OCTEON TX crypto PMD may be verified by running the test
application:

.. code-block:: console

        ./test
        RTE>>cryptodev_octeontx_asym_autotest
