# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Configuration models representing a node.

The root model of a node configuration is :class:`NodeConfiguration`.
"""

from enum import auto, unique

from pydantic import Field, model_validator
from typing_extensions import Self

from framework.utils import REGEX_FOR_IDENTIFIER, REGEX_FOR_PCI_ADDRESS, StrEnum

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


class HugepageConfiguration(FrozenModel):
    r"""The hugepage configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The number of hugepages to allocate.
    number_of: int
    #: If :data:`True`, the hugepages will be configured on the first NUMA node.
    force_first_numa: bool


class PortConfig(FrozenModel):
    r"""The port configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: An identifier for the port. May contain letters, digits, underscores, hyphens and spaces.
    name: str = Field(pattern=REGEX_FOR_IDENTIFIER)
    #: The PCI address of the port.
    pci: str = Field(pattern=REGEX_FOR_PCI_ADDRESS)
    #: The driver that the kernel should bind this device to for DPDK to use it.
    os_driver_for_dpdk: str = Field(examples=["vfio-pci", "mlx5_core"])
    #: The operating system driver name when the operating system controls the port.
    os_driver: str = Field(examples=["i40e", "ice", "mlx5_core"])


class NodeConfiguration(FrozenModel):
    r"""The configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The name of the :class:`~framework.testbed_model.node.Node`.
    name: str = Field(pattern=REGEX_FOR_IDENTIFIER)
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

    @model_validator(mode="after")
    def verify_unique_port_names(self) -> Self:
        """Verify that there are no ports with the same name."""
        used_port_names: dict[str, int] = {}
        for idx, port in enumerate(self.ports):
            assert port.name not in used_port_names, (
                f"Cannot use port name '{port.name}' for ports.{idx}. "
                f"This was already used in ports.{used_port_names[port.name]}."
            )
            used_port_names[port.name] = idx
        return self
