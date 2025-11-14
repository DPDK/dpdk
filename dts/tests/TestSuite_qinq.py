# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""QinQ (802.1ad) Test Suite.

This test suite verifies the correctness and capability of DPDK Poll Mode Drivers (PMDs)
in handling QinQ-tagged Ethernet frames, which contain a pair of stacked VLAN headers
(outer S-VLAN and inner C-VLAN). These tests ensure that both software and hardware offloads
related to QinQ behave as expected across different NIC vendors and PMD implementations.
"""

from typing import Optional

from scapy.layers.inet import IP, UDP
from scapy.layers.l2 import Dot1AD, Dot1Q, Ether
from scapy.packet import Packet, Raw

from api.capabilities import NicCapability, requires_nic_capability
from api.packet import send_packet_and_capture
from api.test import log, verify
from api.testpmd import TestPmd
from framework.test_suite import TestSuite, func_test


class TestQinq(TestSuite):
    """QinQ test suite.

    This suite consists of 2 test cases:
    1. QinQ Forwarding: Send a QinQ packet and verify the received packet contains
        both QinQ/VLAN layers.
    2. QinQ Strip: Enable VLAN/QinQ stripping and verify sent packets are received with the
        expected VLAN/QinQ layers.
    """

    def _send_packet_and_verify(
        self, packet: Packet, testpmd: TestPmd, should_receive: bool
    ) -> None:
        """Send packet and verify reception.

        Args:
            packet: The packet to send to testpmd.
            testpmd: The testpmd session to send commands to.
            should_receive: If :data:`True`, verifies packet was received.
        """
        testpmd.start()
        packets = send_packet_and_capture(packet=packet)
        test_packet = self._get_relevant_packet(packets)
        if should_receive:
            verify(test_packet is not None, "Packet was dropped when it should have been received.")
        else:
            verify(test_packet is None, "Packet was received when it should have been dropped.")

    def _strip_verify(self, packet: Optional[Packet], expects_tag: bool, context: str) -> bool:
        """Helper method for verifying packet stripping functionality.

        Returns: :data:`True` if tags are stripped or not stripped accordingly,
            otherwise :data:`False`
        """
        if packet is None:
            log(f"{context} packet was dropped when it should have been received.")
            return False

        if not expects_tag:
            if packet.haslayer(Dot1Q) or packet.haslayer(Dot1AD):
                log(
                    f"VLAN tags found in packet when should have been stripped: "
                    f"{packet.summary()}\tsent packet: {context}",
                )
                return False

        if expects_tag:
            if vlan_layer := packet.getlayer(Dot1Q):
                if vlan_layer.vlan != 200:
                    log(
                        f"Expected VLAN ID 200 but found ID {vlan_layer.vlan}: "
                        f"{packet.summary()}\tsent packet: {context}",
                    )
                    return False
            else:
                log(
                    f"Expected 0x8100 VLAN tag but none found: {packet.summary()}"
                    f"\tsent packet: {context}"
                )
                return False

        return True

    def _get_relevant_packet(self, packet_list: list[Packet]) -> Optional[Packet]:
        """Helper method for checking received packet list for sent packet."""
        for packet in packet_list:
            if hasattr(packet, "load") and b"xxxxx" in packet.load:
                return packet
        return None

    @func_test
    def test_qinq_forwarding(self) -> None:
        """QinQ Rx filter test case.

        Steps:
            Launch testpmd with mac forwarding mode.
            Disable VLAN filter mode on port 0.
            Send test packet and capture verbose output.

        Verify:
            Check that the received packet has two separate VLAN layers in proper QinQ fashion.
            Check that the received packet outer and inner VLAN layer has the appropriate ID.
        """
        test_packet = (
            Ether(dst="ff:ff:ff:ff:ff:ff")
            / Dot1AD(vlan=100)
            / Dot1Q(vlan=200)
            / IP(dst="1.2.3.4")
            / UDP(dport=1234, sport=4321)
            / Raw(load="xxxxx")
        )
        with TestPmd() as testpmd:
            testpmd.set_vlan_filter(0, False)
            testpmd.start()
            received_packets = send_packet_and_capture(test_packet)
            packet = self._get_relevant_packet(received_packets)

            verify(packet is not None, "Packet was dropped when it should have been received.")

            if packet is not None:
                verify(
                    bool(packet.haslayer(Dot1AD)) and bool(packet.haslayer(Dot1Q)),
                    "QinQ/VLAN layers not found in packet",
                )

                if outer_vlan := packet.getlayer(Dot1AD):
                    outer_vlan_id = outer_vlan.vlan
                    verify(
                        outer_vlan_id == 100,
                        f"Outer VLAN ID was {outer_vlan_id} when it should have been 100.",
                    )
                else:
                    verify(False, "VLAN layer not found in received packet.")

                if outer_vlan and (inner_vlan := outer_vlan.getlayer(Dot1Q)):
                    inner_vlan_id = inner_vlan.vlan
                    verify(
                        inner_vlan_id == 200,
                        f"Inner VLAN ID was {inner_vlan_id} when it should have been 200",
                    )

    @requires_nic_capability(NicCapability.PORT_RX_OFFLOAD_QINQ_STRIP)
    @func_test
    def test_qinq_strip(self) -> None:
        """Test combinations of VLAN/QinQ strip settings with various QinQ packets.

        Steps:
            Launch testpmd with QinQ and VLAN strip enabled.
            Send four VLAN/QinQ related test packets.

        Verify:
            Check received packets have the expected VLAN/QinQ layers/tags.
        """
        test_packets = [
            Ether() / Dot1Q() / IP() / UDP(dport=1234, sport=4321) / Raw(load="xxxxx"),
            Ether()
            / Dot1Q(vlan=100)
            / Dot1Q(vlan=200)
            / IP()
            / UDP(dport=1234, sport=4321)
            / Raw(load="xxxxx"),
            Ether() / Dot1AD() / IP() / UDP(dport=1234, sport=4321) / Raw(load="xxxxx"),
            Ether() / Dot1AD() / Dot1Q() / IP() / UDP(dport=1234, sport=4321) / Raw(load="xxxxx"),
        ]
        with TestPmd() as testpmd:
            testpmd.set_qinq_strip(0, True)
            testpmd.set_vlan_strip(0, True)
            testpmd.start()

            received_packets1 = send_packet_and_capture(test_packets[0])
            vlan_packet = self._get_relevant_packet(received_packets1)
            received_packets2 = send_packet_and_capture(test_packets[1])
            double_vlan_packet = self._get_relevant_packet(received_packets2)
            received_packets3 = send_packet_and_capture(test_packets[2])
            single_88a8_packet = self._get_relevant_packet(received_packets3)
            received_packets4 = send_packet_and_capture(test_packets[3])
            qinq_packet = self._get_relevant_packet(received_packets4)

            testpmd.stop()

            tests = [
                ("Single 8100 tag", self._strip_verify(vlan_packet, False, "Single 8100 tag")),
                (
                    "Double 8100 tag",
                    self._strip_verify(double_vlan_packet, True, "Double 8100 tag"),
                ),
                (
                    "Single 88a8 tag",
                    self._strip_verify(single_88a8_packet, False, "Single 88a8 tag"),
                ),
                (
                    "QinQ (88a8 and 8100 tags)",
                    self._strip_verify(qinq_packet, False, "QinQ (88a8 and 8100 tags)"),
                ),
            ]

            failed = [ctx for ctx, result in tests if not result]

            verify(
                not failed,
                f"The following packets were not stripped correctly: {', '.join(failed)}",
            )
