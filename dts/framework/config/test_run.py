# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Configuration models representing a test run.

The root model of a test run configuration is :class:`TestRunConfiguration`.
"""

import tarfile
from enum import auto, unique
from functools import cached_property
from pathlib import Path, PurePath
from typing import Any, Literal

from pydantic import Field, field_validator, model_validator
from typing_extensions import TYPE_CHECKING, Self

from framework.utils import StrEnum

from .common import FrozenModel, load_fields_from_settings

if TYPE_CHECKING:
    from framework.test_suite import TestSuiteSpec


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

        test_runs:
        - test_suites:
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

        This validator relies on the cached property `test_suite_spec` to run for the first
        time in this call, therefore triggering the assertions if needed.
        """
        available_test_cases = map(
            lambda t: t.name, self.test_suite_spec.class_obj.get_test_cases()
        )
        for requested_test_case in self.test_cases_names:
            assert requested_test_case in available_test_cases, (
                f"{requested_test_case} is not a valid test case "
                f"of test suite {self.test_suite_name}."
            )

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


class TestRunConfiguration(FrozenModel):
    """The configuration of a test run.

    The configuration contains testbed information, what tests to execute
    and with what DPDK build.
    """

    #: The DPDK configuration used to test.
    dpdk_config: DPDKBuildConfiguration = Field(alias="dpdk_build")
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
    #: The names of virtual devices to test.
    vdevs: list[str] = Field(default_factory=list)
    #: The seed to use for pseudo-random generation.
    random_seed: int | None = None

    fields_from_settings = model_validator(mode="before")(
        load_fields_from_settings("test_suites", "random_seed")
    )
