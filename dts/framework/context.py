# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Runtime contexts."""

import functools
from dataclasses import MISSING, dataclass, field, fields
from typing import TYPE_CHECKING, ParamSpec, Union

from framework.exception import InternalError
from framework.remote_session.shell_pool import ShellPool
from framework.settings import SETTINGS
from framework.testbed_model.cpu import LogicalCoreCount, LogicalCoreList
from framework.testbed_model.node import Node
from framework.testbed_model.topology import Topology

if TYPE_CHECKING:
    from framework.remote_session.dpdk import DPDKBuildEnvironment, DPDKRuntimeEnvironment
    from framework.test_suite import TestCase, TestSuite
    from framework.testbed_model.traffic_generator.traffic_generator import TrafficGenerator

P = ParamSpec("P")


@dataclass
class LocalContext:
    """Updatable context local to test suites and cases.

    Attributes:
        current_test_suite: The currently running test suite, if any.
        current_test_case: The currently running test case, if any.
        lcore_filter_specifier: A number of lcores/cores/sockets to use or a list of lcore ids to
            use. The default will select one lcore for each of two cores on one socket, in ascending
            order of core ids.
        ascending_cores: Sort cores in ascending order (lowest to highest IDs). If :data:`False`,
            sort in descending order.
        append_prefix_timestamp: If :data:`True`, will append a timestamp to DPDK file prefix.
        timeout: The timeout used for the SSH channel that is dedicated to this interactive
            shell. This timeout is for collecting output, so if reading from the buffer
            and no output is gathered within the timeout, an exception is thrown.
    """

    current_test_suite: Union["TestSuite", None] = None
    current_test_case: Union[type["TestCase"], None] = None
    lcore_filter_specifier: LogicalCoreCount | LogicalCoreList = field(
        default_factory=LogicalCoreCount
    )
    ascending_cores: bool = True
    append_prefix_timestamp: bool = True
    timeout: float = SETTINGS.timeout

    def reset(self) -> None:
        """Reset the local context to the default values."""
        for _field in fields(LocalContext):
            default = (
                _field.default_factory()
                if _field.default_factory is not MISSING
                else _field.default
            )

            assert (
                default is not MISSING
            ), "{LocalContext.__name__} must have defaults on all fields!"

            setattr(self, _field.name, default)


@dataclass(frozen=True)
class Context:
    """Runtime context."""

    sut_node: Node
    tg_node: Node
    topology: Topology
    dpdk_build: "DPDKBuildEnvironment"
    dpdk: "DPDKRuntimeEnvironment"
    tg: "TrafficGenerator"
    local: LocalContext = field(default_factory=LocalContext)
    shell_pool: ShellPool = field(default_factory=ShellPool)


__current_ctx: Context | None = None


def get_ctx() -> Context:
    """Retrieve the current runtime context.

    Raises:
        InternalError: If there is no context.
    """
    if __current_ctx:
        return __current_ctx

    raise InternalError("Attempted to retrieve context that has not been initialized yet.")


def init_ctx(ctx: Context) -> None:
    """Initialize context."""
    global __current_ctx
    __current_ctx = ctx


def filter_cores(
    specifier: LogicalCoreCount | LogicalCoreList, ascending_cores: bool | None = None
):
    """Decorates functions that require a temporary update to the lcore specifier."""

    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs):
            local_ctx = get_ctx().local

            old_specifier = local_ctx.lcore_filter_specifier
            local_ctx.lcore_filter_specifier = specifier
            if ascending_cores is not None:
                old_ascending_cores = local_ctx.ascending_cores
                local_ctx.ascending_cores = ascending_cores

            try:
                return func(*args, **kwargs)
            finally:
                local_ctx.lcore_filter_specifier = old_specifier
                if ascending_cores is not None:
                    local_ctx.ascending_cores = old_ascending_cores

        return wrapper

    return decorator
