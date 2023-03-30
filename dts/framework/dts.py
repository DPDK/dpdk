# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import sys

from .config import CONFIGURATION, BuildTargetConfiguration, ExecutionConfiguration
from .logger import DTSLOG, getLogger
from .test_result import BuildTargetResult, DTSResult, ExecutionResult, Result
from .test_suite import get_test_suites
from .testbed_model import SutNode
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

    nodes: dict[str, SutNode] = {}
    try:
        # for all Execution sections
        for execution in CONFIGURATION.executions:
            sut_node = None
            if execution.system_under_test.name in nodes:
                # a Node with the same name already exists
                sut_node = nodes[execution.system_under_test.name]
            else:
                # the SUT has not been initialized yet
                try:
                    sut_node = SutNode(execution.system_under_test)
                    result.update_setup(Result.PASS)
                except Exception as e:
                    dts_logger.exception(
                        f"Connection to node {execution.system_under_test} failed."
                    )
                    result.update_setup(Result.FAIL, e)
                else:
                    nodes[sut_node.name] = sut_node

            if sut_node:
                _run_execution(sut_node, execution, result)

    except Exception as e:
        dts_logger.exception("An unexpected error has occurred.")
        result.add_error(e)
        raise

    finally:
        try:
            for node in nodes.values():
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
    sut_node: SutNode, execution: ExecutionConfiguration, result: DTSResult
) -> None:
    """
    Run the given execution. This involves running the execution setup as well as
    running all build targets in the given execution.
    """
    dts_logger.info(f"Running execution with SUT '{execution.system_under_test.name}'.")
    execution_result = result.add_execution(sut_node.config)

    try:
        sut_node.set_up_execution(execution)
        execution_result.update_setup(Result.PASS)
    except Exception as e:
        dts_logger.exception("Execution setup failed.")
        execution_result.update_setup(Result.FAIL, e)

    else:
        for build_target in execution.build_targets:
            _run_build_target(sut_node, build_target, execution, execution_result)

    finally:
        try:
            sut_node.tear_down_execution()
            execution_result.update_teardown(Result.PASS)
        except Exception as e:
            dts_logger.exception("Execution teardown failed.")
            execution_result.update_teardown(Result.FAIL, e)


def _run_build_target(
    sut_node: SutNode,
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
        build_target_result.update_setup(Result.PASS)
    except Exception as e:
        dts_logger.exception("Build target setup failed.")
        build_target_result.update_setup(Result.FAIL, e)

    else:
        _run_suites(sut_node, execution, build_target_result)

    finally:
        try:
            sut_node.tear_down_build_target()
            build_target_result.update_teardown(Result.PASS)
        except Exception as e:
            dts_logger.exception("Build target teardown failed.")
            build_target_result.update_teardown(Result.FAIL, e)


def _run_suites(
    sut_node: SutNode,
    execution: ExecutionConfiguration,
    build_target_result: BuildTargetResult,
) -> None:
    """
    Use the given build_target to run execution's test suites
    with possibly only a subset of test cases.
    If no subset is specified, run all test cases.
    """
    for test_suite_config in execution.test_suites:
        try:
            full_suite_path = f"tests.TestSuite_{test_suite_config.test_suite}"
            test_suite_classes = get_test_suites(full_suite_path)
            suites_str = ", ".join((x.__name__ for x in test_suite_classes))
            dts_logger.debug(
                f"Found test suites '{suites_str}' in '{full_suite_path}'."
            )
        except Exception as e:
            dts_logger.exception("An error occurred when searching for test suites.")
            result.update_setup(Result.ERROR, e)

        else:
            for test_suite_class in test_suite_classes:
                test_suite = test_suite_class(
                    sut_node,
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
