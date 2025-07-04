# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Packet capture test suite.

Test the DPDK packet capturing framework through the combined use of testpmd and dumpcap.
"""

from dataclasses import dataclass, field
from pathlib import Path, PurePath

from scapy.contrib.lldp import (
    LLDPDUChassisID,
    LLDPDUEndOfLLDPDU,
    LLDPDUPortID,
    LLDPDUSystemCapabilities,
    LLDPDUSystemDescription,
    LLDPDUSystemName,
    LLDPDUTimeToLive,
)
from scapy.layers.inet import IP, TCP, UDP
from scapy.layers.inet6 import IPv6
from scapy.layers.l2 import Dot1Q, Ether
from scapy.layers.sctp import SCTP
from scapy.packet import Packet, Raw, raw
from scapy.utils import rdpcap
from typing_extensions import Self

from framework.context import get_ctx
from framework.params import Params
from framework.remote_session.dpdk_shell import compute_eal_params
from framework.remote_session.interactive_shell import InteractiveShell
from framework.remote_session.testpmd_shell import TestPmdShell
from framework.settings import SETTINGS
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import requires
from framework.testbed_model.cpu import LogicalCoreList
from framework.testbed_model.os_session import FilePermissions
from framework.testbed_model.topology import TopologyType
from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
    PacketFilteringConfig,
)


@dataclass(kw_only=True)
class DumpcapParams(Params):
    """Parameters for the dpdk-dumpcap app.

    Attributes:
        lcore_list: The list of logical cores to use.
        file_prefix: The DPDK file prefix that the primary DPDK application is using.
        interface: The PCI address of the interface to capture.
        output_pcap_path: The path to the pcapng file to dump captured packets into.
        packet_filter: A packet filter in libpcap filter syntax.
    """

    lcore_list: LogicalCoreList | None = field(default=None, metadata=Params.long("lcore"))
    file_prefix: str | None = None
    interface: str = field(metadata=Params.short("i"))
    output_pcap_path: PurePath = field(metadata=Params.convert_value(str) | Params.short("w"))
    packet_filter: str | None = field(default=None, metadata=Params.short("f"))


class Dumpcap(InteractiveShell):
    """Class to spawn and manage a dpdk-dumpcap process.

    The dpdk-dumpcap is a DPDK app but instead of providing a regular DPDK EAL interface to the
    user, it replicates the Wireshark dumpcap app.
    """

    _app_params: DumpcapParams

    def __init__(self, params: DumpcapParams) -> None:
        """Extends :meth:`~.interactive_shell.InteractiveShell.__init__`."""
        self.ctx = get_ctx()
        eal_params = compute_eal_params()
        params.lcore_list = eal_params.lcore_list
        params.file_prefix = eal_params.prefix

        super().__init__(self.ctx.sut_node, name=None, privileged=True, app_params=params)

    @property
    def path(self) -> PurePath:
        """Path to the shell executable."""
        return PurePath(self.ctx.dpdk_build.remote_dpdk_build_dir).joinpath("app/dpdk-dumpcap")

    def wait_until_ready(self) -> Self:
        """Start app and wait until ready."""
        self.start_application(f"Capturing on '{self._app_params.interface}'")
        return self

    def close(self) -> None:
        """Close the application.

        Sends a SIGINT to close the application.
        """
        self.send_command("\x03")
        super().close()


@requires(topology_type=TopologyType.two_links)
class TestPacketCapture(TestSuite):
    """Packet Capture TestSuite.

    Attributes:
        packets: List of packets to send for testing pdump.
        rx_pcap_path: The remote path where to create the Rx packets pcap with pdump.
        tx_pcap_path: The remote path where to create the Tx packets pcap with pdump.
    """

    packets: list[Packet]
    rx_pcap_path: PurePath
    tx_pcap_path: PurePath

    def set_up_suite(self) -> None:
        """Test suite setup.

        Prepare the packets, file paths and queue range to be used in the test suite.
        """
        self.packets = [
            Ether() / IP() / Raw(b"\0" * 60),
            Ether() / IP() / TCP() / Raw(b"\0" * 60),
            Ether() / IP() / UDP() / Raw(b"\0" * 60),
            Ether() / IP() / SCTP() / Raw(b"\0" * 40),
            Ether() / IPv6() / TCP() / Raw(b"\0" * 60),
            Ether() / IPv6() / UDP() / Raw(b"\0" * 60),
            Ether() / IP() / IPv6() / SCTP() / Raw(b"\0" * 40),
            Ether() / Dot1Q() / IP() / UDP() / Raw(b"\0" * 40),
            Ether(dst="FF:FF:FF:FF:FF:FF", type=0x88F7) / Raw(b"\0" * 60),
            Ether(type=0x88CC)
            / LLDPDUChassisID(subtype=4, id=self.topology.tg_port_egress.mac_address)
            / LLDPDUPortID(subtype=5, id="Test Id")
            / LLDPDUTimeToLive(ttl=180)
            / LLDPDUSystemName(system_name="DTS Test sys")
            / LLDPDUSystemDescription(description="DTS Test Packet")
            / LLDPDUSystemCapabilities()
            / LLDPDUEndOfLLDPDU(),
        ]
        self.tx_pcap_path = self._ctx.sut_node.tmp_dir.joinpath("tx.pcapng")
        self.rx_pcap_path = self._ctx.sut_node.tmp_dir.joinpath("rx.pcapng")

    def _load_pcap_packets(self, remote_pcap_path: PurePath) -> list[Packet]:
        local_pcap_path = Path(SETTINGS.output_dir).joinpath(remote_pcap_path.name)
        self._ctx.sut_node.main_session.copy_from(remote_pcap_path, local_pcap_path)
        return list(rdpcap(str(local_pcap_path)))

    def _send_and_dump(
        self, packet_filter: str | None = None, rx_only: bool = False
    ) -> list[Packet]:
        dumpcap_rx = Dumpcap(
            DumpcapParams(
                interface=self.topology.sut_port_ingress.pci,
                output_pcap_path=self.rx_pcap_path,
                packet_filter=packet_filter,
            )
        ).wait_until_ready()
        if not rx_only:
            dumpcap_tx = Dumpcap(
                DumpcapParams(
                    interface=self.topology.sut_port_egress.pci,
                    output_pcap_path=self.tx_pcap_path,
                    packet_filter=packet_filter,
                )
            ).wait_until_ready()

        received_packets = self.send_packets_and_capture(
            self.packets, PacketFilteringConfig(no_lldp=False)
        )

        dumpcap_rx.close()
        self._ctx.sut_node.main_session.change_permissions(
            self.rx_pcap_path, FilePermissions(0o644)
        )
        if not rx_only:
            dumpcap_tx.close()
            self._ctx.sut_node.main_session.change_permissions(
                self.tx_pcap_path, FilePermissions(0o644)
            )

        return received_packets

    @func_test
    def test_dumpcap(self) -> None:
        """Test dumpcap on Rx and Tx interfaces.

        Steps:
            * Start up testpmd shell.
            * Start up dpdk-dumpcap with the default values.
            * Send packets.

        Verify:
            * The expected packets are the same as the Rx packets.
            * The Tx packets are the same as the packets received from Scapy.
        """
        with TestPmdShell() as testpmd:
            testpmd.start()
            received_packets = self._send_and_dump()

            expected_packets = self.get_expected_packets(self.packets, sent_from_tg=True)
            rx_pcap_packets = self._load_pcap_packets(self.rx_pcap_path)
            self.verify(
                self.match_all_packets(expected_packets, rx_pcap_packets, verify=False),
                "Rx packets from dumpcap weren't the same as the expected packets.",
            )

            tx_pcap_packets = self._load_pcap_packets(self.tx_pcap_path)
            self.verify(
                self.match_all_packets(tx_pcap_packets, received_packets, verify=False),
                "Tx packets from dumpcap weren't the same as the packets received by Scapy.",
            )

    @func_test
    def test_dumpcap_filter(self) -> None:
        """Test the dumpcap filtering feature.

        Steps:
            * Start up testpmd shell.
            * Start up dpdk-dumpcap listening for TCP packets on the Rx interface.
            * Send packets.

        Verify:
            * The dumped packets did not contain any of the packets meant for filtering.
        """
        with TestPmdShell() as testpmd:
            testpmd.start()
            self._send_and_dump("tcp", rx_only=True)
            filtered_packets = [
                raw(p)
                for p in self.get_expected_packets(self.packets, sent_from_tg=True)
                if not p.haslayer(TCP)
            ]

            rx_pcap_packets = [raw(p) for p in self._load_pcap_packets(self.rx_pcap_path)]
            for filtered_packet in filtered_packets:
                self.verify(
                    filtered_packet not in rx_pcap_packets,
                    "Found a packet in the pcap that was meant to be filtered out.",
                )
