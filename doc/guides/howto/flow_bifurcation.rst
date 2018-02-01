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


Using Flow Bifurcation on IXGBE in Linux
----------------------------------------

On Intel 82599 10 Gigabit Ethernet Controller series NICs Flow Bifurcation can
be achieved by SR-IOV and Intel Flow Director technologies. Traffic can be
directed to queues by the Flow Director capability, typically by matching
5-tuple of UDP/TCP packets.

The typical procedure to achieve this is as follows:

#. Boot the system without iommu, or with ``iommu=pt``.

#. Create Virtual Functions:

   .. code-block:: console

       echo 2 > /sys/bus/pci/devices/0000:01:00.0/sriov_numvfs

#. Enable and set flow filters:

   .. code-block:: console

       ethtool -K eth1 ntuple on
       ethtool -N eth1 flow-type udp4 src-ip 192.0.2.2 dst-ip 198.51.100.2 \
               action $queue_index_in_VF0
       ethtool -N eth1 flow-type udp4 src-ip 198.51.100.2 dst-ip 192.0.2.2 \
               action $queue_index_in_VF1

   Where:

   * ``$queue_index_in_VFn``: Bits 39:32 of the variable defines VF id + 1; the lower 32 bits indicates the queue index of the VF. Thus:

     * ``$queue_index_in_VF0`` = ``(0x1 & 0xFF) << 32 + [queue index]``.

     * ``$queue_index_in_VF1`` = ``(0x2 & 0xFF) << 32 + [queue index]``.

   .. _figure_ixgbe_bifu_queue_idx:

   .. figure:: img/ixgbe_bifu_queue_idx.*

#. Compile the DPDK application and insert ``igb_uio`` or probe the ``vfio-pci`` kernel modules as normal.

#. Bind the virtual functions:

   .. code-block:: console

       modprobe vfio-pci
       dpdk-devbind.py -b vfio-pci 01:10.0
       dpdk-devbind.py -b vfio-pci 01:10.1

#. Run a DPDK application on the VFs:

   .. code-block:: console

       testpmd -l 0-7 -n 4 -- -i -w 01:10.0 -w 01:10.1 --forward-mode=mac

In this example, traffic matching the rules will go through the VF by matching
the filter rule. All other traffic, not matching the rules, will go through
the default queue or scaling on queues in the PF. That is to say UDP packets
with the specified IP source and destination addresses will go through the
DPDK application. All other traffic, with different hosts or different
protocols, will go through the Linux networking stack.

.. note::

    * The above steps work on the Linux kernel v4.2.

    * The Flow Bifurcation is implemented in Linux kernel and ixgbe kernel driver using the following patches:

        * `ethtool: Add helper routines to pass vf to rx_flow_spec <https://patchwork.ozlabs.org/patch/476511/>`_

        * `ixgbe: Allow flow director to use entire queue space <https://patchwork.ozlabs.org/patch/476516/>`_

    * The Ethtool version used in this example is 3.18.


Using Flow Bifurcation on I40E in Linux
---------------------------------------

On Intel X710/XL710 series Ethernet Controllers Flow Bifurcation can be
achieved by SR-IOV, Cloud Filter and L3 VEB switch. The traffic can be
directed to queues by the Cloud Filter and L3 VEB switch's matching rule.

* L3 VEB filters work for non-tunneled packets. It can direct a packet just by
  the Destination IP address to a queue in a VF.

* Cloud filters work for the following types of tunneled packets.

    * Inner mac.

    * Inner mac + VNI.

    * Outer mac + Inner mac + VNI.

    * Inner mac + Inner vlan + VNI.

    * Inner mac + Inner vlan.

The typical procedure to achieve this is as follows:

#. Boot the system without iommu, or with ``iommu=pt``.

#. Build and insert the ``i40e.ko`` module.

#. Create Virtual Functions:

   .. code-block:: console

       echo 2 > /sys/bus/pci/devices/0000:01:00.0/sriov_numvfs

#. Add udp port offload to the NIC if using cloud filter:

   .. code-block:: console

       ip li add vxlan0 type vxlan id 42 group 239.1.1.1 local 10.16.43.214 dev <name>
       ifconfig vxlan0 up
       ip -d li show vxlan0

   .. note::

       Output such as ``add vxlan port 8472, index 0 success`` should be
       found in the system log.

#. Examples of enabling and setting flow filters:

   * L3 VEB filter, for a route whose destination IP is 192.168.50.108 to VF
     0's queue 2.

     .. code-block:: console

       ethtool -N <dev_name> flow-type ip4 dst-ip 192.168.50.108 \
               user-def 0xffffffff00000000 action 2 loc 8

   * Inner mac, for a route whose inner destination mac is 0:0:0:0:9:0 to
     PF's queue 6.

     .. code-block:: console

       ethtool -N <dev_name> flow-type ether dst 00:00:00:00:00:00 \
               m ff:ff:ff:ff:ff:ff src 00:00:00:00:09:00 m 00:00:00:00:00:00 \
               user-def 0xffffffff00000003 action 6 loc 1

   * Inner mac + VNI, for a route whose inner destination mac is 0:0:0:0:9:0
     and VNI is 8 to PF's queue 4.

     .. code-block:: console

       ethtool -N <dev_name> flow-type ether dst 00:00:00:00:00:00 \
               m ff:ff:ff:ff:ff:ff src 00:00:00:00:09:00 m 00:00:00:00:00:00 \
               user-def 0x800000003 action 4 loc 4

   * Outer mac + Inner mac + VNI, for a route whose outer mac is
     68:05:ca:24:03:8b, inner destination mac is c2:1a:e1:53:bc:57, and VNI
     is 8 to PF's queue 2.

     .. code-block:: console

       ethtool -N <dev_name> flow-type ether dst 68:05:ca:24:03:8b \
               m 00:00:00:00:00:00 src c2:1a:e1:53:bc:57 m 00:00:00:00:00:00 \
               user-def 0x800000003 action 2 loc 2

   * Inner mac + Inner vlan + VNI, for a route whose inner destination mac is
     00:00:00:00:20:00, inner vlan is 10, and VNI is 8 to VF 0's queue 1.

     .. code-block:: console

       ethtool -N <dev_name> flow-type ether dst 00:00:00:00:01:00 \
               m ff:ff:ff:ff:ff:ff src 00:00:00:00:20:00 m 00:00:00:00:00:00 \
               vlan 10 user-def 0x800000000 action 1 loc 5

   * Inner mac + Inner vlan, for a route whose inner destination mac is
     00:00:00:00:20:00, and inner vlan is 10 to VF 0's queue 1.

     .. code-block:: console

       ethtool -N <dev_name> flow-type ether dst 00:00:00:00:01:00 \
               m ff:ff:ff:ff:ff:ff src 00:00:00:00:20:00 m 00:00:00:00:00:00 \
               vlan 10 user-def 0xffffffff00000000 action 1 loc 5

   .. note::

       * If the upper 32 bits of 'user-def' are ``0xffffffff``, then the
         filter can be used for programming an L3 VEB filter, otherwise the
         upper 32 bits of 'user-def' can carry the tenant ID/VNI if
         specified/required.

       * Cloud filters can be defined with inner mac, outer mac, inner ip,
         inner vlan and VNI as part of the cloud tuple. It is always the
         destination (not source) mac/ip that these filters use. For all
         these examples dst and src mac address fields are overloaded dst ==
         outer, src == inner.

       * The filter will direct a packet matching the rule to a vf id
         specified in the lower 32 bit of user-def to the queue specified by
         'action'.

       * If the vf id specified by the lower 32 bit of user-def is greater
         than or equal to ``max_vfs``, then the filter is for the PF queues.

#. Compile the DPDK application and insert ``igb_uio`` or probe the ``vfio-pci``
   kernel modules as normal.

#. Bind the virtual function:

   .. code-block:: console

       modprobe vfio-pci
       dpdk-devbind.py -b vfio-pci 01:10.0
       dpdk-devbind.py -b vfio-pci 01:10.1

#. run DPDK application on VFs:

   .. code-block:: console

       testpmd -l 0-7 -n 4 -- -i -w 01:10.0 -w 01:10.1 --forward-mode=mac

.. note::

   * The above steps work on the i40e Linux kernel driver v1.5.16.

   * The Ethtool version used in this example is 3.18. The mask ``ff`` means
     'not involved', while ``00`` or no mask means 'involved'.

   * For more details of the configuration, refer to the
     `cloud filter test plan <http://dpdk.org/browse/tools/dts/tree/test_plans/cloud_filter_test_plan.rst>`_
