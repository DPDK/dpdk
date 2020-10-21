..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2017 Intel Corporation.

Vhost_blk Sample Application
=============================

The vhost_blk sample application implemented a simple block device,
which used as the  backend of Qemu vhost-user-blk device. Users can extend
the exist example to use other type of block device(e.g. AIO) besides
memory based block device. Similar with vhost-user-net device, the sample
application used domain socket to communicate with Qemu, and the virtio
ring (split or packed format) was processed by vhost_blk sample application.

The sample application reuse lots codes from SPDK(Storage Performance
Development Kit, https://github.com/spdk/spdk) vhost-user-blk target,
for DPDK vhost library used in storage area, user can take SPDK as
reference as well.

Testing steps
-------------

This section shows the steps how to start a VM with the block device as
fast data path for critical application.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``examples`` sub-directory.

You will also need to build DPDK both on the host and inside the guest

Start the vhost_blk example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

        ./dpdk-vhost_blk -m 1024

.. _vhost_blk_app_run_vm:

Start the VM
~~~~~~~~~~~~

.. code-block:: console

    qemu-system-x86_64 -machine accel=kvm \
        -m $mem -object memory-backend-file,id=mem,size=$mem,\
        mem-path=/dev/hugepages,share=on -numa node,memdev=mem \
        -drive file=os.img,if=none,id=disk \
        -device ide-hd,drive=disk,bootindex=0 \
        -chardev socket,id=char0,reconnect=1,path=/tmp/vhost.socket \
        -device vhost-user-blk-pci,packed=on,chardev=char0,num-queues=1 \
        ...

.. note::
    You must check whether your Qemu can support "vhost-user-blk" or not,
    Qemu v4.0 or newer version is required.
    reconnect=1 means live recovery support that qemu can reconnect vhost_blk
    after we restart vhost_blk example.
    packed=on means the device support packed ring but need the guest kernel
    version >= 5.0.
    Now Qemu commit 9bb73502321d46f4d320fa17aa38201445783fc4 both support the
    vhost-blk reconnect and packed ring.
