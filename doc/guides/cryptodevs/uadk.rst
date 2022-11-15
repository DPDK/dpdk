.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2022-2023 Huawei Technologies Co.,Ltd. All rights reserved.
   Copyright 2022-2023 Linaro ltd.

UADK Crypto Poll Mode Driver
============================

This code provides the initial implementation of the UADK poll mode driver.
All cryptographic operations are using UADK library crypto API,
which is algorithm level API, abstracting accelerators' low level implementations.

UADK crypto PMD relies on `UADK library <https://github.com/Linaro/uadk>`_.

UADK is a framework for user applications to access hardware accelerators.
UADK relies on IOMMU SVA (Shared Virtual Address) feature,
which share the same page table between IOMMU and MMU.
As a result, user application can directly use virtual address for device DMA,
which enhances the performance as well as easy usability.


Features
--------

UADK crypto PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_AES_ECB``
* ``RTE_CRYPTO_CIPHER_AES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_XTS``
* ``RTE_CRYPTO_CIPHER_DES_CBC``

Hash algorithms:

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

Test steps
----------


#. Build UADK

   .. code-block:: console

      git clone https://github.com/Linaro/uadk.git
      cd uadk
      mkdir build
      ./autogen.sh
      ./configure --prefix=$PWD/build
      make
      make install

   .. note::

      Without ``--prefix``, UADK will be installed to ``/usr/local/lib`` by default.

   .. note::

      If get error: "cannot find -lnuma", please install the libnuma-dev.

#. Run pkg-config libwd to ensure env is setup correctly

   .. code-block:: console

      export PKG_CONFIG_PATH=$PWD/build/lib/pkgconfig
      pkg-config libwd --cflags --libs -I/usr/local/include -L/usr/local/lib -lwd

   .. note::

      export ``PKG_CONFIG_PATH`` is required on demand,
      not needed if UADK is installed to ``/usr/local/lib``.

#. Build DPDK

   .. code-block:: console

      cd dpdk
      mkdir build
      meson setup build (--reconfigure)
      cd build
      ninja
      sudo ninja install

#. Prepare hugepages for DPDK (see also :doc:`../tools/hugepages`)

   .. code-block:: console

      echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
      echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
      echo 1024 > /sys/devices/system/node/node2/hugepages/hugepages-2048kB/nr_hugepages
      echo 1024 > /sys/devices/system/node/node3/hugepages/hugepages-2048kB/nr_hugepages
      mkdir -p /mnt/huge_2mb
      mount -t hugetlbfs none /mnt/huge_2mb -o pagesize=2MB

#. Run test app

   .. code-block:: console

      sudo dpdk-test --vdev=crypto_uadk --log-level=6
      RTE>>cryptodev_uadk_autotest
      RTE>>quit
