..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2017, Cisco Systems, Inc.
    All rights reserved.

ENIC Poll Mode Driver
=====================

ENIC PMD is the DPDK poll-mode driver for the Cisco System Inc. VIC Ethernet
NICs. These adapters are also referred to as vNICs below. If you are running
or would like to run DPDK software applications on Cisco UCS servers using
Cisco VIC adapters the following documentation is relevant.

Supported Cisco VIC adapters
----------------------------

ENIC PMD supports all recent generations of Cisco VIC adapters including:

- VIC 1200 series
- VIC 1300 series
- VIC 1400/14000 series
- VIC 15000 series

Supported features
------------------

- Unicast, multicast and broadcast transmission and reception
- Receive queue polling
- Port Hardware Statistics
- Hardware VLAN acceleration
- IP checksum offload
- Receive side VLAN stripping
- Multiple receive and transmit queues
- Promiscuous mode
- Setting RX VLAN (supported via UCSM/CIMC only)
- VLAN filtering (supported via UCSM/CIMC only)
- Execution of application by unprivileged system users
- IPV4, IPV6 and TCP RSS hashing
- UDP RSS hashing (1400 series and later adapters)
- Scattered Rx
- MTU update
- SR-IOV virtual function
- Flow API
- Overlay offload

  - Rx/Tx checksum offloads for VXLAN, NVGRE, GENEVE
  - TSO for VXLAN and GENEVE packets
  - Inner RSS

How to obtain ENIC PMD integrated DPDK
--------------------------------------

ENIC PMD support is integrated into the DPDK suite. dpdk-<version>.tar.gz
should be downloaded from https://core.dpdk.org/download/


Configuration information
-------------------------

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


SR-IOV Virtual Function
-----------------------

VIC 1400 and later series supports SR-IOV.
It can be enabled via both UCSM and CIMC.
Please refer to the following guides to enable SR-IOV virtual functions (VFs).

  - CIMC: `Managing vNICs <https://www.cisco.com/c/en/us/td/docs/unified_computing/ucs/c/sw/gui/config/guide/4_3/b_cisco_ucs_c-series_gui_configuration_guide_43/b_Cisco_UCS_C-series_GUI_Configuration_Guide_41_chapter_01011.html#d77871e5874a1635>`_

  - UCSM: `Configuring SRIOV HPN Connection Policies <https://www.cisco.com/c/en/us/td/docs/unified_computing/ucs/ucs-manager/GUI-User-Guides/Network-Mgmt/4-3/b_UCSM_Network_Mgmt_Guide_4_3/b_UCSM_Network_Mgmt_Guide_chapter_01010.html#d21438e9555a1635>`_

Note that the previous SR-IOV implementation that is tied to VM-FEX
(Cisco Virtual Machine Fabric Extender) has been discontinued,
and ENIC PMD no longer supports it.
The current SR-IOV implementation does not require the Fabric Interconnect (FI),
as layer 2 switching is done within the VIC adapter.

Once SR-IOV is enabled, reboot the host OS and follow OS specific steps to create VFs
and assign them to virtual machines (VMs) or containers as necessary.
The VIC physical function (PF) drivers for ESXi and Linux support SR-IOV.
The following shows simplified steps for Linux.

.. code-block:: console

   # echo 4 > /sys/class/net/<pf-interface>/device/sriov_numvfs

   # lspci | grep Cisco | grep Ethernet
   12:00.0 Ethernet controller: Cisco Systems Inc VIC Ethernet NIC (rev a2)
   12:00.1 Ethernet controller: Cisco Systems Inc Device 02b7 (rev a2)
   12:00.2 Ethernet controller: Cisco Systems Inc Device 02b7 (rev a2)
   12:00.3 Ethernet controller: Cisco Systems Inc Device 02b7 (rev a2)
   12:00.4 Ethernet controller: Cisco Systems Inc Device 02b7 (rev a2)

Writing 4 to ``sriov_numvfs`` creates 4 VFs.
``lspci`` shows VFs and their PCI locations.
Interfaces with device ID ``02b7`` are the VFs.
The following snippet for libvirt XML assigns VF at ``12:00.1`` to VM.

.. code-block:: console

    <interface type="hostdev" managed="yes">
      <mac address="fa:16:3e:46:39:c5"/>
      <driver name='vfio'/>
      <source>
        <address type="pci" domain="0x0000" bus="0x12" slot="0x00" function="0x1"/>
      </source>
      <vlan>
        <tag id="1000"/>
      </vlan>
    </interface>

When the VM instance is started, libvirt will bind the host VF to vfio-pci.
In the VM instance, the VF will now be visible.
In this example, VF at ``07:00.0`` is seen on the VM instance
and is available for binding to DPDK.

.. code-block:: console

   # lspci | grep Cisco
   07:00.0 Ethernet controller: Cisco Systems Inc Device 02b7 (rev a2)

There are two known limitations of the current SR-IOV implementation.

  - Software Rx statistics

    VF on old VIC models does not have hardware Rx counters. In this case,
    ENIC PMD counts packets/bytes and reports them as device statistics.

  - Backward compatibility mode

    Old PF drivers on ESXi may lack full admin channel support.
    ENIC PMD detects such PF driver during initialization
    and reverts to the compatibility mode.
    In this mode, ENIC PMD does not use the admin channel,
    and trust mode (e.g. enabling promiscuous mode on VF) is not supported.

.. note::

   Passthrough does not require SR-IOV.
   If SR-IOV is not desired, the user may create as many regular vNICs as necessary
   and assign them to VMs as passthrough devices.


.. _enic-generic-flow-api:

Generic Flow API support
------------------------

Generic Flow API (also called "rte_flow" API) is supported. More advanced
capabilities are available when "Advanced Filtering" is enabled on the adapter.
Advanced filtering was added to 1300 series VIC firmware starting with version
2.0.13 for C-series UCS servers and version 3.1.2 for UCSM managed blade
servers. Advanced filtering is available on 1400 series adapters and beyond.
To enable advanced filtering, the 'Advanced filter' radio button should be
selected via CIMC or UCSM followed by a reboot of the server.

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
  - Items: eth, vlan, ipv4, ipv6, udp, tcp, vxlan, inner eth, vlan, ipv4, ipv6, udp, tcp
  - Actions: queue and void
  - Selectors: 'is', 'spec' and 'mask'. 'last' is not supported
  - In total, up to 64 bytes of mask is allowed across all headers

- **1300 and later series VICS with advanced filters enabled**

  - Attributes: ingress
  - Items: eth, vlan, ipv4, ipv6, udp, tcp, vxlan, raw, inner eth, vlan, ipv4, ipv6, udp, tcp
  - Actions: queue, mark, drop, flag, rss, passthru, and void
  - Selectors: 'is', 'spec' and 'mask'. 'last' is not supported
  - In total, up to 64 bytes of mask is allowed across all headers

- **1400 and later series VICs with Flow Manager API enabled**

  - Attributes: ingress, egress
  - Items: eth, vlan, ipv4, ipv6, sctp, udp, tcp, vxlan, raw, inner eth, vlan, ipv4, ipv6, sctp, udp, tcp
  - Ingress Actions: count, drop, flag, jump, mark, port_id, passthru, queue, rss, vxlan_decap, vxlan_encap, and void
  - Egress Actions: count, drop, jump, passthru, vxlan_encap, and void
  - Selectors: 'is', 'spec' and 'mask'. 'last' is not supported
  - In total, up to 64 bytes of mask is allowed across all headers

The VIC performs packet matching after applying VLAN strip. If VLAN
stripping is enabled, EtherType in the ETH item corresponds to the
stripped VLAN header's EtherType. Stripping does not affect the VLAN
item. TCI and EtherType in the VLAN item are matched against those in
the (stripped) VLAN header whether stripping is enabled or disabled.

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
  Good checksum flags (e.g. ``RTE_MBUF_F_RX_L4_CKSUM_GOOD``) indicate that the inner
  packet has the correct checksum, and if applicable, the outer packet also
  has the correct checksum. Bad checksum flags (e.g. ``RTE_MBUF_F_RX_L4_CKSUM_BAD``)
  indicate that the inner and/or outer packets have invalid checksum values.

- Inner Rx packet type classification

  PMD sets inner L3/L4 packet types (e.g. ``RTE_PTYPE_INNER_L4_TCP``), and
  ``RTE_PTYPE_TUNNEL_GRENAT`` to indicate that the packet is tunneled.
  PMD does not set L3/L4 packet types for outer packets.

- Inner RSS

  RSS hash calculation, therefore queue selection, is done on inner packets.

In order to enable overlay offload, enable VXLAN and/or Geneve on vNIC
via CIMC or UCSM followed by a reboot of the server. When PMD successfully
enables overlay offload, it prints one of the following messages on the console.

.. code-block:: console

    Overlay offload is enabled (VxLAN)
    Overlay offload is enabled (Geneve)
    Overlay offload is enabled (VxLAN, Geneve)

By default, PMD enables overlay offload if hardware supports it. To disable
it, set ``devargs`` parameter ``disable-overlay=1``. For example::

    -a 12:00.0,disable-overlay=1

By default, the NIC uses 4789 and 6081 as the VXLAN and Geneve ports,
respectively. The user may change them through
``rte_eth_dev_udp_tunnel_port_{add,delete}``. However, as the current
NIC has a single VXLAN port number and a single Geneve port number,
the user cannot configure multiple port numbers for each tunnel type.

Geneve offload support has evolved over VIC models. On older models,
Geneve offload and advanced filters are mutually exclusive.  This is
enforced by UCSM and CIMC, which only allow one of the two features
to be selected at one time. Newer VIC models do not have this restriction.

Ingress VLAN Rewrite
--------------------

VIC adapters can tag, untag, or modify the VLAN headers of ingress
packets. The ingress VLAN rewrite mode controls this behavior. By
default, it is set to pass-through, where the NIC does not modify the
VLAN header in any way so that the application can see the original
header. This mode is sufficient for many applications, but may not be
suitable for others. Such applications may change the mode by setting
``devargs`` parameter ``ig-vlan-rewrite`` to one of the following.

- ``pass``: Pass-through mode. The NIC does not modify the VLAN
  header. This is the default mode.

- ``priority``: Priority-tag default VLAN mode. If the ingress packet
  is tagged with the default VLAN, the NIC replaces its VLAN header
  with the priority tag (VLAN ID 0).

- ``trunk``: Default trunk mode. The NIC tags untagged ingress packets
  with the default VLAN. Tagged ingress packets are not modified. To
  the application, every packet appears as tagged.

- ``untag``: Untag default VLAN mode. If the ingress packet is tagged
  with the default VLAN, the NIC removes or untags its VLAN header so
  that the application sees an untagged packet. As a result, the
  default VLAN becomes `untagged`. This mode can be useful for
  applications such as OVS-DPDK performance benchmarks that utilize
  only the default VLAN and want to see only untagged packets.


Vectorized Rx Handler
---------------------

ENIC PMD includes a version of the receive handler that is vectorized using
AVX2 SIMD instructions. It is meant for bulk, throughput oriented workloads
where reducing cycles/packet in PMD is a priority. In order to use the
vectorized handler, take the following steps.

- Use a recent version of gcc or clang and build 64-bit DPDK. If
  the compiler is known to support AVX2, DPDK build system
  automatically compiles the vectorized handler. Otherwise, the
  handler is not available.

- Set ``devargs`` parameter ``enable-avx2-rx=1`` to explicitly request that
  PMD consider the vectorized handler when selecting the receive handler.
  For example::

    -a 12:00.0,enable-avx2-rx=1

  As the current implementation is intended for field trials, by default, the
  vectorized handler is not considered (``enable-avx2-rx=0``).

- Run on a UCS M4 or later server with CPUs that support AVX2.

PMD selects the vectorized handler when the handler is compiled into
the driver, the user requests its use via ``enable-avx2-rx=1``, CPU
supports AVX2, and scatter Rx is not used. To verify that the
vectorized handler is selected, enable debug logging
(``--log-level=pmd,debug``) and check the following message.

.. code-block:: console

    enic_use_vector_rx_handler use the non-scatter avx2 Rx handler

64B Completion Queue Entry
--------------------------

Recent VIC adapters support 64B completion queue entries, as well as
16B entries that are available on all adapter models. ENIC PMD enables
and uses 64B entries by default, if available. 64B entries generally
lower CPU cycles per Rx packet, as they avoid partial DMA writes and
reduce cache contention between DMA and polling CPU. The effect is
most pronounced when multiple Rx queues are used on Intel platforms
with Data Direct I/O Technology (DDIO).

If 64B entries are not available, PMD uses 16B entries. The user may
explicitly disable 64B entries and use 16B entries by setting
``devarg`` parameter ``cq64=0``. For example::

    -a 12:00.0,cq64=0

To verify the selected entry size, enable debug logging
(``--log-level=enic,debug``) and check the following messages.

.. code-block:: console

    PMD: rte_enic_pmd: Supported CQ entry sizes: 16 32
    PMD: rte_enic_pmd: Using 16B CQ entry size

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
  as l3fwd may not account for VLAN tags in packets and may misbehave. One
  solution is to enable VLAN stripping on ingress so the VLAN tag is removed
  from the packet and put into the mbuf->vlan_tci field. Here is an example
  of how to accomplish this:

.. code-block:: console

     vlan_offload = rte_eth_dev_get_vlan_offload(port);
     vlan_offload |= RTE_ETH_VLAN_STRIP_OFFLOAD;
     rte_eth_dev_set_vlan_offload(port, vlan_offload);

Another alternative is modify the adapter's ingress VLAN rewrite mode so that
packets with the default VLAN tag are stripped by the adapter and presented to
DPDK as untagged packets. In this case mbuf->vlan_tci and the RTE_MBUF_F_RX_VLAN and
RTE_MBUF_F_RX_VLAN_STRIPPED mbuf flags would not be set. This mode is enabled with the
``devargs`` parameter ``ig-vlan-rewrite=untag``. For example::

    -a 12:00.0,ig-vlan-rewrite=untag

- **SR-IOV**

  - KVM hypervisor support only. VMware has not been tested.
  - Requires VM-FEX, and so is only available on UCS managed servers connected
    to Fabric Interconnects. It is not on standalone C-Series servers.
  - VF devices are not usable directly from the host. They can  only be used
    as assigned devices on VM instances.
  - Currently, unbind of the ENIC kernel mode driver 'enic.ko' on the VM
    instance may hang. As a workaround, enic.ko should be blocked or removed
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
  - The supported range of IDs for the 'MARK' action is 0 - 0xFFFD.
  - RSS and PASSTHRU actions only support "receive normally". They are limited
    to supporting MARK + RSS and PASSTHRU + MARK to allow the application to mark
    packets and then receive them normally. These require 1400 series VIC adapters
    and latest firmware.
  - RAW items are limited to matching UDP tunnel headers like VXLAN.
  - GTP, GTP-C and GTP-U header matching is enabled, however matching items within
    the tunnel is not supported.
  - For 1400 VICs, all flows using the RSS action on a port use same hash
    configuration. The RETA is ignored. The queues used in the RSS group must be
    sequential. There is a performance hit if the number of queues is not a power of 2.
    Only level 0 (outer header) RSS is allowed.

- **Statistics**

  - ``rx_good_bytes`` (ibytes) always includes VLAN header (4B) and CRC bytes (4B).
    This behavior applies to 1300 and older series VIC adapters.
    1400 series VICs do not count CRC bytes, and count VLAN header only when VLAN
    stripping is disabled.
  - When the NIC drops a packet because the Rx queue has no free buffers,
    ``rx_good_bytes`` still increments by 4B if the packet is not VLAN tagged or
    VLAN stripping is disabled, or by 8B if the packet is VLAN tagged and stripping
    is enabled.
    This behavior applies to 1300 and older series VIC adapters. 1400 series VICs
    do not increment this byte counter when packets are dropped.

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

Supported Operating Systems
---------------------------

Any Linux distribution fulfilling the conditions described in Dependencies
section of DPDK documentation.

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

Additional Reference
--------------------

- https://www.cisco.com/c/en/us/products/servers-unified-computing/index.html
- https://www.cisco.com/c/en/us/products/interfaces-modules/unified-computing-system-adapters/index.html
