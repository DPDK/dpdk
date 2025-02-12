# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""Testbed topology representation.

A topology of a testbed captures what links are available between the testbed's nodes.
The link information then implies what type of topology is available.
"""

from collections.abc import Iterator
from enum import Enum
from typing import NamedTuple

from framework.config.node import PortConfig
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


class Topology:
    """Testbed topology.

    The topology contains ports processed into ingress and egress ports.
    If there are no ports on a node, dummy ports (ports with no actual values) are stored.
    If there is only one link available, the ports of this link are stored
    as both ingress and egress ports.

    The dummy ports shouldn't be used. It's up to :class:`~framework.runner.DTSRunner`
    to ensure no test case or suite requiring actual links is executed
    when the topology prohibits it and up to the developers to make sure that test cases
    not requiring any links don't use any ports. Otherwise, the underlying methods
    using the ports will fail.

    Attributes:
        type: The type of the topology.
        tg_port_egress: The egress port of the TG node.
        sut_port_ingress: The ingress port of the SUT node.
        sut_port_egress: The egress port of the SUT node.
        tg_port_ingress: The ingress port of the TG node.
    """

    type: TopologyType
    tg_port_egress: Port
    sut_port_ingress: Port
    sut_port_egress: Port
    tg_port_ingress: Port

    def __init__(self, port_links: Iterator[PortLink]):
        """Create the topology from `port_links`.

        Args:
            port_links: The test run's required port links.

        Raises:
            ConfigurationError: If an unsupported link topology is supplied.
        """
        dummy_port = Port(
            "",
            PortConfig(
                name="dummy",
                pci="0000:00:00.0",
                os_driver_for_dpdk="",
                os_driver="",
            ),
        )

        self.type = TopologyType.no_link
        self.tg_port_egress = dummy_port
        self.sut_port_ingress = dummy_port
        self.sut_port_egress = dummy_port
        self.tg_port_ingress = dummy_port

        if port_link := next(port_links, None):
            self.type = TopologyType.one_link
            self.tg_port_egress = port_link.tg_port
            self.sut_port_ingress = port_link.sut_port
            self.sut_port_egress = self.sut_port_ingress
            self.tg_port_ingress = self.tg_port_egress

            if port_link := next(port_links, None):
                self.type = TopologyType.two_links
                self.sut_port_egress = port_link.sut_port
                self.tg_port_ingress = port_link.tg_port

                if next(port_links, None) is not None:
                    msg = "More than two links in a topology are not supported."
                    raise ConfigurationError(msg)
