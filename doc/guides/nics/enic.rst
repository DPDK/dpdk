..  BSD LICENSE
    Copyright (c) 2017, Cisco Systems, Inc.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

ENIC Poll Mode Driver
=====================

ENIC PMD is the DPDK poll-mode driver for the Cisco System Inc. VIC Ethernet
NICs. These adapters are also referred to as vNICs below. If you are running
or would like to run DPDK software applications on Cisco UCS servers using
Cisco VIC adapters the following documentation is relevant.

How to obtain ENIC PMD integrated DPDK
--------------------------------------

ENIC PMD support is integrated into the DPDK suite. dpdk-<version>.tar.gz
should be downloaded from http://dpdk.org


Configuration information
-------------------------

- **DPDK Configuration Parameters**

  The following configuration options are available for the ENIC PMD:

  - **CONFIG_RTE_LIBRTE_ENIC_PMD** (default y): Enables or disables inclusion
    of the ENIC PMD driver in the DPDK compilation.

- **vNIC Configuration Parameters**

  - **Number of Queues**

    The maximum number of receive queues (RQs), work queues (WQs) and
    completion queues (CQs) are configurable on a per vNIC basis
    through the Cisco UCS Manager (CIMC or UCSM).

    These values should be configured as follows:

    - The number of WQs should be greater or equal to the value of the
      expected nb_tx_q parameter in the call to
      rte_eth_dev_configure()

    - The number of RQs configured in the vNIC should be greater or
      equal to *twice* the value of the expected nb_rx_q parameter in
      the call to rte_eth_dev_configure().  With the addition of Rx
      scatter, a pair of RQs on the vnic is needed for each receive
      queue used by DPDK, even if Rx scatter is not being used.
      Having a vNIC with only 1 RQ is not a valid configuration, and
      will fail with an error message.

    - The number of CQs should set so that there is one CQ for each
      WQ, and one CQ for each pair of RQs.

    For example: If the application requires 3 Rx queues, and 3 Tx
    queues, the vNIC should be configured to have at least 3 WQs, 6
    RQs (3 pairs), and 6 CQs (3 for use by WQs + 3 for use by the 3
    pairs of RQs).

  - **Size of Queues**

    Likewise, the number of receive and transmit descriptors are configurable on
    a per-vNIC basis via the UCS Manager and should be greater than or equal to
    the nb_rx_desc and   nb_tx_desc parameters expected to be used in the calls
    to rte_eth_rx_queue_setup() and rte_eth_tx_queue_setup() respectively.
    An application requesting more than the set size will be limited to that
    size.

    Unless there is a lack of resources due to creating many vNICs, it
    is recommended that the WQ and RQ sizes be set to the maximum.  This
    gives the application the greatest amount of flexibility in its
    queue configuration.

    - *Note*: Since the introduction of Rx scatter, for performance
      reasons, this PMD uses two RQs on the vNIC per receive queue in
      DPDK.  One RQ holds descriptors for the start of a packet, and the
      second RQ holds the descriptors for the rest of the fragments of
      a packet.  This means that the nb_rx_desc parameter to
      rte_eth_rx_queue_setup() can be a greater than 4096.  The exact
      amount will depend on the size of the mbufs being used for
      receives, and the MTU size.

      For example: If the mbuf size is 2048, and the MTU is 9000, then
      receiving a full size packet will take 5 descriptors, 1 from the
      start-of-packet queue, and 4 from the second queue.  Assuming
      that the RQ size was set to the maximum of 4096, then the
      application can specify up to 1024 + 4096 as the nb_rx_desc
      parameter to rte_eth_rx_queue_setup().

  - **Interrupts**

    At least one interrupt per vNIC interface should be configured in the UCS
    manager regardless of the number receive/transmit queues. The ENIC PMD
    uses this interrupt to get information about link status and errors
    in the fast path.

    In addition to the interrupt for link status and errors, when using Rx queue
    interrupts, increase the number of configured interrupts so that there is at
    least one interrupt for each Rx queue. For example, if the app uses 3 Rx
    queues and wants to use per-queue interrupts, configure 4 (3 + 1) interrupts.

  - **Receive Side Scaling**

    In order to fully utilize RSS in DPDK, enable all RSS related settings in
    CIMC or UCSM. These include the following items listed under
    Receive Side Scaling:
    TCP, IPv4, TCP-IPv4, IPv6, TCP-IPv6, IPv6 Extension, TCP-IPv6 Extension.


.. _enic-flow-director:

Flow director support
---------------------

Advanced filtering support was added to 1300 series VIC firmware starting
with version 2.0.13 for C-series UCS servers and version 3.1.2 for UCSM
managed blade servers. In order to enable advanced filtering the 'Advanced
filter' radio button should be enabled via CIMC or UCSM followed by a reboot
of the server.

With advanced filters, perfect matching of all fields of IPv4, IPv6 headers
as well as TCP, UDP and SCTP L4 headers is available through flow director.
Masking of these fields for partial match is also supported.

Without advanced filter support, the flow director is limited to IPv4
perfect filtering of the 5-tuple with no masking of fields supported.

SR-IOV mode utilization
-----------------------

UCS blade servers configured with dynamic vNIC connection policies in UCSM
are capable of supporting SR-IOV. SR-IOV virtual functions (VFs) are
specialized vNICs, distinct from regular Ethernet vNICs. These VFs can be
directly assigned to virtual machines (VMs) as 'passthrough' devices.

In UCS, SR-IOV VFs require the use of the Cisco Virtual Machine Fabric Extender
(VM-FEX), which gives the VM a dedicated
interface on the Fabric Interconnect (FI). Layer 2 switching is done at
the FI. This may eliminate the requirement for software switching on the
host to route intra-host VM traffic.

Please refer to `Creating a Dynamic vNIC Connection Policy
<http://www.cisco.com/c/en/us/td/docs/unified_computing/ucs/sw/vm_fex/vmware/gui/config_guide/b_GUI_VMware_VM-FEX_UCSM_Configuration_Guide/b_GUI_VMware_VM-FEX_UCSM_Configuration_Guide_chapter_010.html#task_433E01651F69464783A68E66DA8A47A5>`_
for information on configuring SR-IOV adapter policies and port profiles
using UCSM.

Once the policies are in place and the host OS is rebooted, VFs should be
visible on the host, E.g.:

.. code-block:: console

     # lspci | grep Cisco | grep Ethernet
     0d:00.0 Ethernet controller: Cisco Systems Inc VIC Ethernet NIC (rev a2)
     0d:00.1 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)
     0d:00.2 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)
     0d:00.3 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)
     0d:00.4 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)
     0d:00.5 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)
     0d:00.6 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)
     0d:00.7 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)

Enable Intel IOMMU on the host and install KVM and libvirt, and reboot again as
required. Then, using libvirt, create a VM instance with an assigned device.
Below is an example ``interface`` block (part of the domain configuration XML)
that adds the host VF 0d:00:01 to the VM. ``profileid='pp-vlan-25'`` indicates
the port profile that has been configured in UCSM.

.. code-block:: console

    <interface type='hostdev' managed='yes'>
      <mac address='52:54:00:ac:ff:b6'/>
      <driver name='vfio'/>
      <source>
        <address type='pci' domain='0x0000' bus='0x0d' slot='0x00' function='0x1'/>
      </source>
      <virtualport type='802.1Qbh'>
        <parameters profileid='pp-vlan-25'/>
      </virtualport>
    </interface>


Alternatively, the configuration can be done in a separate file using the
``network`` keyword. These methods are described in the libvirt documentation for
`Network XML format <https://libvirt.org/formatnetwork.html>`_.

When the VM instance is started, libvirt will bind the host VF to
vfio, complete provisioning on the FI and bring up the link.

.. note::

    It is not possible to use a VF directly from the host because it is not
    fully provisioned until libvirt brings up the VM that it is assigned
    to.

In the VM instance, the VF will now be visible. E.g., here the VF 00:04.0 is
seen on the VM instance and should be available for binding to a DPDK.

.. code-block:: console

     # lspci | grep Ether
     00:04.0 Ethernet controller: Cisco Systems Inc VIC SR-IOV VF (rev a2)

Follow the normal DPDK install procedure, binding the VF to either ``igb_uio``
or ``vfio`` in non-IOMMU mode.

In the VM, the kernel enic driver may be automatically bound to the VF during
boot. Unbinding it currently hangs due to a known issue with the driver. To
work around the issue, blacklist the enic module as follows.
Please see :ref:`Limitations <enic_limitations>` for limitations in
the use of SR-IOV.

.. code-block:: console

     # cat /etc/modprobe.d/enic.conf
     blacklist enic

     # dracut --force

.. note::

    Passthrough does not require SR-IOV. If VM-FEX is not desired, the user
    may create as many regular vNICs as necessary and assign them to VMs as
    passthrough devices. Since these vNICs are not SR-IOV VFs, using them as
    passthrough devices do not require libvirt, port profiles, and VM-FEX.


.. _enic-genic-flow-api:

Generic Flow API support
------------------------

Generic Flow API is supported. The baseline support is:

- **1200 series VICs**

  5-tuple exact flow support for 1200 series adapters. This allows:

  - Attributes: ingress
  - Items: ipv4, ipv6, udp, tcp (must exactly match src/dst IP
    addresses and ports and all must be specified)
  - Actions: queue and void
  - Selectors: 'is'

- **1300 and later series VICS with advanced filters disabled**

  With advanced filters disabled, an IPv4 or IPv6 item must be specified
  in the pattern.

  - Attributes: ingress
  - Items: eth, ipv4, ipv6, udp, tcp, vxlan, inner eth, ipv4, ipv6, udp, tcp
  - Actions: queue and void
  - Selectors: 'is', 'spec' and 'mask'. 'last' is not supported
  - In total, up to 64 bytes of mask is allowed across all headers

- **1300 and later series VICS with advanced filters enabled**

  - Attributes: ingress
  - Items: eth, ipv4, ipv6, udp, tcp, vxlan, inner eth, ipv4, ipv6, udp, tcp
  - Actions: queue, mark, drop, flag and void
  - Selectors: 'is', 'spec' and 'mask'. 'last' is not supported
  - In total, up to 64 bytes of mask is allowed across all headers

More features may be added in future firmware and new versions of the VIC.
Please refer to the release notes.

.. _overlay_offload:

Overlay Offload
---------------

Recent hardware models support overlay offload. When enabled, the NIC performs
the following operations for VXLAN, NVGRE, and GENEVE packets. In all cases,
inner and outer packets can be IPv4 or IPv6.

- TSO for VXLAN and GENEVE packets.

  Hardware supports NVGRE TSO, but DPDK currently has no NVGRE offload flags.

- Tx checksum offloads.

  The NIC fills in IPv4/UDP/TCP checksums for both inner and outer packets.

- Rx checksum offloads.

  The NIC validates IPv4/UDP/TCP checksums of both inner and outer packets.
  Good checksum flags (e.g. ``PKT_RX_L4_CKSUM_GOOD``) indicate that the inner
  packet has the correct checksum, and if applicable, the outer packet also
  has the correct checksum. Bad checksum flags (e.g. ``PKT_RX_L4_CKSUM_BAD``)
  indicate that the inner and/or outer packets have invalid checksum values.

- Inner Rx packet type classification

  PMD sets inner L3/L4 packet types (e.g. ``RTE_PTYPE_INNER_L4_TCP``), and
  ``RTE_PTYPE_TUNNEL_GRENAT`` to indicate that the packet is tunneled.
  PMD does not set L3/L4 packet types for outer packets.

- Inner RSS

  RSS hash calculation, therefore queue selection, is done on inner packets.

In order to enable overlay offload, the 'Enable VXLAN' box should be checked
via CIMC or UCSM followed by a reboot of the server. When PMD successfully
enables overlay offload, it prints the following message on the console.

.. code-block:: console

    Overlay offload is enabled

By default, PMD enables overlay offload if hardware supports it. To disable
it, set ``devargs`` parameter ``disable-overlay=1``. For example::

    -w 12:00.0,disable-overlay=1

.. _enic_limitations:

Limitations
-----------

- **VLAN 0 Priority Tagging**

  If a vNIC is configured in TRUNK mode by the UCS manager, the adapter will
  priority tag egress packets according to 802.1Q if they were not already
  VLAN tagged by software. If the adapter is connected to a properly configured
  switch, there will be no unexpected behavior.

  In test setups where an Ethernet port of a Cisco adapter in TRUNK mode is
  connected point-to-point to another adapter port or connected though a router
  instead of a switch, all ingress packets will be VLAN tagged. Programs such
  as l3fwd which do not account for VLAN tags in packets will misbehave. The
  solution is to enable VLAN stripping on ingress. The following code fragment is
  an example of how to accomplish this:

.. code-block:: console

     vlan_offload = rte_eth_dev_get_vlan_offload(port);
     vlan_offload |= ETH_VLAN_STRIP_OFFLOAD;
     rte_eth_dev_set_vlan_offload(port, vlan_offload);

- Limited flow director support on 1200 series and 1300 series Cisco VIC
  adapters with old firmware. Please see :ref:`enic-flow-director`.

- Flow director features are not supported on generation 1 Cisco VIC adapters
  (M81KR and P81E)

- **SR-IOV**

  - KVM hypervisor support only. VMware has not been tested.
  - Requires VM-FEX, and so is only available on UCS managed servers connected
    to Fabric Interconnects. It is not on standalone C-Series servers.
  - VF devices are not usable directly from the host. They can  only be used
    as assigned devices on VM instances.
  - Currently, unbind of the ENIC kernel mode driver 'enic.ko' on the VM
    instance may hang. As a workaround, enic.ko should be blacklisted or removed
    from the boot process.
  - pci_generic cannot be used as the uio module in the VM. igb_uio or
    vfio in non-IOMMU mode can be used.
  - The number of RQs in UCSM dynamic vNIC configurations must be at least 2.
  - The number of SR-IOV devices is limited to 256. Components on target system
    might limit this number to fewer than 256.

- **Flow API**

  - The number of filters that can be specified with the Generic Flow API is
    dependent on how many header fields are being masked. Use 'flow create' in
    a loop to determine how many filters your VIC will support (not more than
    1000 for 1300 series VICs). Filters are checked for matching in the order they
    were added. Since there currently is no grouping or priority support,
    'catch-all' filters should be added last.

- **Statistics**

  - ``rx_good_bytes`` (ibytes) always includes VLAN header (4B) and CRC bytes (4B).
    This behavior applies to 1300 and older series VIC adapters.
  - When the NIC drops a packet because the Rx queue has no free buffers,
    ``rx_good_bytes`` still increments by 4B if the packet is not VLAN tagged or
    VLAN stripping is disabled, or by 8B if the packet is VLAN tagged and stripping
    is enabled. This behavior applies to 1300 and older series VIC adapters.

- **RSS Hashing**

  - Hardware enables and disables UDP and TCP RSS hashing together. The driver
    cannot control UDP and TCP hashing individually.

How to build the suite
----------------------

The build instructions for the DPDK suite should be followed. By default
the ENIC PMD library will be built into the DPDK library.

Refer to the document :ref:`compiling and testing a PMD for a NIC
<pmd_build_and_test>` for details.

For configuring and using UIO and VFIO frameworks, please refer to the
documentation that comes with DPDK suite.

Supported Cisco VIC adapters
----------------------------

ENIC PMD supports all recent generations of Cisco VIC adapters including:

- VIC 1200 series
- VIC 1300 series

Supported Operating Systems
---------------------------

Any Linux distribution fulfilling the conditions described in Dependencies
section of DPDK documentation.

Supported features
------------------

- Unicast, multicast and broadcast transmission and reception
- Receive queue polling
- Port Hardware Statistics
- Hardware VLAN acceleration
- IP checksum offload
- Receive side VLAN stripping
- Multiple receive and transmit queues
- Flow Director ADD, UPDATE, DELETE, STATS operation support IPv4 and IPv6
- Promiscuous mode
- Setting RX VLAN (supported via UCSM/CIMC only)
- VLAN filtering (supported via UCSM/CIMC only)
- Execution of application by unprivileged system users
- IPV4, IPV6 and TCP RSS hashing
- UDP RSS hashing (support for upcoming adapters)
- Scattered Rx
- MTU update
- SR-IOV on UCS managed servers connected to Fabric Interconnects
- Flow API
- Overlay offload

  - Rx/Tx checksum offloads for VXLAN, NVGRE, GENEVE
  - TSO for VXLAN and GENEVE packets
  - Inner RSS

Known bugs and unsupported features in this release
---------------------------------------------------

- Signature or flex byte based flow direction
- Drop feature of flow direction
- VLAN based flow direction
- Non-IPV4 flow direction
- Setting of extended VLAN
- MTU update only works if Scattered Rx mode is disabled
- Maximum receive packet length is ignored if Scattered Rx mode is used

Prerequisites
-------------

- Prepare the system as recommended by DPDK suite.  This includes environment
  variables, hugepages configuration, tool-chains and configuration.
- Insert vfio-pci kernel module using the command 'modprobe vfio-pci' if the
  user wants to use VFIO framework.
- Insert uio kernel module using the command 'modprobe uio' if the user wants
  to use UIO framework.
- DPDK suite should be configured based on the user's decision to use VFIO or
  UIO framework.
- If the vNIC device(s) to be used is bound to the kernel mode Ethernet driver
  use 'ip' to bring the interface down. The dpdk-devbind.py tool can
  then be used to unbind the device's bus id from the ENIC kernel mode driver.
- Bind the intended vNIC to vfio-pci in case the user wants ENIC PMD to use
  VFIO framework using dpdk-devbind.py.
- Bind the intended vNIC to igb_uio in case the user wants ENIC PMD to use
  UIO framework using dpdk-devbind.py.

At this point the system should be ready to run DPDK applications. Once the
application runs to completion, the vNIC can be detached from vfio-pci or
igb_uio if necessary.

Root privilege is required to bind and unbind vNICs to/from VFIO/UIO.
VFIO framework helps an unprivileged user to run the applications.
For an unprivileged user to run the applications on DPDK and ENIC PMD,
it may be necessary to increase the maximum locked memory of the user.
The following command could be used to do this.

.. code-block:: console

    sudo sh -c "ulimit -l <value in Kilo Bytes>"

The value depends on the memory configuration of the application, DPDK and
PMD.  Typically, the limit has to be raised to higher than 2GB.
e.g., 2621440

The compilation of any unused drivers can be disabled using the
configuration file in config/ directory (e.g., config/common_linuxapp).
This would help in bringing down the time taken for building the
libraries and the initialization time of the application.

Additional Reference
--------------------

- https://www.cisco.com/c/en/us/products/servers-unified-computing/index.html
- https://www.cisco.com/c/en/us/products/interfaces-modules/unified-computing-system-adapters/index.html

Contact Information
-------------------

Any questions or bugs should be reported to DPDK community and to the ENIC PMD
maintainers:

- John Daley <johndale@cisco.com>
- Nelson Escobar <neescoba@cisco.com>
