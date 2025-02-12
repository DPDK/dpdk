# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""NIC port model.

Basic port information, such as location (the port are identified by their PCI address on a node),
drivers and address.
"""

from dataclasses import dataclass

from framework.config.node import PortConfig


@dataclass(slots=True)
class Port:
    """Physical port on a node.

    The ports are identified by the node they're on and their PCI addresses. The port on the other
    side of the connection is also captured here.
    Each port is serviced by a driver, which may be different for the operating system (`os_driver`)
    and for DPDK (`os_driver_for_dpdk`). For some devices, they are the same, e.g.: ``mlx5_core``.

    Attributes:
        config: The port's configuration.
        mac_address: The MAC address of the port.
        logical_name: The logical name of the port. Must be discovered.
    """

    _node: str
    config: PortConfig
    mac_address: str = ""
    logical_name: str = ""

    def __init__(self, node_name: str, config: PortConfig):
        """Initialize the port from `node_name` and `config`.

        Args:
            node_name: The name of the port's node.
            config: The test run configuration of the port.
        """
        self._node = node_name
        self.config = config

    @property
    def node(self) -> str:
        """The node where the port resides."""
        return self._node

    @property
    def name(self) -> str:
        """The name of the port."""
        return self.config.name

    @property
    def pci(self) -> str:
        """The PCI address of the port."""
        return self.config.pci
