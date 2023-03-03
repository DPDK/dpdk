# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import sys

from .config import CONFIGURATION, BuildTargetConfiguration, ExecutionConfiguration
from .exception import DTSError, ErrorSeverity
from .logger import DTSLOG, getLogger
from .test_suite import get_test_suites
from .testbed_model import SutNode
from .utils import check_dts_python_version

dts_logger: DTSLOG = getLogger("DTSRunner")
errors = []


def run_all() -> None:
    """
    The main process of DTS. Runs all build targets in all executions from the main
    config file.
    """
    global dts_logger
    global errors

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
                except Exception as e:
                    dts_logger.exception(
                        f"Connection to node {execution.system_under_test} failed."
                    )
                    errors.append(e)
                else:
                    nodes[sut_node.name] = sut_node

            if sut_node:
                _run_execution(sut_node, execution)

    except Exception as e:
        dts_logger.exception("An unexpected error has occurred.")
        errors.append(e)
        raise

    finally:
        try:
            for node in nodes.values():
                node.close()
        except Exception as e:
            dts_logger.exception("Final cleanup of nodes failed.")
            errors.append(e)

    # we need to put the sys.exit call outside the finally clause to make sure
    # that unexpected exceptions will propagate
    # in that case, the error that should be reported is the uncaught exception as
    # that is a severe error originating from the framework
    # at that point, we'll only have partial results which could be impacted by the
    # error causing the uncaught exception, making them uninterpretable
    _exit_dts()


def _run_execution(sut_node: SutNode, execution: ExecutionConfiguration) -> None:
    """
    Run the given execution. This involves running the execution setup as well as
    running all build targets in the given execution.
    """
    dts_logger.info(f"Running execution with SUT '{execution.system_under_test.name}'.")

    try:
        sut_node.set_up_execution(execution)
    except Exception as e:
        dts_logger.exception("Execution setup failed.")
        errors.append(e)

    else:
        for build_target in execution.build_targets:
            _run_build_target(sut_node, build_target, execution)

    finally:
        try:
            sut_node.tear_down_execution()
        except Exception as e:
            dts_logger.exception("Execution teardown failed.")
            errors.append(e)


def _run_build_target(
    sut_node: SutNode,
    build_target: BuildTargetConfiguration,
    execution: ExecutionConfiguration,
) -> None:
    """
    Run the given build target.
    """
    dts_logger.info(f"Running build target '{build_target.name}'.")

    try:
        sut_node.set_up_build_target(build_target)
    except Exception as e:
        dts_logger.exception("Build target setup failed.")
        errors.append(e)

    else:
        _run_suites(sut_node, execution)

    finally:
        try:
            sut_node.tear_down_build_target()
        except Exception as e:
            dts_logger.exception("Build target teardown failed.")
            errors.append(e)


def _run_suites(
    sut_node: SutNode,
    execution: ExecutionConfiguration,
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
            errors.append(e)

        else:
            for test_suite_class in test_suite_classes:
                test_suite = test_suite_class(
                    sut_node, test_suite_config.test_cases, execution.func, errors
                )
                test_suite.run()


def _exit_dts() -> None:
    """
    Process all errors and exit with the proper exit code.
    """
    if errors and dts_logger:
        dts_logger.debug("Summary of errors:")
        for error in errors:
            dts_logger.debug(repr(error))

    return_code = ErrorSeverity.NO_ERR
    for error in errors:
        error_return_code = ErrorSeverity.GENERIC_ERR
        if isinstance(error, DTSError):
            error_return_code = error.severity

        if error_return_code > return_code:
            return_code = error_return_code

    if dts_logger:
        dts_logger.info("DTS execution has ended.")
    sys.exit(return_code)
