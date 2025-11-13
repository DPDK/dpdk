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
    _is_setup: bool

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
        self._is_setup = False

    def send_packets(self, packets: list[Packet], port: Port) -> None:
        """Send `packets` and block until they are fully sent.

        Send `packets` on `port`, then wait until `packets` are fully sent.

        Args:
            packets: The packets to send.
            port: The egress port on the TG node.
        """

    def setup(self, topology: Topology) -> None:
        """Setup the traffic generator."""
        self._is_setup = True

    def teardown(self) -> None:
        """Teardown the traffic generator."""
        self._is_setup = False
        self.close()

    @property
    def is_capturing(self) -> bool:
        """This traffic generator can't capture traffic."""
        return False

    @abstractmethod
    def close(self) -> None:
        """Free all resources used by the traffic generator."""

    @property
    def is_setup(self) -> bool:
        """Indicates whether the traffic generator application is currently running."""
        return self._is_setup
