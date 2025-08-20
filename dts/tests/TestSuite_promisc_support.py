# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Promiscuous mode support test suite.

Test promiscuous support by sending a packet with a different destination
mac address from the TG to the SUT.
"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from framework.remote_session.testpmd_shell import NicCapability, TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import requires


@requires(NicCapability.PHYSICAL_FUNCTION)
class TestPromiscSupport(TestSuite):
    """Promiscuous mode support test suite."""

    #: Alternate MAC address.
    ALTERNATIVE_MAC_ADDRESS: str = "02:00:00:00:00:00"

    @func_test
    def promisc_packets(self) -> None:
        """Verify that promiscuous mode works.

        Steps:
            Create a packet with a different mac address to the destination.
            Enable promiscuous mode.
            Send and receive packet.
            Disable promiscuous mode.
            Send and receive packet.
        Verify:
            Packet sent with the wrong address is received in promiscuous mode and filtered out
            otherwise.

        """
        packet = [Ether(dst=self.ALTERNATIVE_MAC_ADDRESS) / IP() / Raw(load=b"\x00" * 64)]

        with TestPmdShell() as testpmd:
            for port_id, _ in enumerate(self.topology.sut_ports):
                testpmd.set_promisc(port=port_id, enable=True, verify=True)
            testpmd.start()

            received_packets = self.send_packets_and_capture(packet)
            expected_packets = self.get_expected_packets(packet, sent_from_tg=True)
            self.match_all_packets(expected_packets, received_packets)

            testpmd.stop()

            for port_id, _ in enumerate(self.topology.sut_ports):
                testpmd.set_promisc(port=port_id, enable=False, verify=True)
            testpmd.start()

            received_packets = self.send_packets_and_capture(packet)
            expected_packets = self.get_expected_packets(packet, sent_from_tg=True)
            self.verify(
                not self.match_all_packets(expected_packets, received_packets, verify=False),
                "Invalid packet wasn't filtered out.",
            )
