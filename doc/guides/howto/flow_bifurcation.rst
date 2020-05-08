..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

Flow Bifurcation How-to Guide
=============================

Flow Bifurcation is a mechanism which uses hardware capable Ethernet devices
to split traffic between Linux user space and kernel space. Since it is a
hardware assisted feature this approach can provide line rate processing
capability. Other than :ref:`KNI <kni>`, the software is just required to
enable device configuration, there is no need to take care of the packet
movement during the traffic split. This can yield better performance with
less CPU overhead.

The Flow Bifurcation splits the incoming data traffic to user space
applications (such as DPDK applications) and/or kernel space programs (such as
the Linux kernel stack). It can direct some traffic, for example data plane
traffic, to DPDK, while directing some other traffic, for example control
plane traffic, to the traditional Linux networking stack.

There are a number of technical options to achieve this. A typical example is
to combine the technology of SR-IOV and packet classification filtering.

SR-IOV is a PCI standard that allows the same physical adapter to be split as
multiple virtual functions. Each virtual function (VF) has separated queues
with physical functions (PF). The network adapter will direct traffic to a
virtual function with a matching destination MAC address. In a sense, SR-IOV
has the capability for queue division.

Packet classification filtering is a hardware capability available on most
network adapters. Filters can be configured to direct specific flows to a
given receive queue by hardware. Different NICs may have different filter
types to direct flows to a Virtual Function or a queue that belong to it.

In this way the Linux networking stack can receive specific traffic through
the kernel driver while a DPDK application can receive specific traffic
bypassing the Linux kernel by using drivers like VFIO or the DPDK ``igb_uio``
module.

.. _figure_flow_bifurcation_overview:

.. figure:: img/flow_bifurcation_overview.*

   Flow Bifurcation Overview


Using Flow Bifurcation on Mellanox ConnectX
-------------------------------------------

The Mellanox devices are :ref:`natively bifurcated <bifurcated_driver>`,
so there is no need to split into SR-IOV PF/VF
in order to get the flow bifurcation mechanism.
The full device is already shared with the kernel driver.

The DPDK application can setup some flow steering rules,
and let the rest go to the kernel stack.
In order to define the filters strictly with flow rules,
the :ref:`flow_isolated_mode` can be configured.

There is no specific instructions to follow.
The recommended reading is the :doc:`../prog_guide/rte_flow` guide.
Below is an example of testpmd commands
for receiving VXLAN 42 in 4 queues of the DPDK port 0,
while all other packets go to the kernel:

.. code-block:: console

   testpmd> flow isolate 0 true
   testpmd> flow create 0 ingress pattern eth / ipv4 / udp / vxlan vni is 42 / end \
            actions rss queues 0 1 2 3 end / end
