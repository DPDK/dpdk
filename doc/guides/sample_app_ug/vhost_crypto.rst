..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Intel Corporation.

Vhost_Crypto Sample Application
===============================

The vhost_crypto sample application implemented a simple Crypto device,
which used as the  backend of Qemu vhost-user-crypto device. Similar with
vhost-user-net and vhost-user-scsi device, the sample application used
domain socket to communicate with Qemu, and the virtio ring was processed
by vhost_crypto sample application.

Testing steps
-------------

This section shows the steps how to start a VM with the crypto device as
fast data path for critical application.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``examples`` sub-directory.

Start the vhost_crypto example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

    ./dpdk-vhost_crypto [EAL options] --
    		--config (lcore,cdev-id,queue-id)[,(lcore,cdev-id,queue-id)]
    		--socket-file lcore,PATH
    		[--zero-copy]
    		[--guest-polling]

where,

* config (lcore,cdev-id,queue-id): build the lcore-cryptodev id-queue id
  connection. Once specified, the specified lcore will only work with
  specified cryptodev's queue.

* socket-file lcore,PATH: the path of UNIX socket file to be created and
  the lcore id that will deal with the all workloads of the socket. Multiple
  instances of this config item is supported and one lcore supports processing
  multiple sockets.

* zero-copy: the presence of this item means the ZERO-COPY feature will be
  enabled. Otherwise it is disabled. PLEASE NOTE the ZERO-COPY feature is still
  in experimental stage and may cause the problem like segmentation fault. If
  the user wants to use LKCF in the guest, this feature shall be turned off.

* guest-polling: the presence of this item means the application assumes the
  guest works in polling mode, thus will NOT notify the guest completion of
  processing.

The application requires that crypto devices capable of performing
the specified crypto operation are available on application initialization.
This means that HW crypto device/s must be bound to a DPDK driver or
a SW crypto device/s (virtual crypto PMD) must be created (using --vdev).

.. _vhost_crypto_app_run_vm:

Start the VM
~~~~~~~~~~~~

.. code-block:: console

    qemu-system-x86_64 -machine accel=kvm \
        -m $mem -object memory-backend-file,id=mem,size=$mem,\
        mem-path=/dev/hugepages,share=on -numa node,memdev=mem \
        -drive file=os.img,if=none,id=disk \
        -device ide-hd,drive=disk,bootindex=0 \
        -chardev socket,id={chardev_id},path={PATH} \
        -object cryptodev-vhost-user,id={obj_id},chardev={chardev_id} \
        -device virtio-crypto-pci,id={dev_id},cryptodev={obj_id} \
        ...

.. note::
    You must check whether your Qemu can support "vhost-user-crypto" or not.
