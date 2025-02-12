# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""NIC port model.

Basic port information, such as location (the port are identified by their PCI address on a node),
drivers and address.
"""

from typing import TYPE_CHECKING, Any, Final

from framework.config.node import PortConfig

if TYPE_CHECKING:
    from .node import Node


class Port:
    """Physical port on a node.

    Attributes:
        node: The port's node.
        config: The port's configuration.
        mac_address: The MAC address of the port.
        logical_name: The logical name of the port.
        bound_for_dpdk: :data:`True` if the port is bound to the driver for DPDK.
    """

    node: Final["Node"]
    config: Final[PortConfig]
    mac_address: Final[str]
    logical_name: Final[str]
    bound_for_dpdk: bool

    def __init__(self, node: "Node", config: PortConfig):
        """Initialize the port from `node` and `config`.

        Args:
            node: The port's node.
            config: The test run configuration of the port.
        """
        self.node = node
        self.config = config
        self.logical_name, self.mac_address = node.main_session.get_port_info(config.pci)
        self.bound_for_dpdk = False

    @property
    def name(self) -> str:
        """The name of the port."""
        return self.config.name

    @property
    def pci(self) -> str:
        """The PCI address of the port."""
        return self.config.pci

    def configure_mtu(self, mtu: int):
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
