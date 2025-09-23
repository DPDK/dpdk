# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Common test utilities.

This module provides utility functions for test cases, including logging, verification.
"""

from framework.context import get_ctx
from framework.exception import InternalError, SkippedTestException, TestCaseVerifyError
from framework.logger import DTSLogger


def get_current_test_case_name() -> str:
    """The name of the current test case.

    Raises:
        InternalError: If no test is running.
    """
    current_test_case = get_ctx().local.current_test_case
    if current_test_case is None:
        raise InternalError("No current test case")
    return current_test_case.name


def log(message: str) -> None:
    """Log the given message with the level 'INFO'.

    Args:
        message: String representing the message to log.

    Raises:
        InternalError: If no test is running.
    """
    get_logger().info(message)


def log_debug(message: str) -> None:
    """Log the given message with the level 'DEBUG'.

    Args:
        message: String representing the message to log.

    Raises:
        InternalError: If no test is running.
    """
    get_logger().debug(message)


def verify(condition: bool, failure_description: str) -> None:
    """Verify `condition` and handle failures.

    When `condition` is :data:`False`, raise an exception and log the last 10 commands
    executed on both the SUT and TG.

    Args:
        condition: The condition to check.
        failure_description: A short description of the failure
            that will be stored in the raised exception.

    Raises:
        TestCaseVerifyError: If `condition` is :data:`False`.
    """
    if not condition:
        fail(failure_description)


def verify_else_skip(condition: bool, skip_reason: str) -> None:
    """Verify `condition` and handle skips.

    When `condition` is :data:`False`, raise a skip exception.

    Args:
        condition: The condition to check.
        skip_reason: Description of the reason for skipping.

    Raises:
        SkippedTestException: If `condition` is :data:`False`.
    """
    if not condition:
        skip(skip_reason)


def skip(skip_description: str) -> None:
    """Skip the current test case or test suite with a given description.

    Args:
        skip_description: Description of the reason for skipping.

    Raises:
        SkippedTestException: Always raised to indicate the test was skipped.
    """
    get_logger().debug(f"Test skipped: {skip_description}")
    raise SkippedTestException(skip_description)


def fail(failure_description: str) -> None:
    """Fail the current test case with a given description.

    Logs the last 10 commands executed on both the SUT and TG before raising an exception.

    Args:
        failure_description: Description of the reason for failure.

    Raises:
        TestCaseVerifyError: Always raised to indicate the test case failed.
    """
    get_logger().debug("A test case failed, showing the last 10 commands executed on SUT:")
    for command_res in get_ctx().sut_node.main_session.remote_session.history[-10:]:
        get_logger().debug(command_res.command)
    get_logger().debug("A test case failed, showing the last 10 commands executed on TG:")
    for command_res in get_ctx().tg_node.main_session.remote_session.history[-10:]:
        get_logger().debug(command_res.command)
    raise TestCaseVerifyError(failure_description)


def get_logger() -> DTSLogger:
    """Get a logger instance for tests.

    Raises:
        InternalError: If no test is running.
    """
    current_test_suite = get_ctx().local.current_test_suite
    if current_test_suite is None:
        raise InternalError("No current test suite")
    return current_test_suite._logger
