# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023-2024 University of New Hampshire

"""Multi-segment packet scattering testing suite.

This testing suite tests the support of transmitting and receiving scattered packets.
This is shown by the Poll Mode Driver being able to forward
scattered multi-segment packets composed of multiple non-contiguous memory buffers.
To ensure the receipt of scattered packets,
the DMA rings of the port's Rx queues must be configured
with mbuf data buffers whose size is less than the maximum length.

If it is the case that the Poll Mode Driver can forward scattered packets which it receives,
then this suffices to show the Poll Mode Driver is capable of both receiving and transmitting
scattered packets.
"""

import struct

from scapy.layers.inet import IP  # type: ignore[import-untyped]
from scapy.layers.l2 import Ether  # type: ignore[import-untyped]
from scapy.packet import Raw  # type: ignore[import-untyped]
from scapy.utils import hexstr  # type: ignore[import-untyped]

from framework.params.testpmd import SimpleForwardingModes
from framework.remote_session.testpmd_shell import TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import NicCapability, requires


@requires(NicCapability.RX_OFFLOAD_SCATTER)
class TestPmdBufferScatter(TestSuite):
    """DPDK PMD packet scattering test suite.

    Configure the Rx queues to have mbuf data buffers
    whose sizes are smaller than the maximum packet size.
    Specifically, set mbuf data buffers to have a size of 2048
    to fit a full 1512-byte (CRC included) ethernet frame in a mono-segment packet.
    The testing of scattered packets is done by sending a packet
    whose length is greater than the size of the configured size of mbuf data buffers.
    There are a total of 5 packets sent within test cases
    which have lengths less than, equal to, and greater than the mbuf size.
    There are multiple packets sent with lengths greater than the mbuf size
    in order to test cases such as:

    1. A single byte of the CRC being in a second buffer
       while the remaining 3 bytes are stored in the first buffer alongside packet data.
    2. The entire CRC being stored in a second buffer
       while all of the packet data is stored in the first.
    3. Most of the packet data being stored in the first buffer
       and a single byte of packet data stored in a second buffer alongside the CRC.
    """

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Setup:
            Increase the MTU of both ports on the traffic generator to 9000
            to support larger packet sizes.
        """
        self.tg_node.main_session.configure_port_mtu(9000, self._tg_port_egress)
        self.tg_node.main_session.configure_port_mtu(9000, self._tg_port_ingress)

    def scatter_pktgen_send_packet(self, pktsize: int) -> str:
        """Generate and send a packet to the SUT then capture what is forwarded back.

        Generate an IP packet of a specific length and send it to the SUT,
        then capture the resulting received packet and extract its payload.
        The desired length of the packet is met by packing its payload
        with the letter "X" in hexadecimal.

        Args:
            pktsize: Size of the packet to generate and send.

        Returns:
            The payload of the received packet as a string.
        """
        packet = Ether() / IP() / Raw()
        packet.getlayer(2).load = ""
        payload_len = pktsize - len(packet) - 4
        payload = ["58"] * payload_len
        # pack the payload
        for X_in_hex in payload:
            packet.load += struct.pack("=B", int("%s%s" % (X_in_hex[0], X_in_hex[1]), 16))
        received_packets = self.send_packet_and_capture(packet)
        self.verify(len(received_packets) > 0, "Did not receive any packets.")
        load = hexstr(received_packets[0].getlayer(2), onlyhex=1)

        return load

    def pmd_scatter(self, mbsize: int) -> None:
        """Testpmd support of receiving and sending scattered multi-segment packets.

        Support for scattered packets is shown by sending 5 packets of differing length
        where the length of the packet is calculated by taking mbuf-size + an offset.
        The offsets used in the test are -1, 0, 1, 4, 5 respectively.

        Test:
            Start testpmd and run functional test with preset mbsize.
        """
        with TestPmdShell(
            self.sut_node,
            forward_mode=SimpleForwardingModes.mac,
            mbcache=200,
            mbuf_size=[mbsize],
            max_pkt_len=9000,
            tx_offloads=0x00008000,
        ) as testpmd:
            testpmd.start()

            for offset in [-1, 0, 1, 4, 5]:
                recv_payload = self.scatter_pktgen_send_packet(mbsize + offset)
                self._logger.debug(
                    f"Payload of scattered packet after forwarding: \n{recv_payload}"
                )
                self.verify(
                    ("58 " * 8).strip() in recv_payload,
                    "Payload of scattered packet did not match expected payload with offset "
                    f"{offset}.",
                )

    @requires(NicCapability.SCATTERED_RX_ENABLED)
    @func_test
    def test_scatter_mbuf_2048(self) -> None:
        """Run the :meth:`pmd_scatter` test with `mbsize` set to 2048."""
        self.pmd_scatter(mbsize=2048)

    def tear_down_suite(self) -> None:
        """Tear down the test suite.

        Teardown:
            Set the MTU of the tg_node back to a more standard size of 1500.
        """
        self.tg_node.main_session.configure_port_mtu(1500, self._tg_port_egress)
        self.tg_node.main_session.configure_port_mtu(1500, self._tg_port_ingress)
