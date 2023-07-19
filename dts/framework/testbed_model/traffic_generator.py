# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""The base traffic generator.

These traffic generators can't capture received traffic,
only count the number of received packets.
"""

from abc import ABC, abstractmethod

from scapy.packet import Packet  # type: ignore[import]

from framework.logger import DTSLOG
from framework.utils import get_packet_summaries

from .hw.port import Port


class TrafficGenerator(ABC):
    """The base traffic generator.

    Defines the few basic methods that each traffic generator must implement.
    """

    _logger: DTSLOG

    def send_packet(self, packet: Packet, port: Port) -> None:
        """Send a packet and block until it is fully sent.

        What fully sent means is defined by the traffic generator.

        Args:
            packet: The packet to send.
            port: The egress port on the TG node.
        """
        self.send_packets([packet], port)

    def send_packets(self, packets: list[Packet], port: Port) -> None:
        """Send packets and block until they are fully sent.

        What fully sent means is defined by the traffic generator.

        Args:
            packets: The packets to send.
            port: The egress port on the TG node.
        """
        self._logger.info(f"Sending packet{'s' if len(packets) > 1 else ''}.")
        self._logger.debug(get_packet_summaries(packets))
        self._send_packets(packets, port)

    @abstractmethod
    def _send_packets(self, packets: list[Packet], port: Port) -> None:
        """
        The extended classes must implement this method which
        sends packets on send_port. The method should block until all packets
        are fully sent.
        """

    @property
    def is_capturing(self) -> bool:
        """Whether this traffic generator can capture traffic.

        Returns:
            True if the traffic generator can capture traffic, False otherwise.
        """
        return False

    @abstractmethod
    def close(self) -> None:
        """Free all resources used by the traffic generator."""
