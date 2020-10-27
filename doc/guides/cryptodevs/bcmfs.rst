..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2020 Broadcom

Broadcom FlexSparc Crypto Poll Mode Driver
==========================================

The FlexSparc crypto poll mode driver (BCMFS PMD) provides support for offloading
cryptographic operations to the Broadcom SoCs having FlexSparc4 unit.
Detailed information about SoCs can be found at `Broadcom Official Website
<https://www.broadcom.com/products/ethernet-connectivity/network-adapters/smartnic>`__.

Supported Broadcom SoCs
-----------------------

* Stingray

Features
--------

The BCMFS PMD has support for below symmetric algorithms:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_3DES_CTR``
* ``RTE_CRYPTO_CIPHER_AES128_CBC``
* ``RTE_CRYPTO_CIPHER_AES192_CBC``
* ``RTE_CRYPTO_CIPHER_AES256_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CTR``
* ``RTE_CRYPTO_CIPHER_AES192_CTR``
* ``RTE_CRYPTO_CIPHER_AES256_CTR``
* ``RTE_CRYPTO_CIPHER_AES_XTS``
* ``RTE_CRYPTO_CIPHER_DES_CBC``

Hash algorithms:

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
* ``RTE_CRYPTO_AUTH_AES_XCBC_MAC``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_AES_GMAC``
* ``RTE_CRYPTO_AUTH_AES_CMAC``

Supported AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``

Installation
------------
Information about kernel, rootfs and toolchain can be found at
`Broadcom Official Website <https://www.broadcom.com/products/ethernet-connectivity
/network-adapters/smartnic/stingray-software>`__.

    .. Note::
        To execute BCMFS PMD, it must be compiled with VFIO_PRESENT flag on the
        compiling platform and same gets enabled in rte_vfio.h.

The BCMFS PMD may be compiled natively on a Stingray platform or
cross-compiled on an x86 platform. For example, below commands can be executed
for cross compiling on x86 platform.

.. code-block:: console

    cd <DPDK-source-directory>
    meson <dest-dir> --cross-file config/arm/arm64_stingray_linux_gcc
    cd <dest-dir>
    ninja

Initialization
--------------
The supported platform devices should be present in the
*/sys/bus/platform/devices/fs<version>/<dev_name>* path on the booted kernel.
To get BCMFS PMD executing, device node must be owned by VFIO platform module only.
For example, below commands can be run to get hold of a device node by VFIO.

.. code-block:: console

    SETUP_SYSFS_DEV_NAME=67000000.crypto_mbox
    io_device_name="vfio-platform"
    echo $io_device_name > /sys/bus/platform/devices/${SETUP_SYSFS_DEV_NAME}/driver_override
    echo ${SETUP_SYSFS_DEV_NAME} > /sys/bus/platform/drivers_probe

Limitations
-----------

* The session-oriented APIs are supported but the session-less APIs are not.
* CCM is not supported.

Testing
-------

The symmetric crypto operations on BCMFS PMD may be verified by running the test
application:

.. code-block:: console

    ./dpdk-test
    RTE>>cryptodev_bcmfs_autotest
