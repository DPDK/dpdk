..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Mellanox Technologies, Ltd

Overview of vDPA Drivers Features
=================================

This section explains the supported features that are listed in the table below.

csum
  Device can handle packets with partial checksum.

guest csum
  Guest can handle packets with partial checksum.

mac
  Device has given MAC address.

gso
  Device can handle packets with any GSO type.

guest tso4
  Guest can receive TSOv4.

guest tso6
  Guest can receive TSOv6.

ecn
  Device can receive TSO with ECN.

ufo
  Device can receive UFO.

host tso4
  Device can receive TSOv4.

host tso6
  Device can receive TSOv6.

mrg rxbuf
  Guest can merge receive buffers.

ctrl vq
  Control channel is available.

ctrl rx
  Control channel RX mode support.

any layout
  Device can handle any descriptor layout.

guest announce
  Guest can send gratuitous packets.

mq
  Device supports Receive Flow Steering.

version 1
  v1.0 compliant.

log all
  Device can log all write descriptors (live migration).

indirect desc
  Indirect buffer descriptors support.

event idx
  Support for avail_idx and used_idx fields.

mtu
  Host can advise the guest with its maximum supported MTU.

in_order
  Device can use descriptors in ring order.

IOMMU platform
  Device support IOMMU addresses.

packed
  Device support packed virtio queues.

proto mq
  Support the number of queues query.

proto log shmfd
  Guest support setting log base.

proto rarp
  Host can broadcast a fake RARP after live migration.

proto reply ack
  Host support requested operation status ack.

proto host notifier
  Host can register memory region based host notifiers.

proto pagefault
  Slave expose page-fault FD for migration process.

queue statistics
  Support virtio queue statistics query.

BSD nic_uio
  BSD ``nic_uio`` module supported.

Linux VFIO
  Works with ``vfio-pci`` kernel module.

Other kdrv
  Kernel module other than above ones supported.

ARMv7
  Support armv7 architecture.

ARMv8
  Support armv8a (64bit) architecture.

Power8
  Support PowerPC architecture.

x86-32
  Support 32bits x86 architecture.

x86-64
  Support 64bits x86 architecture.

Usage doc
  Documentation describes usage, In ``doc/guides/vdpadevs/``.

Design doc
  Documentation describes design. In ``doc/guides/vdpadevs/``.

Perf doc
  Documentation describes performance values, In ``doc/perf/``.

.. note::

   Most of the features capabilities should be provided by the drivers via the
   next vDPA operations: ``get_features`` and ``get_protocol_features``.


References
==========

  * `OASIS: Virtual I/O Device (VIRTIO) Version 1.1 <https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html>`_
  * `QEMU: Vhost-user Protocol <https://qemu.weilnetz.de/doc/interop/vhost-user.html>`_


Features Table
==============

.. _table_vdpa_pmd_features:

.. include:: overview_feature_table.txt

.. Note::

   Features marked with "P" are partially supported. Refer to the appropriate
   driver guide in the following sections for details.
