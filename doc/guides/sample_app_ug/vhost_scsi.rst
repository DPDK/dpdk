..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2017 Intel Corporation.

Vhost_scsi Sample Application
=============================

The vhost_scsi sample application implemented a simple SCSI block device,
which used as the  backend of Qemu vhost-user-scsi device. Users can extend
the exist example to use other type of block device(e.g. AIO) besides
memory based block device. Similar with vhost-user-net device, the sample
application used domain socket to communicate with Qemu, and the virtio
ring was processed by vhost_scsi sample application.

The sample application reuse lots codes from SPDK(Storage Performance
Development Kit, https://github.com/spdk/spdk) vhost-user-scsi target,
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

Start the vhost_scsi example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

        ./vhost_scsi -m 1024

.. _vhost_scsi_app_run_vm:

Start the VM
~~~~~~~~~~~~

.. code-block:: console

    qemu-system-x86_64 -machine accel=kvm \
        -m $mem -object memory-backend-file,id=mem,size=$mem,\
        mem-path=/dev/hugepages,share=on -numa node,memdev=mem \
        -drive file=os.img,if=none,id=disk \
        -device ide-hd,drive=disk,bootindex=0 \
        -chardev socket,id=char0,path=/tmp/vhost.socket \
        -device vhost-user-scsi-pci,chardev=char0,bootindex=2 \
        ...

.. note::
    You must check whether your Qemu can support "vhost-user-scsi" or not,
    Qemu v2.10 or newer version is required.

Vhost_scsi Common Issues
------------------------

* vhost_scsi can not start with block size 512 Bytes:

  Currently DPDK vhost library was designed for NET device(althrough the APIs
  are generic now), for 512 Bytes block device, Qemu BIOS(x86 BIOS Enhanced
  Disk Device) will enumerate all block device and do some IOs to those block
  devices with 512 Bytes sector size. DPDK vhost library can not process such
  scenarios(both BIOS and OS will enumerate the block device), so as a
  workaround, the vhost_scsi example application hardcoded the block size
  with 4096 Bytes.

* vhost_scsi can only support the block device as fast data disk(non OS image):

  Make sure ``bootindex=2`` Qemu option is given to vhost-user-scsi-pci device.

