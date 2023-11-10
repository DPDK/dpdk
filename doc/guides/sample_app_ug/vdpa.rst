..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Vdpa Sample Application
=======================

The vdpa sample application creates vhost-user sockets by using the
vDPA backend. vDPA stands for vhost Data Path Acceleration which utilizes
virtio ring compatible devices to serve virtio driver directly to enable
datapath acceleration. As vDPA driver can help to set up vhost datapath,
this application doesn't need to launch dedicated worker threads for vhost
enqueue/dequeue operations.

Testing steps
-------------

This section shows the steps of how to start VMs with vDPA vhost-user
backend and verify network connection & live migration.

Build
~~~~~

To compile the sample application see :doc:`compiling`.

The application is located in the ``vdpa`` sub-directory.

Start the vdpa example
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

        ./dpdk-vdpa [EAL options]  -- [--client] [--interactive|-i] or [--iface SOCKET_PATH]

where

* --client means running vdpa app in client mode, in the client mode, QEMU needs
  to run as the server mode and take charge of socket file creation.
* --iface specifies the path prefix of the UNIX domain socket file, e.g.
  /tmp/vhost-user-, then the socket files will be named as /tmp/vhost-user-<n>
  (n starts from 0).
* --interactive means run the vDPA sample in interactive mode:

  #. help: show help message

  #. list: list all available vDPA devices

  #. create: create a new vDPA port with socket file and vDPA device address

  #. stats: show statistics of virtio queues

  #. quit: unregister vhost driver and exit the application

Take IFCVF driver for example:

.. code-block:: console

        ./dpdk-vdpa -c 0x2 -n 4 --socket-mem 1024,1024 \
                -a 0000:06:00.3,vdpa=1 -a 0000:06:00.4,vdpa=1 \
                -- --interactive

.. note::
    Here 0000:06:00.3 and 0000:06:00.4 refer to virtio ring compatible devices,
    and we need to bind vfio-pci to them before running vdpa sample.

    * modprobe vfio-pci
    * ./usertools/dpdk-devbind.py -b vfio-pci 06:00.3 06:00.4

Then we can create 2 vdpa ports in interactive cmdline.

.. code-block:: console

        vdpa> list
        device id       device address  queue num       supported features
        0               0000:06:00.3    1               0x14c238020
        1               0000:06:00.4    1               0x14c238020
        2               0000:06:00.5    1               0x14c238020

        vdpa> create /tmp/vdpa-socket0 0000:06:00.3
        vdpa> create /tmp/vdpa-socket1 0000:06:00.4

.. _vdpa_app_run_vm:

Start the VMs
~~~~~~~~~~~~~

.. code-block:: console

       qemu-system-x86_64 -cpu host -enable-kvm \
       <snip>
       -mem-prealloc \
       -chardev socket,id=char0,path=<socket_file created in above steps> \
       -netdev type=vhost-user,id=vdpa,chardev=char0 \
       -device virtio-net-pci,netdev=vdpa,mac=00:aa:bb:cc:dd:ee,page-per-vq=on \

After the VMs launches, we can login the VMs and configure the ip, verify the
network connection via ping or netperf.

.. note::
    Suggest to use QEMU 3.0.0 which extends vhost-user for vDPA.

Live Migration
~~~~~~~~~~~~~~
vDPA supports cross-backend live migration, user can migrate SW vhost backend
VM to vDPA backend VM and vice versa. Here are the detailed steps. Assume A is
the source host with SW vhost VM and B is the destination host with vDPA.

#. Start vdpa sample and launch a VM with exact same parameters as the VM on A,
   in migration-listen mode:

   .. code-block:: console

        B: <qemu-command-line> -incoming tcp:0:4444 (or other PORT))

#. Start the migration (on source host):

   .. code-block:: console

        A: (qemu) migrate -d tcp:<B ip>:4444 (or other PORT)

#. Check the status (on source host):

   .. code-block:: console

        A: (qemu) info migrate
