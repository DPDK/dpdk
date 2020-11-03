..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 6WIND S.A.
    Copyright 2017 Mellanox Technologies, Ltd

VDEV_NETVSC driver
==================

The VDEV_NETVSC driver (**librte_net_vdev_netvsc**) provides support for NetVSC
interfaces and associated SR-IOV virtual function (VF) devices found in
Linux virtual machines running on Microsoft Hyper-V_ (including Azure)
platforms.

.. _Hyper-V: https://docs.microsoft.com/en-us/windows-hardware/drivers/network/overview-of-hyper-v

Implementation details
----------------------

Each instance of this driver effectively needs to drive two devices: the
NetVSC interface proper and its SR-IOV VF (referred to as "physical" from
this point on) counterpart sharing the same MAC address.

Physical devices are part of the host system and cannot be maintained during
VM migration. From a VM standpoint they appear as hot-plug devices that come
and go without prior notice.

When the physical device is present, egress and most of the ingress traffic
flows through it; only multicasts and other hypervisor control still flow
through NetVSC. Otherwise, NetVSC acts as a fallback for all traffic.

To avoid unnecessary code duplication and ensure maximum performance,
handling of physical devices is left to their original PMDs; this virtual
device driver (also known as *vdev*) manages other PMDs as summarized by the
following block diagram::

         .------------------.
         | DPDK application |
         `--------+---------'
                  |
           .------+------.
           | DPDK ethdev |
           `------+------'       Control
                  |                 |
     .------------+------------.    v    .--------------------.
     |       failsafe PMD      +---------+ vdev_netvsc driver |
     `--+-------------------+--'         `--------------------'
        |                   |
        |          .........|.........
        |          :        |        :
   .----+----.     :   .----+----.   :
   | tap PMD |     :   | any PMD |   :
   `----+----'     :   `----+----'   : <-- Hot-pluggable
        |          :        |        :
 .------+-------.  :  .-----+-----.  :
 | NetVSC-based |  :  | SR-IOV VF |  :
 |   netdevice  |  :  |   device  |  :
 `--------------'  :  `-----------'  :
                   :.................:


This driver implementation may be temporary and should be improved or removed
either when hot-plug will be fully supported in EAL and bus drivers or when
a new NetVSC driver will be integrated.


Run-time parameters
-------------------

This driver is invoked automatically in Hyper-V VM systems unless the user
invoked it by command line using ``--vdev=net_vdev_netvsc`` EAL option.

The following device parameters are supported:

- ``iface`` [string]

  Provide a specific NetVSC interface (netdevice) name to attach this driver
  to. Can be provided multiple times for additional instances.

- ``mac`` [string]

  Same as ``iface`` except a suitable NetVSC interface is located using its
  MAC address.

- ``force`` [int]

  If nonzero, forces the use of specified interfaces even if not detected as
  NetVSC.

- ``ignore`` [int]

  If nonzero, ignores the driver running (actually used to disable the
  auto-detection in Hyper-V VM).

.. note::

   Not specifying either ``iface`` or ``mac`` makes this driver attach itself to
   all unrouted NetVSC interfaces found on the system.
   Specifying the device makes this driver attach itself to the device
   regardless the device routes.
