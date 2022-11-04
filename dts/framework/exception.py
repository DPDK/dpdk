# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

"""
User-defined exceptions used across the framework.
"""


class SSHTimeoutError(Exception):
    """
    Command execution timeout.
    """

    command: str
    output: str

    def __init__(self, command: str, output: str):
        self.command = command
        self.output = output

    def __str__(self) -> str:
        return f"TIMEOUT on {self.command}"

    def get_output(self) -> str:
        return self.output


class SSHConnectionError(Exception):
    """
    SSH connection error.
    """

    host: str

    def __init__(self, host: str):
        self.host = host

    def __str__(self) -> str:
        return f"Error trying to connect with {self.host}"


class SSHSessionDeadError(Exception):
    """
    SSH session is not alive.
    It can no longer be used.
    """

    host: str

    def __init__(self, host: str):
        self.host = host

    def __str__(self) -> str:
        return f"SSH session with {self.host} has died"
