..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Wind River Systems, Inc.
    All rights reserved.

AVP Poll Mode Driver
=================================================================

The Accelerated Virtual Port (AVP) device is a shared memory based device
only available on `virtualization platforms <http://www.windriver.com/products/titanium-cloud/>`_
from Wind River Systems.  The Wind River Systems virtualization platform
currently uses QEMU/KVM as its hypervisor and as such provides support for all
of the QEMU supported virtual and/or emulated devices (e.g., virtio, e1000,
etc.).  The platform offers the virtio device type as the default device when
launching a virtual machine or creating a virtual machine port.  The AVP device
is a specialized device available to customers that require increased
throughput and decreased latency to meet the demands of their performance
focused applications.

The AVP driver binds to any AVP PCI devices that have been exported by the Wind
River Systems QEMU/KVM hypervisor.  As a user of the DPDK driver API it
supports a subset of the full Ethernet device API to enable the application to
use the standard device configuration functions and packet receive/transmit
functions.

These devices enable optimized packet throughput by bypassing QEMU and
delivering packets directly to the virtual switch via a shared memory
mechanism.  This provides DPDK applications running in virtual machines with
significantly improved throughput and latency over other device types.

The AVP device implementation is integrated with the QEMU/KVM live-migration
mechanism to allow applications to seamlessly migrate from one hypervisor node
to another with minimal packet loss.


Features and Limitations of the AVP PMD
---------------------------------------

The AVP PMD provides the following functionality.

*   Receive and transmit of both simple and chained mbuf packets,

*   Chained mbufs may include up to 5 chained segments,

*   Up to 8 receive and transmit queues per device,

*   Only a single MAC address is supported,

*   The MAC address cannot be modified,

*   The maximum receive packet length is 9238 bytes,

*   VLAN header stripping and inserting,

*   Promiscuous mode

*   VM live-migration

*   PCI hotplug insertion and removal


Prerequisites
-------------

The following prerequisites apply:

*   A virtual machine running in a Wind River Systems virtualization
    environment and configured with at least one neutron port defined with a
    vif-model set to "avp".


Launching a VM with an AVP type network attachment
--------------------------------------------------

The following example will launch a VM with three network attachments.  The
first attachment will have a default vif-model of "virtio".  The next two
network attachments will have a vif-model of "avp" and may be used with a DPDK
application which is built to include the AVP PMD.

.. code-block:: console

    nova boot --flavor small --image my-image \
       --nic net-id=${NETWORK1_UUID} \
       --nic net-id=${NETWORK2_UUID},vif-model=avp \
       --nic net-id=${NETWORK3_UUID},vif-model=avp \
       --security-group default my-instance1
