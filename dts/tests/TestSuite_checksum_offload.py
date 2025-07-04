# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""DPDK checksum offload testing suite.

This suite verifies L3/L4 checksum offload features of the Poll Mode Driver.
On the Rx side, IPv4 and UDP/TCP checksum by hardware is checked to ensure
checksum flags match expected flags. On the Tx side, IPv4/UDP, IPv4/TCP,
IPv6/UDP, and IPv6/TCP insertion by hardware is checked to checksum flags
match expected flags.

"""

from typing import List

from scapy.layers.inet import IP, TCP, UDP
from scapy.layers.inet6 import IPv6
from scapy.layers.l2 import Dot1Q, Ether
from scapy.layers.sctp import SCTP
from scapy.packet import Packet, Raw

from framework.params.testpmd import SimpleForwardingModes
from framework.remote_session.testpmd_shell import (
    ChecksumOffloadOptions,
    PacketOffloadFlag,
    TestPmdShell,
)
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import NicCapability, TopologyType, requires


@requires(topology_type=TopologyType.two_links)
@requires(NicCapability.RX_OFFLOAD_CHECKSUM)
@requires(NicCapability.RX_OFFLOAD_IPV4_CKSUM)
@requires(NicCapability.RX_OFFLOAD_UDP_CKSUM)
@requires(NicCapability.RX_OFFLOAD_TCP_CKSUM)
class TestChecksumOffload(TestSuite):
    """Checksum offload test suite.

    This suite consists of 6 test cases:
    1. Insert checksum on transmit packet
    2. Do not insert checksum on transmit packet
    3. Hardware checksum check L4 Rx
    4. Hardware checksum check L3 Rx
    5. Validate Rx checksum valid flags
    6. Checksum offload with VLAN
    7. Checksum offload with SCTP

    """

    def send_packets_and_verify(
        self, packet_list: List[Packet], load: bytes, should_receive: bool
    ) -> None:
        """Iterates through a list of packets and verifies they are received.

        Args:
            packet_list: List of Scapy packets to send and verify.
            load: Raw layer load attribute in the sent packet.
            should_receive: Indicates whether the packet should be received
                by the traffic generator.
        """
        for i in range(0, len(packet_list)):
            received_packets = self.send_packet_and_capture(packet=packet_list[i])
            received = any(
                packet.haslayer(Raw) and load in packet.load for packet in received_packets
            )
            self.verify(
                received == should_receive,
                f"Packet was {'dropped' if should_receive else 'received'}",
            )

    def send_packet_and_verify_checksum(
        self, packet: Packet, good_L4: bool, good_IP: bool, testpmd: TestPmdShell, id: str
    ) -> None:
        """Send packet and verify verbose output matches expected output.

        Args:
            packet: Scapy packet to send to DUT.
            good_L4: Verifies RTE_MBUF_F_RX_L4_CKSUM_GOOD in verbose output
                if :data:`True`, or RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN if :data:`False`.
            good_IP: Verifies RTE_MBUF_F_RX_IP_CKSUM_GOOD in verbose output
                if :data:`True`, or RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN if :data:`False`.
            testpmd: Testpmd shell session to analyze verbose output of.
            id: The destination mac address that matches the sent packet in verbose output.
        """
        testpmd.start()
        self.send_packet_and_capture(packet=packet)
        verbose_output = testpmd.extract_verbose_output(testpmd.stop())
        for testpmd_packet in verbose_output:
            if testpmd_packet.dst_mac == id:
                is_IP = PacketOffloadFlag.RTE_MBUF_F_RX_IP_CKSUM_GOOD in testpmd_packet.ol_flags
                is_L4 = PacketOffloadFlag.RTE_MBUF_F_RX_L4_CKSUM_GOOD in testpmd_packet.ol_flags
        self.verify(is_L4 == good_L4, "Layer 4 checksum flag did not match expected checksum flag.")
        self.verify(is_IP == good_IP, "IP checksum flag did not match expected checksum flag.")

    def setup_hw_offload(self, testpmd: TestPmdShell) -> None:
        """Sets IP, UDP, and TCP layers to hardware offload.

        Args:
            testpmd: Testpmd shell to configure.
        """
        testpmd.stop_all_ports()
        offloads = (
            ChecksumOffloadOptions.ip | ChecksumOffloadOptions.udp | ChecksumOffloadOptions.tcp
        )
        testpmd.csum_set_hw(layers=offloads, port_id=0)
        testpmd.start_all_ports()

    @func_test
    def test_insert_checksums(self) -> None:
        """Enable checksum offload insertion and verify packet reception.

        Steps:
            Create a list of packets to send.
            Launch testpmd with the necessary configuration.
            Enable checksum hardware offload.
            Send list of packets.

        Verify:
            Verify packets are received.
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        payload = b"xxxxx"
        packet_list = [
            Ether(dst=mac_id) / IP() / UDP() / Raw(payload),
            Ether(dst=mac_id) / IP() / TCP() / Raw(payload),
            Ether(dst=mac_id) / IPv6(src="::1") / UDP() / Raw(payload),
            Ether(dst=mac_id) / IPv6(src="::1") / TCP() / Raw(payload),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            self.setup_hw_offload(testpmd=testpmd)
            testpmd.start()
            self.send_packets_and_verify(packet_list=packet_list, load=payload, should_receive=True)
            for i in range(0, len(packet_list)):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=True, good_IP=True, testpmd=testpmd, id=mac_id
                )

    @func_test
    def test_no_insert_checksums(self) -> None:
        """Disable checksum offload insertion and verify packet reception.

        Steps:
            Create a list of packets to send.
            Launch testpmd with the necessary configuration.
            Send list of packets.

        Verify:
            Verify packets are received.
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        payload = b"xxxxx"
        packet_list = [
            Ether(dst=mac_id) / IP() / UDP() / Raw(payload),
            Ether(dst=mac_id) / IP() / TCP() / Raw(payload),
            Ether(dst=mac_id) / IPv6(src="::1") / UDP() / Raw(payload),
            Ether(dst=mac_id) / IPv6(src="::1") / TCP() / Raw(payload),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            testpmd.start()
            self.send_packets_and_verify(packet_list=packet_list, load=payload, should_receive=True)
            for i in range(0, len(packet_list)):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=True, good_IP=True, testpmd=testpmd, id=mac_id
                )

    @func_test
    def test_l4_rx_checksum(self) -> None:
        """Tests L4 Rx checksum in a variety of scenarios.

        Steps:
            Create a list of packets to send with UDP/TCP fields.
            Launch testpmd with the necessary configuration.
            Enable checksum hardware offload.
            Send list of packets.

        Verify:
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / UDP(),
            Ether(dst=mac_id) / IP() / TCP(),
            Ether(dst=mac_id) / IP() / UDP(chksum=0xF),
            Ether(dst=mac_id) / IP() / TCP(chksum=0xF),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            self.setup_hw_offload(testpmd=testpmd)
            for i in range(0, 2):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=True, good_IP=True, testpmd=testpmd, id=mac_id
                )
            for i in range(2, 4):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=False, good_IP=True, testpmd=testpmd, id=mac_id
                )

    @func_test
    def test_l3_rx_checksum(self) -> None:
        """Tests L3 Rx checksum hardware offload.

        Steps:
            Create a list of packets to send with IP fields.
            Launch testpmd with the necessary configuration.
            Enable checksum hardware offload.
            Send list of packets.

        Verify:
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / UDP(),
            Ether(dst=mac_id) / IP() / TCP(),
            Ether(dst=mac_id) / IP(chksum=0xF) / UDP(),
            Ether(dst=mac_id) / IP(chksum=0xF) / TCP(),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            self.setup_hw_offload(testpmd=testpmd)
            for i in range(0, 2):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=True, good_IP=True, testpmd=testpmd, id=mac_id
                )
            for i in range(2, 4):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=True, good_IP=False, testpmd=testpmd, id=mac_id
                )

    @func_test
    def test_validate_rx_checksum(self) -> None:
        """Verify verbose output of Rx packets matches expected behavior.

        Steps:
            Create a list of packets to send.
            Launch testpmd with the necessary configuration.
            Enable checksum hardware offload.
            Send list of packets.

        Verify:
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / UDP(),
            Ether(dst=mac_id) / IP() / TCP(),
            Ether(dst=mac_id) / IPv6(src="::1") / UDP(),
            Ether(dst=mac_id) / IPv6(src="::1") / TCP(),
            Ether(dst=mac_id) / IP(chksum=0x0) / UDP(chksum=0xF),
            Ether(dst=mac_id) / IP(chksum=0x0) / TCP(chksum=0xF),
            Ether(dst=mac_id) / IPv6(src="::1") / UDP(chksum=0xF),
            Ether(dst=mac_id) / IPv6(src="::1") / TCP(chksum=0xF),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            self.setup_hw_offload(testpmd=testpmd)
            for i in range(0, 4):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=True, good_IP=True, testpmd=testpmd, id=mac_id
                )
            for i in range(4, 6):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=False, good_IP=False, testpmd=testpmd, id=mac_id
                )
            for i in range(6, 8):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=False, good_IP=True, testpmd=testpmd, id=mac_id
                )

    @requires(NicCapability.RX_OFFLOAD_VLAN)
    @func_test
    def test_vlan_checksum(self) -> None:
        """Test VLAN Rx checksum hardware offload and verify packet reception.

        Steps:
            Create a list of packets to send with VLAN fields.
            Launch testpmd with the necessary configuration.
            Enable checksum hardware offload.
            Send list of packets.

        Verify:
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        payload = b"xxxxx"
        packet_list = [
            Ether(dst=mac_id) / Dot1Q(vlan=1) / IP(chksum=0x0) / UDP(chksum=0xF) / Raw(payload),
            Ether(dst=mac_id) / Dot1Q(vlan=1) / IP(chksum=0x0) / TCP(chksum=0xF) / Raw(payload),
            Ether(dst=mac_id) / Dot1Q(vlan=1) / IPv6(src="::1") / UDP(chksum=0xF) / Raw(payload),
            Ether(dst=mac_id) / Dot1Q(vlan=1) / IPv6(src="::1") / TCP(chksum=0xF) / Raw(payload),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            self.setup_hw_offload(testpmd=testpmd)
            testpmd.start()
            self.send_packets_and_verify(packet_list=packet_list, load=payload, should_receive=True)
            for i in range(0, 2):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=False, good_IP=False, testpmd=testpmd, id=mac_id
                )
            for i in range(2, 4):
                self.send_packet_and_verify_checksum(
                    packet=packet_list[i], good_L4=False, good_IP=True, testpmd=testpmd, id=mac_id
                )

    @requires(NicCapability.RX_OFFLOAD_SCTP_CKSUM)
    @func_test
    def test_validate_sctp_checksum(self) -> None:
        """Test SCTP Rx checksum hardware offload and verify packet reception.

        Steps:
            Create a list of packets to send with SCTP fields.
            Launch testpmd with the necessary configuration.
            Enable checksum hardware offload.
            Send list of packets.

        Verify:
            Verify packet checksums match the expected flags.
        """
        mac_id = "00:00:00:00:00:01"
        packet_list = [
            Ether(dst=mac_id) / IP() / SCTP(),
            Ether(dst=mac_id) / IP() / SCTP(chksum=0xF),
        ]
        with TestPmdShell(enable_rx_cksum=True) as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.csum)
            testpmd.set_verbose(level=1)
            testpmd.csum_set_hw(layers=ChecksumOffloadOptions.sctp)
            testpmd.start()
            self.send_packet_and_verify_checksum(
                packet=packet_list[0], good_L4=True, good_IP=True, testpmd=testpmd, id=mac_id
            )
            self.send_packet_and_verify_checksum(
                packet=packet_list[1], good_L4=False, good_IP=True, testpmd=testpmd, id=mac_id
            )
