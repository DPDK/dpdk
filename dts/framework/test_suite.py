# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Features common to all test suites.

The module defines the :class:`TestSuite` class which doesn't contain any test cases, and as such
must be extended by subclasses which add test cases. The :class:`TestSuite` contains the basics
needed by subclasses:

    * Testbed (SUT, TG) configuration,
    * Packet sending and verification,
    * Test case verification.
"""

import inspect
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from enum import Enum, auto
from functools import cached_property
from importlib import import_module
from ipaddress import IPv4Interface, IPv6Interface, ip_interface
from pkgutil import iter_modules
from types import ModuleType
from typing import TYPE_CHECKING, ClassVar, Protocol, TypeVar, Union, cast

from scapy.layers.inet import IP
from scapy.packet import Packet
from typing_extensions import Self

from framework.config.common import FrozenModel
from framework.testbed_model.capability import TestProtocol
from framework.testbed_model.topology import Topology

from .exception import ConfigurationError, InternalError
from .logger import DTSLogger, get_dts_logger
from .utils import to_pascal_case

if TYPE_CHECKING:
    from framework.context import Context


class BaseConfig(FrozenModel):
    """Base for a custom test suite configuration."""


class TestSuite(TestProtocol):
    """The base class with building blocks needed by most test cases.

        * Test suite setup/cleanup methods to override,
        * Test case setup/cleanup methods to override,
        * Test case verification,
        * Testbed configuration,
        * Traffic sending and verification.

    Test cases are implemented by subclasses. Test cases are all methods starting with ``test_``,
    further divided into performance test cases (starting with ``test_perf_``)
    and functional test cases (all other test cases).

    By default, all test cases will be executed. A list of testcase names may be specified
    in the YAML test run configuration file and in the :option:`--test-suite` command line argument
    or in the :envvar:`DTS_TEST_SUITES` environment variable to filter which test cases to run.
    The union of both lists will be used. Any unknown test cases from the latter lists
    will be silently ignored.

    The methods named ``[set_up|tear_down]_[suite|test_case]`` should be overridden in subclasses
    if the appropriate test suite/test case fixtures are needed.

    The test suite is aware of the testbed (the SUT and TG) it's running on. From this, it can
    properly choose the IP addresses and other configuration that must be tailored to the testbed.

    Attributes:
        config: The test suite configuration.
    """

    config: BaseConfig

    #: Whether the test suite is blocking. A failure of a blocking test suite
    #: will block the execution of all subsequent test suites in the current test run.
    is_blocking: ClassVar[bool] = False
    _ctx: "Context"
    _logger: DTSLogger
    _sut_ip_address_ingress: Union[IPv4Interface, IPv6Interface]
    _sut_ip_address_egress: Union[IPv4Interface, IPv6Interface]
    _tg_ip_address_ingress: Union[IPv4Interface, IPv6Interface]
    _tg_ip_address_egress: Union[IPv4Interface, IPv6Interface]

    def __init__(self, config: BaseConfig) -> None:
        """Initialize the test suite testbed information and basic configuration.

        Args:
            config: The test suite configuration.
        """
        from framework.context import get_ctx

        self.config = config
        self._ctx = get_ctx()
        self._logger = get_dts_logger(self.__class__.__name__)
        self._sut_ip_address_ingress = ip_interface("192.168.100.2/24")
        self._sut_ip_address_egress = ip_interface("192.168.101.2/24")
        self._tg_ip_address_egress = ip_interface("192.168.100.3/24")
        self._tg_ip_address_ingress = ip_interface("192.168.101.3/24")

    @property
    def name(self) -> str:
        """The name of the test suite."""
        module_prefix = (
            f"{TestSuiteSpec.TEST_SUITES_PACKAGE_NAME}.{TestSuiteSpec.TEST_SUITE_MODULE_PREFIX}"
        )
        return type(self).__module__[len(module_prefix) :]

    @property
    def topology(self) -> Topology:
        """The current topology in use."""
        return self._ctx.topology

    @classmethod
    def get_test_cases(cls) -> list[type["TestCase"]]:
        """A list of all the available test cases."""

        def is_test_case(function: Callable) -> bool:
            if inspect.isfunction(function):
                # TestCase is not used at runtime, so we can't use isinstance() with `function`.
                # But function.test_type exists.
                if hasattr(function, "test_type"):
                    return isinstance(function.test_type, TestCaseType)
            return False

        return [test_case for _, test_case in inspect.getmembers(cls, is_test_case)]

    @classmethod
    def filter_test_cases(
        cls, test_case_sublist: Sequence[str] | None = None
    ) -> tuple[set[type["TestCase"]], set[type["TestCase"]]]:
        """Filter `test_case_sublist` from this class.

        Test cases are regular (or bound) methods decorated with :func:`func_test`
        or :func:`perf_test`.

        Args:
            test_case_sublist: Test case names to filter from this class.
                If empty or :data:`None`, return all test cases.

        Returns:
            The filtered test case functions. This method returns functions as opposed to methods,
            as methods are bound to instances and this method only has access to the class.

        Raises:
            ConfigurationError: If a test case from `test_case_sublist` is not found.
        """
        if test_case_sublist is None:
            test_case_sublist = []

        # the copy is needed so that the condition "elif test_case_sublist" doesn't
        # change mid-cycle
        test_case_sublist_copy = list(test_case_sublist)
        func_test_cases = set()
        perf_test_cases = set()

        for test_case in cls.get_test_cases():
            if test_case.name in test_case_sublist_copy:
                # if test_case_sublist_copy is non-empty, remove the found test case
                # so that we can look at the remainder at the end
                test_case_sublist_copy.remove(test_case.name)
            elif test_case_sublist:
                # the original list not being empty means we're filtering test cases
                # since we didn't remove test_case.name in the previous branch,
                # it doesn't match the filter and we don't want to remove it
                continue

            match test_case.test_type:
                case TestCaseType.PERFORMANCE:
                    perf_test_cases.add(test_case)
                case TestCaseType.FUNCTIONAL:
                    func_test_cases.add(test_case)

        if test_case_sublist_copy:
            raise ConfigurationError(
                f"Test cases {test_case_sublist_copy} not found among functions of {cls.__name__}."
            )

        return func_test_cases, perf_test_cases

    def set_up_suite(self) -> None:
        """Set up test fixtures common to all test cases.

        This is done before any test case has been run.
        """

    def tear_down_suite(self) -> None:
        """Tear down the previously created test fixtures common to all test cases.

        This is done after all test have been run.
        """

    def set_up_test_case(self) -> None:
        """Set up test fixtures before each test case.

        This is done before *each* test case.
        """

    def tear_down_test_case(self) -> None:
        """Tear down the previously created test fixtures after each test case.

        This is done after *each* test case.
        """

    def _adjust_addresses(self, packets: list[Packet], expected: bool = False) -> list[Packet]:
        """L2 and L3 address additions in both directions.

        Copies of `packets` will be made, modified and returned in this method.

        Only missing addresses are added to packets, existing addresses will not be overridden. If
        any packet in `packets` has multiple IP layers (using GRE, for example) only the inner-most
        IP layer will have its addresses adjusted.

        Assumptions:
            Two links between SUT and TG, one link is TG -> SUT, the other SUT -> TG.

        Args:
            packets: The packets to modify.
            expected: If :data:`True`, the direction is SUT -> TG,
                otherwise the direction is TG -> SUT.

        Returns:
            A list containing copies of all packets in `packets` after modification.
        """
        ret_packets = []
        for original_packet in packets:
            packet: Packet = original_packet.copy()

            # update l2 addresses
            # If `expected` is :data:`True`, the packet enters the TG from SUT, otherwise the
            # packet leaves the TG towards the SUT.

            # The fields parameter of a packet does not include fields of the payload, so this can
            # only be the Ether src/dst.
            if "src" not in packet.fields:
                packet.src = (
                    self.topology.sut_port_egress.mac_address
                    if expected
                    else self.topology.tg_port_egress.mac_address
                )
            if "dst" not in packet.fields:
                packet.dst = (
                    self.topology.tg_port_ingress.mac_address
                    if expected
                    else self.topology.sut_port_ingress.mac_address
                )

            # update l3 addresses
            # The packet is routed from TG egress to TG ingress regardless of whether it is
            # expected or not.
            num_ip_layers = packet.layers().count(IP)
            if num_ip_layers > 0:
                # Update the last IP layer if there are multiple (the framework should be modifying
                # the packet address instead of the tunnel address if there is one).
                l3_to_use = packet.getlayer(IP, num_ip_layers)
                assert l3_to_use is not None
                if "src" not in l3_to_use.fields:
                    l3_to_use.src = self._tg_ip_address_egress.ip.exploded

                if "dst" not in l3_to_use.fields:
                    l3_to_use.dst = self._tg_ip_address_ingress.ip.exploded
            ret_packets.append(packet)

        return ret_packets


#: The generic type for a method of an instance of TestSuite
TestSuiteMethodType = TypeVar("TestSuiteMethodType", bound=Callable[[TestSuite], None])


class TestCaseType(Enum):
    """The types of test cases."""

    #:
    FUNCTIONAL = auto()
    #:
    PERFORMANCE = auto()


class TestCase(TestProtocol, Protocol[TestSuiteMethodType]):
    """Definition of the test case type for static type checking purposes.

    The type is applied to test case functions through a decorator, which casts the decorated
    test case function to :class:`TestCase` and sets common variables.
    """

    #:
    name: ClassVar[str]
    #:
    test_type: ClassVar[TestCaseType]
    #: necessary for mypy so that it can treat this class as the function it's shadowing
    __call__: TestSuiteMethodType

    @classmethod
    def make_decorator(
        cls, test_case_type: TestCaseType
    ) -> Callable[[TestSuiteMethodType], type["TestCase"]]:
        """Create a decorator for test suites.

        The decorator casts the decorated function as :class:`TestCase`,
        sets it as `test_case_type`
        and initializes common variables defined in :class:`RequiresCapabilities`.

        Args:
            test_case_type: Either a functional or performance test case.

        Returns:
            The decorator of a functional or performance test case.
        """

        def _decorator(func: TestSuiteMethodType) -> type[TestCase]:
            test_case = cast(type[TestCase], func)
            test_case.name = func.__name__
            test_case.skip = cls.skip
            test_case.skip_reason = cls.skip_reason
            test_case.required_capabilities = set()
            test_case.topology_type = cls.topology_type
            test_case.topology_type.add_to_required(test_case)
            test_case.test_type = test_case_type
            test_case.sut_ports_drivers = cls.sut_ports_drivers
            return test_case

        return _decorator


#: The decorator for functional test cases.
func_test: Callable = TestCase.make_decorator(TestCaseType.FUNCTIONAL)
#: The decorator for performance test cases.
perf_test: Callable = TestCase.make_decorator(TestCaseType.PERFORMANCE)


@dataclass
class TestSuiteSpec:
    """A class defining the specification of a test suite.

    Apart from defining all the specs of a test suite, a helper function :meth:`discover_all` is
    provided to automatically discover all the available test suites.

    Attributes:
        module_name: The name of the test suite's module.
    """

    #:
    TEST_SUITES_PACKAGE_NAME = "tests"
    #:
    TEST_SUITE_MODULE_PREFIX = "TestSuite_"
    #:
    TEST_SUITE_CLASS_PREFIX = "Test"
    #:
    TEST_CASE_METHOD_PREFIX = "test_"
    #:
    FUNC_TEST_CASE_REGEX = r"test_(?!perf_)"
    #:
    PERF_TEST_CASE_REGEX = r"test_perf_"

    module_name: str

    @cached_property
    def name(self) -> str:
        """The name of the test suite's module."""
        return self.module_name[len(self.TEST_SUITE_MODULE_PREFIX) :]

    @cached_property
    def module(self) -> ModuleType:
        """A reference to the test suite's module."""
        return import_module(f"{self.TEST_SUITES_PACKAGE_NAME}.{self.module_name}")

    @cached_property
    def class_name(self) -> str:
        """The name of the test suite's class."""
        return f"{self.TEST_SUITE_CLASS_PREFIX}{to_pascal_case(self.name)}"

    @cached_property
    def class_obj(self) -> type[TestSuite]:
        """A reference to the test suite's class.

        Raises:
            InternalError: If the test suite class is missing from the module.
        """

        def is_test_suite(obj: type) -> bool:
            """Check whether `obj` is a :class:`TestSuite`.

            The `obj` is a subclass of :class:`TestSuite`, but not :class:`TestSuite` itself.

            Args:
                obj: The object to be checked.

            Returns:
                :data:`True` if `obj` is a subclass of `TestSuite`.
            """
            try:
                if issubclass(obj, TestSuite) and obj is not TestSuite:
                    return True
            except TypeError:
                return False
            return False

        for class_name, class_obj in inspect.getmembers(self.module, is_test_suite):
            if class_name == self.class_name:
                return class_obj

        raise InternalError(
            f"Expected class {self.class_name} not found in module {self.module_name}."
        )

    @cached_property
    def config_obj(self) -> type[BaseConfig]:
        """A reference to the test suite's configuration class."""
        return self.class_obj.__annotations__.get("config", BaseConfig)

    @classmethod
    def discover_all(
        cls, package_name: str | None = None, module_prefix: str | None = None
    ) -> list[Self]:
        """Discover all the test suites.

        The test suites are discovered in the provided `package_name`. The full module name,
        expected under that package, is prefixed with `module_prefix`.
        The module name is a standard filename with words separated with underscores.
        For each module found, search for a :class:`TestSuite` class which starts
        with :attr:`~TestSuiteSpec.TEST_SUITE_CLASS_PREFIX`, continuing with the module name in
        PascalCase.

        The PascalCase convention applies to abbreviations, acronyms, initialisms and so on::

            OS -> Os
            TCP -> Tcp

        Args:
            package_name: The name of the package where to find the test suites. If :data:`None`,
                the :attr:`~TestSuiteSpec.TEST_SUITES_PACKAGE_NAME` is used.
            module_prefix: The name prefix defining the test suite module. If :data:`None`, the
                :attr:`~TestSuiteSpec.TEST_SUITE_MODULE_PREFIX` constant is used.

        Returns:
            A list containing all the discovered test suites.
        """
        if package_name is None:
            package_name = cls.TEST_SUITES_PACKAGE_NAME
        if module_prefix is None:
            module_prefix = cls.TEST_SUITE_MODULE_PREFIX

        test_suites = []

        test_suites_pkg = import_module(package_name)
        for _, module_name, is_pkg in iter_modules(test_suites_pkg.__path__):
            if not module_name.startswith(module_prefix) or is_pkg:
                continue

            test_suite = cls(module_name)
            try:
                if test_suite.class_obj:
                    test_suites.append(test_suite)
            except InternalError as err:
                get_dts_logger().warning(err)

        return test_suites


AVAILABLE_TEST_SUITES: list[TestSuiteSpec] = TestSuiteSpec.discover_all()
"""Constant to store all the available, discovered and imported test suites.

The test suites should be gathered from this list to avoid importing more than once.
"""


def find_by_name(name: str) -> TestSuiteSpec | None:
    """Find a requested test suite by name from the available ones."""
    test_suites = filter(lambda t: t.name == name, AVAILABLE_TEST_SUITES)
    return next(test_suites, None)
