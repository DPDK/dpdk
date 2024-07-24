# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Base interactive shell for DPDK applications.

Provides a base class to create interactive shells based on DPDK.
"""


from abc import ABC
from pathlib import PurePath

from framework.params.eal import EalParams
from framework.remote_session.single_active_interactive_shell import (
    SingleActiveInteractiveShell,
)
from framework.settings import SETTINGS
from framework.testbed_model.cpu import LogicalCoreCount, LogicalCoreList
from framework.testbed_model.sut_node import SutNode


def compute_eal_params(
    sut_node: SutNode,
    params: EalParams | None = None,
    lcore_filter_specifier: LogicalCoreCount | LogicalCoreList = LogicalCoreCount(),
    ascending_cores: bool = True,
    append_prefix_timestamp: bool = True,
) -> EalParams:
    """Compute EAL parameters based on the node's specifications.

    Args:
        sut_node: The SUT node to compute the values for.
        params: If :data:`None`, a new object is created and returned. Otherwise `params.lcore_list`
            is modified according to `lcore_filter_specifier`. A DPDK file prefix is also added. If
            `params.ports` is :data:`None`, then `sut_node.ports` is assigned to it.
        lcore_filter_specifier: A number of lcores/cores/sockets to use or a list of lcore ids to
            use. The default will select one lcore for each of two cores on one socket, in ascending
            order of core ids.
        ascending_cores: Sort cores in ascending order (lowest to highest IDs). If :data:`False`,
            sort in descending order.
        append_prefix_timestamp: If :data:`True`, will append a timestamp to DPDK file prefix.
    """
    if params is None:
        params = EalParams()

    if params.lcore_list is None:
        params.lcore_list = LogicalCoreList(
            sut_node.filter_lcores(lcore_filter_specifier, ascending_cores)
        )

    prefix = params.prefix
    if append_prefix_timestamp:
        prefix = f"{prefix}_{sut_node.dpdk_timestamp}"
    prefix = sut_node.main_session.get_dpdk_file_prefix(prefix)
    if prefix:
        sut_node.dpdk_prefix_list.append(prefix)
    params.prefix = prefix

    if params.ports is None:
        params.ports = sut_node.ports

    return params


class DPDKShell(SingleActiveInteractiveShell, ABC):
    """The base class for managing DPDK-based interactive shells.

    This class shouldn't be instantiated directly, but instead be extended.
    It automatically injects computed EAL parameters based on the node in the
    supplied app parameters.
    """

    _node: SutNode
    _app_params: EalParams

    def __init__(
        self,
        node: SutNode,
        privileged: bool = True,
        timeout: float = SETTINGS.timeout,
        lcore_filter_specifier: LogicalCoreCount | LogicalCoreList = LogicalCoreCount(),
        ascending_cores: bool = True,
        append_prefix_timestamp: bool = True,
        app_params: EalParams = EalParams(),
        name: str | None = None,
    ) -> None:
        """Extends :meth:`~.interactive_shell.InteractiveShell.__init__`.

        Adds the `lcore_filter_specifier`, `ascending_cores` and `append_prefix_timestamp` arguments
        which are then used to compute the EAL parameters based on the node's configuration.
        """
        app_params = compute_eal_params(
            node,
            app_params,
            lcore_filter_specifier,
            ascending_cores,
            append_prefix_timestamp,
        )

        super().__init__(node, privileged, timeout, app_params, name)

    def _update_real_path(self, path: PurePath) -> None:
        """Extends :meth:`~.interactive_shell.InteractiveShell._update_real_path`.

        Adds the remote DPDK build directory to the path.
        """
        super()._update_real_path(self._node.remote_dpdk_build_dir.joinpath(path))
