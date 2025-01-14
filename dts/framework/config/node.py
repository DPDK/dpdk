# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Configuration models representing a node.

The root model of a node configuration is :class:`NodeConfiguration`.
"""

from enum import Enum, auto, unique
from typing import Annotated, Literal

from pydantic import Field

from framework.utils import REGEX_FOR_PCI_ADDRESS, StrEnum

from .common import FrozenModel


@unique
class OS(StrEnum):
    r"""The supported operating systems of :class:`~framework.testbed_model.node.Node`\s."""

    #:
    linux = auto()
    #:
    freebsd = auto()
    #:
    windows = auto()


@unique
class TrafficGeneratorType(str, Enum):
    """The supported traffic generators."""

    #:
    SCAPY = "SCAPY"


class HugepageConfiguration(FrozenModel):
    r"""The hugepage configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The number of hugepages to allocate.
    number_of: int
    #: If :data:`True`, the hugepages will be configured on the first NUMA node.
    force_first_numa: bool


class PortConfig(FrozenModel):
    r"""The port configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The PCI address of the port.
    pci: str = Field(pattern=REGEX_FOR_PCI_ADDRESS)
    #: The driver that the kernel should bind this device to for DPDK to use it.
    os_driver_for_dpdk: str = Field(examples=["vfio-pci", "mlx5_core"])
    #: The operating system driver name when the operating system controls the port.
    os_driver: str = Field(examples=["i40e", "ice", "mlx5_core"])
    #: The name of the peer node this port is connected to.
    peer_node: str
    #: The PCI address of the peer port connected to this port.
    peer_pci: str = Field(pattern=REGEX_FOR_PCI_ADDRESS)


class TrafficGeneratorConfig(FrozenModel):
    """A protocol required to define traffic generator types."""

    #: The traffic generator type the child class is required to define to be distinguished among
    #: others.
    type: TrafficGeneratorType


class ScapyTrafficGeneratorConfig(TrafficGeneratorConfig):
    """Scapy traffic generator specific configuration."""

    type: Literal[TrafficGeneratorType.SCAPY]


#: A union type discriminating traffic generators by the `type` field.
TrafficGeneratorConfigTypes = Annotated[ScapyTrafficGeneratorConfig, Field(discriminator="type")]

#: Comma-separated list of logical cores to use. An empty string or ```any``` means use all lcores.
LogicalCores = Annotated[
    str,
    Field(
        examples=["1,2,3,4,5,18-22", "10-15", "any"],
        pattern=r"^(([0-9]+|([0-9]+-[0-9]+))(,([0-9]+|([0-9]+-[0-9]+)))*)?$|any",
    ),
]


class NodeConfiguration(FrozenModel):
    r"""The configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The name of the :class:`~framework.testbed_model.node.Node`.
    name: str
    #: The hostname of the :class:`~framework.testbed_model.node.Node`. Can also be an IP address.
    hostname: str
    #: The name of the user used to connect to the :class:`~framework.testbed_model.node.Node`.
    user: str
    #: The password of the user. The use of passwords is heavily discouraged, please use SSH keys.
    password: str | None = None
    #: The operating system of the :class:`~framework.testbed_model.node.Node`.
    os: OS
    #: An optional hugepage configuration.
    hugepages: HugepageConfiguration | None = Field(None, alias="hugepages_2mb")
    #: The ports that can be used in testing.
    ports: list[PortConfig] = Field(min_length=1)


class DPDKConfiguration(FrozenModel):
    """Configuration of the DPDK EAL parameters."""

    #: A comma delimited list of logical cores to use when running DPDK. ```any```, an empty
    #: string or omitting this field means use any core except for the first one. The first core
    #: will only be used if explicitly set.
    lcores: LogicalCores = ""

    #: The number of memory channels to use when running DPDK.
    memory_channels: int = 1

    @property
    def use_first_core(self) -> bool:
        """Returns :data:`True` if `lcores` explicitly selects the first core."""
        return "0" in self.lcores


class SutNodeConfiguration(NodeConfiguration):
    """:class:`~framework.testbed_model.sut_node.SutNode` specific configuration."""

    #: The runtime configuration for DPDK.
    dpdk_config: DPDKConfiguration


class TGNodeConfiguration(NodeConfiguration):
    """:class:`~framework.testbed_model.tg_node.TGNode` specific configuration."""

    #: The configuration of the traffic generator present on the TG node.
    traffic_generator: TrafficGeneratorConfigTypes


#: Union type for all the node configuration types.
NodeConfigurationTypes = TGNodeConfiguration | SutNodeConfiguration
