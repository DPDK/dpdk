.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2015 6WIND S.A.
   Copyright 2015 Mellanox Technologies, Ltd
   Copyright (c) 2025 NVIDIA Corporation & Affiliates

.. include:: <isonum.txt>

NVIDIA MLX5 Ethernet Driver
===========================

The mlx5 Ethernet poll mode driver (``librte_net_mlx5``)
provides support for NVIDIA NIC and DPU device families.
The embedded switch, Physical Functions (PF),
SR-IOV Virtual Functions (VF), Linux auxiliary Sub-Functions (SF),
and their port representors are supported
with many :ref:`features <mlx5_net_features>`.

For additional support, you may contact NVIDIA_.

.. _NVIDIA: mailto:enterprisesupport@nvidia.com?subject=DPDK%20mlx5%20support&body=Company:%20%0D%0A%0D%0AEnvironment:%20%0D%0A%0D%0ADPDK%20version:%20%0D%0A%0D%0AQuestion:


Supported Devices
-----------------

The following families of NVIDIA ConnectX NICs and BlueField DPUs are supported
with the same driver:

================== =============== ========= =========== ============
NIC / DPU          total bandwidth max ports PCIe        embedded CPU
================== =============== ========= =========== ============
**ConnectX-4 Lx**   50 Gb/s        2         Gen3        --
**ConnectX-4**     100 Gb/s        2         Gen3        --
**ConnectX-5**     100 Gb/s        2         Gen3        --
**ConnectX-5 Ex**  100 Gb/s        2         Gen4        --
**ConnectX-6 Lx**   50 Gb/s        2         Gen3 / Gen4 --
**ConnectX-6**     200 Gb/s        2         Gen3 / Gen4 --
**ConnectX-6 Dx**  200 Gb/s        2         Gen4        --
**BlueField-2**    200 Gb/s        2         Gen4        A72 x8
**ConnectX-7**     400 Gb/s        4         Gen5        --
**BlueField-3**    400 Gb/s        2         Gen5        A78 x16
**ConnectX-8**     400 Gb/s        4         Gen6        --
**ConnectX-9**     800 Gb/s        4         Gen6        --
================== =============== ========= =========== ============

The details of models and specifications can be found on the website
for `ConnectX NICs <https://www.nvidia.com/en-in/networking/ethernet-adapters/>`_
and `BlueField DPUs <https://www.nvidia.com/en-in/networking/products/data-processing-unit/>`_.

A DPU can act as a NIC in NIC mode.


Design
------

Please refer to the :ref:`mlx5 design <mlx5_common_design>`
for considerations relevant to all mlx5 drivers.

This driver is designed with a strong emphasis on performance,
with data available in the `performance reports <https://core.dpdk.org/perf-reports/>`_
published after each major releases.

Performance optimization is addressed in many paths:

- Steering performed in software using the Rx/Tx packet API.
- Steering partially offloaded to hardware via the flow API.
- Steering fully handled in the hardware switch using flow offload and hairpin API.

Rx/Tx Packet Burst
~~~~~~~~~~~~~~~~~~

The Rx / Tx data path use different techniques to offer the best performance.

- If the enabled features are compatible, a vectorized data path will be selected.
  It means the implementation will use some SIMD (vectorized) instructions
  as much as possible to accelerate the packet burst processing.

- On the receiving (Rx) side, some PCI bandwidth is saved
  by :ref:`compressing the packet descriptors (CQE) <mlx5_cqe_comp_param>`.
  This feature is enabled by default.

- Additional Rx acceleration for small packets is achieved
  by further reducing PCI bandwidth usage
  with :ref:`multi-packet Rx queues (MPRQ) <mlx5_mprq_params>`.
  This feature is disabled by default.

More details about Rx implementations and their configurations are provided
in the chapter about :ref:`mlx5_rx_functions`.

- On the transmitting (Tx) side,
  a :ref:`packet descriptor (WQE) optimization <mlx5_tx_inline>`
  is provided through :ref:`inlining configurations <mlx5_tx_inline_params>`.

- Additional Tx acceleration for small packets is achieved
  by saving PCI bandwidth with :ref:`enhanced Multi-Packet Write (eMPW) <mlx5_mpw_param>`.

- Sending packet can be scheduled accurately
  thanks to :ref:`time-based packet pacing <mlx5_tx_pp_param>`.


Flow Steering
~~~~~~~~~~~~~

A major benefit of the mlx5 devices
is the :ref:`bifurcated driver <mlx5_bifurcated>` capability.
It allows to route some flows from the device to the kernel
while other flows go directly to the userspace PMD.
This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets
as long as they share the same MAC address.
This means Linux control tools (ethtool, iproute and more)
can operate on the same network interfaces as ones owned by the DPDK application.

When using flow offload extensively,
the configuration of the flow rules becomes performance-critical.
That's why the hardware is evolving to offer faster flow steering access.
The steering engine was accelerated by introducing Direct Verbs,
and even more with Direct Rules.
At this stage, a lot of flow rules manipulations were done in software.
This technology is named software steering (SWS).
Later the performance was a lot more improved
with :ref:`hardware steering (HWS) <mlx5_hws>`,
a WQE-based high scaling and safer flow insertion/destruction.
It allows to insert millions of rules per second.

While using the :doc:`synchronous flow API <../prog_guide/ethdev/flow_offload>`
is convenient and easy to manage,
it is not efficient enough at a large scale.
That's why mlx5 allows to offload many features asynchronously.
The :ref:`asynchronous flow API <flow_async_api>`
makes use of :ref:`pre-configured templates <flow_template_api>`
to insert new rules very fast with predictable performance.
The best flow offload performance is achieved by using the asynchronous API
which requires the :ref:`hardware steering engine <mlx5_hws>`
to be :ref:`enabled <mlx5_dv_flow>`.

The device operates flow offload in two different domains:

- NIC part is where packets are going through the PCI bus
  for Rx and Tx operations.

- E-Switch is an embedded network switch
  which may operate full offload of packet processing.

Note that flow rules are not cached in the driver.
When stopping a device port, all the flows created on this port
from the application will be flushed automatically in the background.
After stopping the device port, all flows on this port
become invalid and not represented in the system.
All references to these flows held by the application
should be discarded directly but neither destroyed nor flushed.
The application should re-create the flows as required after the port restart.

See :doc:`../../platform/mlx5` guide for more design details,
including prerequisites installation.

:ref:`All the features are detailed in a chapter below. <mlx5_net_features>`


Compilation
-----------

The dependencies and steps are described in
the :ref:`common compilation chapter <mlx5_common_compilation>`
for all mlx5 drivers.


Configuration
-------------

Environment Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~

The variables and tools are described in
the :ref:`common environment configuration chapter <mlx5_common_env>`
for all mlx5 drivers.

Firmware Configuration
~~~~~~~~~~~~~~~~~~~~~~

The firmware variables required for specific networking features
are described as part of each feature in this guide.

All the variables and tools are described in
the :ref:`common firmware configuration chapter <mlx5_firmware_config>`
for all mlx5 drivers.

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

Please refer to :ref:`mlx5 common options <mlx5_common_driver_options>`
for an additional list of options shared with other mlx5 drivers.

- ``probe_opt_en`` parameter [int]

  A non-zero value optimizes the probing process, especially for large scale.
  The PMD will hold the IB device information internally and reuse it.

  By default, the PMD will set this value to 0.

  .. note::

     There is a race condition in probing port if ``probe_opt_en`` is enabled.
     Port probing may fail with a wrong ifindex in cache
     while the interrupt thread is updating the cache.
     Please try again if port probing failed.

.. _mlx5_cqe_comp_param:

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
    ConnectX-6 Lx, ConnectX-7, ConnectX-8, ConnectX-9, BlueField-2, and BlueField-3.
  - POWER9 and ARMv8 with ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7, ConnectX-8, ConnectX-9, BlueField-2, and BlueField-3.

- ``rxq_pkt_pad_en`` parameter [int]

  A nonzero value enables padding Rx packet to the size of cacheline on PCI
  transaction. This feature would waste PCI bandwidth but could improve
  performance by avoiding partial cacheline write which may cause costly
  read-modify-copy in memory transaction on some architectures. Disabled by
  default.

  Supported on:

  - x86_64 with ConnectX-4, ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7, ConnectX-8, ConnectX-9, BlueField-2, and BlueField-3.
  - POWER8 and ARMv8 with ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx,
    ConnectX-6 Lx, ConnectX-7, ConnectX-8, ConnectX-9, BlueField-2, and BlueField-3.

.. _mlx5_delay_drop_param:

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
  can be set and queried via ethtool::

     ethtool --set-priv-flags <netdev> dropless_rq on (/ off)
     ethtool --show-priv-flags <netdev>

  The configuration flag is global per PF and can only be set on the PF, once
  it is on, all the VFs', SFs' and representors' Rx queues will share the timer
  and rearming.

.. _mlx5_mprq_params:

- ``mprq_en`` parameter [int]

  A nonzero value enables configuring Multi-Packet Rx queues. Rx queue is
  configured as Multi-Packet RQ if the total number of Rx queues is
  ``rxqs_min_mprq`` or more. Disabled by default.

  Multi-Packet Rx Queue (MPRQ a.k.a Striding RQ) can further save PCIe bandwidth
  by posting a single large buffer for multiple packets. Instead of posting a
  buffer per packet, one large buffer is posted in order to receive multiple
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

  User-managed mempools with external pinned data buffers
  cannot be used in conjunction with MPRQ
  since packets may be already attached to PMD-managed external buffers.

  All the Rx mbufs must be freed before the device is closed.
  Otherwise, the mempool of the external buffers will be freed by the PMD,
  and the application which still holds the external buffers may be corrupted.

- ``rxqs_min_mprq`` parameter [int]

  Configure Rx queues as Multi-Packet RQ if the total number of Rx queues is
  greater or equal to this value. The default value is 12, valid only if
  ``mprq_en`` is set.

.. _mlx5_tx_inline_params:

- ``txq_inline`` parameter [int]

  Amount of data to be inlined during Tx operations. This parameter is
  deprecated and converted to the new parameter ``txq_inline_max`` providing
  partial compatibility.

- ``txqs_min_inline`` parameter [int]

  Enable inline data send only when the number of Tx queues is greater or equal
  to this value.

  This option should be used in combination with ``txq_inline_max`` and
  ``txq_inline_mpw`` and does not affect ``txq_inline_min`` settings.

  If this option is not specified the default value 16 is used for BlueField
  and 8 for other platforms.

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

  Enable vectorized Tx only when the number of Tx queues is less than or
  equal to this value. This parameter is deprecated and ignored, kept
  for compatibility issue to not prevent driver from probing.

- ``txq_mpw_hdr_dseg_en`` parameter [int]

  A nonzero value enables including two pointers in the first block of TrxX
  descriptor. The parameter is deprecated and ignored, kept for compatibility
  issue.

- ``txq_max_inline_len`` parameter [int]

  Maximum size of packet to be inlined. This limits the size of packet to
  be inlined. If the size of a packet is larger than configured value, the
  packet isn't inlined even though there's enough space remained in the
  descriptor. Instead, the packet is included with pointer. This parameter
  is deprecated and converted directly to ``txq_inline_mpw`` providing full
  compatibility. Valid only if eMPW feature is engaged.

.. _mlx5_mpw_param:

- ``txq_mpw_en`` parameter [int]

  A nonzero value enables Enhanced Multi-Packet Write (eMPW) for NICs starting
  ConnectX-5, and BlueField. eMPW allows the Tx burst function to pack up multiple packets
  in a single descriptor session in order to save PCI bandwidth
  and improve performance at the cost of a slightly higher CPU usage.
  When ``txq_inline_mpw`` is set along with ``txq_mpw_en``,
  Tx burst function copies entire packet data on to Tx descriptor
  instead of including pointer of packet.

  The Enhanced Multi-Packet Write feature is enabled by default if NIC supports
  it, can be disabled by explicit specifying 0 value for ``txq_mpw_en`` option.
  Also, if minimal data inlining is requested by non-zero ``txq_inline_min``
  option or reported by the NIC, the eMPW feature is disengaged.

- ``txq_mem_algn`` parameter [int]

  A logarithm base 2 value for the memory starting address alignment
  for Tx queues' WQ and associated CQ.

  Different CPU architectures and generations may have different cache systems.
  The memory accessing order may impact the cache misses rate on different CPUs.
  This devarg gives the ability to control the umem alignment for all Tx queues
  without rebuilding the application binary.

  The performance can be tuned by specifying this devarg after benchmark testing
  on a specific system and hardware.

  By default, ``txq_mem_algn`` is set to log2(4K),
  or log2(64K) on some specific OS distributions -
  based on the system page size configuration.
  All Tx queues will use a unique memory region and umem area.
  Each Tx queue will start at an address right after the previous one
  except the first queue that will be aligned at the given size
  of address boundary controlled by this devarg.

  If the value is less then the page size, it will be rounded up.
  If it is bigger than the maximal queue size, a warning message will appear,
  there will be some waste of memory at the beginning.

  0 indicates legacy per queue memory allocation and separate Memory Regions (MR).

- ``tx_db_nc`` parameter [int]

  This parameter name is deprecated and ignored.
  The new name for this parameter is ``sq_db_nc``.
  See :ref:`common driver options <mlx5_common_driver_options>`.

.. _mlx5_tx_pp_param:

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

  A nonzero value enables Tx vector with ConnectX-5 NICs and above.
  if the number of global Tx queues on the port is less than ``txqs_max_vec``.
  The parameter is deprecated and ignored.

.. _mlx5_rx_vec_param:

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

.. _mlx5_vxlan_param:

- ``l3_vxlan_en`` parameter [int]

  A nonzero value allows L3 VXLAN and VXLAN-GPE flow creation. To enable
  L3 VXLAN or VXLAN-GPE, users has to configure firmware and enable this
  parameter. This is a prerequisite to receive this kind of traffic.

  Disabled by default.

.. _mlx5_meta_mark_param:

- ``dv_xmeta_en`` parameter [int]

  A nonzero value enables extensive flow metadata support if device is
  capable and driver supports it. This can enable extensive support of
  ``MARK`` and ``META`` item of ``rte_flow``.

  There are some possible configurations, depending on parameter value:

  - 0, this is default value, defines the legacy mode, the ``MARK`` and
    ``META`` related actions and items operate only within NIC Tx and
    NIC Rx steering domains, no ``MARK`` and ``META`` information crosses
    the domain boundaries. The ``MARK`` item is 24 bits wide, the ``META``
    item is 32 bits wide and match supported on egress only
    when ``dv_flow_en=1``.

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
    Tunnel offload is not supported in synchronous flow API
    when using :ref:`HW steering <mlx5_hws>`.

  - 4, this mode is only supported in :ref:`HW steering <mlx5_hws>`.
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

  The Direct Verbs/Rules (engaged with ``dv_flow_en=1``) supports all
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

.. _mlx5_dv_flow:

- ``dv_flow_en`` parameter [int]

  Value 0 means legacy Verbs flow offloading.

  Value 1 enables the DV flow steering assuming it is supported by the
  driver (requires rdma-core 24 or higher).

  Value 2 enables the WQE based hardware steering.
  In this mode, only queue-based flow management is supported.

  By default, the PMD will set this value according to capability.
  If DV flow steering is supported, it will be set to 1.
  If DV flow steering is not supported and HW steering is supported,
  then it will be set to 2.
  Otherwise, it will be set to 0.

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

.. _mlx5_representor_params:

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

  On ConnectX-7 multi-host setup, the PF index is not continuous,
  and must be queried in sysfs::

    cat /sys/class/net/*/phys_port_name

  With an example output 0 and 2 for PF1 and PF2, use [0,2] for PF index
  to probe VF port representors 0 through 2 on both PFs of bonding device::

    <Primary_PCI_BDF>,representor=pf[0,2]vf[0-2]

- ``max_dump_files_num`` parameter [int]

  The maximum number of files per PMD entity that may be created for debug information.
  The files will be created in /var/log directory or in current directory.

  set to 128 by default.

.. _mlx5_lro_timeout:

- ``lro_timeout_usec`` parameter [int]

  The maximum allowed duration of an LRO session, in micro-seconds.
  PMD will set the nearest value supported by HW, which is not bigger than
  the input ``lro_timeout_usec`` value.
  If this parameter is not specified, by default PMD will set
  the smallest value supported by HW.


.. _mlx5_hairpin_size:

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

  - 0. Prevent insertion of rules with the same pattern items on non-root HW table.
    In this case, only the first rule is inserted and the following rules are
    rejected and error code EEXIST is returned.

  - 1. Allow insertion of rules with the same pattern items.
    In this case, all rules are inserted but only the first rule takes effect,
    the next rule takes effect only if the previous rules are deleted.

  This option is not supported in :ref:`HW steering <mlx5_hws>`,
  and will be forced to 0 in this mode.

  By default, the PMD will set this value according to capability.


.. _mlx5_net_stats:

Statistics
----------

MLX5 supports various methods to report statistics:

Basic port statistics can be queried using ``rte_eth_stats_get()``.
The received and sent statistics are through SW only
and counts the number of packets received or sent successfully by the PMD.
The ``imissed`` counter is the amount of packets that could not be delivered
to SW because a queue was full.
Packets not received due to congestion in the bus or on the NIC
can be queried via the ``rx_discards_phy`` xstats counter.

Extended statistics can be queried using ``rte_eth_xstats_get()``.
The extended statistics expose a wider set of counters counted by the device.
The extended port statistics counts the number of packets
received or sent successfully by the port.
As NVIDIA NICs are using a :ref:`bifurcated Linux driver <bifurcated_driver>`,
those counters counts also packet received or sent by the Linux kernel.

Finally per-flow statistics can by queried using ``rte_flow_query()``
when attaching a count action for specific flow.
The flow counter counts the number of packets received successfully
by the port and match the specific flow rule.


Extended Statistics Counters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The counters with ``_phy`` suffix counts the total events on the physical port,
therefore not valid for VF.

Send Scheduling Counters
^^^^^^^^^^^^^^^^^^^^^^^^

The mlx5 PMD provides a comprehensive set of counters designed for
debugging and diagnostics related to packet scheduling during transmission.
These counters are applicable only if the port was configured with the ``tx_pp`` devarg
and reflect the status of the PMD scheduling infrastructure
based on Clock and Rearm Queues, used as a workaround on ConnectX-6 DX NICs.

``tx_pp_missed_interrupt_errors``
  Indicates that the Rearm Queue interrupt was not serviced on time.
  The EAL manages interrupts in a dedicated thread,
  and it is possible that other time-consuming actions were being processed concurrently.

``tx_pp_rearm_queue_errors``
  Signifies hardware errors that occurred on the Rearm Queue,
  typically caused by delays in servicing interrupts.

``tx_pp_clock_queue_errors``
  Reflects hardware errors on the Clock Queue,
  which usually indicate configuration issues
  or problems with the internal NIC hardware or firmware.

``tx_pp_timestamp_past_errors``
  Tracks the application attempted to send packets with timestamps set in the past.
  It is useful for debugging application code
  and does not indicate a malfunction of the PMD.

``tx_pp_timestamp_future_errors``
  Records attempts by the application to send packets
  with timestamps set too far into the future,
  exceeding the hardware’s scheduling capabilities.
  Like the previous counter, it aids in application debugging
  without suggesting a PMD malfunction.

``tx_pp_jitter``
  Measures the internal NIC real-time clock jitter estimation
  between two consecutive Clock Queue completions, expressed in nanoseconds.
  Significant jitter may signal potential clock synchronization issues,
  possibly due to inappropriate adjustments
  made by a system PTP (Precision Time Protocol) agent.

``tx_pp_wander``
  Indicates the long-term stability of the internal NIC real-time clock
  over 2^24 completions, measured in nanoseconds.
  Significant wander may also suggest clock synchronization problems.

``tx_pp_sync_lost``
  A general operational indicator;
  a non-zero value indicates that the driver has lost synchronization with the Clock Queue,
  resulting in improper scheduling operations.
  To restore correct scheduling functionality, it is necessary to restart the port.

The following counters are particularly valuable for verifying and debugging application code.
They do not indicate driver or hardware malfunctions
and are applicable to newer hardware with direct on-time scheduling capabilities
(such as ConnectX-7 and above):

``tx_pp_timestamp_order_errors``
  Indicates attempts by the application to send packets
  with timestamps that are not in strictly ascending order.
  Since the PMD does not reorder packets within hardware queues,
  violations of timestamp order can lead to packets being sent at incorrect times.


.. _mlx5_rx_functions:

Rx Burst Functions
------------------

There are multiple Rx burst functions with different advantages and limitations.

The Rx function is selected based on multiple parameters:

- :ref:`multi-segments Rx scatter <nic_features_scattered_rx>`
- :ref:`multi-packet Rx queues (MPRQ) <mlx5_mprq_params>`
- :ref:`vectorized Rx datapath <mlx5_rx_vec_param>`

This parameter may also have an impact on the behavior:

- :ref:`packet descriptor (CQE) compression <mlx5_cqe_comp_param>`

.. table:: Rx burst functions

   +-------------------+------------------------+---------+-----------------+------+-------+
   || Function Name    || Parameters to Enable  || Scatter|| Error Recovery || CQE || Large|
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


Rx/Tx Tuning
------------

The Tx datapath can be :ref:`traced for analysis <mlx5_tx_trace>`.

Below are some steps to improve performance of the datapath.

#. Configure aggressive CQE Zipping for maximum performance::

        mlxconfig -d <mst device> s CQE_COMPRESSION=1

   To set it back to the default CQE Zipping mode use::

        mlxconfig -d <mst device> s CQE_COMPRESSION=0

#. In case of virtualization:

   - Make sure that hypervisor kernel is 3.16 or newer.
   - Configure boot with ``iommu=pt``.
   - Use 1G huge pages.
   - Make sure to allocate a VM on huge pages.
   - Make sure to set CPU pinning.

#. Use the CPU near local NUMA node to which the PCIe adapter is connected,
   for better performance. For VMs, verify that the right CPU
   and NUMA node are pinned according to the above. Run::

        lstopo-no-graphics --merge

   to identify the NUMA node to which the PCIe adapter is connected.

#. If more than one adapter is used, and root complex capabilities allow
   to put both adapters on the same NUMA node without PCI bandwidth degradation,
   it is recommended to locate both adapters on the same NUMA node.
   This in order to forward packets from one to the other without
   NUMA performance penalty.

#. Disable pause frames::

        ethtool -A <netdev> rx off tx off

#. Verify IO non-posted prefetch is disabled by default. This can be checked
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

#. To minimize overhead of searching Memory Regions:

   - '--numa-mem' is recommended to pin memory by predictable amount.
   - Configure per-lcore cache when creating Mempools for packet buffer.
   - Refrain from dynamically allocating/freeing memory in run-time.


.. _mlx5_net_features:

Features & Support
------------------

.. rst-class:: punchcard
.. rst-class:: numbered-table

=======================================  =======  =======
Port/Queue Feature                        Linux   Windows
=======================================  =======  =======
:ref:`multi-process <mlx5_multiproc>`       X
:ref:`Virtual Function <mlx5_net_vf>`       X        X
:ref:`Sub-Function <mlx5_net_sf>`           X
:ref:`port representor <mlx5_repr>`         X        X
:ref:`hairpin <mlx5_hairpin>`               X        X
:ref:`statistics <mlx5_net_stats>`          X        P
link flow control (pause frame)             X
Rx interrupt                                X
:ref:`shared Rx queue <mlx5_shared_rx>`     X
:ref:`Rx threshold <mlx5_rx_threshold>`     X        X
:ref:`Rx drop delay <mlx5_drop>`            X        X
:ref:`Rx timestamp <mlx5_rx_timstp>`        X        X
:ref:`Tx scheduling <mlx5_tx_sched>`        X
:ref:`Tx inline <mlx5_tx_inline>`           X        X
:ref:`Tx fast free <mlx5_tx_fast_free>`     X        X
:ref:`Tx affinity <mlx5_aggregated>`        X
:ref:`buffer split <mlx5_buf_split>`        X        X
:ref:`multi-segment <mlx5_multiseg>`        X        X
promiscuous                                 X        X
multicast promiscuous                       X        X
multiple MAC addresses                      X
:ref:`LRO <mlx5_lro>`                       X        X
:ref:`TSO <mlx5_tso>`                       X        X
:ref:`L2 CRC <mlx5_crc>`                    X        X
:ref:`L3/L4 checksums <mlx5_cksums>`        X        X
:ref:`RSS with L3/L4 <mlx5_rss>`            X        X
:ref:`symmetric RSS <mlx5_rss>`             X        X
:ref:`configurable RETA <mlx5_rss>`         X        X
:ref:`RSS hash result <mlx5_rss>`           X        X
:ref:`VLAN <mlx5_vlan>`                     X        X
:ref:`host shaper <mlx5_host_shaper>`       X
=======================================  =======  =======


.. note::

   The mlx5 PMD is supported on Linux and :ref:`limited on Windows <mlx5_windows_limitations>`.


There are 2 different flow API:

- sync API (``rte_flow_create``)
- template async API (``rte_flow_template_table_create`` / ``rte_flow_async_create``)

The sync API is supported in both software (SWS) and hardware (HWS) steering engines,
while the template async API requires using the hardware steering engine (HWS).

Below tables apply to Linux implementation only.

.. rst-class:: punchcard
.. rst-class:: numbered-table

========================================  ========  ========  ==================
Flow Matching                             sync SWS  sync HWS  template async HWS
========================================  ========  ========  ==================
:ref:`aggr affinity <mlx5_aggregated>`       X         X              X
:ref:`compare <mlx5_compare>`                                         X
:ref:`conntrack <mlx5_conntrack>`            X         X              X
:ref:`eCPRI <mlx5_ecpri>`                    X         X              X
:ref:`Eth <mlx5_eth>`                        X         X              X
:ref:`flex <mlx5_flex_item>`                 X         X              X
:ref:`GENEVE <mlx5_geneve>`                  X         X              X
:ref:`GRE <mlx5_gre>`                        X         X              X
:ref:`GTP <mlx5_gtp>`                        X         X              X
:ref:`ICMP <mlx5_icmp>`                      X         X              X
:ref:`integrity <mlx5_integrity>`            X         X              X
:ref:`IP-in-IP <mlx5_ip>`                    X         X              X
:ref:`IPsec ESP <mlx5_ipsec>`                X         X              X
:ref:`IPv4 <mlx5_ip>`                        X         X              X
:ref:`IPv6 <mlx5_ip>`                        X         X              X
:ref:`mark <mlx5_mark>`                      X         X              X
:ref:`meta <mlx5_meta>`                      X         X              X
:ref:`meter color <mlx5_meter>`              X         X              X
:ref:`MPLSoGRE <mlx5_mpls>`                  X
:ref:`MPLSoUDP <mlx5_mpls>`                  X         X              X
:ref:`NSH <mlx5_nsh>`                        X         X              X
:ref:`NVGRE <mlx5_nvgre>`                    X         X              X
:ref:`port representor <mlx5_flow_port>`     X         X              X
:ref:`ptype <mlx5_ptype>`                                             X
:ref:`quota <mlx5_quota>`                                             X
:ref:`random <mlx5_random>`                            X              X
:ref:`represented port <mlx5_flow_port>`     X         X              X
:ref:`RoCE IB BTH <mlx5_roce>`               X         X              X
:ref:`SRv6 <mlx5_ip>`                                  X              X
:ref:`tag <mlx5_tag>`                        X         X              X
:ref:`TCP <mlx5_tcp>`                        X         X              X
:ref:`Tx queue <mlx5_flow_queue>`            X         X              X
:ref:`UDP <mlx5_udp>`                        X         X              X
:ref:`VLAN <mlx5_vlan>`                      X         X              X
:ref:`VXLAN <mlx5_vxlan>`                    X         X              X
========================================  ========  ========  ==================


.. rst-class:: punchcard
.. rst-class:: numbered-table

========================================  ========  ========  ==================
Flow Action                               sync SWS  sync HWS  template async HWS
========================================  ========  ========  ==================
:ref:`age <mlx5_age>`                        X         X              X
:ref:`conntrack <mlx5_conntrack>`            X         X              X
:ref:`count <mlx5_flow_count>`               X         X              X
:ref:`drop <mlx5_flow_drop>`                 X         X              X
:ref:`flag <mlx5_flag>`                      X         X              X
:ref:`indirect <mlx5_indirect>`              X         X              X
:ref:`indirect list <mlx5_indirect>`         X         X              X
:ref:`IPv6 ext push <mlx5_ip>`                         X              X
:ref:`IPv6 ext remove <mlx5_ip>`                       X              X
:ref:`jump <mlx5_jump>`                      X         X              X
:ref:`mark <mlx5_mark>`                      X         X              X
:ref:`meter <mlx5_meter>`                    X
:ref:`meter mark <mlx5_meter>`               X         X              X
:ref:`modify field <mlx5_modify>`            X         X              X
:ref:`NAT64 <mlx5_nat64>`                              X              X
:ref:`nvgre decap <mlx5_nvgre>`              X         X              X
:ref:`nvgre encap <mlx5_nvgre>`              X         X              X
:ref:`port representor <mlx5_flow_port>`     X         X              X
:ref:`quota <mlx5_quota>`                                             X
:ref:`queue <mlx5_flow_queue>`               X         X              X
:ref:`raw decap <mlx5_raw_header>`           X         X              X
:ref:`raw encap <mlx5_raw_header>`           X         X              X
:ref:`represented port <mlx5_flow_port>`     X         X              X
:ref:`rss <mlx5_rss>`                        X         X              X
:ref:`sample <mlx5_sample>`                  X         X              X
:ref:`send to kernel <mlx5_bifurcated>`      X         X              X
:ref:`set meta <mlx5_meta>`                  X         X              X
:ref:`set tag <mlx5_tag>`                    X         X              X
:ref:`VLAN push <mlx5_vlan>`                 X         X              X
:ref:`VLAN pop <mlx5_vlan>`                  X         X              X
:ref:`VLAN set <mlx5_vlan>`                  X         X              X
:ref:`VXLAN decap <mlx5_vxlan>`              X         X              X
:ref:`VXLAN decap <mlx5_vxlan>`              X         X              X
========================================  ========  ========  ==================


Supported Architectures
~~~~~~~~~~~~~~~~~~~~~~~

The mlx5 PMD is tested on several 64-bit architectures:

- Intel/AMD (x86_64)
- Arm (aarch64)
- IBM Power


Supported Operating Systems
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The mlx5 PMD is designed to run on Linux and Windows.
It requires a kernel driver and some libraries.

The :ref:`Linux prerequisites <mlx5_linux_prerequisites>`
are Linux kernel driver and rdma-core libraries.
These dependencies are also packaged in MLNX_OFED or MLNX_EN,
shortened below as "OFED".

The :ref:`Windows prerequisites <mlx5_windows_prerequisites>`
are Windows kernel driver and DevX library.
These dependencies are packaged in WinOF2.

.. _mlx5_windows_limitations:

On Windows, the features are limited:

- The flow template asynchronous API is not supported.

- The following flow rules are supported:

  - IPv4/UDP with CVLAN filtering
  - Unicast MAC filtering

- Additional rules are supported from WinOF2 version 2.70:

  - IPv4/TCP with CVLAN filtering
  - L4 steering rules for port RSS of IP, UDP, TCP

- Tunnel protocol support:

  - NVGRE (requires DevX dynamic insertion mode)


.. _mlx5_multiproc:

Multi-Process
~~~~~~~~~~~~~

When starting a DPDK application, the process mode can be primary (only one)
or secondary (all others).

The flow group 0 is shared between DPDK processes
while the other flow groups are limited to the current process.

Live Migration
^^^^^^^^^^^^^^

During live migration to a new process,
the flow engine must be set as standby mode.
The user should only program flow rules in group 0 (``fdb_def_rule_en=0``).

The flow engine of a process cannot move from active to standby mode
if preceding active application rules are still present and vice versa.

Live migration is only supported with SWS (``dv_flow_en=1``).

Limitations
^^^^^^^^^^^

#. Forked secondary processes are not supported with mlx5.

#. MPRQ is not supported in multi-process.
   The callback to free externally attached MPRQ buffer
   is set in a primary process,
   but has a different virtual address in a secondary process.
   Calling the function at the wrong address would crash.

#. External memory unregistered in EAL memseg list cannot be used for DMA
   unless such memory has been
   registered by ``mlx5_mr_update_ext_mp()`` in the primary process
   and remapped to the same virtual address in the secondary process.
   If the external memory is registered by the primary process
   but has a different virtual address in the secondary process,
   unexpected error may happen.


.. _mlx5_flow_create:

Flow Rule Creation
~~~~~~~~~~~~~~~~~~

In mlx5 devices, the flow rules are inserted in HW tables.

As explained in the :doc:`flow API documentation <../prog_guide/ethdev/flow_offload>`,
a rule can be created synchronously (``rte_flow_create``)
or asynchronously (``rte_flow_async_create``)
with the help of a template table (``rte_flow_template_table_create``).
A template table is added as a matcher entity inside a HW table.

Depending on the attribute of a rule,
it belongs to different domains:

- ingress: NIC Rx domain
- egress: NIC Tx domain
- transfer: E-Switch domain

The Embedded Switch (E-Switch) contains a Forwarding DataBase (FDB)
which controls the forwarding of packets between E-Switch ports
if :ref:`switchdev mode <mlx5_switchdev>` is enabled in the kernel.
The FDB is updated by creating flow rules and template tables in E-Switch domain.

The flow rules of the same group and same domain are in the same HW table.
Inside a group, the flow rules may have different priorities:
up to 21844 priorities in non-root tables.

There is a default HW root table in each domain.
These root tables are shared with the mlx5 kernel driver,
and this is where rules lookup starts,
then other groups are reached with :ref:`jump redirections <mlx5_jump>`.

In NIC Rx and NIC Tx domains, the first group (index 0) is the HW root table.
In E-Switch domain, by default (``fdb_def_rule_en=1``),
the FDB root table is hidden and only used internally for default rules.
In this case, the first group (index 0) for transfer rules is an FDB table
which is linked to the FDB root table.

A template table specialized
with the flags ``RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG``
or ``RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG``
can be created only in non-root HW table.

The flow rules database can be :ref:`dumped and parsed with a tool <mlx5_flow_dump>`.

Tuning
^^^^^^

In order to achieve the best flow rule insertion rate,
the flow rule creation should be managed in parallel lcores.

The flow object allocation and release with cache may be accelerated
by disabling memory reclaim with ``reclaim_mem_mode=0``.


.. _mlx5_hws:

Hardware Steering
~~~~~~~~~~~~~~~~~

Faster than software steering (SWS),
hardware steering (HWS) is the only mode supporting the flow template async API.

Flow rules are managed by the hardware,
with a WQE-based high scaling and safer flow insertion/destruction.

Requirements
^^^^^^^^^^^^

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
firmware   xx.35.1012
DPDK       22.11
=========  =============

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

HW steering is :ref:`enabled <mlx5_dv_flow>` with ``dv_flow_en=2``.

Reconfiguring HW steering engine is not supported.
Any subsequent call to ``rte_flow_configure()`` with different configuration
than initially provided will be rejected.

.. note::

   The application must call ``rte_flow_configure()``
   before creating any flow rules
   when using :ref:`asynchronous flow API <flow_async_api>`.
   It is also recommended for synchronous API.

Limitations
^^^^^^^^^^^

#. With E-Switch enabled, ports which share the E-Switch domain
   should be started and stopped in a specific order:

   - When starting ports, the transfer proxy port should be started first
     and port representors should follow.
   - When stopping ports, all port representors should be stopped
     before stopping the transfer proxy port.

   If ports are started/stopped in an incorrect order,
   ``rte_eth_dev_start()``/``rte_eth_dev_stop()`` will return an appropriate error code:

   - ``-EAGAIN`` for ``rte_eth_dev_start()``.
   - ``-EBUSY`` for ``rte_eth_dev_stop()``.

#. The supported actions order is as below::

      MARK (a)
      *_DECAP (b)
      OF_POP_VLAN
      COUNT | AGE
      METER_MARK | CONNTRACK
      OF_PUSH_VLAN
      MODIFY_FIELD
      *_ENCAP (c)
      JUMP | DROP | RSS (a) | QUEUE (a) | REPRESENTED_PORT (d)

   a. Only supported on ingress.
   b. Any decapsulation action, including the combination of RAW_ENCAP and RAW_DECAP actions
      which results in L3 decapsulation.
      Not supported on egress.
   c. Any encapsulation action, including the combination of RAW_ENCAP and RAW_DECAP actions
      which results in L3 encap.
   d. Only in transfer (switchdev) mode.

#. When using synchronous flow API,
   the following limitations and considerations apply:

   - There are limitations on size of match fields.
     Exceeding these limitations will result in an error,
     unlike other flow engines (``dv_flow_en`` < 2)
     that handle this by creating a tree of rules.

   - When updating a rule by inserting a new one and deleting the old one,
     for non-zero group, after adding the new rule,
     and before the deletion of the old rule, the new rule will be matched,
     contrary to the behavior in other flow engines (``dv_flow_en`` < 2)
     in which the old rule will be matched.

   - By default, the port is configured with zeroed ``rte_flow_port_attr``:
     there are zero flow queues (one is created by default),
     no actions, and no flags set.
     The default flow queue size for ``rte_flow_queue_attr`` is 32
     (used for the internal flow queue).
     If the application uses any configurable actions
     (such as meter, age, conntrack or counter),
     the system will allocate the maximum number of available actions per port.
     To optimize memory usage,
     the application should call ``rte_flow_configure``
     and specify only the required number of actions.
     If the application needs to modify flow queue settings,
     it should also use ``rte_flow_configure``.

   - When creating a rule with a partial `mask` provided in the flow item,
     the `spec` value must be calculated after the "AND" operation with the `mask`.
     If more significant bits are present in the `spec` than in the `mask`,
     the rule will be created without any error
     but the packets will not hit as expected.
     Such limitation will be removed in future.


.. _mlx5_bifurcated:

Bifurcated Driver
~~~~~~~~~~~~~~~~~

The same device is managed by both :ref:`kernel <bifurcated_driver>` and DPDK drivers.

After enabling the :ref:`isolated mode <flow_isolated_mode>`,
non-matched packets are routed directly from the hardware to the kernel.

.. note::

   The isolated mode must be enabled with ``rte_flow_isolate()``
   before calling ``rte_eth_dev_configure()``.
   It will avoid automatic rules created for basic configurations
   like promiscuous, multicast, MAC, VLAN, RSS.

In isolated mode, if a specific flow needs to be received by the kernel,
it is possible to use the flow action ``RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL``.

Limitations
^^^^^^^^^^^

The flow action ``RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL``
is not supported on HW root table.

In :ref:`HW steering <mlx5_hws>`, the action ``RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL``
is not supported on guest port.

Example
^^^^^^^

Usage of ``RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL`` in testpmd::

   testpmd> flow create 0 ingress priority 0 group 1 pattern eth type spec 0x0800 type mask 0xffff / end actions send_to_kernel / end


Multiport E-Switch
~~~~~~~~~~~~~~~~~~

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
- matching traffic coming from E-Switch manager
  using REPRESENTED_PORT item with port ID ``UINT16_MAX``;
- forwarding traffic to physical port, PF, VF or SF using REPRESENTED_PORT actions;

Requirements
^^^^^^^^^^^^

Supported HCAs:

- ConnectX family: ConnectX-6 Dx and above.
- BlueField family: BlueField-2 and above.
- FW version: at least ``XX.37.1014``.

Supported mlx5 kernel modules versions:

- Upstream Linux - from version 6.3 with CONFIG_NET_TC_SKB_EXT and CONFIG_MLX5_CLS_ACT enabled.
- Modules packaged in MLNX_OFED - from version v23.04-0.5.3.3.

Configuration
^^^^^^^^^^^^^

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
^^^^^^^^^^^

- Multiport E-Switch is not supported on Windows.
- Multiport E-Switch is supported only with :ref:`HW steering <mlx5_hws>`.
- Matching traffic coming from a physical port and forwarding it to a physical port
  (either the same or other one) is not supported.

  In order to achieve such a functionality, an application has to setup hairpin queues
  between physical port representors and forward the traffic using hairpin queues.


.. _mlx5_net_vf:

Virtual Function
~~~~~~~~~~~~~~~~

SR-IOV Virtual Function (VF) is a type of supported port.

VF must be :ref:`trusted <mlx5_vf_trusted>`
to be able to offload flow rules to a group bigger than 0.

Limitations
^^^^^^^^^^^

#. MTU settings on PCI Virtual Functions have no effect.
   The maximum receivable packet size for a VF is determined by the MTU
   configured on its associated Physical Function (PF).
   DPDK applications using VFs must be prepared to handle packets
   up to the maximum size of this PF port.

#. Flow rules created on VF devices can only match
   traffic targeted at the configured MAC addresses.

   .. note::

      MAC addresses not already present in the bridge table
      of the associated kernel network device
      will be added and cleaned up by the PMD when closing the device.
      In case of ungraceful program termination,
      some entries may remain present and should be removed manually by other means.


.. _mlx5_net_sf:

Sub-Function
~~~~~~~~~~~~

See :ref:`mlx5_sub_function`.

A SF netdev supports E-Switch representation offload
similar to PF and VF representors.
Use ``sfnum`` to probe SF representor::

   testpmd> port attach <PCI_BDF>,representor=sf<sfnum>,dv_flow_en=1


.. _mlx5_repr:

Port Representor
~~~~~~~~~~~~~~~~

The entities allowing to manage the ports of the hardware switch
are the port representors.

The flow item and action for port representor
are described in a section dedicated to handling
:ref:`port in a flow rule <mlx5_flow_port>`.

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

Port representors require to configure
the device in :ref:`switchdev mode <mlx5_switchdev>`.

The port representors and their matching behaviour are configured
with some :ref:`parameters <mlx5_representor_params>`.


.. _mlx5_hairpin:

Hairpin Ports
~~~~~~~~~~~~~

Requirements
^^^^^^^^^^^^

These are the requirements for using 1 hairpin port:

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-5
OFED       4.7-3
rdma-core  26
DPDK       19.11
=========  ==========

These are the requirements for using 2 hairpin ports:

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-5
OFED       5.1-2
DPDK       20.11
=========  ==========

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Hairpin Rx queue data may be stored in locked internal device memory
if enabled by setting these values (see :ref:`mlx5_firmware_config`)::

   HAIRPIN_DATA_BUFFER_LOCK=1
   MEMIC_SIZE_LIMIT=0

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

The size of the buffers may be specified
with the parameter :ref:`hp_buf_log_sz <mlx5_hairpin_size>`.

By default, data buffers and packet descriptors for hairpin queues
are placed in device memory which is shared with other resources (e.g. flow rules).

Since DPDK 22.11 and NVIDIA MLNX_OFED 5.8, it is possible
to specify memory placement for hairpin Rx and Tx queues:

- Place data buffers and Rx packet descriptors in dedicated device memory.
  Application can request that configuration
  through ``use_locked_device_memory`` configuration option.

  Placing data buffers and Rx packet descriptors in dedicated device memory
  can decrease latency on hairpinned traffic,
  since traffic processing for the hairpin queue will not be memory starved.

  However, reserving device memory for hairpin Rx queues
  may decrease throughput under heavy load,
  since less resources will be available on device.

  This option is supported only for Rx hairpin queues.

- Place Tx packet descriptors in host memory.
  Application can request that configuration
  through ``use_rte_memory`` configuration option.

  Placing Tx packet descriptors in host memory can increase traffic throughput.
  This results in more resources available on the device for other purposes,
  which reduces memory contention on device.
  Side effect of this option is visible increase in latency,
  since each packet incurs additional PCI transactions.

  This option is supported only for Tx hairpin queues.

Limitations
^^^^^^^^^^^

#. Hairpin between two ports could only do manual binding and explicit Tx flow mode.
   For single port hairpin, all the combinations of auto/manual binding
   and explicit/implicit Tx flow mode are supported.

#. Hairpin in switchdev SR-IOV mode is not supported till now.

#. ``out_of_buffer`` statistics are not available on:

   - NICs older than ConnectX-7.
   - DPUs older than BlueField-3.


.. _mlx5_aggregated:

Aggregated Ports
~~~~~~~~~~~~~~~~

When multiple ports are merged together like in bonding,
the aggregated physical ports are hidden behind the virtual bonding port.
However we may need to distinguish the physical ports
to transmit packets on the same port as where the flow is received.

The number of aggregated ports can be revealed
with the function ``rte_eth_dev_count_aggr_ports()``.

On Rx side, an aggregated port is matched in a flow rule
with the item ``RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY``
so it can be directed to a specific queue.

On Tx side, queues may be mapped to an aggregated port
with ``rte_eth_dev_map_aggr_tx_affinity()``,
otherwise the queue affinity depends on HW hash.

Requirements
^^^^^^^^^^^^

Linux bonding under socket direct mode requires at least MLNX_OFED 5.4.

Matching on aggregated affinity in E-Switch
depends on device-managed flow steering (DMFS) mode.

Limitations
^^^^^^^^^^^

#. Matching on aggregated affinity is supported only in group 0.

Example
^^^^^^^

See `affinity matching with testpmd`_.


.. _mlx5_shared_rx:

Shared Rx Queue
~~~~~~~~~~~~~~~

Limitations
^^^^^^^^^^^

#. Counters of received packets and bytes of devices in the same share group are same.

#. Counters of received packets and bytes of queues in the same group and queue ID are same.


.. _mlx5_rx_threshold:

Rx Threshold
~~~~~~~~~~~~

When a queue receives too many packets,
an event is sent to allow taking actions.

This threshold is a percentage of available descriptors in Rx queue,
and it is configured per queue with the function ``rte_eth_rx_avail_thresh_set()``.
When the number of available descriptor is too low,
the event ``RTE_ETH_EVENT_RX_AVAIL_THRESH`` is fired.
Then the busy queue can be found with ``rte_eth_rx_avail_thresh_query()``.

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

The Rx descriptor threshold event requires DevX (``UCTX_EN=1``).

Limitations
^^^^^^^^^^^

#. The Rx descriptor threshold cannot be configured
   on a shared Rx queue or a hairpin queue.


.. _mlx5_drop:

Rx Drop
~~~~~~~

When all packet descriptors of an Rx queue are exhausted,
the received packets are dropped.

It is possible to :ref:`delay <mlx5_delay_drop_param>` such packet drop,
waiting a short time for descriptors to become available.
By default, there is a delay before dropping packets of hairpin Rx queues.

Requirements
^^^^^^^^^^^^

There are requirements for having drop delay supported:

=========  ==========
Minimum    Version
=========  ==========
OFED       5.5
=========  ==========

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

To enable / disable the drop timer rearming,
the private flag ``dropless_rq`` can be set and queried via ethtool::

   ethtool --set-priv-flags <netdev> dropless_rq on (/ off)
   ethtool --show-priv-flags <netdev>

Limitations
^^^^^^^^^^^

#. The configuration of the drop delay is global for the port, and must be set on the PF.

#. The drop delay timer is per port, shared with the Rx queues of VF, SF and representors.


.. _mlx5_rx_timstp:

Rx Timestamp
~~~~~~~~~~~~

A :ref:`timestamp <nic_features_hw_timestamp>` may be written in packet metadata.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-4
firmware   12.21.1000
OFED       4.2-1
Linux      4.14
rdma-core  16
DPDK       17.11
=========  ==========

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Real-time timestamp format is enabled
by setting this value (see :ref:`mlx5_firmware_config`)::

   REAL_TIME_CLOCK_ENABLE=1

Limitations
^^^^^^^^^^^

#. CQE timestamp field width is limited by hardware to 63 bits, MSB is zero.

#. In the free-running mode the timestamp counter is reset on power on
   and 63-bit value provides over 1800 years of uptime till overflow.

#. In the real-time mode
   (configurable with ``REAL_TIME_CLOCK_ENABLE`` firmware settings),
   the timestamp presents the nanoseconds elapsed since 01-Jan-1970,
   hardware timestamp overflow will happen on 19-Jan-2038
   (0x80000000 seconds since 01-Jan-1970).

#. The send scheduling is based on timestamps
   from the reference "Clock Queue" completions,
   the scheduled send timestamps should not be specified with non-zero MSB.


.. _mlx5_tx_sched:

Tx Scheduling
~~~~~~~~~~~~~

When PMD sees the ``RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME`` set on the packet
being sent it tries to synchronize the time of packet appearing on
the wire with the specified packet timestamp. If the specified one
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
in desired order.

Requirements
^^^^^^^^^^^^

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
firmware   22.28.2006
OFED       5.1-2
Linux
rdma-core
DPDK       20.08
=========  =============

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

To provide the packet send scheduling on mbuf timestamps the ``tx_pp``
parameter should be specified.

Limitations
^^^^^^^^^^^

#. The timestamps can be put only in the first packet
   in the burst providing the entire burst scheduling.


.. _mlx5_tx_inline:

Tx Inline
~~~~~~~~~

The packet data can be inlined into the packet descriptor (WQE)
to save some PCI bandwidth.
It has a CPU cost but it reduces the latency and can improve the throughput.

There is flag to disable inlining per packet.

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

Inlining is enabled per port
through multiple :ref:`configuration parameters <mlx5_tx_inline_params>`.

The configuration per packet is described below.

To support a mixed traffic pattern (some buffers from local host memory,
some buffers from other devices) with high bandwidth, an mbuf flag is used.

An application hints the PMD whether or not it should try to inline
the given mbuf data buffer.
The PMD should do the best effort to act upon this request.

The hint flag ``RTE_PMD_MLX5_FINE_GRANULARITY_INLINE`` is dynamic,
registered by the application with ``rte_mbuf_dynflag_register()``.
This flag is purely driver-specific
and declared in the PMD specific header ``rte_pmd_mlx5.h``.

To query the supported specific flags in runtime,
the function ``rte_pmd_mlx5_get_dyn_flag_names()`` returns the array
of currently (over present hardware and configuration) supported specific flags.

The "not inline hint" feature operating flow is the following one:

#. application starts
#. probe the devices, ports are created
#. query the port capabilities
#. if port supporting the feature is found
#. register dynamic flag ``RTE_PMD_MLX5_FINE_GRANULARITY_INLINE``
#. application starts the ports
#. on ``dev_start()`` PMD checks whether the feature flag is registered
   and enables the feature support in datapath
#. application might set the registered flag bit in ``ol_flags`` field
   of mbuf being sent and PMD will handle ones appropriately.


.. _mlx5_tx_fast_free:

Fast Tx mbuf Free
~~~~~~~~~~~~~~~~~

With the flag ``RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE``,
it is assumed that all mbufs being sent are originated from the same memory pool
and there is no any extra references to the mbufs
(the reference counter for each mbuf is equal to 1 on Tx burst).
The latter means there should be no any externally attached buffers in mbufs.

It is an application responsibility to provide the correct mbufs
if the :ref:`fast free offload <nic_features_fast_mbuf_free>` is engaged.

Requirements
^^^^^^^^^^^^

:ref:`MPRQ <mlx5_mprq_params>` must be disabled.

The mlx5 PMD implicitly produces the mbufs with externally attached buffers
if MPRQ option is enabled, hence, the fast free offload is
neither supported nor advertised if MPRQ is enabled.


.. _mlx5_buf_split:

Buffer Split
~~~~~~~~~~~~

On Rx, the packets can be :ref:`split at a specific offset <nic_features_buffer_split>`,
allowing header or payload processing from a specific memory or device.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-5
firmware   16.28.2006
OFED       5.1-2
DPDK       20.11
=========  ==========

Limitations
^^^^^^^^^^^

#. Buffer split offload is supported with regular Rx burst routine only,
   no MPRQ feature or vectorized code can be engaged.


.. _mlx5_multiseg:

Multi-Segment Scatter/Gather
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:ref:`nic_features_scattered_rx`

Limitations
^^^^^^^^^^^

#. A multi-segment packet must not have more segments
   than reported in ``rte_eth_dev_info.tx_desc_lim.nb_seg_max``.
   This value depends on maximal supported Tx descriptor size
   and ``txq_inline_min`` settings,
   and may be from 2 (worst case forced by maximal inline settings) to 58.


.. _mlx5_lro:

LRO
~~~

:ref:`Large Receive Offload (LRO) <nic_features_lro>`
aggregates multiple packets in a big buffer.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-5
firmware   16.25.6406
OFED       4.6-4
DPDK       19.08
=========  ==========

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

LRO requires DevX (``UCTX_EN=1``).

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

LRO requires DV flow to be enabled (``dv_flow_en=1`` or ``dv_flow_en=2``).

The session timeout may be changed
with the parameter :ref:`lro_timeout_usec <mlx5_lro_timeout>`.

Limitations
^^^^^^^^^^^

#. The driver rounds down the port configuration value ``max_lro_pkt_size``
   (from ``rte_eth_rxmode``) to a multiple of 256 due to hardware limitation.

#. LRO is performed only for packet size larger than ``lro_min_mss_size``.
   This value is reported on device start, when debug mode is enabled.

#. Rx queue with LRO offload enabled, receiving a non-LRO packet,
   can forward it with size limited to max LRO size, not to max Rx packet length.

#. ``RTE_ETH_RX_OFFLOAD_KEEP_CRC`` offload cannot be supported with LRO.

#. The first mbuf length, without headroom,
   must be big enough to include the TCP header (122B).

#. LRO can be used with outer header of TCP packets of the standard format::

     eth (with or without vlan) / ipv4 or ipv6 / tcp / payload

   Other TCP packets (e.g. with MPLS label) received on Rx queue with LRO enabled,
   will be received with bad checksum.


.. _mlx5_tso:

TSO
~~~

:ref:`TCP Segmentation Offload (TSO) <nic_features_tso>`
is supported for generic IP or UDP tunnel, including VXLAN and GRE.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-4
firmware   12.21.1000
OFED       4.2-1
Linux      4.14
rdma-core  16
DPDK       17.11
=========  ==========


.. _mlx5_crc:

CRC
~~~

:ref:`nic_features_crc_offload`

Requirements
^^^^^^^^^^^^

``RTE_ETH_RX_OFFLOAD_KEEP_CRC`` is not supported with decapsulation for some devices
(such as ConnectX-6 Dx, ConnectX-6 Lx, ConnectX-7, BlueField-2 and BlueField-3).
The capability bit ``scatter_fcs_w_decap_disable`` shows HW support.

Limitations
^^^^^^^^^^^

#. ``RTE_ETH_RX_OFFLOAD_KEEP_CRC`` is not supported if :ref:`mlx5_lro` is enabled.


.. _mlx5_cksums:

L3/L4 Checksums Offloading
~~~~~~~~~~~~~~~~~~~~~~~~~~

Checksum API includes
:ref:`L3 <nic_features_l3_checksum_offload>`,
:ref:`L4 <nic_features_l4_checksum_offload>`,
:ref:`inner L3 <nic_features_inner_l3_checksum>`
and :ref:`inner L4 <nic_features_inner_l4_checksum>`.

IP and UDP checksums are verified on Rx, including in tunnels.

Hardware checksum Tx offload is supported
for generic IP or UDP tunnel, including VXLAN and GRE.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-4
firmware   12.21.1000
OFED       4.2-1
Linux      4.14
rdma-core  16
DPDK       17.11
=========  ==========


.. _mlx5_rss:

RSS
~~~

Receive Side Scaling (RSS) allows to dispatch received packets to multiple queues.

RSS can be configured at port level:

- :ref:`nic_features_rss_hash`
- :ref:`nic_features_rss_key_update`
- :ref:`nic_features_rss_reta_update`

or at flow level with ``RTE_FLOW_ACTION_TYPE_RSS``,
including :ref:`inner RSS <nic_features_inner_rss>`.

Different combinations of fields are supported:
L3 only, L4 only or both, and source only, destination only or both.

Multiple RSS hash keys are supported, one for each flow type.

The symmetric RSS function is supported by swapping source and destination
addresses and ports.

Requirements
^^^^^^^^^^^^

=========  ==========  =================
Minimum    Version     for Shared Action
=========  ==========  =================
hardware   ConnectX-4  ConnectX-5
OFED       4.5         5.2
rdma-core  23          33
DPDK       18.11       20.11
=========  ==========  =================

Limitations
^^^^^^^^^^^

#. RSS hash result:

   Full support is only available when hash RSS format is selected
   as the current :ref:`CQE compression format <mlx5_cqe_comp_param>` on the Rx side.

   Using any other format may result in some Rx packets
   not having the ``RTE_MBUF_F_RX_RSS_HASH`` flag set.

#. If :ref:`multi-packet Rx queue <mlx5_mprq_params>` is configured
   and :ref:`Rx CQE compression <mlx5_cqe_comp_param>` is enabled at the same time,
   RSS hash result is not fully supported.
   This is because the checksum format is selected by default in this configuration.

#. Flex item fields (``next_header``, ``next_protocol``, ``samples``)
   do not participate in RSS hash functions.

#. ``RTE_FLOW_ACTION_TYPE_RSS`` can be used in transfer flow rules,
   since firmware version xx.43.1014,
   but only on template tables
   with ``RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG`` specialization.

Examples
^^^^^^^^

- `RSS`_
- `RSS IPv6`_
- `RSS IPv6 in indirect action`_
- `RSS in flow template`_
- `RSS with custom hash`_


.. _mlx5_flow_queue:

Flow Queue
~~~~~~~~~~

A flow may be sent to a specific queue with ``RTE_FLOW_ACTION_TYPE_QUEUE``.

The egress traffic sent via a specific Tx queue
is matched with ``RTE_FLOW_ITEM_TYPE_TX_QUEUE``.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-4
OFED       4.5
rdma-core  23
DPDK       18.11
=========  ==========

Limitations
^^^^^^^^^^^

#. ``RTE_FLOW_ACTION_TYPE_QUEUE`` can be used in transfer flow rules,
   since firmware version xx.43.1014,
   but only on template tables
   with ``RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG`` specialization.


.. _mlx5_flow_port:

Flow Port ID
~~~~~~~~~~~~

The flow item ``RTE_FLOW_ITEM_TYPE_PORT_ID`` is deprecated.
For matching flows coming from a specific port,
the items ``RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR``
and ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT`` can be used.

The flow action ``RTE_FLOW_ACTION_TYPE_PORT_ID`` is deprecated.
For sending flows to a specific port,
the actions ``RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR``
and ``RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT`` can be used.

In E-Switch mode, a flow rule matching
``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT`` with port ID ``UINT16_MAX``
means matching packets sent by the E-Switch manager from software.
This feature requires at least OFED 24.04 or an upstream equivalent.

Requirements
^^^^^^^^^^^^

=========  ============
Minimum    for E-Switch
=========  ============
hardware   ConnectX-5
OFED       4.7-1
rdma-core  24
DPDK       19.05
=========  ============

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

The behaviour of port representors is configured
with some :ref:`parameters <mlx5_representor_params>`.

Limitations
^^^^^^^^^^^

#. The NIC egress flow rules on port representor are not supported.

#. A driver limitation for ``RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR`` action
   restricts the ``port_id`` configuration to only accept the value ``0xffff``,
   indicating the E-Switch manager.

#. When using synchronous flow API with :ref:`HW steering <mlx5_hws>`:

   - ``RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR`` is not supported.
     ``RTE_FLOW_ITEM_TYPE_TX_QUEUE`` can be used with a rule for each queue.
   - Transfer rules are not supported on representor ports.
   - Rules created on proxy ports without explicit represented port matching
     will match packets from all ports.

Examples
^^^^^^^^

- When testpmd starts with a PF, a VF-rep0 and a VF-rep1,
  the below example will redirect packets from VF0 and VF1 to the wire::

     testpmd> flow create 0 ingress transfer pattern eth / port_representor / end actions represented_port ethdev_port_id 0 / end

- To match on traffic from port representor 1 and redirect to port 2::

     testpmd> flow create 0 transfer pattern eth / port_representor port_id is 1 / end actions represented_port ethdev_port_id 2 / end


.. _mlx5_flow_drop:

Flow Drop
~~~~~~~~~

A flow may be dropped with ``RTE_FLOW_ACTION_TYPE_DROP``.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-4
OFED       4.6           4.5
rdma-core  24            23
DPDK       19.05         18.11
=========  ============  ============


.. _mlx5_jump:

Flow Jump
~~~~~~~~~

A flow rule may redirect to another group of rules
with ``RTE_FLOW_ACTION_TYPE_JUMP``.

In template API, it is possible to redirect to a specific index
in a flow template table with ``RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX``.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       4.7-1         4.7-1
rdma-core  24
DPDK       19.05         19.02
=========  ============  ============

Jump to a template table index is supported with these requirements:

=========  ============
Minimum    Version
=========  ============
DPDK       24.11
=========  ============

Limitations
^^^^^^^^^^^

#. With the async template API, jumping to a template table
   with ``RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG`` specialization
   from a template table with a different specialization
   is supported since firmware version xx.43.1014.

Example
^^^^^^^

See `jump to table index with testpmd`_.


.. _mlx5_indirect:

Flow Indirect
~~~~~~~~~~~~~

An action can be created separately of a flow rule.
Then the indirect action can be referenced in one or more flow rules
with ``RTE_FLOW_ACTION_TYPE_INDIRECT``.

Similarly a list of indirect actions can be referenced
with ``RTE_FLOW_ACTION_TYPE_INDIRECT_LIST``.

Limitations
^^^^^^^^^^^

#. For actions list, the handle in the template action part must be non-zero.

#. If an indirect actions list handle is non-masked,
   the handle will be used as a reference to the action type in template creation.

#. If an indirect actions list handle is masked,
   the mask will be used in template creation and flow rule.

Example
^^^^^^^

- `indirect list of encap/decap`_


.. _mlx5_meta:

Flow Metadata
~~~~~~~~~~~~~

A metadata can be attached to a flow with ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD``
and matched with ``RTE_FLOW_ITEM_TYPE_META``.

The flow items ``RTE_FLOW_ITEM_TYPE_META`` and ``RTE_FLOW_ITEM_TYPE_MARK``
are interrelated with the datapath -
they might move from/to the applications in mbuf fields.
Hence, zero value for these items has the special meaning "no metadata are provided".
Non-zero values are treated by the application and the driver as valid ones.

Moreover in the flow engine domain, the value zero is acceptable to match and set,
and it should be allowed to specify zero values as parameters
for the META and MARK flow items and actions.
In the same time, zero mask has no meaning and should be rejected on validation stage.

Starting from firmware version 47.0274,
if :ref:`switchdev mode <mlx5_switchdev>` was enabled,
flow metadata can be shared between flows in FDB and VF domains:

* If metadata was attached to FDB flow
  and that flow transferred incoming packet to a VF,
  representor, ingress flow bound to the VF can match the metadata.

* If metadata was attached to VF egress flow, FDB flow can match the metadata.

The metadata sharing functionality is controlled with firmware configuration.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       4.7-3         4.7-3
rdma-core  26            26
DPDK       19.11         19.11
=========  ============  ============

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

The parameter :ref:`dv_xmeta_en <mlx5_meta_mark_param>`
allows to configure the driver handling of META and MARK.

Limitations
^^^^^^^^^^^

#. In SW steering (``dv_flow_en<2``),
   no Tx metadata go to the E-Switch steering domain for the flow group 0.
   It means the rules attaching metadata within group 0 are rejected.


.. _mlx5_mark:

Flow Mark
~~~~~~~~~

A mark can be attached to a flow with ``RTE_FLOW_ACTION_TYPE_MARK``
and matched with ``RTE_FLOW_ITEM_TYPE_MARK``.

See notes about :ref:`mlx5_meta`.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-4
OFED       4.6           4.5
rdma-core  24            23
DPDK       19.05         18.11
=========  ============  ============

Limitations
^^^^^^^^^^^

#. When using synchronous flow API with :ref:`HW steering <mlx5_hws>`,
   ``RTE_FLOW_ITEM_TYPE_MARK`` (16-bit match) is unsupported.
   ``RTE_FLOW_ITEM_TYPE_META`` (32-bit match) can be used as an alternative.

#. ``RTE_FLOW_ACTION_TYPE_MARK`` can be used in transfer flow rules,
   since firmware version xx.43.1014,
   but only on template tables
   with ``RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG`` specialization.


.. _mlx5_flag:

Flow Flag
~~~~~~~~~

A flow can be flagged with ``RTE_FLOW_ACTION_TYPE_FLAG``.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-4
OFED       4.6           4.5
rdma-core  24            23
DPDK       19.05         18.11
=========  ============  ============


.. _mlx5_tag:

Flow Tag
~~~~~~~~

A tag is matched with the flow item ``RTE_FLOW_ITEM_TYPE_TAG``.



.. _mlx5_integrity:

Flow Integrity
~~~~~~~~~~~~~~

Packets may be matched on their integrity status (like checksums)
with ``RTE_FLOW_ITEM_TYPE_INTEGRITY``.

Requirements
^^^^^^^^^^^^

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
DPDK       21.05
=========  =============

Limitations
^^^^^^^^^^^

#. Verification bits provided by the hardware are:

   - ``l3_ok``
   - ``ipv4_csum_ok``
   - ``l4_ok``
   - ``l4_csum_ok``

#. ``level`` value 0 references outer headers.

#. Negative integrity item verification is not supported.

#. With SW steering (``dv_flow_en=1``)

   - Multiple integrity items are not supported in a single flow rule.
   - The network headers referred by the integrity item must be explicitly matched.
     For example, if the integrity mask sets ``l4_ok`` or ``l4_csum_ok`` bits,
     the L4 network headers, TCP or UDP, must be in the rule pattern.
     Examples::

        flow create 0 ingress pattern integrity level is 0 value mask l3_ok value spec l3_ok / eth / ipv6 / end ...
        flow create 0 ingress pattern integrity level is 0 value mask l4_ok value spec l4_ok / eth / ipv4 proto is udp / end ...

#. With :ref:`HW steering <mlx5_hws>`

   - The ``l3_ok`` field represents all L3 checks, but nothing about IPv4 checksum.
   - The ``l4_ok`` field represents all L4 checks including L4 checksum.


.. _mlx5_flow_count:

Flow Count
~~~~~~~~~~

Matched packets (and bytes) counters may be enabled
in flow rules with ``RTE_FLOW_ACTION_TYPE_COUNT``.

Below is described how it works in the :ref:`HW steering <mlx5_hws>` flow engine.

Flow counters are allocated from HW in bulks.
A set of bulks forms a flow counter pool managed by PMD.
When flow counters are queried from HW,
each counter is identified by an offset in a given bulk.
Querying HW flow counter requires sending a request to HW,
which will request a read of counter values for given offsets.
HW will asynchronously provide these values through a DMA write.

In order to optimize HW to SW communication,
these requests are handled in a separate counter service thread
spawned by mlx5 PMD.
This service thread will refresh the counter values stored in memory,
in cycles, each spanning ``svc_cycle_time`` milliseconds.
By default, ``svc_cycle_time`` is set to 500.
When applications query the ``COUNT`` flow action,
PMD returns the values stored in host memory.

mlx5 PMD manages 3 global rings of allocated counter offsets:

- ``free`` ring - Counters which were not used at all.
- ``wait_reset`` ring - Counters which were used in some flow rules,
  but were recently freed (flow rule was destroyed
  or an indirect action was destroyed).
  Since the count value might have changed
  between the last counter service thread cycle and the moment it was freed,
  the value in host memory might be stale.
  During the next service thread cycle,
  such counters will be moved to ``reuse`` ring.
- ``reuse`` ring - Counters which were used at least once
  and can be reused in new flow rules.

When counters are assigned to a flow rule (or allocated to indirect action),
the PMD first tries to fetch a counter from ``reuse`` ring.
If it's empty, the PMD fetches a counter from ``free`` ring.

The counter service thread works as follows:

#. Record counters stored in ``wait_reset`` ring.
#. Read values of all counters which were used at least once
   or are currently in use.
#. Move recorded counters from ``wait_reset`` to ``reuse`` ring.
#. Sleep for ``(query time) - svc_cycle_time`` milliseconds
#. Repeat.

Requirements
^^^^^^^^^^^^

=========  ============  =============  ======================   =================
Minimum    for E-Switch  NIC domain     E-Switch Shared Action   NIC Shared Action
=========  ============  =============  ======================   =================
hardware   ConnectX-5    ConnectX-5     ConnectX-5               ConnectX-5
firmware
OFED       4.6           4.6            4.6                      4.6
Linux
rdma-core  24            23             24                       23
DPDK       19.05         19.02          21.05                    21.05
=========  ============  =============  ======================   =================

Limitations
^^^^^^^^^^^

With :ref:`HW steering <mlx5_hws>`:

#. Because freeing a counter (by destroying a flow rule or destroying indirect action)
   does not immediately make it available for the application,
   the PMD might return:

   - ``ENOENT`` if no counter is available in ``free``, ``reuse``
     or ``wait_reset`` rings.
     No counter will be available until the application releases some of them.
   - ``EAGAIN`` if no counter is available in ``free`` and ``reuse`` rings,
     but there are counters in ``wait_reset`` ring.
     This means that after the next service thread cycle,
     new counters will be available.

   The application has to be aware that flow rule or indirect action creation
   might need to be retried.

#. Using count action on root tables requires:

   - Linux kernel >= 6.4
   - rdma-core >= 60.0


.. _mlx5_age:

Flow Age
~~~~~~~~

A flow rule timeout can be set with ``RTE_FLOW_ACTION_TYPE_AGE``.

Requirements
^^^^^^^^^^^^

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
OFED       5.2
rdma-core  32
DPDK       20.11
=========  =============

Limitations
^^^^^^^^^^^

With :ref:`HW steering <mlx5_hws>`,

#. Using the same indirect count action combined with multiple age actions
   in different flows may cause a wrong age state for the age actions.

#. Creating/destroying flow rules with indirect age action when it is active
   (timeout != 0) may cause a wrong age state for the indirect age action.

#. The driver reuses counters for aging action, so for optimization
   the values in ``rte_flow_port_attr`` structure should describe:

   - ``nb_counters`` is the number of flow rules using counter (with/without age)
     in addition to flow rules using only age (without count action).
   - ``nb_aging_objects`` is the number of flow rules containing age action.

#. When using synchronous flow API with :ref:`HW steering <mlx5_hws>`,
   aged flows are reported only once.

#. With strict queueing enabled
   (``RTE_FLOW_PORT_FLAG_STRICT_QUEUE`` passed to ``rte_flow_configure()``),
   indirect age actions can be created only through asynchronous flow API.

#. Using age action on root tables requires:

   - Linux kernel >= 6.4
   - rdma-core >= 60.0


.. _mlx5_quota:

Quota
~~~~~

A quota limit (packets or bytes) may be applied to a flow
with ``RTE_FLOW_ACTION_TYPE_QUOTA``.
Then the quota state is handled by matching with ``RTE_FLOW_ITEM_TYPE_QUOTA``.

Requirements
^^^^^^^^^^^^

Such flow rule requires :ref:`HW steering <mlx5_hws>`.

Limitations
^^^^^^^^^^^

#. Quota is supported only in non-root HW tables (group > 0).

#. Quota must be an indirect rule.

#. The maximal value is ``INT32_MAX`` (2G).

#. After increasing the value with ``RTE_FLOW_UPDATE_QUOTA_ADD``,
   the next update must be with ``RTE_FLOW_UPDATE_QUOTA_SET``.

#. The maximal number of HW quota and HW meter objects is ``16e6``.

#. ``RTE_FLOW_ACTION_TYPE_QUOTA`` cannot be used in the same rule
   with a meter action or ``RTE_FLOW_ACTION_TYPE_CONNTRACK``.

Example
^^^^^^^

See `quota usage with testpmd`_.


.. _mlx5_meter:

Metering
~~~~~~~~

Meter profile packet mode is supported.

Meter profiles of RFC2697, RFC2698 and RFC4115 are supported.

RFC4115 implementation is following MEF,
meaning yellow traffic may reclaim unused green bandwidth
when green token bucket is full.

A meter M can be created on port X and to be shared with a port Y
on the same switch domain:

.. code-block:: console

   flow create X ingress transfer pattern eth / port_id id is Y / end actions meter mtr_id M / end

A termination meter M can be the policy green action of another termination meter N.
The two meters are chained together as a chain. Using meter N in a flow will apply
both the meters in hierarchy on that flow:

.. code-block:: console

   add port meter policy 0 1 g_actions queue index 0 / end y_actions end r_actions drop / end
   create port meter 0 M 1 1 yes 0xffff 1 0
   add port meter policy 0 2 g_actions meter mtr_id M / end y_actions end r_actions drop / end
   create port meter 0 N 2 2 yes 0xffff 1 0
   flow create 0 ingress group 1 pattern eth / end actions meter mtr_id N / end

Requirements
^^^^^^^^^^^^

=========  ==========  =============  =============
Minimum    Basic       ASO            Hierarchy
=========  ==========  =============  =============
hardware   ConnectX-5  ConnectX-6 Dx  ConnectX-6 Dx
firmware
OFED       4.7-3       5.3            5.3
Linux
rdma-core  26          33
DPDK       19.11       21.05          21.08
=========  ==========  =============  =============

Limitations
^^^^^^^^^^^

#. All the meter colors with drop action will be counted only by the global drop statistics.

#. Yellow detection is only supported with ASO metering.

#. Red color must be with drop action.

#. Meter statistics are supported only for drop case.

#. A meter action created with pre-defined policy must be the last action in the flow except single case where the policy actions are:

   - green: NULL or END.
   - yellow: NULL or END.
   - RED: DROP / END.

#. The only supported meter policy actions:

   - green: QUEUE, RSS, PORT_ID, REPRESENTED_PORT, JUMP, DROP, MODIFY_FIELD, MARK, METER and SET_TAG.
   - yellow: QUEUE, RSS, PORT_ID, REPRESENTED_PORT, JUMP, DROP, MODIFY_FIELD, MARK, METER and SET_TAG.
   - RED: must be DROP.

#. Policy actions of RSS for green and yellow should have the same configuration except queues.

#. Policy with RSS/queue action is not supported when ``dv_xmeta_en`` enabled.

#. If green action is METER, yellow action must be the same METER action or NULL.

#. When using DV flow engine (``dv_flow_en=1``),
   if meter has drop count
   or meter hierarchy contains any meter that uses drop count,
   it cannot be used by flow rule matching all ports.

#. When using DV flow engine (``dv_flow_en=1``),
   if meter hierarchy contains any meter that has MODIFY_FIELD/SET_TAG,
   it cannot be used by flow matching all ports.

#. When using :ref:`HW steering <mlx5_hws>`,
   only ``RTE_FLOW_ACTION_TYPE_METER_MARK`` is supported,
   not ``RTE_FLOW_ACTION_TYPE_METER``.

#. The maximal number of HW quota and HW meter objects is ``16e6``.


Examples
^^^^^^^^

- `meter mark with sync API`_
- `meter mark with template API`_


.. _mlx5_sample:

Sampling
~~~~~~~~

A ratio of a flow can be duplicated before getting a fate action applied.
Such sampling rule is configured with ``RTE_FLOW_ACTION_TYPE_SAMPLE``.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       5.1-2         5.1-2
rdma-core  32
DPDK       20.11         20.11
=========  ============  ============

Limitations
^^^^^^^^^^^

#. Supports ``RTE_FLOW_ACTION_TYPE_SAMPLE`` action only within NIC Rx and
   E-Switch steering domain.

#. In E-Switch steering domain, for sampling with sample ratio > 1 in a transfer rule,
   additional actions are not supported in the sample actions list.

#. For ConnectX-5, the ``RTE_FLOW_ACTION_TYPE_SAMPLE`` is typically used as
   first action in the E-Switch egress flow if with header modify or
   encapsulation actions.

#. For NIC Rx flow, supports only ``MARK``, ``COUNT``, ``QUEUE``, ``RSS`` in the
   sample actions list.

#. In E-Switch steering domain, for mirroring with sample ratio = 1 in a transfer rule,
   supports only ``RAW_ENCAP``, ``PORT_ID``, ``REPRESENTED_PORT``, ``VXLAN_ENCAP``, ``NVGRE_ENCAP``
   in the sample actions list.

#. In E-Switch steering domain, for mirroring with sample ratio = 1 in a transfer rule,
   the encapsulation actions (``RAW_ENCAP`` or ``VXLAN_ENCAP`` or ``NVGRE_ENCAP``)
   support uplink port only.

#. In E-Switch steering domain, for mirroring with sample ratio = 1 in a transfer rule,
   the port actions (``PORT_ID`` or ``REPRESENTED_PORT``) with uplink port and ``JUMP`` action
   are not supported without the encapsulation actions
   (``RAW_ENCAP`` or ``VXLAN_ENCAP`` or ``NVGRE_ENCAP``) in the sample actions list.

#. For ConnectX-5 trusted device, the application metadata with SET_TAG index 0
   is not supported before ``RTE_FLOW_ACTION_TYPE_SAMPLE`` action.

Examples
^^^^^^^^

Create an indirect action list with sample and jump::

   testpmd> set sample_actions 1 port_representor port_id 0xffff / end
   testpmd> flow indirect_action 0 create transfer list actions sample ratio 1 index 1 / jump group 2 / end

E-Switch mirroring: the matched ingress packets are sent to port 2,
mirror the packets with NVGRE encapsulation and send to port 0::

   testpmd> set nvgre ip-version ipv4 tni 4 ip-src 127.0.0.1 ip-dst 128.0.0.1 eth-src 11:11:11:11:11:11 eth-dst 22:22:22:22:22:22
   testpmd> set sample_actions 0 nvgre_encap / port_id id 0 / end
   testpmd> flow create 0 transfer pattern eth / end actions sample ratio 1 index 0 / port_id id 2 / end


.. _mlx5_random:

Random
~~~~~~

A flow rule can match randomly by using ``RTE_FLOW_ITEM_TYPE_RANDOM``.

Requirements
^^^^^^^^^^^^

Such flow rule requires :ref:`HW steering <mlx5_hws>`.

Limitations
^^^^^^^^^^^

#. Supports matching only 16 bits (LSB).

#. NIC ingress/egress flow in group 0 is not supported.

#. Supported only in template table with ``nb_flows=1``.

Example
^^^^^^^

See `random matching with testpmd`_.


.. _mlx5_flex_item:

Flex Item
~~~~~~~~~

A flow can be matched on a set of fields at specific offsets
with ``RTE_FLOW_ITEM_TYPE_FLEX``.

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching a flex item requires to enable the dynamic flex parser
by setting these values::

   FLEX_PARSER_PROFILE_ENABLE=4
   PROG_PARSE_GRAPH=1

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.

Limitations
^^^^^^^^^^^

#. Flex item is supported on PF only.

#. The header length mask width can go up to 6 bits.

#. The Firmware supports 8 global sample fields.
   Each flex item allocates non-shared sample fields from that pool.

#. The flex item can have 1 input link - ``eth`` or ``udp``
   and up to 3 output links - ``ipv4`` or ``ipv6``.

#. In flex item configuration, ``next_header.field_base`` value
   must be byte aligned (multiple of 8).

#. With ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD``, the flex item offset
   must be byte aligned (multiple of 8).

#. The flex item fields (``next_header``, ``next_protocol``, ``samples``)
   do not participate in RSS hash functions.


.. _mlx5_raw_header:

Raw Header
~~~~~~~~~~

A flow can be encapsulated with an additional header
provided through ``RTE_FLOW_ACTION_TYPE_RAW_ENCAP``.

Similarly a header can be removed by decapsulating a flow
with ``RTE_FLOW_ACTION_TYPE_RAW_DECAP``.

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       4.7-1         4.6
rdma-core  24            23
DPDK       19.05         19.02
=========  ============  ============

Limitations
^^^^^^^^^^^

#. The input buffer, used as outer header for raw encapsulation, is not validated.

#. The input buffer, providing the removal size of decapsulation, is not validated.

#. The buffer size must match the length of the headers to be removed.

#. The decapsulation is always done up to the outermost tunnel detected by the HW.


.. _mlx5_modify:

Header Field Modification
~~~~~~~~~~~~~~~~~~~~~~~~~

Packet headers of a flow can be modified with ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD``.

Requirements
^^^^^^^^^^^^

=========  ==========
Minimum    Version
=========  ==========
hardware   ConnectX-5
OFED       5.2
rdma-core  35
DPDK       21.02
=========  ==========

Limitations
^^^^^^^^^^^

#. Supports the 'set' operation for ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD`` in all flow engines.

#. Supports the 'add' operation with 'src' field
   of type ``RTE_FLOW_FIELD_VALUE`` or ``RTE_FLOW_FIELD_POINTER``
   with both :ref:`HW steering <mlx5_hws>` and DV flow engine (``dv_flow_en=1``).

   HW steering flow engine, starting with ConnectX-7 and BlueField-3,
   supports packet header fields in 'src' field.

   'dst' field can be any of the following:

   - ``RTE_FLOW_FIELD_IPV4_TTL``
   - ``RTE_FLOW_FIELD_IPV6_HOPLIMIT``
   - ``RTE_FLOW_FIELD_TCP_SEQ_NUM``
   - ``RTE_FLOW_FIELD_TCP_ACK_NUM``
   - ``RTE_FLOW_FIELD_TAG``
   - ``RTE_FLOW_FIELD_META``
   - ``RTE_FLOW_FIELD_FLEX_ITEM``
   - ``RTE_FLOW_FIELD_TCP_DATA_OFFSET``
   - ``RTE_FLOW_FIELD_IPV4_IHL``
   - ``RTE_FLOW_FIELD_IPV4_TOTAL_LEN``
   - ``RTE_FLOW_FIELD_IPV6_PAYLOAD_LEN``

#. In template tables of group 0, the modify action must be fully masked.

#. Modification of an arbitrary place in a packet via
   the special ``RTE_FLOW_FIELD_START`` field ID is not supported.

#. Offsets must be 4-byte aligned: 32, 64 and 96 for IPv6.

#. Offsets cannot skip past the boundary of a field.

#. For packet fields (e.g. MAC addresses, IPv4 addresses or L4 ports)
   offset specifies the number of bits to skip from field's start,
   starting from MSB in the first byte, in the network order.

#. For flow metadata fields (e.g. META or TAG)
   offset specifies the number of bits to skip from field's start,
   starting from LSB in the least significant byte, in the host order.

#. Cannot use ``RTE_FLOW_FIELD_RANDOM``.

#. Cannot use ``RTE_FLOW_FIELD_MARK``.

#. Modification of the 802.1Q tag is not supported.

#. If the field type is ``RTE_FLOW_FIELD_MAC_TYPE``
   and packet contains one or more VLAN headers,
   the meaningful type field following the last VLAN header
   is used as modify field operation argument.
   The modify field action is not intended to modify VLAN headers type field,
   dedicated VLAN push and pop actions should be used instead.

#. Modification of VXLAN network or GENEVE network ID
   is supported only for :ref:`HW steering <mlx5_hws>`.

#. Modification of the VXLAN header is supported with below limitations:

   - Only for :ref:`HW steering <mlx5_hws>`.
   - Support VNI and the last reserved byte modifications for traffic
     with default UDP destination port: 4789 for VXLAN and VXLAN-GBP, 4790 for VXLAN-GPE.

#. Modification of GENEVE network ID is not supported when configured
   ``FLEX_PARSER_PROFILE_ENABLE`` supports GENEVE TLV options.
   See :ref:`mlx5_firmware_config` for more flex parser information.

#. Modification of GENEVE TLV option fields
   is supported only for :ref:`HW steering <mlx5_hws>`.
   Only DWs configured in :ref:`parser creation <mlx5_geneve_parser>` can be modified,
   'type' and 'class' fields can be modified when ``match_on_class_mode=2``.

#. Modification of GENEVE TLV option data supports one DW per action.

#. Modification of the MPLS header is supported with some limitations:

   - Only in :ref:`HW steering <mlx5_hws>`.
   - Only in ``src`` field.
   - Only for outermost tunnel header (``level=2``).
     For ``RTE_FLOW_FIELD_MPLS``,
     the default encapsulation level ``0`` describes the outermost tunnel header.

   .. note::

      The default encapsulation level ``0`` describes
      the "outermost that match is supported",
      currently it is the first tunnel,
      but it can be changed to outer when it is supported.

#. Default encapsulation level ``0`` describes outermost.

#. Encapsulation level ``2`` is supported with some limitations:

   - Only in :ref:`HW steering <mlx5_hws>`.
   - Only in ``src`` field.
   - ``RTE_FLOW_FIELD_VLAN_ID`` is not supported.
   - ``RTE_FLOW_FIELD_IPV4_PROTO`` is not supported.
   - ``RTE_FLOW_FIELD_IPV6_PROTO/DSCP/ECN`` are not supported.
   - ``RTE_FLOW_FIELD_ESP_PROTO/SPI/SEQ_NUM`` are not supported.
   - ``RTE_FLOW_FIELD_TCP_SEQ/ACK_NUM`` are not supported.
   - Second tunnel fields are not supported.

#. Encapsulation levels greater than ``2`` are not supported.

Examples
^^^^^^^^

- `set value`_
- `copy field`_


.. _mlx5_compare:

Field Comparison
~~~~~~~~~~~~~~~~

The traffic may be matched in a flow by comparing some header or metadata fields
to be lesser, greater or equal a value or another field,
thanks to ``RTE_FLOW_ITEM_TYPE_COMPARE``.

Requirements
^^^^^^^^^^^^

Such flow rule requires :ref:`HW steering <mlx5_hws>`.

Limitations
^^^^^^^^^^^

#. Only single flow is supported to the flow table.

#. Only single item is supported per pattern template.

#. In switch mode,
   matching ``RTE_FLOW_ITEM_TYPE_COMPARE`` is not supported for ``ingress`` rules.
   This is because an implicit ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``
   needs to be added to the matcher,
   which conflicts with the single item limitation.

#. Only 32-bit comparison is supported or 16-bit for random field.

#. Only supported for ``RTE_FLOW_FIELD_META``, ``RTE_FLOW_FIELD_TAG``,
   ``RTE_FLOW_FIELD_ESP_SEQ_NUM``,
   ``RTE_FLOW_FIELD_RANDOM`` and ``RTE_FLOW_FIELD_VALUE``.

#. The field type ``RTE_FLOW_FIELD_VALUE`` must be the base (``b``) field.

#. The field type ``RTE_FLOW_FIELD_RANDOM`` can only be compared with
   ``RTE_FLOW_FIELD_VALUE``.

Examples
^^^^^^^^

- `compare ESP to value`_
- `compare ESP to meta`_
- `compare meta to value`_
- `compare tag to value`_
- `compare random to value`_


.. _mlx5_ptype:

Packet Type
~~~~~~~~~~~

When receiving a packet, its type is parsed
and :ref:`stored <nic_features_packet_type_parsing>`
in the field ``packet_type`` of ``rte_mbuf``.

The same packet type format may be used to match a flow
with ``RTE_FLOW_ITEM_TYPE_PTYPE``.

Requirements
^^^^^^^^^^^^

Such flow rule requires :ref:`HW steering <mlx5_hws>`.

Limitations
^^^^^^^^^^^

#. The supported values are:

   - L2: ``RTE_PTYPE_L2_ETHER``, ``RTE_PTYPE_L2_ETHER_VLAN``, ``RTE_PTYPE_L2_ETHER_QINQ``
   - L3: ``RTE_PTYPE_L3_IPV4``, ``RTE_PTYPE_L3_IPV6``
   - L4: ``RTE_PTYPE_L4_TCP``, ``RTE_PTYPE_L4_UDP``, ``RTE_PTYPE_L4_ICMP``
     and their ``RTE_PTYPE_INNER_XXX`` counterparts as well as ``RTE_PTYPE_TUNNEL_ESP``.

   Any other values are not supported.
   Using them as a value will cause unexpected behavior.

#. Matching on both outer and inner fragmented IP is supported
   using ``RTE_PTYPE_L4_FRAG`` and ``RTE_PTYPE_INNER_L4_FRAG`` values.
   They are not part of L4 types, so they should be provided explicitly
   as a mask value during pattern template creation.
   Providing ``RTE_PTYPE_L4_MASK`` during pattern template creation
   and ``RTE_PTYPE_L4_FRAG`` during flow rule creation
   will cause unexpected behavior.

Example
^^^^^^^

See `packet type matching with testpmd`_.


.. _mlx5_eth:

Ethernet
~~~~~~~~

Ethernet header is matched with the flow item ``RTE_FLOW_ITEM_TYPE_ETH``.


.. _mlx5_roce:

RoCE
~~~~

Remote Direct Memory Access (RDMA) over Converged Ethernet (RoCE)
can be matched through the InfiniBand Base Transport Header (BTH)
with the flow item ``RTE_FLOW_ITEM_TYPE_IB_BTH``.

Limitations
^^^^^^^^^^^

#. ``RTE_FLOW_ITEM_TYPE_IB_BTH`` is not supported in group 0.


.. _mlx5_icmp:

ICMP
~~~~

Internet Control Message Protocol (ICMP) for IPv4 and IPv6
are matched respectively with
``RTE_FLOW_ITEM_TYPE_ICMP`` and ``RTE_FLOW_ITEM_TYPE_ICMP6``.
There are more flow items for ICMPv6.

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching ICMP flow is enabled by setting this value::

   FLEX_PARSER_PROFILE_ENABLE=2

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.


.. _mlx5_ip:

IP
~~

Internet Protocol (IP) v4 and v6
are matched respectively with the flow items
``RTE_FLOW_ITEM_TYPE_IPV4`` and ``RTE_FLOW_ITEM_TYPE_IPV6``.

There are more flow items for IPv6 extensions (like SRv6)
which can be pushed or removed thanks to the flow offload actions
``RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH`` and ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE``.

Translation between IPv4 and IPv6 can be offloaded with :ref:`mlx5_nat64`.

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching IP-in-IP tunnel flow is enabled by setting this value::

   FLEX_PARSER_PROFILE_ENABLE=0

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.

Limitations
^^^^^^^^^^^

#. IPv6 5-tuple matching is supported with :ref:`HW steering <mlx5_hws>`
   from ConnectX-8/BlueField-3.
   Previous devices support matching on either IPv6 `src` or `dst` in a rule.
   In general, the matching limitation is related to the number of dwords:
   older hardware supports up to 5 dwords for matching,
   while newer hardware (ConnectX-8/BlueField-3 and up)
   supports up to 11 dwords for matching.

#. IPv6 multicast messages are not supported on VM,
   while promiscuous mode and allmulticast mode are both set to off.
   To receive IPv6 Multicast messages on VM,
   explicitly set the relevant MAC address
   using the function ``rte_eth_dev_mac_addr_add()``.

#. IPv6 header item ``proto`` field, indicating the next header protocol,
   should not be set as extension header.
   In case the next header is an extension header,
   it should not be specified in IPv6 header item ``proto`` field.
   The last extension header item 'next header' field
   can specify the following header protocol type.

#. Match on IPv6 routing extension header requires :ref:`HW steering <mlx5_hws>`,
   and supports the following fields:

   - ``type``
   - ``next_hdr``
   - ``segments_left``

#. IPv6 routing extension matching is not supported in flow template relaxed mode
   (see ``struct rte_flow_pattern_template_attr::relaxed_matching``).

#. Matching ICMP6 following IPv6 routing extension header
   should match ``ipv6_routing_ext_next_hdr`` instead of ICMP6.

#. IPv6 routing extension push/remove:

   - Requires :ref:`HW steering <mlx5_hws>`.
   - Supported in non-zero group
     (no limits on transfer domain if ``fdb_def_rule_en=1`` which is default).
   - Supports TCP or UDP as next layer.
   - IPv6 routing header must be the only present extension.
   - Not supported on guest port.

#. IP-in-IP is not supported with :ref:`HW steering <mlx5_hws>`.

#. Matching on packet headers appearing after an IP header is not supported
   if that packet is an IP fragment.
   Example:

   - If a flow rule with pattern matching on L4 header contents is created,
     and the first IP fragment is received,
     then this IP fragment will miss on that flow rule.


.. _mlx5_nat64:

NAT64
~~~~~

Network Address Translation between IPv6 and IPv4 (NAT64)
can be offloaded with the flow action ``RTE_FLOW_ACTION_TYPE_NAT64``.

Requirements
^^^^^^^^^^^^

=========  =============
Minimum    Version
=========  =============
firmware   xx.39.1002
=========  =============

NAT64 requires :ref:`HW steering <mlx5_hws>`.

Limitations
^^^^^^^^^^^

#. Supported only on non-root HW table.

#. TOS / Traffic Class is not supported.

#. Actions order limitation should follow the :ref:`modify action <mlx5_modify>`.

#. The last 2 tag registers will be used implicitly in address backup mode.

#. Even if the action can be shared, new steering entries will be created per flow rule.
   It is recommended a single rule with NAT64 should be shared
   to reduce the duplication of entries.
   The default address and other fields conversion will be handled with NAT64 action.
   To support other address, new rule(s) with modify fields on the IP addresses
   should be created.


.. _mlx5_udp:

UDP
~~~

User Datagram Protocol (UDP)
is matched with the flow item ``RTE_FLOW_ITEM_TYPE_UDP``.

Limitations
^^^^^^^^^^^

#. Outer UDP checksum calculation for encapsulation flow actions:

  Currently available NVIDIA NICs and DPUs do not have a capability to calculate
  the UDP checksum in the header added using encapsulation flow actions.

  The application is required to use 0 in UDP checksum field in such flow actions.
  Resulting packet will have outer UDP checksum equal to 0.


.. _mlx5_tcp:

TCP
~~~

Transmission Control Protocol (TCP)
is matched with the flow item ``RTE_FLOW_ITEM_TYPE_TCP``.


.. _mlx5_conntrack:

TCP Connection Tracking
~~~~~~~~~~~~~~~~~~~~~~~

Connection tracking is the process of tracking a TCP connection
between two endpoints (e.g. between server and client).
This involves maintaining the state of the connection,
identifying state changing packets, malformed packets,
packets that do not conform to the protocol or the current connection state, and more.
This process requires state awareness.

:ref:`TCP connection tracking <flow_conntrack_action>` is offloaded to the hardware
thanks to the item ``RTE_FLOW_ITEM_TYPE_CONNTRACK``
and the action ``RTE_FLOW_ACTION_TYPE_CONNTRACK``.

This offload can track a single connection only,
meaning every connection should be offloaded separately.

Requirements
^^^^^^^^^^^^

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
OFED       5.3
rdma-core  35
DPDK       21.05
=========  =============

Limitations
^^^^^^^^^^^

#. 16 ports maximum (with ``dv_flow_en=1``).

#. 32M connections maximum.

#. The CT action must be wrapped by an action_handle

#. ``RTE_FLOW_ACTION_TYPE_CONNTRACK`` action cannot be inserted in group 0.

#. Flow rules insertion rate and memory consumption need more optimization.

#. Cannot track a connection before seeing the first 2 handshake packets (SYN and SYN-ACK).

#. No direction validation is done for closing sequence (FIN).

#. Retransmission limit checking cannot work.

#. Cannot co-exist with ASO meter, ASO age action in a single flow rule.


.. _mlx5_vlan:

VLAN
~~~~

Virtual Local Area Network (VLAN) tags
can be configured at port level:

- :ref:`nic_features_vlan_filter`
- :ref:`nic_features_vlan_offload`
- :ref:`nic_features_qinq_offload`

or at flow level with the item ``RTE_FLOW_ITEM_TYPE_VLAN``
and the actions :

- ``RTE_FLOW_ACTION_TYPE_OF_POP_VLAN``
- ``RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN``
- ``RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID``
- ``RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP``

Requirements
^^^^^^^^^^^^

=========  ====================  ===============
Minimum    Version for E-Switch  Version for NIC
=========  ====================  ===============
hardware   ConnectX-5            ConnectX-5
OFED       4.7-1                 4.7-1
DPDK       19.11                 19.11
=========  ====================  ===============

The flow transfer actions ``RTE_FLOW_ACTION_TYPE_OF_POP_VLAN`` on egress,
and ``RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN`` on ingress,
are supported with these requirements:

=========  ====================
Minimum    Version for E-Switch
=========  ====================
hardware   ConnectX-6 Dx
OFED       5.3
DPDK       21.05
=========  ====================

Limitations
^^^^^^^^^^^

#. When using Verbs flow engine (``dv_flow_en=0``),
   a flow pattern without any specific VLAN
   will match for VLAN packets as well.

   When VLAN spec is not specified in the pattern,
   the matching rule will be created with VLAN as a wild card.
   Meaning, the flow rule::

      flow create 0 ingress pattern eth / ipv4 / end ...

   will match any IPv4 packet (VLAN included).

   While the flow rule::

      flow create 0 ingress pattern eth / vlan vid is 3 / ipv4 / end ...

   will only match VLAN packets with ``vid=3``.

#. When using Verbs flow engine (``dv_flow_en=0``),
   multi-tagged (QinQ) match is not supported.

#. When using DV flow engine (``dv_flow_en=1``),
   flow pattern with any VLAN specification will match only single-tagged packets
   unless the ethdev item ``type`` field is 0x88A8
   or the VLAN item ``has_more_vlan`` field is 1.

   The flow rule::

      flow create 0 ingress pattern eth / ipv4 / end ...

   will match any IPv4 packet.

   The flow rules::

      flow create 0 ingress pattern eth / vlan / end ...
      flow create 0 ingress pattern eth has_vlan is 1 / end ...
      flow create 0 ingress pattern eth type is 0x8100 / end ...

   will match single-tagged packets only, with any VLAN ID value.

   The flow rules::

      flow create 0 ingress pattern eth type is 0x88A8 / end ...
      flow create 0 ingress pattern eth / vlan has_more_vlan is 1 / end ...

   will match multi-tagged packets only, with any VLAN ID value.

#. A flow pattern with 2 sequential VLAN items is not supported.

#. VLAN pop rule is not supported on egress traffic in NIC domain.

#. VLAN pop rule without a match on VLAN item is not supported.

#. VLAN push rule is not supported on ingress traffic in NIC domain.

#. VLAN set PCP rule is not supported on existing headers.


.. _mlx5_vxlan:

VXLAN
~~~~~

Virtual Extensible Local Area Network (VXLAN) tunnel
can be matched with the flow item ``RTE_FLOW_ITEM_TYPE_VXLAN``.

There are 2 flow actions for encapsulation and decapsulation:

- ``RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP``
- ``RTE_FLOW_ACTION_TYPE_VXLAN_DECAP``

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       4.7-1         4.6
rdma-core  24            23
DPDK       19.05         19.02
=========  ============  ============

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching VXLAN-GPE flow is enabled by setting one of these values::

   FLEX_PARSER_PROFILE_ENABLE=0
   or
   FLEX_PARSER_PROFILE_ENABLE=2

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.

The UDP destination port may be changed::

   IP_OVER_VXLAN_EN=1
   IP_OVER_VXLAN_PORT=<udp dport>

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

The parameter :ref:`l3_vxlan_en <mlx5_vxlan_param>` must be set.

Limitations
^^^^^^^^^^^

#. Matching on 8-bit flag and first 24-bit reserved fields
   is only supported when using :ref:`HW steering <mlx5_hws>`.

#. For ConnectX-5, the UDP destination port must be the standard one (4789).

#. Default UDP destination is 4789 if not explicitly specified.

#. Group zero's behavior may differ which depends on FW.

#. Different flags should be set when matching on VXLAN-GPE/GBP:

   - for VXLAN-GPE - P flag
   - for VXLAN-GBP - G flag

#. Matching on VXLAN-GPE fields ``rsvd0``/``rsvd1`` depends on FW version
   when using DV flow engine (``dv_flow_en=1``).

#. Matching on VXLAN-GPE field ``protocol`` should be explicitly specified
   in :ref:`HW steering <mlx5_hws>`.

#. L3 VXLAN and VXLAN-GPE tunnels cannot be supported together with MPLSoGRE and MPLSoUDP.

Examples
^^^^^^^^

- match VXLAN-GPE flags, next protocol, VNI, rsvd0 and rsvd1::

     testpmd> flow create 0 transfer group 0 pattern eth / end actions count / jump group 1 / end
     testpmd> flow create 0 transfer group 1 pattern eth / ipv4 / udp / vxlan-gpe flags is 12 protocol is 6 rsvd1 is 8 vni is 100 rsvd0 is 5 / end actions count / jump group 2 / end

- `match VXLAN-GBP`_

- `modify VXLAN header`_


.. _mlx5_gre:

GRE
~~~

Generic Routing Encapsulation (GRE) tunnel
can be matched with the flow item ``RTE_FLOW_ITEM_TYPE_GRE``.

More precise matching is possible with
``RTE_FLOW_ITEM_TYPE_GRE_OPTION`` and ``RTE_FLOW_ITEM_TYPE_GRE_KEY``.

The following fields of the GRE header can be matched:

  - bits C, K and S
  - protocol type
  - checksum
  - key
  - sequence

Requirements
^^^^^^^^^^^^

Matching on checksum and sequence needs MLNX_OFED 5.6+.

Limitations
^^^^^^^^^^^

#. When using synchronous flow API with :ref:`HW steering <mlx5_hws>`,
   matching the field ``c_rsvd0_ver`` is not supported on group 0 (root table).


.. _mlx5_nvgre:

NVGRE
~~~~~

Network Virtualization using Generic Routing Encapsulation (NVGRE) tunnel
can be matched with the flow item ``RTE_FLOW_ITEM_TYPE_NVGRE``.

There are 2 flow actions for encapsulation and decapsulation:

- ``RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP``
- ``RTE_FLOW_ACTION_TYPE_NVGRE_DECAP``

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       4.7-1         4.6
rdma-core  24            23
DPDK       19.05         19.02
=========  ============  ============

Limitations
^^^^^^^^^^^

#. In SW steering (``dv_flow_en=1``), this field can be matched:

   - tni

#. In :ref:`HW steering <mlx5_hws>`, these fields can be matched:

   - c_rc_k_s_rsvd0_ver
   - protocol
   - tni
   - flow_id


.. _mlx5_geneve:

GENEVE
~~~~~~

GEneric NEtwork Virtualization Encapsulation (GENEVE) tunnel
can be matched with the flow item ``RTE_FLOW_ITEM_TYPE_GENEVE``
for the following fields:

- VNI
- OAM
- protocol type
- options length

The variable length option can be matched with ``RTE_FLOW_ITEM_TYPE_GENEVE_OPT``
for the following fields:

- Class
- Type
- Length
- Data

Requirements
^^^^^^^^^^^^

=========  ============  ============
Minimum    for E-Switch  NIC domain
=========  ============  ============
hardware   ConnectX-5    ConnectX-5
OFED       4.7-3         4.7-3
rdma-core  27            27
DPDK       19.11         19.11
=========  ============  ============

For matching TLV option, these are the requirements:

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
OFED       5.2
rdma-core  34
DPDK       21.02
=========  =============

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching GENEVE flow is enabled by setting one of these values::

   FLEX_PARSER_PROFILE_ENABLE=0
   or
   FLEX_PARSER_PROFILE_ENABLE=1

Matching GENEVE flow with TLV option is enabled by setting one of these values::

   FLEX_PARSER_PROFILE_ENABLE=0
   or
   FLEX_PARSER_PROFILE_ENABLE=8

Matching GENEVE flow with TLV option in :ref:`HW steering <mlx5_hws>`
with the sync API requires this value::

   FLEX_PARSER_PROFILE_ENABLE=0

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.

Limitations
^^^^^^^^^^^

#. GENEVE TLV option matching has restrictions.

   Class/Type/Length fields must be specified as well as masks.
   Class/Type/Length specified masks must be full.
   Matching GENEVE TLV option without specifying data is not supported.
   Matching GENEVE TLV option with ``data & mask == 0`` is not supported.

   In SW steering (``dv_flow_en=1``):

   - Only one Class/Type/Length GENEVE TLV option is supported per shared device.
   - Supported only with ``FLEX_PARSER_PROFILE_ENABLE=0``.

   In :ref:`HW steering <mlx5_hws>`:

   - Multiple Class/Type/Length GENEVE TLV options are supported per physical device.
   - Multiple of same GENEVE TLV option isn't supported at the same pattern template.
   - Supported only with ``FLEX_PARSER_PROFILE_ENABLE=8``.
   - Supported also with ``FLEX_PARSER_PROFILE_ENABLE=0`` for single DW only.
   - Supported for FW version xx.37.0142 and above.

.. _mlx5_geneve_parser:

#. An API (``rte_pmd_mlx5_create_geneve_tlv_parser``)
   is available for the flexible parser used in :ref:`HW steering <mlx5_hws>`:

   Each physical device has 7 DWs for GENEVE TLV options.
   Partial option configuration is supported,
   mask for data is provided in parser creation
   indicating which DWs configuration is requested.
   Only masked data DWs can be matched later as item field using flow API.

   - Matching of ``type`` field is supported for each configured option.
   - However, for matching ``class`` field,
     the option should be configured with ``match_on_class_mode=2``.
     One extra DW is consumed for it.
   - Matching on ``length`` field is not supported.

   - More limitations with ``FLEX_PARSER_PROFILE_ENABLE=0``:

     - single DW
     - ``sample_len`` must be equal to ``option_len`` and not bigger than 1.
     - ``match_on_class_mode`` different than 1 is not supported.
     - ``offset`` must be 0.

   Although the parser is created per physical device, this API is port oriented.
   Each port should call this API before using GENEVE OPT item,
   but its configuration must use the same options list
   with same internal order configured by first port.

   Calling this API for different ports under same physical device doesn't consume
   more DWs, the first one creates the parser and the rest use same configuration.


.. _mlx5_gtp:

GTP
~~~

GPRS Tunneling Protocol (GTP)
can be matched with the flow item ``RTE_FLOW_ITEM_TYPE_GTP``
for the following fields:

- bits E, S and PN
- message type
- TEID

GTP extension header for PDU session container (extension header type ``0x85``)
can be matched with the flow item ``RTE_FLOW_ITEM_TYPE_GTP_PSC``.

Requirements
^^^^^^^^^^^^

For matching GTP PSC, these are the requirements:

=========  =============
Minimum    Version
=========  =============
hardware   ConnectX-6 Dx
OFED       5.2
rdma-core  35
DPDK       21.02
=========  =============

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching GTP flow is enabled by setting this value::

   FLEX_PARSER_PROFILE_ENABLE=3

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.

Limitations
^^^^^^^^^^^

#. Matching on GTP extension header is not supported in group 0.


.. _mlx5_mpls:

MPLS
~~~~

Multi Protocol Label Switching (MPLS) flows (MPLS-over-GRE and MPLS-over-UDP)
can be matched with ``RTE_FLOW_ITEM_TYPE_MPLS``.

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching MPLS flow is enabled by setting this value::

   FLEX_PARSER_PROFILE_ENABLE=1

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.

Limitations
^^^^^^^^^^^

#. MPLSoUDP with multiple MPLS headers is only supported in :ref:`HW steering <mlx5_hws>`.

#. MPLS matching with :ref:`HW steering <mlx5_hws>` is not supported on group 0.

#. The maximum supported MPLS headers is 5.

Example
^^^^^^^

See `matching MPLSoGRE with testpmd`_.


.. _mlx5_nsh:

NSH
~~~

Network Service Header (NSH) flows
can be matched with ``RTE_FLOW_ITEM_TYPE_NSH``.

Runtime configuration
^^^^^^^^^^^^^^^^^^^^^

The DV flow engine must be enabled (``dv_flow_en=1``).

The parameter :ref:`l3_vxlan_en <mlx5_vxlan_param>` must be set.

Limitations
^^^^^^^^^^^

#. NSH matching is supported only when NSH follows VXLAN-GPE.

#. NSH fields matching is not supported.


.. _mlx5_ipsec:

IPsec
~~~~~

Encapsulating Security Payload (ESP) is matched with ``RTE_FLOW_ITEM_TYPE_ESP``.

Limitations
^^^^^^^^^^^

#. Matching on SPI field in ESP header is supported over the PF only.

#. When using DV/Verbs flow engine (``dv_flow_en`` = 1/0 respectively),
   match on SPI field in ESP header for group 0 is supported from ConnectX-7.


.. _mlx5_ecpri:

eCPRI
~~~~~

Enhanced Common Public Radio Interface (eCPRI) flows
can be matched with ``RTE_FLOW_ITEM_TYPE_ECPRI``.

Firmware configuration
^^^^^^^^^^^^^^^^^^^^^^

Matching eCPRI flow is enabled by setting these values::

   FLEX_PARSER_PROFILE_ENABLE=4
   PROG_PARSE_GRAPH=1

Other protocols may require a different firmware configuration.
See :ref:`mlx5_firmware_config` for more details about the flex parser profile.


.. _mlx5_host_shaper:

Host Shaper
~~~~~~~~~~~

Host shaper register is per host port register
which sets a shaper on the host port.
All VF/host PF representors belonging to one host port share one host shaper.
For example, if representor 0 and representor 1 belong to the same host port,
and a host shaper rate of 1Gbps is configured,
the shaper throttles both representors traffic from the host.

Host shaper has two modes for setting the shaper:
immediate and deferred to available descriptor threshold event trigger.

In immediate mode, the rate limit is configured immediately to host shaper.

When deferring to the `available descriptor threshold <mlx5_rx_threshold>` trigger,
the shaper is not set until an available descriptor threshold event
is received by any Rx queue in a VF representor belonging to the host port.
The only rate supported for deferred mode is 100Mbps
(there is no limit on the supported rates for immediate mode).
In deferred mode, the shaper is set on the host port by the firmware
upon receiving the available descriptor threshold event,
which allows throttling host traffic on available descriptor threshold events
at minimum latency, preventing excess drops in the Rx queue.

Requirements
^^^^^^^^^^^^

=========  ===========
Minimum    Version
=========  ===========
hardware   BlueField-2
=========  ===========

Dependency on mstflint package
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In order to configure host shaper register,
``librte_net_mlx5`` depends on ``libmtcr_ul``
which can be installed from MLNX_OFED mstflint package.
Meson detects ``libmtcr_ul`` existence at configure stage.
If the library is detected, the application must link with ``-lmtcr_ul``,
as done by the pkg-config file libdpdk.pc.

Available descriptor threshold and host shaper
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Limitations
^^^^^^^^^^^

#. When configuring host shaper with ``RTE_PMD_MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED`` flag,
   only rates 0 and 100Mbps are supported.


Usage
-----

Start Example
~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with NVIDIA devices managed by mlx5.

#. Load the kernel modules::

      modprobe -a ib_uverbs mlx5_core mlx5_ib

   Alternatively if MLNX_OFED/MLNX_EN is fully installed, the following script
   can be run::

      /etc/init.d/openibd restart

   .. note::

      User space I/O kernel modules (UIO, VFIO) are not used and do
      not have to be loaded.

#. Make sure Ethernet interfaces are in working order and linked to kernel
   verbs. Related sysfs entries should be present::

      ls -d /sys/class/net/*/device/infiniband_verbs/uverbs* | cut -d / -f 5

   Example output::

      eth30
      eth31
      eth32
      eth33

#. Optionally, retrieve their PCI bus addresses to be used with the allow list::

      for intf in eth2 eth3 eth4 eth5; do
          basename $(readlink -e /sys/class/net/$intf/device);
      done | sed 's,^,-a ,'

   Example output::

      -a 0000:05:00.1
      -a 0000:06:00.0
      -a 0000:06:00.1
      -a 0000:05:00.0

#. Request huge pages::

      dpdk-hugepages.py --setup 2G

#. Start testpmd with basic parameters::

      dpdk-testpmd -l 8-15 -a 05:00.0 -a 05:00.1 -a 06:00.0 -a 06:00.1 -- --rxq=2 --txq=2 -i

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


.. _mlx5_vf_trusted:

How to Configure a VF as Trusted
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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


.. _mlx5_tx_trace:

How to Trace Tx Datapath
~~~~~~~~~~~~~~~~~~~~~~~~

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

   The optional parameter ``-a`` forces to dump incomplete bursts.

   The optional parameter ``-v [level]`` forces to dump raw records data
   for the specified level and below.
   Level 0 dumps bursts, level 1 dumps WQEs, level 2 dumps mbufs.

   .. code-block:: console

      mlx5_trace.py /var/log/rte-2023-01-23-AM-11-52-39

#. Interpreting the script output data

   All the timings are given in nanoseconds.
   The list of Tx bursts per port/queue is presented in the output.
   Each list element contains the list of built WQEs with specific opcodes.
   Each WQE contains the list of the encompassed packets to send.


.. _mlx5_flow_dump:

How to Dump Flows
~~~~~~~~~~~~~~~~~

This section demonstrates how to dump flows.
Currently, it's possible to dump all flows with assistance of external tools.

#. Two ways to get flow raw file:

   - Using testpmd CLI

     to dump all flows::

        testpmd> flow dump <port> all <output_file>

     and dump one flow::

        testpmd> flow dump <port> rule <rule_id> <output_file>

   - Call ``rte_flow_dev_dump`` function::

        rte_flow_dev_dump(port, flow, file, NULL);

#. Dump human-readable flows from raw file:

   Get flow parsing tool from: https://github.com/Mellanox/mlx_steering_dump

   .. code-block:: console

      mlx_steering_dump.py -f <output_file> -flowptr <flow_ptr>


Testpmd Driver-Specific Commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RSS
^^^

Compared to librte_net_mlx4 that implements a single RSS configuration per
port, librte_net_mlx5 supports per-protocol RSS configuration.

Since ``testpmd`` defaults to IP RSS mode and there is currently no
command-line parameter to enable additional protocols (UDP and TCP as well
as IP), the following commands must be entered from its CLI to get the same
behavior as librte_net_mlx4::

   > port stop all
   > port config all rss all
   > port start all

Port Attach with Socket Path
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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


Port Map External Rx Queue
^^^^^^^^^^^^^^^^^^^^^^^^^^

External Rx queue indexes mapping management.

Map HW queue index (32-bit) to ethdev queue index (16-bit) for external Rx queue::

   testpmd> mlx5 port (port_id) ext_rxq map (sw_queue_id) (hw_queue_id)

Unmap external Rx queue::

   testpmd> mlx5 port (port_id) ext_rxq unmap (sw_queue_id)

where:

* ``sw_queue_id``: queue index in range [64536, 65535].
  This range is the highest 1000 numbers.
* ``hw_queue_id``: queue index given by HW in queue creation.


Dump RQ/SQ/CQ HW Context for Debug Purposes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Dump RQ/CQ HW context for a given port/queue to a file::

   testpmd> mlx5 port (port_id) queue (queue_id) dump rq_context (file_name)

Dump SQ/CQ HW context for a given port/queue to a file::

   testpmd> mlx5 port (port_id) queue (queue_id) dump sq_context (file_name)


Set Flow Engine Mode
^^^^^^^^^^^^^^^^^^^^

Set the flow engine to active or standby mode with specific flags (bitmap style).
See ``RTE_PMD_MLX5_FLOW_ENGINE_FLAG_*`` for the flag definitions.

.. code-block:: console

   testpmd> mlx5 set flow_engine <active|standby> [<flags>]

This command is used for testing live migration,
and works for software steering only.
Default FDB jump should be disabled if switchdev is enabled.
The mode will propagate to all the probed ports.


GENEVE TLV Options Parser
^^^^^^^^^^^^^^^^^^^^^^^^^

See the :ref:`GENEVE parser API <mlx5_geneve_parser>` for more information.

Set
'''

Add single option to the global option list::

   testpmd> mlx5 set tlv_option class (class) type (type) len (length) \
            offset (sample_offset) sample_len (sample_len) \
            class_mode (ignore|fixed|matchable) data (0xffffffff|0x0 [0xffffffff|0x0]*)

where:

* ``class``: option class.
* ``type``: option type.
* ``length``: option data length in 4 bytes granularity.
* ``sample_offset``: offset to data list related to option data start.
  The offset is in 4 bytes granularity.
* ``sample_len``: length data list in 4 bytes granularity.
* ``ignore``: ignore ``class`` field.
* ``fixed``: option class is fixed and defines the option along with the type.
* ``matchable``: ``class`` field is matchable.
* ``data``: list of masks indicating which DW should be configure.
  The size of list should be equal to ``sample_len``.
* ``0xffffffff``: this DW should be configure.
* ``0x0``: this DW shouldn't be configure.

Flush
'''''

Remove several options from the global option list::

   testpmd> mlx5 flush tlv_options max (nb_option)

where:

* ``nb_option``: maximum number of option to remove from list. The order is LIFO.

List
''''

Print all options which are set in the global option list so far::

   testpmd> mlx5 list tlv_options

Output contains the values of each option, one per line.
There is no output at all when no options are configured on the global list::

   ID      Type    Class   Class_mode   Len     Offset  Sample_len   Data
   [...]   [...]   [...]   [...]        [...]   [...]   [...]        [...]

Setting several options and listing them::

   testpmd> mlx5 set tlv_option class 1 type 1 len 4 offset 1 sample_len 3
            class_mode fixed data 0xffffffff 0x0 0xffffffff
   testpmd: set new option in global list, now it has 1 options
   testpmd> mlx5 set tlv_option class 1 type 2 len 2 offset 0 sample_len 2
            class_mode fixed data 0xffffffff 0xffffffff
   testpmd: set new option in global list, now it has 2 options
   testpmd> mlx5 set tlv_option class 1 type 3 len 5 offset 4 sample_len 1
            class_mode fixed data 0xffffffff
   testpmd: set new option in global list, now it has 3 options
   testpmd> mlx5 list tlv_options
   ID      Type    Class   Class_mode   Len    Offset  Sample_len  Data
   0       1       1       fixed        4      1       3           0xffffffff 0x0 0xffffffff
   1       2       1       fixed        2      0       2           0xffffffff 0xffffffff
   2       3       1       fixed        5      4       1           0xffffffff
   testpmd>

Apply
'''''

Create GENEVE TLV parser for specific port using option list which are set so far::

   testpmd> mlx5 port (port_id) apply tlv_options

The same global option list can used by several ports.

Destroy
'''''''

Destroy GENEVE TLV parser for specific port::

   testpmd> mlx5 port (port_id) destroy tlv_options

This command doesn't destroy the global list,
For releasing options, ``flush`` command should be used.


.. links to examples on GitHub:

.. _affinity matching with testpmd: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/items/classification_and_integrity/port_affinity_match.yaml
.. _RSS IPv6 in indirect action: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/fate/rss_ip6_indirect.yaml
.. _RSS IPv6: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/fate/rss_ip6.yaml
.. _RSS: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/fate/rss_simple.yaml
.. _RSS in flow template: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/actions/fate/rss_basic.yaml
.. _RSS with custom hash: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/actions/fate/customized_rss_hash.yaml
.. _jump to table index with testpmd: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/actions/fate/jump_table_index_action.yaml
.. _indirect list of encap/decap: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/packet_reformat/indirect_raw_encap_decap.yaml
.. _quota usage with testpmd: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/actions/monitor_diagnostic/quota/quota.yaml
.. _meter mark with template API: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/actions/monitor_diagnostic/meter/meter_mark.yaml
.. _meter mark with sync API: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/monitor_diagnostic/meter/meter_mark_no_cfg.yaml
.. _random matching with testpmd: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/random/random.yaml
.. _set value: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/packet_reformat/modify_header_src_value.yaml
.. _copy field: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/actions/packet_reformat/modify_header_src_field.yaml
.. _compare ESP to value: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/compare/esp_seq_to_value.yaml
.. _compare ESP to meta: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/compare/esp_seq_to_meta.yaml
.. _compare meta to value: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/compare/meta_to_value.yaml
.. _compare tag to value: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/compare/tag_to_value.yaml
.. _compare random to value: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/compare/random_to_value.yaml
.. _packet type matching with testpmd: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/classification_and_integrity/ptype_l4_tcp.yaml
.. _match VXLAN-GBP: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/items/UDP_tunnels/vxlan/vxlan_gbp.yaml
.. _modify VXLAN header: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_template_async/actions/packet_reformat/vxlan_modify_last_rsvd.yaml
.. _matching MPLSoGRE with testpmd: https://github.com/Mellanox/dpdk-utest/blob/main/tests/flow_sync/items/MPLS/mpls_o_gre.yaml
