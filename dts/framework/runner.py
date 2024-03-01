# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""Test suite runner module.

The module is responsible for running DTS in a series of stages:

    #. Execution stage,
    #. Build target stage,
    #. Test suite stage,
    #. Test case stage.

The execution and build target stages set up the environment before running test suites.
The test suite stage sets up steps common to all test cases
and the test case stage runs test cases individually.
"""

import logging
import sys

from .config import (
    BuildTargetConfiguration,
    ExecutionConfiguration,
    TestSuiteConfig,
    load_config,
)
from .exception import BlockingTestSuiteError
from .logger import DTSLOG, getLogger
from .test_result import BuildTargetResult, DTSResult, ExecutionResult, Result
from .test_suite import get_test_suites
from .testbed_model import SutNode, TGNode


class DTSRunner:
    r"""Test suite runner class.

    The class is responsible for running tests on testbeds defined in the test run configuration.
    Each setup or teardown of each stage is recorded in a :class:`~framework.test_result.DTSResult`
    or one of its subclasses. The test case results are also recorded.

    If an error occurs, the current stage is aborted, the error is recorded and the run continues in
    the next iteration of the same stage. The return code is the highest `severity` of all
    :class:`~.framework.exception.DTSError`\s.

    Example:
        An error occurs in a build target setup. The current build target is aborted and the run
        continues with the next build target. If the errored build target was the last one in the
        given execution, the next execution begins.
    """

    _logger: DTSLOG
    _result: DTSResult

    def __init__(self):
        """Initialize the instance with logger and result."""
        self._logger = getLogger("DTSRunner")
        self._result = DTSResult(self._logger)

    def run(self):
        """Run all build targets in all executions from the test run configuration.

        Before running test suites, executions and build targets are first set up.
        The executions and build targets defined in the test run configuration are iterated over.
        The executions define which tests to run and where to run them and build targets define
        the DPDK build setup.

        The tests suites are set up for each execution/build target tuple and each discovered
        test case within the test suite is set up, executed and torn down. After all test cases
        have been executed, the test suite is torn down and the next build target will be tested.

        All the nested steps look like this:

            #. Execution setup

                #. Build target setup

                    #. Test suite setup

                        #. Test case setup
                        #. Test case logic
                        #. Test case teardown

                    #. Test suite teardown

                #. Build target teardown

            #. Execution teardown

        The test cases are filtered according to the specification in the test run configuration and
        the :option:`--test-cases` command line argument or
        the :envvar:`DTS_TESTCASES` environment variable.
        """
        sut_nodes: dict[str, SutNode] = {}
        tg_nodes: dict[str, TGNode] = {}
        try:
            # check the python version of the server that runs dts
            self._check_dts_python_version()

            # for all Execution sections
            for execution in load_config().executions:
                sut_node = sut_nodes.get(execution.system_under_test_node.name)
                tg_node = tg_nodes.get(execution.traffic_generator_node.name)

                try:
                    if not sut_node:
                        sut_node = SutNode(execution.system_under_test_node)
                        sut_nodes[sut_node.name] = sut_node
                    if not tg_node:
                        tg_node = TGNode(execution.traffic_generator_node)
                        tg_nodes[tg_node.name] = tg_node
                    self._result.update_setup(Result.PASS)
                except Exception as e:
                    failed_node = execution.system_under_test_node.name
                    if sut_node:
                        failed_node = execution.traffic_generator_node.name
                    self._logger.exception(f"The Creation of node {failed_node} failed.")
                    self._result.update_setup(Result.FAIL, e)

                else:
                    self._run_execution(sut_node, tg_node, execution)

        except Exception as e:
            self._logger.exception("An unexpected error has occurred.")
            self._result.add_error(e)
            raise

        finally:
            try:
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

    def _run_execution(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        execution: ExecutionConfiguration,
    ) -> None:
        """Run the given execution.

        This involves running the execution setup as well as running all build targets
        in the given execution. After that, execution teardown is run.

        Args:
            sut_node: The execution's SUT node.
            tg_node: The execution's TG node.
            execution: An execution's test run configuration.
        """
        self._logger.info(f"Running execution with SUT '{execution.system_under_test_node.name}'.")
        execution_result = self._result.add_execution(sut_node.config)
        execution_result.add_sut_info(sut_node.node_info)

        try:
            sut_node.set_up_execution(execution)
            execution_result.update_setup(Result.PASS)
        except Exception as e:
            self._logger.exception("Execution setup failed.")
            execution_result.update_setup(Result.FAIL, e)

        else:
            for build_target in execution.build_targets:
                self._run_build_target(sut_node, tg_node, build_target, execution, execution_result)

        finally:
            try:
                sut_node.tear_down_execution()
                execution_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception("Execution teardown failed.")
                execution_result.update_teardown(Result.FAIL, e)

    def _run_build_target(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        build_target: BuildTargetConfiguration,
        execution: ExecutionConfiguration,
        execution_result: ExecutionResult,
    ) -> None:
        """Run the given build target.

        This involves running the build target setup as well as running all test suites
        of the build target's execution.
        After that, build target teardown is run.

        Args:
            sut_node: The execution's sut node.
            tg_node: The execution's tg node.
            build_target: A build target's test run configuration.
            execution: The build target's execution's test run configuration.
            execution_result: The execution level result object associated with the execution.
        """
        self._logger.info(f"Running build target '{build_target.name}'.")
        build_target_result = execution_result.add_build_target(build_target)

        try:
            sut_node.set_up_build_target(build_target)
            self._result.dpdk_version = sut_node.dpdk_version
            build_target_result.add_build_target_info(sut_node.get_build_target_info())
            build_target_result.update_setup(Result.PASS)
        except Exception as e:
            self._logger.exception("Build target setup failed.")
            build_target_result.update_setup(Result.FAIL, e)

        else:
            self._run_all_suites(sut_node, tg_node, execution, build_target_result)

        finally:
            try:
                sut_node.tear_down_build_target()
                build_target_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception("Build target teardown failed.")
                build_target_result.update_teardown(Result.FAIL, e)

    def _run_all_suites(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        execution: ExecutionConfiguration,
        build_target_result: BuildTargetResult,
    ) -> None:
        """Run the execution's (possibly a subset of) test suites using the current build target.

        The method assumes the build target we're testing has already been built on the SUT node.
        The current build target thus corresponds to the current DPDK build present on the SUT node.

        Args:
            sut_node: The execution's SUT node.
            tg_node: The execution's TG node.
            execution: The execution's test run configuration associated
                with the current build target.
            build_target_result: The build target level result object associated
                with the current build target.
        """
        end_build_target = False
        if not execution.skip_smoke_tests:
            execution.test_suites[:0] = [TestSuiteConfig.from_dict("smoke_tests")]
        for test_suite_config in execution.test_suites:
            try:
                self._run_single_suite(
                    sut_node, tg_node, execution, build_target_result, test_suite_config
                )
            except BlockingTestSuiteError as e:
                self._logger.exception(
                    f"An error occurred within {test_suite_config.test_suite}. "
                    "Skipping build target..."
                )
                self._result.add_error(e)
                end_build_target = True
            # if a blocking test failed and we need to bail out of suite executions
            if end_build_target:
                break

    def _run_single_suite(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
        execution: ExecutionConfiguration,
        build_target_result: BuildTargetResult,
        test_suite_config: TestSuiteConfig,
    ) -> None:
        """Run all test suites in a single test suite module.

        The method assumes the build target we're testing has already been built on the SUT node.
        The current build target thus corresponds to the current DPDK build present on the SUT node.

        Args:
            sut_node: The execution's SUT node.
            tg_node: The execution's TG node.
            execution: The execution's test run configuration associated
                with the current build target.
            build_target_result: The build target level result object associated
                with the current build target.
            test_suite_config: Test suite test run configuration specifying the test suite module
                and possibly a subset of test cases of test suites in that module.

        Raises:
            BlockingTestSuiteError: If a blocking test suite fails.
        """
        try:
            full_suite_path = f"tests.TestSuite_{test_suite_config.test_suite}"
            test_suite_classes = get_test_suites(full_suite_path)
            suites_str = ", ".join((x.__name__ for x in test_suite_classes))
            self._logger.debug(f"Found test suites '{suites_str}' in '{full_suite_path}'.")
        except Exception as e:
            self._logger.exception("An error occurred when searching for test suites.")
            self._result.update_setup(Result.ERROR, e)

        else:
            for test_suite_class in test_suite_classes:
                test_suite = test_suite_class(
                    sut_node,
                    tg_node,
                    test_suite_config.test_cases,
                    execution.func,
                    build_target_result,
                )
                test_suite.run()

    def _exit_dts(self) -> None:
        """Process all errors and exit with the proper exit code."""
        self._result.process()

        if self._logger:
            self._logger.info("DTS execution has ended.")

        logging.shutdown()
        sys.exit(self._result.get_return_code())
