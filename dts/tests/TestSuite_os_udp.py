# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Basic IPv4 OS routing test suite.

Configure SUT node to route traffic from if1 to if2.
Send a packet to the SUT node, verify it comes back on the second port on the TG node.
"""

from scapy.layers.inet import IP, UDP  # type: ignore[import]
from scapy.layers.l2 import Ether  # type: ignore[import]

from framework.test_suite import TestSuite


class TestOSUdp(TestSuite):
    """IPv4 UDP OS routing test suite."""

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Setup:
            Bind the SUT ports to the OS driver, configure the ports and configure the SUT
            to route traffic from if1 to if2.
        """
        self.sut_node.bind_ports_to_driver(for_dpdk=False)
        self.configure_testbed_ipv4()

    def test_os_udp(self) -> None:
        """Basic UDP IPv4 traffic test case.

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
        """Tear down the test suite.

        Teardown:
            Remove the SUT port configuration configured in setup.
        """
        self.configure_testbed_ipv4(restore=True)
        # Assume other suites will likely need dpdk driver
        self.sut_node.bind_ports_to_driver(for_dpdk=True)
