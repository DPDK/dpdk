# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Module containing all classes needed to configure :class:`TestPmd`.

This module defines the :class:`TestPmdParams` class which is used to configure the
TestPmd shell. It also includes various data classes and enums that are used
to represent different configurations and settings.
"""

from dataclasses import dataclass, field
from enum import EnumMeta, Flag, auto, unique
from pathlib import PurePath
from typing import Literal, NamedTuple

from framework.params import (
    Params,
    Switch,
    YesNoSwitch,
    bracketed,
    comma_separated,
    hex_from_flag_value,
    modify_str,
    str_from_flag_value,
)
from framework.params.eal import EalParams
from framework.utils import StrEnum


class PortTopology(StrEnum):
    """Enum representing the port topology."""

    #: In paired mode, the forwarding is between pairs of ports, e.g.: (0,1), (2,3), (4,5).
    paired = auto()

    #: In chained mode, the forwarding is to the next available port in the port mask, e.g.:
    #: (0,1), (1,2), (2,0).
    #:
    #: The ordering of the ports can be changed using the portlist testpmd runtime function.
    chained = auto()

    #: In loop mode, ingress traffic is simply transmitted back on the same interface.
    loop = auto()


@modify_str(comma_separated, bracketed)
class PortNUMAConfig(NamedTuple):
    """DPDK port to NUMA socket association tuple."""

    #:
    port: int
    #:
    socket: int


@modify_str(str_from_flag_value)
@unique
class FlowDirection(Flag):
    """Flag indicating the direction of the flow.

    A bi-directional flow can be specified with the pipe:

    >>> TestPmdFlowDirection.RX | TestPmdFlowDirection.TX
    <TestPmdFlowDirection.TX|RX: 3>
    """

    #:
    RX = 1 << 0
    #:
    TX = 1 << 1


@modify_str(comma_separated, bracketed)
class RingNUMAConfig(NamedTuple):
    """Tuple associating DPDK port, direction of the flow and NUMA socket."""

    #:
    port: int
    #:
    direction: FlowDirection
    #:
    socket: int


@modify_str(comma_separated)
class EthPeer(NamedTuple):
    """Tuple associating a MAC address to the specified DPDK port."""

    #:
    port_no: int
    #:
    mac_address: str


@modify_str(comma_separated)
class TxIPAddrPair(NamedTuple):
    """Tuple specifying the source and destination IPs for the packets."""

    #:
    source_ip: str
    #:
    dest_ip: str


@modify_str(comma_separated)
class TxUDPPortPair(NamedTuple):
    """Tuple specifying the UDP source and destination ports for the packets.

    If leaving ``dest_port`` unspecified, ``source_port`` will be used for
    the destination port as well.
    """

    #:
    source_port: int
    #:
    dest_port: int | None = None


@dataclass
class DisableRSS(Params):
    """Disables RSS (Receive Side Scaling)."""

    _disable_rss: Literal[True] = field(
        default=True, init=False, metadata=Params.long("disable-rss")
    )


@dataclass
class SetRSSIPOnly(Params):
    """Sets RSS (Receive Side Scaling) functions for IPv4/IPv6 only."""

    _rss_ip: Literal[True] = field(default=True, init=False, metadata=Params.long("rss-ip"))


@dataclass
class SetRSSUDP(Params):
    """Sets RSS (Receive Side Scaling) functions for IPv4/IPv6 and UDP."""

    _rss_udp: Literal[True] = field(default=True, init=False, metadata=Params.long("rss-udp"))


class RSSSetting(EnumMeta):
    """Enum representing a RSS setting. Each property is a class that needs to be initialised."""

    #:
    Disabled = DisableRSS
    #:
    SetIPOnly = SetRSSIPOnly
    #:
    SetUDP = SetRSSUDP


class SimpleForwardingModes(StrEnum):
    r"""The supported packet forwarding modes for :class:`~TestPmd`\s."""

    #:
    io = auto()
    #:
    mac = auto()
    #:
    macswap = auto()
    #:
    rxonly = auto()
    #:
    csum = auto()
    #:
    icmpecho = auto()
    #:
    ieee1588 = auto()
    #:
    fivetswap = "5tswap"
    #:
    shared_rxq = "shared-rxq"
    #:
    recycle_mbufs = auto()


@dataclass(kw_only=True)
class TXOnlyForwardingMode(Params):
    """Sets a TX-Only forwarding mode.

    Attributes:
        multi_flow: Generates multiple flows if set to True.
        segments_length: Sets TX segment sizes or total packet length.
    """

    _forward_mode: Literal["txonly"] = field(
        default="txonly", init=False, metadata=Params.long("forward-mode")
    )
    multi_flow: Switch = field(default=None, metadata=Params.long("txonly-multi-flow"))
    segments_length: list[int] | None = field(
        default=None, metadata=Params.long("txpkts") | Params.convert_value(comma_separated)
    )


@dataclass(kw_only=True)
class FlowGenForwardingMode(Params):
    """Sets a flowgen forwarding mode.

    Attributes:
        clones: Set the number of each packet clones to be sent. Sending clones reduces host CPU
                load on creating packets and may help in testing extreme speeds or maxing out
                Tx packet performance. N should be not zero, but less than ‘burst’ parameter.
        flows: Set the number of flows to be generated, where 1 <= N <= INT32_MAX.
        segments_length: Set TX segment sizes or total packet length.
    """

    _forward_mode: Literal["flowgen"] = field(
        default="flowgen", init=False, metadata=Params.long("forward-mode")
    )
    clones: int | None = field(default=None, metadata=Params.long("flowgen-clones"))
    flows: int | None = field(default=None, metadata=Params.long("flowgen-flows"))
    segments_length: list[int] | None = field(
        default=None, metadata=Params.long("txpkts") | Params.convert_value(comma_separated)
    )


@dataclass(kw_only=True)
class NoisyForwardingMode(Params):
    """Sets a noisy forwarding mode.

    Attributes:
        forward_mode: Set the noisy VNF forwarding mode.
        tx_sw_buffer_size: Set the maximum number of elements of the FIFO queue to be created for
                           buffering packets.
        tx_sw_buffer_flushtime: Set the time before packets in the FIFO queue are flushed.
        lkup_memory: Set the size of the noisy neighbor simulation memory buffer in MB to N.
        lkup_num_reads: Set the size of the noisy neighbor simulation memory buffer in MB to N.
        lkup_num_writes: Set the number of writes to be done in noisy neighbor simulation
                         memory buffer to N.
        lkup_num_reads_writes: Set the number of r/w accesses to be done in noisy neighbor
                               simulation memory buffer to N.
    """

    _forward_mode: Literal["noisy"] = field(
        default="noisy", init=False, metadata=Params.long("forward-mode")
    )
    forward_mode: (
        Literal[
            SimpleForwardingModes.io,
            SimpleForwardingModes.mac,
            SimpleForwardingModes.macswap,
            SimpleForwardingModes.fivetswap,
        ]
        | None
    ) = field(default=SimpleForwardingModes.io, metadata=Params.long("noisy-forward-mode"))
    tx_sw_buffer_size: int | None = field(
        default=None, metadata=Params.long("noisy-tx-sw-buffer-size")
    )
    tx_sw_buffer_flushtime: int | None = field(
        default=None, metadata=Params.long("noisy-tx-sw-buffer-flushtime")
    )
    lkup_memory: int | None = field(default=None, metadata=Params.long("noisy-lkup-memory"))
    lkup_num_reads: int | None = field(default=None, metadata=Params.long("noisy-lkup-num-reads"))
    lkup_num_writes: int | None = field(default=None, metadata=Params.long("noisy-lkup-num-writes"))
    lkup_num_reads_writes: int | None = field(
        default=None, metadata=Params.long("noisy-lkup-num-reads-writes")
    )


@modify_str(hex_from_flag_value)
@unique
class HairpinMode(Flag):
    """Flag representing the hairpin mode."""

    #: Two hairpin ports loop.
    TWO_PORTS_LOOP = 1 << 0
    #: Two hairpin ports paired.
    TWO_PORTS_PAIRED = 1 << 1
    #: Explicit Tx flow rule.
    EXPLICIT_TX_FLOW = 1 << 4
    #: Force memory settings of hairpin RX queue.
    FORCE_RX_QUEUE_MEM_SETTINGS = 1 << 8
    #: Force memory settings of hairpin TX queue.
    FORCE_TX_QUEUE_MEM_SETTINGS = 1 << 9
    #: Hairpin RX queues will use locked device memory.
    RX_QUEUE_USE_LOCKED_DEVICE_MEMORY = 1 << 12
    #: Hairpin RX queues will use RTE memory.
    RX_QUEUE_USE_RTE_MEMORY = 1 << 13
    #: Hairpin TX queues will use locked device memory.
    TX_QUEUE_USE_LOCKED_DEVICE_MEMORY = 1 << 16
    #: Hairpin TX queues will use RTE memory.
    TX_QUEUE_USE_RTE_MEMORY = 1 << 18


@dataclass(kw_only=True)
class RXRingParams(Params):
    """Sets the RX ring parameters.

    Attributes:
        descriptors: Set the number of descriptors in the RX rings to N, where N > 0.
        prefetch_threshold: Set the prefetch threshold register of RX rings to N, where N >= 0.
        host_threshold: Set the host threshold register of RX rings to N, where N >= 0.
        write_back_threshold: Set the write-back threshold register of RX rings to N, where N >= 0.
        free_threshold: Set the free threshold of RX descriptors to N,
                        where 0 <= N < value of ``-–rxd``.
    """

    descriptors: int | None = field(default=None, metadata=Params.long("rxd"))
    prefetch_threshold: int | None = field(default=None, metadata=Params.long("rxpt"))
    host_threshold: int | None = field(default=None, metadata=Params.long("rxht"))
    write_back_threshold: int | None = field(default=None, metadata=Params.long("rxwt"))
    free_threshold: int | None = field(default=None, metadata=Params.long("rxfreet"))


@modify_str(hex_from_flag_value)
@unique
class RXMultiQueueMode(Flag):
    """Flag representing the RX multi-queue mode."""

    #:
    RSS = 1 << 0
    #:
    DCB = 1 << 1
    #:
    VMDQ = 1 << 2


@dataclass(kw_only=True)
class TXRingParams(Params):
    """Sets the TX ring parameters.

    Attributes:
        descriptors: Set the number of descriptors in the TX rings to N, where N > 0.
        rs_bit_threshold: Set the transmit RS bit threshold of TX rings to N,
                          where 0 <= N <= value of ``--txd``.
        prefetch_threshold: Set the prefetch threshold register of TX rings to N, where N >= 0.
        host_threshold: Set the host threshold register of TX rings to N, where N >= 0.
        write_back_threshold: Set the write-back threshold register of TX rings to N, where N >= 0.
        free_threshold: Set the transmit free threshold of TX rings to N,
                        where 0 <= N <= value of ``--txd``.
    """

    descriptors: int | None = field(default=None, metadata=Params.long("txd"))
    rs_bit_threshold: int | None = field(default=None, metadata=Params.long("txrst"))
    prefetch_threshold: int | None = field(default=None, metadata=Params.long("txpt"))
    host_threshold: int | None = field(default=None, metadata=Params.long("txht"))
    write_back_threshold: int | None = field(default=None, metadata=Params.long("txwt"))
    free_threshold: int | None = field(default=None, metadata=Params.long("txfreet"))


class Event(StrEnum):
    """Enum representing a testpmd event."""

    #:
    unknown = auto()
    #:
    queue_state = auto()
    #:
    vf_mbox = auto()
    #:
    macsec = auto()
    #:
    intr_lsc = auto()
    #:
    intr_rmv = auto()
    #:
    intr_reset = auto()
    #:
    dev_probed = auto()
    #:
    dev_released = auto()
    #:
    flow_aged = auto()
    #:
    err_recovering = auto()
    #:
    recovery_success = auto()
    #:
    recovery_failed = auto()
    #:
    all = auto()


class SimpleMempoolAllocationMode(StrEnum):
    """Enum representing simple mempool allocation modes."""

    #: Create and populate mempool using native DPDK memory.
    native = auto()
    #: Create and populate mempool using externally and anonymously allocated area.
    xmem = auto()
    #: Create and populate mempool using externally and anonymously allocated hugepage area.
    xmemhuge = auto()


@dataclass(kw_only=True)
class AnonMempoolAllocationMode(Params):
    """Create mempool using native DPDK memory, but populate using anonymous memory.

    Attributes:
        no_iova_contig: Enables to create mempool which is not IOVA contiguous.
    """

    _mp_alloc: Literal["anon"] = field(default="anon", init=False, metadata=Params.long("mp-alloc"))
    no_iova_contig: Switch = None


@dataclass(slots=True, kw_only=True)
class TestPmdParams(EalParams):
    """The testpmd shell parameters.

    Attributes:
        interactive_mode: Run testpmd in interactive mode.
        auto_start: Start forwarding on initialization.
        tx_first: Start forwarding, after sending a burst of packets first.
        stats_period: Display statistics every ``PERIOD`` seconds, if interactive mode is disabled.
                      The default value is 0, which means that the statistics will not be displayed.

                      .. note:: This flag should be used only in non-interactive mode.
        display_xstats: Display comma-separated list of extended statistics every ``PERIOD`` seconds
                        as specified in ``--stats-period`` or when used with interactive commands
                        that show Rx/Tx statistics (i.e. ‘show port stats’).
        nb_cores: Set the number of forwarding cores, where 1 <= N <= “number of cores” or
                  ``RTE_MAX_LCORE`` from the configuration file.
        coremask: Set the bitmask of the cores running the packet forwarding test. The main
                  lcore is reserved for command line parsing only and cannot be masked on for packet
                  forwarding.
        nb_ports: Set the number of forwarding ports, where 1 <= N <= “number of ports” on the board
                  or ``RTE_MAX_ETHPORTS`` from the configuration file. The default value is the
                  number of ports on the board.
        port_topology: Set port topology, where mode is paired (the default), chained or loop.
        portmask: Set the bitmask of the ports used by the packet forwarding test.
        portlist: Set the forwarding ports based on the user input used by the packet forwarding
                  test. ‘-‘ denotes a range of ports to set including the two specified port IDs ‘,’
                  separates multiple port values. Possible examples like –portlist=0,1 or
                  –portlist=0-2 or –portlist=0,1-2 etc.
        numa: Enable/disable NUMA-aware allocation of RX/TX rings and of RX memory buffers (mbufs).
        socket_num: Set the socket from which all memory is allocated in NUMA mode, where
                    0 <= N < number of sockets on the board.
        port_numa_config: Specify the socket on which the memory pool to be used by the port will be
                          allocated.
        ring_numa_config: Specify the socket on which the TX/RX rings for the port will be
                          allocated. Where flag is 1 for RX, 2 for TX, and 3 for RX and TX.
        total_num_mbufs: Set the number of mbufs to be allocated in the mbuf pools, where N > 1024.
        mbuf_size: Set the data size of the mbufs used to N bytes, where N < 65536.
                   If multiple mbuf-size values are specified the extra memory pools will be created
                   for allocating mbufs to receive packets with buffer splitting features.
        mbcache: Set the cache of mbuf memory pools to N, where 0 <= N <= 512.
        max_pkt_len: Set the maximum packet size to N bytes, where N >= 64.
        eth_peers_configfile: Use a configuration file containing the Ethernet addresses of
                              the peer ports.
        eth_peer: Set the MAC address XX:XX:XX:XX:XX:XX of the peer port N,
                  where 0 <= N < RTE_MAX_ETHPORTS.
        tx_ip: Set the source and destination IP address used when doing transmit only test.
               The defaults address values are source 198.18.0.1 and destination 198.18.0.2.
               These are special purpose addresses reserved for benchmarking (RFC 5735).
        tx_udp: Set the source and destination UDP port number for transmit test only test.
                The default port is the port 9 which is defined for the discard protocol (RFC 863).
        enable_lro: Enable large receive offload.
        max_lro_pkt_size: Set the maximum LRO aggregated packet size to N bytes, where N >= 64.
        disable_crc_strip: Disable hardware CRC stripping.
        enable_scatter: Enable scatter (multi-segment) RX.
        enable_hw_vlan: Enable hardware VLAN.
        enable_hw_vlan_filter: Enable hardware VLAN filter.
        enable_hw_vlan_strip: Enable hardware VLAN strip.
        enable_hw_vlan_extend: Enable hardware VLAN extend.
        enable_hw_qinq_strip: Enable hardware QINQ strip.
        pkt_drop_enabled: Enable per-queue packet drop for packets with no descriptors.
        rss: Receive Side Scaling setting.
        forward_mode: Set the forwarding mode.
        hairpin_mode: Set the hairpin port configuration.
        hairpin_queues: Set the number of hairpin queues per port to N, where 1 <= N <= 65535.
        burst: Set the number of packets per burst to N, where 1 <= N <= 512.
        enable_rx_cksum: Enable hardware RX checksum offload.
        rx_queues: Set the number of RX queues per port to N, where 1 <= N <= 65535.
        rx_ring: Set the RX rings parameters.
        no_flush_rx: Don’t flush the RX streams before starting forwarding. Used mainly with
                     the PCAP PMD.
        rx_segments_offsets: Set the offsets of packet segments on receiving
                             if split feature is engaged.
        rx_segments_length: Set the length of segments to scatter packets on receiving
                            if split feature is engaged.
        multi_rx_mempool: Enable multiple mbuf pools per Rx queue.
        rx_shared_queue: Create queues in shared Rx queue mode if device supports. Shared Rx queues
                         are grouped per X ports. X defaults to UINT32_MAX, implies all ports join
                         share group 1. Forwarding engine “shared-rxq” should be used for shared Rx
                         queues. This engine does Rx only and update stream statistics accordingly.
        rx_offloads: Set the bitmask of RX queue offloads.
        rx_mq_mode: Set the RX multi queue mode which can be enabled.
        tx_queues: Set the number of TX queues per port to N, where 1 <= N <= 65535.
        tx_ring: Set the TX rings params.
        tx_offloads: Set the hexadecimal bitmask of TX queue offloads.
        eth_link_speed: Set a forced link speed to the ethernet port. E.g. 1000 for 1Gbps.
        disable_link_check: Disable check on link status when starting/stopping ports.
        disable_device_start: Do not automatically start all ports. This allows testing
                              configuration of rx and tx queues before device is started
                              for the first time.
        no_lsc_interrupt: Disable LSC interrupts for all ports, even those supporting it.
        no_rmv_interrupt: Disable RMV interrupts for all ports, even those supporting it.
        bitrate_stats: Set the logical core N to perform bitrate calculation.
        latencystats: Set the logical core N to perform latency and jitter calculations.
        print_events: Enable printing the occurrence of the designated events.
                      Using :attr:`TestPmdEvent.ALL` will enable all of them.
        mask_events: Disable printing the occurrence of the designated events.
                     Using :attr:`TestPmdEvent.ALL` will disable all of them.
        flow_isolate_all: Providing this parameter requests flow API isolated mode on all ports at
                          initialization time. It ensures all traffic is received through the
                          configured flow rules only (see flow command). Ports that do not support
                          this mode are automatically discarded.
        disable_flow_flush: Disable port flow flush when stopping port.
                            This allows testing keep flow rules or shared flow objects across
                            restart.
        hot_plug: Enable device event monitor mechanism for hotplug.
        vxlan_gpe_port: Set the UDP port number of tunnel VXLAN-GPE to N.
        geneve_parsed_port: Set the UDP port number that is used for parsing the GENEVE protocol
                            to N. HW may be configured with another tunnel Geneve port.
        lock_all_memory: Enable/disable locking all memory. Disabled by default.
        mempool_allocation_mode: Set mempool allocation mode.
        record_core_cycles: Enable measurement of CPU cycles per packet.
        record_burst_status: Enable display of RX and TX burst stats.
    """

    interactive_mode: Switch = field(default=True, metadata=Params.short("i"))
    auto_start: Switch = field(default=None, metadata=Params.short("a"))
    tx_first: Switch = None
    stats_period: int | None = None
    display_xstats: list[str] | None = field(
        default=None, metadata=Params.convert_value(comma_separated)
    )
    nb_cores: int | None = None
    coremask: int | None = field(default=None, metadata=Params.convert_value(hex))
    nb_ports: int | None = None
    port_topology: PortTopology | None = PortTopology.paired
    portmask: int | None = field(default=None, metadata=Params.convert_value(hex))
    portlist: str | None = None  # TODO: can be ranges 0,1-3

    numa: YesNoSwitch = None
    socket_num: int | None = None
    port_numa_config: list[PortNUMAConfig] | None = field(
        default=None, metadata=Params.convert_value(comma_separated)
    )
    ring_numa_config: list[RingNUMAConfig] | None = field(
        default=None, metadata=Params.convert_value(comma_separated)
    )
    total_num_mbufs: int | None = None
    mbuf_size: list[int] | None = field(
        default=None, metadata=Params.convert_value(comma_separated)
    )
    mbcache: int | None = None
    max_pkt_len: int | None = None
    eth_peers_configfile: PurePath | None = None
    eth_peer: list[EthPeer] | None = field(default=None, metadata=Params.multiple())
    tx_ip: TxIPAddrPair | None = None
    tx_udp: TxUDPPortPair | None = None
    enable_lro: Switch = None
    max_lro_pkt_size: int | None = None
    disable_crc_strip: Switch = None
    enable_scatter: Switch = None
    enable_hw_vlan: Switch = None
    enable_hw_vlan_filter: Switch = None
    enable_hw_vlan_strip: Switch = None
    enable_hw_vlan_extend: Switch = None
    enable_hw_qinq_strip: Switch = None
    pkt_drop_enabled: Switch = field(default=None, metadata=Params.long("enable-drop-en"))
    rss: RSSSetting | None = None
    forward_mode: (
        SimpleForwardingModes
        | FlowGenForwardingMode
        | TXOnlyForwardingMode
        | NoisyForwardingMode
        | None
    ) = None
    hairpin_mode: HairpinMode | None = None
    hairpin_queues: int | None = field(default=None, metadata=Params.long("hairpinq"))
    burst: int | None = None
    enable_rx_cksum: Switch = None

    rx_queues: int | None = field(default=None, metadata=Params.long("rxq"))
    rx_ring: RXRingParams | None = None
    no_flush_rx: Switch = None
    rx_segments_offsets: list[int] | None = field(
        default=None, metadata=Params.long("rxoffs") | Params.convert_value(comma_separated)
    )
    rx_segments_length: list[int] | None = field(
        default=None, metadata=Params.long("rxpkts") | Params.convert_value(comma_separated)
    )
    multi_rx_mempool: Switch = None
    rx_shared_queue: Switch | int = field(default=None, metadata=Params.long("rxq-share"))
    rx_offloads: int | None = field(default=None, metadata=Params.convert_value(hex))
    rx_mq_mode: RXMultiQueueMode | None = None

    tx_queues: int | None = field(default=None, metadata=Params.long("txq"))
    tx_ring: TXRingParams | None = None
    tx_offloads: int | None = field(default=None, metadata=Params.convert_value(hex))

    eth_link_speed: int | None = None
    disable_link_check: Switch = None
    disable_device_start: Switch = None
    no_lsc_interrupt: Switch = None
    no_rmv_interrupt: Switch = None
    bitrate_stats: int | None = None
    latencystats: int | None = None
    print_events: list[Event] | None = field(
        default=None, metadata=Params.multiple() | Params.long("print-event")
    )
    mask_events: list[Event] | None = field(
        default_factory=lambda: [Event.intr_lsc],
        metadata=Params.multiple() | Params.long("mask-event"),
    )

    flow_isolate_all: Switch = None
    disable_flow_flush: Switch = None

    hot_plug: Switch = None
    vxlan_gpe_port: int | None = None
    geneve_parsed_port: int | None = None
    lock_all_memory: YesNoSwitch = field(default=None, metadata=Params.long("mlockall"))
    mempool_allocation_mode: SimpleMempoolAllocationMode | AnonMempoolAllocationMode | None = field(
        default=None, metadata=Params.long("mp-alloc")
    )
    record_core_cycles: Switch = None
    record_burst_status: Switch = None
