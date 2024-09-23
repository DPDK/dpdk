# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.

"""Testbed capabilities.

This module provides a protocol that defines the common attributes of test cases and suites
and support for test environment capabilities.

Many test cases are testing features not available on all hardware.

The module also allows developers to mark test cases or suites as requiring certain
hardware capabilities with the :func:`requires` decorator.

Example:
    .. code:: python

        from framework.test_suite import TestSuite, func_test
        from framework.testbed_model.capability import NicCapability, requires
        class TestPmdBufferScatter(TestSuite):
            # only the test case requires the SCATTERED_RX_ENABLED capability
            # other test cases may not require it
            @requires(NicCapability.SCATTERED_RX_ENABLED)
            @func_test
            def test_scatter_mbuf_2048(self):
"""

from abc import ABC, abstractmethod
from collections.abc import MutableSet, Sequence
from dataclasses import dataclass
from typing import Callable, ClassVar, Protocol

from typing_extensions import Self

from framework.logger import get_dts_logger
from framework.remote_session.testpmd_shell import (
    NicCapability,
    TestPmdShell,
    TestPmdShellCapabilityMethod,
    TestPmdShellDecorator,
    TestPmdShellMethod,
)

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


@dataclass
class DecoratedNicCapability(Capability):
    """A wrapper around :class:`~framework.remote_session.testpmd_shell.NicCapability`.

    New instances should be created with the :meth:`create_unique` class method to ensure
    there are no duplicate instances.

    Attributes:
        nic_capability: The NIC capability that defines each instance.
        capability_fn: The capability retrieval function of `nic_capability`.
        capability_decorator: The decorator function of `nic_capability`.
            This function will wrap `capability_fn`.
    """

    nic_capability: NicCapability
    capability_fn: TestPmdShellCapabilityMethod
    capability_decorator: TestPmdShellDecorator | None
    _unique_capabilities: ClassVar[dict[NicCapability, Self]] = {}

    @classmethod
    def get_unique(cls, nic_capability: NicCapability) -> "DecoratedNicCapability":
        """Get the capability uniquely identified by `nic_capability`.

        This is a factory method that implements a quasi-enum pattern.
        The instances of this class are stored in an internal class variable,
        `_unique_capabilities`.

        If an instance identified by `nic_capability` doesn't exist,
        it is created and added to `_unique_capabilities`.
        If it exists, it is returned so that a new identical instance is not created.

        Args:
            nic_capability: The NIC capability.

        Returns:
            The capability uniquely identified by `nic_capability`.
        """
        decorator_fn = None
        if isinstance(nic_capability.value, tuple):
            capability_fn, decorator_fn = nic_capability.value
        else:
            capability_fn = nic_capability.value

        if nic_capability not in cls._unique_capabilities:
            cls._unique_capabilities[nic_capability] = cls(
                nic_capability, capability_fn, decorator_fn
            )
        return cls._unique_capabilities[nic_capability]

    @classmethod
    def get_supported_capabilities(
        cls, sut_node: SutNode, topology: "Topology"
    ) -> set["DecoratedNicCapability"]:
        """Overrides :meth:`~Capability.get_supported_capabilities`.

        The capabilities are first sorted by decorators, then reduced into a single function which
        is then passed to the decorator. This way we execute each decorator only once.
        Each capability is first checked whether it's supported/unsupported
        before executing its `capability_fn` so that each capability is retrieved only once.
        """
        supported_conditional_capabilities: set["DecoratedNicCapability"] = set()
        logger = get_dts_logger(f"{sut_node.name}.{cls.__name__}")
        if topology.type is topology.type.no_link:
            logger.debug(
                "No links available in the current topology, not getting NIC capabilities."
            )
            return supported_conditional_capabilities
        logger.debug(
            f"Checking which NIC capabilities from {cls.capabilities_to_check} are supported."
        )
        if cls.capabilities_to_check:
            capabilities_to_check_map = cls._get_decorated_capabilities_map()
            with TestPmdShell(
                sut_node, privileged=True, disable_device_start=True
            ) as testpmd_shell:
                for conditional_capability_fn, capabilities in capabilities_to_check_map.items():
                    supported_capabilities: set[NicCapability] = set()
                    unsupported_capabilities: set[NicCapability] = set()
                    capability_fn = cls._reduce_capabilities(
                        capabilities, supported_capabilities, unsupported_capabilities
                    )
                    if conditional_capability_fn:
                        capability_fn = conditional_capability_fn(capability_fn)
                    capability_fn(testpmd_shell)
                    for capability in capabilities:
                        if capability.nic_capability in supported_capabilities:
                            supported_conditional_capabilities.add(capability)

        logger.debug(f"Found supported capabilities {supported_conditional_capabilities}.")
        return supported_conditional_capabilities

    @classmethod
    def _get_decorated_capabilities_map(
        cls,
    ) -> dict[TestPmdShellDecorator | None, set["DecoratedNicCapability"]]:
        capabilities_map: dict[TestPmdShellDecorator | None, set["DecoratedNicCapability"]] = {}
        for capability in cls.capabilities_to_check:
            if capability.capability_decorator not in capabilities_map:
                capabilities_map[capability.capability_decorator] = set()
            capabilities_map[capability.capability_decorator].add(capability)

        return capabilities_map

    @classmethod
    def _reduce_capabilities(
        cls,
        capabilities: set["DecoratedNicCapability"],
        supported_capabilities: MutableSet,
        unsupported_capabilities: MutableSet,
    ) -> TestPmdShellMethod:
        def reduced_fn(testpmd_shell: TestPmdShell) -> None:
            for capability in capabilities:
                if capability not in supported_capabilities | unsupported_capabilities:
                    capability.capability_fn(
                        testpmd_shell, supported_capabilities, unsupported_capabilities
                    )

        return reduced_fn

    def __hash__(self) -> int:
        """Instances are identified by :attr:`nic_capability` and :attr:`capability_decorator`."""
        return hash(self.nic_capability)

    def __repr__(self) -> str:
        """Easy to read string of :attr:`nic_capability` and :attr:`capability_decorator`."""
        return f"{self.nic_capability}"


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


def requires(
    *nic_capabilities: NicCapability,
) -> Callable[[type[TestProtocol]], type[TestProtocol]]:
    """A decorator that adds the required capabilities to a test case or test suite.

    Args:
        nic_capabilities: The NIC capabilities that are required by the test case or test suite.

    Returns:
        The decorated test case or test suite.
    """

    def add_required_capability(test_case_or_suite: type[TestProtocol]) -> type[TestProtocol]:
        for nic_capability in nic_capabilities:
            decorated_nic_capability = DecoratedNicCapability.get_unique(nic_capability)
            decorated_nic_capability.add_to_required(test_case_or_suite)

        return test_case_or_suite

    return add_required_capability


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
