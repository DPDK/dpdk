# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Traffic generators for performance tests which can generate a high number of packets."""

from abc import abstractmethod
from dataclasses import dataclass

from scapy.packet import Packet

from .traffic_generator import TrafficGenerator


@dataclass(slots=True)
class PerformanceTrafficStats:
    """Data structure to store performance statistics for a given test run.

    Attributes:
        tx_pps: Recorded tx packets per second.
        tx_bps: Recorded tx bytes per second.
        rx_pps: Recorded rx packets per second.
        rx_bps: Recorded rx bytes per second.
        frame_size: The total length of the frame.
    """

    tx_pps: float
    tx_bps: float
    rx_pps: float
    rx_bps: float

    frame_size: int | None = None


class PerformanceTrafficGenerator(TrafficGenerator):
    """An abstract base class for all performance-oriented traffic generators.

    Provides an intermediary interface for performance-based traffic generator.
    """

    @abstractmethod
    def calculate_traffic_and_stats(
        self,
        packet: Packet,
        duration: float,
        send_mpps: int | None = None,
    ) -> PerformanceTrafficStats:
        """Send packet traffic and acquire associated statistics.

        If `send_mpps` is provided, attempt to transmit traffic at the `send_mpps` rate.
        Otherwise, attempt to transmit at line rate.

        Args:
            packet: The packet to send.
            duration: Performance test duration (in seconds).
            send_mpps: The millions packets per second send rate.

        Returns:
            Performance statistics of the generated test.
        """
