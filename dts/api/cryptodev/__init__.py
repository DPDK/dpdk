# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Cryptodev-pmd non-interactive shell.

Typical usage example in a TestSuite::

    cryptodev = CryptodevPmd(CryptoPmdParams)
    stats = cryptodev.run_app()
"""

import re
from typing import TYPE_CHECKING, Any

from typing_extensions import Unpack

from api.cryptodev.config import CryptoPmdParams, TestType
from api.cryptodev.types import (
    CryptodevResults,
    LatencyResults,
    PmdCyclecountResults,
    ThroughputResults,
    VerifyResults,
)
from framework.context import get_ctx
from framework.exception import RemoteCommandExecutionError, SkippedTestException
from framework.remote_session.dpdk_shell import compute_eal_params

if TYPE_CHECKING:
    from framework.params.types import CryptoPmdParamsDict
from pathlib import PurePath


class Cryptodev:
    """non-interactive cryptodev application.

    Attributes:
        _app_params: app parameters to pass to dpdk-test-crypto-perf application
    """

    _app_params: dict[str, Any]

    def __init__(self, **app_params: Unpack["CryptoPmdParamsDict"]) -> None:
        """Initialize the cryptodev application.

        Args:
            app_params: The application parameters as keyword arguments.

        Raises:
            ValueError: if the build environment is `None`
        """
        self._app_params = {}
        for k, v in app_params.items():
            if v is not None:
                self._app_params[k] = (
                    self.vector_directory.joinpath(str(v)) if k == "test_file" else v
                )
        dpdk = get_ctx().dpdk.build
        if dpdk is None:
            raise ValueError("No DPDK build environment exists.")
        self._path = dpdk.get_app("test-crypto-perf")

    @property
    def path(self) -> PurePath:
        """Get the path to the cryptodev application.

        Returns:
            The path to the cryptodev application.
        """
        return PurePath(self._path)

    @property
    def vector_directory(self) -> PurePath:
        """Get the path to the cryptodev vector files.

        Returns:
            The path to the cryptodev vector files.
        """
        return get_ctx().dpdk_build.remote_dpdk_tree_path.joinpath("app/test-crypto-perf/data/")

    def run_app(self, num_vfs: int = 1) -> list[CryptodevResults]:
        """Run the cryptodev application with the given app parameters.

        Args:
            num_vfs: number of virtual functions to run the crypto-dev application with.

        Raises:
            SkippedTestException: If the device type is not supported on the main session.
            RemoteCommandExecutionError: If there is an error running the command.

        Returns:
            list[CryptodevResults]: The list of parsed results for the cryptodev application.
        """
        try:
            result = get_ctx().dpdk.run_dpdk_app(
                self.path,
                compute_eal_params(
                    CryptoPmdParams(
                        allowed_ports=get_ctx().topology.get_crypto_vfs(num_vfs),
                        memory_channels=get_ctx().dpdk.config.memory_channels,
                        **self._app_params,
                    ),
                ),
                timeout=120,
            )
        except RemoteCommandExecutionError as e:
            if "Crypto device type does not support capabilities requested" in e._command_stderr:
                raise SkippedTestException(
                    f"{self._app_params['devtype']} does not support the requested capabilities"
                )
            elif "Failed to initialise requested crypto device type" in e._command_stderr:
                raise SkippedTestException(
                    f"could not run application with devtype {self._app_params['devtype']}"
                )
            elif "failed to parse device" in e._command_stderr:
                raise SkippedTestException(
                    f"dependencies missing for virtual device {self._app_params['vdevs'][0].name}"
                )
            raise e

        regex = r"^\s+\d+.*$"
        parser_options = re.MULTILINE
        parser: type[CryptodevResults]

        match self._app_params["ptest"]:
            case TestType.throughput:
                parser = ThroughputResults
            case TestType.latency:
                regex = r"total operations:.*time[^\n]*"
                parser_options |= re.DOTALL
                parser = LatencyResults
            case TestType.pmd_cyclecount:
                parser = PmdCyclecountResults
            case TestType.verify:
                parser = VerifyResults

        return [parser.parse(line) for line in re.findall(regex, result.stdout, parser_options)]
