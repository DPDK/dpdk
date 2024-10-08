# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Testpmd interactive shell.

Typical usage example in a TestSuite::

    testpmd_shell = TestPmdShell(self.sut_node)
    devices = testpmd_shell.get_devices()
    for device in devices:
        print(device)
    testpmd_shell.close()
"""

import functools
import re
import time
from collections.abc import Callable, MutableSet
from dataclasses import dataclass, field
from enum import Flag, auto
from pathlib import PurePath
from typing import TYPE_CHECKING, Any, ClassVar, Concatenate, ParamSpec, TypeAlias

if TYPE_CHECKING:
    from enum import Enum as NoAliasEnum
else:
    from aenum import NoAliasEnum

from typing_extensions import Self, Unpack

from framework.exception import InteractiveCommandExecutionError, InternalError
from framework.params.testpmd import SimpleForwardingModes, TestPmdParams
from framework.params.types import TestPmdParamsDict
from framework.parser import ParserFn, TextParser
from framework.remote_session.dpdk_shell import DPDKShell
from framework.settings import SETTINGS
from framework.testbed_model.cpu import LogicalCoreCount, LogicalCoreList
from framework.testbed_model.sut_node import SutNode
from framework.utils import REGEX_FOR_MAC_ADDRESS, StrEnum

P = ParamSpec("P")
TestPmdShellMethod = Callable[Concatenate["TestPmdShell", P], Any]

TestPmdShellCapabilityMethod: TypeAlias = Callable[
    ["TestPmdShell", MutableSet["NicCapability"], MutableSet["NicCapability"]], None
]

TestPmdShellDecorator: TypeAlias = Callable[[TestPmdShellMethod], TestPmdShellMethod]

TestPmdShellNicCapability = (
    TestPmdShellCapabilityMethod | tuple[TestPmdShellCapabilityMethod, TestPmdShellDecorator]
)


class TestPmdDevice:
    """The data of a device that testpmd can recognize.

    Attributes:
        pci_address: The PCI address of the device.
    """

    pci_address: str

    def __init__(self, pci_address_line: str):
        """Initialize the device from the testpmd output line string.

        Args:
            pci_address_line: A line of testpmd output that contains a device.
        """
        self.pci_address = pci_address_line.strip().split(": ")[1].strip()

    def __str__(self) -> str:
        """The PCI address captures what the device is."""
        return self.pci_address


class VLANOffloadFlag(Flag):
    """Flag representing the VLAN offload settings of a NIC port."""

    #:
    STRIP = auto()
    #:
    FILTER = auto()
    #:
    EXTEND = auto()
    #:
    QINQ_STRIP = auto()

    @classmethod
    def from_str_dict(cls, d):
        """Makes an instance from a dict containing the flag member names with an "on" value.

        Args:
            d: A dictionary containing the flag members as keys and any string value.

        Returns:
            A new instance of the flag.
        """
        flag = cls(0)
        for name in cls.__members__:
            if d.get(name) == "on":
                flag |= cls[name]
        return flag

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find(
                r"VLAN offload:\s+"
                r"strip (?P<STRIP>on|off), "
                r"filter (?P<FILTER>on|off), "
                r"extend (?P<EXTEND>on|off), "
                r"qinq strip (?P<QINQ_STRIP>on|off)",
                re.MULTILINE,
                named=True,
            ),
            cls.from_str_dict,
        )


class RSSOffloadTypesFlag(Flag):
    """Flag representing the RSS offload flow types supported by the NIC port."""

    #:
    ipv4 = auto()
    #:
    ipv4_frag = auto()
    #:
    ipv4_tcp = auto()
    #:
    ipv4_udp = auto()
    #:
    ipv4_sctp = auto()
    #:
    ipv4_other = auto()
    #:
    ipv6 = auto()
    #:
    ipv6_frag = auto()
    #:
    ipv6_tcp = auto()
    #:
    ipv6_udp = auto()
    #:
    ipv6_sctp = auto()
    #:
    ipv6_other = auto()
    #:
    l2_payload = auto()
    #:
    ipv6_ex = auto()
    #:
    ipv6_tcp_ex = auto()
    #:
    ipv6_udp_ex = auto()
    #:
    port = auto()
    #:
    vxlan = auto()
    #:
    geneve = auto()
    #:
    nvgre = auto()
    #:
    user_defined_22 = auto()
    #:
    gtpu = auto()
    #:
    eth = auto()
    #:
    s_vlan = auto()
    #:
    c_vlan = auto()
    #:
    esp = auto()
    #:
    ah = auto()
    #:
    l2tpv3 = auto()
    #:
    pfcp = auto()
    #:
    pppoe = auto()
    #:
    ecpri = auto()
    #:
    mpls = auto()
    #:
    ipv4_chksum = auto()
    #:
    l4_chksum = auto()
    #:
    l2tpv2 = auto()
    #:
    ipv6_flow_label = auto()
    #:
    user_defined_38 = auto()
    #:
    user_defined_39 = auto()
    #:
    user_defined_40 = auto()
    #:
    user_defined_41 = auto()
    #:
    user_defined_42 = auto()
    #:
    user_defined_43 = auto()
    #:
    user_defined_44 = auto()
    #:
    user_defined_45 = auto()
    #:
    user_defined_46 = auto()
    #:
    user_defined_47 = auto()
    #:
    user_defined_48 = auto()
    #:
    user_defined_49 = auto()
    #:
    user_defined_50 = auto()
    #:
    user_defined_51 = auto()
    #:
    l3_pre96 = auto()
    #:
    l3_pre64 = auto()
    #:
    l3_pre56 = auto()
    #:
    l3_pre48 = auto()
    #:
    l3_pre40 = auto()
    #:
    l3_pre32 = auto()
    #:
    l2_dst_only = auto()
    #:
    l2_src_only = auto()
    #:
    l4_dst_only = auto()
    #:
    l4_src_only = auto()
    #:
    l3_dst_only = auto()
    #:
    l3_src_only = auto()

    #:
    ip = ipv4 | ipv4_frag | ipv4_other | ipv6 | ipv6_frag | ipv6_other | ipv6_ex
    #:
    udp = ipv4_udp | ipv6_udp | ipv6_udp_ex
    #:
    tcp = ipv4_tcp | ipv6_tcp | ipv6_tcp_ex
    #:
    sctp = ipv4_sctp | ipv6_sctp
    #:
    tunnel = vxlan | geneve | nvgre
    #:
    vlan = s_vlan | c_vlan
    #:
    all = (
        eth
        | vlan
        | ip
        | tcp
        | udp
        | sctp
        | l2_payload
        | l2tpv3
        | esp
        | ah
        | pfcp
        | gtpu
        | ecpri
        | mpls
        | l2tpv2
    )

    @classmethod
    def from_list_string(cls, names: str) -> Self:
        """Makes a flag from a whitespace-separated list of names.

        Args:
            names: a whitespace-separated list containing the members of this flag.

        Returns:
            An instance of this flag.
        """
        flag = cls(0)
        for name in names.split():
            flag |= cls.from_str(name)
        return flag

    @classmethod
    def from_str(cls, name: str) -> Self:
        """Makes a flag matching the supplied name.

        Args:
            name: a valid member of this flag in text
        Returns:
            An instance of this flag.
        """
        member_name = name.strip().replace("-", "_")
        return cls[member_name]

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find(r"Supported RSS offload flow types:((?:\r?\n?  \S+)+)", re.MULTILINE),
            RSSOffloadTypesFlag.from_list_string,
        )


class DeviceCapabilitiesFlag(Flag):
    """Flag representing the device capabilities."""

    #: Device supports Rx queue setup after device started.
    RUNTIME_RX_QUEUE_SETUP = auto()
    #: Device supports Tx queue setup after device started.
    RUNTIME_TX_QUEUE_SETUP = auto()
    #: Device supports shared Rx queue among ports within Rx domain and switch domain.
    RXQ_SHARE = auto()
    #: Device supports keeping flow rules across restart.
    FLOW_RULE_KEEP = auto()
    #: Device supports keeping shared flow objects across restart.
    FLOW_SHARED_OBJECT_KEEP = auto()

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find_int(r"Device capabilities: (0x[A-Fa-f\d]+)"),
            cls,
        )


class DeviceErrorHandlingMode(StrEnum):
    """Enum representing the device error handling mode."""

    #:
    none = auto()
    #:
    passive = auto()
    #:
    proactive = auto()
    #:
    unknown = auto()

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this enum from text.
        """
        return TextParser.wrap(TextParser.find(r"Device error handling mode: (\w+)"), cls)


def make_device_private_info_parser() -> ParserFn:
    """Device private information parser.

    Ensures that we are not parsing invalid device private info output.

    Returns:
        ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a parser
            function that parses the device private info from the TestPmd port info output.
    """

    def _validate(info: str):
        info = info.strip()
        if info == "none" or info.startswith("Invalid file") or info.startswith("Failed to dump"):
            return None
        return info

    return TextParser.wrap(TextParser.find(r"Device private info:\s+([\s\S]+)"), _validate)


class RxQueueState(StrEnum):
    """RX queue states.

    References:
        DPDK lib: ``lib/ethdev/rte_ethdev.h``
        testpmd display function: ``app/test-pmd/config.c:get_queue_state_name()``
    """

    #:
    stopped = auto()
    #:
    started = auto()
    #:
    hairpin = auto()
    #:
    unknown = auto()

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this enum from text.
        """
        return TextParser.wrap(TextParser.find(r"Rx queue state: ([^\r\n]+)"), cls)


@dataclass
class TestPmdRxqInfo(TextParser):
    """Representation of testpmd's ``show rxq info <port_id> <queue_id>`` command.

    References:
        testpmd command function: ``app/test-pmd/cmdline.c:cmd_showqueue()``
        testpmd display function: ``app/test-pmd/config.c:rx_queue_infos_display()``
    """

    #:
    port_id: int = field(metadata=TextParser.find_int(r"Infos for port (\d+)\b ?, RX queue \d+\b"))
    #:
    queue_id: int = field(metadata=TextParser.find_int(r"Infos for port \d+\b ?, RX queue (\d+)\b"))
    #: Mempool used by that queue
    mempool: str = field(metadata=TextParser.find(r"Mempool: ([^\r\n]+)"))
    #: Ring prefetch threshold
    rx_prefetch_threshold: int = field(
        metadata=TextParser.find_int(r"RX prefetch threshold: (\d+)\b")
    )
    #: Ring host threshold
    rx_host_threshold: int = field(metadata=TextParser.find_int(r"RX host threshold: (\d+)\b"))
    #: Ring writeback threshold
    rx_writeback_threshold: int = field(
        metadata=TextParser.find_int(r"RX writeback threshold: (\d+)\b")
    )
    #: Drives the freeing of Rx descriptors
    rx_free_threshold: int = field(metadata=TextParser.find_int(r"RX free threshold: (\d+)\b"))
    #: Drop packets if no descriptors are available
    rx_drop_packets: bool = field(metadata=TextParser.find(r"RX drop packets: on"))
    #: Do not start queue with rte_eth_dev_start()
    rx_deferred_start: bool = field(metadata=TextParser.find(r"RX deferred start: on"))
    #: Scattered packets Rx enabled
    rx_scattered_packets: bool = field(metadata=TextParser.find(r"RX scattered packets: on"))
    #: The state of the queue
    rx_queue_state: str = field(metadata=RxQueueState.make_parser())
    #: Configured number of RXDs
    number_of_rxds: int = field(metadata=TextParser.find_int(r"Number of RXDs: (\d+)\b"))
    #: Hardware receive buffer size
    rx_buffer_size: int | None = field(
        default=None, metadata=TextParser.find_int(r"RX buffer size: (\d+)\b")
    )
    #: Burst mode information
    burst_mode: str | None = field(
        default=None, metadata=TextParser.find(r"Burst mode: ([^\r\n]+)")
    )


@dataclass
class TestPmdPort(TextParser):
    """Dataclass representing the result of testpmd's ``show port info`` command."""

    #:
    id: int = field(metadata=TextParser.find_int(r"Infos for port (\d+)\b"))
    #:
    device_name: str = field(metadata=TextParser.find(r"Device name: ([^\r\n]+)"))
    #:
    driver_name: str = field(metadata=TextParser.find(r"Driver name: ([^\r\n]+)"))
    #:
    socket_id: int = field(metadata=TextParser.find_int(r"Connect to socket: (\d+)"))
    #:
    is_link_up: bool = field(metadata=TextParser.find("Link status: up"))
    #:
    link_speed: str = field(metadata=TextParser.find(r"Link speed: ([^\r\n]+)"))
    #:
    is_link_full_duplex: bool = field(metadata=TextParser.find("Link duplex: full-duplex"))
    #:
    is_link_autonegotiated: bool = field(metadata=TextParser.find("Autoneg status: On"))
    #:
    is_promiscuous_mode_enabled: bool = field(metadata=TextParser.find("Promiscuous mode: enabled"))
    #:
    is_allmulticast_mode_enabled: bool = field(
        metadata=TextParser.find("Allmulticast mode: enabled")
    )
    #: Maximum number of MAC addresses
    max_mac_addresses_num: int = field(
        metadata=TextParser.find_int(r"Maximum number of MAC addresses: (\d+)")
    )
    #: Maximum configurable length of RX packet
    max_hash_mac_addresses_num: int = field(
        metadata=TextParser.find_int(r"Maximum number of MAC addresses of hash filtering: (\d+)")
    )
    #: Minimum size of RX buffer
    min_rx_bufsize: int = field(metadata=TextParser.find_int(r"Minimum size of RX buffer: (\d+)"))
    #: Maximum configurable length of RX packet
    max_rx_packet_length: int = field(
        metadata=TextParser.find_int(r"Maximum configurable length of RX packet: (\d+)")
    )
    #: Maximum configurable size of LRO aggregated packet
    max_lro_packet_size: int = field(
        metadata=TextParser.find_int(r"Maximum configurable size of LRO aggregated packet: (\d+)")
    )

    #: Current number of RX queues
    rx_queues_num: int = field(metadata=TextParser.find_int(r"Current number of RX queues: (\d+)"))
    #: Max possible RX queues
    max_rx_queues_num: int = field(metadata=TextParser.find_int(r"Max possible RX queues: (\d+)"))
    #: Max possible number of RXDs per queue
    max_queue_rxd_num: int = field(
        metadata=TextParser.find_int(r"Max possible number of RXDs per queue: (\d+)")
    )
    #: Min possible number of RXDs per queue
    min_queue_rxd_num: int = field(
        metadata=TextParser.find_int(r"Min possible number of RXDs per queue: (\d+)")
    )
    #: RXDs number alignment
    rxd_alignment_num: int = field(metadata=TextParser.find_int(r"RXDs number alignment: (\d+)"))

    #: Current number of TX queues
    tx_queues_num: int = field(metadata=TextParser.find_int(r"Current number of TX queues: (\d+)"))
    #: Max possible TX queues
    max_tx_queues_num: int = field(metadata=TextParser.find_int(r"Max possible TX queues: (\d+)"))
    #: Max possible number of TXDs per queue
    max_queue_txd_num: int = field(
        metadata=TextParser.find_int(r"Max possible number of TXDs per queue: (\d+)")
    )
    #: Min possible number of TXDs per queue
    min_queue_txd_num: int = field(
        metadata=TextParser.find_int(r"Min possible number of TXDs per queue: (\d+)")
    )
    #: TXDs number alignment
    txd_alignment_num: int = field(metadata=TextParser.find_int(r"TXDs number alignment: (\d+)"))
    #: Max segment number per packet
    max_packet_segment_num: int = field(
        metadata=TextParser.find_int(r"Max segment number per packet: (\d+)")
    )
    #: Max segment number per MTU/TSO
    max_mtu_segment_num: int = field(
        metadata=TextParser.find_int(r"Max segment number per MTU\/TSO: (\d+)")
    )

    #:
    device_capabilities: DeviceCapabilitiesFlag = field(
        metadata=DeviceCapabilitiesFlag.make_parser(),
    )
    #:
    device_error_handling_mode: DeviceErrorHandlingMode | None = field(
        default=None, metadata=DeviceErrorHandlingMode.make_parser()
    )
    #:
    device_private_info: str | None = field(
        default=None,
        metadata=make_device_private_info_parser(),
    )

    #:
    hash_key_size: int | None = field(
        default=None, metadata=TextParser.find_int(r"Hash key size in bytes: (\d+)")
    )
    #:
    redirection_table_size: int | None = field(
        default=None, metadata=TextParser.find_int(r"Redirection table size: (\d+)")
    )
    #:
    supported_rss_offload_flow_types: RSSOffloadTypesFlag = field(
        default=RSSOffloadTypesFlag(0), metadata=RSSOffloadTypesFlag.make_parser()
    )

    #:
    mac_address: str | None = field(
        default=None, metadata=TextParser.find(r"MAC address: ([A-Fa-f0-9:]+)")
    )
    #:
    fw_version: str | None = field(
        default=None, metadata=TextParser.find(r"Firmware-version: ([^\r\n]+)")
    )
    #:
    dev_args: str | None = field(default=None, metadata=TextParser.find(r"Devargs: ([^\r\n]+)"))
    #: Socket id of the memory allocation
    mem_alloc_socket_id: int | None = field(
        default=None,
        metadata=TextParser.find_int(r"memory allocation on the socket: (\d+)"),
    )
    #:
    mtu: int | None = field(default=None, metadata=TextParser.find_int(r"MTU: (\d+)"))

    #:
    vlan_offload: VLANOffloadFlag | None = field(
        default=None,
        metadata=VLANOffloadFlag.make_parser(),
    )

    #: Maximum size of RX buffer
    max_rx_bufsize: int | None = field(
        default=None, metadata=TextParser.find_int(r"Maximum size of RX buffer: (\d+)")
    )
    #: Maximum number of VFs
    max_vfs_num: int | None = field(
        default=None, metadata=TextParser.find_int(r"Maximum number of VFs: (\d+)")
    )
    #: Maximum number of VMDq pools
    max_vmdq_pools_num: int | None = field(
        default=None, metadata=TextParser.find_int(r"Maximum number of VMDq pools: (\d+)")
    )

    #:
    switch_name: str | None = field(
        default=None, metadata=TextParser.find(r"Switch name: ([\r\n]+)")
    )
    #:
    switch_domain_id: int | None = field(
        default=None, metadata=TextParser.find_int(r"Switch domain Id: (\d+)")
    )
    #:
    switch_port_id: int | None = field(
        default=None, metadata=TextParser.find_int(r"Switch Port Id: (\d+)")
    )
    #:
    switch_rx_domain: int | None = field(
        default=None, metadata=TextParser.find_int(r"Switch Rx domain: (\d+)")
    )


@dataclass
class TestPmdPortStats(TextParser):
    """Port statistics."""

    #:
    port_id: int = field(metadata=TextParser.find_int(r"NIC statistics for port (\d+)"))

    #:
    rx_packets: int = field(metadata=TextParser.find_int(r"RX-packets:\s+(\d+)"))
    #:
    rx_missed: int = field(metadata=TextParser.find_int(r"RX-missed:\s+(\d+)"))
    #:
    rx_bytes: int = field(metadata=TextParser.find_int(r"RX-bytes:\s+(\d+)"))
    #:
    rx_errors: int = field(metadata=TextParser.find_int(r"RX-errors:\s+(\d+)"))
    #:
    rx_nombuf: int = field(metadata=TextParser.find_int(r"RX-nombuf:\s+(\d+)"))

    #:
    tx_packets: int = field(metadata=TextParser.find_int(r"TX-packets:\s+(\d+)"))
    #:
    tx_errors: int = field(metadata=TextParser.find_int(r"TX-errors:\s+(\d+)"))
    #:
    tx_bytes: int = field(metadata=TextParser.find_int(r"TX-bytes:\s+(\d+)"))

    #:
    rx_pps: int = field(metadata=TextParser.find_int(r"Rx-pps:\s+(\d+)"))
    #:
    rx_bps: int = field(metadata=TextParser.find_int(r"Rx-bps:\s+(\d+)"))

    #:
    tx_pps: int = field(metadata=TextParser.find_int(r"Tx-pps:\s+(\d+)"))
    #:
    tx_bps: int = field(metadata=TextParser.find_int(r"Tx-bps:\s+(\d+)"))


class PacketOffloadFlag(Flag):
    """Flag representing the Packet Offload Features Flags in DPDK.

    Values in this class are taken from the definitions in the RTE MBUF core library in DPDK
    located in ``lib/mbuf/rte_mbuf_core.h``. It is expected that flag values in this class will
    match the values they are set to in said DPDK library with one exception; all values must be
    unique. For example, the definitions for unknown checksum flags in ``rte_mbuf_core.h`` are all
    set to :data:`0`, but it is valuable to distinguish between them in this framework. For this
    reason flags that are not unique in the DPDK library are set either to values within the
    RTE_MBUF_F_FIRST_FREE-RTE_MBUF_F_LAST_FREE range for Rx or shifted 61+ bits for Tx.

    References:
        DPDK lib: ``lib/mbuf/rte_mbuf_core.h``
    """

    # RX flags

    #: The RX packet is a 802.1q VLAN packet, and the tci has been saved in mbuf->vlan_tci. If the
    #: flag RTE_MBUF_F_RX_VLAN_STRIPPED is also present, the VLAN header has been stripped from
    #: mbuf data, else it is still present.
    RTE_MBUF_F_RX_VLAN = auto()

    #: RX packet with RSS hash result.
    RTE_MBUF_F_RX_RSS_HASH = auto()

    #: RX packet with FDIR match indicate.
    RTE_MBUF_F_RX_FDIR = auto()

    #: This flag is set when the outermost IP header checksum is detected as wrong by the hardware.
    RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD = 1 << 5

    #: A vlan has been stripped by the hardware and its tci is saved in mbuf->vlan_tci. This can
    #: only happen if vlan stripping is enabled in the RX configuration of the PMD. When
    #: RTE_MBUF_F_RX_VLAN_STRIPPED is set, RTE_MBUF_F_RX_VLAN must also be set.
    RTE_MBUF_F_RX_VLAN_STRIPPED = auto()

    #: No information about the RX IP checksum. Value is 0 in the DPDK library.
    RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN = 1 << 23
    #: The IP checksum in the packet is wrong.
    RTE_MBUF_F_RX_IP_CKSUM_BAD = 1 << 4
    #: The IP checksum in the packet is valid.
    RTE_MBUF_F_RX_IP_CKSUM_GOOD = 1 << 7
    #: The IP checksum is not correct in the packet data, but the integrity of the IP header is
    #: verified. Value is RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD in the DPDK
    #: library.
    RTE_MBUF_F_RX_IP_CKSUM_NONE = 1 << 24

    #: No information about the RX L4 checksum. Value is 0 in the DPDK library.
    RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN = 1 << 25
    #: The L4 checksum in the packet is wrong.
    RTE_MBUF_F_RX_L4_CKSUM_BAD = 1 << 3
    #: The L4 checksum in the packet is valid.
    RTE_MBUF_F_RX_L4_CKSUM_GOOD = 1 << 8
    #: The L4 checksum is not correct in the packet data, but the integrity of the L4 data is
    #: verified. Value is RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD in the DPDK
    #: library.
    RTE_MBUF_F_RX_L4_CKSUM_NONE = 1 << 26

    #: RX IEEE1588 L2 Ethernet PT Packet.
    RTE_MBUF_F_RX_IEEE1588_PTP = 1 << 9
    #: RX IEEE1588 L2/L4 timestamped packet.
    RTE_MBUF_F_RX_IEEE1588_TMST = 1 << 10

    #: FD id reported if FDIR match.
    RTE_MBUF_F_RX_FDIR_ID = 1 << 13
    #: Flexible bytes reported if FDIR match.
    RTE_MBUF_F_RX_FDIR_FLX = 1 << 14

    #: If both RTE_MBUF_F_RX_QINQ_STRIPPED and RTE_MBUF_F_RX_VLAN_STRIPPED are set, the 2 VLANs
    #: have been stripped by the hardware. If RTE_MBUF_F_RX_QINQ_STRIPPED is set and
    #: RTE_MBUF_F_RX_VLAN_STRIPPED is unset, only the outer VLAN is removed from packet data.
    RTE_MBUF_F_RX_QINQ_STRIPPED = auto()

    #: When packets are coalesced by a hardware or virtual driver, this flag can be set in the RX
    #: mbuf, meaning that the m->tso_segsz field is valid and is set to the segment size of
    #: original packets.
    RTE_MBUF_F_RX_LRO = auto()

    #: Indicate that security offload processing was applied on the RX packet.
    RTE_MBUF_F_RX_SEC_OFFLOAD = 1 << 18
    #: Indicate that security offload processing failed on the RX packet.
    RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED = auto()

    #: The RX packet is a double VLAN. If this flag is set, RTE_MBUF_F_RX_VLAN must also be set. If
    #: the flag RTE_MBUF_F_RX_QINQ_STRIPPED is also present, both VLANs headers have been stripped
    #: from mbuf data, else they are still present.
    RTE_MBUF_F_RX_QINQ = auto()

    #: No info about the outer RX L4 checksum. Value is 0 in the DPDK library.
    RTE_MBUF_F_RX_OUTER_L4_CKSUM_UNKNOWN = 1 << 27
    #: The outer L4 checksum in the packet is wrong
    RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD = 1 << 21
    #: The outer L4 checksum in the packet is valid
    RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD = 1 << 22
    #: Invalid outer L4 checksum state. Value is
    #: RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD | RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD in the DPDK library.
    RTE_MBUF_F_RX_OUTER_L4_CKSUM_INVALID = 1 << 28

    # TX flags

    #: Outer UDP checksum offload flag. This flag is used for enabling outer UDP checksum in PMD.
    #: To use outer UDP checksum, the user either needs to enable the following in mbuf:
    #:
    #:  a) Fill outer_l2_len and outer_l3_len in mbuf.
    #:  b) Set the RTE_MBUF_F_TX_OUTER_UDP_CKSUM flag.
    #:  c) Set the RTE_MBUF_F_TX_OUTER_IPV4 or RTE_MBUF_F_TX_OUTER_IPV6 flag.
    #:
    #: Or configure RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM offload flag.
    RTE_MBUF_F_TX_OUTER_UDP_CKSUM = 1 << 41

    #: UDP Fragmentation Offload flag. This flag is used for enabling UDP fragmentation in SW or in
    #: HW.
    RTE_MBUF_F_TX_UDP_SEG = auto()

    #: Request security offload processing on the TX packet. To use Tx security offload, the user
    #: needs to fill l2_len in mbuf indicating L2 header size and where L3 header starts.
    #: Similarly, l3_len should also be filled along with ol_flags reflecting current L3 type.
    RTE_MBUF_F_TX_SEC_OFFLOAD = auto()

    #: Offload the MACsec. This flag must be set by the application to enable this offload feature
    #: for a packet to be transmitted.
    RTE_MBUF_F_TX_MACSEC = auto()

    # Bits 45:48 are used for the tunnel type in ``lib/mbuf/rte_mbuf_core.h``, but some are modified
    # in this Flag to maintain uniqueness. The tunnel type must be specified for TSO or checksum on
    # the inner part of tunnel packets. These flags can be used with RTE_MBUF_F_TX_TCP_SEG for TSO,
    # or RTE_MBUF_F_TX_xxx_CKSUM. The mbuf fields for inner and outer header lengths are required:
    # outer_l2_len, outer_l3_len, l2_len, l3_len, l4_len and tso_segsz for TSO.

    #:
    RTE_MBUF_F_TX_TUNNEL_VXLAN = 1 << 45
    #:
    RTE_MBUF_F_TX_TUNNEL_GRE = 1 << 46
    #: Value is 3 << 45 in the DPDK library.
    RTE_MBUF_F_TX_TUNNEL_IPIP = 1 << 61
    #:
    RTE_MBUF_F_TX_TUNNEL_GENEVE = 1 << 47
    #: TX packet with MPLS-in-UDP RFC 7510 header. Value is 5 << 45 in the DPDK library.
    RTE_MBUF_F_TX_TUNNEL_MPLSINUDP = 1 << 62
    #: Value is 6 << 45 in the DPDK library.
    RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE = 1 << 63
    #: Value is 7 << 45 in the DPDK library.
    RTE_MBUF_F_TX_TUNNEL_GTP = 1 << 64
    #:
    RTE_MBUF_F_TX_TUNNEL_ESP = 1 << 48
    #: Generic IP encapsulated tunnel type, used for TSO and checksum offload. This can be used for
    #: tunnels which are not standards or listed above. It is preferred to use specific tunnel
    #: flags like RTE_MBUF_F_TX_TUNNEL_GRE or RTE_MBUF_F_TX_TUNNEL_IPIP if possible. The ethdev
    #: must be configured with RTE_ETH_TX_OFFLOAD_IP_TNL_TSO.  Outer and inner checksums are done
    #: according to the existing flags like RTE_MBUF_F_TX_xxx_CKSUM. Specific tunnel headers that
    #: contain payload length, sequence id or checksum are not expected to be updated. Value is
    #: 0xD << 45 in the DPDK library.
    RTE_MBUF_F_TX_TUNNEL_IP = 1 << 65
    #: Generic UDP encapsulated tunnel type, used for TSO and checksum offload. UDP tunnel type
    #: implies outer IP layer. It can be used for tunnels which are not standards or listed above.
    #: It is preferred to use specific tunnel flags like RTE_MBUF_F_TX_TUNNEL_VXLAN if possible.
    #: The ethdev must be configured with RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO. Outer and inner checksums
    #: are done according to the existing flags like RTE_MBUF_F_TX_xxx_CKSUM. Specific tunnel
    #: headers that contain payload length, sequence id or checksum are not expected to be updated.
    #: value is 0xE << 45 in the DPDK library.
    RTE_MBUF_F_TX_TUNNEL_UDP = 1 << 66

    #: Double VLAN insertion (QinQ) request to driver, driver may offload the insertion based on
    #: device capability. Mbuf 'vlan_tci' & 'vlan_tci_outer' must be valid when this flag is set.
    RTE_MBUF_F_TX_QINQ = 1 << 49

    #: TCP segmentation offload. To enable this offload feature for a packet to be transmitted on
    #: hardware supporting TSO:
    #:
    #:  - set the RTE_MBUF_F_TX_TCP_SEG flag in mbuf->ol_flags (this flag implies
    #:      RTE_MBUF_F_TX_TCP_CKSUM)
    #:  - set the flag RTE_MBUF_F_TX_IPV4 or RTE_MBUF_F_TX_IPV6
    #:      * if it's IPv4, set the RTE_MBUF_F_TX_IP_CKSUM flag
    #:  - fill the mbuf offload information: l2_len, l3_len, l4_len, tso_segsz
    RTE_MBUF_F_TX_TCP_SEG = auto()

    #: TX IEEE1588 packet to timestamp.
    RTE_MBUF_F_TX_IEEE1588_TMST = auto()

    # Bits 52+53 used for L4 packet type with checksum enabled in ``lib/mbuf/rte_mbuf_core.h`` but
    # some values must be modified in this framework to maintain uniqueness. To use hardware
    # L4 checksum offload, the user needs to:
    #
    # - fill l2_len and l3_len in mbuf
    # - set the flags RTE_MBUF_F_TX_TCP_CKSUM, RTE_MBUF_F_TX_SCTP_CKSUM or
    #   RTE_MBUF_F_TX_UDP_CKSUM
    # - set the flag RTE_MBUF_F_TX_IPV4 or RTE_MBUF_F_TX_IPV6

    #: Disable L4 cksum of TX pkt. Value is 0 in the DPDK library.
    RTE_MBUF_F_TX_L4_NO_CKSUM = 1 << 67
    #: TCP cksum of TX pkt. Computed by NIC.
    RTE_MBUF_F_TX_TCP_CKSUM = 1 << 52
    #: SCTP cksum of TX pkt. Computed by NIC.
    RTE_MBUF_F_TX_SCTP_CKSUM = 1 << 53
    #: UDP cksum of TX pkt. Computed by NIC. Value is 3 << 52 in the DPDK library.
    RTE_MBUF_F_TX_UDP_CKSUM = 1 << 68

    #: Offload the IP checksum in the hardware. The flag RTE_MBUF_F_TX_IPV4 should also be set by
    #: the application, although a PMD will only check RTE_MBUF_F_TX_IP_CKSUM.
    RTE_MBUF_F_TX_IP_CKSUM = 1 << 54

    #: Packet is IPv4. This flag must be set when using any offload feature (TSO, L3 or L4
    #: checksum) to tell the NIC that the packet is an IPv4 packet. If the packet is a tunneled
    #: packet, this flag is related to the inner headers.
    RTE_MBUF_F_TX_IPV4 = auto()
    #: Packet is IPv6. This flag must be set when using an offload feature (TSO or L4 checksum) to
    #: tell the NIC that the packet is an IPv6 packet. If the packet is a tunneled packet, this
    #: flag is related to the inner headers.
    RTE_MBUF_F_TX_IPV6 = auto()
    #: VLAN tag insertion request to driver, driver may offload the insertion based on the device
    #: capability. mbuf 'vlan_tci' field must be valid when this flag is set.
    RTE_MBUF_F_TX_VLAN = auto()

    #: Offload the IP checksum of an external header in the hardware. The flag
    #: RTE_MBUF_F_TX_OUTER_IPV4 should also be set by the application, although a PMD will only
    #: check RTE_MBUF_F_TX_OUTER_IP_CKSUM.
    RTE_MBUF_F_TX_OUTER_IP_CKSUM = auto()
    #: Packet outer header is IPv4. This flag must be set when using any outer offload feature (L3
    #: or L4 checksum) to tell the NIC that the outer header of the tunneled packet is an IPv4
    #: packet.
    RTE_MBUF_F_TX_OUTER_IPV4 = auto()
    #: Packet outer header is IPv6. This flag must be set when using any outer offload feature (L4
    #: checksum) to tell the NIC that the outer header of the tunneled packet is an IPv6 packet.
    RTE_MBUF_F_TX_OUTER_IPV6 = auto()

    @classmethod
    def from_list_string(cls, names: str) -> Self:
        """Makes a flag from a whitespace-separated list of names.

        Args:
            names: a whitespace-separated list containing the members of this flag.

        Returns:
            An instance of this flag.
        """
        flag = cls(0)
        for name in names.split():
            flag |= cls.from_str(name)
        return flag

    @classmethod
    def from_str(cls, name: str) -> Self:
        """Makes a flag matching the supplied name.

        Args:
            name: a valid member of this flag in text
        Returns:
            An instance of this flag.
        """
        member_name = name.strip().replace("-", "_")
        return cls[member_name]

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find(r"ol_flags: ([^\n]+)"),
            cls.from_list_string,
        )


class RtePTypes(Flag):
    """Flag representing possible packet types in DPDK verbose output.

    Values in this class are derived from definitions in the RTE MBUF ptype library in DPDK located
    in ``lib/mbuf/rte_mbuf_ptype.h``. Specifically, the names of values in this class should match
    the possible return options from the functions ``rte_get_ptype_*_name`` in ``rte_mbuf_ptype.c``.

    References:
        DPDK lib: ``lib/mbuf/rte_mbuf_ptype.h``
        DPDK ptype name formatting functions: ``lib/mbuf/rte_mbuf_ptype.c:rte_get_ptype_*_name()``
    """

    # L2
    #: Ethernet packet type. This is used for outer packet for tunneling cases.
    L2_ETHER = auto()
    #: Ethernet packet type for time sync.
    L2_ETHER_TIMESYNC = auto()
    #: ARP (Address Resolution Protocol) packet type.
    L2_ETHER_ARP = auto()
    #: LLDP (Link Layer Discovery Protocol) packet type.
    L2_ETHER_LLDP = auto()
    #: NSH (Network Service Header) packet type.
    L2_ETHER_NSH = auto()
    #: VLAN packet type.
    L2_ETHER_VLAN = auto()
    #: QinQ packet type.
    L2_ETHER_QINQ = auto()
    #: PPPOE packet type.
    L2_ETHER_PPPOE = auto()
    #: FCoE packet type..
    L2_ETHER_FCOE = auto()
    #: MPLS packet type.
    L2_ETHER_MPLS = auto()
    #: No L2 packet information.
    L2_UNKNOWN = auto()

    # L3
    #: IP (Internet Protocol) version 4 packet type. This is used for outer packet for tunneling
    #: cases, and does not contain any header option.
    L3_IPV4 = auto()
    #: IP (Internet Protocol) version 4 packet type. This is used for outer packet for tunneling
    #: cases, and contains header options.
    L3_IPV4_EXT = auto()
    #: IP (Internet Protocol) version 6 packet type. This is used for outer packet for tunneling
    #: cases, and does not contain any extension header.
    L3_IPV6 = auto()
    #: IP (Internet Protocol) version 4 packet type. This is used for outer packet for tunneling
    #: cases, and may or maynot contain header options.
    L3_IPV4_EXT_UNKNOWN = auto()
    #: IP (Internet Protocol) version 6 packet type. This is used for outer packet for tunneling
    #: cases, and contains extension headers.
    L3_IPV6_EXT = auto()
    #: IP (Internet Protocol) version 6 packet type. This is used for outer packet for tunneling
    #: cases, and may or maynot contain extension headers.
    L3_IPV6_EXT_UNKNOWN = auto()
    #: No L3 packet information.
    L3_UNKNOWN = auto()

    # L4
    #: TCP (Transmission Control Protocol) packet type. This is used for outer packet for tunneling
    #: cases.
    L4_TCP = auto()
    #: UDP (User Datagram Protocol) packet type. This is used for outer packet for tunneling cases.
    L4_UDP = auto()
    #: Fragmented IP (Internet Protocol) packet type. This is used for outer packet for tunneling
    #: cases and refers to those packets of any IP types which can be recognized as fragmented. A
    #: fragmented packet cannot be recognized as any other L4 types (RTE_PTYPE_L4_TCP,
    #: RTE_PTYPE_L4_UDP, RTE_PTYPE_L4_SCTP, RTE_PTYPE_L4_ICMP, RTE_PTYPE_L4_NONFRAG).
    L4_FRAG = auto()
    #: SCTP (Stream Control Transmission Protocol) packet type. This is used for outer packet for
    #: tunneling cases.
    L4_SCTP = auto()
    #: ICMP (Internet Control Message Protocol) packet type. This is used for outer packet for
    #: tunneling cases.
    L4_ICMP = auto()
    #: Non-fragmented IP (Internet Protocol) packet type. This is used for outer packet for
    #: tunneling cases and refers to those packets of any IP types, that cannot be recognized as
    #: any of the above L4 types (RTE_PTYPE_L4_TCP, RTE_PTYPE_L4_UDP, RTE_PTYPE_L4_FRAG,
    #: RTE_PTYPE_L4_SCTP, RTE_PTYPE_L4_ICMP).
    L4_NONFRAG = auto()
    #: IGMP (Internet Group Management Protocol) packet type.
    L4_IGMP = auto()
    #: No L4 packet information.
    L4_UNKNOWN = auto()

    # Tunnel
    #: IP (Internet Protocol) in IP (Internet Protocol) tunneling packet type.
    TUNNEL_IP = auto()
    #: GRE (Generic Routing Encapsulation) tunneling packet type.
    TUNNEL_GRE = auto()
    #: VXLAN (Virtual eXtensible Local Area Network) tunneling packet type.
    TUNNEL_VXLAN = auto()
    #: NVGRE (Network Virtualization using Generic Routing Encapsulation) tunneling packet type.
    TUNNEL_NVGRE = auto()
    #: GENEVE (Generic Network Virtualization Encapsulation) tunneling packet type.
    TUNNEL_GENEVE = auto()
    #: Tunneling packet type of Teredo, VXLAN (Virtual eXtensible Local Area Network) or GRE
    #: (Generic Routing Encapsulation) could be recognized as this packet type, if they can not be
    #: recognized independently as of hardware capability.
    TUNNEL_GRENAT = auto()
    #: GTP-C (GPRS Tunnelling Protocol) control tunneling packet type.
    TUNNEL_GTPC = auto()
    #: GTP-U (GPRS Tunnelling Protocol) user data tunneling packet type.
    TUNNEL_GTPU = auto()
    #: ESP (IP Encapsulating Security Payload) tunneling packet type.
    TUNNEL_ESP = auto()
    #: L2TP (Layer 2 Tunneling Protocol) tunneling packet type.
    TUNNEL_L2TP = auto()
    #: VXLAN-GPE (VXLAN Generic Protocol Extension) tunneling packet type.
    TUNNEL_VXLAN_GPE = auto()
    #: MPLS-in-UDP tunneling packet type (RFC 7510).
    TUNNEL_MPLS_IN_UDP = auto()
    #: MPLS-in-GRE tunneling packet type (RFC 4023).
    TUNNEL_MPLS_IN_GRE = auto()
    #: No tunnel information found on the packet.
    TUNNEL_UNKNOWN = auto()

    # Inner L2
    #: Ethernet packet type. This is used for inner packet type only.
    INNER_L2_ETHER = auto()
    #: Ethernet packet type with VLAN (Virtual Local Area Network) tag.
    INNER_L2_ETHER_VLAN = auto()
    #: QinQ packet type.
    INNER_L2_ETHER_QINQ = auto()
    #: No inner L2 information found on the packet.
    INNER_L2_UNKNOWN = auto()

    # Inner L3
    #: IP (Internet Protocol) version 4 packet type. This is used for inner packet only, and does
    #: not contain any header option.
    INNER_L3_IPV4 = auto()
    #: IP (Internet Protocol) version 4 packet type. This is used for inner packet only, and
    #: contains header options.
    INNER_L3_IPV4_EXT = auto()
    #: IP (Internet Protocol) version 6 packet type. This is used for inner packet only, and does
    #: not contain any extension header.
    INNER_L3_IPV6 = auto()
    #: IP (Internet Protocol) version 4 packet type. This is used for inner packet only, and may or
    #: may not contain header options.
    INNER_L3_IPV4_EXT_UNKNOWN = auto()
    #: IP (Internet Protocol) version 6 packet type. This is used for inner packet only, and
    #: contains extension headers.
    INNER_L3_IPV6_EXT = auto()
    #: IP (Internet Protocol) version 6 packet type. This is used for inner packet only, and may or
    #: may not contain extension headers.
    INNER_L3_IPV6_EXT_UNKNOWN = auto()
    #: No inner L3 information found on the packet.
    INNER_L3_UNKNOWN = auto()

    # Inner L4
    #: TCP (Transmission Control Protocol) packet type. This is used for inner packet only.
    INNER_L4_TCP = auto()
    #: UDP (User Datagram Protocol) packet type. This is used for inner packet only.
    INNER_L4_UDP = auto()
    #: Fragmented IP (Internet Protocol) packet type. This is used for inner packet only, and may
    #: or maynot have a layer 4 packet.
    INNER_L4_FRAG = auto()
    #: SCTP (Stream Control Transmission Protocol) packet type. This is used for inner packet only.
    INNER_L4_SCTP = auto()
    #: ICMP (Internet Control Message Protocol) packet type. This is used for inner packet only.
    INNER_L4_ICMP = auto()
    #: Non-fragmented IP (Internet Protocol) packet type. It is used for inner packet only, and may
    #: or may not have other unknown layer 4 packet types.
    INNER_L4_NONFRAG = auto()
    #: No inner L4 information found on the packet.
    INNER_L4_UNKNOWN = auto()

    @classmethod
    def from_list_string(cls, names: str) -> Self:
        """Makes a flag from a whitespace-separated list of names.

        Args:
            names: a whitespace-separated list containing the members of this flag.

        Returns:
            An instance of this flag.
        """
        flag = cls(0)
        for name in names.split():
            flag |= cls.from_str(name)
        return flag

    @classmethod
    def from_str(cls, name: str) -> Self:
        """Makes a flag matching the supplied name.

        Args:
            name: a valid member of this flag in text
        Returns:
            An instance of this flag.
        """
        member_name = name.strip().replace("-", "_")
        return cls[member_name]

    @classmethod
    def make_parser(cls, hw: bool) -> ParserFn:
        """Makes a parser function.

        Args:
            hw: Whether to make a parser for hardware ptypes or software ptypes. If :data:`True`,
                hardware ptypes will be collected, otherwise software pytpes will.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find(f"{'hw' if hw else 'sw'} ptype: ([^-]+)"),
            cls.from_list_string,
        )


@dataclass
class TestPmdVerbosePacket(TextParser):
    """Packet information provided by verbose output in Testpmd.

    This dataclass expects that packet information be prepended with the starting line of packet
    bursts. Specifically, the line that reads "port X/queue Y: sent/received Z packets".
    """

    #: ID of the port that handled the packet.
    port_id: int = field(metadata=TextParser.find_int(r"port (\d+)/queue \d+"))
    #: ID of the queue that handled the packet.
    queue_id: int = field(metadata=TextParser.find_int(r"port \d+/queue (\d+)"))
    #: Whether the packet was received or sent by the queue/port.
    was_received: bool = field(metadata=TextParser.find(r"received \d+ packets"))
    #:
    src_mac: str = field(metadata=TextParser.find(f"src=({REGEX_FOR_MAC_ADDRESS})"))
    #:
    dst_mac: str = field(metadata=TextParser.find(f"dst=({REGEX_FOR_MAC_ADDRESS})"))
    #: Memory pool the packet was handled on.
    pool: str = field(metadata=TextParser.find(r"pool=(\S+)"))
    #: Packet type in hex.
    p_type: int = field(metadata=TextParser.find_int(r"type=(0x[a-fA-F\d]+)"))
    #:
    length: int = field(metadata=TextParser.find_int(r"length=(\d+)"))
    #: Number of segments in the packet.
    nb_segs: int = field(metadata=TextParser.find_int(r"nb_segs=(\d+)"))
    #: Hardware packet type.
    hw_ptype: RtePTypes = field(metadata=RtePTypes.make_parser(hw=True))
    #: Software packet type.
    sw_ptype: RtePTypes = field(metadata=RtePTypes.make_parser(hw=False))
    #:
    l2_len: int = field(metadata=TextParser.find_int(r"l2_len=(\d+)"))
    #:
    ol_flags: PacketOffloadFlag = field(metadata=PacketOffloadFlag.make_parser())
    #: RSS hash of the packet in hex.
    rss_hash: int | None = field(
        default=None, metadata=TextParser.find_int(r"RSS hash=(0x[a-fA-F\d]+)")
    )
    #: RSS queue that handled the packet in hex.
    rss_queue: int | None = field(
        default=None, metadata=TextParser.find_int(r"RSS queue=(0x[a-fA-F\d]+)")
    )
    #:
    l3_len: int | None = field(default=None, metadata=TextParser.find_int(r"l3_len=(\d+)"))
    #:
    l4_len: int | None = field(default=None, metadata=TextParser.find_int(r"l4_len=(\d+)"))


class RxOffloadCapability(Flag):
    """Rx offload capabilities of a device.

    The flags are taken from ``lib/ethdev/rte_ethdev.h``.
    They're prefixed with ``RTE_ETH_RX_OFFLOAD`` in ``lib/ethdev/rte_ethdev.h``
    instead of ``RX_OFFLOAD``, which is what testpmd changes the prefix to.
    The values are not contiguous, so the correspondence is preserved
    by specifying concrete values interspersed between auto() values.

    The ``RX_OFFLOAD`` prefix has been preserved so that the same flag names can be used
    in :class:`NicCapability`. The prefix is needed in :class:`NicCapability` since there's
    no other qualifier which would sufficiently distinguish it from other capabilities.

    References:
        DPDK lib: ``lib/ethdev/rte_ethdev.h``
        testpmd display function: ``app/test-pmd/cmdline.c:print_rx_offloads()``
    """

    #:
    RX_OFFLOAD_VLAN_STRIP = auto()
    #: Device supports L3 checksum offload.
    RX_OFFLOAD_IPV4_CKSUM = auto()
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_UDP_CKSUM = auto()
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_TCP_CKSUM = auto()
    #: Device supports Large Receive Offload.
    RX_OFFLOAD_TCP_LRO = auto()
    #: Device supports QinQ (queue in queue) offload.
    RX_OFFLOAD_QINQ_STRIP = auto()
    #: Device supports inner packet L3 checksum.
    RX_OFFLOAD_OUTER_IPV4_CKSUM = auto()
    #: Device supports MACsec.
    RX_OFFLOAD_MACSEC_STRIP = auto()
    #: Device supports filtering of a VLAN Tag identifier.
    RX_OFFLOAD_VLAN_FILTER = 1 << 9
    #: Device supports VLAN offload.
    RX_OFFLOAD_VLAN_EXTEND = auto()
    #: Device supports receiving segmented mbufs.
    RX_OFFLOAD_SCATTER = 1 << 13
    #: Device supports Timestamp.
    RX_OFFLOAD_TIMESTAMP = auto()
    #: Device supports crypto processing while packet is received in NIC.
    RX_OFFLOAD_SECURITY = auto()
    #: Device supports CRC stripping.
    RX_OFFLOAD_KEEP_CRC = auto()
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_SCTP_CKSUM = auto()
    #: Device supports inner packet L4 checksum.
    RX_OFFLOAD_OUTER_UDP_CKSUM = auto()
    #: Device supports RSS hashing.
    RX_OFFLOAD_RSS_HASH = auto()
    #: Device supports
    RX_OFFLOAD_BUFFER_SPLIT = auto()
    #: Device supports all checksum capabilities.
    RX_OFFLOAD_CHECKSUM = RX_OFFLOAD_IPV4_CKSUM | RX_OFFLOAD_UDP_CKSUM | RX_OFFLOAD_TCP_CKSUM
    #: Device supports all VLAN capabilities.
    RX_OFFLOAD_VLAN = (
        RX_OFFLOAD_VLAN_STRIP
        | RX_OFFLOAD_VLAN_FILTER
        | RX_OFFLOAD_VLAN_EXTEND
        | RX_OFFLOAD_QINQ_STRIP
    )

    @classmethod
    def from_string(cls, line: str) -> Self:
        """Make an instance from a string containing the flag names separated with a space.

        Args:
            line: The line to parse.

        Returns:
            A new instance containing all found flags.
        """
        flag = cls(0)
        for flag_name in line.split():
            flag |= cls[f"RX_OFFLOAD_{flag_name}"]
        return flag

    @classmethod
    def make_parser(cls, per_port: bool) -> ParserFn:
        """Make a parser function.

        Args:
            per_port: If :data:`True`, will return capabilities per port. If :data:`False`,
                will return capabilities per queue.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        granularity = "Port" if per_port else "Queue"
        return TextParser.wrap(
            TextParser.find(rf"Per {granularity}\s+:(.*)$", re.MULTILINE),
            cls.from_string,
        )


@dataclass
class RxOffloadCapabilities(TextParser):
    """The result of testpmd's ``show port <port_id> rx_offload capabilities`` command.

    References:
        testpmd command function: ``app/test-pmd/cmdline.c:cmd_rx_offload_get_capa()``
        testpmd display function: ``app/test-pmd/cmdline.c:cmd_rx_offload_get_capa_parsed()``
    """

    #:
    port_id: int = field(
        metadata=TextParser.find_int(r"Rx Offloading Capabilities of port (\d+) :")
    )
    #: Per-queue Rx offload capabilities.
    per_queue: RxOffloadCapability = field(metadata=RxOffloadCapability.make_parser(False))
    #: Capabilities other than per-queue Rx offload capabilities.
    per_port: RxOffloadCapability = field(metadata=RxOffloadCapability.make_parser(True))


def requires_stopped_ports(func: TestPmdShellMethod) -> TestPmdShellMethod:
    """Decorator for :class:`TestPmdShell` commands methods that require stopped ports.

    If the decorated method is called while the ports are started, then these are stopped before
    continuing.

    Args:
        func: The :class:`TestPmdShell` method to decorate.
    """

    @functools.wraps(func)
    def _wrapper(self: "TestPmdShell", *args: P.args, **kwargs: P.kwargs):
        if self.ports_started:
            self._logger.debug("Ports need to be stopped to continue.")
            self.stop_all_ports()

        return func(self, *args, **kwargs)

    return _wrapper


def requires_started_ports(func: TestPmdShellMethod) -> TestPmdShellMethod:
    """Decorator for :class:`TestPmdShell` commands methods that require started ports.

    If the decorated method is called while the ports are stopped, then these are started before
    continuing.

    Args:
        func: The :class:`TestPmdShell` method to decorate.
    """

    @functools.wraps(func)
    def _wrapper(self: "TestPmdShell", *args: P.args, **kwargs: P.kwargs):
        if not self.ports_started:
            self._logger.debug("Ports need to be started to continue.")
            self.start_all_ports()

        return func(self, *args, **kwargs)

    return _wrapper


def add_remove_mtu(mtu: int = 1500) -> Callable[[TestPmdShellMethod], TestPmdShellMethod]:
    """Configure MTU to `mtu` on all ports, run the decorated function, then revert.

    Args:
        mtu: The MTU to configure all ports on.

    Returns:
        The method decorated with setting and reverting MTU.
    """

    def decorator(func: TestPmdShellMethod) -> TestPmdShellMethod:
        @functools.wraps(func)
        def wrapper(self: "TestPmdShell", *args: P.args, **kwargs: P.kwargs):
            original_mtu = self.ports[0].mtu
            self.set_port_mtu_all(mtu=mtu, verify=False)
            retval = func(self, *args, **kwargs)
            self.set_port_mtu_all(original_mtu if original_mtu else 1500, verify=False)
            return retval

        return wrapper

    return decorator


class TestPmdShell(DPDKShell):
    """Testpmd interactive shell.

    The testpmd shell users should never use
    the :meth:`~.interactive_shell.InteractiveShell.send_command` method directly, but rather
    call specialized methods. If there isn't one that satisfies a need, it should be added.

    Attributes:
        ports_started: Indicates whether the ports are started.
    """

    _app_params: TestPmdParams
    _ports: list[TestPmdPort] | None

    #: The path to the testpmd executable.
    path: ClassVar[PurePath] = PurePath("app", "dpdk-testpmd")

    #: The testpmd's prompt.
    _default_prompt: ClassVar[str] = "testpmd>"

    #: This forces the prompt to appear after sending a command.
    _command_extra_chars: ClassVar[str] = "\n"

    ports_started: bool

    def __init__(
        self,
        node: SutNode,
        privileged: bool = True,
        timeout: float = SETTINGS.timeout,
        lcore_filter_specifier: LogicalCoreCount | LogicalCoreList = LogicalCoreCount(),
        ascending_cores: bool = True,
        append_prefix_timestamp: bool = True,
        name: str | None = None,
        **app_params: Unpack[TestPmdParamsDict],
    ) -> None:
        """Overrides :meth:`~.dpdk_shell.DPDKShell.__init__`. Changes app_params to kwargs."""
        super().__init__(
            node,
            privileged,
            timeout,
            lcore_filter_specifier,
            ascending_cores,
            append_prefix_timestamp,
            TestPmdParams(**app_params),
            name,
        )
        self.ports_started = not self._app_params.disable_device_start
        self._ports = None

    @property
    def ports(self) -> list[TestPmdPort]:
        """The ports of the instance.

        This caches the ports returned by :meth:`show_port_info_all`.
        To force an update of port information, execute :meth:`show_port_info_all` or
        :meth:`show_port_info`.

        Returns: The list of known testpmd ports.
        """
        if self._ports is None:
            return self.show_port_info_all()
        return self._ports

    @requires_started_ports
    def start(self, verify: bool = True) -> None:
        """Start packet forwarding with the current configuration.

        Args:
            verify: If :data:`True` , a second start command will be sent in an attempt to verify
                packet forwarding started as expected.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and forwarding fails to
                start or ports fail to come up.
        """
        self.send_command("start")
        if verify:
            # If forwarding was already started, sending "start" again should tell us
            start_cmd_output = self.send_command("start")
            if "Packet forwarding already started" not in start_cmd_output:
                self._logger.debug(f"Failed to start packet forwarding: \n{start_cmd_output}")
                raise InteractiveCommandExecutionError("Testpmd failed to start packet forwarding.")

            number_of_ports = len(self._app_params.ports or [])
            for port_id in range(number_of_ports):
                if not self.wait_link_status_up(port_id):
                    raise InteractiveCommandExecutionError(
                        "Not all ports came up after starting packet forwarding in testpmd."
                    )

    def stop(self, verify: bool = True) -> str:
        """Stop packet forwarding.

        Args:
            verify: If :data:`True` , the output of the stop command is scanned to verify that
                forwarding was stopped successfully or not started. If neither is found, it is
                considered an error.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the command to stop
                forwarding results in an error.

        Returns:
            Output gathered from the stop command and all other preceding logs in the buffer. This
            output is most often used to view forwarding statistics that are displayed when this
            command is sent as well as any verbose packet information that hasn't been consumed
            prior to calling this method.
        """
        stop_cmd_output = self.send_command("stop")
        if verify:
            if (
                "Done." not in stop_cmd_output
                and "Packet forwarding not started" not in stop_cmd_output
            ):
                self._logger.debug(f"Failed to stop packet forwarding: \n{stop_cmd_output}")
                raise InteractiveCommandExecutionError("Testpmd failed to stop packet forwarding.")
        return stop_cmd_output

    def get_devices(self) -> list[TestPmdDevice]:
        """Get a list of device names that are known to testpmd.

        Uses the device info listed in testpmd and then parses the output.

        Returns:
            A list of devices.
        """
        dev_info: str = self.send_command("show device info all")
        dev_list: list[TestPmdDevice] = []
        for line in dev_info.split("\n"):
            if "device name:" in line.lower():
                dev_list.append(TestPmdDevice(line))
        return dev_list

    def wait_link_status_up(self, port_id: int, timeout=SETTINGS.timeout) -> bool:
        """Wait until the link status on the given port is "up".

        Arguments:
            port_id: Port to check the link status on.
            timeout: Time to wait for the link to come up. The default value for this
                argument may be modified using the :option:`--timeout` command-line argument
                or the :envvar:`DTS_TIMEOUT` environment variable.

        Returns:
            Whether the link came up in time or not.
        """
        time_to_stop = time.time() + timeout
        port_info: str = ""
        while time.time() < time_to_stop:
            port_info = self.send_command(f"show port info {port_id}")
            if "Link status: up" in port_info:
                break
            time.sleep(0.5)
        else:
            self._logger.error(f"The link for port {port_id} did not come up in the given timeout.")
        return "Link status: up" in port_info

    def set_forward_mode(self, mode: SimpleForwardingModes, verify: bool = True):
        """Set packet forwarding mode.

        Args:
            mode: The forwarding mode to use.
            verify: If :data:`True` the output of the command will be scanned in an attempt to
                verify that the forwarding mode was set to `mode` properly.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the forwarding mode
                fails to update.
        """
        set_fwd_output = self.send_command(f"set fwd {mode.value}")
        if f"Set {mode.value} packet forwarding mode" not in set_fwd_output:
            self._logger.debug(f"Failed to set fwd mode to {mode.value}:\n{set_fwd_output}")
            raise InteractiveCommandExecutionError(
                f"Test pmd failed to set fwd mode to {mode.value}"
            )

    def stop_all_ports(self, verify: bool = True) -> None:
        """Stops all the ports.

        Args:
            verify: If :data:`True`, the output of the command will be checked for a successful
                execution.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the ports were not
                stopped successfully.
        """
        self._logger.debug("Stopping all the ports...")
        output = self.send_command("port stop all")
        if verify and not output.strip().endswith("Done"):
            raise InteractiveCommandExecutionError("Ports were not stopped successfully.")

        self.ports_started = False

    def start_all_ports(self, verify: bool = True) -> None:
        """Starts all the ports.

        Args:
            verify: If :data:`True`, the output of the command will be checked for a successful
                execution.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the ports were not
                started successfully.
        """
        self._logger.debug("Starting all the ports...")
        output = self.send_command("port start all")
        if verify and not output.strip().endswith("Done"):
            raise InteractiveCommandExecutionError("Ports were not started successfully.")

        self.ports_started = True

    @requires_stopped_ports
    def set_ports_queues(self, number_of: int) -> None:
        """Sets the number of queues per port.

        Args:
            number_of: The number of RX/TX queues to create per port.

        Raises:
            InternalError: If `number_of` is invalid.
        """
        if number_of < 1:
            raise InternalError("The number of queues must be positive and non-zero.")

        self.send_command(f"port config all rxq {number_of}")
        self.send_command(f"port config all txq {number_of}")

    def show_port_info_all(self) -> list[TestPmdPort]:
        """Returns the information of all the ports.

        Returns:
            list[TestPmdPort]: A list containing all the ports information as `TestPmdPort`.
        """
        output = self.send_command("show port info all")

        # Sample output of the "all" command looks like:
        #
        # <start>
        #
        #   ********************* Infos for port 0 *********************
        #   Key: value
        #
        #   ********************* Infos for port 1 *********************
        #   Key: value
        # <end>
        #
        # Takes advantage of the double new line in between ports as end delimiter. But we need to
        # artificially add a new line at the end to pick up the last port. Because commands are
        # executed on a pseudo-terminal created by paramiko on the remote node, lines end with CRLF.
        # Therefore we also need to take the carriage return into account.
        iter = re.finditer(r"\*{21}.*?[\r\n]{4}", output + "\r\n", re.S)
        self._ports = [TestPmdPort.parse(block.group(0)) for block in iter]
        return self._ports

    def show_port_info(self, port_id: int) -> TestPmdPort:
        """Returns the given port information.

        Args:
            port_id: The port ID to gather information for.

        Raises:
            InteractiveCommandExecutionError: If `port_id` is invalid.

        Returns:
            TestPmdPort: An instance of `TestPmdPort` containing the given port's information.
        """
        output = self.send_command(f"show port info {port_id}", skip_first_line=True)
        if output.startswith("Invalid port"):
            raise InteractiveCommandExecutionError("invalid port given")

        port = TestPmdPort.parse(output)
        self._update_port(port)
        return port

    def _update_port(self, port: TestPmdPort) -> None:
        if self._ports:
            self._ports = [
                existing_port if port.id != existing_port.id else port
                for existing_port in self._ports
            ]

    def show_port_stats_all(self) -> list[TestPmdPortStats]:
        """Returns the statistics of all the ports.

        Returns:
            list[TestPmdPortStats]: A list containing all the ports stats as `TestPmdPortStats`.
        """
        output = self.send_command("show port stats all")

        # Sample output of the "all" command looks like:
        #
        #   ########### NIC statistics for port 0 ###########
        #   values...
        #   #################################################
        #
        #   ########### NIC statistics for port 1 ###########
        #   values...
        #   #################################################
        #
        iter = re.finditer(r"(^  #*.+#*$[^#]+)^  #*\r$", output, re.MULTILINE)
        return [TestPmdPortStats.parse(block.group(1)) for block in iter]

    def show_port_stats(self, port_id: int) -> TestPmdPortStats:
        """Returns the given port statistics.

        Args:
            port_id: The port ID to gather information for.

        Raises:
            InteractiveCommandExecutionError: If `port_id` is invalid.

        Returns:
            TestPmdPortStats: An instance of `TestPmdPortStats` containing the given port's stats.
        """
        output = self.send_command(f"show port stats {port_id}", skip_first_line=True)
        if output.startswith("Invalid port"):
            raise InteractiveCommandExecutionError("invalid port given")

        return TestPmdPortStats.parse(output)

    @requires_stopped_ports
    def set_port_mtu(self, port_id: int, mtu: int, verify: bool = True) -> None:
        """Change the MTU of a port using testpmd.

        Some PMDs require that the port be stopped before changing the MTU, and it does no harm to
        stop the port before configuring in cases where it isn't required, so ports are stopped
        prior to changing their MTU.

        Args:
            port_id: ID of the port to adjust the MTU on.
            mtu: Desired value for the MTU to be set to.
            verify: If `verify` is :data:`True` then the output will be scanned in an attempt to
                verify that the mtu was properly set on the port. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the MTU was not
                properly updated on the port matching `port_id`.
        """
        set_mtu_output = self.send_command(f"port config mtu {port_id} {mtu}")
        if verify and (f"MTU: {mtu}" not in self.send_command(f"show port info {port_id}")):
            self._logger.debug(
                f"Failed to set mtu to {mtu} on port {port_id}." f" Output was:\n{set_mtu_output}"
            )
            raise InteractiveCommandExecutionError(
                f"Test pmd failed to update mtu of port {port_id} to {mtu}"
            )

    def set_port_mtu_all(self, mtu: int, verify: bool = True) -> None:
        """Change the MTU of all ports using testpmd.

        Runs :meth:`set_port_mtu` for every port that testpmd is aware of.

        Args:
            mtu: Desired value for the MTU to be set to.
            verify: Whether to verify that setting the MTU on each port was successful or not.
                Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the MTU was not
                properly updated on at least one port.
        """
        for port in self.ports:
            self.set_port_mtu(port.id, mtu, verify)

    @staticmethod
    def extract_verbose_output(output: str) -> list[TestPmdVerbosePacket]:
        """Extract the verbose information present in given testpmd output.

        This method extracts sections of verbose output that begin with the line
        "port X/queue Y: sent/received Z packets" and end with the ol_flags of a packet.

        Args:
            output: Testpmd output that contains verbose information

        Returns:
            List of parsed packet information gathered from verbose information in `output`.
        """
        out: list[TestPmdVerbosePacket] = []
        prev_header: str = ""
        iter = re.finditer(
            r"(?P<HEADER>(?:port \d+/queue \d+: (?:received|sent) \d+ packets)?)\s*"
            r"(?P<PACKET>src=[\w\s=:-]+?ol_flags: [\w ]+)",
            output,
        )
        for match in iter:
            if match.group("HEADER"):
                prev_header = match.group("HEADER")
            out.append(TestPmdVerbosePacket.parse(f"{prev_header}\n{match.group('PACKET')}"))
        return out

    def set_vlan_filter(self, port: int, enable: bool, verify: bool = True) -> None:
        """Set vlan filter on.

        Args:
            port: The port number to enable VLAN filter on.
            enable: Enable the filter on `port` if :data:`True`, otherwise disable it.
            verify: If :data:`True`, the output of the command and show port info
                is scanned to verify that vlan filtering was set successfully.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the filter
                fails to update.
        """
        filter_cmd_output = self.send_command(f"vlan set filter {'on' if enable else 'off'} {port}")
        if verify:
            vlan_settings = self.show_port_info(port_id=port).vlan_offload
            if enable ^ (vlan_settings is not None and VLANOffloadFlag.FILTER in vlan_settings):
                self._logger.debug(
                    f"""Failed to {'enable' if enable else 'disable'}
                                   filter on port {port}: \n{filter_cmd_output}"""
                )
                raise InteractiveCommandExecutionError(
                    f"""Failed to {'enable' if enable else 'disable'}
                    filter on port {port}"""
                )

    def rx_vlan(self, vlan: int, port: int, add: bool, verify: bool = True) -> None:
        """Add specified vlan tag to the filter list on a port. Requires vlan filter to be on.

        Args:
            vlan: The vlan tag to add, should be within 1-1005.
            port: The port number to add the tag on.
            add: Adds the tag if :data:`True`, otherwise removes the tag.
            verify: If :data:`True`, the output of the command is scanned to verify that
                the vlan tag was added to the filter list on the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the tag
                is not added.
        """
        rx_cmd_output = self.send_command(f"rx_vlan {'add' if add else 'rm'} {vlan} {port}")
        if verify:
            if (
                "VLAN-filtering disabled" in rx_cmd_output
                or "Invalid vlan_id" in rx_cmd_output
                or "Bad arguments" in rx_cmd_output
            ):
                self._logger.debug(
                    f"""Failed to {'add' if add else 'remove'} tag {vlan}
                    port {port}: \n{rx_cmd_output}"""
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to {'add' if add else 'remove'} tag {vlan} on port {port}."
                )

    def set_vlan_strip(self, port: int, enable: bool, verify: bool = True) -> None:
        """Enable or disable vlan stripping on the specified port.

        Args:
            port: The port number to use.
            enable: If :data:`True`, will turn vlan stripping on, otherwise will turn off.
            verify: If :data:`True`, the output of the command and show port info
                is scanned to verify that vlan stripping was enabled on the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and stripping
                fails to update.
        """
        strip_cmd_output = self.send_command(f"vlan set strip {'on' if enable else 'off'} {port}")
        if verify:
            vlan_settings = self.show_port_info(port_id=port).vlan_offload
            if enable ^ (vlan_settings is not None and VLANOffloadFlag.STRIP in vlan_settings):
                self._logger.debug(
                    f"""Failed to set strip {'on' if enable else 'off'}
                    port {port}: \n{strip_cmd_output}"""
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set strip {'on' if enable else 'off'} port {port}."
                )

    @requires_stopped_ports
    def tx_vlan_set(
        self, port: int, enable: bool, vlan: int | None = None, verify: bool = True
    ) -> None:
        """Set hardware insertion of vlan tags in packets sent on a port.

        Args:
            port: The port number to use.
            enable: Sets vlan tag insertion if :data:`True`, and resets if :data:`False`.
            vlan: The vlan tag to insert if enable is :data:`True`.
            verify: If :data:`True`, the output of the command is scanned to verify that
                vlan insertion was enabled on the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the insertion
                tag is not set.
        """
        if enable:
            tx_vlan_cmd_output = self.send_command(f"tx_vlan set {port} {vlan}")
            if verify:
                if (
                    "Please stop port" in tx_vlan_cmd_output
                    or "Invalid vlan_id" in tx_vlan_cmd_output
                    or "Invalid port" in tx_vlan_cmd_output
                ):
                    self._logger.debug(
                        f"Failed to set vlan tag {vlan} on port {port}:\n{tx_vlan_cmd_output}"
                    )
                    raise InteractiveCommandExecutionError(
                        f"Testpmd failed to set vlan insertion tag {vlan} on port {port}."
                    )
        else:
            tx_vlan_cmd_output = self.send_command(f"tx_vlan reset {port}")
            if verify:
                if "Please stop port" in tx_vlan_cmd_output or "Invalid port" in tx_vlan_cmd_output:
                    self._logger.debug(
                        f"Failed to reset vlan insertion on port {port}: \n{tx_vlan_cmd_output}"
                    )
                    raise InteractiveCommandExecutionError(
                        f"Testpmd failed to reset vlan insertion on port {port}."
                    )

    def set_promisc(self, port: int, enable: bool, verify: bool = True) -> None:
        """Enable or disable promiscuous mode for the specified port.

        Args:
            port: Port number to use.
            enable: If :data:`True`, turn promiscuous mode on, otherwise turn off.
            verify: If :data:`True` an additional command will be sent to verify that
                promiscuous mode is properly set. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and promiscuous mode
                is not correctly set.
        """
        promisc_cmd_output = self.send_command(f"set promisc {port} {'on' if enable else 'off'}")
        if verify:
            stats = self.show_port_info(port_id=port)
            if enable ^ stats.is_promiscuous_mode_enabled:
                self._logger.debug(
                    f"Failed to set promiscuous mode on port {port}: \n{promisc_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set promiscuous mode on port {port}."
                )

    def set_verbose(self, level: int, verify: bool = True) -> None:
        """Set debug verbosity level.

        Args:
            level: 0 - silent except for error
                1 - fully verbose except for Tx packets
                2 - fully verbose except for Rx packets
                >2 - fully verbose
            verify: If :data:`True` the command output will be scanned to verify that verbose level
                is properly set. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and verbose level
            is not correctly set.
        """
        verbose_cmd_output = self.send_command(f"set verbose {level}")
        if verify:
            if "Change verbose level" not in verbose_cmd_output:
                self._logger.debug(
                    f"Failed to set verbose level to {level}: \n{verbose_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set verbose level to {level}."
                )

    def _close(self) -> None:
        """Overrides :meth:`~.interactive_shell.close`."""
        self.stop()
        self.send_command("quit", "Bye...")
        return super()._close()

    """
    ====== Capability retrieval methods ======
    """

    def get_capabilities_rx_offload(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get all rx offload capabilities and divide them into supported and unsupported.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._logger.debug("Getting rx offload capabilities.")
        command = f"show port {self.ports[0].id} rx_offload capabilities"
        rx_offload_capabilities_out = self.send_command(command)
        rx_offload_capabilities = RxOffloadCapabilities.parse(rx_offload_capabilities_out)
        self._update_capabilities_from_flag(
            supported_capabilities,
            unsupported_capabilities,
            RxOffloadCapability,
            rx_offload_capabilities.per_port | rx_offload_capabilities.per_queue,
        )

    def _update_capabilities_from_flag(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
        flag_class: type[Flag],
        supported_flags: Flag,
    ) -> None:
        """Divide all flags from `flag_class` into supported and unsupported."""
        for flag in flag_class:
            if flag in supported_flags:
                supported_capabilities.add(NicCapability[str(flag.name)])
            else:
                unsupported_capabilities.add(NicCapability[str(flag.name)])

    @requires_started_ports
    def get_capabilities_rxq_info(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get all rxq capabilities and divide them into supported and unsupported.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._logger.debug("Getting rxq capabilities.")
        command = f"show rxq info {self.ports[0].id} 0"
        rxq_info = TestPmdRxqInfo.parse(self.send_command(command))
        if rxq_info.rx_scattered_packets:
            supported_capabilities.add(NicCapability.SCATTERED_RX_ENABLED)
        else:
            unsupported_capabilities.add(NicCapability.SCATTERED_RX_ENABLED)

    def get_capabilities_show_port_info(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get all capabilities from show port info and divide them into supported and unsupported.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._update_capabilities_from_flag(
            supported_capabilities,
            unsupported_capabilities,
            DeviceCapabilitiesFlag,
            self.ports[0].device_capabilities,
        )


class NicCapability(NoAliasEnum):
    """A mapping between capability names and the associated :class:`TestPmdShell` methods.

    The :class:`TestPmdShell` capability checking method executes the command that checks
    whether the capability is supported.
    A decorator may optionally be added to the method that will add and remove configuration
    that's necessary to retrieve the capability support status.
    The Enum members may be assigned the method itself or a tuple of
    (capability_checking_method, decorator_function).

    The signature of each :class:`TestPmdShell` capability checking method must be::

        fn(self, supported_capabilities: MutableSet, unsupported_capabilities: MutableSet) -> None

    The capability checking method must get whether a capability is supported or not
    from a testpmd command. If multiple capabilities can be obtained from a testpmd command,
    each should be obtained in the method. These capabilities should then
    be added to `supported_capabilities` or `unsupported_capabilities` based on their support.

    The two dictionaries are shared across all capability discovery function calls in a given
    test run so that we don't call the same function multiple times. For example, when we find
    :attr:`SCATTERED_RX_ENABLED` in :meth:`TestPmdShell.get_capabilities_rxq_info`,
    we don't go looking for it again if a different test case also needs it.
    """

    #: Scattered packets Rx enabled
    SCATTERED_RX_ENABLED: TestPmdShellNicCapability = (
        TestPmdShell.get_capabilities_rxq_info,
        add_remove_mtu(9000),
    )
    #:
    RX_OFFLOAD_VLAN_STRIP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports L3 checksum offload.
    RX_OFFLOAD_IPV4_CKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_UDP_CKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_TCP_CKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports Large Receive Offload.
    RX_OFFLOAD_TCP_LRO: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports QinQ (queue in queue) offload.
    RX_OFFLOAD_QINQ_STRIP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports inner packet L3 checksum.
    RX_OFFLOAD_OUTER_IPV4_CKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports MACsec.
    RX_OFFLOAD_MACSEC_STRIP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports filtering of a VLAN Tag identifier.
    RX_OFFLOAD_VLAN_FILTER: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports VLAN offload.
    RX_OFFLOAD_VLAN_EXTEND: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports receiving segmented mbufs.
    RX_OFFLOAD_SCATTER: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports Timestamp.
    RX_OFFLOAD_TIMESTAMP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports crypto processing while packet is received in NIC.
    RX_OFFLOAD_SECURITY: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports CRC stripping.
    RX_OFFLOAD_KEEP_CRC: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_SCTP_CKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports inner packet L4 checksum.
    RX_OFFLOAD_OUTER_UDP_CKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports RSS hashing.
    RX_OFFLOAD_RSS_HASH: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports scatter Rx packets to segmented mbufs.
    RX_OFFLOAD_BUFFER_SPLIT: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports all checksum capabilities.
    RX_OFFLOAD_CHECKSUM: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports all VLAN capabilities.
    RX_OFFLOAD_VLAN: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_rx_offload
    )
    #: Device supports Rx queue setup after device started.
    RUNTIME_RX_QUEUE_SETUP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_show_port_info
    )
    #: Device supports Tx queue setup after device started.
    RUNTIME_TX_QUEUE_SETUP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_show_port_info
    )
    #: Device supports shared Rx queue among ports within Rx domain and switch domain.
    RXQ_SHARE: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_show_port_info
    )
    #: Device supports keeping flow rules across restart.
    FLOW_RULE_KEEP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_show_port_info
    )
    #: Device supports keeping shared flow objects across restart.
    FLOW_SHARED_OBJECT_KEEP: TestPmdShellCapabilityMethod = functools.partial(
        TestPmdShell.get_capabilities_show_port_info
    )

    def __call__(
        self,
        testpmd_shell: TestPmdShell,
        supported_capabilities: MutableSet[Self],
        unsupported_capabilities: MutableSet[Self],
    ) -> None:
        """Execute the associated capability retrieval function.

        Args:
            testpmd_shell: :class:`TestPmdShell` object to which the function will be bound to.
            supported_capabilities: The dictionary storing the supported capabilities
                of a given test run.
            unsupported_capabilities: The dictionary storing the unsupported capabilities
                of a given test run.
        """
        self.value(testpmd_shell, supported_capabilities, unsupported_capabilities)
