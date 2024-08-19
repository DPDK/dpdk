# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""NIC port model.

Basic port information, such as location (the port are identified by their PCI address on a node),
drivers and address.
"""


from dataclasses import dataclass

from framework.config import PortConfig


@dataclass(slots=True, frozen=True)
class PortIdentifier:
    """The port identifier.

    Attributes:
        node: The node where the port resides.
        pci: The PCI address of the port on `node`.
    """

    node: str
    pci: str


@dataclass(slots=True)
class Port:
    """Physical port on a node.

    The ports are identified by the node they're on and their PCI addresses. The port on the other
    side of the connection is also captured here.
    Each port is serviced by a driver, which may be different for the operating system (`os_driver`)
    and for DPDK (`os_driver_for_dpdk`). For some devices, they are the same, e.g.: ``mlx5_core``.

    Attributes:
        identifier: The PCI address of the port on a node.
        os_driver: The operating system driver name when the operating system controls the port,
            e.g.: ``i40e``.
        os_driver_for_dpdk: The operating system driver name for use with DPDK, e.g.: ``vfio-pci``.
        peer: The identifier of a port this port is connected with.
            The `peer` is on a different node.
        mac_address: The MAC address of the port.
        logical_name: The logical name of the port. Must be discovered.
    """

    identifier: PortIdentifier
    os_driver: str
    os_driver_for_dpdk: str
    peer: PortIdentifier
    mac_address: str = ""
    logical_name: str = ""

    def __init__(self, node_name: str, config: PortConfig):
        """Initialize the port from `node_name` and `config`.

        Args:
            node_name: The name of the port's node.
            config: The test run configuration of the port.
        """
        self.identifier = PortIdentifier(
            node=node_name,
            pci=config.pci,
        )
        self.os_driver = config.os_driver
        self.os_driver_for_dpdk = config.os_driver_for_dpdk
        self.peer = PortIdentifier(node=config.peer_node, pci=config.peer_pci)

    @property
    def node(self) -> str:
        """The node where the port resides."""
        return self.identifier.node

    @property
    def pci(self) -> str:
        """The PCI address of the port."""
        return self.identifier.pci


@dataclass(slots=True, frozen=True)
class PortLink:
    """The physical, cabled connection between the ports.

    Attributes:
        sut_port: The port on the SUT node connected to `tg_port`.
        tg_port: The port on the TG node connected to `sut_port`.
    """

    sut_port: Port
    tg_port: Port
