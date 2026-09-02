# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""CPU core representation and filtering.

This module provides a unified representation of logical CPU cores along
with filtering capabilities.

When symmetric multiprocessing (SMP or multithreading) is enabled on a server,
the physical CPU cores are split into logical CPU cores with different IDs.

:class:`LogicalCoreCountFilter` filters by the number of logical cores. It's possible to specify
the socket from which to filter the number of logical cores. It's also possible to not use all
logical CPU cores from each physical core (e.g. only the first logical core of each physical core).

:class:`LogicalCoreListFilter` filters by logical core IDs. This mostly checks that
the logical cores are actually present on the server.
"""

from framework.testbed_model.cpu import LogicalCoreCount as LocgicalCoreCountBase
from framework.testbed_model.cpu import LogicalCoreList as LogicalCoreListBase


class LogicalCoreList(LogicalCoreListBase):
    r"""A unified way to store :class:`LogicalCore`\s.

    Create a unified format used across the framework and allow the user to use
    either a :class:`str` representation (using ``str(instance)`` or directly in f-strings)
    or a :class:`list` representation (by accessing the `lcore_list` property,
    which stores logical core IDs).
    """


class LogicalCoreCount(LocgicalCoreCountBase):
    """Define the number of logical cores per physical cores per sockets."""
