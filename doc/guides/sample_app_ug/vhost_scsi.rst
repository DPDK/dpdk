
..  BSD LICENSE
    Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
    All rights reserved.

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

Build
~~~~~

Follow the *Getting Started Guide for Linux* on generic info about
environment setup and building DPDK from source.

In this example, you need build DPDK both on the host and inside guest.
Also, you need build this example.

.. code-block:: console

    export RTE_SDK=/path/to/dpdk_source
    export RTE_TARGET=x86_64-native-linuxapp-gcc

    cd ${RTE_SDK}/examples/vhost_scsi
    make


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

