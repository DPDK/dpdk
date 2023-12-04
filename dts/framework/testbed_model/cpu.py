# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

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

import dataclasses
from abc import ABC, abstractmethod
from collections.abc import Iterable, ValuesView
from dataclasses import dataclass

from framework.utils import expand_range


@dataclass(slots=True, frozen=True)
class LogicalCore(object):
    """Representation of a logical CPU core.

    A physical core is represented in OS by multiple logical cores (lcores)
    if CPU multithreading is enabled. When multithreading is disabled, their IDs are the same.

    Attributes:
        lcore: The logical core ID of a CPU core. It's the same as `core` with
            disabled multithreading.
        core: The physical core ID of a CPU core.
        socket: The physical socket ID where the CPU resides.
        node: The NUMA node ID where the CPU resides.
    """

    lcore: int
    core: int
    socket: int
    node: int

    def __int__(self) -> int:
        """The CPU is best represented by the logical core, as that's what we configure in EAL."""
        return self.lcore


class LogicalCoreList(object):
    r"""A unified way to store :class:`LogicalCore`\s.

    Create a unified format used across the framework and allow the user to use
    either a :class:`str` representation (using ``str(instance)`` or directly in f-strings)
    or a :class:`list` representation (by accessing the `lcore_list` property,
    which stores logical core IDs).
    """

    _lcore_list: list[int]
    _lcore_str: str

    def __init__(self, lcore_list: list[int] | list[str] | list[LogicalCore] | str):
        """Process `lcore_list`, then sort.

        There are four supported logical core list formats::

            lcore_list=[LogicalCore1, LogicalCore2]  # a list of LogicalCores
            lcore_list=[0,1,2,3]        # a list of int indices
            lcore_list=['0','1','2-3']  # a list of str indices; ranges are supported
            lcore_list='0,1,2-3'        # a comma delimited str of indices; ranges are supported

        Args:
            lcore_list: Various ways to represent multiple logical cores.
                Empty `lcore_list` is allowed.
        """
        self._lcore_list = []
        if isinstance(lcore_list, str):
            lcore_list = lcore_list.split(",")
        for lcore in lcore_list:
            if isinstance(lcore, str):
                self._lcore_list.extend(expand_range(lcore))
            else:
                self._lcore_list.append(int(lcore))

        # the input lcores may not be sorted
        self._lcore_list.sort()
        self._lcore_str = f'{",".join(self._get_consecutive_lcores_range(self._lcore_list))}'

    @property
    def lcore_list(self) -> list[int]:
        """The logical core IDs."""
        return self._lcore_list

    def _get_consecutive_lcores_range(self, lcore_ids_list: list[int]) -> list[str]:
        formatted_core_list = []
        segment = lcore_ids_list[:1]
        for lcore_id in lcore_ids_list[1:]:
            if lcore_id - segment[-1] == 1:
                segment.append(lcore_id)
            else:
                formatted_core_list.append(
                    f"{segment[0]}-{segment[-1]}" if len(segment) > 1 else f"{segment[0]}"
                )
                current_core_index = lcore_ids_list.index(lcore_id)
                formatted_core_list.extend(
                    self._get_consecutive_lcores_range(lcore_ids_list[current_core_index:])
                )
                segment.clear()
                break
        if len(segment) > 0:
            formatted_core_list.append(
                f"{segment[0]}-{segment[-1]}" if len(segment) > 1 else f"{segment[0]}"
            )
        return formatted_core_list

    def __str__(self) -> str:
        """The consecutive ranges of logical core IDs."""
        return self._lcore_str


@dataclasses.dataclass(slots=True, frozen=True)
class LogicalCoreCount(object):
    """Define the number of logical cores per physical cores per sockets."""

    #: Use this many logical cores per each physical core.
    lcores_per_core: int = 1
    #: Use this many physical cores per each socket.
    cores_per_socket: int = 2
    #: Use this many sockets.
    socket_count: int = 1
    #: Use exactly these sockets. This takes precedence over `socket_count`,
    #: so when `sockets` is not :data:`None`, `socket_count` is ignored.
    sockets: list[int] | None = None


class LogicalCoreFilter(ABC):
    """Common filtering class.

    Each filter needs to be implemented in a subclass. This base class sorts the list of cores
    and defines the filtering method, which must be implemented by subclasses.
    """

    _filter_specifier: LogicalCoreCount | LogicalCoreList
    _lcores_to_filter: list[LogicalCore]

    def __init__(
        self,
        lcore_list: list[LogicalCore],
        filter_specifier: LogicalCoreCount | LogicalCoreList,
        ascending: bool = True,
    ):
        """Filter according to the input filter specifier.

        The input `lcore_list` is copied and sorted by physical core before filtering.
        The list is copied so that the original is left intact.

        Args:
            lcore_list: The logical CPU cores to filter.
            filter_specifier: Filter cores from `lcore_list` according to this filter.
            ascending: Sort cores in ascending order (lowest to highest IDs). If data:`False`,
                sort in descending order.
        """
        self._filter_specifier = filter_specifier

        # sorting by core is needed in case hyperthreading is enabled
        self._lcores_to_filter = sorted(lcore_list, key=lambda x: x.core, reverse=not ascending)
        self.filter()

    @abstractmethod
    def filter(self) -> list[LogicalCore]:
        r"""Filter the cores.

        Use `self._filter_specifier` to filter `self._lcores_to_filter` and return
        the filtered :class:`LogicalCore`\s.
        `self._lcores_to_filter` is a sorted copy of the original list, so it may be modified.

        Returns:
            The filtered cores.
        """


class LogicalCoreCountFilter(LogicalCoreFilter):
    """Filter cores by specified counts.

    Filter the input list of LogicalCores according to specified rules:

        * The input `filter_specifier` is :class:`LogicalCoreCount`,
        * Use cores from the specified number of sockets or from the specified socket ids,
        * If `sockets` is specified, it takes precedence over `socket_count`,
        * From each of those sockets, use only `cores_per_socket` of cores,
        * And for each core, use `lcores_per_core` of logical cores. Hypertheading
          must be enabled for this to take effect.
    """

    _filter_specifier: LogicalCoreCount

    def filter(self) -> list[LogicalCore]:
        """Filter the cores according to :class:`LogicalCoreCount`.

        Start by filtering the allowed sockets. The cores matching the allowed sockets are returned.
        The cores of each socket are stored in separate lists.

        Then filter the allowed physical cores from those lists of cores per socket. When filtering
        physical cores, store the desired number of logical cores per physical core which then
        together constitute the final filtered list.

        Returns:
            The filtered cores.
        """
        sockets_to_filter = self._filter_sockets(self._lcores_to_filter)
        filtered_lcores = []
        for socket_to_filter in sockets_to_filter:
            filtered_lcores.extend(self._filter_cores_from_socket(socket_to_filter))
        return filtered_lcores

    def _filter_sockets(
        self, lcores_to_filter: Iterable[LogicalCore]
    ) -> ValuesView[list[LogicalCore]]:
        """Filter a list of cores per each allowed socket.

        The sockets may be specified in two ways, either a number or a specific list of sockets.
        In case of a specific list, we just need to return the cores from those sockets.
        If filtering a number of cores, we need to go through all cores and note which sockets
        appear and only filter from the first n that appear.

        Args:
            lcores_to_filter: The cores to filter. These must be sorted by the physical core.

        Returns:
            A list of lists of logical CPU cores. Each list contains cores from one socket.
        """
        allowed_sockets: set[int] = set()
        socket_count = self._filter_specifier.socket_count
        if self._filter_specifier.sockets:
            # when sockets in filter is specified, the sockets are already set
            socket_count = len(self._filter_specifier.sockets)
            allowed_sockets = set(self._filter_specifier.sockets)

        # filter socket_count sockets from all sockets by checking the socket of each CPU
        filtered_lcores: dict[int, list[LogicalCore]] = {}
        for lcore in lcores_to_filter:
            if not self._filter_specifier.sockets:
                # this is when sockets is not set, so we do the actual filtering
                # when it is set, allowed_sockets is already defined and can't be changed
                if len(allowed_sockets) < socket_count:
                    # allowed_sockets is a set, so adding an existing socket won't re-add it
                    allowed_sockets.add(lcore.socket)
            if lcore.socket in allowed_sockets:
                # separate lcores into sockets; this makes it easier in further processing
                if lcore.socket in filtered_lcores:
                    filtered_lcores[lcore.socket].append(lcore)
                else:
                    filtered_lcores[lcore.socket] = [lcore]

        if len(allowed_sockets) < socket_count:
            raise ValueError(
                f"The actual number of sockets from which to use cores "
                f"({len(allowed_sockets)}) is lower than required ({socket_count})."
            )

        return filtered_lcores.values()

    def _filter_cores_from_socket(
        self, lcores_to_filter: Iterable[LogicalCore]
    ) -> list[LogicalCore]:
        """Filter a list of cores from the given socket.

        Go through the cores and note how many logical cores per physical core have been filtered.

        Returns:
            The filtered logical CPU cores.
        """
        # no need to use ordered dict, from Python3.7 the dict
        # insertion order is preserved (LIFO).
        lcore_count_per_core_map: dict[int, int] = {}
        filtered_lcores = []
        for lcore in lcores_to_filter:
            if lcore.core in lcore_count_per_core_map:
                current_core_lcore_count = lcore_count_per_core_map[lcore.core]
                if self._filter_specifier.lcores_per_core > current_core_lcore_count:
                    # only add lcores of the given core
                    lcore_count_per_core_map[lcore.core] += 1
                    filtered_lcores.append(lcore)
                else:
                    # we have enough lcores per this core
                    continue
            elif self._filter_specifier.cores_per_socket > len(lcore_count_per_core_map):
                # only add cores if we need more
                lcore_count_per_core_map[lcore.core] = 1
                filtered_lcores.append(lcore)
            else:
                # we have enough cores
                break

        cores_per_socket = len(lcore_count_per_core_map)
        if cores_per_socket < self._filter_specifier.cores_per_socket:
            raise ValueError(
                f"The actual number of cores per socket ({cores_per_socket}) "
                f"is lower than required ({self._filter_specifier.cores_per_socket})."
            )

        lcores_per_core = lcore_count_per_core_map[filtered_lcores[-1].core]
        if lcores_per_core < self._filter_specifier.lcores_per_core:
            raise ValueError(
                f"The actual number of logical cores per core ({lcores_per_core}) "
                f"is lower than required ({self._filter_specifier.lcores_per_core})."
            )

        return filtered_lcores


class LogicalCoreListFilter(LogicalCoreFilter):
    """Filter the logical CPU cores by logical CPU core IDs.

    This is a simple filter that looks at logical CPU IDs and only filter those that match.

    The input filter is :class:`LogicalCoreList`. An empty LogicalCoreList won't filter anything.
    """

    _filter_specifier: LogicalCoreList

    def filter(self) -> list[LogicalCore]:
        """Filter based on logical CPU core ID.

        Return:
            The filtered logical CPU cores.
        """
        if not len(self._filter_specifier.lcore_list):
            return self._lcores_to_filter

        filtered_lcores = []
        for core in self._lcores_to_filter:
            if core.lcore in self._filter_specifier.lcore_list:
                filtered_lcores.append(core)

        if len(filtered_lcores) != len(self._filter_specifier.lcore_list):
            raise ValueError(
                f"Not all logical cores from {self._filter_specifier.lcore_list} "
                f"were found among {self._lcores_to_filter}"
            )

        return filtered_lcores


def lcore_filter(
    core_list: list[LogicalCore],
    filter_specifier: LogicalCoreCount | LogicalCoreList,
    ascending: bool,
) -> LogicalCoreFilter:
    """Factory for providing the filter that corresponds to `filter_specifier`.

    Args:
        core_list: The logical CPU cores to filter.
        filter_specifier: The filter to use.
        ascending: Sort cores in ascending order (lowest to highest IDs). If :data:`False`,
            sort in descending order.

    Returns:
        The filter that corresponds to `filter_specifier`.
    """
    if isinstance(filter_specifier, LogicalCoreList):
        return LogicalCoreListFilter(core_list, filter_specifier, ascending)
    elif isinstance(filter_specifier, LogicalCoreCount):
        return LogicalCoreCountFilter(core_list, filter_specifier, ascending)
    else:
        raise ValueError(f"Unsupported filter r{filter_specifier}")
