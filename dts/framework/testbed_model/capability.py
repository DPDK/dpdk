# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""Testbed capabilities.

This module provides a protocol that defines the common attributes of test cases and suites
and support for test environment capabilities.

Many test cases are testing features not available on all hardware.
On the other hand, some test cases or suites may not need the most complex topology available.

The module allows developers to mark test cases or suites a requiring certain hardware capabilities
or a particular topology with the :func:`requires` decorator.

There are differences between hardware and topology capabilities:

    * Hardware capabilities are assumed to not be required when not specified.
    * However, some topology is always available, so each test case or suite is assigned
      a default topology if no topology is specified in the decorator.

The module also allows developers to mark test cases or suites as requiring certain
hardware capabilities with the :func:`requires` decorator.

Examples:
    .. code:: python

        from framework.test_suite import TestSuite, func_test
        from framework.testbed_model.capability import TopologyType, requires
        # The whole test suite (each test case within) doesn't require any links.
        @requires(topology_type=TopologyType.no_link)
        @func_test
        class TestHelloWorld(TestSuite):
            def hello_world_single_core(self):
            ...

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

import inspect
from abc import ABC, abstractmethod
from collections.abc import MutableSet
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any, Callable, ClassVar, Protocol

from typing_extensions import Self

from framework.exception import ConfigurationError, InternalError, SkippedTestException
from framework.logger import get_dts_logger
from framework.remote_session.testpmd_shell import (
    NicCapability,
    TestPmdShell,
    TestPmdShellCapabilityMethod,
    TestPmdShellDecorator,
    TestPmdShellMethod,
)
from framework.testbed_model.node import Node
from framework.testbed_model.port import DriverKind

from .topology import Topology, TopologyType

if TYPE_CHECKING:
    from framework.test_suite import TestCase


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

    def register_to_check(self) -> Callable[[Node, "Topology"], set[Self]]:
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
    def _get_and_reset(cls, node: Node, topology: "Topology") -> set[Self]:
        """The callback method to be called after all capabilities have been registered.

        Not only does this method check the support of capabilities,
        but it also reset the internal set of registered capabilities
        so that the "register, then get support" workflow works in subsequent test runs.
        """
        supported_capabilities = cls.get_supported_capabilities(node, topology)
        cls.capabilities_to_check = set()
        return supported_capabilities

    @classmethod
    @abstractmethod
    def get_supported_capabilities(cls, node: Node, topology: "Topology") -> set[Self]:
        """Get the support status of each registered capability.

        Each subclass must implement this method and return the subset of supported capabilities
        of :attr:`capabilities_to_check`.

        Args:
            node: The node to check capabilities against.
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
    def get_unique(cls, nic_capability: NicCapability) -> Self:
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
        capability_fn, decorator_fn = nic_capability.value

        if nic_capability not in cls._unique_capabilities:
            cls._unique_capabilities[nic_capability] = cls(
                nic_capability, capability_fn, decorator_fn
            )
        return cls._unique_capabilities[nic_capability]

    @classmethod
    def get_supported_capabilities(
        cls, node: Node, topology: "Topology"
    ) -> set["DecoratedNicCapability"]:
        """Overrides :meth:`~Capability.get_supported_capabilities`.

        The capabilities are first sorted by decorators, then reduced into a single function which
        is then passed to the decorator. This way we execute each decorator only once.
        Each capability is first checked whether it's supported/unsupported
        before executing its `capability_fn` so that each capability is retrieved only once.
        """
        supported_conditional_capabilities: set["DecoratedNicCapability"] = set()
        logger = get_dts_logger(f"{node.name}.{cls.__name__}")
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
            with TestPmdShell() as testpmd_shell:
                for (
                    conditional_capability_fn,
                    capabilities,
                ) in capabilities_to_check_map.items():
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


@dataclass
class TopologyCapability(Capability):
    """A wrapper around :class:`~.topology.TopologyType`.

    Each test case must be assigned a topology. It could be done explicitly;
    the implicit default is given by :meth:`~.topology.TopologyType.default`, which this class
    returns :attr:`~.topology.TopologyType.two_links`.

    Test case topology may be set by setting the topology for the whole suite.
    The priority in which topology is set is as follows:

        #. The topology set using the :func:`requires` decorator with a test case,
        #. The topology set using the :func:`requires` decorator with a test suite,
        #. The default topology if the decorator is not used.

    The default topology of test suite (i.e. when not using the decorator
    or not setting the topology with the decorator) does not affect the topology of test cases.

    New instances should be created with the :meth:`create_unique` class method to ensure
    there are no duplicate instances.

    Attributes:
        topology_type: The topology type that defines each instance.
    """

    topology_type: TopologyType

    _unique_capabilities: ClassVar[dict[str, Self]] = {}

    def _preprocess_required(self, test_case_or_suite: type["TestProtocol"]) -> None:
        test_case_or_suite.required_capabilities.discard(test_case_or_suite.topology_type)
        test_case_or_suite.topology_type = self

    @classmethod
    def get_unique(cls, topology_type: TopologyType) -> Self:
        """Get the capability uniquely identified by `topology_type`.

        This is a factory method that implements a quasi-enum pattern.
        The instances of this class are stored in an internal class variable,
        `_unique_capabilities`.

        If an instance identified by `topology_type` doesn't exist,
        it is created and added to `_unique_capabilities`.
        If it exists, it is returned so that a new identical instance is not created.

        Args:
            topology_type: The topology type.

        Returns:
            The capability uniquely identified by `topology_type`.
        """
        if topology_type.name not in cls._unique_capabilities:
            cls._unique_capabilities[topology_type.name] = cls(topology_type)
        return cls._unique_capabilities[topology_type.name]

    @classmethod
    def get_supported_capabilities(
        cls, node: Node, topology: "Topology"
    ) -> set["TopologyCapability"]:
        """Overrides :meth:`~Capability.get_supported_capabilities`."""
        supported_capabilities = set()
        topology_capability = cls.get_unique(topology.type)
        for topology_type in TopologyType:
            candidate_topology_type = cls.get_unique(topology_type)
            if candidate_topology_type <= topology_capability:
                supported_capabilities.add(candidate_topology_type)
        return supported_capabilities

    def set_required(self, test_case_or_suite: type["TestProtocol"]) -> None:
        """The logic for setting the required topology of a test case or suite.

        Decorators are applied on methods of a class first, then on the class.
        This means we have to modify test case topologies when processing the test suite topologies.
        At that point, the test case topologies have been set by the :func:`requires` decorator.
        The test suite topology only affects the test case topologies
        if not :attr:`~.topology.TopologyType.default`.

        Raises:
            ConfigurationError: If the topology type requested by the test case is more complex than
                the test suite's.
        """
        if inspect.isclass(test_case_or_suite):
            if self.topology_type is not TopologyType.default():
                self.add_to_required(test_case_or_suite)
                for test_case in test_case_or_suite.get_test_cases():
                    if test_case.topology_type.topology_type is TopologyType.default():
                        # test case topology has not been set, use the one set by the test suite
                        self.add_to_required(test_case)
                    elif test_case.topology_type > test_case_or_suite.topology_type:
                        raise ConfigurationError(
                            "The required topology type of a test case "
                            f"({test_case.__name__}|{test_case.topology_type}) "
                            "cannot be more complex than that of a suite "
                            f"({test_case_or_suite.__name__}|{test_case_or_suite.topology_type})."
                        )
        else:
            self.add_to_required(test_case_or_suite)

    def __eq__(self, other: Any) -> bool:
        """Compare the :attr:`~TopologyCapability.topology_type`s.

        Args:
            other: The object to compare with.

        Returns:
            :data:`True` if the topology types are the same.
        """
        return self.topology_type == other.topology_type

    def __lt__(self, other: Any) -> bool:
        """Compare the :attr:`~TopologyCapability.topology_type`s.

        Args:
            other: The object to compare with.

        Returns:
            :data:`True` if the instance's topology type is less complex than the compared object's.
        """
        return self.topology_type < other.topology_type

    def __gt__(self, other: Any) -> bool:
        """Compare the :attr:`~TopologyCapability.topology_type`s.

        Args:
            other: The object to compare with.

        Returns:
            :data:`True` if the instance's topology type is more complex than the compared object's.
        """
        return other < self

    def __le__(self, other: Any) -> bool:
        """Compare the :attr:`~TopologyCapability.topology_type`s.

        Args:
            other: The object to compare with.

        Returns:
            :data:`True` if the instance's topology type is less complex or equal than
            the compared object's.
        """
        return not self > other

    def __hash__(self):
        """Each instance is identified by :attr:`topology_type`."""
        return self.topology_type.__hash__()

    def __str__(self):
        """Easy to read string of class and name of :attr:`topology_type`."""
        return f"{type(self.topology_type).__name__}.{self.topology_type.name}"

    def __repr__(self):
        """Easy to read string of class and name of :attr:`topology_type`."""
        return self.__str__()


class TestProtocol(Protocol):
    """Common test suite and test case attributes."""

    #: Whether to skip the test case or suite.
    skip: ClassVar[bool] = False
    #: The reason for skipping the test case or suite.
    skip_reason: ClassVar[str] = ""
    #: The topology type of the test case or suite.
    topology_type: ClassVar[TopologyCapability] = TopologyCapability(TopologyType.default())
    #: The capabilities the test case or suite requires in order to be executed.
    required_capabilities: ClassVar[set[Capability]] = set()
    #: The SUT ports topology configuration of the test case or suite.
    sut_ports_drivers: ClassVar[DriverKind | tuple[DriverKind, ...] | None] = None

    @classmethod
    def get_test_cases(cls) -> list[type["TestCase"]]:
        """Get test cases. Should be implemented by subclasses containing test cases.

        Raises:
            NotImplementedError: The subclass does not implement the method.
        """
        raise NotImplementedError()


def configure_ports(
    *drivers: DriverKind, all_for: DriverKind | None = None
) -> Callable[[type[TestProtocol]], type[TestProtocol]]:
    """Decorator for test suite and test cases to configure ports drivers.

    Configure all the SUT ports for the specified driver kind with `all_for`. Otherwise, specify
    the port's respective driver kind in the positional argument. The amount of ports specified must
    adhere to the requested topology.

    Raises:
        InternalError: If both positional arguments and `all_for` are set.
    """
    if len(drivers) and all_for is not None:
        msg = "Cannot set both positional arguments and `all_for` to configure ports drivers."
        raise InternalError(msg)

    def _decorator(func: type[TestProtocol]) -> type[TestProtocol]:
        func.sut_ports_drivers = all_for or drivers
        return func

    return _decorator


def requires(
    *nic_capabilities: NicCapability,
    topology_type: TopologyType = TopologyType.default(),
) -> Callable[[type[TestProtocol]], type[TestProtocol]]:
    """A decorator that adds the required capabilities to a test case or test suite.

    Args:
        nic_capabilities: The NIC capabilities that are required by the test case or test suite.
        topology_type: The topology type the test suite or case requires.

    Returns:
        The decorated test case or test suite.
    """

    def add_required_capability(
        test_case_or_suite: type[TestProtocol],
    ) -> type[TestProtocol]:
        for nic_capability in nic_capabilities:
            decorated_nic_capability = DecoratedNicCapability.get_unique(nic_capability)
            decorated_nic_capability.add_to_required(test_case_or_suite)

        topology_capability = TopologyCapability.get_unique(topology_type)
        topology_capability.set_required(test_case_or_suite)

        return test_case_or_suite

    return add_required_capability


def get_supported_capabilities(
    node: Node,
    topology_config: Topology,
    capabilities_to_check: set[Capability],
) -> set[Capability]:
    """Probe the environment for `capabilities_to_check` and return the supported ones.

    Args:
        node: The node to check capabilities against.
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
        supported_capabilities.update(callback(node, topology_config))

    return supported_capabilities


def test_if_supported(test: type[TestProtocol], supported_caps: set[Capability]) -> None:
    """Test if the given test suite or test case is supported.

    Args:
        test: The test suite or case.
        supported_caps: The capabilities that need to be checked against the test.

    Raises:
        SkippedTestException: If the test hasn't met the requirements.
    """
    unsupported_caps = test.required_capabilities - supported_caps
    if unsupported_caps:
        capability_str = "capabilities" if len(unsupported_caps) > 1 else "capability"
        msg = f"Required {capability_str} '{unsupported_caps}' not found."
        raise SkippedTestException(msg)
