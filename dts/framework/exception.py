# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""
User-defined exceptions used across the framework.
"""

from enum import IntEnum, unique
from typing import ClassVar


@unique
class ErrorSeverity(IntEnum):
    """
    The severity of errors that occur during DTS execution.
    All exceptions are caught and the most severe error is used as return code.
    """

    NO_ERR = 0
    GENERIC_ERR = 1
    CONFIG_ERR = 2
    REMOTE_CMD_EXEC_ERR = 3
    SSH_ERR = 4
    DPDK_BUILD_ERR = 10
    TESTCASE_VERIFY_ERR = 20
    BLOCKING_TESTSUITE_ERR = 25


class DTSError(Exception):
    """
    The base exception from which all DTS exceptions are derived.
    Stores error severity.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.GENERIC_ERR


class SSHTimeoutError(DTSError):
    """
    Command execution timeout.
    """

    command: str
    output: str
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.SSH_ERR

    def __init__(self, command: str, output: str):
        self.command = command
        self.output = output

    def __str__(self) -> str:
        return f"TIMEOUT on {self.command}"

    def get_output(self) -> str:
        return self.output


class SSHConnectionError(DTSError):
    """
    SSH connection error.
    """

    host: str
    errors: list[str]
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.SSH_ERR

    def __init__(self, host: str, errors: list[str] | None = None):
        self.host = host
        self.errors = [] if errors is None else errors

    def __str__(self) -> str:
        message = f"Error trying to connect with {self.host}."
        if self.errors:
            message += f" Errors encountered while retrying: {', '.join(self.errors)}"

        return message


class SSHSessionDeadError(DTSError):
    """
    SSH session is not alive.
    It can no longer be used.
    """

    host: str
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.SSH_ERR

    def __init__(self, host: str):
        self.host = host

    def __str__(self) -> str:
        return f"SSH session with {self.host} has died"


class ConfigurationError(DTSError):
    """
    Raised when an invalid configuration is encountered.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.CONFIG_ERR


class RemoteCommandExecutionError(DTSError):
    """
    Raised when a command executed on a Node returns a non-zero exit status.
    """

    command: str
    command_return_code: int
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.REMOTE_CMD_EXEC_ERR

    def __init__(self, command: str, command_return_code: int):
        self.command = command
        self.command_return_code = command_return_code

    def __str__(self) -> str:
        return f"Command {self.command} returned a non-zero exit code: {self.command_return_code}"


class RemoteDirectoryExistsError(DTSError):
    """
    Raised when a remote directory to be created already exists.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.REMOTE_CMD_EXEC_ERR


class DPDKBuildError(DTSError):
    """
    Raised when DPDK build fails for any reason.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.DPDK_BUILD_ERR


class TestCaseVerifyError(DTSError):
    """
    Used in test cases to verify the expected behavior.
    """

    value: str
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.TESTCASE_VERIFY_ERR

    def __init__(self, value: str):
        self.value = value

    def __str__(self) -> str:
        return repr(self.value)


class BlockingTestSuiteError(DTSError):
    suite_name: str
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.BLOCKING_TESTSUITE_ERR

    def __init__(self, suite_name: str) -> None:
        self.suite_name = suite_name

    def __str__(self) -> str:
        return f"Blocking suite {self.suite_name} failed."
