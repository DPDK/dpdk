# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

import time
from pathlib import PurePath

import pexpect  # type: ignore
from pexpect import pxssh  # type: ignore

from framework.config import NodeConfiguration
from framework.exception import SSHConnectionError, SSHSessionDeadError, SSHTimeoutError
from framework.logger import DTSLOG
from framework.utils import GREEN, RED, EnvVarsDict

from .remote_session import CommandResult, RemoteSession


class SSHSession(RemoteSession):
    """
    Module for creating Pexpect SSH remote sessions.
    """

    session: pxssh.pxssh
    magic_prompt: str

    def __init__(
        self,
        node_config: NodeConfiguration,
        session_name: str,
        logger: DTSLOG,
    ):
        self.magic_prompt = "MAGIC PROMPT"
        super(SSHSession, self).__init__(node_config, session_name, logger)

    def _connect(self) -> None:
        """
        Create connection to assigned node.
        """
        retry_attempts = 10
        login_timeout = 20 if self.port else 10
        password_regex = (
            r"(?i)(?:password:)|(?:passphrase for key)|(?i)(password for .+:)"
        )
        try:
            for retry_attempt in range(retry_attempts):
                self.session = pxssh.pxssh(encoding="utf-8")
                try:
                    self.session.login(
                        self.ip,
                        self.username,
                        self.password,
                        original_prompt="[$#>]",
                        port=self.port,
                        login_timeout=login_timeout,
                        password_regex=password_regex,
                    )
                    break
                except Exception as e:
                    self._logger.warning(e)
                    time.sleep(2)
                    self._logger.info(
                        f"Retrying connection: retry number {retry_attempt + 1}."
                    )
            else:
                raise Exception(f"Connection to {self.hostname} failed")

            self.send_expect("stty -echo", "#")
            self.send_expect("stty columns 1000", "#")
            self.send_expect("bind 'set enable-bracketed-paste off'", "#")
        except Exception as e:
            self._logger.error(RED(str(e)))
            if getattr(self, "port", None):
                suggestion = (
                    f"\nSuggestion: Check if the firewall on {self.hostname} is "
                    f"stopped.\n"
                )
                self._logger.info(GREEN(suggestion))

            raise SSHConnectionError(self.hostname)

    def send_expect(
        self, command: str, prompt: str, timeout: float = 15, verify: bool = False
    ) -> str | int:
        try:
            ret = self.send_expect_base(command, prompt, timeout)
            if verify:
                ret_status = self.send_expect_base("echo $?", prompt, timeout)
                try:
                    retval = int(ret_status)
                    if retval:
                        self._logger.error(f"Command: {command} failure!")
                        self._logger.error(ret)
                        return retval
                    else:
                        return ret
                except ValueError:
                    return ret
            else:
                return ret
        except Exception as e:
            self._logger.error(
                f"Exception happened in [{command}] and output is "
                f"[{self._get_output()}]"
            )
            raise e

    def send_expect_base(self, command: str, prompt: str, timeout: float) -> str:
        self._clean_session()
        original_prompt = self.session.PROMPT
        self.session.PROMPT = prompt
        self._send_line(command)
        self._prompt(command, timeout)

        before = self._get_output()
        self.session.PROMPT = original_prompt
        return before

    def _clean_session(self) -> None:
        self.session.PROMPT = self.magic_prompt
        self.get_output(timeout=0.01)
        self.session.PROMPT = self.session.UNIQUE_PROMPT

    def _send_line(self, command: str) -> None:
        if not self.is_alive():
            raise SSHSessionDeadError(self.hostname)
        if len(command) == 2 and command.startswith("^"):
            self.session.sendcontrol(command[1])
        else:
            self.session.sendline(command)

    def _prompt(self, command: str, timeout: float) -> None:
        if not self.session.prompt(timeout):
            raise SSHTimeoutError(command, self._get_output()) from None

    def get_output(self, timeout: float = 15) -> str:
        """
        Get all output before timeout
        """
        try:
            self.session.prompt(timeout)
        except Exception:
            pass

        before = self._get_output()
        self._flush()

        return before

    def _get_output(self) -> str:
        if not self.is_alive():
            raise SSHSessionDeadError(self.hostname)
        before = self.session.before.rsplit("\r\n", 1)[0]
        if before == "[PEXPECT]":
            return ""
        return before

    def _flush(self) -> None:
        """
        Clear all session buffer
        """
        self.session.buffer = ""
        self.session.before = ""

    def is_alive(self) -> bool:
        return self.session.isalive()

    def _send_command(
        self, command: str, timeout: float, env: EnvVarsDict | None
    ) -> CommandResult:
        output = self._send_command_get_output(command, timeout, env)
        return_code = int(self._send_command_get_output("echo $?", timeout, None))

        # we're capturing only stdout
        return CommandResult(self.name, command, output, "", return_code)

    def _send_command_get_output(
        self, command: str, timeout: float, env: EnvVarsDict | None
    ) -> str:
        try:
            self._clean_session()
            if env:
                command = f"{env} {command}"
            self._send_line(command)
        except Exception as e:
            raise e

        output = self.get_output(timeout=timeout)
        self.session.PROMPT = self.session.UNIQUE_PROMPT
        self.session.prompt(0.1)

        return output

    def _close(self, force: bool = False) -> None:
        if force is True:
            self.session.close()
        else:
            if self.is_alive():
                self.session.logout()

    def copy_file(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
        source_remote: bool = False,
    ) -> None:
        """
        Send a local file to a remote host.
        """
        if source_remote:
            source_file = f"{self.username}@{self.ip}:{source_file}"
        else:
            destination_file = f"{self.username}@{self.ip}:{destination_file}"

        port = ""
        if self.port:
            port = f" -P {self.port}"

        command = (
            f"scp -v{port} -o NoHostAuthenticationForLocalhost=yes"
            f" {source_file} {destination_file}"
        )

        self._spawn_scp(command)

    def _spawn_scp(self, scp_cmd: str) -> None:
        """
        Transfer a file with SCP
        """
        self._logger.info(scp_cmd)
        p: pexpect.spawn = pexpect.spawn(scp_cmd)
        time.sleep(0.5)
        ssh_newkey: str = "Are you sure you want to continue connecting"
        i: int = p.expect(
            [ssh_newkey, "[pP]assword", "# ", pexpect.EOF, pexpect.TIMEOUT], 120
        )
        if i == 0:  # add once in trust list
            p.sendline("yes")
            i = p.expect([ssh_newkey, "[pP]assword", pexpect.EOF], 2)

        if i == 1:
            time.sleep(0.5)
            p.sendline(self.password)
            p.expect("Exit status 0", 60)
        if i == 4:
            self._logger.error("SCP TIMEOUT error %d" % i)
        p.close()
