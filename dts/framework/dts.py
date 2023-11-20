# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import sys

from .config import (
    CONFIGURATION,
    BuildTargetConfiguration,
    ExecutionConfiguration,
    TestSuiteConfig,
)
from .exception import BlockingTestSuiteError
from .logger import DTSLOG, getLogger
from .test_result import BuildTargetResult, DTSResult, ExecutionResult, Result
from .test_suite import get_test_suites
from .testbed_model import SutNode, TGNode
from .utils import check_dts_python_version

dts_logger: DTSLOG = getLogger("DTSRunner")
result: DTSResult = DTSResult(dts_logger)


def run_all() -> None:
    """
    The main process of DTS. Runs all build targets in all executions from the main
    config file.
    """
    global dts_logger
    global result

    # check the python version of the server that run dts
    check_dts_python_version()

    sut_nodes: dict[str, SutNode] = {}
    tg_nodes: dict[str, TGNode] = {}
    try:
        # for all Execution sections
        for execution in CONFIGURATION.executions:
            sut_node = sut_nodes.get(execution.system_under_test_node.name)
            tg_node = tg_nodes.get(execution.traffic_generator_node.name)

            try:
                if not sut_node:
                    sut_node = SutNode(execution.system_under_test_node)
                    sut_nodes[sut_node.name] = sut_node
                if not tg_node:
                    tg_node = TGNode(execution.traffic_generator_node)
                    tg_nodes[tg_node.name] = tg_node
                result.update_setup(Result.PASS)
            except Exception as e:
                failed_node = execution.system_under_test_node.name
                if sut_node:
                    failed_node = execution.traffic_generator_node.name
                dts_logger.exception(f"Creation of node {failed_node} failed.")
                result.update_setup(Result.FAIL, e)

            else:
                _run_execution(sut_node, tg_node, execution, result)

    except Exception as e:
        dts_logger.exception("An unexpected error has occurred.")
        result.add_error(e)
        raise

    finally:
        try:
            for node in (sut_nodes | tg_nodes).values():
                node.close()
            result.update_teardown(Result.PASS)
        except Exception as e:
            dts_logger.exception("Final cleanup of nodes failed.")
            result.update_teardown(Result.ERROR, e)

    # we need to put the sys.exit call outside the finally clause to make sure
    # that unexpected exceptions will propagate
    # in that case, the error that should be reported is the uncaught exception as
    # that is a severe error originating from the framework
    # at that point, we'll only have partial results which could be impacted by the
    # error causing the uncaught exception, making them uninterpretable
    _exit_dts()


def _run_execution(
    sut_node: SutNode,
    tg_node: TGNode,
    execution: ExecutionConfiguration,
    result: DTSResult,
) -> None:
    """
    Run the given execution. This involves running the execution setup as well as
    running all build targets in the given execution.
    """
    dts_logger.info(f"Running execution with SUT '{execution.system_under_test_node.name}'.")
    execution_result = result.add_execution(sut_node.config)
    execution_result.add_sut_info(sut_node.node_info)

    try:
        sut_node.set_up_execution(execution)
        execution_result.update_setup(Result.PASS)
    except Exception as e:
        dts_logger.exception("Execution setup failed.")
        execution_result.update_setup(Result.FAIL, e)

    else:
        for build_target in execution.build_targets:
            _run_build_target(sut_node, tg_node, build_target, execution, execution_result)

    finally:
        try:
            sut_node.tear_down_execution()
            execution_result.update_teardown(Result.PASS)
        except Exception as e:
            dts_logger.exception("Execution teardown failed.")
            execution_result.update_teardown(Result.FAIL, e)


def _run_build_target(
    sut_node: SutNode,
    tg_node: TGNode,
    build_target: BuildTargetConfiguration,
    execution: ExecutionConfiguration,
    execution_result: ExecutionResult,
) -> None:
    """
    Run the given build target.
    """
    dts_logger.info(f"Running build target '{build_target.name}'.")
    build_target_result = execution_result.add_build_target(build_target)

    try:
        sut_node.set_up_build_target(build_target)
        result.dpdk_version = sut_node.dpdk_version
        build_target_result.add_build_target_info(sut_node.get_build_target_info())
        build_target_result.update_setup(Result.PASS)
    except Exception as e:
        dts_logger.exception("Build target setup failed.")
        build_target_result.update_setup(Result.FAIL, e)

    else:
        _run_all_suites(sut_node, tg_node, execution, build_target_result)

    finally:
        try:
            sut_node.tear_down_build_target()
            build_target_result.update_teardown(Result.PASS)
        except Exception as e:
            dts_logger.exception("Build target teardown failed.")
            build_target_result.update_teardown(Result.FAIL, e)


def _run_all_suites(
    sut_node: SutNode,
    tg_node: TGNode,
    execution: ExecutionConfiguration,
    build_target_result: BuildTargetResult,
) -> None:
    """
    Use the given build_target to run execution's test suites
    with possibly only a subset of test cases.
    If no subset is specified, run all test cases.
    """
    end_build_target = False
    if not execution.skip_smoke_tests:
        execution.test_suites[:0] = [TestSuiteConfig.from_dict("smoke_tests")]
    for test_suite_config in execution.test_suites:
        try:
            _run_single_suite(sut_node, tg_node, execution, build_target_result, test_suite_config)
        except BlockingTestSuiteError as e:
            dts_logger.exception(
                f"An error occurred within {test_suite_config.test_suite}. Skipping build target."
            )
            result.add_error(e)
            end_build_target = True
        # if a blocking test failed and we need to bail out of suite executions
        if end_build_target:
            break


def _run_single_suite(
    sut_node: SutNode,
    tg_node: TGNode,
    execution: ExecutionConfiguration,
    build_target_result: BuildTargetResult,
    test_suite_config: TestSuiteConfig,
) -> None:
    """Runs a single test suite.

    Args:
        sut_node: Node to run tests on.
        execution: Execution the test case belongs to.
        build_target_result: Build target configuration test case is run on
        test_suite_config: Test suite configuration

    Raises:
        BlockingTestSuiteError: If a test suite that was marked as blocking fails.
    """
    try:
        full_suite_path = f"tests.TestSuite_{test_suite_config.test_suite}"
        test_suite_classes = get_test_suites(full_suite_path)
        suites_str = ", ".join((x.__name__ for x in test_suite_classes))
        dts_logger.debug(f"Found test suites '{suites_str}' in '{full_suite_path}'.")
    except Exception as e:
        dts_logger.exception("An error occurred when searching for test suites.")
        result.update_setup(Result.ERROR, e)

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


def _exit_dts() -> None:
    """
    Process all errors and exit with the proper exit code.
    """
    result.process()

    if dts_logger:
        dts_logger.info("DTS execution has ended.")
    sys.exit(result.get_return_code())
