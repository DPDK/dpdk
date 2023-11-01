..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2015 6WIND S.A.
    Copyright 2015 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

NVIDIA MLX5 Ethernet Driver
===========================

.. note::

   NVIDIA acquired Mellanox Technologies in 2020.
   The DPDK documentation and code might still include instances
   of or references to Mellanox trademarks (like BlueField and ConnectX)
   that are now NVIDIA trademarks.

The mlx5 Ethernet poll mode driver library (**librte_net_mlx5**) provides support
for **NVIDIA ConnectX-4**, **NVIDIA ConnectX-4 Lx** , **NVIDIA ConnectX-5**,
**NVIDIA ConnectX-6**, **NVIDIA ConnectX-6 Dx**, **NVIDIA ConnectX-6 Lx**,
**NVIDIA ConnectX-7**, **NVIDIA BlueField**, **NVIDIA BlueField-2** and
**NVIDIA BlueField-3** families of 10/25/40/50/100/200/400 Gb/s adapters
as well as their virtual functions (VF) in SR-IOV context.

Supported NICs
--------------

The following NVIDIA device families are supported by the same mlx5 driver:

  - ConnectX-4
  - ConnectX-4 Lx
  - ConnectX-5
  - ConnectX-5 Ex
  - ConnectX-6
  - ConnectX-6 Dx
  - ConnectX-6 Lx
  - ConnectX-7
  - BlueField
  - BlueField-2
  - BlueField-3

Below are detailed device names:

* NVIDIA\ |reg| ConnectX\ |reg|-4 10G MCX4111A-XCAT (1x10G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 10G MCX412A-XCAT (2x10G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 25G MCX4111A-ACAT (1x25G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 25G MCX412A-ACAT (2x25G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 40G MCX413A-BCAT (1x40G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 40G MCX4131A-BCAT (1x40G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 40G MCX415A-BCAT (1x40G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX413A-GCAT (1x50G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX4131A-GCAT (1x50G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX414A-BCAT (2x50G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX415A-GCAT (1x50G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX416A-BCAT (2x50G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX416A-GCAT (2x50G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 50G MCX415A-CCAT (1x100G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 100G MCX416A-CCAT (2x100G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 10G MCX4111A-XCAT (1x10G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 10G MCX4121A-XCAT (2x10G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4111A-ACAT (1x25G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)
* NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 40G MCX4131A-BCAT (1x40G)
* NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)
* NVIDIA\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)
* NVIDIA\ |reg| ConnectX\ |reg|-6 200G MCX654106A-HCAT (2x200G)
* NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)
* NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 200G MCX623105AN-VDAT (1x200G)
* NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)
* NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)
* NVIDIA\ |reg| BlueField\ |reg|-2 25G MBF2H332A-AEEOT_A1 (2x25Gg
* NVIDIA\ |reg| BlueField\ |reg|-3 200GbE 900-9D3B6-00CV-AA0 (2x200)
* NVIDIA\ |reg| BlueField\ |reg|-3 200GbE 900-9D3B6-00SV-AA0 (2x200)
* NVIDIA\ |reg| BlueField\ |reg|-3 400GbE 900-9D3B6-00CN-AB0 (2x400)
* NVIDIA\ |reg| BlueField\ |reg|-3 100GbE 900-9D3B4-00CC-EA0 (2x100)
* NVIDIA\ |reg| BlueField\ |reg|-3 100GbE 900-9D3B4-00SC-EA0 (2x100)
* NVIDIA\ |reg| BlueField\ |reg|-3 400GbE 900-9D3B4-00EN-EA0 (1x100)


Design
------

Besides its dependency on libibverbs (that implies libmlx5 and associated
kernel support), librte_net_mlx5 relies heavily on system calls for control
operations such as querying/updating the MTU and flow control parameters.

This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets as
long as they share the same MAC address.
This means legacy linux control tools (for example: ethtool, ifconfig and
more) can operate on the same network interfaces that owned by the DPDK
application.

See :doc:`../../platform/mlx5` guide for more design details,
including prerequisites installation.

Features
--------

- Multi arch support: x86_64, POWER8, ARMv8, i686.
- Multiple TX and RX queues.
- Shared Rx queue.
- Rx queue delay drop.
- Rx queue available descriptor threshold event.
- Host shaper support.
- Support steering for external Rx queue created outside the PMD.
- Support for scattered TX frames.
- Advanced support for scattered Rx frames with tunable buffer attributes.
- IPv4, IPv6, TCPv4, TCPv6, UDPv4 and UDPv6 RSS on any number of queues.
- RSS using different combinations of fields: L3 only, L4 only or both,
  and source only, destination only or both.
- Several RSS hash keys, one for each flow type.
- Default RSS operation with no hash key specification.
- Symmetric RSS function.
- Configurable RETA table.
- Link flow control (pause frame).
- Support for multiple MAC addresses.
- VLAN filtering.
- RX VLAN stripping.
- TX VLAN insertion.
- RX CRC stripping configuration.
- TX mbuf fast free offload.
- Promiscuous mode on PF and VF.
- Multicast promiscuous mode on PF and VF.
- Hardware checksum offloads.
- Flow director (RTE_FDIR_MODE_PERFECT, RTE_FDIR_MODE_PERFECT_MAC_VLAN and
  RTE_ETH_FDIR_REJECT).
- Flow API, including :ref:`flow_isolated_mode`.
- Multiple process.
- KVM and VMware ESX SR-IOV modes are supported.
- RSS hash result is supported.
- Hardware TSO for generic IP or UDP tunnel, including VXLAN and GRE.
- Hardware checksum Tx offload for generic IP or UDP tunnel, including VXLAN and GRE.
- RX interrupts.
- Statistics query including Basic, Extended and per queue.
- Rx HW timestamp.
- Tunnel types: VXLAN, L3 VXLAN, VXLAN-GPE, GRE, MPLSoGRE, MPLSoUDP, IP-in-IP, Geneve, GTP.
- Tunnel HW offloads: packet type, inner/outer RSS, IP and UDP checksum verification.
- NIC HW offloads: encapsulation (vxlan, gre, mplsoudp, mplsogre), NAT, routing, TTL
  increment/decrement, count, drop, mark. For details please see :ref:`mlx5_offloads_support`.
- Flow insertion rate of more then million flows per second, when using Direct Rules.
- Support for multiple rte_flow groups.
- Per packet no-inline hint flag to disable packet data copying into Tx descriptors.
- Hardware LRO.
- Hairpin.
- Multiple-thread flow insertion.
- Matching on IPv4 Internet Header Length (IHL).
- Matching on IPv6 routing extension header.
- Matching on GTP extension header with raw encap/decap action.
- Matching on Geneve TLV option header with raw encap/decap action.
- Matching on ESP header SPI field.
- Matching on InfiniBand BTH.
- Modify IPv4/IPv6 ECN field.
- Push or remove IPv6 routing extension.
- RSS support in sample action.
- E-Switch mirroring and jump.
- E-Switch mirroring and modify.
- Send to kernel.
- 21844 flow priorities for ingress or egress flow groups greater than 0 and for any transfer
  flow group.
- Flow quota.
- Flow metering, including meter policy API.
- Flow meter hierarchy.
- Flow meter mark.
- Flow integrity offload API.
- Connection tracking.
- Sub-Function representors.
- Sub-Function.
- Matching on represented port.
- Matching on aggregated affinity.


Limitations
-----------

- Windows support:

  On Windows, the features are limited:

  - Promiscuous mode is not supported
  - The following rules are supported:

    - IPv4/UDP with CVLAN filtering
    - Unicast MAC filtering

  - Additional rules are supported from WinOF2 version 2.70:

    - IPv4/TCP with CVLAN filtering
    - L4 steering rules for port RSS of UDP, TCP and IP

- For secondary process:

  - Forked secondary process not supported.
  - MPRQ is not supported. Callback to free externally attached MPRQ buffer is set
    in a primary process, but has a different virtual address in a secondary process.
    Calling a function at the wrong address leads to a segmentation fault.
  - External memory unregistered in EAL memseg list cannot be used for DMA
    unless such memory has been registered by ``mlx5_mr_update_ext_mp()`` in
    primary process and remapped to the same virtual address in secondary
    process. If the external memory is registered by primary process but has
    different virtual address in secondary process, unexpected error may happen.

- Shared Rx queue:

  - Counters of received packets and bytes number of devices in same share group are same.
  - Counters of received packets and bytes number of queues in same group and queue ID are same.

- Available descriptor threshold event:

  - Does not support shared Rx queue and hairpin Rx queue.

- The symmetric RSS function is supported by swapping source and destination
  addresses and ports.

- Host shaper:

  - Support BlueField series NIC from BlueField-2.
  - When configuring host shaper with ``RTE_PMD_MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED`` flag,
    only rates 0 and 100Mbps are supported.

- HW steering:

  - WQE based high scaling and safer flow insertion/destruction.
  - Set ``dv_flow_en`` to 2 in order to enable HW steering.
  - Async queue-based ``rte_flow_async`` APIs supported only.
  - NIC ConnectX-5 and before are not supported.
  - Reconfiguring flow API engine is not supported.
    Any subsequent call to ``rte_flow_configure()`` with different configuration
    than initially provided will be rejected with ``-ENOTSUP`` error code.
  - Partial match with item template is not supported.
  - IPv6 5-tuple matching is not supported.
  - With E-Switch enabled, ports which share the E-Switch domain
    should be started and stopped in a specific order:

    - When starting ports, the transfer proxy port should be started first
      and port representors should follow.
    - When stopping ports, all of the port representors
      should be stopped before stopping the transfer proxy port.

    If ports are started/stopped in an incorrect order,
    ``rte_eth_dev_start()``/``rte_eth_dev_stop()`` will return an appropriate error code:

    - ``-EAGAIN`` for ``rte_eth_dev_start()``.
    - ``-EBUSY`` for ``rte_eth_dev_stop()``.

  - Matching on ICMP6 following IPv6 routing extension header,
    should match ``ipv6_routing_ext_next_hdr`` instead of ICMP6.

- When using Verbs flow engine (``dv_flow_en`` = 0), flow pattern without any
  specific VLAN will match for VLAN packets as well:

  When VLAN spec is not specified in the pattern, the matching rule will be created with VLAN as a wild card.
  Meaning, the flow rule::

        flow create 0 ingress pattern eth / vlan vid is 3 / ipv4 / end ...

  Will only match vlan packets with vid=3. and the flow rule::

        flow create 0 ingress pattern eth / ipv4 / end ...

  Will match any ipv4 packet (VLAN included).

- When using Verbs flow engine (``dv_flow_en`` = 0), multi-tagged(QinQ) match is not supported.

- When using DV flow engine (``dv_flow_en`` = 1), flow pattern with any VLAN specification will match only single-tagged packets unless the ETH item ``type`` field is 0x88A8 or the VLAN item ``has_more_vlan`` field is 1.
  The flow rule::

        flow create 0 ingress pattern eth / ipv4 / end ...

  Will match any ipv4 packet.
  The flow rules::

        flow create 0 ingress pattern eth / vlan / end ...
        flow create 0 ingress pattern eth has_vlan is 1 / end ...
        flow create 0 ingress pattern eth type is 0x8100 / end ...

  Will match single-tagged packets only, with any VLAN ID value.
  The flow rules::

        flow create 0 ingress pattern eth type is 0x88A8 / end ...
        flow create 0 ingress pattern eth / vlan has_more_vlan is 1 / end ...

  Will match multi-tagged packets only, with any VLAN ID value.

- A flow pattern with 2 sequential VLAN items is not supported.

- VLAN pop offload command:

  - Flow rules having a VLAN pop offload command as one of their actions and
    are lacking a match on VLAN as one of their items are not supported.
  - The command is not supported on egress traffic in NIC mode.

- VLAN push offload is not supported on ingress traffic in NIC mode.

- VLAN set PCP offload is not supported on existing headers.

- A multi segment packet must have not more segments than reported by dev_infos_get()
  in tx_desc_lim.nb_seg_max field. This value depends on maximal supported Tx descriptor
  size and ``txq_inline_min`` settings and may be from 2 (worst case forced by maximal
  inline settings) to 58.

- Match on VXLAN supports the following fields only:

     - VNI
     - Last reserved 8-bits

  Last reserved 8-bits matching is only supported When using DV flow
  engine (``dv_flow_en`` = 1).
  For ConnectX-5, the UDP destination port must be the standard one (4789).
  Group zero's behavior may differ which depends on FW.
  Matching value equals 0 (value & mask) is not supported.

- L3 VXLAN and VXLAN-GPE tunnels cannot be supported together with MPLSoGRE and MPLSoUDP.

- MPLSoGRE is not supported in HW steering (``dv_flow_en`` = 2).

- MPLSoUDP with multiple MPLS headers is only supported in HW steering (``dv_flow_en`` = 2).

- Match on Geneve header supports the following fields only:

     - VNI
     - OAM
     - protocol type
     - options length

- Match on Geneve TLV option is supported on the following fields:

     - Class
     - Type
     - Length
     - Data

  Only one Class/Type/Length Geneve TLV option is supported per shared device.
  Class/Type/Length fields must be specified as well as masks.
  Class/Type/Length specified masks must be full.
  Matching Geneve TLV option without specifying data is not supported.
  Matching Geneve TLV option with ``data & mask == 0`` is not supported.

- VF: flow rules created on VF devices can only match traffic targeted at the
  configured MAC addresses (see ``rte_eth_dev_mac_addr_add()``).

- Match on GTP tunnel header item supports the following fields only:

     - v_pt_rsv_flags: E flag, S flag, PN flag
     - msg_type
     - teid

- Match on GTP extension header only for GTP PDU session container (next
  extension header type = 0x85).
- Match on GTP extension header is not supported in group 0.

- When using DV/Verbs flow engine (``dv_flow_en`` = 1/0 respectively),
  match on SPI field in ESP header for group 0 is supported from ConnectX-7.

- Matching on SPI field in ESP header is supported over the PF only.

- Flex item:

  - Hardware support: **NVIDIA BlueField-2** and **NVIDIA BlueField-3**.
  - Flex item is supported on PF only.
  - Hardware limits ``header_length_mask_width`` up to 6 bits.
  - Firmware supports 8 global sample fields.
    Each flex item allocates non-shared sample fields from that pool.
  - Supported flex item can have 1 input link - ``eth`` or ``udp``
    and up to 3 output links - ``ipv4`` or ``ipv6``.
  - Flex item fields (``next_header``, ``next_protocol``, ``samples``)
    do not participate in RSS hash functions.
  - In flex item configuration, ``next_header.field_base`` value
    must be byte aligned (multiple of 8).
  - Modify field with flex item, the offset must be byte aligned (multiple of 8).

- No Tx metadata go to the E-Switch steering domain for the Flow group 0.
  The flows within group 0 and set metadata action are rejected by hardware.

.. note::

   MAC addresses not already present in the bridge table of the associated
   kernel network device will be added and cleaned up by the PMD when closing
   the device. In case of ungraceful program termination, some entries may
   remain present and should be removed manually by other means.

- Buffer split offload is supported with regular Rx burst routine only,
  no MPRQ feature or vectorized code can be engaged.

- When Multi-Packet Rx queue is configured (``mprq_en``), a Rx packet can be
  externally attached to a user-provided mbuf with having RTE_MBUF_F_EXTERNAL in
  ol_flags. As the mempool for the external buffer is managed by PMD, all the
  Rx mbufs must be freed before the device is closed. Otherwise, the mempool of
  the external buffers will be freed by PMD and the application which still
  holds the external buffers may be corrupted.
  User-managed mempools with external pinned data buffers
  cannot be used in conjunction with MPRQ
  since packets may be already attached to PMD-managed external buffers.

- If Multi-Packet Rx queue is configured (``mprq_en``) and Rx CQE compression is
  enabled (``rxq_cqe_comp_en``) at the same time, RSS hash result is not fully
  supported. Some Rx packets may not have RTE_MBUF_F_RX_RSS_HASH.

- IPv6 Multicast messages are not supported on VM, while promiscuous mode
  and allmulticast mode are both set to off.
  To receive IPv6 Multicast messages on VM, explicitly set the relevant
  MAC address using rte_eth_dev_mac_addr_add() API.

- To support a mixed traffic pattern (some buffers from local host memory, some
  buffers from other devices) with high bandwidth, a mbuf flag is used.

  An application hints the PMD whether or not it should try to inline the
  given mbuf data buffer. PMD should do the best effort to act upon this request.

  The hint flag ``RTE_PMD_MLX5_FINE_GRANULARITY_INLINE`` is dynamic,
  registered by application with rte_mbuf_dynflag_register(). This flag is
  purely driver-specific and declared in PMD specific header ``rte_pmd_mlx5.h``,
  which is intended to be used by the application.

  To query the supported specific flags in runtime,
  the function ``rte_pmd_mlx5_get_dyn_flag_names`` returns the array of
  currently (over present hardware and configuration) supported specific flags.
  The "not inline hint" feature operating flow is the following one:

    - application starts
    - probe the devices, ports are created
    - query the port capabilities
    - if port supporting the feature is found
    - register dynamic flag ``RTE_PMD_MLX5_FINE_GRANULARITY_INLINE``
    - application starts the ports
    - on ``dev_start()`` PMD checks whether the feature flag is registered and
      enables the feature support in datapath
    - application might set the registered flag bit in ``ol_flags`` field
      of mbuf being sent and PMD will handle ones appropriately.

- The amount of descriptors in Tx queue may be limited by data inline settings.
  Inline data require the more descriptor building blocks and overall block
  amount may exceed the hardware supported limits. The application should
  reduce the requested Tx size or adjust data inline settings with
  ``txq_inline_max`` and ``txq_inline_mpw`` devargs keys.

- To provide the packet send scheduling on mbuf timestamps the ``tx_pp``
  parameter should be specified.
  When PMD sees the RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME set on the packet
  being sent it tries to synchronize the time of packet appearing on
  the wire with the specified packet timestamp. It the specified one
  is in the past it should be ignored, if one is in the distant future
  it should be capped with some reasonable value (in range of seconds).
  These specific cases ("too late" and "distant future") can be optionally
  reported via device xstats to assist applications to detect the
  time-related problems.

  The timestamp upper "too-distant-future" limit
  at the moment of invoking the Tx burst routine
  can be estimated as ``tx_pp`` option (in nanoseconds) multiplied by 2^23.
  Please note, for the testpmd txonly mode,
  the limit is deduced from the expression::

        (n_tx_descriptors / burst_size + 1) * inter_burst_gap

  There is no any packet reordering according timestamps is supposed,
  neither within packet burst, nor between packets, it is an entirely
  application responsibility to generate packets and its timestamps
  in desired order. The timestamps can be put only in the first packet
  in the burst providing the entire burst scheduling.

- E-Switch decapsulation Flow:

  - can be applied to PF port only.
  - must specify VF port action (packet redirection from PF to VF).
  - optionally may specify tunnel inner source and destination MAC addresses.

- E-Switch  encapsulation Flow:

  - can be applied to VF ports only.
  - must specify PF port action (packet redirection from VF to PF).

- E-Switch Manager matching:

  - For BlueField with old FW
    which doesn't expose the E-Switch Manager vport ID in the capability,
    matching E-Switch Manager should be used only in BlueField embedded CPU mode.

- Raw encapsulation:

  - The input buffer, used as outer header, is not validated.

- Raw decapsulation:

  - The decapsulation is always done up to the outermost tunnel detected by the HW.
  - The input buffer, providing the removal size, is not validated.
  - The buffer size must match the length of the headers to be removed.

- Outer UDP checksum calculation for encapsulation flow actions:

  - Currently available NVIDIA NICs and DPUs do not have a capability to calculate
    the UDP checksum in the header added using encapsulation flow actions.

    Applications are required to use 0 in UDP checksum field in such flow actions.
    Resulting packet will have outer UDP checksum equal to 0.

- ICMP(code/type/identifier/sequence number) / ICMP6(code/type/identifier/sequence number) matching,
  IP-in-IP and MPLS flow matching are all mutually exclusive features which cannot be supported together
  (see :ref:`mlx5_firmware_config`).

- LRO:

  - Requires DevX and DV flow to be enabled.
  - KEEP_CRC offload cannot be supported with LRO.
  - The first mbuf length, without head-room,  must be big enough to include the
    TCP header (122B).
  - Rx queue with LRO offload enabled, receiving a non-LRO packet, can forward
    it with size limited to max LRO size, not to max RX packet length.
  - The driver rounds down the port configuration value ``max_lro_pkt_size``
    (from ``rte_eth_rxmode``) to a multiple of 256 due to hardware limitation.
  - LRO can be used with outer header of TCP packets of the standard format:
        eth (with or without vlan) / ipv4 or ipv6 / tcp / payload

    Other TCP packets (e.g. with MPLS label) received on Rx queue with LRO enabled, will be received with bad checksum.
  - LRO packet aggregation is performed by HW only for packet size larger than
    ``lro_min_mss_size``. This value is reported on device start, when debug
    mode is enabled.

- CRC:

  - ``RTE_ETH_RX_OFFLOAD_KEEP_CRC`` cannot be supported with decapsulation
    for some NICs (such as ConnectX-6 Dx, ConnectX-6 Lx, ConnectX-7, BlueField-2,
    and BlueField-3).
    The capability bit ``scatter_fcs_w_decap_disable`` shows NIC support.

- TX mbuf fast free:

  - fast free offload assumes the all mbufs being sent are originated from the
    same memory pool and there is no any extra references to the mbufs (the
    reference counter for each mbuf is equal 1 on tx_burst call). The latter
    means there should be no any externally attached buffers in mbufs. It is
    an application responsibility to provide the correct mbufs if the fast
    free offload is engaged. The mlx5 PMD implicitly produces the mbufs with
    externally attached buffers if MPRQ option is enabled, hence, the fast
    free offload is neither supported nor advertised if there is MPRQ enabled.

- Sample flow:

  - Supports ``RTE_FLOW_ACTION_TYPE_SAMPLE`` action only within NIC Rx and
    E-Switch steering domain.
  - For E-Switch Sampling flow with sample ratio > 1, additional actions are not
    supported in the sample actions list.
  - For ConnectX-5, the ``RTE_FLOW_ACTION_TYPE_SAMPLE`` is typically used as
    first action in the E-Switch egress flow if with header modify or
    encapsulation actions.
  - For NIC Rx flow, supports ``MARK``, ``COUNT``, ``QUEUE``, ``RSS`` in the
    sample actions list.
  - For E-Switch mirroring flow, supports ``RAW_ENCAP``, ``PORT_ID``,
    ``VXLAN_ENCAP``, ``NVGRE_ENCAP`` in the sample actions list.
  - For E-Switch mirroring flow with sample ratio = 1, the ``ENCAP`` action
    supports uplink port only.
  - For E-Switch mirroring flow with sample ratio = 1, the ``PORT`` and ``JUMP`` actions
    are not supported without presented ``ENCAP`` action in the sample actions list.
  - For ConnectX-5 trusted device, the application metadata with SET_TAG index 0
    is not supported before ``RTE_FLOW_ACTION_TYPE_SAMPLE`` action.

- Modify Field flow:

  - Supports the 'set' and 'add' operations for ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD`` action.
  - Modification of an arbitrary place in a packet via the special ``RTE_FLOW_FIELD_START`` Field ID is not supported.
  - Modification of the MPLS header is supported only in HWS and only to copy from,
    the encapsulation level is always 0.
  - Modification of the 802.1Q Tag, VXLAN Network or GENEVE Network ID's is not supported.
  - Encapsulation levels are not supported, can modify outermost header fields only.
  - Offsets cannot skip past the boundary of a field.
  - If the field type is ``RTE_FLOW_FIELD_MAC_TYPE``
    and packet contains one or more VLAN headers,
    the meaningful type field following the last VLAN header
    is used as modify field operation argument.
    The modify field action is not intended to modify VLAN headers type field,
    dedicated VLAN push and pop actions should be used instead.
  - For packet fields (e.g. MAC addresses, IPv4 addresses or L4 ports)
    offset specifies the number of bits to skip from field's start,
    starting from MSB in the first byte, in the network order.
  - For flow metadata fields (e.g. META or TAG)
    offset specifies the number of bits to skip from field's start,
    starting from LSB in the least significant byte, in the host order.

- Age action:

  - with HW steering (``dv_flow_en=2``)

    - Using the same indirect count action combined with multiple age actions
      in different flows may cause a wrong age state for the age actions.
    - Creating/destroying flow rules with indirect age action when it is active
      (timeout != 0) may cause a wrong age state for the indirect age action.

    - The driver reuses counters for aging action, so for optimization
      the values in ``rte_flow_port_attr`` structure should describe:

      - ``nb_counters`` is the number of flow rules using counter (with/without age)
        in addition to flow rules using only age (without count action).
      - ``nb_aging_objects`` is the number of flow rules containing age action.

- IPv6 header item 'proto' field, indicating the next header protocol, should
  not be set as extension header.
  In case the next header is an extension header, it should not be specified in
  IPv6 header item 'proto' field.
  The last extension header item 'next header' field can specify the following
  header protocol type.

- Match on IPv6 routing extension header supports the following fields only:

  - ``type``
  - ``next_hdr``
  - ``segments_left``

  Only supports HW steering (``dv_flow_en=2``).

- IPv6 routing extension push/remove:

  - Supported only with HW Steering enabled (``dv_flow_en=2``).
  - Supported in non-zero group
    (no limits on transfer domain if ``fdb_def_rule_en=1`` which is default).
  - Only supports TCP or UDP as next layer.
  - IPv6 routing header must be the only present extension.
  - Not supported on guest port.

- Hairpin:

  - Hairpin between two ports could only manual binding and explicit Tx flow mode. For single port hairpin, all the combinations of auto/manual binding and explicit/implicit Tx flow mode could be supported.
  - Hairpin in switchdev SR-IOV mode is not supported till now.

- Quota:

  - Quota implemented for HWS / template API.
  - Maximal value for quota SET and ADD operations in INT32_MAX (2GB).
  - Application cannot use 2 consecutive ADD updates.
    Next tokens update after ADD must always be SET.
  - Quota flow action cannot be used with Meter or CT flow actions in the same rule.
  - Quota flow action and item supported in non-root HWS tables.
  - Maximal number of HW quota and HW meter objects <= 16e6.

- Meter:

  - All the meter colors with drop action will be counted only by the global drop statistics.
  - Yellow detection is only supported with ASO metering.
  - Red color must be with drop action.
  - Meter statistics are supported only for drop case.
  - A meter action created with pre-defined policy must be the last action in the flow except single case where the policy actions are:
     - green: NULL or END.
     - yellow: NULL or END.
     - RED: DROP / END.
  - The only supported meter policy actions:
     - green: QUEUE, RSS, PORT_ID, REPRESENTED_PORT, JUMP, DROP, MODIFY_FIELD, MARK, METER and SET_TAG.
     - yellow: QUEUE, RSS, PORT_ID, REPRESENTED_PORT, JUMP, DROP, MODIFY_FIELD, MARK, METER and SET_TAG.
     - RED: must be DROP.
  - Policy actions of RSS for green and yellow should have the same configuration except queues.
  - Policy with RSS/queue action is not supported when ``dv_xmeta_en`` enabled.
  - If green action is METER, yellow action must be the same METER action or NULL.
  - meter profile packet mode is supported.
  - meter profiles of RFC2697, RFC2698 and RFC4115 are supported.
  - RFC4115 implementation is following MEF, meaning yellow traffic may reclaim unused green bandwidth when green token bucket is full.
  - When using DV flow engine (``dv_flow_en`` = 1),
    if meter has drop count
    or meter hierarchy contains any meter that uses drop count,
    it cannot be used by flow rule matching all ports.
  - When using DV flow engine (``dv_flow_en`` = 1),
    if meter hierarchy contains any meter that has MODIFY_FIELD/SET_TAG,
    it cannot be used by flow matching all ports.
  - When using HWS flow engine (``dv_flow_en`` = 2),
    only meter mark action is supported.

- Ptype:

  - Only supports HW steering (``dv_flow_en=2``).
  - The supported values are:
    L2: ``RTE_PTYPE_L2_ETHER``, ``RTE_PTYPE_L2_ETHER_VLAN``, ``RTE_PTYPE_L2_ETHER_QINQ``
    L3: ``RTE_PTYPE_L3_IPV4``, ``RTE_PTYPE_L3_IPV6``
    L4: ``RTE_PTYPE_L4_TCP``, ``RTE_PTYPE_L4_UDP``, ``RTE_PTYPE_L4_ICMP``
    and their ``RTE_PTYPE_INNER_XXX`` counterparts as well as ``RTE_PTYPE_TUNNEL_ESP``.
    Any other values are not supported. Using them as a value will cause unexpected behavior.
  - Matching on both outer and inner IP fragmented is supported
    using ``RTE_PTYPE_L4_FRAG`` and ``RTE_PTYPE_INNER_L4_FRAG`` values.
    They are not part of L4 types, so they should be provided explicitly
    as a mask value during pattern template creation.
    Providing ``RTE_PTYPE_L4_MASK`` during pattern template creation
    and ``RTE_PTYPE_L4_FRAG`` during flow rule creation
    will cause unexpected behavior.

- Integrity:

  - Verification bits provided by the hardware are ``l3_ok``, ``ipv4_csum_ok``, ``l4_ok``, ``l4_csum_ok``.
  - ``level`` value 0 references outer headers.
  - Negative integrity item verification is not supported.

  - With SW steering (``dv_flow_en=1``)

    - Integrity offload is enabled starting from **ConnectX-6 Dx**.
    - Multiple integrity items not supported in a single flow rule.
    - Flow rule items supplied by application must explicitly specify
      network headers referred by integrity item.

      For example, if integrity item mask sets ``l4_ok`` or ``l4_csum_ok`` bits,
      reference to L4 network header, TCP or UDP, must be in the rule pattern as well::

         flow create 0 ingress pattern integrity level is 0 value mask l3_ok value spec l3_ok / eth / ipv6 / end ...
         flow create 0 ingress pattern integrity level is 0 value mask l4_ok value spec l4_ok / eth / ipv4 proto is udp / end ...

  - With HW steering (``dv_flow_en=2``)
    - The ``l3_ok`` field represents all L3 checks, but nothing about IPv4 checksum.
    - The ``l4_ok`` field represents all L4 checks including L4 checksum.

- Connection tracking:

  - Cannot co-exist with ASO meter, ASO age action in a single flow rule.
  - Flow rules insertion rate and memory consumption need more optimization.
  - 256 ports maximum.
  - 4M connections maximum with ``dv_flow_en`` 1 mode. 16M with ``dv_flow_en`` 2.

- Multi-thread flow insertion:

  - In order to achieve best insertion rate, application should manage the flows per lcore.
  - Better to disable memory reclaim by setting ``reclaim_mem_mode`` to 0 to accelerate the flow object allocation and release with cache.

- HW hashed bonding

  - TXQ affinity subjects to HW hash once enabled.

- Bonding under socket direct mode

  - Needs MLNX_OFED 5.4+.

- Match on aggregated affinity:

  - Supports NIC ingress flow in group 0.
  - Supports E-Switch flow in group 0 and depends on
    device-managed flow steering (DMFS) mode.

- Timestamps:

  - CQE timestamp field width is limited by hardware to 63 bits, MSB is zero.
  - In the free-running mode the timestamp counter is reset on power on
    and 63-bit value provides over 1800 years of uptime till overflow.
  - In the real-time mode
    (configurable with ``REAL_TIME_CLOCK_ENABLE`` firmware settings),
    the timestamp presents the nanoseconds elapsed since 01-Jan-1970,
    hardware timestamp overflow will happen on 19-Jan-2038
    (0x80000000 seconds since 01-Jan-1970).
  - The send scheduling is based on timestamps
    from the reference "Clock Queue" completions,
    the scheduled send timestamps should not be specified with non-zero MSB.

- Match on GRE header supports the following fields:

  - c_rsvd0_v: C bit, K bit, S bit
  - protocol type
  - checksum
  - key
  - sequence

  Matching on checksum and sequence needs MLNX_OFED 5.6+.

- The NIC egress flow rules on representor port are not supported.

- A driver limitation for ``RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR`` action
  restricts the ``port_id`` configuration to only accept the value ``0xffff``,
  indicating the E-Switch manager.
  If the ``repr_matching_en`` flag is enabled, the traffic will be directed
  to the representor of the source virtual port (SF/VF), while if it is disabled,
  the traffic will be routed based on the steering rules in the ingress domain.

- Send to kernel action (``RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL``):

  - Supported on non-root table.
  - Supported in isolated mode.
  - In HW steering (``dv_flow_en`` = 2):
    - not supported on guest port.

- During live migration to a new process set its flow engine as standby mode,
  the user should only program flow rules in group 0 (``fdb_def_rule_en=0``).
  Live migration is only supported under SWS (``dv_flow_en=1``).
  The flow group 0 is shared between DPDK processes
  while the other flow groups are limited to the current process.
  The flow engine of a process cannot move from active to standby mode
  if preceding active application rules are still present and vice versa.


Statistics
----------

MLX5 supports various methods to report statistics:

Port statistics can be queried using ``rte_eth_stats_get()``. The received and sent statistics are through SW only and counts the number of packets received or sent successfully by the PMD. The imissed counter is the amount of packets that could not be delivered to SW because a queue was full. Packets not received due to congestion in the bus or on the NIC can be queried via the rx_discards_phy xstats counter.

Extended statistics can be queried using ``rte_eth_xstats_get()``. The extended statistics expose a wider set of counters counted by the device. The extended port statistics counts the number of packets received or sent successfully by the port. As NVIDIA NICs are using the :ref:`Bifurcated Linux Driver <linux_gsg_linux_drivers>` those counters counts also packet received or sent by the Linux kernel. The counters with ``_phy`` suffix counts the total events on the physical port, therefore not valid for VF.

Finally per-flow statistics can by queried using ``rte_flow_query`` when attaching a count action for specific flow. The flow counter counts the number of packets received successfully by the port and match the specific flow.


Compilation
-----------

See :ref:`mlx5 common compilation <mlx5_common_compilation>`.


Configuration
-------------

Environment Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~

See :ref:`mlx5 common configuration <mlx5_common_env>`.

Firmware configuration
~~~~~~~~~~~~~~~~~~~~~~

See :ref:`mlx5_firmware_config` guide.

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

Please refer to :ref:`mlx5 common options <mlx5_common_driver_options>`
for an additional list of options shared with other mlx5 drivers.

- ``rxq_cqe_comp_en`` parameter [int]

  A nonzero value enables the compression of CQE on RX side. This feature
  allows to save PCI bandwidth and improve performance. Enabled by default.
  Different compression formats are supported in order to achieve the best
  performance for different traffic patterns. Default format depends on
  Multi-Packet Rx queue configuration: Hash RSS format is used in case
  MPRQ is disabled, Checksum format is used in case MPRQ is enabled.

  The lower 3 bits define the CQE compression format:
  Specifying 2 in these bits of the ``rxq_cqe_comp_en`` parameter selects
  the flow tag format for better compression rate in case of flow mark traffic.
  Specifying 3 in these bits selects checksum format.
  Specifying 4 in these bits selects L3/L4 header format for
  better compression rate in case of mixed TCP/UDP and IPv4/IPv6 traffic.
  CQE compression format selection requires DevX to be enabled. If there is
  no DevX enabled/supported the value is reset to 1 by default.

  8th bit defines the CQE compression layout.
  Setting this bit to 1 turns enhanced CQE compression layout on.
  Enhanced CQE compression is designed for better latency and SW utilization.
  This bit is ignored if only the basic CQE compression layout is supported.

  Supported on:

  - x86_64 with ConnectX-4, ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7, BlueField, BlueField-2, and BlueField-3.
  - POWER9 and ARMv8 with ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7 BlueField, BlueField-2, and BlueField-3.

- ``rxq_pkt_pad_en`` parameter [int]

  A nonzero value enables padding Rx packet to the size of cacheline on PCI
  transaction. This feature would waste PCI bandwidth but could improve
  performance by avoiding partial cacheline write which may cause costly
  read-modify-copy in memory transaction on some architectures. Disabled by
  default.

  Supported on:

  - x86_64 with ConnectX-4, ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7, BlueField, BlueField-2, and BlueField-3.
  - POWER8 and ARMv8 with ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7, BlueField, BlueField-2, and BlueField-3.

- ``delay_drop`` parameter [int]

  Bitmask value for the Rx queue delay drop attribute. Bit 0 is used for the
  standard Rx queue and bit 1 is used for the hairpin Rx queue. By default, the
  delay drop is disabled for all Rx queues. It will be ignored if the port does
  not support the attribute even if it is enabled explicitly.

  The packets being received will not be dropped immediately when the WQEs are
  exhausted in a Rx queue with delay drop enabled.

  A timeout value is set in the driver to control the waiting time before
  dropping a packet. Once the timer is expired, the delay drop will be
  deactivated for all the Rx queues with this feature enable. To re-activate
  it, a rearming is needed and it is part of the kernel driver starting from
  MLNX_OFED 5.5.

  To enable / disable the delay drop rearming, the private flag ``dropless_rq``
  can be set and queried via ethtool:

  - ethtool --set-priv-flags <netdev> dropless_rq on (/ off)
  - ethtool --show-priv-flags <netdev>

  The configuration flag is global per PF and can only be set on the PF, once
  it is on, all the VFs', SFs' and representors' Rx queues will share the timer
  and rearming.

- ``mprq_en`` parameter [int]

  A nonzero value enables configuring Multi-Packet Rx queues. Rx queue is
  configured as Multi-Packet RQ if the total number of Rx queues is
  ``rxqs_min_mprq`` or more. Disabled by default.

  Multi-Packet Rx Queue (MPRQ a.k.a Striding RQ) can further save PCIe bandwidth
  by posting a single large buffer for multiple packets. Instead of posting a
  buffers per a packet, one large buffer is posted in order to receive multiple
  packets on the buffer. A MPRQ buffer consists of multiple fixed-size strides
  and each stride receives one packet. MPRQ can improve throughput for
  small-packet traffic.

  When MPRQ is enabled, MTU can be larger than the size of
  user-provided mbuf even if RTE_ETH_RX_OFFLOAD_SCATTER isn't enabled. PMD will
  configure large stride size enough to accommodate MTU as long as
  device allows. Note that this can waste system memory compared to enabling Rx
  scatter and multi-segment packet.

- ``mprq_log_stride_num`` parameter [int]

  Log 2 of the number of strides for Multi-Packet Rx queue. Configuring more
  strides can reduce PCIe traffic further. If configured value is not in the
  range of device capability, the default value will be set with a warning
  message. The default value is 4 which is 16 strides per a buffer, valid only
  if ``mprq_en`` is set.

  The size of Rx queue should be bigger than the number of strides.

- ``mprq_log_stride_size`` parameter [int]

  Log 2 of the size of a stride for Multi-Packet Rx queue. Configuring a smaller
  stride size can save some memory and reduce probability of a depletion of all
  available strides due to unreleased packets by an application. If configured
  value is not in the range of device capability, the default value will be set
  with a warning message. The default value is 11 which is 2048 bytes per a
  stride, valid only if ``mprq_en`` is set. With ``mprq_log_stride_size`` set
  it is possible for a packet to span across multiple strides. This mode allows
  support of jumbo frames (9K) with MPRQ. The memcopy of some packets (or part
  of a packet if Rx scatter is configured) may be required in case there is no
  space left for a head room at the end of a stride which incurs some
  performance penalty.

- ``mprq_max_memcpy_len`` parameter [int]

  The maximum length of packet to memcpy in case of Multi-Packet Rx queue. Rx
  packet is mem-copied to a user-provided mbuf if the size of Rx packet is less
  than or equal to this parameter. Otherwise, PMD will attach the Rx packet to
  the mbuf by external buffer attachment - ``rte_pktmbuf_attach_extbuf()``.
  A mempool for external buffers will be allocated and managed by PMD. If Rx
  packet is externally attached, ol_flags field of the mbuf will have
  RTE_MBUF_F_EXTERNAL and this flag must be preserved. ``RTE_MBUF_HAS_EXTBUF()``
  checks the flag. The default value is 128, valid only if ``mprq_en`` is set.

- ``rxqs_min_mprq`` parameter [int]

  Configure Rx queues as Multi-Packet RQ if the total number of Rx queues is
  greater or equal to this value. The default value is 12, valid only if
  ``mprq_en`` is set.

- ``txq_inline`` parameter [int]

  Amount of data to be inlined during TX operations. This parameter is
  deprecated and converted to the new parameter ``txq_inline_max`` providing
  partial compatibility.

- ``txqs_min_inline`` parameter [int]

  Enable inline data send only when the number of TX queues is greater or equal
  to this value.

  This option should be used in combination with ``txq_inline_max`` and
  ``txq_inline_mpw`` below and does not affect ``txq_inline_min`` settings above.

  If this option is not specified the default value 16 is used for BlueField
  and 8 for other platforms

  The data inlining consumes the CPU cycles, so this option is intended to
  auto enable inline data if we have enough Tx queues, which means we have
  enough CPU cores and PCI bandwidth is getting more critical and CPU
  is not supposed to be bottleneck anymore.

  The copying data into WQE improves latency and can improve PPS performance
  when PCI back pressure is detected and may be useful for scenarios involving
  heavy traffic on many queues.

  Because additional software logic is necessary to handle this mode, this
  option should be used with care, as it may lower performance when back
  pressure is not expected.

  If inline data are enabled it may affect the maximal size of Tx queue in
  descriptors because the inline data increase the descriptor size and
  queue size limits supported by hardware may be exceeded.

- ``txq_inline_min`` parameter [int]

  Minimal amount of data to be inlined into WQE during Tx operations. NICs
  may require this minimal data amount to operate correctly. The exact value
  may depend on NIC operation mode, requested offloads, etc. It is strongly
  recommended to omit this parameter and use the default values. Anyway,
  applications using this parameter should take into consideration that
  specifying an inconsistent value may prevent the NIC from sending packets.

  If ``txq_inline_min`` key is present the specified value (may be aligned
  by the driver in order not to exceed the limits and provide better descriptor
  space utilization) will be used by the driver and it is guaranteed that
  requested amount of data bytes are inlined into the WQE beside other inline
  settings. This key also may update ``txq_inline_max`` value (default
  or specified explicitly in devargs) to reserve the space for inline data.

  If ``txq_inline_min`` key is not present, the value may be queried by the
  driver from the NIC via DevX if this feature is available. If there is no DevX
  enabled/supported the value 18 (supposing L2 header including VLAN) is set
  for ConnectX-4 and ConnectX-4 Lx, and 0 is set by default for ConnectX-5
  and newer NICs. If packet is shorter the ``txq_inline_min`` value, the entire
  packet is inlined.

  For ConnectX-4 NIC, driver does not allow specifying value below 18
  (minimal L2 header, including VLAN), error will be raised.

  For ConnectX-4 Lx NIC, it is allowed to specify values below 18, but
  it is not recommended and may prevent NIC from sending packets over
  some configurations.

  For ConnectX-4 and ConnectX-4 Lx NICs, automatically configured value
  is insufficient for some traffic, because they require at least all L2 headers
  to be inlined. For example, Q-in-Q adds 4 bytes to default 18 bytes
  of Ethernet and VLAN, thus ``txq_inline_min`` must be set to 22.
  MPLS would add 4 bytes per label. Final value must account for all possible
  L2 encapsulation headers used in particular environment.

  Please, note, this minimal data inlining disengages eMPW feature (Enhanced
  Multi-Packet Write), because last one does not support partial packet inlining.
  This is not very critical due to minimal data inlining is mostly required
  by ConnectX-4 and ConnectX-4 Lx, these NICs do not support eMPW feature.

- ``txq_inline_max`` parameter [int]

  Specifies the maximal packet length to be completely inlined into WQE
  Ethernet Segment for ordinary SEND method. If packet is larger than specified
  value, the packet data won't be copied by the driver at all, data buffer
  is addressed with a pointer. If packet length is less or equal all packet
  data will be copied into WQE. This may improve PCI bandwidth utilization for
  short packets significantly but requires the extra CPU cycles.

  The data inline feature is controlled by number of Tx queues, if number of Tx
  queues is larger than ``txqs_min_inline`` key parameter, the inline feature
  is engaged, if there are not enough Tx queues (which means not enough CPU cores
  and CPU resources are scarce), data inline is not performed by the driver.
  Assigning ``txqs_min_inline`` with zero always enables the data inline.

  The default ``txq_inline_max`` value is 290. The specified value may be adjusted
  by the driver in order not to exceed the limit (930 bytes) and to provide better
  WQE space filling without gaps, the adjustment is reflected in the debug log.
  Also, the default value (290) may be decreased in run-time if the large transmit
  queue size is requested and hardware does not support enough descriptor
  amount, in this case warning is emitted. If ``txq_inline_max`` key is
  specified and requested inline settings can not be satisfied then error
  will be raised.

- ``txq_inline_mpw`` parameter [int]

  Specifies the maximal packet length to be completely inlined into WQE for
  Enhanced MPW method. If packet is large the specified value, the packet data
  won't be copied, and data buffer is addressed with pointer. If packet length
  is less or equal, all packet data will be copied into WQE. This may improve PCI
  bandwidth utilization for short packets significantly but requires the extra
  CPU cycles.

  The data inline feature is controlled by number of TX queues, if number of Tx
  queues is larger than ``txqs_min_inline`` key parameter, the inline feature
  is engaged, if there are not enough Tx queues (which means not enough CPU cores
  and CPU resources are scarce), data inline is not performed by the driver.
  Assigning ``txqs_min_inline`` with zero always enables the data inline.

  The default ``txq_inline_mpw`` value is 268. The specified value may be adjusted
  by the driver in order not to exceed the limit (930 bytes) and to provide better
  WQE space filling without gaps, the adjustment is reflected in the debug log.
  Due to multiple packets may be included to the same WQE with Enhanced Multi
  Packet Write Method and overall WQE size is limited it is not recommended to
  specify large values for the ``txq_inline_mpw``. Also, the default value (268)
  may be decreased in run-time if the large transmit queue size is requested
  and hardware does not support enough descriptor amount, in this case warning
  is emitted. If ``txq_inline_mpw`` key is  specified and requested inline
  settings can not be satisfied then error will be raised.

- ``txqs_max_vec`` parameter [int]

  Enable vectorized Tx only when the number of TX queues is less than or
  equal to this value. This parameter is deprecated and ignored, kept
  for compatibility issue to not prevent driver from probing.

- ``txq_mpw_hdr_dseg_en`` parameter [int]

  A nonzero value enables including two pointers in the first block of TX
  descriptor. The parameter is deprecated and ignored, kept for compatibility
  issue.

- ``txq_max_inline_len`` parameter [int]

  Maximum size of packet to be inlined. This limits the size of packet to
  be inlined. If the size of a packet is larger than configured value, the
  packet isn't inlined even though there's enough space remained in the
  descriptor. Instead, the packet is included with pointer. This parameter
  is deprecated and converted directly to ``txq_inline_mpw`` providing full
  compatibility. Valid only if eMPW feature is engaged.

- ``txq_mpw_en`` parameter [int]

  A nonzero value enables Enhanced Multi-Packet Write (eMPW) for ConnectX-5,
  ConnectX-6, ConnectX-6 Dx, ConnectX-6 Lx, ConnectX-7, BlueField, BlueField-2
  BlueField-3. eMPW allows the Tx burst function to pack up multiple packets
  in a single descriptor session in order to save PCI bandwidth
  and improve performance at the cost of a slightly higher CPU usage.
  When ``txq_inline_mpw`` is set along with ``txq_mpw_en``,
  Tx burst function copies entire packet data on to Tx descriptor
  instead of including pointer of packet.

  The Enhanced Multi-Packet Write feature is enabled by default if NIC supports
  it, can be disabled by explicit specifying 0 value for ``txq_mpw_en`` option.
  Also, if minimal data inlining is requested by non-zero ``txq_inline_min``
  option or reported by the NIC, the eMPW feature is disengaged.

- ``tx_db_nc`` parameter [int]

  This parameter name is deprecated and ignored.
  The new name for this parameter is ``sq_db_nc``.
  See :ref:`common driver options <mlx5_common_driver_options>`.

- ``tx_pp`` parameter [int]

  If a nonzero value is specified the driver creates all necessary internal
  objects to provide accurate packet send scheduling on mbuf timestamps.
  The positive value specifies the scheduling granularity in nanoseconds,
  the packet send will be accurate up to specified digits. The allowed range is
  from 500 to 1 million of nanoseconds. The negative value specifies the module
  of granularity and engages the special test mode the check the schedule rate.
  By default (if the ``tx_pp`` is not specified) send scheduling on timestamps
  feature is disabled.

  Starting with ConnectX-7 the capability to schedule traffic directly
  on timestamp specified in descriptor is provided,
  no extra objects are needed anymore and scheduling capability
  is advertised and handled regardless ``tx_pp`` parameter presence.

- ``tx_skew`` parameter [int]

  The parameter adjusts the send packet scheduling on timestamps and represents
  the average delay between beginning of the transmitting descriptor processing
  by the hardware and appearance of actual packet data on the wire. The value
  should be provided in nanoseconds and is valid only if ``tx_pp`` parameter is
  specified. The default value is zero.

- ``tx_vec_en`` parameter [int]

  A nonzero value enables Tx vector on ConnectX-5, ConnectX-6, ConnectX-6 Dx,
  ConnectX-6 Lx, ConnectX-7, BlueField, BlueField-2, and BlueField-3 NICs
  if the number of global Tx queues on the port is less than ``txqs_max_vec``.
  The parameter is deprecated and ignored.

- ``rx_vec_en`` parameter [int]

  A nonzero value enables Rx vector if the port is not configured in
  multi-segment otherwise this parameter is ignored.

  Enabled by default.

- ``vf_nl_en`` parameter [int]

  A nonzero value enables Netlink requests from the VF to add/remove MAC
  addresses or/and enable/disable promiscuous/all multicast on the Netdevice.
  Otherwise the relevant configuration must be run with Linux iproute2 tools.
  This is a prerequisite to receive this kind of traffic.

  Enabled by default, valid only on VF devices ignored otherwise.

- ``l3_vxlan_en`` parameter [int]

  A nonzero value allows L3 VXLAN and VXLAN-GPE flow creation. To enable
  L3 VXLAN or VXLAN-GPE, users has to configure firmware and enable this
  parameter. This is a prerequisite to receive this kind of traffic.

  Disabled by default.

- ``dv_xmeta_en`` parameter [int]

  A nonzero value enables extensive flow metadata support if device is
  capable and driver supports it. This can enable extensive support of
  ``MARK`` and ``META`` item of ``rte_flow``. The newly introduced
  ``SET_TAG`` and ``SET_META`` actions do not depend on ``dv_xmeta_en``.

  There are some possible configurations, depending on parameter value:

  - 0, this is default value, defines the legacy mode, the ``MARK`` and
    ``META`` related actions and items operate only within NIC Tx and
    NIC Rx steering domains, no ``MARK`` and ``META`` information crosses
    the domain boundaries. The ``MARK`` item is 24 bits wide, the ``META``
    item is 32 bits wide and match supported on egress only.

  - 1, this engages extensive metadata mode, the ``MARK`` and ``META``
    related actions and items operate within all supported steering domains,
    including FDB, ``MARK`` and ``META`` information may cross the domain
    boundaries. The ``MARK`` item is 24 bits wide, the ``META`` item width
    depends on kernel and firmware configurations and might be 0, 16 or
    32 bits. Within NIC Tx domain ``META`` data width is 32 bits for
    compatibility, the actual width of data transferred to the FDB domain
    depends on kernel configuration and may be vary. The actual supported
    width can be retrieved in runtime by series of rte_flow_validate()
    trials.

  - 2, this engages extensive metadata mode, the ``MARK`` and ``META``
    related actions and items operate within all supported steering domains,
    including FDB, ``MARK`` and ``META`` information may cross the domain
    boundaries. The ``META`` item is 32 bits wide, the ``MARK`` item width
    depends on kernel and firmware configurations and might be 0, 16 or
    24 bits. The actual supported width can be retrieved in runtime by
    series of rte_flow_validate() trials.

  - 3, this engages tunnel offload mode. In E-Switch configuration, that
    mode implicitly activates ``dv_xmeta_en=1``.

  - 4, this mode is only supported in HWS (``dv_flow_en=2``).
    The Rx/Tx metadata with 32b width copy between FDB and NIC is supported.
    The mark is only supported in NIC and there is no copy supported.

  +------+-----------+-----------+-------------+-------------+
  | Mode | ``MARK``  | ``META``  | ``META`` Tx | FDB/Through |
  +======+===========+===========+=============+=============+
  | 0    | 24 bits   | 32 bits   | 32 bits     | no          |
  +------+-----------+-----------+-------------+-------------+
  | 1    | 24 bits   | vary 0-32 | 32 bits     | yes         |
  +------+-----------+-----------+-------------+-------------+
  | 2    | vary 0-24 | 32 bits   | 32 bits     | yes         |
  +------+-----------+-----------+-------------+-------------+

  If there is no E-Switch configuration the ``dv_xmeta_en`` parameter is
  ignored and the device is configured to operate in legacy mode (0).

  Disabled by default (set to 0).

  The Direct Verbs/Rules (engaged with ``dv_flow_en`` = 1) supports all
  of the extensive metadata features. The legacy Verbs supports FLAG and
  MARK metadata actions over NIC Rx steering domain only.

  Setting META value to zero in flow action means there is no item provided
  and receiving datapath will not report in mbufs the metadata are present.
  Setting MARK value to zero in flow action means the zero FDIR ID value
  will be reported on packet receiving.

  For the MARK action the last 16 values in the full range are reserved for
  internal PMD purposes (to emulate FLAG action). The valid range for the
  MARK action values is 0-0xFFEF for the 16-bit mode and 0-0xFFFFEF
  for the 24-bit mode, the flows with the MARK action value outside
  the specified range will be rejected.

- ``dv_flow_en`` parameter [int]

  Value 0 means legacy Verbs flow offloading.

  Value 1 enables the DV flow steering assuming it is supported by the
  driver (requires rdma-core 24 or higher).

  Value 2 enables the WQE based hardware steering.
  In this mode, only queue-based flow management is supported.

  It is configured by default to 1 (DV flow steering) if supported.
  Otherwise, the value is 0 which indicates legacy Verbs flow offloading.

- ``dv_esw_en`` parameter [int]

  A nonzero value enables E-Switch using Direct Rules.

  Enabled by default if supported.

- ``fdb_def_rule_en`` parameter [int]

  A non-zero value enables to create a dedicated rule on E-Switch root table.
  This dedicated rule forwards all incoming packets into table 1.
  Other rules will be created in E-Switch table original table level plus one,
  to improve the flow insertion rate due to skipping root table managed by firmware.
  If set to 0, all rules will be created on the original E-Switch table level.

  By default, the PMD will set this value to 1.

- ``lacp_by_user`` parameter [int]

  A nonzero value enables the control of LACP traffic by the user application.
  When a bond exists in the driver, by default it should be managed by the
  kernel and therefore LACP traffic should be steered to the kernel.
  If this devarg is set to 1 it will allow the user to manage the bond by
  itself and not steer LACP traffic to the kernel.

  Disabled by default (set to 0).

- ``representor`` parameter [list]

  This parameter can be used to instantiate DPDK Ethernet devices from
  existing port (PF, VF or SF) representors configured on the device.

  It is a standard parameter whose format is described in
  :ref:`ethernet_device_standard_device_arguments`.

  For instance, to probe VF port representors 0 through 2::

    <PCI_BDF>,representor=vf[0-2]

  To probe SF port representors 0 through 2::

    <PCI_BDF>,representor=sf[0-2]

  To probe VF port representors 0 through 2 on both PFs of bonding device::

    <Primary_PCI_BDF>,representor=pf[0,1]vf[0-2]

- ``repr_matching_en`` parameter [int]

  - 0. If representor matching is disabled, then there will be no implicit
    item added. As a result, ingress flow rules will match traffic
    coming to any port, not only the port on which flow rule is created.
    Because of that, default flow rules for ingress traffic cannot be created
    and port starts in isolated mode by default. Port cannot be switched back
    to non-isolated mode.

  - 1. If representor matching is enabled (default setting),
    then each ingress pattern template has an implicit REPRESENTED_PORT
    item added. Flow rules based on this pattern template will match
    the vport associated with port on which rule is created.

- ``max_dump_files_num`` parameter [int]

  The maximum number of files per PMD entity that may be created for debug information.
  The files will be created in /var/log directory or in current directory.

  set to 128 by default.

- ``lro_timeout_usec`` parameter [int]

  The maximum allowed duration of an LRO session, in micro-seconds.
  PMD will set the nearest value supported by HW, which is not bigger than
  the input ``lro_timeout_usec`` value.
  If this parameter is not specified, by default PMD will set
  the smallest value supported by HW.

- ``hp_buf_log_sz`` parameter [int]

  The total data buffer size of a hairpin queue (logarithmic form), in bytes.
  PMD will set the data buffer size to 2 ** ``hp_buf_log_sz``, both for RX & TX.
  The capacity of the value is specified by the firmware and the initialization
  will get a failure if it is out of scope.
  The range of the value is from 11 to 19 right now, and the supported frame
  size of a single packet for hairpin is from 512B to 128KB. It might change if
  different firmware release is being used. By using a small value, it could
  reduce memory consumption but not work with a large frame. If the value is
  too large, the memory consumption will be high and some potential performance
  degradation will be introduced.
  By default, the PMD will set this value to 16, which means that 9KB jumbo
  frames will be supported.

- ``reclaim_mem_mode`` parameter [int]

  Cache some resources in flow destroy will help flow recreation more efficient.
  While some systems may require the all the resources can be reclaimed after
  flow destroyed.
  The parameter ``reclaim_mem_mode`` provides the option for user to configure
  if the resource cache is needed or not.

  There are three options to choose:

  - 0. It means the flow resources will be cached as usual. The resources will
    be cached, helpful with flow insertion rate.

  - 1. It will only enable the DPDK PMD level resources reclaim.

  - 2. Both DPDK PMD level and rdma-core low level will be configured as
    reclaimed mode.

  By default, the PMD will set this value to 0.

- ``decap_en`` parameter [int]

  Some devices do not support FCS (frame checksum) scattering for
  tunnel-decapsulated packets.
  If set to 0, this option forces the FCS feature and rejects tunnel
  decapsulation in the flow engine for such devices.

  By default, the PMD will set this value to 1.

- ``allow_duplicate_pattern`` parameter [int]

  There are two options to choose:

  - 0. Prevent insertion of rules with the same pattern items on non-root table.
    In this case, only the first rule is inserted and the following rules are
    rejected and error code EEXIST is returned.

  - 1. Allow insertion of rules with the same pattern items.
    In this case, all rules are inserted but only the first rule takes effect,
    the next rule takes effect only if the previous rules are deleted.

  By default, the PMD will set this value to 1.


Multiport E-Switch
------------------

In standard deployments of NVIDIA ConnectX and BlueField HCAs, where embedded switch is enabled,
each physical port is associated with a single switching domain.
Only PFs, VFs and SFs related to that physical port are connected to this domain
and offloaded flow rules are allowed to steer traffic only between the entities in the given domain.

The following diagram pictures the high level overview of this architecture::

       .---. .------. .------. .---. .------. .------.
       |PF0| |PF0VFi| |PF0SFi| |PF1| |PF1VFi| |PF1SFi|
       .-+-. .--+---. .--+---. .-+-. .--+---. .--+---.
         |      |        |       |      |        |
     .---|------|--------|-------|------|--------|---------.
     |   |      |        |       |      |        |      HCA|
     | .-+------+--------+---. .-+------+--------+---.     |
     | |                     | |                     |     |
     | |      E-Switch       | |     E-Switch        |     |
     | |         PF0         | |        PF1          |     |
     | |                     | |                     |     |
     | .---------+-----------. .--------+------------.     |
     |           |                      |                  |
     .--------+--+---+---------------+--+---+--------------.
              |      |               |      |
              | PHY0 |               | PHY1 |
              |      |               |      |
              .------.               .------.

Multiport E-Switch is a deployment scenario where:

- All physical ports, PFs, VFs and SFs share the same switching domain.
- Each physical port gets a separate representor port.
- Traffic can be matched or forwarded explicitly between any of the entities
  connected to the domain.

The following diagram pictures the high level overview of this architecture::

       .---. .------. .------. .---. .------. .------.
       |PF0| |PF0VFi| |PF0SFi| |PF1| |PF1VFi| |PF1SFi|
       .-+-. .--+---. .--+---. .-+-. .--+---. .--+---.
         |      |        |       |      |        |
     .---|------|--------|-------|------|--------|---------.
     |   |      |        |       |      |        |      HCA|
     | .-+------+--------+-------+------+--------+---.     |
     | |                                             |     |
     | |                   Shared                    |     |
     | |                  E-Switch                   |     |
     | |                                             |     |
     | .---------+----------------------+------------.     |
     |           |                      |                  |
     .--------+--+---+---------------+--+---+--------------.
              |      |               |      |
              | PHY0 |               | PHY1 |
              |      |               |      |
              .------.               .------.

In this deployment a single application can control the switching and forwarding behavior for all
entities on the HCA.

With this configuration, mlx5 PMD supports:

- matching traffic coming from physical port, PF, VF or SF using REPRESENTED_PORT items;
- forwarding traffic to physical port, PF, VF or SF using REPRESENTED_PORT actions;

Requirements
~~~~~~~~~~~~

Supported HCAs:

- ConnectX family: ConnectX-6 Dx and above.
- BlueField family: BlueField-2 and above.
- FW version: at least ``XX.37.1014``.

Supported mlx5 kernel modules versions:

- Upstream Linux - from version 6.3.
- Modules packaged in MLNX_OFED - from version v23.04-0.5.3.3.

Configuration
~~~~~~~~~~~~~

#. Apply required FW configuration::

      sudo mlxconfig -d /dev/mst/mt4125_pciconf0 set LAG_RESOURCE_ALLOCATION=1

#. Reset FW or cold reboot the host.

#. Switch E-Switch mode on all of the PFs to ``switchdev`` mode::

      sudo devlink dev eswitch set pci/0000:08:00.0 mode switchdev
      sudo devlink dev eswitch set pci/0000:08:00.1 mode switchdev

#. Enable Multiport E-Switch on all of the PFs::

      sudo devlink dev param set pci/0000:08:00.0 name esw_multiport value true cmode runtime
      sudo devlink dev param set pci/0000:08:00.1 name esw_multiport value true cmode runtime

#. Configure required number of VFs/SFs::

      echo 4 | sudo tee /sys/class/net/eth2/device/sriov_numvfs
      echo 4 | sudo tee /sys/class/net/eth3/device/sriov_numvfs

#. Start testpmd and verify that all ports are visible::

      $ sudo dpdk-testpmd -a 08:00.0,dv_flow_en=2,representor=pf0-1vf0-3 -- -i
      testpmd> show port summary all
      Number of available ports: 10
      Port MAC Address       Name         Driver         Status   Link
      0    E8:EB:D5:18:22:BC 08:00.0_p0   mlx5_pci       up       200 Gbps
      1    E8:EB:D5:18:22:BD 08:00.0_p1   mlx5_pci       up       200 Gbps
      2    D2:F6:43:0B:9E:19 08:00.0_representor_c0pf0vf0 mlx5_pci       up       200 Gbps
      3    E6:42:27:B7:68:BD 08:00.0_representor_c0pf0vf1 mlx5_pci       up       200 Gbps
      4    A6:5B:7F:8B:B8:47 08:00.0_representor_c0pf0vf2 mlx5_pci       up       200 Gbps
      5    12:93:50:45:89:02 08:00.0_representor_c0pf0vf3 mlx5_pci       up       200 Gbps
      6    06:D3:B2:79:FE:AC 08:00.0_representor_c0pf1vf0 mlx5_pci       up       200 Gbps
      7    12:FC:08:E4:C2:CA 08:00.0_representor_c0pf1vf1 mlx5_pci       up       200 Gbps
      8    8E:A9:9A:D0:35:4C 08:00.0_representor_c0pf1vf2 mlx5_pci       up       200 Gbps
      9    E6:35:83:1F:B0:A9 08:00.0_representor_c0pf1vf3 mlx5_pci       up       200 Gbps

Limitations
~~~~~~~~~~~

- Multiport E-Switch is not supported on Windows.
- Multiport E-Switch is supported only with HW Steering flow engine (``dv_flow_en=2``).
- Matching traffic coming from a physical port and forwarding it to a physical port
  (either the same or other one) is not supported.

  In order to achieve such a functionality, an application has to setup hairpin queues
  between physical port representors and forward the traffic using hairpin queues.


Sub-Function
------------

See :ref:`mlx5_sub_function`.

Sub-Function representor support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A SF netdev supports E-Switch representation offload
similar to PF and VF representors.
Use <sfnum> to probe SF representor::

   testpmd> port attach <PCI_BDF>,representor=sf<sfnum>,dv_flow_en=1


Performance tuning
------------------

1. Configure aggressive CQE Zipping for maximum performance::

        mlxconfig -d <mst device> s CQE_COMPRESSION=1

  To set it back to the default CQE Zipping mode use::

        mlxconfig -d <mst device> s CQE_COMPRESSION=0

2. In case of virtualization:

   - Make sure that hypervisor kernel is 3.16 or newer.
   - Configure boot with ``iommu=pt``.
   - Use 1G huge pages.
   - Make sure to allocate a VM on huge pages.
   - Make sure to set CPU pinning.

3. Use the CPU near local NUMA node to which the PCIe adapter is connected,
   for better performance. For VMs, verify that the right CPU
   and NUMA node are pinned according to the above. Run::

        lstopo-no-graphics --merge

   to identify the NUMA node to which the PCIe adapter is connected.

4. If more than one adapter is used, and root complex capabilities allow
   to put both adapters on the same NUMA node without PCI bandwidth degradation,
   it is recommended to locate both adapters on the same NUMA node.
   This in order to forward packets from one to the other without
   NUMA performance penalty.

5. Disable pause frames::

        ethtool -A <netdev> rx off tx off

6. Verify IO non-posted prefetch is disabled by default. This can be checked
   via the BIOS configuration. Please contact you server provider for more
   information about the settings.

.. note::

        On some machines, depends on the machine integrator, it is beneficial
        to set the PCI max read request parameter to 1K. This can be
        done in the following way:

        To query the read request size use::

                setpci -s <NIC PCI address> 68.w

        If the output is different than 3XXX, set it by::

                setpci -s <NIC PCI address> 68.w=3XXX

        The XXX can be different on different systems. Make sure to configure
        according to the setpci output.

7. To minimize overhead of searching Memory Regions:

   - '--socket-mem' is recommended to pin memory by predictable amount.
   - Configure per-lcore cache when creating Mempools for packet buffer.
   - Refrain from dynamically allocating/freeing memory in run-time.

Rx burst functions
------------------

There are multiple Rx burst functions with different advantages and limitations.

.. table:: Rx burst functions

   +-------------------+------------------------+---------+-----------------+------+-------+
   || Function Name    || Enabler               || Scatter|| Error Recovery || CQE || Large|
   |                   |                        |         |                 || comp|| MTU  |
   +===================+========================+=========+=================+======+=======+
   | rx_burst          | rx_vec_en=0            |   Yes   | Yes             |  Yes |  Yes  |
   +-------------------+------------------------+---------+-----------------+------+-------+
   | rx_burst_vec      | rx_vec_en=1 (default)  |   No    | if CQE comp off |  Yes |  No   |
   +-------------------+------------------------+---------+-----------------+------+-------+
   | rx_burst_mprq     || mprq_en=1             |   No    | Yes             |  Yes |  Yes  |
   |                   || RxQs >= rxqs_min_mprq |         |                 |      |       |
   +-------------------+------------------------+---------+-----------------+------+-------+
   | rx_burst_mprq_vec || rx_vec_en=1 (default) |   No    | if CQE comp off |  Yes |  Yes  |
   |                   || mprq_en=1             |         |                 |      |       |
   |                   || RxQs >= rxqs_min_mprq |         |                 |      |       |
   +-------------------+------------------------+---------+-----------------+------+-------+

.. _mlx5_offloads_support:

Supported hardware offloads
---------------------------

Below tables show offload support depending on hardware, firmware,
and Linux software support.

The :ref:`Linux prerequisites <mlx5_linux_prerequisites>`
are Linux kernel and rdma-core libraries.
These dependencies are also packaged in MLNX_OFED or MLNX_EN,
shortened below as "OFED".

.. table:: Minimal SW/HW versions for queue offloads

   ============== ===== ===== ========= ===== ========== =============
   Offload        DPDK  Linux rdma-core OFED   firmware   hardware
   ============== ===== ===== ========= ===== ========== =============
   common base    17.11  4.14    16     4.2-1 12.21.1000 ConnectX-4
   checksums      17.11  4.14    16     4.2-1 12.21.1000 ConnectX-4
   Rx timestamp   17.11  4.14    16     4.2-1 12.21.1000 ConnectX-4
   TSO            17.11  4.14    16     4.2-1 12.21.1000 ConnectX-4
   LRO            19.08  N/A     N/A    4.6-4 16.25.6406 ConnectX-5
   Tx scheduling  20.08  N/A     N/A    5.1-2 22.28.2006 ConnectX-6 Dx
   Buffer Split   20.11  N/A     N/A    5.1-2 16.28.2006 ConnectX-5
   ============== ===== ===== ========= ===== ========== =============

.. table:: Minimal SW/HW versions for rte_flow offloads

   +-----------------------+-----------------+-----------------+
   | Offload               | with E-Switch   | with NIC        |
   +=======================+=================+=================+
   | Count                 | | DPDK 19.05    | | DPDK 19.02    |
   |                       | | OFED 4.6      | | OFED 4.6      |
   |                       | | rdma-core 24  | | rdma-core 23  |
   |                       | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Drop                  | | DPDK 19.05    | | DPDK 18.11    |
   |                       | | OFED 4.6      | | OFED 4.5      |
   |                       | | rdma-core 24  | | rdma-core 23  |
   |                       | | ConnectX-5    | | ConnectX-4    |
   +-----------------------+-----------------+-----------------+
   | Queue / RSS           | |               | | DPDK 18.11    |
   |                       | |     N/A       | | OFED 4.5      |
   |                       | |               | | rdma-core 23  |
   |                       | |               | | ConnectX-4    |
   +-----------------------+-----------------+-----------------+
   | Shared action         | |               | |               |
   |                       | | :numref:`sact`| | :numref:`sact`|
   |                       | |               | |               |
   |                       | |               | |               |
   +-----------------------+-----------------+-----------------+
   | | VLAN                | | DPDK 19.11    | | DPDK 19.11    |
   | | (of_pop_vlan /      | | OFED 4.7-1    | | OFED 4.7-1    |
   | | of_push_vlan /      | | ConnectX-5    | | ConnectX-5    |
   | | of_set_vlan_pcp /   | |               | |               |
   | | of_set_vlan_vid)    | |               | |               |
   +-----------------------+-----------------+-----------------+
   | | VLAN                | | DPDK 21.05    | |               |
   | | ingress and /       | | OFED 5.3      | |    N/A        |
   | | of_push_vlan /      | | ConnectX-6 Dx | |               |
   +-----------------------+-----------------+-----------------+
   | | VLAN                | | DPDK 21.05    | |               |
   | | egress and /        | | OFED 5.3      | |    N/A        |
   | | of_pop_vlan /       | | ConnectX-6 Dx | |               |
   +-----------------------+-----------------+-----------------+
   | Encapsulation         | | DPDK 19.05    | | DPDK 19.02    |
   | (VXLAN / NVGRE / RAW) | | OFED 4.7-1    | | OFED 4.6      |
   |                       | | rdma-core 24  | | rdma-core 23  |
   |                       | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Encapsulation         | | DPDK 19.11    | | DPDK 19.11    |
   | GENEVE                | | OFED 4.7-3    | | OFED 4.7-3    |
   |                       | | rdma-core 27  | | rdma-core 27  |
   |                       | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Tunnel Offload        | |  DPDK 20.11   | | DPDK 20.11    |
   |                       | |  OFED 5.1-2   | | OFED 5.1-2    |
   |                       | |  rdma-core 32 | | N/A           |
   |                       | |  ConnectX-5   | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | | Header rewrite      | | DPDK 19.05    | | DPDK 19.02    |
   | | (set_ipv4_src /     | | OFED 4.7-1    | | OFED 4.7-1    |
   | | set_ipv4_dst /      | | rdma-core 24  | | rdma-core 24  |
   | | set_ipv6_src /      | | ConnectX-5    | | ConnectX-5    |
   | | set_ipv6_dst /      | |               | |               |
   | | set_tp_src /        | |               | |               |
   | | set_tp_dst /        | |               | |               |
   | | dec_ttl /           | |               | |               |
   | | set_ttl /           | |               | |               |
   | | set_mac_src /       | |               | |               |
   | | set_mac_dst)        | |               | |               |
   +-----------------------+-----------------+-----------------+
   | | Header rewrite      | | DPDK 20.02    | | DPDK 20.02    |
   | | (set_dscp)          | | OFED 5.0      | | OFED 5.0      |
   | |                     | | rdma-core 24  | | rdma-core 24  |
   | |                     | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | | Header rewrite      | | DPDK 22.07    | | DPDK 22.07    |
   | | (ipv4_ecn /         | | OFED 5.6-2    | | OFED 5.6-2    |
   | | ipv6_ecn)           | | rdma-core 41  | | rdma-core 41  |
   | |                     | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Jump                  | | DPDK 19.05    | | DPDK 19.02    |
   |                       | | OFED 4.7-1    | | OFED 4.7-1    |
   |                       | | rdma-core 24  | | N/A           |
   |                       | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Mark / Flag           | | DPDK 19.05    | | DPDK 18.11    |
   |                       | | OFED 4.6      | | OFED 4.5      |
   |                       | | rdma-core 24  | | rdma-core 23  |
   |                       | | ConnectX-5    | | ConnectX-4    |
   +-----------------------+-----------------+-----------------+
   | Meta data             | |  DPDK 19.11   | | DPDK 19.11    |
   |                       | |  OFED 4.7-3   | | OFED 4.7-3    |
   |                       | |  rdma-core 26 | | rdma-core 26  |
   |                       | |  ConnectX-5   | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Port ID               | | DPDK 19.05    |     | N/A       |
   |                       | | OFED 4.7-1    |     | N/A       |
   |                       | | rdma-core 24  |     | N/A       |
   |                       | | ConnectX-5    |     | N/A       |
   +-----------------------+-----------------+-----------------+
   | Hairpin               | |               | | DPDK 19.11    |
   |                       | |     N/A       | | OFED 4.7-3    |
   |                       | |               | | rdma-core 26  |
   |                       | |               | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | 2-port Hairpin        | |               | | DPDK 20.11    |
   |                       | |     N/A       | | OFED 5.1-2    |
   |                       | |               | | N/A           |
   |                       | |               | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Metering              | |  DPDK 19.11   | | DPDK 19.11    |
   |                       | |  OFED 4.7-3   | | OFED 4.7-3    |
   |                       | |  rdma-core 26 | | rdma-core 26  |
   |                       | |  ConnectX-5   | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | ASO Metering          | |  DPDK 21.05   | | DPDK 21.05    |
   |                       | |  OFED 5.3     | | OFED 5.3      |
   |                       | |  rdma-core 33 | | rdma-core 33  |
   |                       | |  ConnectX-6 Dx| | ConnectX-6 Dx |
   +-----------------------+-----------------+-----------------+
   | Metering Hierarchy    | |  DPDK 21.08   | | DPDK 21.08    |
   |                       | |  OFED 5.3     | | OFED 5.3      |
   |                       | |  N/A          | | N/A           |
   |                       | |  ConnectX-6 Dx| | ConnectX-6 Dx |
   +-----------------------+-----------------+-----------------+
   | Sampling              | |  DPDK 20.11   | | DPDK 20.11    |
   |                       | |  OFED 5.1-2   | | OFED 5.1-2    |
   |                       | |  rdma-core 32 | | N/A           |
   |                       | |  ConnectX-5   | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Encapsulation         | |  DPDK 21.02   | | DPDK 21.02    |
   | GTP PSC               | |  OFED 5.2     | | OFED 5.2      |
   |                       | |  rdma-core 35 | | rdma-core 35  |
   |                       | |  ConnectX-6 Dx| | ConnectX-6 Dx |
   +-----------------------+-----------------+-----------------+
   | Encapsulation         | | DPDK 21.02    | | DPDK 21.02    |
   | GENEVE TLV option     | | OFED 5.2      | | OFED 5.2      |
   |                       | | rdma-core 34  | | rdma-core 34  |
   |                       | | ConnectX-6 Dx | | ConnectX-6 Dx |
   +-----------------------+-----------------+-----------------+
   | Modify Field          | | DPDK 21.02    | | DPDK 21.02    |
   |                       | | OFED 5.2      | | OFED 5.2      |
   |                       | | rdma-core 35  | | rdma-core 35  |
   |                       | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Connection tracking   | |               | | DPDK 21.05    |
   |                       | |     N/A       | | OFED 5.3      |
   |                       | |               | | rdma-core 35  |
   |                       | |               | | ConnectX-6 Dx |
   +-----------------------+-----------------+-----------------+

.. table:: Minimal SW/HW versions for shared action offload
   :name: sact

   +-----------------------+-----------------+-----------------+
   | Shared Action         | with E-Switch   | with NIC        |
   +=======================+=================+=================+
   | RSS                   | |               | | DPDK 20.11    |
   |                       | |     N/A       | | OFED 5.2      |
   |                       | |               | | rdma-core 33  |
   |                       | |               | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+
   | Age                   | | DPDK 20.11    | | DPDK 20.11    |
   |                       | | OFED 5.2      | | OFED 5.2      |
   |                       | | rdma-core 32  | | rdma-core 32  |
   |                       | | ConnectX-6 Dx | | ConnectX-6 Dx |
   +-----------------------+-----------------+-----------------+
   | Count                 | | DPDK 21.05    | | DPDK 21.05    |
   |                       | | OFED 4.6      | | OFED 4.6      |
   |                       | | rdma-core 24  | | rdma-core 23  |
   |                       | | ConnectX-5    | | ConnectX-5    |
   +-----------------------+-----------------+-----------------+

.. table:: Minimal SW/HW versions for flow template API

   +-----------------+--------------------+--------------------+
   | DPDK            | NIC                | Firmware           |
   +=================+====================+====================+
   | 22.11           | ConnectX-6 Dx      | xx.35.1012         |
   +-----------------+--------------------+--------------------+

Notes for metadata
------------------

MARK and META items are interrelated with datapath - they might move from/to
the applications in mbuf fields. Hence, zero value for these items has the
special meaning - it means "no metadata are provided", not zero values are
treated by applications and PMD as valid ones.

Moreover in the flow engine domain the value zero is acceptable to match and
set, and we should allow to specify zero values as rte_flow parameters for the
META and MARK items and actions. In the same time zero mask has no meaning and
should be rejected on validation stage.

Notes for rte_flow
------------------

Flows are not cached in the driver.
When stopping a device port, all the flows created on this port from the
application will be flushed automatically in the background.
After stopping the device port, all flows on this port become invalid and
not represented in the system.
All references to these flows held by the application should be discarded
directly but neither destroyed nor flushed.

The application should re-create the flows as required after the port restart.


Notes for hairpin
-----------------

NVIDIA ConnectX and BlueField devices support
specifying memory placement for hairpin Rx and Tx queues.
This feature requires NVIDIA MLNX_OFED 5.8.

By default, data buffers and packet descriptors for hairpin queues
are placed in device memory
which is shared with other resources (e.g. flow rules).

Starting with DPDK 22.11 and NVIDIA MLNX_OFED 5.8,
applications are allowed to:

#. Place data buffers and Rx packet descriptors in dedicated device memory.
   Application can request that configuration
   through ``use_locked_device_memory`` configuration option.

   Placing data buffers and Rx packet descriptors in dedicated device memory
   can decrease latency on hairpinned traffic,
   since traffic processing for the hairpin queue will not be memory starved.

   However, reserving device memory for hairpin Rx queues
   may decrease throughput under heavy load,
   since less resources will be available on device.

   This option is supported only for Rx hairpin queues.

#. Place Tx packet descriptors in host memory.
   Application can request that configuration
   through ``use_rte_memory`` configuration option.

   Placing Tx packet descritors in host memory can increase traffic throughput.
   This results in more resources available on the device for other purposes,
   which reduces memory contention on device.
   Side effect of this option is visible increase in latency,
   since each packet incurs additional PCI transactions.

   This option is supported only for Tx hairpin queues.


Notes for testpmd
-----------------

Compared to librte_net_mlx4 that implements a single RSS configuration per
port, librte_net_mlx5 supports per-protocol RSS configuration.

Since ``testpmd`` defaults to IP RSS mode and there is currently no
command-line parameter to enable additional protocols (UDP and TCP as well
as IP), the following commands must be entered from its CLI to get the same
behavior as librte_net_mlx4::

   > port stop all
   > port config all rss all
   > port start all

Usage example
-------------

This section demonstrates how to launch **testpmd** with NVIDIA
ConnectX-4/ConnectX-5/ConnectX-6/BlueField devices managed by librte_net_mlx5.

#. Load the kernel modules::

      modprobe -a ib_uverbs mlx5_core mlx5_ib

   Alternatively if MLNX_OFED/MLNX_EN is fully installed, the following script
   can be run::

      /etc/init.d/openibd restart

   .. note::

      User space I/O kernel modules (uio and igb_uio) are not used and do
      not have to be loaded.

#. Make sure Ethernet interfaces are in working order and linked to kernel
   verbs. Related sysfs entries should be present::

      ls -d /sys/class/net/*/device/infiniband_verbs/uverbs* | cut -d / -f 5

   Example output::

      eth30
      eth31
      eth32
      eth33

#. Optionally, retrieve their PCI bus addresses for to be used with the allow list::

      {
          for intf in eth2 eth3 eth4 eth5;
          do
              (cd "/sys/class/net/${intf}/device/" && pwd -P);
          done;
      } |
      sed -n 's,.*/\(.*\),-a \1,p'

   Example output::

      -a 0000:05:00.1
      -a 0000:06:00.0
      -a 0000:06:00.1
      -a 0000:05:00.0

#. Request huge pages::

      dpdk-hugepages.py --setup 2G

#. Start testpmd with basic parameters::

      dpdk-testpmd -l 8-15 -n 4 -a 05:00.0 -a 05:00.1 -a 06:00.0 -a 06:00.1 -- --rxq=2 --txq=2 -i

   Example output::

      [...]
      EAL: PCI device 0000:05:00.0 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_net_mlx5
      PMD: librte_net_mlx5: PCI information matches, using device "mlx5_0" (VF: false)
      PMD: librte_net_mlx5: 1 port(s) detected
      PMD: librte_net_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:fe
      EAL: PCI device 0000:05:00.1 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_net_mlx5
      PMD: librte_net_mlx5: PCI information matches, using device "mlx5_1" (VF: false)
      PMD: librte_net_mlx5: 1 port(s) detected
      PMD: librte_net_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:ff
      EAL: PCI device 0000:06:00.0 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_net_mlx5
      PMD: librte_net_mlx5: PCI information matches, using device "mlx5_2" (VF: false)
      PMD: librte_net_mlx5: 1 port(s) detected
      PMD: librte_net_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:fa
      EAL: PCI device 0000:06:00.1 on NUMA socket 0
      EAL:   probe driver: 15b3:1013 librte_net_mlx5
      PMD: librte_net_mlx5: PCI information matches, using device "mlx5_3" (VF: false)
      PMD: librte_net_mlx5: 1 port(s) detected
      PMD: librte_net_mlx5: port 1 MAC address is e4:1d:2d:e7:0c:fb
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      PMD: librte_net_mlx5: 0x8cba80: TX queues number update: 0 -> 2
      PMD: librte_net_mlx5: 0x8cba80: RX queues number update: 0 -> 2
      Port 0: E4:1D:2D:E7:0C:FE
      Configuring Port 1 (socket 0)
      PMD: librte_net_mlx5: 0x8ccac8: TX queues number update: 0 -> 2
      PMD: librte_net_mlx5: 0x8ccac8: RX queues number update: 0 -> 2
      Port 1: E4:1D:2D:E7:0C:FF
      Configuring Port 2 (socket 0)
      PMD: librte_net_mlx5: 0x8cdb10: TX queues number update: 0 -> 2
      PMD: librte_net_mlx5: 0x8cdb10: RX queues number update: 0 -> 2
      Port 2: E4:1D:2D:E7:0C:FA
      Configuring Port 3 (socket 0)
      PMD: librte_net_mlx5: 0x8ceb58: TX queues number update: 0 -> 2
      PMD: librte_net_mlx5: 0x8ceb58: RX queues number update: 0 -> 2
      Port 3: E4:1D:2D:E7:0C:FB
      Checking link statuses...
      Port 0 Link Up - speed 40000 Mbps - full-duplex
      Port 1 Link Up - speed 40000 Mbps - full-duplex
      Port 2 Link Up - speed 10000 Mbps - full-duplex
      Port 3 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>

How to dump flows
-----------------

This section demonstrates how to dump flows. Currently, it's possible to dump
all flows with assistance of external tools.

#. 2 ways to get flow raw file:

   - Using testpmd CLI:

   .. code-block:: console

       To dump all flows:
       testpmd> flow dump <port> all <output_file>
       and dump one flow:
       testpmd> flow dump <port> rule <rule_id> <output_file>

   - call rte_flow_dev_dump api:

   .. code-block:: console

       rte_flow_dev_dump(port, flow, file, NULL);

#. Dump human-readable flows from raw file:

   Get flow parsing tool from: https://github.com/Mellanox/mlx_steering_dump

   .. code-block:: console

       mlx_steering_dump.py -f <output_file> -flowptr <flow_ptr>

How to share a meter between ports in the same switch domain
------------------------------------------------------------

This section demonstrates how to use the shared meter. A meter M can be created
on port X and to be shared with a port Y on the same switch domain by the next way:

.. code-block:: console

   flow create X ingress transfer pattern eth / port_id id is Y / end actions meter mtr_id M / end

How to use meter hierarchy
--------------------------

This section demonstrates how to create and use a meter hierarchy.
A termination meter M can be the policy green action of another termination meter N.
The two meters are chained together as a chain. Using meter N in a flow will apply
both the meters in hierarchy on that flow.

.. code-block:: console

   add port meter policy 0 1 g_actions queue index 0 / end y_actions end r_actions drop / end
   create port meter 0 M 1 1 yes 0xffff 1 0
   add port meter policy 0 2 g_actions meter mtr_id M / end y_actions end r_actions drop / end
   create port meter 0 N 2 2 yes 0xffff 1 0
   flow create 0 ingress group 1 pattern eth / end actions meter mtr_id N / end

How to configure a VF as trusted
--------------------------------

This section demonstrates how to configure a virtual function (VF) interface as trusted.
Trusted VF is needed to offload rules with rte_flow to a group that is bigger than 0.
The configuration is done in two parts: driver and FW.

The procedure below is an example of using a ConnectX-5 adapter card (pf0) with 2 VFs:

#. Create 2 VFs on the PF pf0 when in Legacy SR-IOV mode::

   $ echo 2 > /sys/class/net/pf0/device/mlx5_num_vfs

#. Verify the VFs are created:

   .. code-block:: console

      $ lspci | grep Mellanox
      82:00.0 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5]
      82:00.1 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5]
      82:00.2 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5 Virtual Function]
      82:00.3 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5 Virtual Function]

#. Unbind all VFs. For each VF PCIe, using the following command to unbind the driver::

   $ echo "0000:82:00.2" >> /sys/bus/pci/drivers/mlx5_core/unbind

#. Set the VFs to be trusted for the kernel by using one of the methods below:

      - Using sysfs file::

        $ echo ON | tee /sys/class/net/pf0/device/sriov/0/trust
        $ echo ON | tee /sys/class/net/pf0/device/sriov/1/trust

      - Using “ip link” command::

        $ ip link set p0 vf 0 trust on
        $ ip link set p0 vf 1 trust on

#. Configure all VFs using ``mlxreg``:

   - For MFT >= 4.21::

     $ mlxreg -d /dev/mst/mt4121_pciconf0 --reg_name VHCA_TRUST_LEVEL --yes --indexes 'all_vhca=0x1,vhca_id=0x0' --set 'trust_level=0x1'

   - For MFT < 4.21::

     $ mlxreg -d /dev/mst/mt4121_pciconf0 --reg_name VHCA_TRUST_LEVEL --yes --set "all_vhca=0x1,trust_level=0x1"

   .. note::

      Firmware version used must be >= xx.29.1016 and MFT >= 4.18

#. For each VF PCIe, using the following command to bind the driver::

   $ echo "0000:82:00.2" >> /sys/bus/pci/drivers/mlx5_core/bind

How to trace Tx datapath
------------------------

The mlx5 PMD provides Tx datapath tracing capability with extra debug information:
when and how packets were scheduled,
and when the actual sending was completed by the NIC hardware.

Steps to enable Tx datapath tracing:

#. Build DPDK application with enabled datapath tracing

   The Meson option ``--enable_trace_fp=true`` and
   the C flag ``ALLOW_EXPERIMENTAL_API`` should be specified.

   .. code-block:: console

      meson configure --buildtype=debug -Denable_trace_fp=true
         -Dc_args='-DRTE_LIBRTE_MLX5_DEBUG -DRTE_ENABLE_ASSERT -DALLOW_EXPERIMENTAL_API' build

#. Configure the NIC

   If the sending completion timings are important,
   the NIC should be configured to provide realtime timestamps.
   The non-volatile settings parameter  ``REAL_TIME_CLOCK_ENABLE`` should be configured as ``1``.

   .. code-block:: console

      mlxconfig -d /dev/mst/mt4125_pciconf0 s REAL_TIME_CLOCK_ENABLE=1

   The ``mlxconfig`` utility is part of the MFT package.

#. Run application with EAL parameter enabling tracing in mlx5 Tx datapath

   By default all tracepoints are disabled.
   To analyze Tx datapath and its timings: ``--trace=pmd.net.mlx5.tx``.

#. Commit the tracing data to the storage (with ``rte_trace_save()`` API call).

#. Install or build the ``babeltrace2`` package

   The Python script analyzing gathered trace data uses the ``babeltrace2`` library.
   The package should be either installed or built from source as shown below.

   .. code-block:: console

      git clone https://github.com/efficios/babeltrace.git
      cd babeltrace
      ./bootstrap
      ./configure -help
      ./configure --disable-api-doc --disable-man-pages
                  --disable-python-bindings-doc --enable-python-plugins
                  --enable-python-binding

#. Run analyzing script

   ``mlx5_trace.py`` is used to combine related events (packet firing and completion)
   and to show the results in human-readable view.

   The analyzing script is located in the DPDK source tree: ``drivers/net/mlx5/tools``.

   It requires Python 3.6 and ``babeltrace2`` package.

   The parameter of the script is the trace data folder.

   .. code-block:: console

      mlx5_trace.py /var/log/rte-2023-01-23-AM-11-52-39

#. Interpreting the script output data

   All the timings are given in nanoseconds.
   The list of Tx bursts per port/queue is presented in the output.
   Each list element contains the list of built WQEs with specific opcodes.
   Each WQE contains the list of the encompassed packets to send.

Host shaper
-----------

Host shaper register is per host port register
which sets a shaper on the host port.
All VF/host PF representors belonging to one host port share one host shaper.
For example, if representor 0 and representor 1 belong to the same host port,
and a host shaper rate of 1Gbps is configured,
the shaper throttles both representors traffic from the host.

Host shaper has two modes for setting the shaper,
immediate and deferred to available descriptor threshold event trigger.

In immediate mode, the rate limit is configured immediately to host shaper.

When deferring to the available descriptor threshold trigger,
the shaper is not set until an available descriptor threshold event
is received by any Rx queue in a VF representor belonging to the host port.
The only rate supported for deferred mode is 100Mbps
(there is no limit on the supported rates for immediate mode).
In deferred mode, the shaper is set on the host port by the firmware
upon receiving the available descriptor threshold event,
which allows throttling host traffic on available descriptor threshold events
at minimum latency, preventing excess drops in the Rx queue.

Dependency on mstflint package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to configure host shaper register,
``librte_net_mlx5`` depends on ``libmtcr_ul``
which can be installed from MLNX_OFED mstflint package.
Meson detects ``libmtcr_ul`` existence at configure stage.
If the library is detected, the application must link with ``-lmtcr_ul``,
as done by the pkg-config file libdpdk.pc.

Available descriptor threshold and host shaper
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There is a command to configure the available descriptor threshold in testpmd.
Testpmd also contains sample logic to handle available descriptor threshold events.
The typical workflow is:
testpmd configures available descriptor threshold for Rx queues,
enables ``avail_thresh_triggered`` in host shaper and registers a callback.
When traffic from the host is too high
and Rx queue emptiness is below the available descriptor threshold,
the PMD receives an event
and the firmware configures a 100Mbps shaper on the host port automatically.
Then the PMD call the callback registered previously,
which will delay a while to let Rx queue empty,
then disable host shaper.

Let's assume we have a simple BlueField-2 setup:
port 0 is uplink, port 1 is VF representor.
Each port has 2 Rx queues.
To control traffic from the host to the Arm device,
we can enable the available descriptor threshold in testpmd by:

.. code-block:: console

   testpmd> mlx5 set port 1 host_shaper avail_thresh_triggered 1 rate 0
   testpmd> set port 1 rxq 0 avail_thresh 70
   testpmd> set port 1 rxq 1 avail_thresh 70

The first command disables the current host shaper
and enables the available descriptor threshold triggered mode.
The other commands configure the available descriptor threshold
to 70% of Rx queue size for both Rx queues.

When traffic from the host is too high,
testpmd console prints log about available descriptor threshold event,
then host shaper is disabled.
The traffic rate from the host is controlled and less drop happens in Rx queues.

The threshold event and shaper can be disabled like this:

.. code-block:: console

   testpmd> mlx5 set port 1 host_shaper avail_thresh_triggered 0 rate 0
   testpmd> set port 1 rxq 0 avail_thresh 0
   testpmd> set port 1 rxq 1 avail_thresh 0

It is recommended an application disables the available descriptor threshold
and ``avail_thresh_triggered`` before exit,
if it enables them before.

The shaper can also be configured with a value, the rate unit is 100Mbps.
Below, the command sets the current shaper to 5Gbps
and disables ``avail_thresh_triggered``.

.. code-block:: console

   testpmd> mlx5 set port 1 host_shaper avail_thresh_triggered 0 rate 50


Testpmd driver specific commands
--------------------------------

port attach with socket path
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is possible to allocate a port with ``libibverbs`` from external application.
For importing the external port with extra device arguments,
there is a specific testpmd command
similar to :ref:`port attach command <port_attach>`::

   testpmd> mlx5 port attach (identifier) socket=(path)

where:

* ``identifier``: device identifier with optional parameters
  as same as :ref:`port attach command <port_attach>`.
* ``path``: path to IPC server socket created by the external application.

This command performs:

#. Open IPC client socket using the given path, and connect it.

#. Import ibverbs context and ibverbs protection domain.

#. Add two device arguments for context (``cmd_fd``)
   and protection domain (``pd_handle``) to the device identifier.
   See :ref:`mlx5 driver options <mlx5_common_driver_options>` for more
   information about these device arguments.

#. Call the regular ``port attach`` function with updated identifier.

For example, to attach a port whose PCI address is ``0000:0a:00.0``
and its socket path is ``/var/run/import_ipc_socket``:

.. code-block:: console

   testpmd> mlx5 port attach 0000:0a:00.0 socket=/var/run/import_ipc_socket
   testpmd: MLX5 socket path is /var/run/import_ipc_socket
   testpmd: Attach port with extra devargs 0000:0a:00.0,cmd_fd=40,pd_handle=1
   Attaching a new port...
   EAL: Probe PCI driver: mlx5_pci (15b3:101d) device: 0000:0a:00.0 (socket 0)
   Port 0 is attached. Now total ports is 1
   Done


port map external Rx queue
~~~~~~~~~~~~~~~~~~~~~~~~~~

External Rx queue indexes mapping management.

Map HW queue index (32-bit) to ethdev queue index (16-bit) for external Rx queue::

   testpmd> mlx5 port (port_id) ext_rxq map (sw_queue_id) (hw_queue_id)

Unmap external Rx queue::

   testpmd> mlx5 port (port_id) ext_rxq unmap (sw_queue_id)

where:

* ``sw_queue_id``: queue index in range [64536, 65535].
  This range is the highest 1000 numbers.
* ``hw_queue_id``: queue index given by HW in queue creation.

Set Flow Engine Mode
~~~~~~~~~~~~~~~~~~~~

Set the flow engine to active or standby mode with specific flags (bitmap style).
See ``RTE_PMD_MLX5_FLOW_ENGINE_FLAG_*`` for the flag definitions.

.. code-block:: console

   testpmd> mlx5 set flow_engine <active|standby> [<flags>]

This command is used for testing live migration,
and works for software steering only.
Default FDB jump should be disabled if switchdev is enabled.
The mode will propagate to all the probed ports.
