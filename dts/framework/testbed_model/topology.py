# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""Testbed topology representation.

A topology of a testbed captures what links are available between the testbed's nodes.
The link information then implies what type of topology is available.
"""

from collections.abc import Iterator
from dataclasses import dataclass
from enum import Enum
from typing import NamedTuple

from typing_extensions import Self

from framework.exception import ConfigurationError

from .port import Port


class TopologyType(int, Enum):
    """Supported topology types."""

    #: A topology with no Traffic Generator.
    no_link = 0
    #: A topology with one physical link between the SUT node and the TG node.
    one_link = 1
    #: A topology with two physical links between the Sut node and the TG node.
    two_links = 2

    @classmethod
    def default(cls) -> "TopologyType":
        """The default topology required by test cases if not specified otherwise."""
        return cls.two_links


class PortLink(NamedTuple):
    """The physical, cabled connection between the ports."""

    #: The port on the SUT node connected to `tg_port`.
    sut_port: Port
    #: The port on the TG node connected to `sut_port`.
    tg_port: Port


@dataclass(frozen=True)
class Topology:
    """Testbed topology.

    The topology contains ports processed into ingress and egress ports.
    If there are no ports on a node, accesses to :attr:`~Topology.tg_port_egress` and alike will
    raise an exception. If there is only one link available, the ports of this link are stored
    as both ingress and egress ports.

    It's up to :class:`~framework.test_run.TestRun` to ensure no test case or suite requiring actual
    links is executed when the topology prohibits it and up to the developers to make sure that test
    cases not requiring any links don't use any ports. Otherwise, the underlying methods using the
    ports will fail.

    Attributes:
        type: The type of the topology.
        sut_ports: The SUT ports.
        tg_ports: The TG ports.
    """

    type: TopologyType
    sut_ports: list[Port]
    tg_ports: list[Port]

    @classmethod
    def from_port_links(cls, port_links: Iterator[PortLink]) -> Self:
        """Create the topology from `port_links`.

        Args:
            port_links: The test run's required port links.

        Raises:
            ConfigurationError: If an unsupported link topology is supplied.
        """
        type = TopologyType.no_link

        if port_link := next(port_links, None):
            type = TopologyType.one_link
            sut_ports = [port_link.sut_port]
            tg_ports = [port_link.tg_port]

            if port_link := next(port_links, None):
                type = TopologyType.two_links
                sut_ports.append(port_link.sut_port)
                tg_ports.append(port_link.tg_port)

                if next(port_links, None) is not None:
                    msg = "More than two links in a topology are not supported."
                    raise ConfigurationError(msg)

        return cls(type, sut_ports, tg_ports)

    @property
    def tg_port_egress(self) -> Port:
        """The egress port of the TG node."""
        return self.tg_ports[0]

    @property
    def sut_port_ingress(self) -> Port:
        """The ingress port of the SUT node."""
        return self.sut_ports[0]

    @property
    def sut_port_egress(self) -> Port:
        """The egress port of the SUT node."""
        return self.sut_ports[1 if self.type is TopologyType.two_links else 0]

    @property
    def tg_port_ingress(self) -> Port:
        """The ingress port of the TG node."""
        return self.tg_ports[1 if self.type is TopologyType.two_links else 0]
