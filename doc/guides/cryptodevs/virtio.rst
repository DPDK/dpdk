..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.

Virtio Crypto Poll Mode Driver
==============================

The virtio crypto PMD provides poll mode driver support for the virtio crypto
device.

Features
--------

The virtio crypto PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_AES_CBC``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1_HMAC``

Limitations
-----------

*  Only supports the session-oriented API implementation (session-less APIs are
   not supported).
*  Only supports modern mode since virtio crypto conforms to virtio-1.0.
*  Only has two types of queues: data queue and control queue. These two queues
   only support indirect buffers to communication with the virtio backend.
*  Only supports AES_CBC cipher only algorithm and AES_CBC with HMAC_SHA1
   chaining algorithm since the vhost crypto backend only these algorithms
   are supported.
*  Does not support Link State interrupt.
*  Does not support runtime configuration.

Virtio crypto PMD Rx/Tx Callbacks
---------------------------------

Rx callbacks:

* ``virtio_crypto_pkt_rx_burst``

Tx callbacks:

* ``virtio_crypto_pkt_tx_burst``

Installation
------------

Quick instructions are as follows:

Firstly run DPDK vhost crypto sample as a server side and build QEMU with
vhost crypto enabled.
QEMU can then be started using the following parameters:

.. code-block:: console

    qemu-system-x86_64 \
    [...] \
        -chardev socket,id=charcrypto0,path=/path/to/your/socket \
        -object cryptodev-vhost-user,id=cryptodev0,chardev=charcrypto0 \
        -device virtio-crypto-pci,id=crypto0,cryptodev=cryptodev0
    [...]

Secondly bind the uio_pci_generic driver for the virtio-crypto device.
For example, 0000:00:04.0 is the domain, bus, device and function
number of the virtio-crypto device:

.. code-block:: console

    modprobe uio_pci_generic
    echo -n 0000:00:04.0 > /sys/bus/pci/drivers/virtio-pci/unbind
    echo "1af4 1054" > /sys/bus/pci/drivers/uio_pci_generic/new_id

Finally the front-end virtio crypto PMD driver can be installed.

Tests
-----

The unit test cases can be tested as below:

.. code-block:: console

    reserve enough huge pages
    cd to <build_dir>
    meson test cryptodev_virtio_autotest

The performance can be tested as below:

.. code-block:: console

    reserve enough huge pages
    cd to <build_dir>

    ./app/dpdk-test-crypto-perf -l 0,1 -- --devtype crypto_virtio \
        --ptest throughput --optype cipher-then-auth --cipher-algo aes-cbc \
        --cipher-op encrypt --cipher-key-sz 16 --auth-algo sha1-hmac \
        --auth-op generate --auth-key-sz 64 --digest-sz 12 \
        --total-ops 100000000 --burst-sz 64 --buffer-sz 2048
