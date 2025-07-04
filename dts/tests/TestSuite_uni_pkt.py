# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""Unified packet type flag testing suite.

According to DPDK documentation, each Poll Mode Driver should reserve 32 bits
of packet headers for unified packet type flags. These flags serve as an
identifier for user applications, and are divided into subcategories:
L2, L3, L4, tunnel, inner L2, inner L3, and inner L4 types.
This suite verifies the ability of the driver to recognize these types.

"""

from scapy.contrib.nsh import NSH
from scapy.layers.inet import GRE, ICMP, IP, TCP, UDP
from scapy.layers.inet6 import IPv6, IPv6ExtHdrFragment
from scapy.layers.l2 import ARP, Ether
from scapy.layers.sctp import SCTP, SCTPChunkData
from scapy.layers.vxlan import VXLAN
from scapy.packet import Packet, Raw

from framework.remote_session.testpmd_shell import (
    RtePTypes,
    SimpleForwardingModes,
    TestPmdShell,
    TestPmdVerbosePacket,
)
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import TopologyType, requires


@requires(topology_type=TopologyType.two_links)
class TestUniPkt(TestSuite):
    """DPDK Unified packet test suite.

    This testing suite uses testpmd's verbose output hardware/software
    packet type field to verify the ability of the driver to recognize
    unified packet types when receiving different packets.

    """

    def check_for_matching_packet(
        self, output: list[TestPmdVerbosePacket], flags: RtePTypes
    ) -> bool:
        """Returns :data:`True` if the packet in verbose output contains all specified flags."""
        for packet in output:
            if packet.dst_mac == "00:00:00:00:00:01":
                if flags not in packet.hw_ptype and flags not in packet.sw_ptype:
                    return False
        return True

    def send_packet_and_verify_flags(
        self, expected_flag: RtePTypes, packet: Packet, testpmd: TestPmdShell
    ) -> None:
        """Sends a packet to the DUT and verifies the verbose ptype flags."""
        self.send_packet_and_capture(packet=packet)
        verbose_output = testpmd.extract_verbose_output(testpmd.stop())
        valid = self.check_for_matching_packet(output=verbose_output, flags=expected_flag)
        self.verify(valid, f"Packet type flag did not match the expected flag: {expected_flag}.")

    def setup_session(
        self, testpmd: TestPmdShell, expected_flags: list[RtePTypes], packet_list=list[Packet]
    ) -> None:
        """Sets the forwarding and verbose mode of each test case interactive shell session."""
        testpmd.set_forward_mode(SimpleForwardingModes.rxonly)
        testpmd.set_verbose(level=1)
        for i in range(0, len(packet_list)):
            self.send_packet_and_verify_flags(
                expected_flag=expected_flags[i], packet=packet_list[i], testpmd=testpmd
            )

    @func_test
    def test_l2_packet_detect(self) -> None:
        """Ensure the correct flags are shown in verbose output when sending L2 packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [Ether(dst=mac_id, type=0x88F7) / Raw(), Ether(dst=mac_id) / ARP() / Raw()]
        flag_list = [RtePTypes.L2_ETHER_TIMESYNC, RtePTypes.L2_ETHER_ARP]
        with TestPmdShell() as testpmd:
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)

    @func_test
    def test_l3_l4_packet_detect(self) -> None:
        """Ensure correct flags are shown in the verbose output when sending IP/L4 packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / Raw(),
            Ether(dst=mac_id) / IP() / UDP() / Raw(),
            Ether(dst=mac_id) / IP() / TCP() / Raw(),
            Ether(dst=mac_id) / IP() / SCTP() / Raw(),
            Ether(dst=mac_id) / IP() / ICMP() / Raw(),
            Ether(dst=mac_id) / IP(frag=5) / TCP() / Raw(),
        ]
        flag_list = [
            RtePTypes.L3_IPV4 | RtePTypes.L2_ETHER,
            RtePTypes.L4_UDP,
            RtePTypes.L4_TCP,
            RtePTypes.L4_SCTP,
            RtePTypes.L4_ICMP,
            RtePTypes.L4_FRAG | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L2_ETHER,
        ]
        with TestPmdShell() as testpmd:
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)

    @func_test
    def test_ipv6_l4_packet_detect(self) -> None:
        """Ensure correct flags are shown in the verbose output when sending IPv6/L4 packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IPv6() / Raw(),
            Ether(dst=mac_id) / IPv6() / UDP() / Raw(),
            Ether(dst=mac_id) / IPv6() / TCP() / Raw(),
            Ether(dst=mac_id) / IPv6() / IPv6ExtHdrFragment() / Raw(),
        ]
        flag_list = [
            RtePTypes.L2_ETHER | RtePTypes.L3_IPV6,
            RtePTypes.L4_UDP,
            RtePTypes.L4_TCP,
            RtePTypes.L3_IPV6_EXT_UNKNOWN,
        ]
        with TestPmdShell() as testpmd:
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)

    @func_test
    def test_l3_tunnel_packet_detect(self) -> None:
        """Ensure correct flags are shown in the verbose output when sending IPv6/L4 packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / IP(frag=5) / UDP() / Raw(),
            Ether(dst=mac_id) / IP() / IP() / Raw(),
            Ether(dst=mac_id) / IP() / IP() / UDP() / Raw(),
            Ether(dst=mac_id) / IP() / IP() / TCP() / Raw(),
            Ether(dst=mac_id) / IP() / IP() / SCTP() / Raw(),
            Ether(dst=mac_id) / IP() / IP() / ICMP() / Raw(),
            Ether(dst=mac_id) / IP() / IPv6() / IPv6ExtHdrFragment() / Raw(),
        ]
        flag_list = [
            RtePTypes.TUNNEL_IP | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.INNER_L4_FRAG,
            RtePTypes.TUNNEL_IP | RtePTypes.INNER_L4_NONFRAG,
            RtePTypes.TUNNEL_IP | RtePTypes.INNER_L4_UDP,
            RtePTypes.TUNNEL_IP | RtePTypes.INNER_L4_TCP,
            RtePTypes.TUNNEL_IP | RtePTypes.INNER_L4_SCTP,
            RtePTypes.TUNNEL_IP | RtePTypes.INNER_L4_ICMP,
            RtePTypes.TUNNEL_IP | RtePTypes.INNER_L3_IPV6_EXT_UNKNOWN | RtePTypes.INNER_L4_FRAG,
        ]
        with TestPmdShell() as testpmd:
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)

    @func_test
    def test_gre_tunnel_packet_detect(self) -> None:
        """Ensure the correct flags are shown in the verbose output when sending GRE packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / GRE() / IP(frag=5) / Raw(),
            Ether(dst=mac_id) / IP() / GRE() / IP() / Raw(),
            Ether(dst=mac_id) / IP() / GRE() / IP() / UDP() / Raw(),
            Ether(dst=mac_id) / IP() / GRE() / IP() / TCP() / Raw(),
            Ether(dst=mac_id) / IP() / GRE() / IP() / SCTP() / Raw(),
            Ether(dst=mac_id) / IP() / GRE() / IP() / ICMP() / Raw(),
        ]
        flag_list = [
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_FRAG | RtePTypes.INNER_L3_IPV4_EXT_UNKNOWN,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_NONFRAG,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_UDP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_TCP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_SCTP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_ICMP,
        ]
        with TestPmdShell() as testpmd:
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)

    @func_test
    def test_nsh_packet_detect(self) -> None:
        """Verify the correct flags are shown in the verbose output when sending NSH packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id, type=0x894F) / NSH() / IP(),
            Ether(dst=mac_id, type=0x894F) / NSH() / IP() / ICMP(),
            Ether(dst=mac_id, type=0x894F) / NSH() / IP(frag=1, flags="MF"),
            Ether(dst=mac_id, type=0x894F) / NSH() / IP() / TCP(),
            Ether(dst=mac_id, type=0x894F) / NSH() / IP() / UDP(),
            Ether(dst=mac_id, type=0x894F) / NSH() / IP() / SCTP(tag=1) / SCTPChunkData(data="x"),
            Ether(dst=mac_id, type=0x894F) / NSH() / IPv6(),
        ]
        flag_list = [
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L4_NONFRAG,
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L4_ICMP,
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L4_FRAG,
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L4_TCP,
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L4_UDP,
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV4_EXT_UNKNOWN | RtePTypes.L4_SCTP,
            RtePTypes.L2_ETHER_NSH | RtePTypes.L3_IPV6_EXT_UNKNOWN | RtePTypes.L4_NONFRAG,
        ]
        with TestPmdShell() as testpmd:
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)

    @func_test
    def test_vxlan_tunnel_packet_detect(self) -> None:
        """Ensure the correct flags are shown in the verbose output when sending VXLAN packets.

        Steps:
            Create a list of packets to test, with a corresponding flag list to check against.
            Add a UDP port for VXLAN packet filter within testpmd.
            Launch testpmd with the necessary configuration.
            Send each packet in the list, capture testpmd verbose output.

        Verify:
            Check that each packet has a destination MAC address matching the set ID.
            Check the packet type fields in verbose output, verify the flags match.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / UDP() / VXLAN() / Ether() / IP(frag=5) / Raw(),
            Ether(dst=mac_id) / IP() / UDP() / VXLAN() / Ether() / IP() / Raw(),
            Ether(dst=mac_id) / IP() / UDP() / VXLAN() / Ether() / IP() / UDP() / Raw(),
            Ether(dst=mac_id) / IP() / UDP() / VXLAN() / Ether() / IP() / TCP() / Raw(),
            Ether(dst=mac_id) / IP() / UDP() / VXLAN() / Ether() / IP() / SCTP() / Raw(),
            Ether(dst=mac_id) / IP() / UDP() / VXLAN() / Ether() / IP() / ICMP() / Raw(),
            (
                Ether(dst=mac_id)
                / IP()
                / UDP()
                / VXLAN()
                / Ether()
                / IPv6()
                / IPv6ExtHdrFragment()
                / Raw()
            ),
        ]
        flag_list = [
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_FRAG | RtePTypes.INNER_L3_IPV4_EXT_UNKNOWN,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_NONFRAG,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_UDP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_TCP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_SCTP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L4_ICMP,
            RtePTypes.TUNNEL_GRENAT | RtePTypes.INNER_L3_IPV6_EXT_UNKNOWN | RtePTypes.INNER_L4_FRAG,
        ]
        with TestPmdShell() as testpmd:
            testpmd.rx_vxlan(4789, 0, True)
            self.setup_session(testpmd=testpmd, expected_flags=flag_list, packet_list=packet_list)
