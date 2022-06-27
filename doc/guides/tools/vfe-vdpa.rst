..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2022, NVIDIA CORPORATION & AFFILIATES.

dpdk-vfe-vdpa Application
=======================

The ``dpdk-vfe-vdpa`` tool is a Data Plane Development Kit (DPDK)
utility that creates vhost-user sockets by using the
vDPA backend. vDPA stands for vhost Data Path Acceleration which utilizes
virtio ring compatible devices to serve virtio driver directly to enable
datapath acceleration. As vDPA driver can help to set up vhost datapath,
this application doesn't need to launch dedicated worker threads for vhost
enqueue/dequeue operations.

Running the Application
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

        ./dpdk-vfe-vdpa [EAL options]  -- [--client] [--interactive|-i] or [--iface SOCKET_PATH]

where

* --client means running vdpa app in client mode, in the client mode, QEMU needs
  to run as the server mode and take charge of socket file creation.
* --iface specifies the path prefix of the UNIX domain socket file, e.g.
  /tmp/vhost-user-, then the socket files will be named as /tmp/vhost-user-<n>
  (n starts from 0).
* --interactive means run the vdpa sample in interactive mode, currently 4
  internal cmds are supported:

  1. help: show help message
  2. list: list all available vdpa devices
  3. create: create a new vdpa port with socket file and vdpa device address
  4. stats: show statistics of virtio queues
  5. quit: unregister vhost driver and exit the application

Take IFCVF driver for example:

.. code-block:: console

        ./dpdk-vfe-vdpa -c 0x2 -n 4 --socket-mem 1024,1024 \
                -a 0000:06:00.3,vdpa=1 -a 0000:06:00.4,vdpa=1 \
                -- --interactive

.. note::
    Here 0000:06:00.3 and 0000:06:00.4 refer to virtio ring compatible devices,
    and we need to bind vfio-pci to them before running vdpa sample.

    * modprobe vfio-pci
    * ./usertools/dpdk-devbind.py -b vfio-pci 06:00.3 06:00.4

Then we can create 2 vdpa ports in interactive cmdline.
