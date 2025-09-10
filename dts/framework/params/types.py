# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Module containing TypeDict-equivalents of Params classes for static typing and hinting.

TypedDicts can be used in conjunction with Unpack and kwargs for type hinting on function calls.

Example:
    .. code:: python

        def create_testpmd(**kwargs: Unpack[TestPmdParamsDict]):
            params = TestPmdParams(**kwargs)
"""

from pathlib import PurePath
from typing import TypedDict

from api.testpmd.config import (
    AnonMempoolAllocationMode,
    EthPeer,
    Event,
    FlowGenForwardingMode,
    HairpinMode,
    NoisyForwardingMode,
    Params,
    PortNUMAConfig,
    PortTopology,
    RingNUMAConfig,
    RSSSetting,
    RXMultiQueueMode,
    RXRingParams,
    SimpleForwardingModes,
    SimpleMempoolAllocationMode,
    TxIPAddrPair,
    TXOnlyForwardingMode,
    TXRingParams,
    TxUDPPortPair,
)
from framework.params import Switch, YesNoSwitch
from framework.testbed_model.cpu import LogicalCoreList
from framework.testbed_model.port import Port
from framework.testbed_model.virtual_device import VirtualDevice


class EalParamsDict(TypedDict, total=False):
    """:class:`TypedDict` equivalent of :class:`~.eal.EalParams`."""

    lcore_list: LogicalCoreList | None
    memory_channels: int | None
    prefix: str
    no_pci: Switch
    vdevs: list[VirtualDevice] | None
    allowed_ports: list[Port] | None
    blocked_ports: list[Port] | None
    other_eal_param: Params | None


class TestPmdParamsDict(EalParamsDict, total=False):
    """:class:`TypedDict` equivalent of :class:`~.testpmd.TestPmdParams`."""

    interactive_mode: Switch
    auto_start: Switch
    tx_first: Switch
    stats_period: int | None
    display_xstats: list[str] | None
    nb_cores: int | None
    coremask: int | None
    nb_ports: int | None
    port_topology: PortTopology | None
    portmask: int | None
    portlist: str | None
    numa: YesNoSwitch
    socket_num: int | None
    port_numa_config: list[PortNUMAConfig] | None
    ring_numa_config: list[RingNUMAConfig] | None
    total_num_mbufs: int | None
    mbuf_size: list[int] | None
    mbcache: int | None
    max_pkt_len: int | None
    eth_peers_configfile: PurePath | None
    eth_peer: list[EthPeer] | None
    tx_ip: TxIPAddrPair | None
    tx_udp: TxUDPPortPair | None
    enable_lro: Switch
    max_lro_pkt_size: int | None
    disable_crc_strip: Switch
    enable_scatter: Switch
    enable_hw_vlan: Switch
    enable_hw_vlan_filter: Switch
    enable_hw_vlan_strip: Switch
    enable_hw_vlan_extend: Switch
    enable_hw_qinq_strip: Switch
    pkt_drop_enabled: Switch
    rss: RSSSetting | None
    forward_mode: (
        SimpleForwardingModes
        | FlowGenForwardingMode
        | TXOnlyForwardingMode
        | NoisyForwardingMode
        | None
    )
    hairpin_mode: HairpinMode | None
    hairpin_queues: int | None
    burst: int | None
    enable_rx_cksum: Switch
    rx_queues: int | None
    rx_ring: RXRingParams | None
    no_flush_rx: Switch
    rx_segments_offsets: list[int] | None
    rx_segments_length: list[int] | None
    multi_rx_mempool: Switch
    rx_shared_queue: Switch | int
    rx_offloads: int | None
    rx_mq_mode: RXMultiQueueMode | None
    tx_queues: int | None
    tx_ring: TXRingParams | None
    tx_offloads: int | None
    eth_link_speed: int | None
    disable_link_check: Switch
    disable_device_start: Switch
    no_lsc_interrupt: Switch
    no_rmv_interrupt: Switch
    bitrate_stats: int | None
    latencystats: int | None
    print_events: list[Event] | None
    mask_events: list[Event] | None
    flow_isolate_all: Switch
    disable_flow_flush: Switch
    hot_plug: Switch
    vxlan_gpe_port: int | None
    geneve_parsed_port: int | None
    lock_all_memory: YesNoSwitch
    mempool_allocation_mode: SimpleMempoolAllocationMode | AnonMempoolAllocationMode | None
    record_core_cycles: Switch
    record_burst_status: Switch
