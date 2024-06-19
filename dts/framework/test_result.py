# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

r"""Record and process DTS results.

The results are recorded in a hierarchical manner:

    * :class:`DTSResult` contains
    * :class:`TestRunResult` contains
    * :class:`BuildTargetResult` contains
    * :class:`TestSuiteResult` contains
    * :class:`TestCaseResult`

Each result may contain multiple lower level results, e.g. there are multiple
:class:`TestSuiteResult`\s in a :class:`BuildTargetResult`.
The results have common parts, such as setup and teardown results, captured in :class:`BaseResult`,
which also defines some common behaviors in its methods.

Each result class has its own idiosyncrasies which they implement in overridden methods.

The :option:`--output` command line argument and the :envvar:`DTS_OUTPUT_DIR` environment
variable modify the directory where the files with results will be stored.
"""

import os.path
from collections.abc import MutableSequence
from dataclasses import dataclass
from enum import Enum, auto
from types import FunctionType
from typing import Union

from .config import (
    OS,
    Architecture,
    BuildTargetConfiguration,
    BuildTargetInfo,
    Compiler,
    CPUType,
    NodeInfo,
    TestRunConfiguration,
    TestSuiteConfig,
)
from .exception import DTSError, ErrorSeverity
from .logger import DTSLogger
from .settings import SETTINGS
from .test_suite import TestSuite


@dataclass(slots=True, frozen=True)
class TestSuiteWithCases:
    """A test suite class with test case methods.

    An auxiliary class holding a test case class with test case methods. The intended use of this
    class is to hold a subset of test cases (which could be all test cases) because we don't have
    all the data to instantiate the class at the point of inspection. The knowledge of this subset
    is needed in case an error occurs before the class is instantiated and we need to record
    which test cases were blocked by the error.

    Attributes:
        test_suite_class: The test suite class.
        test_cases: The test case methods.
    """

    test_suite_class: type[TestSuite]
    test_cases: list[FunctionType]

    def create_config(self) -> TestSuiteConfig:
        """Generate a :class:`TestSuiteConfig` from the stored test suite with test cases.

        Returns:
            The :class:`TestSuiteConfig` representation.
        """
        return TestSuiteConfig(
            test_suite=self.test_suite_class.__name__,
            test_cases=[test_case.__name__ for test_case in self.test_cases],
        )


class Result(Enum):
    """The possible states that a setup, a teardown or a test case may end up in."""

    #:
    PASS = auto()
    #:
    FAIL = auto()
    #:
    ERROR = auto()
    #:
    SKIP = auto()
    #:
    BLOCK = auto()

    def __bool__(self) -> bool:
        """Only PASS is True."""
        return self is self.PASS


class FixtureResult:
    """A record that stores the result of a setup or a teardown.

    :attr:`~Result.FAIL` is a sensible default since it prevents false positives (which could happen
    if the default was :attr:`~Result.PASS`).

    Preventing false positives or other false results is preferable since a failure
    is mostly likely to be investigated (the other false results may not be investigated at all).

    Attributes:
        result: The associated result.
        error: The error in case of a failure.
    """

    result: Result
    error: Exception | None = None

    def __init__(
        self,
        result: Result = Result.FAIL,
        error: Exception | None = None,
    ):
        """Initialize the constructor with the fixture result and store a possible error.

        Args:
            result: The result to store.
            error: The error which happened when a failure occurred.
        """
        self.result = result
        self.error = error

    def __bool__(self) -> bool:
        """A wrapper around the stored :class:`Result`."""
        return bool(self.result)


class BaseResult:
    """Common data and behavior of DTS results.

    Stores the results of the setup and teardown portions of the corresponding stage.
    The hierarchical nature of DTS results is captured recursively in an internal list.
    A stage is each level in this particular hierarchy (pre-run or the top-most level,
    test run, build target, test suite and test case.)

    Attributes:
        setup_result: The result of the setup of the particular stage.
        teardown_result: The results of the teardown of the particular stage.
        child_results: The results of the descendants in the results hierarchy.
    """

    setup_result: FixtureResult
    teardown_result: FixtureResult
    child_results: MutableSequence["BaseResult"]

    def __init__(self):
        """Initialize the constructor."""
        self.setup_result = FixtureResult()
        self.teardown_result = FixtureResult()
        self.child_results = []

    def update_setup(self, result: Result, error: Exception | None = None) -> None:
        """Store the setup result.

        If the result is :attr:`~Result.BLOCK`, :attr:`~Result.ERROR` or :attr:`~Result.FAIL`,
        then the corresponding child results in result hierarchy
        are also marked with :attr:`~Result.BLOCK`.

        Args:
            result: The result of the setup.
            error: The error that occurred in case of a failure.
        """
        self.setup_result.result = result
        self.setup_result.error = error

        if result in [Result.BLOCK, Result.ERROR, Result.FAIL]:
            self.update_teardown(Result.BLOCK)
            self._block_result()

    def _block_result(self) -> None:
        r"""Mark the result as :attr:`~Result.BLOCK`\ed.

        The blocking of child results should be done in overloaded methods.
        """

    def update_teardown(self, result: Result, error: Exception | None = None) -> None:
        """Store the teardown result.

        Args:
            result: The result of the teardown.
            error: The error that occurred in case of a failure.
        """
        self.teardown_result.result = result
        self.teardown_result.error = error

    def _get_setup_teardown_errors(self) -> list[Exception]:
        errors = []
        if self.setup_result.error:
            errors.append(self.setup_result.error)
        if self.teardown_result.error:
            errors.append(self.teardown_result.error)
        return errors

    def _get_child_errors(self) -> list[Exception]:
        return [error for child_result in self.child_results for error in child_result.get_errors()]

    def get_errors(self) -> list[Exception]:
        """Compile errors from the whole result hierarchy.

        Returns:
            The errors from setup, teardown and all errors found in the whole result hierarchy.
        """
        return self._get_setup_teardown_errors() + self._get_child_errors()

    def add_stats(self, statistics: "Statistics") -> None:
        """Collate stats from the whole result hierarchy.

        Args:
            statistics: The :class:`Statistics` object where the stats will be collated.
        """
        for child_result in self.child_results:
            child_result.add_stats(statistics)


class DTSResult(BaseResult):
    """Stores environment information and test results from a DTS run.

        * Test run level information, such as testbed and the test suite list,
        * Build target level information, such as compiler, target OS and cpu,
        * Test suite and test case results,
        * All errors that are caught and recorded during DTS execution.

    The information is stored hierarchically. This is the first level of the hierarchy
    and as such is where the data form the whole hierarchy is collated or processed.

    The internal list stores the results of all test runs.

    Attributes:
        dpdk_version: The DPDK version to record.
    """

    dpdk_version: str | None
    _logger: DTSLogger
    _errors: list[Exception]
    _return_code: ErrorSeverity
    _stats_result: Union["Statistics", None]
    _stats_filename: str

    def __init__(self, logger: DTSLogger):
        """Extend the constructor with top-level specifics.

        Args:
            logger: The logger instance the whole result will use.
        """
        super().__init__()
        self.dpdk_version = None
        self._logger = logger
        self._errors = []
        self._return_code = ErrorSeverity.NO_ERR
        self._stats_result = None
        self._stats_filename = os.path.join(SETTINGS.output_dir, "statistics.txt")

    def add_test_run(self, test_run_config: TestRunConfiguration) -> "TestRunResult":
        """Add and return the child result (test run).

        Args:
            test_run_config: A test run configuration.

        Returns:
            The test run's result.
        """
        result = TestRunResult(test_run_config)
        self.child_results.append(result)
        return result

    def add_error(self, error: Exception) -> None:
        """Record an error that occurred outside any test run.

        Args:
            error: The exception to record.
        """
        self._errors.append(error)

    def process(self) -> None:
        """Process the data after a whole DTS run.

        The data is added to child objects during runtime and this object is not updated
        at that time. This requires us to process the child data after it's all been gathered.

        The processing gathers all errors and the statistics of test case results.
        """
        self._errors += self.get_errors()
        if self._errors and self._logger:
            self._logger.debug("Summary of errors:")
            for error in self._errors:
                self._logger.debug(repr(error))

        self._stats_result = Statistics(self.dpdk_version)
        self.add_stats(self._stats_result)
        with open(self._stats_filename, "w+") as stats_file:
            stats_file.write(str(self._stats_result))

    def get_return_code(self) -> int:
        """Go through all stored Exceptions and return the final DTS error code.

        Returns:
            The highest error code found.
        """
        for error in self._errors:
            error_return_code = ErrorSeverity.GENERIC_ERR
            if isinstance(error, DTSError):
                error_return_code = error.severity

            if error_return_code > self._return_code:
                self._return_code = error_return_code

        return int(self._return_code)


class TestRunResult(BaseResult):
    """The test run specific result.

    The internal list stores the results of all build targets in a given test run.

    Attributes:
        sut_os_name: The operating system of the SUT node.
        sut_os_version: The operating system version of the SUT node.
        sut_kernel_version: The operating system kernel version of the SUT node.
    """

    sut_os_name: str
    sut_os_version: str
    sut_kernel_version: str
    _config: TestRunConfiguration
    _parent_result: DTSResult
    _test_suites_with_cases: list[TestSuiteWithCases]

    def __init__(self, test_run_config: TestRunConfiguration):
        """Extend the constructor with the test run's config and DTSResult.

        Args:
            test_run_config: A test run configuration.
        """
        super().__init__()
        self._config = test_run_config
        self._test_suites_with_cases = []

    def add_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> "BuildTargetResult":
        """Add and return the child result (build target).

        Args:
            build_target_config: The build target's test run configuration.

        Returns:
            The build target's result.
        """
        result = BuildTargetResult(
            self._test_suites_with_cases,
            build_target_config,
        )
        self.child_results.append(result)
        return result

    @property
    def test_suites_with_cases(self) -> list[TestSuiteWithCases]:
        """The test suites with test cases to be executed in this test run.

        The test suites can only be assigned once.

        Returns:
            The list of test suites with test cases. If an error occurs between
            the initialization of :class:`TestRunResult` and assigning test cases to the instance,
            return an empty list, representing that we don't know what to execute.
        """
        return self._test_suites_with_cases

    @test_suites_with_cases.setter
    def test_suites_with_cases(self, test_suites_with_cases: list[TestSuiteWithCases]) -> None:
        if self._test_suites_with_cases:
            raise ValueError(
                "Attempted to assign test suites to a test run result "
                "which already has test suites."
            )
        self._test_suites_with_cases = test_suites_with_cases

    def add_sut_info(self, sut_info: NodeInfo) -> None:
        """Add SUT information gathered at runtime.

        Args:
            sut_info: The additional SUT node information.
        """
        self.sut_os_name = sut_info.os_name
        self.sut_os_version = sut_info.os_version
        self.sut_kernel_version = sut_info.kernel_version

    def _block_result(self) -> None:
        r"""Mark the result as :attr:`~Result.BLOCK`\ed."""
        for build_target in self._config.build_targets:
            child_result = self.add_build_target(build_target)
            child_result.update_setup(Result.BLOCK)


class BuildTargetResult(BaseResult):
    """The build target specific result.

    The internal list stores the results of all test suites in a given build target.

    Attributes:
        arch: The DPDK build target architecture.
        os: The DPDK build target operating system.
        cpu: The DPDK build target CPU.
        compiler: The DPDK build target compiler.
        compiler_version: The DPDK build target compiler version.
        dpdk_version: The built DPDK version.
    """

    arch: Architecture
    os: OS
    cpu: CPUType
    compiler: Compiler
    compiler_version: str | None
    dpdk_version: str | None
    _test_suites_with_cases: list[TestSuiteWithCases]

    def __init__(
        self,
        test_suites_with_cases: list[TestSuiteWithCases],
        build_target_config: BuildTargetConfiguration,
    ):
        """Extend the constructor with the build target's config and test suites with cases.

        Args:
            test_suites_with_cases: The test suites with test cases to be run in this build target.
            build_target_config: The build target's test run configuration.
        """
        super().__init__()
        self.arch = build_target_config.arch
        self.os = build_target_config.os
        self.cpu = build_target_config.cpu
        self.compiler = build_target_config.compiler
        self.compiler_version = None
        self.dpdk_version = None
        self._test_suites_with_cases = test_suites_with_cases

    def add_test_suite(
        self,
        test_suite_with_cases: TestSuiteWithCases,
    ) -> "TestSuiteResult":
        """Add and return the child result (test suite).

        Args:
            test_suite_with_cases: The test suite with test cases.

        Returns:
            The test suite's result.
        """
        result = TestSuiteResult(test_suite_with_cases)
        self.child_results.append(result)
        return result

    def add_build_target_info(self, versions: BuildTargetInfo) -> None:
        """Add information about the build target gathered at runtime.

        Args:
            versions: The additional information.
        """
        self.compiler_version = versions.compiler_version
        self.dpdk_version = versions.dpdk_version

    def _block_result(self) -> None:
        r"""Mark the result as :attr:`~Result.BLOCK`\ed."""
        for test_suite_with_cases in self._test_suites_with_cases:
            child_result = self.add_test_suite(test_suite_with_cases)
            child_result.update_setup(Result.BLOCK)


class TestSuiteResult(BaseResult):
    """The test suite specific result.

    The internal list stores the results of all test cases in a given test suite.

    Attributes:
        test_suite_name: The test suite name.
    """

    test_suite_name: str
    _test_suite_with_cases: TestSuiteWithCases
    _parent_result: BuildTargetResult
    _child_configs: list[str]

    def __init__(self, test_suite_with_cases: TestSuiteWithCases):
        """Extend the constructor with test suite's config and BuildTargetResult.

        Args:
            test_suite_with_cases: The test suite with test cases.
        """
        super().__init__()
        self.test_suite_name = test_suite_with_cases.test_suite_class.__name__
        self._test_suite_with_cases = test_suite_with_cases

    def add_test_case(self, test_case_name: str) -> "TestCaseResult":
        """Add and return the child result (test case).

        Args:
            test_case_name: The name of the test case.

        Returns:
            The test case's result.
        """
        result = TestCaseResult(test_case_name)
        self.child_results.append(result)
        return result

    def _block_result(self) -> None:
        r"""Mark the result as :attr:`~Result.BLOCK`\ed."""
        for test_case_method in self._test_suite_with_cases.test_cases:
            child_result = self.add_test_case(test_case_method.__name__)
            child_result.update_setup(Result.BLOCK)


class TestCaseResult(BaseResult, FixtureResult):
    r"""The test case specific result.

    Stores the result of the actual test case. This is done by adding an extra superclass
    in :class:`FixtureResult`. The setup and teardown results are :class:`FixtureResult`\s and
    the class is itself a record of the test case.

    Attributes:
        test_case_name: The test case name.
    """

    test_case_name: str

    def __init__(self, test_case_name: str):
        """Extend the constructor with test case's name and TestSuiteResult.

        Args:
            test_case_name: The test case's name.
        """
        super().__init__()
        self.test_case_name = test_case_name

    def update(self, result: Result, error: Exception | None = None) -> None:
        """Update the test case result.

        This updates the result of the test case itself and doesn't affect
        the results of the setup and teardown steps in any way.

        Args:
            result: The result of the test case.
            error: The error that occurred in case of a failure.
        """
        self.result = result
        self.error = error

    def _get_child_errors(self) -> list[Exception]:
        if self.error:
            return [self.error]
        return []

    def add_stats(self, statistics: "Statistics") -> None:
        r"""Add the test case result to statistics.

        The base method goes through the hierarchy recursively and this method is here to stop
        the recursion, as the :class:`TestCaseResult`\s are the leaves of the hierarchy tree.

        Args:
            statistics: The :class:`Statistics` object where the stats will be added.
        """
        statistics += self.result

    def _block_result(self) -> None:
        r"""Mark the result as :attr:`~Result.BLOCK`\ed."""
        self.update(Result.BLOCK)

    def __bool__(self) -> bool:
        """The test case passed only if setup, teardown and the test case itself passed."""
        return bool(self.setup_result) and bool(self.teardown_result) and bool(self.result)


class Statistics(dict):
    """How many test cases ended in which result state along some other basic information.

    Subclassing :class:`dict` provides a convenient way to format the data.

    The data are stored in the following keys:

    * **PASS RATE** (:class:`int`) -- The FAIL/PASS ratio of all test cases.
    * **DPDK VERSION** (:class:`str`) -- The tested DPDK version.
    """

    def __init__(self, dpdk_version: str | None):
        """Extend the constructor with keys in which the data are stored.

        Args:
            dpdk_version: The version of tested DPDK.
        """
        super().__init__()
        for result in Result:
            self[result.name] = 0
        self["PASS RATE"] = 0.0
        self["DPDK VERSION"] = dpdk_version

    def __iadd__(self, other: Result) -> "Statistics":
        """Add a Result to the final count.

        Example:
            stats: Statistics = Statistics()  # empty Statistics
            stats += Result.PASS  # add a Result to `stats`

        Args:
            other: The Result to add to this statistics object.

        Returns:
            The modified statistics object.
        """
        self[other.name] += 1
        self["PASS RATE"] = (
            float(self[Result.PASS.name]) * 100 / sum(self[result.name] for result in Result)
        )
        return self

    def __str__(self) -> str:
        """Each line contains the formatted key = value pair."""
        stats_str = ""
        for key, value in self.items():
            stats_str += f"{key:<12} = {value}\n"
            # according to docs, we should use \n when writing to text files
            # on all platforms
        return stats_str
