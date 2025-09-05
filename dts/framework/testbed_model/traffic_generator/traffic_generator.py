# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""The base traffic generator.

These traffic generators can't capture received traffic,
only count the number of received packets.
"""

from abc import ABC, abstractmethod
from typing import Any

from scapy.packet import Packet

from framework.config.test_run import TrafficGeneratorConfig
from framework.logger import DTSLogger, get_dts_logger
from framework.testbed_model.node import Node
from framework.testbed_model.port import Port
from framework.testbed_model.topology import Topology
from framework.utils import get_packet_summaries


class TrafficGenerator(ABC):
    """The base traffic generator.

    Exposes the common public methods of all traffic generators and defines private methods
    that must implement the traffic generation logic in subclasses. This class also extends from
    :class:`framework.utils.MultiInheritanceBaseClass` to allow subclasses the ability to inherit
    from multiple classes to fulfil the traffic generating functionality without breaking
    single inheritance.
    """

    _config: TrafficGeneratorConfig
    _tg_node: Node
    _logger: DTSLogger

    def __init__(self, tg_node: Node, config: TrafficGeneratorConfig, **kwargs: Any) -> None:
        """Initialize the traffic generator.

        Additional keyword arguments can be passed through `kwargs` if needed for fulfilling other
        constructors in the case of multiple inheritance.

        Args:
            tg_node: The traffic generator node where the created traffic generator will be running.
            config: The traffic generator's test run configuration.
            **kwargs: Any additional arguments if any.
        """
        self._config = config
        self._tg_node = tg_node
        self._logger = get_dts_logger(f"{self._tg_node.name} {self._config.type}")

    def setup(self, topology: Topology) -> None:
        """Setup the traffic generator."""

    def teardown(self) -> None:
        """Teardown the traffic generator."""
        self.close()

    def send_packet(self, packet: Packet, port: Port) -> None:
        """Send `packet` and block until it is fully sent.

        Send `packet` on `port`, then wait until `packet` is fully sent.

        Args:
            packet: The packet to send.
            port: The egress port on the TG node.
        """
        self.send_packets([packet], port)

    def send_packets(self, packets: list[Packet], port: Port) -> None:
        """Send `packets` and block until they are fully sent.

        Send `packets` on `port`, then wait until `packets` are fully sent.

        Args:
            packets: The packets to send.
            port: The egress port on the TG node.
        """
        self._logger.info(f"Sending packet{'s' if len(packets) > 1 else ''}.")
        self._logger.debug(get_packet_summaries(packets))
        self._send_packets(packets, port)

    @abstractmethod
    def _send_packets(self, packets: list[Packet], port: Port) -> None:
        """The implementation of :method:`send_packets`.

        The subclasses must implement this method which sends `packets` on `port`.
        The method should block until all `packets` are fully sent.

        What fully sent means is defined by the traffic generator.
        """

    @property
    def is_capturing(self) -> bool:
        """This traffic generator can't capture traffic."""
        return False

    @abstractmethod
    def close(self) -> None:
        """Free all resources used by the traffic generator."""
