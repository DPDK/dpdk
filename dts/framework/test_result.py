# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

r"""Record and process DTS results.

The results are recorded in a hierarchical manner:

    * :class:`DTSResult` contains
    * :class:`TestRunResult` contains
    * :class:`TestSuiteResult` contains
    * :class:`TestCaseResult`

Each result may contain multiple lower level results, e.g. there are multiple
:class:`TestSuiteResult`\s in a :class:`TestRunResult`.
The results have common parts, such as setup and teardown results, captured in :class:`BaseResult`,
which also defines some common behaviors in its methods.

Each result class has its own idiosyncrasies which they implement in overridden methods.

The :option:`--output` command line argument and the :envvar:`DTS_OUTPUT_DIR` environment
variable modify the directory where the files with results will be stored.
"""

import json
from collections.abc import MutableSequence
from enum import Enum, auto
from pathlib import Path
from typing import Any, Callable, TypedDict

from .config.test_run import TestRunConfiguration
from .exception import DTSError, ErrorSeverity
from .logger import DTSLogger
from .remote_session.dpdk import DPDKBuildInfo
from .testbed_model.os_session import OSSessionInfo
from .testbed_model.port import Port


class Result(Enum):
    """The possible states that a setup, a teardown or a test case may end up in."""

    #:
    PASS = auto()
    #:
    FAIL = auto()
    #:
    ERROR = auto()
    #:
    BLOCK = auto()
    #:
    SKIP = auto()

    def __bool__(self) -> bool:
        """Only :attr:`PASS` is True."""
        return self is self.PASS


class TestCaseResultDict(TypedDict):
    """Represents the `TestCaseResult` results.

    Attributes:
        test_case_name: The name of the test case.
        result: The result name of the test case.
    """

    test_case_name: str
    result: str


class TestSuiteResultDict(TypedDict):
    """Represents the `TestSuiteResult` results.

    Attributes:
        test_suite_name: The name of the test suite.
        test_cases: A list of test case results contained in this test suite.
    """

    test_suite_name: str
    test_cases: list[TestCaseResultDict]


class TestRunResultDict(TypedDict, total=False):
    """Represents the `TestRunResult` results.

    Attributes:
        compiler_version: The version of the compiler used for the DPDK build.
        dpdk_version: The version of DPDK being tested.
        ports: A list of ports associated with the test run.
        test_suites: A list of test suite results included in this test run.
        summary: A dictionary containing overall results, such as pass/fail counts.
    """

    compiler_version: str | None
    dpdk_version: str | None
    ports: list[dict[str, Any]]
    test_suites: list[TestSuiteResultDict]
    summary: dict[str, int | float]


class DtsRunResultDict(TypedDict):
    """Represents the `DtsRunResult` results.

    Attributes:
        test_runs: A list of test run results.
        summary: A summary dictionary containing overall statistics for the test runs.
    """

    test_runs: list[TestRunResultDict]
    summary: dict[str, int | float]


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
    test run, test suite and test case).

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

        if result != Result.PASS:
            result_to_mark = Result.BLOCK if result != Result.SKIP else Result.SKIP
            self.update_teardown(result_to_mark)
            self._mark_results(result_to_mark)

    def _mark_results(self, result) -> None:
        """Mark the child results or the result of the level itself as `result`.

        The marking of results should be done in overloaded methods.
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

    def to_dict(self):
        """Convert the results hierarchy into a dictionary representation."""

    def add_result(self, results: dict[str, int]):
        """Collate the test case result to the given result hierarchy.

        Args:
            results: The dictionary in which results will be collated.
        """
        for child_result in self.child_results:
            child_result.add_result(results)

    def generate_pass_rate_dict(self, test_run_summary) -> dict[str, float]:
        """Generate a dictionary with the PASS/FAIL ratio of all test cases.

        Args:
            test_run_summary: The summary dictionary containing test result counts.

        Returns:
            A dictionary with the PASS/FAIL ratio of all test cases.
        """
        cases_not_skipped = sum(
            test_run_summary[result.name] for result in Result if result != Result.SKIP
        )
        return {"PASS_RATE": test_run_summary[Result.PASS.name] * 100.0 / max(cases_not_skipped, 1)}


class DTSResult(BaseResult):
    """Stores environment information and test results from a DTS run.

        * Test run level information, such as testbed, the test suite list and
          DPDK build compiler configuration,
        * Test suite and test case results,
        * All errors that are caught and recorded during DTS execution.

    The information is stored hierarchically. This is the first level of the hierarchy
    and as such is where the data form the whole hierarchy is collated or processed.

    The internal list stores the results of all test runs.
    """

    _output_dir: str
    _logger: DTSLogger
    _errors: list[Exception]
    _return_code: ErrorSeverity

    def __init__(self, output_dir: str, logger: DTSLogger):
        """Extend the constructor with top-level specifics.

        Args:
            output_dir: The directory where DTS logs and results are saved.
            logger: The logger instance the whole result will use.
        """
        super().__init__()
        self._output_dir = output_dir
        self._logger = logger
        self._errors = []
        self._return_code = ErrorSeverity.NO_ERR

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

        TextSummary(self).save(Path(self._output_dir, "results_summary.txt"))
        JsonResults(self).save(Path(self._output_dir, "results.json"))

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

    def to_dict(self) -> DtsRunResultDict:
        """Convert DTS result into a dictionary format.

        The dictionary contains test runs and summary of test runs.

        Returns:
            A dictionary representation of the DTS result
        """

        def merge_test_run_summaries(test_run_summaries: list[dict[str, int]]) -> dict[str, int]:
            """Merge multiple test run summaries into one dictionary.

            Args:
                test_run_summaries: List of test run summary dictionaries.

            Returns:
                A merged dictionary containing the aggregated summary.
            """
            return {
                key.name: sum(test_run_summary[key.name] for test_run_summary in test_run_summaries)
                for key in Result
            }

        test_runs = [child.to_dict() for child in self.child_results]
        test_run_summary = merge_test_run_summaries([test_run["summary"] for test_run in test_runs])

        return {
            "test_runs": test_runs,
            "summary": test_run_summary | self.generate_pass_rate_dict(test_run_summary),
        }


class TestRunResult(BaseResult):
    """The test run specific result.

    The internal list stores the results of all test suites in a given test run.

    Attributes:
        compiler_version: The DPDK build compiler version.
        dpdk_version: The built DPDK version.
        sut_os_name: The operating system of the SUT node.
        sut_os_version: The operating system version of the SUT node.
        sut_kernel_version: The operating system kernel version of the SUT node.
    """

    _config: TestRunConfiguration
    _ports: list[Port]
    _sut_info: OSSessionInfo | None
    _dpdk_build_info: DPDKBuildInfo | None

    def __init__(self, test_run_config: TestRunConfiguration):
        """Extend the constructor with the test run's config.

        Args:
            test_run_config: A test run configuration.
        """
        super().__init__()
        self._config = test_run_config
        self._ports = []
        self._sut_info = None
        self._dpdk_build_info = None

    def add_test_suite(self, test_suite_name: str) -> "TestSuiteResult":
        """Add and return the child result (test suite).

        Args:
            test_suite_name: The test suite name.

        Returns:
            The test suite's result.
        """
        result = TestSuiteResult(test_suite_name)
        self.child_results.append(result)
        return result

    @property
    def ports(self) -> list[Port]:
        """Get the list of ports associated with this test run."""
        return self._ports

    @ports.setter
    def ports(self, ports: list[Port]) -> None:
        """Set the list of ports associated with this test run.

        Args:
            ports: The list of ports to associate with this test run.

        Raises:
            ValueError: If the ports have already been assigned to this test run.
        """
        if self._ports:
            raise ValueError(
                "Attempted to assign `ports` to a test run result which already has `ports`."
            )
        self._ports = ports

    @property
    def sut_info(self) -> OSSessionInfo | None:
        """Get the SUT OS session information associated with this test run."""
        return self._sut_info

    @sut_info.setter
    def sut_info(self, sut_info: OSSessionInfo) -> None:
        """Set the SUT node information associated with this test run.

        Args:
            sut_info: The SUT node information to associate with this test run.

        Raises:
            ValueError: If the SUT information has already been assigned to this test run.
        """
        if self._sut_info:
            raise ValueError(
                "Attempted to assign `sut_info` to a test run result which already has `sut_info`."
            )
        self._sut_info = sut_info

    @property
    def dpdk_build_info(self) -> DPDKBuildInfo | None:
        """Get the DPDK build information associated with this test run."""
        return self._dpdk_build_info

    @dpdk_build_info.setter
    def dpdk_build_info(self, dpdk_build_info: DPDKBuildInfo) -> None:
        """Set the DPDK build information associated with this test run.

        Args:
            dpdk_build_info: The DPDK build information to associate with this test run.

        Raises:
            ValueError: If the DPDK build information has already been assigned to this test run.
        """
        if self._dpdk_build_info:
            raise ValueError(
                "Attempted to assign `dpdk_build_info` to a test run result which already "
                "has `dpdk_build_info`."
            )
        self._dpdk_build_info = dpdk_build_info

    def to_dict(self) -> TestRunResultDict:
        """Convert the test run result into a dictionary.

        The dictionary contains test suites in this test run, and a summary of the test run and
        information about the DPDK version, compiler version and associated ports.

        Returns:
            TestRunResultDict: A dictionary representation of the test run result.
        """
        results = {result.name: 0 for result in Result}
        self.add_result(results)

        compiler_version = None
        dpdk_version = None

        if self.dpdk_build_info:
            compiler_version = self.dpdk_build_info.compiler_version
            dpdk_version = self.dpdk_build_info.dpdk_version

        return {
            "compiler_version": compiler_version,
            "dpdk_version": dpdk_version,
            "ports": [port.to_dict() for port in self.ports],
            "test_suites": [child.to_dict() for child in self.child_results],
            "summary": results | self.generate_pass_rate_dict(results),
        }


class TestSuiteResult(BaseResult):
    """The test suite specific result.

    The internal list stores the results of all test cases in a given test suite.

    Attributes:
        test_suite_name: The test suite name.
    """

    test_suite_name: str
    _child_configs: list[str]

    def __init__(self, test_suite_name: str):
        """Extend the constructor with test suite's config.

        Args:
            test_suite_name: The test suite name.
        """
        super().__init__()
        self.test_suite_name = test_suite_name

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

    def to_dict(self) -> TestSuiteResultDict:
        """Convert the test suite result into a dictionary.

        The dictionary contains a test suite name and test cases given in this test suite.
        """
        return {
            "test_suite_name": self.test_suite_name,
            "test_cases": [child.to_dict() for child in self.child_results],
        }


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
        """Extend the constructor with test case's name.

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

    def to_dict(self) -> TestCaseResultDict:
        """Convert the test case result into a dictionary.

        The dictionary contains a test case name and the result name.
        """
        return {"test_case_name": self.test_case_name, "result": self.result.name}

    def add_result(self, results: dict[str, int]):
        r"""Add the test case result to the results.

        The base method goes through the hierarchy recursively and this method is here to stop
        the recursion, as the :class:`TestCaseResult` are the leaves of the hierarchy tree.

        Args:
            results: The dictionary to which results will be collated.
        """
        results[self.result.name] += 1

    def _mark_results(self, result) -> None:
        r"""Mark the result as `result`."""
        self.update(result)

    def __bool__(self) -> bool:
        """The test case passed only if setup, teardown and the test case itself passed."""
        return bool(self.setup_result) and bool(self.teardown_result) and bool(self.result)


class TextSummary:
    """Generates and saves textual summaries of DTS run results.

    The summary includes:
    * Results of test cases,
    * Compiler version of the DPDK build,
    * DPDK version of the DPDK source tree,
    * Overall summary of results when multiple test runs are present.
    """

    _dict_result: DtsRunResultDict
    _summary: dict[str, int | float]
    _text: str

    def __init__(self, dts_run_result: DTSResult):
        """Initializes with a DTSResult object and converts it to a dictionary format.

        Args:
            dts_run_result: The DTS result.
        """
        self._dict_result = dts_run_result.to_dict()
        self._summary = self._dict_result["summary"]
        self._text = ""

    @property
    def _outdent(self) -> str:
        """Appropriate indentation based on multiple test run results."""
        return "\t" if len(self._dict_result["test_runs"]) > 1 else ""

    def save(self, output_path: Path):
        """Generate and save text statistics to a file.

        Args:
            output_path: The path where the text file will be saved.
        """
        if self._dict_result["test_runs"]:
            with open(f"{output_path}", "w") as fp:
                self._add_test_runs_dict_decorator(self._add_test_run_dict)
                fp.write(self._text)

    def _add_test_runs_dict_decorator(self, func: Callable):
        """Handles multiple test runs and appends results to the summary.

        Adds headers for each test run and overall result when multiple
        test runs are provided.

        Args:
            func: Function to process and add results from each test run.
        """
        if len(self._dict_result["test_runs"]) > 1:
            for idx, test_run_result in enumerate(self._dict_result["test_runs"]):
                self._text += f"TEST_RUN_{idx}\n"
                func(test_run_result)

            self._add_overall_results()
        else:
            func(self._dict_result["test_runs"][0])

    def _add_test_run_dict(self, test_run_dict: TestRunResultDict):
        """Adds the results and the test run attributes of a single test run to the summary.

        Args:
            test_run_dict: Dictionary containing the test run results.
        """
        self._add_column(
            DPDK_VERSION=test_run_dict["dpdk_version"],
            COMPILER_VERSION=test_run_dict["compiler_version"],
            **test_run_dict["summary"],
        )
        self._text += "\n"

    def _add_column(self, **rows):
        """Formats and adds key-value pairs to the summary text.

        Handles cases where values might be None by replacing them with "N/A".

        Args:
            **rows: Arbitrary key-value pairs representing the result data.
        """
        rows = {k: "N/A" if v is None else v for k, v in rows.items()}
        max_length = len(max(rows, key=len))
        for key, value in rows.items():
            self._text += f"{self._outdent}{key:<{max_length}} = {value}\n"

    def _add_overall_results(self):
        """Add overall summary of test runs."""
        self._text += "OVERALL\n"
        self._add_column(**self._summary)


class JsonResults:
    """Save DTS run result in JSON format."""

    _dict_result: DtsRunResultDict

    def __init__(self, dts_run_result: DTSResult):
        """Initializes with a DTSResult object and converts it to a dictionary format.

        Args:
            dts_run_result: The DTS result.
        """
        self._dict_result = dts_run_result.to_dict()

    def save(self, output_path: Path):
        """Save the result to a file as JSON.

        Args:
            output_path: The path where the JSON file will be saved.
        """
        with open(f"{output_path}", "w") as fp:
            json.dump(self._dict_result, fp, indent=4)
