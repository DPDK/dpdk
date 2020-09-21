..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

VF daemon (VFd)
===============

VFd (the VF daemon) is a mechanism which can be used to configure features on
a VF (SR-IOV Virtual Function) without direct access to the PF (SR-IOV
Physical Function). VFd is an *EXPERIMENTAL* feature which can only be used in
the scenario of DPDK PF with a DPDK VF. If the PF port is driven by the Linux
kernel driver then the VFd feature will not work. Currently VFd is only
supported by the ixgbe and i40e drivers.

In general VF features cannot be configured directly by an end user
application since they are under the control of the PF. The normal approach to
configuring a feature on a VF is that an application would call the APIs
provided by the VF driver. If the required feature cannot be configured by the
VF directly (the most common case) the VF sends a message to the PF through
the mailbox on ixgbe and i40e. This means that the availability of the feature
depends on whether the appropriate mailbox messages are defined.

DPDK leverages the mailbox interface defined by the Linux kernel driver so
that compatibility with the kernel driver can be guaranteed. The downside of
this approach is that the availability of messages supported by the kernel
become a limitation when the user wants to configure features on the VF.

VFd is a new method of controlling the features on a VF. The VF driver doesn't
talk directly to the PF driver when configuring a feature on the VF. When a VF
application (i.e., an application using the VF ports) wants to enable a VF
feature, it can send a message to the PF application (i.e., the application
using the PF port, which can be the same as the VF application). The PF
application will configure the feature for the VF. Obviously, the PF
application can also configure the VF features without a request from the VF
application.

.. _VF_daemon_overview:

.. figure:: img/vf_daemon_overview.*

   VF daemon (VFd) Overview

Compared with the traditional approach the VFd moves the negotiation between
VF and PF from the driver level to application level. So the application
should define how the negotiation between the VF and PF works, or even if the
control should be limited to the PF.

It is the application's responsibility to use VFd. Consider for example a KVM
migration, the VF application may transfer from one VM to another. It is
recommended in this case that the PF control the VF features without
participation from the VF. Then the VF application has no capability to
configure the features. So the user doesn't need to define the interface
between the VF application and the PF application. The service provider should
take the control of all the features.

The following sections describe the VFd functionality.

.. Note::

   Although VFd is supported by both ixgbe and i40e, please be aware that
   since the hardware capability is different, the functions supported by
   ixgbe and i40e are not the same.


Preparing
---------

VFd only can be used in the scenario of DPDK PF + DPDK VF. Users should bind
the PF port to ``igb_uio``, then create the VFs based on the DPDK PF host.

The typical procedure to achieve this is as follows:

#. Boot the system without iommu, or with ``iommu=pt``.

#. Bind the PF port to ``igb_uio``, for example::

      dpdk-devbind.py -b igb_uio 01:00.0

#. Create a Virtual Function::

      echo 1 > /sys/bus/pci/devices/0000:01:00.0/max_vfs

#. Start a VM with the new VF port bypassed to it.

#. Run a DPDK application on the PF in the host::

      dpdk-testpmd -l 0-7 -n 4 -- -i --txqflags=0

#. Bind the VF port to ``igb_uio`` in the VM::

      dpdk-devbind.py -b igb_uio 03:00.0

#. Run a DPDK application on the VF in the VM::

      dpdk-testpmd -l 0-7 -n 4 -- -i --txqflags=0


Common functions of IXGBE and I40E
----------------------------------

The following sections show how to enable PF/VF functionality based on the
above testpmd setup.


TX loopback
~~~~~~~~~~~

Run a testpmd runtime command on the PF to set TX loopback::

   set tx loopback 0 on|off

This sets whether the PF port and all the VF ports that belong to it are
allowed to send the packets to other virtual ports.

Although it is a VFd function, it is the global setting for the whole
physical port. When using this function, the PF and all the VFs TX loopback
will be enabled/disabled.


VF MAC address setting
~~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the MAC address for a VF port::

   set vf mac addr 0 0 A0:36:9F:7B:C3:51

This testpmd runtime command will change the MAC address of the VF port to
this new address. If any other addresses are set before, they will be
overwritten.


VF MAC anti-spoofing
~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable the MAC
anti-spoofing for a VF port::

   set vf mac antispoof 0 0 on|off

When enabling the MAC anti-spoofing, the port will not forward packets whose
source MAC address is not the same as the port.


VF VLAN anti-spoofing
~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable the VLAN
anti-spoofing for a VF port::

   set vf vlan antispoof 0 0 on|off

When enabling the VLAN anti-spoofing, the port will not send packets whose
VLAN ID does not belong to VLAN IDs that this port can receive.


VF VLAN insertion
~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the VLAN insertion for a VF
port::

   set vf vlan insert 0 0 1

When using this testpmd runtime command, an assigned VLAN ID can be inserted
to the transmitted packets by the hardware.

The assigned VLAN ID can be 0. It means disabling the VLAN insertion.


VF VLAN stripping
~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable the VLAN stripping
for a VF port::

   set vf vlan stripq 0 0 on|off

This testpmd runtime command is used to enable/disable the RX VLAN stripping
for a specific VF port.


VF VLAN filtering
~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the VLAN filtering for a VF
port::

   rx_vlan add 1 port 0 vf 1
   rx_vlan rm  1 port 0 vf 1

These two testpmd runtime commands can be used to add or remove the VLAN
filter for several VF ports. When the VLAN filters are added only the packets
that have the assigned VLAN IDs can be received. Other packets will be dropped
by hardware.


The IXGBE specific VFd functions
--------------------------------

The functions in this section are specific to the ixgbe driver.


All queues drop
~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable the all queues
drop::

   set all queues drop on|off

This is a global setting for the PF and all the VF ports of the physical port.

Enabling the ``all queues drop`` feature means that when there is no available
descriptor for the received packets they are dropped. The ``all queues drop``
feature should be enabled in SR-IOV mode to avoid one queue blocking others.


VF packet drop
~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable the packet drop for
a specific VF::

   set vf split drop 0 0 on|off

This is a similar function as ``all queues drop``. The difference is that this
function is per VF setting and the previous function is a global setting.


VF rate limit
~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to all queues' rate limit for a
specific VF::

   set port 0 vf 0 rate 10 queue_mask 1

This is a function to set the rate limit for all the queues in the
``queue_mask`` bitmap. It is not used to set the summary of the rate
limit. The rate limit of every queue will be set equally to the assigned rate
limit.


VF RX enabling
~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable packet receiving for
a specific VF::

   set port 0 vf 0 rx on|off

This function can be used to stop/start packet receiving on a VF.


VF TX enabling
~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable packet transmitting
for a specific VF::

   set port 0 vf 0 tx on|off

This function can be used to stop/start packet transmitting on a VF.


VF RX mode setting
~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the RX mode for a specific VF::

   set port 0 vf 0 rxmode AUPE|ROPE|BAM|MPE on|off

This function can be used to enable/disable some RX modes on the VF, including:

* If it accept untagged packets.
* If it accepts packets matching the MAC filters.
* If it accept MAC broadcast packets,
* If it enables MAC multicast promiscuous mode.


The I40E specific VFd functions
-------------------------------

The functions in this section are specific to the i40e driver.


VF statistics
~~~~~~~~~~~~~

This provides an API to get the a specific VF's statistic from PF.


VF statistics resetting
~~~~~~~~~~~~~~~~~~~~~~~

This provides an API to rest the a specific VF's statistic from PF.


VF link status change notification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This provide an API to let a specific VF know if the physical link status
changed.

Normally if a VF received this notification, the driver should notify the
application to reset the VF port.


VF MAC broadcast setting
~~~~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable MAC broadcast packet
receiving for a specific VF::

   set vf broadcast 0 0 on|off


VF MAC multicast promiscuous mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable MAC multicast
promiscuous mode for a specific VF::

   set vf allmulti 0 0 on|off


VF MAC unicast promiscuous mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable MAC unicast
promiscuous mode for a specific VF::

   set vf promisc 0 0 on|off


VF max bandwidth
~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the TX maximum bandwidth for a
specific VF::

   set vf tx max-bandwidth 0 0 2000

The maximum bandwidth is an absolute value in Mbps.


VF TC bandwidth allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the TCs (traffic class) TX
bandwidth allocation for a specific VF::

   set vf tc tx min-bandwidth 0 0 (20,20,20,40)

The allocated bandwidth should be set for all the TCs. The allocated bandwidth
is a relative value as a percentage. The sum of all the bandwidth should
be 100.


VF TC max bandwidth
~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to set the TCs TX maximum bandwidth
for a specific VF::

   set vf tc tx max-bandwidth 0 0 0 10000

The maximum bandwidth is an absolute value in Mbps.


TC strict priority scheduling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run a testpmd runtime command on the PF to enable/disable several TCs TX
strict priority scheduling::

   set tx strict-link-priority 0 0x3

The 0 in the TC bitmap means disabling the strict priority scheduling for this
TC. To enable use a value of 1.
