# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Traffic generator that can capture packets.

In functional testing, we need to interrogate received packets to check their validity.
The module defines the interface common to all traffic generators capable of capturing
traffic.
"""

import uuid
from abc import abstractmethod

import scapy.utils  # type: ignore[import]
from scapy.packet import Packet  # type: ignore[import]

from framework.settings import SETTINGS
from framework.testbed_model.port import Port
from framework.utils import get_packet_summaries

from .traffic_generator import TrafficGenerator


def _get_default_capture_name() -> str:
    return str(uuid.uuid4())


class CapturingTrafficGenerator(TrafficGenerator):
    """Capture packets after sending traffic.

    The intermediary interface which enables a packet generator to declare that it can capture
    packets and return them to the user.

    Similarly to :class:`~.traffic_generator.TrafficGenerator`, this class exposes
    the public methods specific to capturing traffic generators and defines a private method
    that must implement the traffic generation and capturing logic in subclasses.

    The methods of capturing traffic generators obey the following workflow:

        1. send packets
        2. capture packets
        3. write the capture to a .pcap file
        4. return the received packets
    """

    @property
    def is_capturing(self) -> bool:
        """This traffic generator can capture traffic."""
        return True

    def send_packet_and_capture(
        self,
        packet: Packet,
        send_port: Port,
        receive_port: Port,
        duration: float,
        capture_name: str = _get_default_capture_name(),
    ) -> list[Packet]:
        """Send `packet` and capture received traffic.

        Send `packet` on `send_port` and then return all traffic captured
        on `receive_port` for the given `duration`.

        The captured traffic is recorded in the `capture_name`.pcap file.

        Args:
            packet: The packet to send.
            send_port: The egress port on the TG node.
            receive_port: The ingress port in the TG node.
            duration: Capture traffic for this amount of time after sending the packet.
            capture_name: The name of the .pcap file where to store the capture.

        Returns:
             The received packets. May be empty if no packets are captured.
        """
        return self.send_packets_and_capture(
            [packet], send_port, receive_port, duration, capture_name
        )

    def send_packets_and_capture(
        self,
        packets: list[Packet],
        send_port: Port,
        receive_port: Port,
        duration: float,
        capture_name: str = _get_default_capture_name(),
    ) -> list[Packet]:
        """Send `packets` and capture received traffic.

        Send `packets` on `send_port` and then return all traffic captured
        on `receive_port` for the given `duration`.

        The captured traffic is recorded in the `capture_name`.pcap file. The target directory
        can be configured with the :option:`--output-dir` command line argument or
        the :envvar:`DTS_OUTPUT_DIR` environment variable.

        Args:
            packets: The packets to send.
            send_port: The egress port on the TG node.
            receive_port: The ingress port in the TG node.
            duration: Capture traffic for this amount of time after sending the packets.
            capture_name: The name of the .pcap file where to store the capture.

        Returns:
             The received packets. May be empty if no packets are captured.
        """
        self._logger.debug(get_packet_summaries(packets))
        self._logger.debug(
            f"Sending packet on {send_port.logical_name}, receiving on {receive_port.logical_name}."
        )
        received_packets = self._send_packets_and_capture(
            packets,
            send_port,
            receive_port,
            duration,
        )

        self._logger.debug(f"Received packets: {get_packet_summaries(received_packets)}")
        self._write_capture_from_packets(capture_name, received_packets)
        return received_packets

    @abstractmethod
    def _send_packets_and_capture(
        self,
        packets: list[Packet],
        send_port: Port,
        receive_port: Port,
        duration: float,
    ) -> list[Packet]:
        """The implementation of :method:`send_packets_and_capture`.

        The subclasses must implement this method which sends `packets` on `send_port`
        and receives packets on `receive_port` for the specified `duration`.

        It must be able to handle receiving no packets.
        """

    def _write_capture_from_packets(self, capture_name: str, packets: list[Packet]) -> None:
        file_name = f"{SETTINGS.output_dir}/{capture_name}.pcap"
        self._logger.debug(f"Writing packets to {file_name}.")
        scapy.utils.wrpcap(file_name, packets)
