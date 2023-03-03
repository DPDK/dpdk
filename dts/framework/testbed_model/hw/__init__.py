# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

# pylama:ignore=W0611

from .cpu import (
    LogicalCore,
    LogicalCoreCount,
    LogicalCoreCountFilter,
    LogicalCoreFilter,
    LogicalCoreList,
    LogicalCoreListFilter,
)
from .virtual_device import VirtualDevice


def lcore_filter(
    core_list: list[LogicalCore],
    filter_specifier: LogicalCoreCount | LogicalCoreList,
    ascending: bool,
) -> LogicalCoreFilter:
    if isinstance(filter_specifier, LogicalCoreList):
        return LogicalCoreListFilter(core_list, filter_specifier, ascending)
    elif isinstance(filter_specifier, LogicalCoreCount):
        return LogicalCoreCountFilter(core_list, filter_specifier, ascending)
    else:
        raise ValueError(f"Unsupported filter r{filter_specifier}")
