# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""
Configure SUT node to route traffic from if1 to if2.
Send a packet to the SUT node, verify it comes back on the second port on the TG node.
"""

from scapy.layers.inet import IP, UDP  # type: ignore[import]
from scapy.layers.l2 import Ether  # type: ignore[import]

from framework.test_suite import TestSuite


class TestOSUdp(TestSuite):
    def set_up_suite(self) -> None:
        """
        Setup:
            Configure SUT ports and SUT to route traffic from if1 to if2.
        """

        self.configure_testbed_ipv4()

    def test_os_udp(self) -> None:
        """
        Steps:
            Send a UDP packet.
        Verify:
            The packet with proper addresses arrives at the other TG port.
        """

        packet = Ether() / IP() / UDP()

        received_packets = self.send_packet_and_capture(packet)

        expected_packet = self.get_expected_packet(packet)

        self.verify_packets(expected_packet, received_packets)

    def tear_down_suite(self) -> None:
        """
        Teardown:
            Remove the SUT port configuration configured in setup.
        """
        self.configure_testbed_ipv4(restore=True)
