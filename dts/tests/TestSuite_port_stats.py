# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Port Statistics testing suite.

This test suite tests the functionality of querying the statistics of a port and verifies that the
values provided in the statistics accurately reflect the traffic that has been handled on the port.
This is shown by sending a packet of a fixed size to the SUT and verifying that the number of Rx
packets has increased by 1, the number of Rx bytes has increased by the specified size, the number
of Tx packets has also increased by 1 (since we expect the packet to be forwarded), and the number
of Tx bytes has also increased by the same fixed amount.
"""

from typing import ClassVar, Tuple

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Packet, Raw

from api.capabilities import (
    LinkTopology,
    NicCapability,
    requires_link_topology,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from api.testpmd.config import SimpleForwardingModes
from api.testpmd.types import RtePTypes, TestPmdVerbosePacket
from framework.test_suite import TestSuite, func_test


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
@requires_link_topology(LinkTopology.TWO_LINKS)
class TestPortStats(TestSuite):
    """DPDK Port statistics testing suite.

    Support for port statistics is tested by sending a packet of a fixed size denoted by
    `total_packet_len` and verifying the that Tx/Rx packets of the Tx/Rx ports updated by exactly
    1 and the Tx/Rx bytes of the Tx/Rx ports updated by exactly `total_packet_len`. This is done by
    finding the total amount of packets that were sent/received which did not originate from this
    test suite and taking the sum of the lengths of each of these "noise" packets and subtracting
    it from the total values in the port statistics so that all that is left are relevant values.
    """

    #: Port where traffic will be received on the SUT.
    recv_port: ClassVar[int] = 0
    #: Port where traffic will be sent from on the SUT.
    send_port: ClassVar[int] = 1

    #:
    ip_header_len: ClassVar[int] = 20
    #:
    ether_header_len: ClassVar[int] = 14

    #: Length of the packet being sent including the IP and frame headers.
    total_packet_len: ClassVar[int] = 100

    @property
    def _send_pkt(self) -> Packet:
        """Packet to send during testing."""
        return (
            Ether()
            / IP()
            / Raw(b"X" * (self.total_packet_len - self.ip_header_len - self.ether_header_len))
        )

    def _extract_noise_information(
        self, verbose_out: list[TestPmdVerbosePacket]
    ) -> Tuple[int, int, int, int]:
        """Extract information about packets that were not sent by the framework in `verbose_out`.

        Extract the number of sent/received packets that did not originate from this test suite as
        well as the sum of the lengths of said "noise" packets. Note that received packets are only
        examined on the port with the ID `self.recv_port` since these are the receive stats that
        will be analyzed in this suite. Sent packets are also only examined on the port with the ID
        `self.send_port`.

        Packets are considered to be "noise" when they don't match the expected structure of the
        packets that are being sent by this test suite. Specifically, the source and destination
        mac addresses as well as the software packet type are checked on packets received by
        testpmd to ensure they match the proper addresses of the TG and SUT nodes. Packets that are
        sent by testpmd however only check the source mac address and the software packet type.
        This is because MAC forwarding mode adjusts both addresses, but only the source will belong
        to the TG or SUT node.

        Args:
            verbose_out: Parsed testpmd verbose output to collect the noise information from.

        Returns:
            A tuple containing the total size of received noise in bytes, the number of received
            noise packets, size of all noise packets sent by testpmd in bytes, and the number of
            noise packets sent by testpmd.
        """
        recv_noise_bytes = 0
        recv_noise_packets = 0
        sent_noise_bytes = 0
        num_sent_packets = 0
        for verbose_packet in verbose_out:
            if verbose_packet.was_received and verbose_packet.port_id == self.recv_port:
                if (
                    verbose_packet.src_mac.lower()
                    != self.topology.tg_port_egress.mac_address.lower()
                    or verbose_packet.dst_mac.lower()
                    != self.topology.sut_port_ingress.mac_address.lower()
                    or verbose_packet.sw_ptype != (RtePTypes.L2_ETHER | RtePTypes.L3_IPV4)
                ):
                    recv_noise_bytes += verbose_packet.length
                    recv_noise_packets += 1
            elif not verbose_packet.was_received and verbose_packet.port_id == self.send_port:
                if (
                    verbose_packet.src_mac.lower()
                    != self.topology.sut_port_egress.mac_address.lower()
                    or verbose_packet.sw_ptype != (RtePTypes.L2_ETHER | RtePTypes.L3_IPV4)
                ):
                    sent_noise_bytes += verbose_packet.length
                    num_sent_packets += 1

        return recv_noise_bytes, recv_noise_packets, sent_noise_bytes, num_sent_packets

    @func_test
    def stats_updates(self) -> None:
        """Send a packet with a fixed length and verify port stats updated properly.

        Send a packet with a total length of `self.total_packet_len` and verify that the rx port
        only received one packet and the number of rx_bytes increased by exactly
        `self.total_packet_len`. Also verify that the tx port only sent one packet and that the
        tx_bytes of the port increase by exactly `self.total_packet_len`.

        Noise on the wire is ignored by extracting the total number of noise packets and the sum of
        the lengths of those packets and subtracting them from the totals that are provided by the
        testpmd command `show port info all`.

        Steps:
            * Start testpmd in MAC forwarding mode and set verbose mode to 3 (Rx and Tx).
            * Start packet forwarding and then clear all port statistics.
            * Send a packet, then stop packet forwarding and collect the port stats.

        Verify:
            * Port stats showing number of packets received match what we sent.
        """
        with TestPmd(forward_mode=SimpleForwardingModes.mac) as testpmd:
            testpmd.set_verbose(3)
            testpmd.start()
            testpmd.clear_port_stats_all()
            self.send_packet_and_capture(self._send_pkt)
            port_stats_all, forwarding_info = testpmd.show_port_stats_all()
            verbose_information = TestPmd.extract_verbose_output(forwarding_info)

        # Gather information from irrelevant packets sent/ received on the same port.
        rx_irr_bytes, rx_irr_pakts, tx_irr_bytes, tx_irr_pakts = self._extract_noise_information(
            verbose_information
        )
        recv_relevant_packets = port_stats_all[self.recv_port].rx_packets - rx_irr_pakts
        sent_relevant_packets = port_stats_all[self.send_port].tx_packets - tx_irr_pakts
        recv_relevant_bytes = port_stats_all[self.recv_port].rx_bytes - rx_irr_bytes
        sent_relevant_bytes = port_stats_all[self.send_port].tx_bytes - tx_irr_bytes

        self.verify(
            recv_relevant_packets == 1,
            f"Port {self.recv_port} received {recv_relevant_packets} packets but expected to only "
            "receive 1.",
        )
        self.verify(
            recv_relevant_bytes == self.total_packet_len,
            f"Number of bytes received by port {self.recv_port} did not match the amount sent.",
        )
        self.verify(
            sent_relevant_packets == 1,
            f"Number was packets sent by port {self.send_port} was not equal to the number "
            f"received by port {self.recv_port}.",
        )
        self.verify(
            sent_relevant_bytes == self.total_packet_len,
            f"Number of bytes sent by port {self.send_port} did not match the number of bytes "
            f"received by port {self.recv_port}.",
        )
