# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""Testbed topology representation.

A topology of a testbed captures what links are available between the testbed's nodes.
The link information then implies what type of topology is available.
"""

from collections import defaultdict
from collections.abc import Iterator
from dataclasses import dataclass
from enum import Enum
from typing import Literal, NamedTuple

from typing_extensions import Self

from framework.exception import ConfigurationError, InternalError
from framework.testbed_model.node import Node

from .port import DriverKind, Port


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


NodeIdentifier = Literal["sut", "tg"]
"""The node identifier."""


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

    def node_and_ports_from_id(self, node_identifier: NodeIdentifier) -> tuple[Node, list[Port]]:
        """Retrieve node and its ports for the current topology.

        Raises:
            InternalError: If the given `node_identifier` is invalid.
        """
        from framework.context import get_ctx

        ctx = get_ctx()
        match node_identifier:
            case "sut":
                return ctx.sut_node, self.sut_ports
            case "tg":
                return ctx.tg_node, self.tg_ports
            case _:
                msg = f"Invalid node `{node_identifier}` given."
                raise InternalError(msg)

    def setup(self) -> None:
        """Setup topology ports.

        Binds all the ports to the right kernel driver to retrieve MAC addresses and logical names.
        """
        self._setup_ports("sut")
        self._setup_ports("tg")

    def teardown(self) -> None:
        """Teardown topology ports.

        Restores all the ports to their original drivers before the test run.
        """
        self._restore_ports_original_drivers("sut")
        self._restore_ports_original_drivers("tg")

    def _restore_ports_original_drivers(self, node_identifier: NodeIdentifier) -> None:
        node, ports = self.node_and_ports_from_id(node_identifier)
        driver_to_ports: dict[str, list[Port]] = defaultdict(list)

        for port in ports:
            if port.original_driver is not None and port.original_driver != port.current_driver:
                driver_to_ports[port.original_driver].append(port)

        for driver_name, ports in driver_to_ports.items():
            node.main_session.bind_ports_to_driver(ports, driver_name)

    def _setup_ports(self, node_identifier: NodeIdentifier) -> None:
        node, ports = self.node_and_ports_from_id(node_identifier)

        self._bind_ports_to_drivers(node, ports, "kernel")

        for port in ports:
            if not (port.mac_address and port.logical_name):
                raise ConfigurationError(
                    "Could not gather a valid MAC address and/or logical name "
                    f"for port {port.name} in node {node.name}."
                )

    def configure_ports(
        self, node_identifier: NodeIdentifier, drivers: DriverKind | tuple[DriverKind, ...]
    ) -> None:
        """Configure the ports for the requested node as specified in `drivers`.

        Compares the current topology driver setup with the requested one and binds drivers only if
        needed. Moreover, it brings up the ports when using their kernel drivers.

        Args:
            node_identifier: The identifier of the node to gather the ports from.
            drivers: The driver kind(s) to bind. If a tuple is provided, each element corresponds to
                the driver for the respective port by index. Otherwise, if a driver kind is
                specified directly, this is applied to all the ports in the node.

        Raises:
            InternalError: If the number of given driver kinds is greater than the number of
                available topology ports.
        """
        node, ports = self.node_and_ports_from_id(node_identifier)

        if isinstance(drivers, tuple) and len(drivers) > len(ports):
            msg = "Too many ports have been specified."
            raise InternalError(msg)

        self._bind_ports_to_drivers(node, ports, drivers)

        ports_to_bring_up = [p for p in ports if not (p.bound_for_dpdk or p.is_link_up)]
        if ports_to_bring_up:
            node.main_session.bring_up_link(ports_to_bring_up)

    def _bind_ports_to_drivers(
        self, node: Node, ports: list[Port], drivers: DriverKind | tuple[DriverKind, ...]
    ) -> None:
        driver_to_ports: dict[str, list[Port]] = defaultdict(list)

        for port_id, port in enumerate(ports):
            driver_kind = drivers[port_id] if isinstance(drivers, tuple) else drivers
            desired_driver = port.driver_by_kind(driver_kind)
            if port.current_driver != desired_driver:
                driver_to_ports[desired_driver].append(port)

        for driver_name, ports in driver_to_ports.items():
            node.main_session.bind_ports_to_driver(ports, driver_name)

    @property
    def sut_dpdk_ports(self) -> list[Port]:
        """The DPDK ports for the SUT node."""
        return [port for port in self.sut_ports if port.bound_for_dpdk]

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
