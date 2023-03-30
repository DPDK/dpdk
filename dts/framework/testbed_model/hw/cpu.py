# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

import dataclasses
from abc import ABC, abstractmethod
from collections.abc import Iterable, ValuesView
from dataclasses import dataclass

from framework.utils import expand_range


@dataclass(slots=True, frozen=True)
class LogicalCore(object):
    """
    Representation of a CPU core. A physical core is represented in OS
    by multiple logical cores (lcores) if CPU multithreading is enabled.
    """

    lcore: int
    core: int
    socket: int
    node: int

    def __int__(self) -> int:
        return self.lcore


class LogicalCoreList(object):
    """
    Convert these options into a list of logical core ids.
    lcore_list=[LogicalCore1, LogicalCore2] - a list of LogicalCores
    lcore_list=[0,1,2,3] - a list of int indices
    lcore_list=['0','1','2-3'] - a list of str indices; ranges are supported
    lcore_list='0,1,2-3' - a comma delimited str of indices; ranges are supported

    The class creates a unified format used across the framework and allows
    the user to use either a str representation (using str(instance) or directly
    in f-strings) or a list representation (by accessing instance.lcore_list).
    Empty lcore_list is allowed.
    """

    _lcore_list: list[int]
    _lcore_str: str

    def __init__(self, lcore_list: list[int] | list[str] | list[LogicalCore] | str):
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
        self._lcore_str = (
            f'{",".join(self._get_consecutive_lcores_range(self._lcore_list))}'
        )

    @property
    def lcore_list(self) -> list[int]:
        return self._lcore_list

    def _get_consecutive_lcores_range(self, lcore_ids_list: list[int]) -> list[str]:
        formatted_core_list = []
        segment = lcore_ids_list[:1]
        for lcore_id in lcore_ids_list[1:]:
            if lcore_id - segment[-1] == 1:
                segment.append(lcore_id)
            else:
                formatted_core_list.append(
                    f"{segment[0]}-{segment[-1]}"
                    if len(segment) > 1
                    else f"{segment[0]}"
                )
                current_core_index = lcore_ids_list.index(lcore_id)
                formatted_core_list.extend(
                    self._get_consecutive_lcores_range(
                        lcore_ids_list[current_core_index:]
                    )
                )
                segment.clear()
                break
        if len(segment) > 0:
            formatted_core_list.append(
                f"{segment[0]}-{segment[-1]}" if len(segment) > 1 else f"{segment[0]}"
            )
        return formatted_core_list

    def __str__(self) -> str:
        return self._lcore_str


@dataclasses.dataclass(slots=True, frozen=True)
class LogicalCoreCount(object):
    """
    Define the number of logical cores to use.
    If sockets is not None, socket_count is ignored.
    """

    lcores_per_core: int = 1
    cores_per_socket: int = 2
    socket_count: int = 1
    sockets: list[int] | None = None


class LogicalCoreFilter(ABC):
    """
    Filter according to the input filter specifier. Each filter needs to be
    implemented in a derived class.
    This class only implements operations common to all filters, such as sorting
    the list to be filtered beforehand.
    """

    _filter_specifier: LogicalCoreCount | LogicalCoreList
    _lcores_to_filter: list[LogicalCore]

    def __init__(
        self,
        lcore_list: list[LogicalCore],
        filter_specifier: LogicalCoreCount | LogicalCoreList,
        ascending: bool = True,
    ):
        self._filter_specifier = filter_specifier

        # sorting by core is needed in case hyperthreading is enabled
        self._lcores_to_filter = sorted(
            lcore_list, key=lambda x: x.core, reverse=not ascending
        )
        self.filter()

    @abstractmethod
    def filter(self) -> list[LogicalCore]:
        """
        Use self._filter_specifier to filter self._lcores_to_filter
        and return the list of filtered LogicalCores.
        self._lcores_to_filter is a sorted copy of the original list,
        so it may be modified.
        """


class LogicalCoreCountFilter(LogicalCoreFilter):
    """
    Filter the input list of LogicalCores according to specified rules:
    Use cores from the specified number of sockets or from the specified socket ids.
    If sockets is specified, it takes precedence over socket_count.
    From each of those sockets, use only cores_per_socket of cores.
    And for each core, use lcores_per_core of logical cores. Hypertheading
    must be enabled for this to take effect.
    If ascending is True, use cores with the lowest numerical id first
    and continue in ascending order. If False, start with the highest
    id and continue in descending order. This ordering affects which
    sockets to consider first as well.
    """

    _filter_specifier: LogicalCoreCount

    def filter(self) -> list[LogicalCore]:
        sockets_to_filter = self._filter_sockets(self._lcores_to_filter)
        filtered_lcores = []
        for socket_to_filter in sockets_to_filter:
            filtered_lcores.extend(self._filter_cores_from_socket(socket_to_filter))
        return filtered_lcores

    def _filter_sockets(
        self, lcores_to_filter: Iterable[LogicalCore]
    ) -> ValuesView[list[LogicalCore]]:
        """
        Remove all lcores that don't match the specified socket(s).
        If self._filter_specifier.sockets is not None, keep lcores from those sockets,
        otherwise keep lcores from the first
        self._filter_specifier.socket_count sockets.
        """
        allowed_sockets: set[int] = set()
        socket_count = self._filter_specifier.socket_count
        if self._filter_specifier.sockets:
            socket_count = len(self._filter_specifier.sockets)
            allowed_sockets = set(self._filter_specifier.sockets)

        filtered_lcores: dict[int, list[LogicalCore]] = {}
        for lcore in lcores_to_filter:
            if not self._filter_specifier.sockets:
                if len(allowed_sockets) < socket_count:
                    allowed_sockets.add(lcore.socket)
            if lcore.socket in allowed_sockets:
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
        """
        Keep only the first self._filter_specifier.cores_per_socket cores.
        In multithreaded environments, keep only
        the first self._filter_specifier.lcores_per_core lcores of those cores.
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
            elif self._filter_specifier.cores_per_socket > len(
                lcore_count_per_core_map
            ):
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
    """
    Filter the input list of Logical Cores according to the input list of
    lcore indices.
    An empty LogicalCoreList won't filter anything.
    """

    _filter_specifier: LogicalCoreList

    def filter(self) -> list[LogicalCore]:
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
