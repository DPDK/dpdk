..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) Microsoft Corporation.

Netvsc poll mode driver
=======================

The Netvsc Poll Mode driver (PMD) provides support for the paravirtualized
network device for Microsoft Hyper-V. It can be used with
Window Server 2008/2012/2016, Windows 10.
The device offers multi-queue support (if kernel and host support it),
checksum and segmentation offloads.


Features and Limitations of Hyper-V PMD
---------------------------------------

In this release, the hyper PMD driver provides the basic functionality of packet reception and transmission.

*   It supports merge-able buffers per packet when receiving packets and scattered buffer per packet
    when transmitting packets. The packet size supported is from 64 to 65536.

*   The PMD supports multicast packets and promiscuous mode subject to restrictions on the host.
    In order to this to work, the guest network configuration on Hyper-V must be configured to allow MAC address
    spoofing.

*   The device has only a single MAC address.
    Hyper-V driver does not support MAC or VLAN filtering because the Hyper-V host does not support it.

*   VLAN tags are always stripped and presented in mbuf tci field.

*   The Hyper-V driver does not use or support Link State or Rx interrupt.

*   The maximum number of queues is limited by the host (currently 64).
    When used with 4.16 kernel only a single queue is available.

.. note::
   This driver is intended for use with **Hyper-V only** and is
   not recommended for use on Azure because accelerated Networking
   (SR-IOV) is not supported.

   On Azure, use the :doc:`vdev_netvsc` which
   automatically configures the necessary TAP and failsave drivers.


Installation
------------

The Netvsc PMD is a standalone driver, similar to virtio and vmxnet3.
Using Netvsc PMD requires that the associated VMBUS device be bound to the userspace
I/O device driver for Hyper-V (uio_hv_generic). By default, all netvsc devices
will be bound to the Linux kernel driver; in order to use netvsc PMD the
device must first be overridden.

The first step is to identify the network device to override.
VMBUS uses Universal Unique Identifiers
(`UUID`_) to identify devices on the bus similar to how PCI uses Domain:Bus:Function.
The UUID associated with a Linux kernel network device can be determined
by looking at the sysfs information. To find the UUID for eth1 and
store it in a shell variable:

    .. code-block:: console

	DEV_UUID=$(basename $(readlink /sys/class/net/eth1/device))


.. _`UUID`: https://en.wikipedia.org/wiki/Universally_unique_identifier

There are several possible ways to assign the uio device driver for a device.
The easiest way (but only on 4.18 or later)
is to use the `driverctl Device Driver control utility`_ to override
the normal kernel device.

    .. code-block:: console

	driverctl -b vmbus set-override $DEV_UUID uio_hv_generic

.. _`driverctl Device Driver control utility`: https://gitlab.com/driverctl/driverctl

Any settings done with driverctl are by default persistent and will be reapplied
on reboot.

On older kernels, the same effect can be had by manual sysfs bind and unbind
operations:

    .. code-block:: console

	NET_UUID="f8615163-df3e-46c5-913f-f2d2f965ed0e"
	modprobe uio_hv_generic
	echo $NET_UUID > /sys/bus/vmbus/drivers/uio_hv_generic/new_id
	echo $DEV_UUID > /sys/bus/vmbus/drivers/hv_netvsc/unbind
	echo $DEV_UUID > /sys/bus/vmbus/drivers/uio_hv_generic/bind

.. Note::

   The dpkd-devbind.py script can not be used since it only handles PCI devices.


Prerequisites
-------------

The following prerequisites apply:

*   Linux kernel support for UIO on vmbus is done with the uio_hv_generic driver.
    Full support of multiple queues requires the 4.17 kernel. It is possible
    to use the netvsc PMD with 4.16 kernel but it is limited to a single queue.
