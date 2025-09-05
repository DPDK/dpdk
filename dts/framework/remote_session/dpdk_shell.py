# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Base interactive shell for DPDK applications.

Provides a base class to create interactive shells based on DPDK.
"""

from abc import ABC, abstractmethod
from pathlib import PurePath

from framework.context import get_ctx
from framework.params.eal import EalParams
from framework.remote_session.interactive_shell import (
    InteractiveShell,
)
from framework.testbed_model.cpu import LogicalCoreList


def compute_eal_params(
    params: EalParams | None = None,
) -> EalParams:
    """Compute EAL parameters based on the node's specifications.

    Args:
        params: If :data:`None`, a new object is created and returned. Otherwise `params.lcore_list`
            is modified according to `lcore_filter_specifier`. A DPDK file prefix is also added. If
            `params.ports` is :data:`None`, then `sut_node.ports` is assigned to it.
    """
    ctx = get_ctx()

    if params is None:
        params = EalParams()

    if params.lcore_list is None:
        params.lcore_list = LogicalCoreList(
            ctx.dpdk.filter_lcores(ctx.local.lcore_filter_specifier, ctx.local.ascending_cores)
        )

    prefix = params.prefix
    if ctx.local.append_prefix_timestamp:
        prefix = f"{prefix}_{ctx.dpdk.timestamp}"
    prefix = ctx.sut_node.main_session.get_dpdk_file_prefix(prefix)
    if prefix:
        ctx.dpdk.prefix_list.append(prefix)
    params.prefix = prefix

    if params.allowed_ports is None:
        params.allowed_ports = ctx.topology.sut_dpdk_ports

    return params


class DPDKShell(InteractiveShell, ABC):
    """The base class for managing DPDK-based interactive shells.

    This class shouldn't be instantiated directly, but instead be extended.
    It automatically injects computed EAL parameters based on the node in the
    supplied app parameters.
    """

    _app_params: EalParams

    def __init__(
        self,
        name: str | None = None,
        privileged: bool = True,
        app_params: EalParams = EalParams(),
    ) -> None:
        """Extends :meth:`~.interactive_shell.InteractiveShell.__init__`."""
        app_params = compute_eal_params(app_params)
        node = get_ctx().sut_node

        super().__init__(node, name, privileged, app_params)

    @property
    @abstractmethod
    def path(self) -> PurePath:
        """Relative path to the shell executable from the build folder."""

    def _make_real_path(self) -> PurePath:
        """Overrides :meth:`~.interactive_shell.InteractiveShell._make_real_path`.

        Adds the remote DPDK build directory to the path.
        """
        return get_ctx().dpdk_build.remote_dpdk_build_dir.joinpath(self.path)
