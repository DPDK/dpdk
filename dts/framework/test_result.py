# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""
Generic result container and reporters
"""

import os.path
from collections.abc import MutableSequence
from enum import Enum, auto

from .config import (
    OS,
    Architecture,
    BuildTargetConfiguration,
    Compiler,
    CPUType,
    NodeConfiguration,
)
from .exception import DTSError, ErrorSeverity
from .logger import DTSLOG
from .settings import SETTINGS


class Result(Enum):
    """
    An Enum defining the possible states that
    a setup, a teardown or a test case may end up in.
    """

    PASS = auto()
    FAIL = auto()
    ERROR = auto()
    SKIP = auto()

    def __bool__(self) -> bool:
        return self is self.PASS


class FixtureResult(object):
    """
    A record that stored the result of a setup or a teardown.
    The default is FAIL because immediately after creating the object
    the setup of the corresponding stage will be executed, which also guarantees
    the execution of teardown.
    """

    result: Result
    error: Exception | None = None

    def __init__(
        self,
        result: Result = Result.FAIL,
        error: Exception | None = None,
    ):
        self.result = result
        self.error = error

    def __bool__(self) -> bool:
        return bool(self.result)


class Statistics(dict):
    """
    A helper class used to store the number of test cases by its result
    along a few other basic information.
    Using a dict provides a convenient way to format the data.
    """

    def __init__(self, dpdk_version):
        super(Statistics, self).__init__()
        for result in Result:
            self[result.name] = 0
        self["PASS RATE"] = 0.0
        self["DPDK VERSION"] = dpdk_version

    def __iadd__(self, other: Result) -> "Statistics":
        """
        Add a Result to the final count.
        """
        self[other.name] += 1
        self["PASS RATE"] = (
            float(self[Result.PASS.name])
            * 100
            / sum(self[result.name] for result in Result)
        )
        return self

    def __str__(self) -> str:
        """
        Provide a string representation of the data.
        """
        stats_str = ""
        for key, value in self.items():
            stats_str += f"{key:<12} = {value}\n"
            # according to docs, we should use \n when writing to text files
            # on all platforms
        return stats_str


class BaseResult(object):
    """
    The Base class for all results. Stores the results of
    the setup and teardown portions of the corresponding stage
    and a list of results from each inner stage in _inner_results.
    """

    setup_result: FixtureResult
    teardown_result: FixtureResult
    _inner_results: MutableSequence["BaseResult"]

    def __init__(self):
        self.setup_result = FixtureResult()
        self.teardown_result = FixtureResult()
        self._inner_results = []

    def update_setup(self, result: Result, error: Exception | None = None) -> None:
        self.setup_result.result = result
        self.setup_result.error = error

    def update_teardown(self, result: Result, error: Exception | None = None) -> None:
        self.teardown_result.result = result
        self.teardown_result.error = error

    def _get_setup_teardown_errors(self) -> list[Exception]:
        errors = []
        if self.setup_result.error:
            errors.append(self.setup_result.error)
        if self.teardown_result.error:
            errors.append(self.teardown_result.error)
        return errors

    def _get_inner_errors(self) -> list[Exception]:
        return [
            error
            for inner_result in self._inner_results
            for error in inner_result.get_errors()
        ]

    def get_errors(self) -> list[Exception]:
        return self._get_setup_teardown_errors() + self._get_inner_errors()

    def add_stats(self, statistics: Statistics) -> None:
        for inner_result in self._inner_results:
            inner_result.add_stats(statistics)


class TestCaseResult(BaseResult, FixtureResult):
    """
    The test case specific result.
    Stores the result of the actual test case.
    Also stores the test case name.
    """

    test_case_name: str

    def __init__(self, test_case_name: str):
        super(TestCaseResult, self).__init__()
        self.test_case_name = test_case_name

    def update(self, result: Result, error: Exception | None = None) -> None:
        self.result = result
        self.error = error

    def _get_inner_errors(self) -> list[Exception]:
        if self.error:
            return [self.error]
        return []

    def add_stats(self, statistics: Statistics) -> None:
        statistics += self.result

    def __bool__(self) -> bool:
        return (
            bool(self.setup_result) and bool(self.teardown_result) and bool(self.result)
        )


class TestSuiteResult(BaseResult):
    """
    The test suite specific result.
    The _inner_results list stores results of test cases in a given test suite.
    Also stores the test suite name.
    """

    suite_name: str

    def __init__(self, suite_name: str):
        super(TestSuiteResult, self).__init__()
        self.suite_name = suite_name

    def add_test_case(self, test_case_name: str) -> TestCaseResult:
        test_case_result = TestCaseResult(test_case_name)
        self._inner_results.append(test_case_result)
        return test_case_result


class BuildTargetResult(BaseResult):
    """
    The build target specific result.
    The _inner_results list stores results of test suites in a given build target.
    Also stores build target specifics, such as compiler used to build DPDK.
    """

    arch: Architecture
    os: OS
    cpu: CPUType
    compiler: Compiler

    def __init__(self, build_target: BuildTargetConfiguration):
        super(BuildTargetResult, self).__init__()
        self.arch = build_target.arch
        self.os = build_target.os
        self.cpu = build_target.cpu
        self.compiler = build_target.compiler

    def add_test_suite(self, test_suite_name: str) -> TestSuiteResult:
        test_suite_result = TestSuiteResult(test_suite_name)
        self._inner_results.append(test_suite_result)
        return test_suite_result


class ExecutionResult(BaseResult):
    """
    The execution specific result.
    The _inner_results list stores results of build targets in a given execution.
    Also stores the SUT node configuration.
    """

    sut_node: NodeConfiguration

    def __init__(self, sut_node: NodeConfiguration):
        super(ExecutionResult, self).__init__()
        self.sut_node = sut_node

    def add_build_target(
        self, build_target: BuildTargetConfiguration
    ) -> BuildTargetResult:
        build_target_result = BuildTargetResult(build_target)
        self._inner_results.append(build_target_result)
        return build_target_result


class DTSResult(BaseResult):
    """
    Stores environment information and test results from a DTS run, which are:
    * Execution level information, such as SUT and TG hardware.
    * Build target level information, such as compiler, target OS and cpu.
    * Test suite results.
    * All errors that are caught and recorded during DTS execution.

    The information is stored in nested objects.

    The class is capable of computing the return code used to exit DTS with
    from the stored error.

    It also provides a brief statistical summary of passed/failed test cases.
    """

    dpdk_version: str | None
    _logger: DTSLOG
    _errors: list[Exception]
    _return_code: ErrorSeverity
    _stats_result: Statistics | None
    _stats_filename: str

    def __init__(self, logger: DTSLOG):
        super(DTSResult, self).__init__()
        self.dpdk_version = None
        self._logger = logger
        self._errors = []
        self._return_code = ErrorSeverity.NO_ERR
        self._stats_result = None
        self._stats_filename = os.path.join(SETTINGS.output_dir, "statistics.txt")

    def add_execution(self, sut_node: NodeConfiguration) -> ExecutionResult:
        execution_result = ExecutionResult(sut_node)
        self._inner_results.append(execution_result)
        return execution_result

    def add_error(self, error) -> None:
        self._errors.append(error)

    def process(self) -> None:
        """
        Process the data after a DTS run.
        The data is added to nested objects during runtime and this parent object
        is not updated at that time. This requires us to process the nested data
        after it's all been gathered.

        The processing gathers all errors and the result statistics of test cases.
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
        """
        Go through all stored Exceptions and return the highest error code found.
        """
        for error in self._errors:
            error_return_code = ErrorSeverity.GENERIC_ERR
            if isinstance(error, DTSError):
                error_return_code = error.severity

            if error_return_code > self._return_code:
                self._return_code = error_return_code

        return int(self._return_code)
