# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Module representing the DPDK EAL-related parameters."""

from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Literal

from framework.params import Params, Switch
from framework.testbed_model.cpu import LogicalCoreList
from framework.testbed_model.virtual_device import VirtualDevice

if TYPE_CHECKING:
    from framework.testbed_model.port import Port


def _port_to_pci(port: "Port") -> str:
    return port.pci


@dataclass(kw_only=True)
class EalParams(Params):
    """The environment abstraction layer parameters.

    Attributes:
        lcore_list: The list of logical cores to use.
        memory_channels: The number of memory channels to use.
        prefix: Set the file prefix string with which to start DPDK, e.g.: ``prefix="vf"``.
        no_pci: Switch to disable PCI bus, e.g.: ``no_pci=True``.
        vdevs: Virtual devices, e.g.::

            vdevs = [VirtualDevice("net_ring0"), VirtualDevice("net_ring1")]

        ports: The list of ports to allow.
        other_eal_param: user defined DPDK EAL parameters, e.g.::

            ``other_eal_param='--single-file-segments'``
    """

    lcore_list: LogicalCoreList | None = field(default=None, metadata=Params.short("l"))
    memory_channels: int | None = field(default=None, metadata=Params.short("n"))
    prefix: str = field(default="dpdk", metadata=Params.long("file-prefix"))
    no_pci: Switch = None
    vdevs: list[VirtualDevice] | None = field(
        default=None, metadata=Params.multiple() | Params.long("vdev")
    )
    allowed_ports: list["Port"] | None = field(
        default=None,
        metadata=Params.convert_value(_port_to_pci) | Params.multiple() | Params.short("a"),
    )
    blocked_ports: list["Port"] | None = field(
        default=None,
        metadata=Params.convert_value(_port_to_pci) | Params.multiple() | Params.short("b"),
    )
    other_eal_param: Params | None = None
    _separator: Literal[True] = field(default=True, init=False, metadata=Params.short("-"))
