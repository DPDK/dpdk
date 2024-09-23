# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.

"""Testbed capabilities.

This module provides a protocol that defines the common attributes of test cases and suites
and support for test environment capabilities.
"""

from abc import ABC, abstractmethod
from collections.abc import Sequence
from typing import Callable, ClassVar, Protocol

from typing_extensions import Self

from .sut_node import SutNode
from .topology import Topology


class Capability(ABC):
    """The base class for various capabilities.

    The same capability should always be represented by the same object,
    meaning the same capability required by different test cases or suites
    should point to the same object.

    Example:
        ``test_case1`` and ``test_case2`` each require ``capability1``
        and in both instances, ``capability1`` should point to the same capability object.

    It is up to the subclasses how they implement this.

    The instances are used in sets so they must be hashable.
    """

    #: A set storing the capabilities whose support should be checked.
    capabilities_to_check: ClassVar[set[Self]] = set()

    def register_to_check(self) -> Callable[[SutNode, "Topology"], set[Self]]:
        """Register the capability to be checked for support.

        Returns:
            The callback function that checks the support of capabilities of the particular subclass
            which should be called after all capabilities have been registered.
        """
        if not type(self).capabilities_to_check:
            type(self).capabilities_to_check = set()
        type(self).capabilities_to_check.add(self)
        return type(self)._get_and_reset

    def add_to_required(self, test_case_or_suite: type["TestProtocol"]) -> None:
        """Add the capability instance to the required test case or suite's capabilities.

        Args:
            test_case_or_suite: The test case or suite among whose required capabilities
                to add this instance.
        """
        if not test_case_or_suite.required_capabilities:
            test_case_or_suite.required_capabilities = set()
        self._preprocess_required(test_case_or_suite)
        test_case_or_suite.required_capabilities.add(self)

    def _preprocess_required(self, test_case_or_suite: type["TestProtocol"]) -> None:
        """An optional method that modifies the required capabilities."""

    @classmethod
    def _get_and_reset(cls, sut_node: SutNode, topology: "Topology") -> set[Self]:
        """The callback method to be called after all capabilities have been registered.

        Not only does this method check the support of capabilities,
        but it also reset the internal set of registered capabilities
        so that the "register, then get support" workflow works in subsequent test runs.
        """
        supported_capabilities = cls.get_supported_capabilities(sut_node, topology)
        cls.capabilities_to_check = set()
        return supported_capabilities

    @classmethod
    @abstractmethod
    def get_supported_capabilities(cls, sut_node: SutNode, topology: "Topology") -> set[Self]:
        """Get the support status of each registered capability.

        Each subclass must implement this method and return the subset of supported capabilities
        of :attr:`capabilities_to_check`.

        Args:
            sut_node: The SUT node of the current test run.
            topology: The topology of the current test run.

        Returns:
            The supported capabilities.
        """

    @abstractmethod
    def __hash__(self) -> int:
        """The subclasses must be hashable so that they can be stored in sets."""


class TestProtocol(Protocol):
    """Common test suite and test case attributes."""

    #: Whether to skip the test case or suite.
    skip: ClassVar[bool] = False
    #: The reason for skipping the test case or suite.
    skip_reason: ClassVar[str] = ""
    #: The capabilities the test case or suite requires in order to be executed.
    required_capabilities: ClassVar[set[Capability]] = set()

    @classmethod
    def get_test_cases(cls, test_case_sublist: Sequence[str] | None = None) -> tuple[set, set]:
        """Get test cases. Should be implemented by subclasses containing test cases.

        Raises:
            NotImplementedError: The subclass does not implement the method.
        """
        raise NotImplementedError()


def get_supported_capabilities(
    sut_node: SutNode,
    topology_config: Topology,
    capabilities_to_check: set[Capability],
) -> set[Capability]:
    """Probe the environment for `capabilities_to_check` and return the supported ones.

    Args:
        sut_node: The SUT node to check for capabilities.
        topology_config: The topology config to check for capabilities.
        capabilities_to_check: The capabilities to check.

    Returns:
        The capabilities supported by the environment.
    """
    callbacks = set()
    for capability_to_check in capabilities_to_check:
        callbacks.add(capability_to_check.register_to_check())
    supported_capabilities = set()
    for callback in callbacks:
        supported_capabilities.update(callback(sut_node, topology_config))

    return supported_capabilities
