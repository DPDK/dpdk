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

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Packet, Raw
from scapy.utils import hexstr

from api.capabilities import (
    NicCapability,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from api.testpmd.config import SimpleForwardingModes
from framework.test_suite import TestSuite, func_test


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
@requires_nic_capability(NicCapability.RX_OFFLOAD_SCATTER)
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
        self.topology.tg_port_egress.configure_mtu(9000)
        self.topology.tg_port_ingress.configure_mtu(9000)

    def scatter_pktgen_send_packet(self, pkt_size: int) -> list[Packet]:
        """Generate and send a packet to the SUT then capture what is forwarded back.

        Generate an IP packet of a specific length and send it to the SUT,
        then capture the resulting received packets and filter them down to the ones that have the
        correct layers. The desired length of the packet is met by packing its payload
        with the letter "X" in hexadecimal.

        Args:
            pkt_size: Size of the packet to generate and send.

        Returns:
            The filtered down list of received packets.
        """
        packet = Ether() / IP() / Raw()
        if layer2 := packet.getlayer(2):
            layer2.load = ""
        payload_len = pkt_size - len(packet) - 4
        payload = ["58"] * payload_len
        # pack the payload
        for X_in_hex in payload:
            packet.load += struct.pack("=B", int("%s%s" % (X_in_hex[0], X_in_hex[1]), 16))
        received_packets = self.send_packet_and_capture(packet)
        # filter down the list to packets that have the appropriate structure
        received_packets = [p for p in received_packets if Ether in p and IP in p and Raw in p]

        self.verify(len(received_packets) > 0, "Did not receive any packets.")

        layer2 = received_packets[0].getlayer(2)
        self.verify(layer2 is not None, "The received packet is invalid.")
        assert layer2 is not None

        return received_packets

    def pmd_scatter(self, mb_size: int, enable_offload: bool = False) -> None:
        """Testpmd support of receiving and sending scattered multi-segment packets.

        Support for scattered packets is shown by sending 5 packets of differing length
        where the length of the packet is calculated by taking mbuf-size + an offset.
        The offsets used in the test are -1, 0, 1, 4, 5 respectively.

        Args:
            mb_size: Size to set memory buffers to when starting testpmd.
            enable_offload: Whether or not to offload the scattering functionality in testpmd.

        Test:
            Start testpmd and run functional test with preset `mb_size`.
        """
        with TestPmd(
            forward_mode=SimpleForwardingModes.mac,
            mbcache=200,
            mbuf_size=[mb_size],
            max_pkt_len=9000,
            tx_offloads=0x00008000,
            enable_scatter=True if enable_offload else None,
        ) as testpmd:
            testpmd.start()

            for offset in [-1, 0, 1, 4, 5]:
                recv_packets = self.scatter_pktgen_send_packet(mb_size + offset)
                self._logger.debug(f"Relevant captured packets: \n{recv_packets}")
                self.verify(
                    any(" ".join(["58"] * 8) in hexstr(pkt, onlyhex=1) for pkt in recv_packets),
                    "Payload of scattered packet did not match expected payload with offset "
                    f"{offset}.",
                )

    @requires_nic_capability(NicCapability.SCATTERED_RX_ENABLED)
    @func_test
    def scatter_mbuf_2048(self) -> None:
        """Run the :meth:`pmd_scatter` test with `mb_size` set to 2048."""
        self.pmd_scatter(mb_size=2048)

    @requires_nic_capability(NicCapability.RX_OFFLOAD_SCATTER)
    @func_test
    def scatter_mbuf_2048_with_offload(self) -> None:
        """Run the :meth:`pmd_scatter` test with `mb_size` set to 2048 and rx_scatter offload."""
        self.pmd_scatter(mb_size=2048, enable_offload=True)

    def tear_down_suite(self) -> None:
        """Tear down the test suite.

        Teardown:
            Set the MTU of the tg_node back to a more standard size of 1500.
        """
        self.topology.tg_port_egress.configure_mtu(1500)
        self.topology.tg_port_ingress.configure_mtu(1500)
