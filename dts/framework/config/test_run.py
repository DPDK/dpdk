# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Configuration models representing a test run.

The root model of a test run configuration is :class:`TestRunConfiguration`.
"""

import re
import tarfile
from collections import deque
from collections.abc import Iterable
from enum import Enum, auto, unique
from functools import cached_property
from pathlib import Path, PurePath
from typing import Annotated, Any, Literal, NamedTuple

from pydantic import (
    BaseModel,
    Field,
    create_model,
    field_validator,
    model_validator,
)
from typing_extensions import TYPE_CHECKING, Self

from framework.exception import InternalError
from framework.utils import REGEX_FOR_PORT_LINK, StrEnum

from .common import FrozenModel, load_fields_from_settings

if TYPE_CHECKING:
    from framework.test_suite import BaseConfig, TestCase, TestSuite, TestSuiteSpec


@unique
class Compiler(StrEnum):
    r"""The supported compilers of :class:`~framework.testbed_model.node.Node`\s."""

    #:
    gcc = auto()
    #:
    clang = auto()
    #:
    msvc = auto()


def resolve_path(path: Path) -> Path:
    """Resolve a path into a real path."""
    return path.resolve()


class BaseDPDKLocation(FrozenModel):
    """DPDK location base class.

    The path to the DPDK sources and type of location.
    """

    #: Specifies whether to find DPDK on the SUT node or on the local host. Which are respectively
    #: represented by :class:`RemoteDPDKLocation` and :class:`LocalDPDKTreeLocation`.
    remote: bool = False


class LocalDPDKLocation(BaseDPDKLocation):
    """Local DPDK location base class.

    This class is meant to represent any location that is present only locally.
    """

    remote: Literal[False] = False


class LocalDPDKTreeLocation(LocalDPDKLocation):
    """Local DPDK tree location.

    This class makes a distinction from :class:`RemoteDPDKTreeLocation` by enforcing on the fly
    validation.
    """

    #: The path to the DPDK source tree directory on the local host passed as string.
    dpdk_tree: Path

    #: Resolve the local DPDK tree path.
    resolve_dpdk_tree_path = field_validator("dpdk_tree")(resolve_path)

    @model_validator(mode="after")
    def validate_dpdk_tree_path(self) -> Self:
        """Validate the provided DPDK tree path."""
        assert self.dpdk_tree.exists(), "DPDK tree not found in local filesystem."
        assert self.dpdk_tree.is_dir(), "The DPDK tree path must be a directory."
        return self


class LocalDPDKTarballLocation(LocalDPDKLocation):
    """Local DPDK tarball location.

    This class makes a distinction from :class:`RemoteDPDKTarballLocation` by enforcing on the fly
    validation.
    """

    #: The path to the DPDK tarball on the local host passed as string.
    tarball: Path

    #: Resolve the local tarball path.
    resolve_tarball_path = field_validator("tarball")(resolve_path)

    @model_validator(mode="after")
    def validate_tarball_path(self) -> Self:
        """Validate the provided tarball."""
        assert self.tarball.exists(), "DPDK tarball not found in local filesystem."
        assert tarfile.is_tarfile(self.tarball), "The DPDK tarball must be a valid tar archive."
        return self


class RemoteDPDKLocation(BaseDPDKLocation):
    """Remote DPDK location base class.

    This class is meant to represent any location that is present only remotely.
    """

    remote: Literal[True] = True


class RemoteDPDKTreeLocation(RemoteDPDKLocation):
    """Remote DPDK tree location.

    This class is distinct from :class:`LocalDPDKTreeLocation` which enforces on the fly validation.
    """

    #: The path to the DPDK source tree directory on the remote node passed as string.
    dpdk_tree: PurePath


class RemoteDPDKTarballLocation(RemoteDPDKLocation):
    """Remote DPDK tarball location.

    This class is distinct from :class:`LocalDPDKTarballLocation` which enforces on the fly
    validation.
    """

    #: The path to the DPDK tarball on the remote node passed as string.
    tarball: PurePath


#: Union type for different DPDK locations.
DPDKLocation = (
    LocalDPDKTreeLocation
    | LocalDPDKTarballLocation
    | RemoteDPDKTreeLocation
    | RemoteDPDKTarballLocation
)


class BaseDPDKBuildConfiguration(FrozenModel):
    """The base configuration for different types of build.

    The configuration contain the location of the DPDK and configuration used for building it.
    """

    #: The location of the DPDK tree.
    dpdk_location: DPDKLocation

    dpdk_location_from_settings = model_validator(mode="before")(
        load_fields_from_settings("dpdk_location")
    )


class DPDKPrecompiledBuildConfiguration(BaseDPDKBuildConfiguration):
    """DPDK precompiled build configuration."""

    #: If it's defined, DPDK has been pre-compiled and the build directory is located in a
    #: subdirectory of `~dpdk_location.dpdk_tree` or `~dpdk_location.tarball` root directory.
    precompiled_build_dir: str = Field(min_length=1)

    build_dir_from_settings = model_validator(mode="before")(
        load_fields_from_settings("precompiled_build_dir")
    )


class DPDKBuildOptionsConfiguration(FrozenModel):
    """DPDK build options configuration.

    The build options used for building DPDK.
    """

    #: The compiler executable to use.
    compiler: Compiler
    #: This string will be put in front of the compiler when executing the build. Useful for adding
    #: wrapper commands, such as ``ccache``.
    compiler_wrapper: str = ""


class DPDKUncompiledBuildConfiguration(BaseDPDKBuildConfiguration):
    """DPDK uncompiled build configuration."""

    #: The build options to compiled DPDK with.
    build_options: DPDKBuildOptionsConfiguration


#: Union type for different build configurations.
DPDKBuildConfiguration = DPDKPrecompiledBuildConfiguration | DPDKUncompiledBuildConfiguration


class TestSuiteConfig(FrozenModel):
    """Test suite configuration.

    Information about a single test suite to be executed. This can also be represented as a string
    instead of a mapping, example:

    .. code:: yaml

        test_suites:
        # As string representation:
        - hello_world # test all of `hello_world`, or
        - hello_world hello_world_single_core # test only `hello_world_single_core`
        # or as model fields:
        - test_suite: hello_world
            test_cases: [hello_world_single_core] # without this field all test cases are run
    """

    #: The name of the test suite module without the starting ``TestSuite_``.
    test_suite_name: str = Field(alias="test_suite")
    #: The names of test cases from this test suite to execute. If empty, all test cases will be
    #: executed.
    test_cases_names: list[str] = Field(default_factory=list, alias="test_cases")

    @cached_property
    def test_suite_spec(self) -> "TestSuiteSpec":
        """The specification of the requested test suite."""
        from framework.test_suite import find_by_name

        test_suite_spec = find_by_name(self.test_suite_name)
        assert (
            test_suite_spec is not None
        ), f"{self.test_suite_name} is not a valid test suite module name."
        return test_suite_spec

    @cached_property
    def test_cases(self) -> list[type["TestCase"]]:
        """The objects of the selected test cases."""
        available_test_cases = {t.name: t for t in self.test_suite_spec.class_obj.get_test_cases()}
        selected_test_cases = []

        for requested_test_case in self.test_cases_names:
            assert requested_test_case in available_test_cases, (
                f"{requested_test_case} is not a valid test case "
                f"of test suite {self.test_suite_name}."
            )
            selected_test_cases.append(available_test_cases[requested_test_case])

        return selected_test_cases or list(available_test_cases.values())

    @model_validator(mode="before")
    @classmethod
    def convert_from_string(cls, data: Any) -> Any:
        """Convert the string representation of the model into a valid mapping."""
        if isinstance(data, str):
            [test_suite, *test_cases] = data.split()
            return dict(test_suite=test_suite, test_cases=test_cases)
        return data

    @model_validator(mode="after")
    def validate_names(self) -> Self:
        """Validate the supplied test suite and test cases names.

        This validator relies on the cached properties `test_suite_spec` and `test_cases` to run for
        the first time in this call, therefore triggering the assertions if needed.
        """
        if self.test_cases:
            pass

        return self


def fetch_all_test_suites() -> list[TestSuiteConfig]:
    """Returns all the available test suites as configuration objects.

    This function does not include the smoke tests.
    """
    from framework.test_suite import AVAILABLE_TEST_SUITES

    return [
        TestSuiteConfig(test_suite=test_suite.name)
        for test_suite in AVAILABLE_TEST_SUITES
        if test_suite.name != "smoke_tests"
    ]


def make_test_suite_config_field(config_obj: type["BaseConfig"]):
    """Make a field for a test suite's configuration.

    If the test suite's configuration has required fields, then make the field required. Otherwise
    make it optional.
    """
    if any(f.is_required() for f in config_obj.model_fields.values()):
        return config_obj, Field()
    else:
        return config_obj, Field(default_factory=config_obj)


def create_test_suites_config_model(test_suites: list[TestSuiteConfig]) -> type[BaseModel]:
    """Create model for the test suites configuration."""
    complete_test_suites = [TestSuiteConfig(test_suite="smoke_tests")]
    complete_test_suites += test_suites

    test_suites_kwargs = {
        t.test_suite_name: make_test_suite_config_field(t.test_suite_spec.config_obj)
        for t in complete_test_suites
    }
    return create_model("TestSuitesConfiguration", **test_suites_kwargs)


class LinkPortIdentifier(NamedTuple):
    """A tuple linking test run node type to port name."""

    node_type: Literal["sut", "tg"]
    port_name: str


class PortLinkConfig(FrozenModel):
    """A link between the ports of the nodes.

    Can be represented as a string with the following notation:

    .. code::

        sut.{port name} <-> tg.{port name}  # explicit node nomination
        {port name} <-> {port name}         # implicit node nomination. Left is SUT, right is TG.
    """

    #: The port at the left side of the link.
    left: LinkPortIdentifier
    #: The port at the right side of the link.
    right: LinkPortIdentifier

    @cached_property
    def sut_port(self) -> str:
        """Port name of the SUT node.

        Raises:
            InternalError: If a misconfiguration has been allowed to happen.
        """
        if self.left.node_type == "sut":
            return self.left.port_name
        if self.right.node_type == "sut":
            return self.right.port_name

        raise InternalError("Unreachable state reached.")

    @cached_property
    def tg_port(self) -> str:
        """Port name of the TG node.

        Raises:
            InternalError: If a misconfiguration has been allowed to happen.
        """
        if self.left.node_type == "tg":
            return self.left.port_name
        if self.right.node_type == "tg":
            return self.right.port_name

        raise InternalError("Unreachable state reached.")

    @model_validator(mode="before")
    @classmethod
    def convert_from_string(cls, data: Any) -> Any:
        """Convert the string representation of the model into a valid mapping."""
        if isinstance(data, str):
            m = re.match(REGEX_FOR_PORT_LINK, data, re.I)
            assert m is not None, (
                "The provided link is malformed. Please use the following "
                "notation: sut.{port name} <-> tg.{port name}"
            )

            left = (m.group(1) or "sut").lower(), m.group(2)
            right = (m.group(3) or "tg").lower(), m.group(4)

            return {"left": left, "right": right}
        return data

    @model_validator(mode="after")
    def verify_distinct_nodes(self) -> Self:
        """Verify that each side of the link has distinct nodes."""
        assert (
            self.left.node_type != self.right.node_type
        ), "Linking ports of the same node is unsupported."
        return self


@unique
class TrafficGeneratorType(str, Enum):
    """The supported traffic generators."""

    #:
    SCAPY = "SCAPY"


class TrafficGeneratorConfig(FrozenModel):
    """A protocol required to define traffic generator types."""

    #: The traffic generator type the child class is required to define to be distinguished among
    #: others.
    type: TrafficGeneratorType


class ScapyTrafficGeneratorConfig(TrafficGeneratorConfig):
    """Scapy traffic generator specific configuration."""

    type: Literal[TrafficGeneratorType.SCAPY]


#: A union type discriminating traffic generators by the `type` field.
TrafficGeneratorConfigTypes = Annotated[ScapyTrafficGeneratorConfig, Field(discriminator="type")]

#: Comma-separated list of logical cores to use. An empty string or ```any``` means use all lcores.
LogicalCores = Annotated[
    str,
    Field(
        examples=["1,2,3,4,5,18-22", "10-15", "any"],
        pattern=r"^(([0-9]+|([0-9]+-[0-9]+))(,([0-9]+|([0-9]+-[0-9]+)))*)?$|any",
    ),
]


class DPDKRuntimeConfiguration(FrozenModel):
    """Configuration of the DPDK EAL parameters."""

    #: A comma delimited list of logical cores to use when running DPDK. ```any```, an empty
    #: string or omitting this field means use any core except for the first one. The first core
    #: will only be used if explicitly set.
    lcores: LogicalCores = ""

    #: The number of memory channels to use when running DPDK.
    memory_channels: int = 1

    #: The names of virtual devices to test.
    vdevs: list[str] = Field(default_factory=list)

    @property
    def use_first_core(self) -> bool:
        """Returns :data:`True` if `lcores` explicitly selects the first core."""
        return "0" in self.lcores


class DPDKConfiguration(DPDKRuntimeConfiguration):
    """The DPDK configuration needed to test."""

    #: The DPDK build configuration used to test.
    build: DPDKBuildConfiguration


class TestRunConfiguration(FrozenModel):
    """The configuration of a test run.

    The configuration contains testbed information, what tests to execute
    and with what DPDK build.
    """

    #: The DPDK configuration used to test.
    dpdk: DPDKConfiguration
    #: The traffic generator configuration used to test.
    traffic_generator: TrafficGeneratorConfigTypes
    #: Whether to run performance tests.
    perf: bool
    #: Whether to run functional tests.
    func: bool
    #: Whether to skip smoke tests.
    skip_smoke_tests: bool = False
    #: The names of test suites and/or test cases to execute.
    test_suites: list[TestSuiteConfig] = Field(default_factory=fetch_all_test_suites)
    #: The SUT node name to use in this test run.
    system_under_test_node: str
    #: The TG node name to use in this test run.
    traffic_generator_node: str
    #: The seed to use for pseudo-random generation.
    random_seed: int | None = None
    #: The port links between the specified nodes to use.
    port_topology: list[PortLinkConfig] = Field(max_length=2)

    fields_from_settings = model_validator(mode="before")(
        load_fields_from_settings("test_suites", "random_seed")
    )

    def filter_tests(
        self, tests_config: dict[str, "BaseConfig"]
    ) -> Iterable[tuple[type["TestSuite"], "BaseConfig", deque[type["TestCase"]]]]:
        """Filter test suites and cases selected for execution."""
        from framework.test_suite import TestCaseType

        test_suites = [TestSuiteConfig(test_suite="smoke_tests")]

        if self.skip_smoke_tests:
            test_suites = self.test_suites
        else:
            test_suites += self.test_suites

        return (
            (
                t.test_suite_spec.class_obj,
                tests_config[t.test_suite_name],
                deque(
                    tt
                    for tt in t.test_cases
                    if (tt.test_type is TestCaseType.FUNCTIONAL and self.func)
                    or (tt.test_type is TestCaseType.PERFORMANCE and self.perf)
                ),
            )
            for t in test_suites
        )
