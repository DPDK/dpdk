.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.

SXE2 Poll Mode Driver
=====================

The sxe2 PMD (**librte_net_sxe2**) provides poll mode driver support
for 10/25/50/100 Gbps Network Adapters.
The embedded switch, Physical Functions (PF),
and SR-IOV Virtual Functions (VF) are supported.


Implementation details
----------------------

The sxe2 PMD is designed to operate alongside the sxe2 kernel network driver.
For management and control operations,
the PMD communicates with the kernel driver via ioctl interfaces.
These commands are processed by the kernel driver
and subsequently dispatched to the hardware firmware for execution.

For security and robustness, the driver's data path is optimized to operate
using virtual addresses (IOVA as VA mode).
However, to ensure full compatibility in system environments
where an IOMMU is absent or disabled,
the driver also provides an explicit path to support physical addressing
(IOVA as PA mode).

The hardware is capable of handling the corresponding IOVA addresses
(either VA or PA) directly, as provided by the DPDK memory subsystem.
This ensures that DPDK applications can only access memory segments
explicitly allocated to the current process,
preventing unauthorized access to random physical memory.

This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets
as long as they share the same MAC address.

Configuration
-------------

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

- ``Traffic Management Scheduling Levels``

  The DPDK Traffic Management (rte_tm) API can be used to configure the Tx scheduler on the NIC.
  The ``sched-layer-mode`` parameter can be used to set the number of scheduling levels
  in the transmit scheduling hierarchy.
  The provided value must be between 0 and 3.
  If the value provided is greater than the number of levels supported by the HW,
  the driver will use the hardware maximum value.

- ``flow-duplicate-pattern`` parameter [int]

  This parameter controls whether duplicate flow rules (rules with the same pattern items)
  are allowed on the switch engine flow type.
  Value ``0`` rejects duplicates; values ``1`` and ``2`` allow duplicates
  but differ in which rule takes effect when multiple identical rules exist.

  There are three options to choose:

  - 0. Prevent insertion of rules with the same pattern items.
    In this case, only the first rule is inserted,
    the following rules are rejected and error code EEXIST is returned.

  - 1. Allow insertion of rules with the same pattern items.
    In this case, all rules are inserted but only the first rule takes effect,
    the next rule takes effect only if the previous rules are deleted.

  - 2. Allow insertion of rules with the same pattern items.
    In this case, all rules are inserted but only the last rule takes effect,
    the previous rule takes effect only if the last rule is deleted.

  This option only applies to the switch engine flow type.
  For the Fnav flow engine type, duplicate rules are always rejected.

  When multiple PFs and VFs are bound to the driver,
  the same-priority pattern can be applied to different ports (PF or VF).
  This parameter controls whether duplicate rules across ports are allowed
  and which rule takes effect when multiple identical rules exist.

  When only a single PF or VF is present (one port),
  sending an identical pattern on the same port always returns error code EEXIST,
  regardless of the parameter value.

  By default, the PMD will set this value to 1
  (allow duplicates, first-added takes effect).

- ``fnav-stat-type`` parameter [int]

  This parameter controls the Fnav flow engine statistics type
  used for flow rule hit counting (via ``rte_flow_query``).

  - 1: Only count the number of packets.
  - 2: Only count the number of bytes.
  - 3: Count both packets and bytes (default).

  Default value is 3 (count both packets and bytes).

- ``drv-sw-stats`` parameter [int]

  This parameter controls whether per-packet software statistics (SW stats)
  are collected in the Rx data path.

  Hardware packet statistic counters may be inaccurate for certain packet types
  due to hardware design limitations.
  When accuracy of Rx packet classification statistics is critical,
  enabling this parameter allows the driver to accumulate statistics in software
  as packets are received, providing an alternative statistical path
  that bypasses hardware counter inaccuracies.

  - 0: Disable software statistics collection (default).
    The basic port statistics (``ipackets``, ``ibytes``) are reported
    from the hardware counters.
  - 1: Enable software statistics collection.
    Per-packet software statistics are accumulated for unicast,
    multicast, broadcast, and dropped packets in the Rx data path.

  When enabled, the following extended statistics (xstats) are available:
  ``rx_sw_unicast_packets``, ``rx_sw_multicast_packets``,
  ``rx_sw_broadcast_packets``, ``rx_sw_drop_packets``,
  and ``rx_sw_drop_bytes``.

- ``no-sched-mode`` parameter [int]

  This parameter enables non-scheduling mode (no-sched mode).
  When enabled, the transmit path bypasses the hardware scheduling module
  and packets are sent directly out through the port.
  This results in lower transmit latency and higher throughput,
  but Traffic Management (rte_tm) APIs are not supported in this mode.

  - 0: Disable non-scheduling mode (default).
    The transmit path goes through the hardware scheduling hierarchy.
    Traffic Management (rte_tm) API can be used to configure the Tx scheduler.
  - 1: Enable non-scheduling mode.
    The transmit path bypasses the hardware scheduling module.
    Packets are sent directly from the port at full speed without scheduling.
    Traffic Management (rte_tm) API is not available in this mode.

- ``rx-low-latency`` parameter [int]

  This parameter controls the interrupt throttling (ITR) interval
  for Rx queue interrupts.

  When enabled, the driver sets a shorter interrupt coalescing timeout
  (``SXE2_ITR_INTERVAL_LOW``, approximately 1 μs),
  reducing the time between packet arrival and interrupt delivery to the CPU.
  This lowers receive latency at the cost of increased CPU interrupt rate.

  When disabled (default), the driver uses the normal interrupt throttling interval
  (``SXE2_ITR_INTERVAL_NORMAL``, approximately 20 μs),
  which reduces the CPU interrupt rate at the expense of higher receive latency.

  - 0: Disable Rx low latency (default).
    Normal interrupt throttling interval (~20 μs) is used.
  - 1: Enable Rx low latency.
    Low interrupt throttling interval (~1 μs) is used
    for reduced receive latency.

- ``function-flow-direct`` parameter [int]

  This parameter controls whether flow rules from different functional units
  (DPDK vs kernel driver) are isolated or combined when both drivers
  control the same physical port.

  When the DPDK PMD and the kernel network driver coexist on the same port,
  flow rules may originate from either driver.
  This parameter determines how the source VSI (Virtual Switch Interface)
  of each flow rule is handled during hardware programming.

  - 0 (default): Isolate flow rules between DPDK and kernel.
    When ``flow_isolated`` is enabled (``rte_flow_isolate()`` called),
    kernel-side flow rules take priority and DPDK-side flow rules are suppressed.
    When ``flow_isolated`` is disabled, DPDK-side flow rules take priority
    and kernel-side flow rules are suppressed.
    Only one functional unit's flows are active at a time.

  - 1: Allow direct flow rules from both DPDK and kernel simultaneously.
    Both DPDK and kernel source VSIs are preserved in the hardware flow table.
    Flow rules from both sides are programmed without isolation.

  This option only applies to FNAV and ACL flow engine types
  and does not apply to PF bond devices.

Extended Statistics (xstats)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The PMD provides the following extended statistics (xstats)
for detailed monitoring of receive-side packet classification
and software-level accounting.
The software statistics path is provided as a workaround
for hardware counter inaccuracies on certain packet types ---
it accumulates per-packet statistics directly in the Rx data path,
ensuring that unicast, multicast, broadcast, and drop counts
reflect the actual packets processed by the driver.

Receive Software Statistics
  These counters are collected in the Rx data path
  when ``drv-sw-stats=1`` is configured (see the ``drv-sw-stats`` devarg above).
  When ``drv-sw-stats`` is disabled (default), these xstats report zero.

  - ``rx_sw_unicast_packets``: Number of unicast packets received.
  - ``rx_sw_multicast_packets``: Number of multicast packets received.
  - ``rx_sw_broadcast_packets``: Number of broadcast packets received.
  - ``rx_sw_drop_packets``: Number of packets dropped in the Rx data path.
  - ``rx_sw_drop_bytes``: Number of bytes dropped in the Rx data path.

  When ``drv-sw-stats`` is enabled, the basic counters
  ``ipackets`` and ``ibytes`` (from ``rte_eth_stats``)
  also reflect the software-accumulated packet and byte counts.
  Otherwise, they are reported from hardware counters.

Fnav Flow Engine Statistics
  The Fnav flow engine statistics type is controlled by
  the ``fnav-stat-type`` devarg (see above).
  Depending on the configuration:

  - ``fnav-stat-type=1``: Only packet count is available.
  - ``fnav-stat-type=2``: Only byte count is available.
  - ``fnav-stat-type=3`` (default): Both packet and byte counts are available.

  Flow query results (via ``rte_flow_query``) expose these per-flow counters
  through the query API, not via xstats.
