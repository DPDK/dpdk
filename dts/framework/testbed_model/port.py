# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""NIC port model.

Basic port information, such as location (the port are identified by their PCI address on a node),
drivers and address.
"""

from functools import cached_property
from typing import TYPE_CHECKING, Any, Final, Literal, NamedTuple

from framework.config.node import PortConfig
from framework.exception import InternalError

if TYPE_CHECKING:
    from .node import Node

DriverKind = Literal["kernel", "dpdk"]
"""The driver kind."""


class PortInfo(NamedTuple):
    """Port information.

    Attributes:
        mac_address: The MAC address of the port.
        logical_name: The logical name of the port.
        driver: The name of the port's driver.
    """

    mac_address: str
    logical_name: str
    driver: str
    is_link_up: bool


class Port:
    """Physical port on a node.

    Attributes:
        node: The port's node.
        config: The port's configuration.
    """

    node: Final["Node"]
    config: Final[PortConfig]
    _original_driver: str | None

    def __init__(self, node: "Node", config: PortConfig) -> None:
        """Initialize the port from `node` and `config`.

        Args:
            node: The port's node.
            config: The test run configuration of the port.
        """
        self.node = node
        self.config = config
        self._original_driver = None

    def driver_by_kind(self, kind: DriverKind) -> str:
        """Retrieve the driver name by kind.

        Raises:
            InternalError: If the given `kind` is invalid.
        """
        match kind:
            case "dpdk":
                return self.config.os_driver_for_dpdk
            case "kernel":
                return self.config.os_driver
            case _:
                msg = f"Invalid driver kind `{kind}` given."
                raise InternalError(msg)

    @property
    def name(self) -> str:
        """The name of the port."""
        return self.config.name

    @property
    def pci(self) -> str:
        """The PCI address of the port."""
        return self.config.pci

    @property
    def info(self) -> PortInfo:
        """The port's current system information.

        When this is accessed for the first time, the port's original driver is stored.
        """
        info = self.node.main_session.get_port_info(self.pci)

        if self._original_driver is None:
            self._original_driver = info.driver

        return info

    @cached_property
    def mac_address(self) -> str:
        """The MAC address of the port."""
        return self.info.mac_address

    @cached_property
    def logical_name(self) -> str:
        """The logical name of the port."""
        return self.info.logical_name

    @property
    def is_link_up(self) -> bool:
        """Is the port link up?"""
        return self.info.is_link_up

    @property
    def current_driver(self) -> str:
        """The current driver of the port."""
        return self.info.driver

    @property
    def original_driver(self) -> str | None:
        """The original driver of the port prior to DTS startup."""
        return self._original_driver

    @property
    def bound_for_dpdk(self) -> bool:
        """Is the port bound to the driver for DPDK?"""
        return self.current_driver == self.config.os_driver_for_dpdk

    def configure_mtu(self, mtu: int) -> None:
        """Configure the port's MTU value.

        Args:
            mtu: Desired MTU value.
        """
        return self.node.main_session.configure_port_mtu(mtu, self)

    def to_dict(self) -> dict[str, Any]:
        """Convert to a dictionary."""
        return {
            "node_name": self.node.name,
            "name": self.name,
            "pci": self.pci,
            "mac_address": self.mac_address,
            "logical_name": self.logical_name,
        }
