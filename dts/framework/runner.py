# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""Test suite runner module.

The module is responsible for running DTS in a series of stages:

    #. Test run stage,
    #. Build target stage,
    #. Test suite stage,
    #. Test case stage.

The test run and build target stages set up the environment before running test suites.
The test suite stage sets up steps common to all test cases
and the test case stage runs test cases individually.
"""

import importlib
import inspect
import os
import random
import sys
from pathlib import Path
from types import MethodType
from typing import Iterable

from framework.testbed_model.capability import Capability, get_supported_capabilities
from framework.testbed_model.sut_node import SutNode
from framework.testbed_model.tg_node import TGNode

from .config import (
    BuildTargetConfiguration,
    Configuration,
    TestRunConfiguration,
    TestSuiteConfig,
    load_config,
)
from .exception import (
    BlockingTestSuiteError,
    ConfigurationError,
    SSHTimeoutError,
    TestCaseVerifyError,
)
from .logger import DTSLogger, DtsStage, get_dts_logger
from .settings import SETTINGS
from .test_result import (
    BuildTargetResult,
    DTSResult,
    Result,
    TestCaseResult,
    TestRunResult,
    TestSuiteResult,
    TestSuiteWithCases,
)
from .test_suite import TestCase, TestSuite
from .testbed_model.topology import Topology


class DTSRunner:
    r"""Test suite runner class.

    The class is responsible for running tests on testbeds defined in the test run configuration.
    Each setup or teardown of each stage is recorded in a :class:`~framework.test_result.DTSResult`
    or one of its subclasses. The test case results are also recorded.

    If an error occurs, the current stage is aborted, the error is recorded, everything in
    the inner stages is marked as blocked and the run continues in the next iteration
    of the same stage. The return code is the highest `severity` of all
    :class:`~.framework.exception.DTSError`\s.

    Example:
        An error occurs in a build target setup. The current build target is aborted,
        all test suites and their test cases are marked as blocked and the run continues
        with the next build target. If the errored build target was the last one in the
        given test run, the next test run begins.
    """

    _configuration: Configuration
    _logger: DTSLogger
    _result: DTSResult
    _test_suite_class_prefix: str
    _test_suite_module_prefix: str
    _func_test_case_regex: str
    _perf_test_case_regex: str

    def __init__(self):
        """Initialize the instance with configuration, logger, result and string constants."""
        self._configuration = load_config(SETTINGS.config_file_path)
        self._logger = get_dts_logger()
        if not os.path.exists(SETTINGS.output_dir):
            os.makedirs(SETTINGS.output_dir)
        self._logger.add_dts_root_logger_handlers(SETTINGS.verbose, SETTINGS.output_dir)
        self._result = DTSResult(self._logger)
        self._test_suite_class_prefix = "Test"
        self._test_suite_module_prefix = "tests.TestSuite_"
        self._func_test_case_regex = r"test_(?!perf_)"
        self._perf_test_case_regex = r"test_perf_"

    def run(self) -> None:
        """Run all build targets in all test runs from the test run configuration.

        Before running test suites, test runs and build targets are first set up.
        The test runs and build targets defined in the test run configuration are iterated over.
        The test runs define which tests to run and where to run them and build targets define
        the DPDK build setup.

        The tests suites are set up for each test run/build target tuple and each discovered
        test case within the test suite is set up, executed and torn down. After all test cases
        have been executed, the test suite is torn down and the next build target will be tested.

        In order to properly mark test suites and test cases as blocked in case of a failure,
        we need to have discovered which test suites and test cases to run before any failures
        happen. The discovery happens at the earliest point at the start of each test run.

        All the nested steps look like this:

            #. Test run setup

                #. Build target setup

                    #. Test suite setup

                        #. Test case setup
                        #. Test case logic
                        #. Test case teardown

                    #. Test suite teardown

                #. Build target teardown

            #. Test run teardown

        The test cases are filtered according to the specification in the test run configuration and
        the :option:`--test-suite` command line argument or
        the :envvar:`DTS_TESTCASES` environment variable.
        """
        sut_nodes: dict[str, SutNode] = {}
        tg_nodes: dict[str, TGNode] = {}
        try:
            # check the python version of the server that runs dts
            self._check_dts_python_version()
            self._result.update_setup(Result.PASS)

            # for all test run sections
            for test_run_config in self._configuration.test_runs:
                self._logger.set_stage(DtsStage.test_run_setup)
                self._logger.info(
                    f"Running test run with SUT '{test_run_config.system_under_test_node.name}'."
                )
                self._init_random_seed(test_run_config)
                test_run_result = self._result.add_test_run(test_run_config)
                # we don't want to modify the original config, so create a copy
                test_run_test_suites = list(
                    SETTINGS.test_suites if SETTINGS.test_suites else test_run_config.test_suites
                )
                if not test_run_config.skip_smoke_tests:
                    test_run_test_suites[:0] = [TestSuiteConfig.from_dict("smoke_tests")]
                try:
                    test_suites_with_cases = self._get_test_suites_with_cases(
                        test_run_test_suites, test_run_config.func, test_run_config.perf
                    )
                    test_run_result.test_suites_with_cases = test_suites_with_cases
                except Exception as e:
                    self._logger.exception(
                        f"Invalid test suite configuration found: " f"{test_run_test_suites}."
                    )
                    test_run_result.update_setup(Result.FAIL, e)

                else:
                    self._connect_nodes_and_run_test_run(
                        sut_nodes,
                        tg_nodes,
                        test_run_config,
                        test_run_result,
                        test_suites_with_cases,
                    )

        except Exception as e:
            self._logger.exception("An unexpected error has occurred.")
            self._result.add_error(e)
            raise

        finally:
            try:
                self._logger.set_stage(DtsStage.post_run)
                for node in (sut_nodes | tg_nodes).values():
                    node.close()
                self._result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception("The final cleanup of nodes failed.")
                self._result.update_teardown(Result.ERROR, e)

        # we need to put the sys.exit call outside the finally clause to make sure
        # that unexpected exceptions will propagate
        # in that case, the error that should be reported is the uncaught exception as
        # that is a severe error originating from the framework
        # at that point, we'll only have partial results which could be impacted by the
        # error causing the uncaught exception, making them uninterpretable
        self._exit_dts()

    def _check_dts_python_version(self) -> None:
        """Check the required Python version - v3.10."""
        if sys.version_info.major < 3 or (
            sys.version_info.major == 3 and sys.version_info.minor < 10
        ):
            self._logger.warning(
                "DTS execution node's python version is lower than Python 3.10, "
                "is deprecated and will not work in future releases."
            )
            self._logger.warning("Please use Python >= 3.10 instead.")

    def _get_test_suites_with_cases(
        self,
        test_suite_configs: list[TestSuiteConfig],
        func: bool,
        perf: bool,
    ) -> list[TestSuiteWithCases]:
        """Test suites with test cases discovery.

        The test suites with test cases defined in the user configuration are discovered
        and stored for future use so that we don't import the modules twice and so that
        the list of test suites with test cases is available for recording right away.

        Args:
            test_suite_configs: Test suite configurations.
            func: Whether to include functional test cases in the final list.
            perf: Whether to include performance test cases in the final list.

        Returns:
            The discovered test suites, each with test cases.
        """
        test_suites_with_cases = []

        for test_suite_config in test_suite_configs:
            test_suite_class = self._get_test_suite_class(test_suite_config.test_suite)
            test_cases: list[type[TestCase]] = []
            func_test_cases, perf_test_cases = test_suite_class.get_test_cases(
                test_suite_config.test_cases
            )
            if func:
                test_cases.extend(func_test_cases)
            if perf:
                test_cases.extend(perf_test_cases)

            test_suites_with_cases.append(
                TestSuiteWithCases(test_suite_class=test_suite_class, test_cases=test_cases)
            )

        return test_suites_with_cases

    def _get_test_suite_class(self, module_name: str) -> type[TestSuite]:
        """Find the :class:`TestSuite` class in `module_name`.

        The full module name is `module_name` prefixed with `self._test_suite_module_prefix`.
        The module name is a standard filename with words separated with underscores.
        Search the `module_name` for a :class:`TestSuite` class which starts
        with `self._test_suite_class_prefix`, continuing with CamelCase `module_name`.
        The first matching class is returned.

        The CamelCase convention applies to abbreviations, acronyms, initialisms and so on::

            OS -> Os
            TCP -> Tcp

        Args:
            module_name: The module name without prefix where to search for the test suite.

        Returns:
            The found test suite class.

        Raises:
            ConfigurationError: If the corresponding module is not found or
                a valid :class:`TestSuite` is not found in the module.
        """

        def is_test_suite(object) -> bool:
            """Check whether `object` is a :class:`TestSuite`.

            The `object` is a subclass of :class:`TestSuite`, but not :class:`TestSuite` itself.

            Args:
                object: The object to be checked.

            Returns:
                :data:`True` if `object` is a subclass of `TestSuite`.
            """
            try:
                if issubclass(object, TestSuite) and object is not TestSuite:
                    return True
            except TypeError:
                return False
            return False

        testsuite_module_path = f"{self._test_suite_module_prefix}{module_name}"
        try:
            test_suite_module = importlib.import_module(testsuite_module_path)
        except ModuleNotFoundError as e:
            raise ConfigurationError(
                f"Test suite module '{testsuite_module_path}' not found."
            ) from e

        camel_case_suite_name = "".join(
            [suite_word.capitalize() for suite_word in module_name.split("_")]
        )
        full_suite_name_to_find = f"{self._test_suite_class_prefix}{camel_case_suite_name}"
        for class_name, class_obj in inspect.getmembers(test_suite_module, is_test_suite):
            if class_name == full_suite_name_to_find:
                return class_obj
        raise ConfigurationError(
            f"Couldn't find any valid test suites in {test_suite_module.__name__}."
        )

    def _connect_nodes_and_run_test_run(
        self,
        sut_nodes: dict[str, SutNode],
        tg_nodes: dict[str, TGNode],
        test_run_config: TestRunConfiguration,
        test_run_result: TestRunResult,
        test_suites_with_cases: Iterable[TestSuiteWithCases],
    ) -> None:
        """Connect nodes, then continue to run the given test run.

        Connect the :class:`SutNode` and the :class:`TGNode` of this `test_run_config`.
        If either has already been connected, it's going to be in either `sut_nodes` or `tg_nodes`,
        respectively.
        If not, connect and add the node to the respective `sut_nodes` or `tg_nodes` :class:`dict`.

        Args:
            sut_nodes: A dictionary storing connected/to be connected SUT nodes.
            tg_nodes: A dictionary storing connected/to be connected TG nodes.
            test_run_config: A test run configuration.
            test_run_result: The test run's result.
            test_suites_with_cases: The test suites with test cases to run.
        """
        sut_node = sut_nodes.get(test_run_config.system_under_test_node.name)
        tg_node = tg_nodes.get(test_run_config.traffic_generator_node.name)

        try:
            if not sut_node:
                sut_node = SutNode(test_run_config.system_under_test_node)
                sut_nodes[sut_node.name] = sut_node
            if not tg_node:
                tg_node = TGNode(test_run_config.traffic_generator_node)
                tg_nodes[tg_node.name] = tg_node
        except Exception as e:
            failed_node = test_run_config.system_under_test_node.name
            if sut_node:
                failed_node = test_run_config.traffic_generator_node.name
            self._logger.exception(f"The Creation of node {failed_node} failed.")
            test_run_result.update_setup(Result.FAIL, e)

        else:
            self._run_test_run(
                sut_node, tg_node, test_run_config, test_run_result, test_suites_with_cases
            )

    def _run_test_run(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        test_run_config: TestRunConfiguration,
        test_run_result: TestRunResult,
        test_suites_with_cases: Iterable[TestSuiteWithCases],
    ) -> None:
        """Run the given test run.

        This involves running the test run setup as well as running all build targets
        in the given test run. After that, the test run teardown is run.

        Args:
            sut_node: The test run's SUT node.
            tg_node: The test run's TG node.
            test_run_config: A test run configuration.
            test_run_result: The test run's result.
            test_suites_with_cases: The test suites with test cases to run.
        """
        self._logger.info(
            f"Running test run with SUT '{test_run_config.system_under_test_node.name}'."
        )
        test_run_result.add_sut_info(sut_node.node_info)
        try:
            sut_node.set_up_test_run(test_run_config)
            tg_node.set_up_test_run(test_run_config)
            test_run_result.update_setup(Result.PASS)
        except Exception as e:
            self._logger.exception("Test run setup failed.")
            test_run_result.update_setup(Result.FAIL, e)

        else:
            for build_target_config in test_run_config.build_targets:
                build_target_result = test_run_result.add_build_target(build_target_config)
                self._run_build_target(
                    sut_node,
                    tg_node,
                    build_target_config,
                    build_target_result,
                    test_suites_with_cases,
                )

        finally:
            try:
                self._logger.set_stage(DtsStage.test_run_teardown)
                sut_node.tear_down_test_run()
                tg_node.tear_down_test_run()
                test_run_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception("Test run teardown failed.")
                test_run_result.update_teardown(Result.FAIL, e)

    def _run_build_target(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        build_target_config: BuildTargetConfiguration,
        build_target_result: BuildTargetResult,
        test_suites_with_cases: Iterable[TestSuiteWithCases],
    ) -> None:
        """Run the given build target.

        This involves running the build target setup as well as running all test suites
        of the build target's test run.
        After that, build target teardown is run.

        Args:
            sut_node: The test run's sut node.
            tg_node: The test run's tg node.
            build_target_config: A build target's test run configuration.
            build_target_result: The build target level result object associated
                with the current build target.
            test_suites_with_cases: The test suites with test cases to run.
        """
        self._logger.set_stage(DtsStage.build_target_setup)
        self._logger.info(f"Running build target '{build_target_config.name}'.")

        try:
            sut_node.set_up_build_target(build_target_config)
            self._result.dpdk_version = sut_node.dpdk_version
            build_target_result.add_build_target_info(sut_node.get_build_target_info())
            build_target_result.update_setup(Result.PASS)
        except Exception as e:
            self._logger.exception("Build target setup failed.")
            build_target_result.update_setup(Result.FAIL, e)

        else:
            self._run_test_suites(sut_node, tg_node, build_target_result, test_suites_with_cases)

        finally:
            try:
                self._logger.set_stage(DtsStage.build_target_teardown)
                sut_node.tear_down_build_target()
                build_target_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception("Build target teardown failed.")
                build_target_result.update_teardown(Result.FAIL, e)

    def _get_supported_capabilities(
        self,
        sut_node: SutNode,
        topology_config: Topology,
        test_suites_with_cases: Iterable[TestSuiteWithCases],
    ) -> set[Capability]:

        capabilities_to_check = set()
        for test_suite_with_cases in test_suites_with_cases:
            capabilities_to_check.update(test_suite_with_cases.required_capabilities)

        self._logger.debug(f"Found capabilities to check: {capabilities_to_check}")

        return get_supported_capabilities(sut_node, topology_config, capabilities_to_check)

    def _run_test_suites(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        build_target_result: BuildTargetResult,
        test_suites_with_cases: Iterable[TestSuiteWithCases],
    ) -> None:
        """Run `test_suites_with_cases` with the current build target.

        The method assumes the build target we're testing has already been built on the SUT node.
        The current build target thus corresponds to the current DPDK build present on the SUT node.

        Before running any suites, the method determines whether they should be skipped
        by inspecting any required capabilities the test suite needs and comparing those
        to capabilities supported by the tested environment. If all capabilities are supported,
        the suite is run. If all test cases in a test suite would be skipped, the whole test suite
        is skipped (the setup and teardown is not run).

        If a blocking test suite (such as the smoke test suite) fails, the rest of the test suites
        in the current build target won't be executed.

        Args:
            sut_node: The test run's SUT node.
            tg_node: The test run's TG node.
            build_target_result: The build target level result object associated
                with the current build target.
            test_suites_with_cases: The test suites with test cases to run.
        """
        end_build_target = False
        topology = Topology(sut_node.ports, tg_node.ports)
        supported_capabilities = self._get_supported_capabilities(
            sut_node, topology, test_suites_with_cases
        )
        for test_suite_with_cases in test_suites_with_cases:
            test_suite_with_cases.mark_skip_unsupported(supported_capabilities)
            test_suite_result = build_target_result.add_test_suite(test_suite_with_cases)
            try:
                if not test_suite_with_cases.skip:
                    self._run_test_suite(
                        sut_node,
                        tg_node,
                        topology,
                        test_suite_result,
                        test_suite_with_cases,
                    )
                else:
                    self._logger.info(
                        f"Test suite execution SKIPPED: "
                        f"'{test_suite_with_cases.test_suite_class.__name__}'. Reason: "
                        f"{test_suite_with_cases.test_suite_class.skip_reason}"
                    )
                    test_suite_result.update_setup(Result.SKIP)
            except BlockingTestSuiteError as e:
                self._logger.exception(
                    f"An error occurred within {test_suite_with_cases.test_suite_class.__name__}. "
                    "Skipping build target..."
                )
                self._result.add_error(e)
                end_build_target = True
            # if a blocking test failed and we need to bail out of suite executions
            if end_build_target:
                break

    def _run_test_suite(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        topology: Topology,
        test_suite_result: TestSuiteResult,
        test_suite_with_cases: TestSuiteWithCases,
    ) -> None:
        """Set up, execute and tear down `test_suite_with_cases`.

        The method assumes the build target we're testing has already been built on the SUT node.
        The current build target thus corresponds to the current DPDK build present on the SUT node.

        Test suite execution consists of running the discovered test cases.
        A test case run consists of setup, execution and teardown of said test case.

        Record the setup and the teardown and handle failures.

        Args:
            sut_node: The test run's SUT node.
            tg_node: The test run's TG node.
            test_suite_result: The test suite level result object associated
                with the current test suite.
            test_suite_with_cases: The test suite with test cases to run.

        Raises:
            BlockingTestSuiteError: If a blocking test suite fails.
        """
        test_suite_name = test_suite_with_cases.test_suite_class.__name__
        self._logger.set_stage(
            DtsStage.test_suite_setup, Path(SETTINGS.output_dir, test_suite_name)
        )
        test_suite = test_suite_with_cases.test_suite_class(sut_node, tg_node, topology)
        try:
            self._logger.info(f"Starting test suite setup: {test_suite_name}")
            test_suite.set_up_suite()
            test_suite_result.update_setup(Result.PASS)
            self._logger.info(f"Test suite setup successful: {test_suite_name}")
        except Exception as e:
            self._logger.exception(f"Test suite setup ERROR: {test_suite_name}")
            test_suite_result.update_setup(Result.ERROR, e)

        else:
            self._execute_test_suite(
                test_suite,
                test_suite_with_cases.test_cases,
                test_suite_result,
            )
        finally:
            try:
                self._logger.set_stage(DtsStage.test_suite_teardown)
                test_suite.tear_down_suite()
                sut_node.kill_cleanup_dpdk_apps()
                test_suite_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception(f"Test suite teardown ERROR: {test_suite_name}")
                self._logger.warning(
                    f"Test suite '{test_suite_name}' teardown failed, "
                    "the next test suite may be affected."
                )
                test_suite_result.update_setup(Result.ERROR, e)
            if len(test_suite_result.get_errors()) > 0 and test_suite.is_blocking:
                raise BlockingTestSuiteError(test_suite_name)

    def _execute_test_suite(
        self,
        test_suite: TestSuite,
        test_cases: Iterable[type[TestCase]],
        test_suite_result: TestSuiteResult,
    ) -> None:
        """Execute all `test_cases` in `test_suite`.

        If the :option:`--re-run` command line argument or the :envvar:`DTS_RERUN` environment
        variable is set, in case of a test case failure, the test case will be executed again
        until it passes or it fails that many times in addition of the first failure.

        Args:
            test_suite: The test suite object.
            test_cases: The list of test case functions.
            test_suite_result: The test suite level result object associated
                with the current test suite.
        """
        self._logger.set_stage(DtsStage.test_suite)
        for test_case in test_cases:
            test_case_name = test_case.__name__
            test_case_result = test_suite_result.add_test_case(test_case_name)
            all_attempts = SETTINGS.re_run + 1
            attempt_nr = 1
            if not test_case.skip:
                self._run_test_case(test_suite, test_case, test_case_result)
                while not test_case_result and attempt_nr < all_attempts:
                    attempt_nr += 1
                    self._logger.info(
                        f"Re-running FAILED test case '{test_case_name}'. "
                        f"Attempt number {attempt_nr} out of {all_attempts}."
                    )
                    self._run_test_case(test_suite, test_case, test_case_result)
            else:
                self._logger.info(
                    f"Test case execution SKIPPED: {test_case_name}. Reason: "
                    f"{test_case.skip_reason}"
                )
                test_case_result.update_setup(Result.SKIP)

    def _run_test_case(
        self,
        test_suite: TestSuite,
        test_case: type[TestCase],
        test_case_result: TestCaseResult,
    ) -> None:
        """Setup, execute and teardown `test_case_method` from `test_suite`.

        Record the result of the setup and the teardown and handle failures.

        Args:
            test_suite: The test suite object.
            test_case: The test case function.
            test_case_result: The test case level result object associated
                with the current test case.
        """
        test_case_name = test_case.__name__

        try:
            # run set_up function for each case
            test_suite.set_up_test_case()
            test_case_result.update_setup(Result.PASS)
        except SSHTimeoutError as e:
            self._logger.exception(f"Test case setup FAILED: {test_case_name}")
            test_case_result.update_setup(Result.FAIL, e)
        except Exception as e:
            self._logger.exception(f"Test case setup ERROR: {test_case_name}")
            test_case_result.update_setup(Result.ERROR, e)

        else:
            # run test case if setup was successful
            self._execute_test_case(test_suite, test_case, test_case_result)

        finally:
            try:
                test_suite.tear_down_test_case()
                test_case_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception(f"Test case teardown ERROR: {test_case_name}")
                self._logger.warning(
                    f"Test case '{test_case_name}' teardown failed, "
                    f"the next test case may be affected."
                )
                test_case_result.update_teardown(Result.ERROR, e)
                test_case_result.update(Result.ERROR)

    def _execute_test_case(
        self,
        test_suite: TestSuite,
        test_case: type[TestCase],
        test_case_result: TestCaseResult,
    ) -> None:
        """Execute `test_case_method` from `test_suite`, record the result and handle failures.

        Args:
            test_suite: The test suite object.
            test_case: The test case function.
            test_case_result: The test case level result object associated
                with the current test case.
        """
        test_case_name = test_case.__name__
        try:
            self._logger.info(f"Starting test case execution: {test_case_name}")
            # Explicit method binding is required, otherwise mypy complains
            MethodType(test_case, test_suite)()
            test_case_result.update(Result.PASS)
            self._logger.info(f"Test case execution PASSED: {test_case_name}")

        except TestCaseVerifyError as e:
            self._logger.exception(f"Test case execution FAILED: {test_case_name}")
            test_case_result.update(Result.FAIL, e)
        except Exception as e:
            self._logger.exception(f"Test case execution ERROR: {test_case_name}")
            test_case_result.update(Result.ERROR, e)
        except KeyboardInterrupt:
            self._logger.error(f"Test case execution INTERRUPTED by user: {test_case_name}")
            test_case_result.update(Result.SKIP)
            raise KeyboardInterrupt("Stop DTS")

    def _exit_dts(self) -> None:
        """Process all errors and exit with the proper exit code."""
        self._result.process()

        if self._logger:
            self._logger.info("DTS execution has ended.")

        sys.exit(self._result.get_return_code())

    def _init_random_seed(self, conf: TestRunConfiguration) -> None:
        """Initialize the random seed to use for the test run."""
        seed = SETTINGS.random_seed or conf.random_seed or random.randrange(0xFFFF_FFFF)
        self._logger.info(f"Initializing test run with random seed {seed}.")
        random.seed(seed)
