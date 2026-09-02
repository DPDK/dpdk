# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Runtime contexts."""

from collections.abc import Callable

from api.testbed_model.capability import TestProtocol
from framework.context import Context
from framework.context import filter_cores as _filter_cores
from framework.context import get_ctx as _get_ctx
from framework.context import init_ctx as _init_ctx
from framework.testbed_model.cpu import LogicalCoreCount, LogicalCoreList


def get_ctx() -> Context:
    """Retrieve the current runtime context."""
    return _get_ctx()


def init_ctx(ctx: Context) -> None:
    """Initialize context."""
    _init_ctx(ctx)


def filter_cores(
    specifier: LogicalCoreCount | LogicalCoreList, ascending_cores: bool | None = None
) -> Callable[[type["TestProtocol"]], Callable]:
    """Decorates functions that require a temporary update to the lcore specifier."""
    return _filter_cores(specifier, ascending_cores)
