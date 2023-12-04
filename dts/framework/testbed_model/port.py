# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

from dataclasses import dataclass

from framework.config import PortConfig


@dataclass(slots=True, frozen=True)
class PortIdentifier:
    node: str
    pci: str


@dataclass(slots=True)
class Port:
    """
    identifier: The PCI address of the port on a node.

    os_driver: The driver used by this port when the OS is controlling it.
        Example: i40e
    os_driver_for_dpdk: The driver the device must be bound to for DPDK to use it,
        Example: vfio-pci.

    Note: os_driver and os_driver_for_dpdk may be the same thing.
        Example: mlx5_core

    peer: The identifier of a port this port is connected with.
    """

    identifier: PortIdentifier
    os_driver: str
    os_driver_for_dpdk: str
    peer: PortIdentifier
    mac_address: str = ""
    logical_name: str = ""

    def __init__(self, node_name: str, config: PortConfig):
        self.identifier = PortIdentifier(
            node=node_name,
            pci=config.pci,
        )
        self.os_driver = config.os_driver
        self.os_driver_for_dpdk = config.os_driver_for_dpdk
        self.peer = PortIdentifier(node=config.peer_node, pci=config.peer_pci)

    @property
    def node(self) -> str:
        return self.identifier.node

    @property
    def pci(self) -> str:
        return self.identifier.pci


@dataclass(slots=True, frozen=True)
class PortLink:
    sut_port: Port
    tg_port: Port
