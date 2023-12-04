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

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.SSH_ERR
    _command: str

    def __init__(self, command: str):
        self._command = command

    def __str__(self) -> str:
        return f"TIMEOUT on {self._command}"


class SSHConnectionError(DTSError):
    """
    SSH connection error.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.SSH_ERR
    _host: str
    _errors: list[str]

    def __init__(self, host: str, errors: list[str] | None = None):
        self._host = host
        self._errors = [] if errors is None else errors

    def __str__(self) -> str:
        message = f"Error trying to connect with {self._host}."
        if self._errors:
            message += f" Errors encountered while retrying: {', '.join(self._errors)}"

        return message


class SSHSessionDeadError(DTSError):
    """
    SSH session is not alive.
    It can no longer be used.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.SSH_ERR
    _host: str

    def __init__(self, host: str):
        self._host = host

    def __str__(self) -> str:
        return f"SSH session with {self._host} has died"


class ConfigurationError(DTSError):
    """
    Raised when an invalid configuration is encountered.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.CONFIG_ERR


class RemoteCommandExecutionError(DTSError):
    """
    Raised when a command executed on a Node returns a non-zero exit status.
    """

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.REMOTE_CMD_EXEC_ERR
    command: str
    _command_return_code: int

    def __init__(self, command: str, command_return_code: int):
        self.command = command
        self._command_return_code = command_return_code

    def __str__(self) -> str:
        return f"Command {self.command} returned a non-zero exit code: {self._command_return_code}"


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

    severity: ClassVar[ErrorSeverity] = ErrorSeverity.TESTCASE_VERIFY_ERR


class BlockingTestSuiteError(DTSError):
    severity: ClassVar[ErrorSeverity] = ErrorSeverity.BLOCKING_TESTSUITE_ERR
    _suite_name: str

    def __init__(self, suite_name: str) -> None:
        self._suite_name = suite_name

    def __str__(self) -> str:
        return f"Blocking suite {self._suite_name} failed."
